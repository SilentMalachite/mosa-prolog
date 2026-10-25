#include "internal.hpp"

#include <unordered_map>
#include <utility>
#include <vector>

namespace mosa {
namespace {

int export_rec(Impl& machine, Cell cell, ExportedTerm& out, std::unordered_map<std::uint64_t, int>& seen) {
  cell = machine.heap.deref(cell);
  const auto raw = cell.raw();
  if (machine.heap.unbound(cell) || cell.tag() == Tag::Lis || cell.tag() == Tag::Str) {
    const auto found = seen.find(raw);
    if (found != seen.end()) {
      ExportedTerm::Node share;
      share.kind = ExportedTerm::Kind::Share;
      share.share = found->second;
      out.nodes.push_back(share);
      return static_cast<int>(out.nodes.size() - 1);
    }
  }
  if (machine.heap.unbound(cell)) {
    ExportedTerm::Node node;
    node.kind = ExportedTerm::Kind::Var;
    out.nodes.push_back(node);
    const int id = static_cast<int>(out.nodes.size() - 1);
    seen.emplace(raw, id);
    return id;
  }
  if (cell.tag() == Tag::Atom) {
    ExportedTerm::Node node;
    node.kind = ExportedTerm::Kind::Atom;
    node.atom = static_cast<std::uint32_t>(cell.raw() >> 3);
    out.nodes.push_back(node);
    return static_cast<int>(out.nodes.size() - 1);
  }
  if (cell.tag() == Tag::Int || cell.tag() == Tag::IntBox) {
    ExportedTerm::Node node;
    node.kind = ExportedTerm::Kind::Int;
    node.integer = machine.heap.integer_value(cell);
    out.nodes.push_back(node);
    return static_cast<int>(out.nodes.size() - 1);
  }
  if (cell.tag() == Tag::Float) {
    ExportedTerm::Node node;
    node.kind = ExportedTerm::Kind::Float;
    node.floating = machine.heap.float_value(cell);
    out.nodes.push_back(node);
    return static_cast<int>(out.nodes.size() - 1);
  }
  const int id = static_cast<int>(out.nodes.size());
  out.nodes.push_back(ExportedTerm::Node{});
  seen.emplace(raw, id);
  ExportedTerm::Node node;
  if (cell.tag() == Tag::Lis) {
    node.kind = ExportedTerm::Kind::List;
    node.kids.push_back(export_rec(machine, machine.heap.list_head(cell), out, seen));
    node.kids.push_back(export_rec(machine, machine.heap.list_tail(cell), out, seen));
  } else if (cell.tag() == Tag::Str) {
    node.kind = ExportedTerm::Kind::Str;
    node.atom = machine.heap.functor_atom(cell);
    node.arity = machine.heap.functor_arity(cell);
    for (std::uint16_t index = 0; index < node.arity; ++index) {
      node.kids.push_back(export_rec(machine, machine.heap.arg(cell, index), out, seen));
    }
  }
  out.nodes[static_cast<std::size_t>(id)] = std::move(node);
  return id;
}

class Importer {
 public:
  Importer(Impl& machine, const ExportedTerm& term) : machine_(machine), term_(term), made_(term.nodes.size()) {}

  Cell at(int index) {
    if (index < 0) return {};
    if (started_[static_cast<std::size_t>(index)]) return made_[static_cast<std::size_t>(index)];
    started_[static_cast<std::size_t>(index)] = 1;
    const ExportedTerm::Node& node = term_.nodes[static_cast<std::size_t>(index)];
    Cell cell;
    if (node.kind == ExportedTerm::Kind::Share) cell = at(node.share);
    else if (node.kind == ExportedTerm::Kind::Var) cell = machine_.heap.fresh_ref();
    else if (node.kind == ExportedTerm::Kind::Atom) cell = machine_.heap.atom(node.atom);
    else if (node.kind == ExportedTerm::Kind::Int) cell = machine_.heap.integer(node.integer);
    else if (node.kind == ExportedTerm::Kind::Float) cell = machine_.heap.floating(node.floating);
    else cell = compound(index, node);
    made_[static_cast<std::size_t>(index)] = cell;
    return cell;
  }

 private:
  Cell compound(int index, const ExportedTerm::Node& node) {
    const Cell hole = machine_.heap.fresh_ref();
    made_[static_cast<std::size_t>(index)] = hole;
    std::vector<Cell> args;
    args.reserve(node.kids.size());
    for (const int kid : node.kids) args.push_back(at(kid));
    const Cell real = node.kind == ExportedTerm::Kind::List ? machine_.heap.list(args[0], args[1])
                                                            : machine_.heap.structure(node.atom, args);
    machine_.heap.bind(hole, real);
    return hole;
  }

  Impl& machine_;
  const ExportedTerm& term_;
  std::vector<Cell> made_;
  std::vector<char> started_ = std::vector<char>(made_.size(), 0);
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
  std::unordered_map<std::uint64_t, int> seen;
  term.root = export_rec(machine, cell, term, seen);
  return term;
}

Cell import_term(Impl& machine, const ExportedTerm& term) {
  if (term.root < 0) return {};
  Importer importer(machine, term);
  return importer.at(term.root);
}

void propagate_throw(Impl& machine, Cell ball) {
  const ExportedTerm copy = export_term(machine, ball);
  while (!machine.stack.empty()) {
    // 収集の床で止める。下の節走査は assert が直した添字のまま、呼び出し側がやり直す。
    if (machine.stack.back().pick == Pick::Barrier) break;
    const Entry top = machine.stack.back();
    machine.stack.pop_back();
    if (top.pick != Pick::Catch || !top.armed) continue;
    restore_state(machine, top.heap_mark, top.trail_mark, top.extras_mark);
    const Cell rebuilt = import_term(machine, copy);
    if (do_unify(machine, rebuilt, top.pattern)) {
      machine.failed = false;
      machine.exception = false;
      // 回復ゴールは call と同じで、中のカットは catch の外へ出ない（7.8.9）。
      launch(machine, top.recovery, top.cont, machine.stack.size(), top.env);
      return;
    }
  }
  machine.ball = import_term(machine, copy);
  machine.exception = true;
  machine.failed = false;
  machine.ip = kNoFrame;
}

void arm_catch(Impl& machine, const IrTerm& goal, std::uint64_t cont, std::size_t cut,
               const std::vector<Cell>& env) {
  Entry entry;
  entry.pick = Pick::Catch;
  entry.pattern = materialize(machine, goal.args[1], env);
  entry.recovery = goal.args[2];
  entry.env = env;
  entry.cont = cont;
  entry.cut = cut;
  entry.armed = true;
  entry.heap_mark = machine.heap.size();
  entry.trail_mark = machine.trail.slots.size();
  push_frame(machine, std::move(entry));
  // ゴールを抜けたあとは Catch を無効にする。選択点が残る間は枠を残し、戻ったときだけ有効に戻す。
  Extra exit;
  exit.ctrl = Ctrl::CatchExit;
  exit.trim_to = machine.stack.size() - 1;
  exit.cont = cont;
  const auto frame = push_extra(machine, std::move(exit));
  launch(machine, goal.args[0], frame, machine.stack.size(), env);
}

void arm_throw(Impl& machine, const IrTerm& goal, const std::vector<Cell>& env) {
  Cell ball = machine.heap.deref(materialize(machine, goal.args[0], env));
  const auto name = machine.atoms.intern("throw");
  if (machine.heap.unbound(ball)) {
    raise_formal(machine, formal_instantiation(machine), name, 1);
    return;
  }
  propagate_throw(machine, ball);
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
