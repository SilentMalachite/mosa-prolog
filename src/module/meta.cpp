// docs/SPEC.md 第5節
// meta_predicate/1 は指示。述語ではない。
//   0 と : は呼び出し元:引数。^ は Vars^Goal の Goal だけ。
//   // は 名前/アリティ を呼び出し元:名前/アリティ にする。? はそのまま。
//   call/1..8、findall/3..4、bagof/3、setof/3、catch/3、once/1、\+/1 も同じ境界で包む。
//   すでに :/2 なら包み直さない。
// テスト: tests/pl/modules.pl

#include "../machine/internal.hpp"

#include <array>
#include <cstdint>
#include <string_view>

namespace mosa {
namespace {

bool directive_body(const Impl& machine, Cell term, std::string_view name, Cell& body) {
  term = machine.heap.deref(term);
  if (!is_directive(machine.heap, term)) return false;
  const Cell head = machine.heap.deref(machine.heap.arg(term, 0));
  if (head.tag() != Tag::Str || machine.heap.functor_arity(head) != 1) return false;
  if (machine.atoms.name(machine.heap.functor_atom(head)) != name) return false;
  body = machine.heap.deref(machine.heap.arg(head, 0));
  return true;
}

bool digit_mode(const Impl& machine, Cell cell, std::uint8_t& mode) {
  cell = machine.heap.deref(cell);
  if (cell.tag() != Tag::Int && cell.tag() != Tag::IntBox) return false;
  const auto value = machine.heap.integer_value(cell);
  if (value < 0 || value > 9) return false;
  mode = static_cast<std::uint8_t>(kMetaDigit + value);
  return true;
}

bool atom_mode(const Impl& machine, Cell cell, std::string_view text) {
  cell = machine.heap.deref(cell);
  return cell.tag() == Tag::Atom && machine.atoms.name(static_cast<std::uint32_t>(cell.raw() >> 3)) == text;
}

bool plain_mode(const Impl& machine, Cell cell, std::uint8_t& mode) {
  if (atom_mode(machine, cell, "?")) {
    mode = kMetaAsk;
    return true;
  }
  if (atom_mode(machine, cell, "+")) {
    mode = kMetaPlus;
    return true;
  }
  if (atom_mode(machine, cell, "-")) {
    mode = kMetaMinus;
    return true;
  }
  if (atom_mode(machine, cell, "*")) {
    mode = kMetaStar;
    return true;
  }
  return false;
}

bool read_modes(Impl& machine, Cell head, std::uint32_t& name, std::vector<std::uint8_t>& modes) {
  head = machine.heap.deref(head);
  if (head.tag() != Tag::Str) return false;
  name = machine.heap.functor_atom(head);
  const auto arity = machine.heap.functor_arity(head);
  modes.clear();
  modes.reserve(arity);
  for (std::uint16_t index = 0; index < arity; ++index) {
    const Cell arg = machine.heap.arg(head, index);
    std::uint8_t mode = 0;
    if (digit_mode(machine, arg, mode) || plain_mode(machine, arg, mode)) modes.push_back(mode);
    else if (atom_mode(machine, arg, ":")) modes.push_back(kMetaColon);
    else if (atom_mode(machine, arg, "^")) modes.push_back(kMetaCaret);
    else if (atom_mode(machine, arg, "//")) modes.push_back(kMetaSlash);
    else return false;
  }
  return true;
}

void keep_meta(Impl& machine, std::uint32_t name, const std::vector<std::uint8_t>& modes) {
  const auto arity = static_cast<std::uint16_t>(modes.size());
  Predicate* predicate = predicate_in(machine, machine.load_home, name, arity);
  if (predicate == nullptr) {
    Predicate created;
    created.home = machine.load_home;
    created.name = name;
    created.arity = arity;
    created.meta = modes;
    stamp_load(machine, created);
    machine.predicates.push_back(std::move(created));
    return;
  }
  predicate->meta = modes;
}

bool qualified(const Impl& machine, Cell cell) {
  cell = machine.heap.deref(cell);
  return cell.tag() == Tag::Str && machine.heap.functor_arity(cell) == 2 &&
         machine.atoms.name(machine.heap.functor_atom(cell)) == ":";
}

bool named_binary(const Impl& machine, Cell cell, std::string_view name) {
  cell = machine.heap.deref(cell);
  return cell.tag() == Tag::Str && machine.heap.functor_arity(cell) == 2 &&
         machine.atoms.name(machine.heap.functor_atom(cell)) == name;
}

Cell qualify_cell(Impl& machine, std::uint32_t colon, Cell arg) {
  if (qualified(machine, arg)) return arg;
  const std::array<Cell, 2> args{machine.heap.atom(machine.calling), arg};
  return machine.heap.structure(colon, args);
}

Cell wrap_caret(Impl& machine, std::uint32_t colon, Cell arg) {
  if (qualified(machine, arg)) return arg;
  // A^B^Goal は各 ^ を残し、一番内側の目標だけを包む。外側で包むと bagof が存在量化を落とす。
  if (!named_binary(machine, arg, "^")) return qualify_cell(machine, colon, arg);
  const Cell goal = wrap_caret(machine, colon, machine.heap.deref(machine.heap.arg(arg, 1)));
  const std::array<Cell, 2> args{machine.heap.arg(arg, 0), goal};
  return machine.heap.structure(machine.atoms.intern("^"), args);
}

Cell wrap_slash(Impl& machine, std::uint32_t colon, Cell arg) {
  if (qualified(machine, arg) || !named_binary(machine, arg, "/")) return arg;
  return qualify_cell(machine, colon, arg);
}

}  // namespace

bool store_spec(Impl& machine, Cell spec) {
  spec = machine.heap.deref(spec);
  if (named_binary(machine, spec, ",")) {
    return store_spec(machine, machine.heap.arg(spec, 0)) && store_spec(machine, machine.heap.arg(spec, 1));
  }
  std::uint32_t name = 0;
  std::vector<std::uint8_t> modes;
  if (!read_modes(machine, spec, name, modes)) return false;
  keep_meta(machine, name, modes);
  return true;
}

int apply_meta_directive(Impl& machine, Cell term) {
  Cell spec{};
  if (!directive_body(machine, term, "meta_predicate", spec)) return 0;
  if (!store_spec(machine, spec)) {
    machine.diagnostic = "メタ引数の指定ではない";
    machine.syntax_diag = false;
    return -1;
  }
  return 1;
}

bool builtin_meta(std::string_view name, std::uint16_t arity, std::vector<std::uint8_t>& modes) {
  modes.assign(arity, kMetaAsk);
  if (name == "call" && arity >= 1 && arity <= 8) {
    modes[0] = kMetaColon;
    return true;
  }
  if (name == "findall" && (arity == 3 || arity == 4)) {
    modes[1] = kMetaColon;
    return true;
  }
  if ((name == "bagof" || name == "setof") && arity == 3) {
    modes[1] = kMetaCaret;
    return true;
  }
  if (name == "catch" && arity == 3) {
    modes[0] = kMetaColon;
    modes[2] = kMetaColon;
    return true;
  }
  if ((name == "once" || name == "\\+") && arity == 1) {
    modes[0] = kMetaColon;
    return true;
  }
  return false;
}

void wrap_builtin(Impl& machine, const Goal& goal) {
  std::vector<std::uint8_t> modes;
  if (!builtin_meta(machine.atoms.name(goal.name), goal.arity, modes)) return;
  Predicate spec;
  spec.meta = std::move(modes);
  wrap_meta(machine, goal, spec);
}

void wrap_meta(Impl& machine, const Goal& goal, const Predicate& predicate) {
  if (predicate.meta.size() != goal.arity) return;
  const auto colon = machine.atoms.intern(":");
  for (std::uint16_t index = 0; index < goal.arity; ++index) {
    const auto mode = predicate.meta[index];
    // ? + - * は包まない。^ と // は内側だけ。: と 0 から 9 は項全体。
    if (mode <= kMetaStar) continue;
    const auto slot = goal.at + kFrameHead + index;
    const auto born = machine.heap.size();
    const Cell arg = machine.heap.deref(goal.arg(machine, index));
    Cell wrapped = arg;
    if (mode == kMetaCaret) wrapped = wrap_caret(machine, colon, arg);
    else if (mode == kMetaSlash) wrapped = wrap_slash(machine, colon, arg);
    else wrapped = qualify_cell(machine, colon, arg);
    if (wrapped.raw() == machine.heap.word_at(slot)) continue;
    machine.meta_undos.push_back(MetaUndo{slot, machine.heap.word_at(slot), born});
    machine.heap.set_word(slot, wrapped.raw());
  }
}

}  // namespace mosa
