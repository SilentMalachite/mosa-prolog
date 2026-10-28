#include "internal.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace mosa {
namespace {

struct Build {
  const Heap* heap = nullptr;
  std::vector<std::uint64_t> refs;
  // 今たどっている複合項。循環を匿名に切る。with_output_to が束縛済みの項を再び訳す。
  std::vector<std::uint64_t> open;
  std::vector<Cell> env;
  // 実行時のゴールで巡回の戻り先になった複合項。写さずに元の項を指す。
  std::vector<std::uint64_t> loops;
  bool keep = false;

  [[nodiscard]] std::uint16_t slot_of(Cell cell) {
    const auto raw = cell.raw();
    for (std::size_t index = 0; index < refs.size(); ++index) {
      if (refs[index] == raw) return static_cast<std::uint16_t>(index);
    }
    refs.push_back(raw);
    if (keep) env.push_back(cell);
    return static_cast<std::uint16_t>(refs.size() - 1);
  }

  [[nodiscard]] bool is_name(Cell cell, std::string_view name, std::uint16_t arity) const {
    cell = heap->deref(cell);
    if (arity == 0) return cell.tag() == Tag::Atom && heap->atoms().name(static_cast<std::uint32_t>(cell.raw() >> 3)) == name;
    if (cell.tag() != Tag::Str || heap->functor_arity(cell) != arity) return false;
    return heap->atoms().name(heap->functor_atom(cell)) == name;
  }

  [[nodiscard]] IrTerm data(Cell cell);
  [[nodiscard]] IrTerm compound_data(Cell cell);
  [[nodiscard]] IrTerm goal(Cell cell);
  [[nodiscard]] bool building(std::uint64_t raw) const {
    for (const auto item : open) {
      if (item == raw) return true;
    }
    return false;
  }
};

IrTerm Build::data(Cell cell) {
  cell = heap->deref(cell);
  IrTerm term;
  if (heap->unbound(cell)) {
    term.tag = IrTag::Var;
    term.slot = slot_of(cell);
    return term;
  }
  if (cell.tag() == Tag::Atom) {
    term.tag = IrTag::Atom;
    term.atom = static_cast<std::uint32_t>(cell.raw() >> 3);
    return term;
  }
  if (cell.tag() == Tag::Int || cell.tag() == Tag::IntBox) {
    term.tag = IrTag::Int;
    term.integer = heap->integer_value(cell);
    return term;
  }
  if (cell.tag() == Tag::Float) {
    term.tag = IrTag::Float;
    term.floating = heap->float_value(cell);
    return term;
  }
  if (cell.tag() != Tag::Lis && cell.tag() != Tag::Str) return term;
  if (building(cell.raw())) {
    // 実行時のゴールは元の項を指せば巡回が残る。匿名に切ると巡回リストが部分リストになる。
    // 節は env を持たないので匿名のまま（巡回する節は 7.6.2 の外）。
    if (keep) {
      loops.push_back(cell.raw());
      term.tag = IrTag::Var;
      term.slot = slot_of(cell);
      return term;
    }
    term.tag = IrTag::Anon;
    return term;
  }
  open.push_back(cell.raw());
  term = compound_data(cell);
  open.pop_back();
  // 巡回の入口まで写すと、元の項より一段長い項になる。入口は元の項そのものを渡す。
  if (keep && std::find(loops.begin(), loops.end(), cell.raw()) != loops.end()) {
    term = IrTerm{};
    term.tag = IrTag::Var;
    term.slot = slot_of(cell);
  }
  return term;
}

IrTerm Build::compound_data(Cell cell) {
  IrTerm term;
  if (cell.tag() == Tag::Lis) {
    term.tag = IrTag::List;
    term.args.push_back(data(heap->list_head(cell)));
    term.args.push_back(data(heap->list_tail(cell)));
    return term;
  }
  // '.'/2 はリストそのもの。構造体のままにすると = と == が食い違う。
  if (heap->functor_arity(cell) == 2 && heap->atoms().name(heap->functor_atom(cell)) == ".") {
    term.tag = IrTag::List;
    term.args.push_back(data(heap->arg(cell, 0)));
    term.args.push_back(data(heap->arg(cell, 1)));
    return term;
  }
  term.tag = IrTag::Fun;
  term.atom = heap->functor_atom(cell);
  term.arity = heap->functor_arity(cell);
  term.args.reserve(term.arity);
  for (std::uint16_t index = 0; index < term.arity; ++index) term.args.push_back(data(heap->arg(cell, index)));
  return term;
}

IrTerm Build::goal(Cell cell) {
  cell = heap->deref(cell);
  if (is_name(cell, "true", 0)) {
    IrTerm term;
    term.tag = IrTag::True;
    return term;
  }
  if (is_name(cell, "fail", 0) || is_name(cell, "false", 0)) {
    IrTerm term;
    term.tag = IrTag::Fail;
    return term;
  }
  if (is_name(cell, "!", 0)) {
    IrTerm term;
    term.tag = IrTag::Cut;
    return term;
  }
  if (is_name(cell, ",", 2) || is_name(cell, ";", 2) || is_name(cell, "->", 2) || is_name(cell, "*->", 2)) {
    IrTerm term;
    const auto name = heap->atoms().name(heap->functor_atom(cell));
    if (name == ",") term.tag = IrTag::And;
    else if (name == ";") term.tag = IrTag::Or;
    else if (name == "->") term.tag = IrTag::IfThen;
    else term.tag = IrTag::SoftIf;
    term.args.push_back(goal(heap->arg(cell, 0)));
    term.args.push_back(goal(heap->arg(cell, 1)));
    return term;
  }
  if (is_name(cell, "catch", 3)) {
    IrTerm term;
    term.tag = IrTag::Catch;
    term.args.push_back(goal(heap->arg(cell, 0)));
    term.args.push_back(data(heap->arg(cell, 1)));
    term.args.push_back(goal(heap->arg(cell, 2)));
    return term;
  }
  if (is_name(cell, "throw", 1)) {
    IrTerm term;
    term.tag = IrTag::Throw;
    term.args.push_back(data(heap->arg(cell, 0)));
    return term;
  }
  if (is_name(cell, "halt", 0) || is_name(cell, "halt", 1)) {
    IrTerm term;
    term.tag = IrTag::Halt;
    if (heap->deref(cell).tag() == Tag::Str) term.args.push_back(data(heap->arg(cell, 0)));
    return term;
  }
  return data(cell);
}

void set_key(Clause& clause) {
  if (clause.head.tag != IrTag::Fun || clause.head.args.empty()) {
    clause.key = KeyKind::None;
    return;
  }
  const IrTerm& first = clause.head.args[0];
  if (first.tag == IrTag::Var || first.tag == IrTag::Anon) clause.key = KeyKind::Var;
  else if (first.tag == IrTag::Atom) {
    clause.key = KeyKind::Atom;
    clause.key_atom = first.atom;
  } else if (first.tag == IrTag::Int) {
    clause.key = KeyKind::Int;
    clause.key_int = first.integer;
  } else if (first.tag == IrTag::Float) {
    clause.key = KeyKind::Float;
    clause.key_float = first.floating;
  } else if (first.tag == IrTag::List) clause.key = KeyKind::List;
  else if (first.tag == IrTag::Fun) {
    clause.key = KeyKind::Fun;
    clause.key_atom = first.atom;
    clause.key_arity = first.arity;
  } else {
    clause.key = KeyKind::Any;
  }
}

// 名前はスロットに写すだけで、IR には入れない。読み込みの変数名が節の意味を変えないため。
void name_slots(const Heap& heap, const Build& build, std::span<const VarName> names, Clause& clause) {
  if (names.empty()) return;
  clause.names.assign(build.refs.size(), std::string{});
  for (const VarName& name : names) {
    const auto raw = heap.deref(name.cell).raw();
    for (std::size_t index = 0; index < build.refs.size(); ++index) {
      if (build.refs[index] == raw) clause.names[index] = name.name;
    }
  }
}

}  // namespace

Compiled compile_goal(const Heap& heap, Cell cell) {
  Build build;
  build.heap = &heap;
  build.keep = true;
  Compiled compiled;
  compiled.term = build.goal(cell);
  compiled.env = std::move(build.env);
  return compiled;
}

bool dot_head(const Heap& heap, Cell head, Cell& left, Cell& right) {
  head = heap.deref(head);
  if (head.tag() == Tag::Lis) {
    left = heap.list_head(head);
    right = heap.list_tail(head);
    return true;
  }
  if (head.tag() == Tag::Str && heap.functor_arity(head) == 2 && heap.atoms().name(heap.functor_atom(head)) == ".") {
    left = heap.arg(head, 0);
    right = heap.arg(head, 1);
    return true;
  }
  return false;
}

void set_dot_head(Build& build, const Heap& heap, Clause& clause, Cell left, Cell right) {
  // 頭の '.'/2 はデータとしてのリストではなく、述語 '.'/2 である。
  clause.head.tag = IrTag::Fun;
  clause.head.atom = heap.atoms().intern(".");
  clause.head.arity = 2;
  clause.head.args.push_back(build.data(left));
  clause.head.args.push_back(build.data(right));
}

bool compile_clause(const Heap& heap, Cell cell, Clause& clause, std::span<const VarName> names) {
  Build build;
  build.heap = &heap;
  Cell head = heap.deref(cell);
  IrTerm body;
  body.tag = IrTag::True;
  if (build.is_name(head, ":-", 2)) {
    body = build.goal(heap.arg(head, 1));
    head = heap.deref(heap.arg(head, 0));
  }
  Cell left;
  Cell right;
  if (dot_head(heap, head, left, right)) {
    set_dot_head(build, heap, clause, left, right);
  } else if (heap.unbound(head) || head.tag() == Tag::Int || head.tag() == Tag::IntBox ||
             head.tag() == Tag::Float) {
    return false;
  } else {
    clause.head = build.data(head);
  }
  clause.body = body.tag == IrTag::True && body.args.empty() ? body : body;
  if (build.is_name(heap.deref(cell), ":-", 2)) clause.body = body;
  clause.vars = static_cast<std::uint16_t>(build.refs.size());
  name_slots(heap, build, names, clause);
  if (clause.head.tag == IrTag::Atom) {
    clause.head.arity = 0;
  }
  set_key(clause);
  return clause.head.tag == IrTag::Atom || clause.head.tag == IrTag::Fun;
}

bool is_directive(const Heap& heap, Cell cell) {
  cell = heap.deref(cell);
  if (cell.tag() != Tag::Str || heap.functor_arity(cell) != 1) return false;
  const auto name = heap.atoms().name(heap.functor_atom(cell));
  return name == ":-" || name == "?-";
}

}
