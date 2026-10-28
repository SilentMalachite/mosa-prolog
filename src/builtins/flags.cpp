// 8.17.1 set_prolog_flag(+atom, +term)
//   instantiation_error, type_error(atom), domain_error(prolog_flag),
//   domain_error(flag_value), permission_error(modify, flag, Flag)
// 8.17.2 current_prolog_flag(?atom, ?term)
//   type_error(atom), domain_error(prolog_flag)
// char_conversion は on と off。既定は off。warning と error 以外の浮動小数旗も受け付けない。
// invalid_utf8 は error と replace。既定は error。replace は壊れたバイトを U+FFFD にする。
// debug は on と off。既定は off。値を覚えるだけで、節の追跡はしない。
// japanese_punctuation は true と false。字句解析器は次に読む項から全角記号を半角に読む（iso-notes 第4節）。
// dialect は mosa、version_data は mosa(Major, Minor, Patch, [])。どちらも変えられない（iso-notes 第3節の決定 B）。
// version は Major*10000 + Minor*100 + Patch の整数。変えられない（SPEC 第3節 7）。
// message_language は ja と en。既定は LANG から決める（ADR 0007）。
// テスト: tests/pl/flags.pl

#include "../machine/internal.hpp"

#include "mosa/version.hpp"
#include "mosa/writer.hpp"

#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>

namespace mosa {
namespace {

constexpr std::size_t kFlagCount = 19;
constexpr std::uint8_t kFlagGen = 3;

std::uint32_t atom_id(Cell cell) { return static_cast<std::uint32_t>(cell.raw() >> 3); }

Cell atom_named(Impl& machine, std::string_view name) {
  return machine.heap.atom(machine.atoms.intern(name));
}

std::string_view flag_name(std::size_t index) {
  switch (index) {
    case 0: return "bounded";
    case 1: return "integer_rounding_function";
    case 2: return "max_integer";
    case 3: return "min_integer";
    case 4: return "max_arity";
    case 5: return "double_quotes";
    case 6: return "unknown";
    case 7: return "encoding";
    case 8: return "japanese_punctuation";
    case 9: return "float_overflow";
    case 10: return "float_zero_div";
    case 11: return "float_undefined";
    case 12: return "char_conversion";
    case 13: return "invalid_utf8";
    case 14: return "debug";
    case 15: return "dialect";
    case 16: return "version_data";
    case 17: return "version";
    case 18: return "message_language";
    default: return {};
  }
}

bool flag_writable(std::size_t index) { return index == 5 || index == 6 || index == 8 || (index >= 9 && index <= 14) || index == 18; }

int find_flag(const Impl& machine, Cell flag) {
  const auto text = machine.atoms.name(atom_id(flag));
  for (std::size_t index = 0; index < kFlagCount; ++index) {
    if (flag_name(index) == text) return static_cast<int>(index);
  }
  return -1;
}

Cell flag_value(Impl& machine, std::size_t index) {
  switch (index) {
    case 0: return atom_named(machine, "true");
    case 1: return atom_named(machine, "toward_zero");
    case 2: return machine.heap.integer(std::numeric_limits<std::int64_t>::max());
    case 3: return machine.heap.integer(std::numeric_limits<std::int64_t>::min());
    case 4: return machine.heap.integer(1024);
    case 5:
      if (machine.quotes == DoubleQuotes::Chars) return atom_named(machine, "chars");
      if (machine.quotes == DoubleQuotes::Atom) return atom_named(machine, "atom");
      return atom_named(machine, "codes");
    case 6: return atom_named(machine, machine.unknown_fail ? "fail" : "error");
    case 7: return atom_named(machine, "utf8");
    case 8: return atom_named(machine, machine.japanese_punct ? "true" : "false");
    case 12: return atom_named(machine, machine.char_conv.enabled ? "on" : "off");
    case 13: return atom_named(machine, machine.utf8_replace ? "replace" : "error");
    case 14: return atom_named(machine, machine.debug_on ? "on" : "off");
    case 15: return atom_named(machine, "mosa");
    case 16: {
      const std::array<Cell, 4> args{machine.heap.integer(kVersionMajor), machine.heap.integer(kVersionMinor),
                                     machine.heap.integer(kVersionPatch), atom_named(machine, "[]")};
      return machine.heap.structure(machine.atoms.intern("mosa"), args);
    }
    case 17: return machine.heap.integer(kVersionMajor * 10000 + kVersionMinor * 100 + kVersionPatch);
    case 18: return atom_named(machine, machine.message_lang == MessageLanguage::Ja ? "ja" : "en");
    default: return atom_named(machine, "error");
  }
}

bool same_atom(const Impl& machine, Cell value, std::string_view name) {
  return value.tag() == Tag::Atom && machine.atoms.name(atom_id(value)) == name;
}

bool same_int(const Impl& machine, Cell value, std::int64_t expected) {
  return (value.tag() == Tag::Int || value.tag() == Tag::IntBox) &&
         machine.heap.integer_value(value) == expected;
}

bool flag_accepts(Cell value, const Impl& machine, std::size_t index) {
  value = machine.heap.deref(value);
  switch (index) {
    case 0: return same_atom(machine, value, "true") || same_atom(machine, value, "false");
    case 1: return same_atom(machine, value, "toward_zero");
    case 2: return same_int(machine, value, std::numeric_limits<std::int64_t>::max());
    case 3: return same_int(machine, value, std::numeric_limits<std::int64_t>::min());
    case 4: return same_int(machine, value, 1024);
    case 5: {
      if (value.tag() != Tag::Atom) return false;
      const auto text = machine.atoms.name(atom_id(value));
      return text == "codes" || text == "chars" || text == "atom";
    }
    case 6: {
      if (value.tag() != Tag::Atom) return false;
      const auto text = machine.atoms.name(atom_id(value));
      return text == "error" || text == "fail";
    }
    case 7: return same_atom(machine, value, "utf8");
    case 8: return same_atom(machine, value, "true") || same_atom(machine, value, "false");
    case 12: return same_atom(machine, value, "on") || same_atom(machine, value, "off");
    case 13: return same_atom(machine, value, "error") || same_atom(machine, value, "replace");
    case 14: return same_atom(machine, value, "on") || same_atom(machine, value, "off");
    // 変えられない旗の領域は値の形。形が合えば、次の permission_error になる。
    case 15: return value.tag() == Tag::Atom;
    case 16: return value.tag() == Tag::Str && machine.heap.functor_arity(value) == 4;
    case 18: return same_atom(machine, value, "ja") || same_atom(machine, value, "en");
    default: return same_atom(machine, value, "error");
  }
}

// 8.17.1.3 は領域が先で、変更できないのはその次。整数旗の領域は今の値ではなく整数である。
bool value_admissible(Cell value, const Impl& machine, std::size_t index) {
  value = machine.heap.deref(value);
  if (index == 2 || index == 3 || index == 4 || index == 17) {
    return value.tag() == Tag::Int || value.tag() == Tag::IntBox;
  }
  return flag_accepts(value, machine, index);
}

void flag_store(Impl& machine, std::size_t index, Cell value) {
  const auto text = machine.atoms.name(atom_id(value));
  if (index == 5) {
    if (text == "chars") machine.quotes = DoubleQuotes::Chars;
    else if (text == "atom") machine.quotes = DoubleQuotes::Atom;
    else machine.quotes = DoubleQuotes::Codes;
    return;
  }
  if (index == 6) machine.unknown_fail = text == "fail";
  if (index == 8) machine.japanese_punct = text == "true";
  if (index == 12) machine.char_conv.enabled = text == "on";
  if (index == 13) machine.utf8_replace = text == "replace";
  if (index == 14) machine.debug_on = text == "on";
  if (index == 18) machine.message_lang = text == "ja" ? MessageLanguage::Ja : MessageLanguage::En;
}

void type_at(Impl& machine, const Goal& goal, Cell culprit) {
  raise_formal(machine, formal_type(machine, "atom", culprit), goal.name, goal.arity);
}

void succeed(Impl& machine, const Goal& goal) { machine.ip = goal.cont; }

std::optional<Cell> set_problem(Impl& machine, Cell flag, Cell value) {
  if (machine.heap.unbound(flag) || machine.heap.unbound(value)) return formal_instantiation(machine);
  if (flag.tag() != Tag::Atom) return formal_type(machine, "atom", flag);
  const int index = find_flag(machine, flag);
  if (index < 0) return formal_domain(machine, "prolog_flag", flag);
  const auto at = static_cast<std::size_t>(index);
  if (!value_admissible(value, machine, at)) {
    const std::array<Cell, 2> args{flag, value};
    const Cell both = machine.heap.structure(machine.atoms.intern("+"), args);
    return formal_domain(machine, "flag_value", both);
  }
  if (!flag_writable(at)) return formal_permission(machine, "modify", "flag", flag);
  return std::nullopt;
}

void push_flag(Impl& machine, const Goal& goal, std::size_t next, std::size_t heap_mark, std::size_t trail_mark) {
  Entry entry;
  entry.pick = Pick::AtomGen;
  entry.gen_kind = kFlagGen;
  entry.next = next;
  entry.redo_goal = goal.at;
  entry.heap_mark = heap_mark;
  entry.trail_mark = trail_mark;
  push_frame(machine, std::move(entry));
}

bool bind_flag(Impl& machine, const Goal& goal, std::size_t index) {
  return do_unify(machine, goal.arg(machine, 0), atom_named(machine, flag_name(index))) &&
         do_unify(machine, goal.arg(machine, 1), flag_value(machine, index));
}

bool prove_flags(Impl& machine, const Goal& goal, std::size_t begin) {
  for (std::size_t index = begin; index < kFlagCount; ++index) {
    const auto heap_mark = machine.heap.size();
    const auto trail_mark = machine.trail.slots.size();
    const auto extras_mark = machine.extras.size();
    const bool more = index + 1 < kFlagCount;
    if (more) push_flag(machine, goal, index + 1, heap_mark, trail_mark);
    if (bind_flag(machine, goal, index)) {
      succeed(machine, goal);
      return true;
    }
    if (more) machine.stack.pop_back();
    restore_state(machine, heap_mark, trail_mark, extras_mark);
  }
  return false;
}

void run_current(Impl& machine, const Goal& goal) {
  const Cell flag = machine.heap.deref(goal.arg(machine, 0));
  if (!machine.heap.unbound(flag) && flag.tag() != Tag::Atom) {
    type_at(machine, goal, flag);
    return;
  }
  if (machine.heap.unbound(flag)) {
    if (!prove_flags(machine, goal, 0)) machine.failed = true;
    return;
  }
  const int index = find_flag(machine, flag);
  if (index < 0) {
    raise_formal(machine, formal_domain(machine, "prolog_flag", flag), goal.name, goal.arity);
    return;
  }
  if (!do_unify(machine, goal.arg(machine, 1), flag_value(machine, static_cast<std::size_t>(index)))) {
    machine.failed = true;
  } else {
    succeed(machine, goal);
  }
}

void run_set(Impl& machine, const Goal& goal) {
  const Cell flag = machine.heap.deref(goal.arg(machine, 0));
  const Cell value = machine.heap.deref(goal.arg(machine, 1));
  if (const std::optional<Cell> problem = set_problem(machine, flag, value)) {
    raise_formal(machine, *problem, goal.name, goal.arity);
    return;
  }
  flag_store(machine, static_cast<std::size_t>(find_flag(machine, flag)), value);
  succeed(machine, goal);
}

bool is_set_flag(const Impl& machine, Cell directive, Cell& flag, Cell& value) {
  directive = machine.heap.deref(directive);
  if (!(directive.tag() == Tag::Str && machine.heap.functor_arity(directive) == 1 &&
        machine.atoms.name(machine.heap.functor_atom(directive)) == ":-")) {
    return false;
  }
  const Cell body = machine.heap.deref(machine.heap.arg(directive, 0));
  if (!(body.tag() == Tag::Str && machine.heap.functor_arity(body) == 2 &&
        machine.atoms.name(machine.heap.functor_atom(body)) == "set_prolog_flag")) {
    return false;
  }
  flag = machine.heap.deref(machine.heap.arg(body, 0));
  value = machine.heap.deref(machine.heap.arg(body, 1));
  return true;
}

}  // namespace

DoubleQuotes current_double_quotes(const Impl& machine) { return machine.quotes; }

bool redo_flag_gen(Impl& machine) {
  if (machine.stack.empty() || machine.stack.back().pick != Pick::AtomGen) return false;
  apply_catches(machine, machine.stack.back());
  const Entry top = machine.stack.back();
  const Goal goal = read_goal(machine, top.redo_goal);
  machine.stack.pop_back();
  return prove_flags(machine, goal, top.next);
}

int apply_flag_directive(Impl& machine, Cell directive) {
  Cell flag;
  Cell value;
  if (!is_set_flag(machine, directive, flag, value)) return 0;
  if (const std::optional<Cell> problem = set_problem(machine, flag, value)) {
    machine.diagnostic = write_term(machine.heap, machine.operators, *problem, {}, {});
    return -1;
  }
  flag_store(machine, static_cast<std::size_t>(find_flag(machine, flag)), value);
  return 1;
}

bool run_flags(Impl& machine, const Goal& goal) {
  const auto name = machine.atoms.name(goal.name);
  if (name == "current_prolog_flag" && goal.arity == 2) {
    run_current(machine, goal);
    return true;
  }
  if (name == "set_prolog_flag" && goal.arity == 2) {
    run_set(machine, goal);
    return true;
  }
  return false;
}

}  // namespace mosa
