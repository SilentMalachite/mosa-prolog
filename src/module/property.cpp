// docs/SPEC.md 第5節
// predicate_property(+callable, ?property)
//   instantiation_error, type_error(callable), type_error(atom), existence_error(module)
//   defined_in(Module), exported, imported_from(Module), meta_predicate(Head), dynamic, multifile
// テスト: tests/pl/modules.pl

#include "../machine/internal.hpp"

#include <array>
#include <vector>

namespace mosa {
namespace {

bool colon_goal(const Impl& machine, Cell cell) {
  cell = machine.heap.deref(cell);
  return cell.tag() == Tag::Str && machine.heap.functor_arity(cell) == 2 &&
         machine.atoms.name(machine.heap.functor_atom(cell)) == ":";
}

bool take_module(Impl& machine, const Goal& goal, Cell module, std::uint32_t& home) {
  module = machine.heap.deref(module);
  if (machine.heap.unbound(module)) {
    raise_formal(machine, formal_instantiation(machine), goal.name, goal.arity);
    return false;
  }
  if (module.tag() != Tag::Atom) {
    raise_formal(machine, formal_type(machine, "atom", module), goal.name, goal.arity);
    return false;
  }
  home = static_cast<std::uint32_t>(module.raw() >> 3);
  if (!module_loaded(machine, home)) {
    raise_formal(machine, formal_existence(machine, "module", module), goal.name, goal.arity);
    return false;
  }
  return true;
}

// M1:M2:Goal は内側の M2 を残す。モジュールが使えなければ誤りを送って偽。
bool peel_goal(Impl& machine, const Goal& goal, Cell target, std::uint32_t& home, Cell& inner) {
  target = machine.heap.deref(target);
  home = machine.calling;
  inner = target;
  while (colon_goal(machine, target)) {
    if (!take_module(machine, goal, machine.heap.arg(target, 0), home)) return false;
    inner = machine.heap.deref(machine.heap.arg(target, 1));
    target = inner;
  }
  return true;
}

bool goal_pi(const Impl& machine, Cell goal, std::uint32_t& name, std::uint16_t& arity) {
  goal = machine.heap.deref(goal);
  if (goal.tag() == Tag::Atom) {
    name = static_cast<std::uint32_t>(goal.raw() >> 3);
    arity = 0;
    return true;
  }
  if (goal.tag() != Tag::Str) return false;
  name = machine.heap.functor_atom(goal);
  arity = machine.heap.functor_arity(goal);
  return true;
}

bool accept_goal(Impl& machine, const Goal& goal, Cell inner, std::uint32_t& name, std::uint16_t& arity) {
  inner = machine.heap.deref(inner);
  // 変数は型より先。未束縛を callable の型誤りにしない。
  if (machine.heap.unbound(inner)) {
    raise_formal(machine, formal_instantiation(machine), goal.name, goal.arity);
    return false;
  }
  if (goal_pi(machine, inner, name, arity)) return true;
  raise_formal(machine, formal_type(machine, "callable", inner), goal.name, goal.arity);
  return false;
}

Cell named_module(Impl& machine, std::string_view functor, std::uint32_t module) {
  const std::array<Cell, 1> args{machine.heap.atom(module)};
  return machine.heap.structure(machine.atoms.intern(functor), args);
}

Cell mode_cell(Impl& machine, std::uint8_t mode) {
  if (mode >= kMetaDigit && mode <= kMetaDigit + 9) return machine.heap.integer(mode - kMetaDigit);
  std::string_view text = "?";
  if (mode == kMetaPlus) text = "+";
  else if (mode == kMetaMinus) text = "-";
  else if (mode == kMetaStar) text = "*";
  else if (mode == kMetaColon) text = ":";
  else if (mode == kMetaCaret) text = "^";
  else if (mode == kMetaSlash) text = "//";
  return machine.heap.atom(machine.atoms.intern(text));
}

Cell meta_head(Impl& machine, std::uint32_t name, const std::vector<std::uint8_t>& modes) {
  std::vector<Cell> args;
  args.reserve(modes.size());
  for (const auto mode : modes) args.push_back(mode_cell(machine, mode));
  return machine.heap.structure(name, args);
}

const Predicate* defined_predicate(Impl& machine, const Predicate& slot, std::uint32_t home,
                                   std::uint32_t def_name) {
  if (!slot.imported) return &slot;
  // 仕様は定義した側にあり、取り込みの席には写していない。
  const Predicate* origin = predicate_in(machine, home, def_name, slot.arity);
  return origin != nullptr ? origin : &slot;
}

bool mark_exported(const Impl& machine, std::uint32_t module, std::uint32_t name, std::uint16_t arity) {
  for (const ExportMark& item : machine.exports) {
    if (item.module == module && item.name == name && item.arity == arity) return true;
  }
  return false;
}

void add_props(Impl& machine, const Predicate& slot, std::vector<Cell>& props) {
  // 別名の席は、公開リストでは出所の名前で載っている。
  const auto home = slot.imported ? slot.origin : slot.home;
  const auto def_name = slot.imported && slot.origin_name != 0 ? slot.origin_name : slot.name;
  props.push_back(named_module(machine, "defined_in", home));
  if (mark_exported(machine, home, def_name, slot.arity)) {
    props.push_back(machine.heap.atom(machine.atoms.intern("exported")));
  }
  if (slot.imported) props.push_back(named_module(machine, "imported_from", slot.origin));
  const Predicate* defined = defined_predicate(machine, slot, home, def_name);
  if (!defined->meta.empty()) {
    const std::array<Cell, 1> args{meta_head(machine, defined->name, defined->meta)};
    props.push_back(machine.heap.structure(machine.atoms.intern("meta_predicate"), args));
  }
  if (defined->dynamic) props.push_back(machine.heap.atom(machine.atoms.intern("dynamic")));
  if (multifile_keeps(machine, home, def_name, slot.arity)) {
    props.push_back(machine.heap.atom(machine.atoms.intern("multifile")));
  }
}

constexpr std::uint8_t kPropGen = 7;

void push_props(Impl& machine, const Goal& goal, const std::vector<Cell>& props, std::size_t next,
                std::size_t heap_mark, std::size_t trail_mark) {
  Entry entry;
  entry.pick = Pick::AtomGen;
  entry.gen_kind = kPropGen;
  entry.next = next;
  entry.redo_goal = goal.at;
  entry.heap_mark = heap_mark;
  entry.trail_mark = trail_mark;
  entry.candidates.reserve(props.size());
  for (const Cell& prop : props) entry.candidates.push_back(static_cast<std::size_t>(prop.raw()));
  push_frame(machine, std::move(entry));
}

bool prove_props(Impl& machine, const Goal& goal, const std::vector<Cell>& props, std::size_t begin) {
  for (std::size_t index = begin; index < props.size(); ++index) {
    const auto heap_mark = machine.heap.size();
    const auto trail_mark = machine.trail.slots.size();
    const auto extras_mark = machine.extras.size();
    const bool more = index + 1 < props.size();
    if (more) push_props(machine, goal, props, index + 1, heap_mark, trail_mark);
    if (do_unify(machine, goal.arg(machine, 1), props[index])) {
      machine.ip = goal.cont;
      return true;
    }
    if (more) machine.stack.pop_back();
    restore_state(machine, heap_mark, trail_mark, extras_mark);
  }
  return false;
}

}  // namespace

bool run_property(Impl& machine, const Goal& goal) {
  if (machine.atoms.name(goal.name) != "predicate_property" || goal.arity != 2) return false;
  std::uint32_t home = 0;
  Cell inner;
  if (!peel_goal(machine, goal, goal.arg(machine, 0), home, inner)) return true;
  std::uint32_t name = 0;
  std::uint16_t arity = 0;
  if (!accept_goal(machine, goal, inner, name, arity)) return true;
  const Predicate* slot = predicate_in(machine, home, name, arity);
  if (slot == nullptr) {
    machine.failed = true;
    return true;
  }
  std::vector<Cell> props;
  add_props(machine, *slot, props);
  if (!prove_props(machine, goal, props, 0)) machine.failed = true;
  return true;
}

bool redo_property(Impl& machine) {
  if (machine.stack.empty() || machine.stack.back().pick != Pick::AtomGen) return false;
  if (machine.stack.back().gen_kind != kPropGen) return false;
  apply_catches(machine, machine.stack.back());
  const Entry top = machine.stack.back();
  machine.stack.pop_back();
  std::vector<Cell> props;
  props.reserve(top.candidates.size());
  for (const auto raw : top.candidates) props.push_back(Cell{static_cast<std::uint64_t>(raw)});
  return prove_props(machine, read_goal(machine, top.redo_goal), props, top.next);
}

}  // namespace mosa
