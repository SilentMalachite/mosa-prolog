#include "internal.hpp"

#include "mosa/bigint.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
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

// Big の絶対値の語を Int の子の節点にする（ADR 0025）。語は項ではないので frames に積まず、その場で書き終える。
void export_big_words(const Impl& machine, Cell cell, ExportedTerm& out, int id) {
  const auto words = machine.heap.big_magnitude(cell);
  out.nodes[static_cast<std::size_t>(id)].kids.reserve(words.size());
  for (const std::uint64_t word : words) {
    ExportedTerm::Node child;
    child.kind = ExportedTerm::Kind::Int;
    child.integer = std::bit_cast<std::int64_t>(word);
    out.nodes.push_back(std::move(child));
    out.nodes[static_cast<std::size_t>(id)].kids.push_back(static_cast<int>(out.nodes.size() - 1));
  }
}

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
  } else if (cell.tag() == Tag::Big) {
    node.kind = ExportedTerm::Kind::Big;
    node.integer = machine.heap.big_negative(cell) ? 1 : 0;
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
  const bool big = node.kind == ExportedTerm::Kind::Big;
  out.nodes.push_back(std::move(node));
  const int id = static_cast<int>(out.nodes.size() - 1);
  if (shared) seen.emplace(raw, id);
  if (compound) frames.push_back(ExportFrame{id, cell, 0});
  if (big) export_big_words(machine, cell, out, id);
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

  // run の後で、節点 index を取り込んだセル。根から届かない Share の節点（変数の名前の写し）も、指す先の節点の
  // セルになる。変数と Share の節点だけを渡すこと（複合項は frames に積むだけで作り終えない）。
  Cell cell_of(int index) { return start(index); }

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
    else if (node.kind == ExportedTerm::Kind::Big) cell = big(node);
    else {
      cell = machine_.heap.fresh_ref();
      std::vector<Cell> args;
      args.reserve(node.kids.size());
      frames_.push_back(ImportFrame{index, cell, 0, std::move(args)});
    }
    made_[slot] = cell;
    return cell;
  }

  // 子の Int の節点は語として読むだけで、セルにはしない。heap.big が正規形に直すので、int64 に収まる値を
  // 持つ節点（テストの入口が作る）も Int か IntBox になる。
  Cell big(const ExportedTerm::Node& node) {
    std::vector<std::uint64_t> words;
    words.reserve(node.kids.size());
    for (const int kid : node.kids) {
      words.push_back(std::bit_cast<std::uint64_t>(term_.nodes[static_cast<std::size_t>(kid)].integer));
    }
    return machine_.heap.big(BigInt::from_words(words, node.integer != 0));
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

Cell import_term(Impl& machine, const ExportedTerm& term, std::span<const ExportedName> names,
                 std::vector<Binding>& bindings) {
  if (term.root < 0) return {};
  Importer importer(machine, term);
  const Cell root = importer.run(term.root);
  for (const ExportedName& name : names) bindings.push_back(Binding{name.name, importer.cell_of(name.node)});
  return root;
}

void propagate_throw(Impl& machine, Cell ball) {
  // 例外は call_goal が設定した呼び先を無効にする。回復ゴールの枠を生成関数で始めない。
  machine.native_next = nullptr;
  // 同じ歩みのカット（生成コードの rt::cut_to の後の throw や算術の誤り）が外した後始末は、巻き戻す前に動かす。
  // 先に巻き戻すと、後始末の項の載ったヒープを catch/3 の水位まで切り詰めてしまう（ADR 0016、敵対的レビュー A1）。
  // run_loop が次の歩みの前に後始末を動かし、その後で throw(Ball) を投げ直す。
  if (!machine.pending_cleanups.empty()) {
    machine.ball = ball;
    machine.exception = true;
    machine.failed = false;
    machine.ip = kNoFrame;
    return;
  }
  const ExportedTerm copy = export_term(machine, ball);
  while (!machine.stack.empty()) {
    // 入れ子の run_loop の床で止める。下の節走査は assert が直した添字のまま、呼び出し側がやり直す。
    if (machine.stack.back().pick == Pick::Barrier) break;
    const Entry top = std::move(machine.stack.back());
    machine.stack.pop_back();
    prune_catch_log(machine);
    if (top.pick == Pick::CallCleanup) {
      throw_through_cleanup(machine, top, copy);
      return;
    }
    if (top.pick != Pick::Catch || !top.armed) continue;
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

LaunchNext open_catch(Impl& machine, const GoalRef& goal, std::uint64_t cont, std::size_t cut,
                      const EnvRef& env, std::uint32_t module) {
  Entry entry;
  entry.pick = Pick::Catch;
  entry.pattern = materialize(machine, goal->args[1], env->cells, env->shared.get());
  entry.recovery = sub_goal(goal, 2);
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
  return LaunchNext{sub_goal(goal, 0), frame, machine.stack.size()};
}

// ADR 0012。ゴールが作った選択点はちょうど Catch より上の Entry なので、上に何も無ければゴールの中へ戻る道は無く、
// この Catch も抜ける枠も二度と使われない。外して根と catch_log を残さない。上に選択点があれば無効にするだけにし、
// 戻ったときに有効に戻せるよう記録する。
void disarm_catch(Impl& machine, std::size_t index) {
  if (index >= machine.stack.size() || machine.stack[index].pick != Pick::Catch) return;
  if (index + 1 == machine.stack.size()) {
    trim_stack(machine, index);
    return;
  }
  Entry& entry = machine.stack[index];
  if (entry.armed) machine.catch_log.push_back(CatchRecord{index, entry.effect_id});
  entry.armed = false;
}

void throw_ball(Impl& machine, Cell ball) {
  ball = machine.heap.deref(ball);
  if (machine.heap.unbound(ball)) {
    raise_formal(machine, formal_instantiation(machine), machine.atoms.intern("throw"), 1);
    return;
  }
  propagate_throw(machine, ball);
}

void arm_throw(Impl& machine, const IrTerm& goal, const std::vector<Cell>& env, const SharedTable* shared) {
  throw_ball(machine, materialize(machine, goal.args[0], env, shared));
}

void arm_halt(Impl& machine, const IrTerm& goal, const std::vector<Cell>& env, const SharedTable* shared) {
  int code = 0;
  if (!goal.args.empty()) {
    const Cell value = machine.heap.deref(materialize(machine, goal.args[0], env, shared));
    const auto name = machine.atoms.intern("halt");
    if (machine.heap.unbound(value)) {
      raise_formal(machine, formal_instantiation(machine), name, 1);
      return;
    }
    if (const auto problem = halt_code_problem(machine, value, code)) {
      raise_formal(machine, *problem, name, 1);
      return;
    }
  }
  machine.halt_now = true;
  machine.halt_code = code;
  machine.ip = kNoFrame;
}

}
