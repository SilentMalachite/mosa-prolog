// ISO/IEC 13211-2 7.2.1 current_module/1 (?atom)
//   type_error(atom, M)
// ISO/IEC 13211-2 7.2.2 predicate_property/2 (+callable, ?property)
//   instantiation_error, type_error(callable, H), domain_error(predicate_property, P),
//   type_error(atom, M), existence_error(module, M)
// 性質は built_in、defined、dynamic、static、exported、imported_from(M)、multifile、number_of_clauses(N)。
// テスト: tests/pl/modules.pl
#include "../machine/internal.hpp"

#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

namespace mosa {

namespace {

Cell atom_cell(Impl& machine, std::string_view name) { return machine.heap.atom(machine.atoms.intern(name)); }

Cell unary(Impl& machine, std::string_view name, Cell arg) {
  return machine.heap.structure(machine.atoms.intern(name), std::array<Cell, 1>{arg});
}

void raise(Impl& machine, const Goal& goal, Cell formal) { raise_formal(machine, formal, goal.name, goal.arity); }

// 候補を (T = C1 ; T = C2 ; ...) にして走らせる。選択点と束縛の戻しは選言に任せる。
void offer(Impl& machine, const Goal& goal, Cell target, const std::vector<Cell>& choices) {
  if (choices.empty()) {
    machine.failed = true;
    return;
  }
  const auto equals = machine.atoms.intern("=");
  const auto either = machine.atoms.intern(";");
  Cell term = machine.heap.structure(equals, std::array<Cell, 2>{target, choices.back()});
  for (auto it = choices.rbegin() + 1; it != choices.rend(); ++it) {
    const Cell head = machine.heap.structure(equals, std::array<Cell, 2>{target, *it});
    term = machine.heap.structure(either, std::array<Cell, 2>{head, term});
  }
  const Compiled compiled = compile_goal(machine.heap, term);
  launch(machine, compiled.term, goal.cont, machine.stack.size(), compiled.env, goal.module);
}

bool run_current_module(Impl& machine, const Goal& goal) {
  const Cell module = machine.heap.deref(goal.arg(machine, 0));
  if (!machine.heap.unbound(module) && module.tag() != Tag::Atom) {
    raise(machine, goal, formal_type(machine, "atom", module));
    return true;
  }
  std::vector<Cell> names;
  for (const Module& item : machine.modules) names.push_back(machine.heap.atom(item.name));
  offer(machine, goal, module, names);
  return true;
}

// 性質の形だけを見る。引数の値は単一化に任せ、合わなければ失敗する。
bool known_property(const Impl& machine, Cell property) {
  property = machine.heap.deref(property);
  if (machine.heap.unbound(property)) return true;
  if (property.tag() == Tag::Atom) {
    const auto name = machine.atoms.name(static_cast<std::uint32_t>(property.raw() >> 3));
    return name == "built_in" || name == "defined" || name == "dynamic" || name == "static" || name == "exported" ||
           name == "multifile";
  }
  if (property.tag() != Tag::Str || machine.heap.functor_arity(property) != 1) return false;
  const auto name = machine.atoms.name(machine.heap.functor_atom(property));
  // TODO(iso): 13211-2 7.2.2 meta_predicate(Head) は形だけ受ける。値は Task 9 の meta_predicate/1 で持たせる。
  return name == "imported_from" || name == "number_of_clauses" || name == "meta_predicate";
}

bool exported_by(const Impl& machine, const Predicate& predicate) {
  const Module* owner = find_module(machine, predicate.module);
  if (owner == nullptr) return false;
  for (const PredKey& key : owner->exports) {
    if (key.name == predicate.name && key.arity == predicate.arity) return true;
  }
  return false;
}

// 明示の取り込みで見えるときだけ imported_from。既定の取り込み元から見える述語は取り込みではない。
const Import* import_of(const Impl& machine, std::uint32_t module, std::uint32_t name, std::uint16_t arity) {
  const Module* context = find_module(machine, module);
  if (context == nullptr) return nullptr;
  for (const Import& item : context->imports) {
    if (item.alias == name && item.arity == arity) return &item;
  }
  return nullptr;
}

bool shared(const Impl& machine, const Predicate& predicate) {
  for (const SharedPred& item : machine.multifile) {
    if (item.module == predicate.module && item.name == predicate.name && item.arity == predicate.arity) return true;
  }
  return false;
}

std::vector<Cell> user_properties(Impl& machine, std::uint32_t module, const Predicate& predicate,
                                  std::uint32_t name) {
  std::vector<Cell> found{atom_cell(machine, "defined"), atom_cell(machine, predicate.dynamic ? "dynamic" : "static")};
  if (exported_by(machine, predicate)) found.push_back(atom_cell(machine, "exported"));
  if (const Import* item = import_of(machine, module, name, predicate.arity)) {
    found.push_back(unary(machine, "imported_from", machine.heap.atom(item->from)));
  }
  if (shared(machine, predicate)) found.push_back(atom_cell(machine, "multifile"));
  const auto count = static_cast<std::int64_t>(predicate.clauses.size());
  found.push_back(unary(machine, "number_of_clauses", machine.heap.integer(count)));
  return found;
}

bool run_predicate_property(Impl& machine, const Goal& goal) {
  Cell head = goal.arg(machine, 0);
  std::uint32_t module = goal.module;
  if (const auto formal = strip_module(machine, head, module)) {
    raise(machine, goal, *formal);
    return true;
  }
  if (machine.heap.unbound(head)) {
    raise(machine, goal, formal_instantiation(machine));
    return true;
  }
  if (head.tag() != Tag::Atom && head.tag() != Tag::Str && head.tag() != Tag::Lis) {
    raise(machine, goal, formal_type(machine, "callable", head));
    return true;
  }
  const Cell property = goal.arg(machine, 1);
  if (!known_property(machine, property)) {
    raise(machine, goal, formal_domain(machine, "predicate_property", machine.heap.deref(property)));
    return true;
  }
  const std::uint32_t name = head.tag() == Tag::Atom  ? static_cast<std::uint32_t>(head.raw() >> 3)
                             : head.tag() == Tag::Lis ? machine.atoms.intern(".")
                                                      : machine.heap.functor_atom(head);
  const std::uint16_t arity = head.tag() == Tag::Atom ? 0 : head.tag() == Tag::Lis ? 2 : machine.heap.functor_arity(head);
  // 組込は述語表より先に引かれる。同じ名前の利用者の述語があっても、呼ばれるのは組込である。
  if (is_builtin_procedure(machine, name, arity)) {
    offer(machine, goal, property,
          {atom_cell(machine, "defined"), atom_cell(machine, "built_in"), atom_cell(machine, "static")});
    return true;
  }
  const Predicate* predicate = resolve_pred(machine, module, name, arity);
  if (predicate == nullptr) {
    machine.failed = true;
    return true;
  }
  offer(machine, goal, property, user_properties(machine, module, *predicate, name));
  return true;
}

}  // namespace

bool run_module_prop(Impl& machine, const Goal& goal) {
  const auto name = machine.atoms.name(goal.name);
  if (name == "current_module" && goal.arity == 1) return run_current_module(machine, goal);
  if (name == "predicate_property" && goal.arity == 2) return run_predicate_property(machine, goal);
  return false;
}

}
