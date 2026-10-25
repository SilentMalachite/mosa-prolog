#include "internal.hpp"

#include <cmath>
#include <cstdint>
#include <optional>
#include <string_view>

namespace mosa {
namespace {

struct Build {
  const Heap* heap = nullptr;
  std::vector<std::uint64_t> refs;
  std::vector<Cell> env;
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
  [[nodiscard]] IrTerm goal(Cell cell);
  // 数の前の単項 + と - を畳む。-0.0 は符号を残す。畳めなければ空。
  [[nodiscard]] std::optional<IrTerm> fold_signed(Cell cell);
};

std::optional<IrTerm> Build::fold_signed(Cell cell) {
  if (heap->functor_arity(cell) != 1) return std::nullopt;
  const auto op = heap->atoms().name(heap->functor_atom(cell));
  if (op != "-" && op != "+") return std::nullopt;
  const IrTerm arg = data(heap->arg(cell, 0));
  if (arg.tag == IrTag::Int && op == "+") return arg;
  if (arg.tag == IrTag::Int && op == "-" && arg.integer != INT64_MIN) {
    IrTerm negated = arg;
    negated.integer = -arg.integer;
    return negated;
  }
  if (arg.tag == IrTag::Float && op == "+") return arg;
  if (arg.tag == IrTag::Float && op == "-") {
    IrTerm negated = arg;
    if (arg.floating == 0) negated.floating = std::copysign(0.0, -std::copysign(1.0, arg.floating));
    else negated.floating = -arg.floating;
    return negated;
  }
  return std::nullopt;
}

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
  if (cell.tag() == Tag::Lis) {
    term.tag = IrTag::List;
    term.args.push_back(data(heap->list_head(cell)));
    term.args.push_back(data(heap->list_tail(cell)));
    return term;
  }
  if (cell.tag() != Tag::Str) return term;
  if (const std::optional<IrTerm> folded = fold_signed(cell)) return *folded;
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

bool compile_clause(const Heap& heap, Cell cell, Clause& clause) {
  Build build;
  build.heap = &heap;
  Cell head = heap.deref(cell);
  IrTerm body;
  body.tag = IrTag::True;
  if (build.is_name(head, ":-", 2)) {
    body = build.goal(heap.arg(head, 1));
    head = heap.deref(heap.arg(head, 0));
  }
  if (heap.unbound(head) || head.tag() == Tag::Int || head.tag() == Tag::IntBox || head.tag() == Tag::Float ||
      head.tag() == Tag::Lis) {
    return false;
  }
  clause.head = build.data(head);
  clause.body = body.tag == IrTag::True && body.args.empty() ? body : body;
  if (build.is_name(heap.deref(cell), ":-", 2)) clause.body = body;
  clause.vars = static_cast<std::uint16_t>(build.refs.size());
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
