#include "../machine/internal.hpp"

namespace mosa {

Predicate* predicate_in(Impl& machine, std::uint32_t home, std::uint32_t name, std::uint16_t arity) {
  for (Predicate& predicate : machine.predicates) {
    if (predicate.home == home && predicate.name == name && predicate.arity == arity) return &predicate;
  }
  return nullptr;
}

bool defined_anywhere(const Impl& machine, std::uint32_t name, std::uint16_t arity) {
  for (const Predicate& predicate : machine.predicates) {
    if (predicate.name != name || predicate.arity != arity) continue;
    // 節の無い動的述語も定義である。宣言だけの multifile は手続きにしない。
    if (predicate.dynamic || !predicate.clauses.empty()) return true;
  }
  return false;
}

}  // namespace mosa
