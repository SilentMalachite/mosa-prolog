#include "internal.hpp"

#include <unordered_map>
#include <utility>
#include <vector>

namespace mosa {
namespace {

// 祖先の複合項 1 つ分。kids は親の Node に直に足す。out.nodes は伸びるので添字で持つ。
struct ExportFrame {
  int id;
  Cell cell;
  std::uint16_t next;
};

// 1 つの項に番号を振る。複合項は番号だけ先に取って frames に積み、子は後で辿る。前の再帰と同じ前順の番号に
// するため、子を 1 つ辿り終えるまで次の兄弟へ進まない。
int export_one(Impl& machine, Cell cell, ExportedTerm& out, std::unordered_map<std::uint64_t, int>& seen,
               std::vector<ExportFrame>& frames) {
  cell = machine.heap.deref(cell);
  const auto raw = cell.raw();
  const bool shared = machine.heap.unbound(cell) || cell.tag() == Tag::Lis || cell.tag() == Tag::Str;
  ExportedTerm::Node node;
  if (shared) {
    const auto found = seen.find(raw);
    if (found != seen.end()) {
      node.kind = ExportedTerm::Kind::Share;
      node.share = found->second;
      out.nodes.push_back(std::move(node));
      return static_cast<int>(out.nodes.size() - 1);
    }
  }
  if (machine.heap.unbound(cell)) {
    node.kind = ExportedTerm::Kind::Var;
  } else if (cell.tag() == Tag::Atom) {
    node.kind = ExportedTerm::Kind::Atom;
    node.atom = static_cast<std::uint32_t>(cell.raw() >> 3);
  } else if (cell.tag() == Tag::Int || cell.tag() == Tag::IntBox) {
    node.kind = ExportedTerm::Kind::Int;
    node.integer = machine.heap.integer_value(cell);
  } else if (cell.tag() == Tag::Float) {
    node.kind = ExportedTerm::Kind::Float;
    node.floating = machine.heap.float_value(cell);
  } else if (cell.tag() == Tag::Lis) {
    node.kind = ExportedTerm::Kind::List;
    node.arity = 2;
  } else {
    node.kind = ExportedTerm::Kind::Str;
    node.atom = machine.heap.functor_atom(cell);
    node.arity = machine.heap.functor_arity(cell);
  }
  const bool compound = node.kind == ExportedTerm::Kind::List || node.kind == ExportedTerm::Kind::Str;
  // List の arity は辿る数にだけ使い、書き出す Node には前と同じく 0 を残す。
  const std::uint16_t arity = node.arity;
  if (node.kind == ExportedTerm::Kind::List) node.arity = 0;
  if (compound) node.kids.reserve(arity);
  out.nodes.push_back(std::move(node));
  const int id = static_cast<int>(out.nodes.size() - 1);
  if (shared) seen.emplace(raw, id);
  if (compound) frames.push_back(ExportFrame{id, cell, 0});
  return id;
}

Cell export_child(Impl& machine, Cell cell, std::uint16_t index) {
  if (cell.tag() == Tag::Lis) return index == 0 ? machine.heap.list_head(cell) : machine.heap.list_tail(cell);
  return machine.heap.arg(cell, index);
}

// 深い項でも C++ のスタックを使わないよう、祖先を frames に手で積む（ADR 0011）。
int export_walk(Impl& machine, Cell cell, ExportedTerm& out) {
  std::unordered_map<std::uint64_t, int> seen;
  std::vector<ExportFrame> frames;
  const int root = export_one(machine, cell, out, seen, frames);
  while (!frames.empty()) {
    ExportFrame& top = frames.back();
    const std::uint16_t arity = out.nodes[static_cast<std::size_t>(top.id)].kind == ExportedTerm::Kind::List
                                    ? 2
                                    : out.nodes[static_cast<std::size_t>(top.id)].arity;
    if (top.next == arity) {
      frames.pop_back();
      continue;
    }
    const int parent = top.id;
    const Cell child = export_child(machine, top.cell, top.next++);
    // export_one が frames を伸ばすと top は無効になるので、先に親の番号を写しておく。
    const int kid = export_one(machine, child, out, seen, frames);
    out.nodes[static_cast<std::size_t>(parent)].kids.push_back(kid);
  }
  return root;
}

// 取り込み中の複合項 1 つ分。穴を先に作って made_ に置き、子をすべて作ってから本体を作って穴に束縛する。
struct ImportFrame {
  int index;
  Cell hole;
  std::size_t next;
  std::vector<Cell> args;
};

class Importer {
 public:
  Importer(Impl& machine, const ExportedTerm& term) : machine_(machine), term_(term), made_(term.nodes.size()) {}

  // 前の再帰と同じ順にヒープを割り当てる。変数の標準順序は番地で決まるので、順を変えると答えが変わる。
  Cell run(int root) {
    const Cell result = start(root);
    while (!frames_.empty()) {
      ImportFrame& top = frames_.back();
      const ExportedTerm::Node& node = term_.nodes[static_cast<std::size_t>(top.index)];
      if (top.next == node.kids.size()) {
        finish(top, node);
        frames_.pop_back();
        continue;
      }
      const int kid = node.kids[top.next++];
      // start が frames_ を伸ばすと top は無効になるので、子の値は伸ばす前の位置へ添字で書く。
      const std::size_t at = frames_.size() - 1;
      const Cell value = start(kid);
      frames_[at].args.push_back(value);
    }
    return result;
  }

 private:
  Cell start(int index) {
    if (index < 0) return {};
    const auto slot = static_cast<std::size_t>(index);
    if (started_[slot]) return made_[slot];
    started_[slot] = 1;
    const ExportedTerm::Node& node = term_.nodes[slot];
    Cell cell;
    if (node.kind == ExportedTerm::Kind::Share) cell = start(node.share);
    else if (node.kind == ExportedTerm::Kind::Var) cell = machine_.heap.fresh_ref();
    else if (node.kind == ExportedTerm::Kind::Atom) cell = machine_.heap.atom(node.atom);
    else if (node.kind == ExportedTerm::Kind::Int) cell = machine_.heap.integer(node.integer);
    else if (node.kind == ExportedTerm::Kind::Float) cell = machine_.heap.floating(node.floating);
    else {
      cell = machine_.heap.fresh_ref();
      std::vector<Cell> args;
      args.reserve(node.kids.size());
      frames_.push_back(ImportFrame{index, cell, 0, std::move(args)});
    }
    made_[slot] = cell;
    return cell;
  }

  void finish(const ImportFrame& frame, const ExportedTerm::Node& node) {
    const Cell real = node.kind == ExportedTerm::Kind::List ? machine_.heap.list(frame.args[0], frame.args[1])
                                                            : machine_.heap.structure(node.atom, frame.args);
    machine_.heap.bind(frame.hole, real);
  }

  Impl& machine_;
  const ExportedTerm& term_;
  std::vector<Cell> made_;
  std::vector<char> started_ = std::vector<char>(made_.size(), 0);
  std::vector<ImportFrame> frames_;
};

std::uint64_t push_extra(Impl& machine, Extra extra) {
  machine.extras.push_back(std::move(extra));
  const auto at = machine.heap.allocate_words(kFrameHead);
  machine.heap.set_word(at, 0);
  machine.heap.set_word(at + 1, machine.extras.size() - 1);
  machine.heap.set_word(at + 2, 0);
  machine.heap.set_word(at + 3, kNoFrame);
  return at;
}

}  // namespace

ExportedTerm export_term(Impl& machine, Cell cell) {
  ExportedTerm term;
  term.root = export_walk(machine, cell, term);
  return term;
}

Cell import_term(Impl& machine, const ExportedTerm& term) {
  if (term.root < 0) return {};
  Importer importer(machine, term);
  return importer.run(term.root);
}

void propagate_throw(Impl& machine, Cell ball) {
  // 例外は call_goal が設定した呼び先を無効にする。回復ゴールの枠を生成関数で始めない。
  machine.native_next = nullptr;
  const ExportedTerm copy = export_term(machine, ball);
  while (!machine.stack.empty()) {
    // 収集の床で止める。下の節走査は assert が直した添字のまま、呼び出し側がやり直す。
    if (machine.stack.back().pick == Pick::Barrier) break;
    const Entry top = machine.stack.back();
    machine.stack.pop_back();
    if (top.pick != Pick::Catch || !top.armed) continue;
    // 例外もその catch より後ろの op/3 を戻す。節のデータベースは戻さない。
    restore_operators(machine, top.effect_id);
    restore_state(machine, top.heap_mark, top.trail_mark, top.extras_mark);
    const Cell rebuilt = import_term(machine, copy);
    if (do_unify(machine, rebuilt, top.pattern)) {
      machine.failed = false;
      machine.exception = false;
      // 生成コードの catch（push_catch）は回復ゴールを継続枠で持つ。枠の状態の関数が入口で局所の境界を取る。
      if (top.redo_goal != kNoFrame) {
        machine.ip = top.redo_goal;
        return;
      }
      // 回復ゴールは call と同じで、中のカットは catch の外へ出ない（7.8.9）。
      launch(machine, top.recovery, top.cont, machine.stack.size(), top.env, top.module);
      return;
    }
  }
  machine.ball = import_term(machine, copy);
  machine.exception = true;
  machine.failed = false;
  machine.ip = kNoFrame;
}

void arm_catch(Impl& machine, const IrTerm& goal, std::uint64_t cont, std::size_t cut,
               const std::vector<Cell>& env, std::uint32_t module) {
  Entry entry;
  entry.pick = Pick::Catch;
  entry.pattern = materialize(machine, goal.args[1], env);
  entry.recovery = goal.args[2];
  entry.env = env;
  entry.cont = cont;
  entry.cut = cut;
  entry.armed = true;
  entry.module = module;
  entry.heap_mark = machine.heap.size();
  entry.trail_mark = machine.trail.slots.size();
  push_frame(machine, std::move(entry));
  // ゴールを抜けたあとは Catch を無効にする。選択点が残る間は枠を残し、戻ったときだけ有効に戻す。
  Extra exit;
  exit.ctrl = Ctrl::CatchExit;
  exit.trim_to = machine.stack.size() - 1;
  exit.cont = cont;
  const auto frame = push_extra(machine, std::move(exit));
  launch(machine, goal.args[0], frame, machine.stack.size(), env, module);
}

void disarm_catch(Impl& machine, std::size_t index) {
  if (index < machine.stack.size() && machine.stack[index].pick == Pick::Catch) machine.stack[index].armed = false;
}

void throw_ball(Impl& machine, Cell ball) {
  ball = machine.heap.deref(ball);
  if (machine.heap.unbound(ball)) {
    raise_formal(machine, formal_instantiation(machine), machine.atoms.intern("throw"), 1);
    return;
  }
  propagate_throw(machine, ball);
}

void arm_throw(Impl& machine, const IrTerm& goal, const std::vector<Cell>& env) {
  throw_ball(machine, materialize(machine, goal.args[0], env));
}

void arm_halt(Impl& machine, const IrTerm& goal, const std::vector<Cell>& env) {
  int code = 0;
  if (!goal.args.empty()) {
    const Cell value = machine.heap.deref(materialize(machine, goal.args[0], env));
    const auto name = machine.atoms.intern("halt");
    if (machine.heap.unbound(value)) {
      raise_formal(machine, formal_instantiation(machine), name, 1);
      return;
    }
    if (value.tag() != Tag::Int && value.tag() != Tag::IntBox) {
      raise_formal(machine, formal_type(machine, "integer", value), name, 1);
      return;
    }
    code = static_cast<int>(machine.heap.integer_value(value));
  }
  machine.halt_now = true;
  machine.halt_code = code;
  machine.ip = kNoFrame;
}

}
