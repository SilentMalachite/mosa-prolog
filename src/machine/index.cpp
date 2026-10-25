#include "internal.hpp"

#include <cmath>
#include <vector>

namespace mosa {
namespace {

bool same_float(double left, double right) {
  if (std::isnan(left) || std::isnan(right)) return std::isnan(left) && std::isnan(right);
  return left == right;
}

bool matches(const Impl& machine, const Clause& clause, Cell first) {
  first = machine.heap.deref(first);
  if (machine.heap.unbound(first) || clause.key == KeyKind::Any || clause.key == KeyKind::Var ||
      clause.key == KeyKind::None) {
    return true;
  }
  if (clause.key == KeyKind::Atom) {
    return first.tag() == Tag::Atom && static_cast<std::uint32_t>(first.raw() >> 3) == clause.key_atom;
  }
  if (clause.key == KeyKind::Int) {
    return (first.tag() == Tag::Int || first.tag() == Tag::IntBox) &&
           machine.heap.integer_value(first) == clause.key_int;
  }
  if (clause.key == KeyKind::Float) {
    return first.tag() == Tag::Float && same_float(machine.heap.float_value(first), clause.key_float);
  }
  if (clause.key == KeyKind::List) return first.tag() == Tag::Lis;
  if (clause.key == KeyKind::Fun) {
    return first.tag() == Tag::Str && machine.heap.functor_atom(first) == clause.key_atom &&
           machine.heap.functor_arity(first) == clause.key_arity;
  }
  return true;
}

}  // namespace

std::vector<std::size_t> select_clauses(const Impl& machine, const Predicate& predicate, bool has_arg,
                                        Cell first) {
  std::vector<std::size_t> chosen;
  if (has_arg && machine.heap.unbound(machine.heap.deref(first))) {
    for (std::size_t index = 0; index < predicate.clauses.size(); ++index) chosen.push_back(index);
    return chosen;
  }
  for (std::size_t index = 0; index < predicate.clauses.size(); ++index) {
    const Clause& clause = predicate.clauses[index];
    if (!has_arg || matches(machine, clause, first)) chosen.push_back(index);
  }
  return chosen;
}

}
