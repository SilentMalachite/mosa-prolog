#include "mosa/errors.hpp"

#include "../machine/internal.hpp"

#include <array>

namespace mosa {
namespace {

Cell pair(Impl& machine, std::string_view name, Cell left, Cell right) {
  const std::array<Cell, 2> args{left, right};
  return machine.heap.structure(machine.atoms.intern(name), args);
}

}  // namespace

Cell formal_instantiation(Impl& machine) {
  return machine.heap.atom(machine.atoms.intern("instantiation_error"));
}

Cell formal_system(Impl& machine) { return machine.heap.atom(machine.atoms.intern("system_error")); }

Cell formal_uninstantiation(Impl& machine, Cell culprit) {
  return machine.heap.structure(machine.atoms.intern("uninstantiation_error"), std::array<Cell, 1>{culprit});
}

Cell formal_type(Impl& machine, std::string_view type, Cell culprit) {
  return pair(machine, "type_error", machine.heap.atom(machine.atoms.intern(type)), culprit);
}

Cell formal_domain(Impl& machine, std::string_view domain, Cell culprit) {
  return pair(machine, "domain_error", machine.heap.atom(machine.atoms.intern(domain)), culprit);
}

Cell formal_existence(Impl& machine, std::string_view kind, Cell culprit) {
  return pair(machine, "existence_error", machine.heap.atom(machine.atoms.intern(kind)), culprit);
}

Cell formal_representation(Impl& machine, std::string_view which) {
  return machine.heap.structure(machine.atoms.intern("representation_error"),
                                std::array<Cell, 1>{machine.heap.atom(machine.atoms.intern(which))});
}

Cell formal_syntax(Impl& machine, Cell detail) {
  return machine.heap.structure(machine.atoms.intern("syntax_error"), std::array<Cell, 1>{detail});
}

Cell formal_permission(Impl& machine, std::string_view action, std::string_view type, Cell culprit) {
  const std::array<Cell, 3> args{machine.heap.atom(machine.atoms.intern(action)),
                                 machine.heap.atom(machine.atoms.intern(type)), culprit};
  return machine.heap.structure(machine.atoms.intern("permission_error"), args);
}

Cell formal_evaluation(Impl& machine, std::string_view which) {
  return machine.heap.structure(machine.atoms.intern("evaluation_error"),
                                std::array<Cell, 1>{machine.heap.atom(machine.atoms.intern(which))});
}

Cell cell_functor(Impl& machine, std::uint32_t name, std::uint16_t arity) {
  return pair(machine, "/", machine.heap.atom(name), machine.heap.integer(arity));
}

void raise_formal(Impl& machine, Cell formal, std::uint32_t name, std::uint16_t arity) {
  machine.eval_abort = true;
  const Cell context = pair(machine, "context", cell_functor(machine, name, arity), machine.heap.fresh_ref());
  propagate_throw(machine, pair(machine, "error", formal, context));
}

Cell make_instantiation_error(Machine& machine) { return formal_instantiation(engine_impl(machine)); }

Cell make_type_error(Machine& machine, std::string_view type, Cell culprit) {
  return formal_type(engine_impl(machine), type, culprit);
}

Cell make_domain_error(Machine& machine, std::string_view domain, Cell culprit) {
  return formal_domain(engine_impl(machine), domain, culprit);
}

Cell make_existence_error(Machine& machine, std::string_view kind, Cell culprit) {
  return formal_existence(engine_impl(machine), kind, culprit);
}

Cell make_evaluation_error(Machine& machine, std::string_view which) {
  return formal_evaluation(engine_impl(machine), which);
}

Cell make_permission_error(Machine& machine, std::string_view action, std::string_view type, Cell culprit) {
  return formal_permission(engine_impl(machine), action, type, culprit);
}

Cell throw_error(Machine& machine, Cell formal, std::uint32_t name, std::uint16_t arity) {
  Impl& impl = engine_impl(machine);
  raise_formal(impl, formal, name, arity);
  return impl.exception ? impl.ball : formal;
}

}
