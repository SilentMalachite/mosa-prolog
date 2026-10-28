// 節操作と宣言が受ける M: 付きの項（SPEC 第5節、iso-notes 第5節の節操作の決定）。
// モード: M:Term の M は +atom。重なった M: は一番内側が勝つ。
// エラー: instantiation_error、type_error(atom, M)、existence_error(module, M)。
// テスト: tests/pl/modules.pl
#include "module.hpp"

#include "../machine/internal.hpp"

#include <array>
#include <vector>

namespace mosa {

namespace {

bool qualified(const Impl& machine, Cell cell) {
  return cell.tag() == Tag::Str && machine.heap.functor_arity(cell) == 2 &&
         machine.atoms.name(machine.heap.functor_atom(cell)) == ":";
}

bool neck(const Impl& machine, Cell cell) {
  return cell.tag() == Tag::Str && machine.heap.functor_arity(cell) == 2 &&
         machine.atoms.name(machine.heap.functor_atom(cell)) == ":-";
}

// 名前が同じならヒープを使わずに同じ項を返す。別名でない普通の呼び出しはここで何も作らない。
Cell rename_head(Impl& machine, Cell head, std::uint32_t name) {
  head = machine.heap.deref(head);
  if (head.tag() == Tag::Atom) {
    return static_cast<std::uint32_t>(head.raw() >> 3) == name ? head : machine.heap.atom(name);
  }
  if (head.tag() != Tag::Str || machine.heap.functor_atom(head) == name) return head;
  const std::uint16_t arity = machine.heap.functor_arity(head);
  std::vector<Cell> args;
  args.reserve(arity);
  for (std::uint16_t index = 0; index < arity; ++index) args.push_back(machine.heap.arg(head, index));
  return machine.heap.structure(name, args);
}

}  // namespace

std::optional<Cell> strip_module(Impl& machine, Cell& term, std::uint32_t& module) {
  term = machine.heap.deref(term);
  while (qualified(machine, term)) {
    const Cell name = machine.heap.deref(machine.heap.arg(term, 0));
    if (machine.heap.unbound(name)) return formal_instantiation(machine);
    if (name.tag() != Tag::Atom) return formal_type(machine, "atom", name);
    // module/2 だけがモジュールを作る。節操作で作ると、綴りの誤りが黙って新しい表になる。
    if (find_module(machine, static_cast<std::uint32_t>(name.raw() >> 3)) == nullptr) {
      return formal_existence(machine, "module", name);
    }
    module = static_cast<std::uint32_t>(name.raw() >> 3);
    term = machine.heap.deref(machine.heap.arg(term, 1));
  }
  return std::nullopt;
}

std::optional<Cell> strip_clause(Impl& machine, Cell& term, std::uint32_t& module) {
  if (const auto formal = strip_module(machine, term, module)) return formal;
  if (!neck(machine, term)) return std::nullopt;
  Cell head = machine.heap.deref(machine.heap.arg(term, 0));
  if (!qualified(machine, head)) return std::nullopt;
  if (const auto formal = strip_module(machine, head, module)) return formal;
  // 頭部の M: を外した節を作り直す。本体はそのまま、節の述語のモジュールで動く。
  const std::array<Cell, 2> parts{head, machine.heap.arg(term, 1)};
  term = machine.heap.structure(machine.heap.functor_atom(term), parts);
  return std::nullopt;
}

Cell skip_module(const Impl& machine, Cell term) {
  term = machine.heap.deref(term);
  while (qualified(machine, term)) term = machine.heap.deref(machine.heap.arg(term, 1));
  return term;
}

Predicate* owned_pred(Impl& machine, std::uint32_t& module, std::uint32_t& name, std::uint16_t arity) {
  if (Predicate* own = find_pred(machine, PredKey{module, name, arity})) return own;
  const Module* target = find_module(machine, module);
  if (target == nullptr) return nullptr;
  // 明示の取り込みだけをたどる。既定の取り込み元まで行くと、モジュールの assert が user を書き換える。
  for (const Import& item : target->imports) {
    if (item.alias != name || item.arity != arity) continue;
    // 取り込み元に定義がまだ無くても、作る先は取り込み元。ここで作ると取り込みを局所の述語が隠す。
    module = item.from;
    name = item.name;
    return find_pred(machine, PredKey{item.from, item.name, arity});
  }
  return nullptr;
}

Cell named_head(Impl& machine, Cell term, std::uint32_t name) {
  term = machine.heap.deref(term);
  if (!neck(machine, term)) return rename_head(machine, term, name);
  const Cell head = machine.heap.deref(machine.heap.arg(term, 0));
  const Cell renamed = rename_head(machine, head, name);
  if (renamed.raw() == head.raw()) return term;
  const std::array<Cell, 2> parts{renamed, machine.heap.arg(term, 1)};
  return machine.heap.structure(machine.heap.functor_atom(term), parts);
}

Cell module_indicator(Impl& machine, std::uint32_t module, std::uint32_t name, std::uint16_t arity) {
  const Cell indicator = cell_functor(machine, name, arity);
  if (module == machine.user_module) return indicator;
  const std::array<Cell, 2> parts{machine.heap.atom(module), indicator};
  return machine.heap.structure(machine.atoms.intern(":"), parts);
}

}
