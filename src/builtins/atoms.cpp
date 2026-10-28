// 8.16 アトム処理。atom_number/2（未決 2 の決定 A）は number_chars と同じく atoms_number.cpp に置く。
// atom_length(+atom, ?integer)
//   instantiation_error, type_error(atom), type_error(integer), domain_error(not_less_than_zero)
// atom_concat(?atom, ?atom, ?atom)
//   instantiation_error, type_error(atom)
// sub_atom(+atom, ?integer, ?integer, ?integer, ?atom)
//   instantiation_error, type_error(atom), type_error(integer), domain_error(not_less_than_zero)
// atom_chars(?atom, ?character_list) / atom_codes(?atom, ?character_code_list)
//   instantiation_error, type_error(atom), type_error(list), type_error(character),
//   type_error(integer), representation_error(character_code)
// char_code(?character, ?character_code)
//   instantiation_error, type_error(character), type_error(integer), representation_error(character_code)
// テスト: tests/pl/atoms.pl, tests/pl/japanese.pl

#include "../machine/internal.hpp"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mosa {
namespace {

constexpr std::uint8_t kConcat = 1;
constexpr std::uint8_t kSub = 2;

struct Step {
  char32_t code = 0;
  std::size_t bytes = 0;
  bool ok = false;
};

bool is_int_cell(Cell cell) { return cell.tag() == Tag::Int || cell.tag() == Tag::IntBox; }

std::uint32_t atom_id(Cell cell) { return static_cast<std::uint32_t>(cell.raw() >> 3); }

int continuation(std::string_view text, std::size_t index) {
  if (index >= text.size()) return -1;
  const auto byte = static_cast<unsigned char>(text[index]);
  if ((byte & 0xC0U) != 0x80U) return -1;
  return byte & 0x3F;
}

Step step_at(std::string_view text, std::size_t index) {
  if (index >= text.size()) return {};
  const auto lead = static_cast<unsigned char>(text[index]);
  if (lead <= 0x7FU) return Step{lead, 1, true};
  const int first = continuation(text, index + 1);
  if (first < 0) return {};
  if (lead >= 0xC2U && lead <= 0xDFU) {
    return Step{static_cast<char32_t>(((lead & 0x1FU) << 6) | first), 2, true};
  }
  const int second = continuation(text, index + 2);
  if (second < 0) return {};
  if (lead >= 0xE0U && lead <= 0xEFU) {
    if (lead == 0xE0U && first < 0x20) return {};
    if (lead == 0xEDU && first >= 0x20) return {};
    const auto code = static_cast<char32_t>(((lead & 0x0FU) << 12) | (first << 6) | second);
    return Step{code, 3, true};
  }
  const int third = continuation(text, index + 3);
  if (third < 0 || lead < 0xF0U || lead > 0xF4U) return {};
  if (lead == 0xF0U && first < 0x10) return {};
  if (lead == 0xF4U && first >= 0x10) return {};
  const auto code = static_cast<char32_t>(((lead & 0x07U) << 18) | (first << 12) | (second << 6) | third);
  if (code > 0x10FFFFU) return {};
  return Step{code, 4, true};
}

void instantiation(Impl& machine, const Goal& goal) {
  raise_formal(machine, formal_instantiation(machine), goal.name, goal.arity);
}

void type_at(Impl& machine, const Goal& goal, std::string_view type, Cell culprit) {
  raise_formal(machine, formal_type(machine, type, culprit), goal.name, goal.arity);
}

void domain_at(Impl& machine, const Goal& goal, Cell culprit) {
  raise_formal(machine, formal_domain(machine, "not_less_than_zero", culprit), goal.name, goal.arity);
}

void succeed(Impl& machine, const Goal& goal) { machine.ip = goal.cont; }

struct Slice {
  std::size_t before = 0;
  std::size_t length = 0;
  std::size_t after = 0;
};

Slice slice_at(std::size_t len, std::size_t index) {
  std::size_t cursor = 0;
  Slice slice;
  for (slice.before = 0; slice.before <= len; ++slice.before) {
    const auto room = len - slice.before;
    for (slice.length = 0; slice.length <= room; ++slice.length) {
      if (cursor == index) {
        slice.after = room - slice.length;
        return slice;
      }
      ++cursor;
    }
  }
  return {};
}

std::size_t slice_count(std::size_t len) { return (len + 1) * (len + 2) / 2; }

void push_gen(Impl& machine, const Goal& goal, std::uint8_t kind, std::size_t next, std::size_t heap_mark,
              std::size_t trail_mark) {
  Entry entry;
  entry.pick = Pick::AtomGen;
  entry.gen_kind = kind;
  entry.next = next;
  entry.redo_goal = goal.at;
  entry.heap_mark = heap_mark;
  entry.trail_mark = trail_mark;
  push_frame(machine, std::move(entry));
}

bool bind_concat(Impl& machine, const Goal& goal, std::size_t split) {
  const Cell whole_cell = machine.heap.deref(goal.arg(machine, 2));
  const CodeText whole = decode_codes(machine.atoms.name(atom_id(whole_cell)));
  if (split > whole.size()) return false;
  const Cell left = machine.heap.atom(machine.atoms.intern(whole.slice(0, split)));
  const Cell right = machine.heap.atom(machine.atoms.intern(whole.slice(split, whole.size() - split)));
  return do_unify(machine, goal.arg(machine, 0), left) && do_unify(machine, goal.arg(machine, 1), right);
}

bool bind_sub(Impl& machine, const Goal& goal, std::size_t index) {
  const Cell atom = machine.heap.deref(goal.arg(machine, 0));
  const CodeText text = decode_codes(machine.atoms.name(atom_id(atom)));
  const Slice slice = slice_at(text.size(), index);
  const Cell before = machine.heap.integer(static_cast<std::int64_t>(slice.before));
  const Cell length = machine.heap.integer(static_cast<std::int64_t>(slice.length));
  const Cell after = machine.heap.integer(static_cast<std::int64_t>(slice.after));
  const Cell sub = machine.heap.atom(machine.atoms.intern(text.slice(slice.before, slice.length)));
  return do_unify(machine, goal.arg(machine, 1), before) && do_unify(machine, goal.arg(machine, 2), length) &&
         do_unify(machine, goal.arg(machine, 3), after) && do_unify(machine, goal.arg(machine, 4), sub);
}

bool bind_gen(Impl& machine, const Goal& goal, std::uint8_t kind, std::size_t index) {
  if (kind == kConcat) return bind_concat(machine, goal, index);
  return bind_sub(machine, goal, index);
}

std::size_t gen_total(const Impl& machine, const Goal& goal, std::uint8_t kind) {
  const Cell atom = machine.heap.deref(goal.arg(machine, kind == kConcat ? 2 : 0));
  const auto len = decode_codes(machine.atoms.name(atom_id(atom))).size();
  if (kind == kConcat) return len + 1;
  return slice_count(len);
}

bool prove_gen(Impl& machine, const Goal& goal, std::uint8_t kind, std::size_t begin, std::size_t total) {
  for (std::size_t index = begin; index < total; ++index) {
    const auto heap_mark = machine.heap.size();
    const auto trail_mark = machine.trail.slots.size();
    const auto extras_mark = machine.extras.size();
    const bool more = index + 1 < total;
    // 水位は束縛より前。後だとやり直しが、今の解の束縛を消さない。
    if (more) push_gen(machine, goal, kind, index + 1, heap_mark, trail_mark);
    if (bind_gen(machine, goal, kind, index)) {
      succeed(machine, goal);
      return true;
    }
    if (more) machine.stack.pop_back();
    restore_state(machine, heap_mark, trail_mark, extras_mark);
  }
  return false;
}

void gen_or_fail(Impl& machine, const Goal& goal, std::uint8_t kind) {
  if (!prove_gen(machine, goal, kind, 0, gen_total(machine, goal, kind))) machine.failed = true;
}

bool negative_int(const Impl& machine, Cell cell) {
  return is_int_cell(cell) && machine.heap.integer_value(cell) < 0;
}

bool has_var_item(const Impl& machine, const WalkedList& walked) {
  for (const Cell item : walked.items) {
    if (machine.heap.unbound(item)) return true;
  }
  return false;
}

bool reject_codes(Impl& machine, const Goal& goal, const WalkedList& walked) {
  for (const Cell item : walked.items) {
    if (machine.heap.unbound(item)) continue;
    if (!is_int_cell(item)) {
      type_at(machine, goal, "integer", item);
      return true;
    }
    if (!scalar_code(machine.heap.integer_value(item))) {
      raise_formal(machine, formal_representation(machine, "character_code"), goal.name, goal.arity);
      return true;
    }
  }
  return false;
}

bool reject_chars(Impl& machine, const Goal& goal, const WalkedList& walked) {
  for (const Cell item : walked.items) {
    char32_t code = 0;
    if (machine.heap.unbound(item)) continue;
    if (!one_char_atom(machine, item, code)) {
      type_at(machine, goal, "character", item);
      return true;
    }
  }
  return false;
}

bool reject_atom_list(Impl& machine, const Goal& goal, Cell list, bool atom_var, bool codes, WalkedList& walked) {
  if (machine.heap.unbound(list)) return false;
  walked = walk_list(machine, list);
  if (walked.shape == ListShape::Bad || walked.shape == ListShape::Variable) {
    type_at(machine, goal, "list", list);
    return true;
  }
  if (atom_var && (walked.shape == ListShape::Partial || has_var_item(machine, walked))) {
    instantiation(machine, goal);
    return true;
  }
  return codes ? reject_codes(machine, goal, walked) : reject_chars(machine, goal, walked);
}

void bind_char_list(Impl& machine, const Goal& goal, Cell atom, bool codes) {
  const CodeText text = decode_codes(machine.atoms.name(atom_id(atom)));
  std::vector<Cell> items;
  items.reserve(text.size());
  for (std::size_t index = 0; index < text.size(); ++index) {
    if (codes) items.push_back(machine.heap.integer(static_cast<std::int64_t>(text.codes[index])));
    else items.push_back(machine.heap.atom(machine.atoms.intern_char(text.codes[index])));
  }
  if (!do_unify(machine, goal.arg(machine, 1), list_from_cells(machine, items))) machine.failed = true;
  else succeed(machine, goal);
}

void bind_from_list(Impl& machine, const Goal& goal, const WalkedList& walked, bool codes) {
  std::string bytes;
  for (const Cell item : walked.items) {
    if (codes) append_utf8_code(bytes, static_cast<char32_t>(machine.heap.integer_value(item)));
    else bytes.append(machine.atoms.name(atom_id(item)));
  }
  const Cell made = machine.heap.atom(machine.atoms.intern(bytes));
  if (!do_unify(machine, goal.arg(machine, 0), made)) machine.failed = true;
  else succeed(machine, goal);
}

void run_atom_length(Impl& machine, const Goal& goal) {
  const Cell atom = machine.heap.deref(goal.arg(machine, 0));
  const Cell length = machine.heap.deref(goal.arg(machine, 1));
  if (machine.heap.unbound(atom)) {
    instantiation(machine, goal);
    return;
  }
  if (atom.tag() != Tag::Atom) {
    type_at(machine, goal, "atom", atom);
    return;
  }
  if (!machine.heap.unbound(length) && !is_int_cell(length)) {
    type_at(machine, goal, "integer", length);
    return;
  }
  if (negative_int(machine, length)) {
    domain_at(machine, goal, length);
    return;
  }
  const auto count = static_cast<std::int64_t>(decode_codes(machine.atoms.name(atom_id(atom))).size());
  if (!do_unify(machine, goal.arg(machine, 1), machine.heap.integer(count))) machine.failed = true;
  else succeed(machine, goal);
}

void run_atom_concat(Impl& machine, const Goal& goal) {
  const Cell left = machine.heap.deref(goal.arg(machine, 0));
  const Cell right = machine.heap.deref(goal.arg(machine, 1));
  const Cell whole = machine.heap.deref(goal.arg(machine, 2));
  const bool left_var = machine.heap.unbound(left);
  const bool right_var = machine.heap.unbound(right);
  const bool whole_var = machine.heap.unbound(whole);
  if ((left_var && whole_var) || (right_var && whole_var)) {
    instantiation(machine, goal);
    return;
  }
  if (!left_var && left.tag() != Tag::Atom) {
    type_at(machine, goal, "atom", left);
    return;
  }
  if (!right_var && right.tag() != Tag::Atom) {
    type_at(machine, goal, "atom", right);
    return;
  }
  if (!whole_var && whole.tag() != Tag::Atom) {
    type_at(machine, goal, "atom", whole);
    return;
  }
  if (whole_var) {
    const CodeText prefix = decode_codes(machine.atoms.name(atom_id(left)));
    const CodeText suffix = decode_codes(machine.atoms.name(atom_id(right)));
    std::string joined = prefix.bytes;
    joined.append(suffix.bytes);
    const Cell made = machine.heap.atom(machine.atoms.intern(joined));
    if (!do_unify(machine, goal.arg(machine, 2), made)) machine.failed = true;
    else succeed(machine, goal);
    return;
  }
  gen_or_fail(machine, goal, kConcat);
}

bool reject_sub_type(Impl& machine, const Goal& goal, Cell cell) {
  if (machine.heap.unbound(cell) || is_int_cell(cell)) return false;
  type_at(machine, goal, "integer", cell);
  return true;
}

bool reject_sub_domain(Impl& machine, const Goal& goal, Cell cell) {
  if (!negative_int(machine, cell)) return false;
  domain_at(machine, goal, cell);
  return true;
}

void run_sub_atom(Impl& machine, const Goal& goal) {
  const Cell atom = machine.heap.deref(goal.arg(machine, 0));
  const Cell sub = machine.heap.deref(goal.arg(machine, 4));
  if (machine.heap.unbound(atom)) {
    instantiation(machine, goal);
    return;
  }
  if (atom.tag() != Tag::Atom) {
    type_at(machine, goal, "atom", atom);
    return;
  }
  // 8.16.3.3 は整数の型を、部分アトムの型より先に見る。
  const Cell before = machine.heap.deref(goal.arg(machine, 1));
  const Cell length = machine.heap.deref(goal.arg(machine, 2));
  const Cell after = machine.heap.deref(goal.arg(machine, 3));
  if (reject_sub_type(machine, goal, before)) return;
  if (reject_sub_type(machine, goal, length)) return;
  if (reject_sub_type(machine, goal, after)) return;
  if (!machine.heap.unbound(sub) && sub.tag() != Tag::Atom) {
    type_at(machine, goal, "atom", sub);
    return;
  }
  if (reject_sub_domain(machine, goal, before)) return;
  if (reject_sub_domain(machine, goal, length)) return;
  if (reject_sub_domain(machine, goal, after)) return;
  gen_or_fail(machine, goal, kSub);
}

void run_atom_list(Impl& machine, const Goal& goal, bool codes) {
  const Cell atom = machine.heap.deref(goal.arg(machine, 0));
  const Cell list = machine.heap.deref(goal.arg(machine, 1));
  const bool atom_var = machine.heap.unbound(atom);
  if (!atom_var && atom.tag() != Tag::Atom) {
    type_at(machine, goal, "atom", atom);
    return;
  }
  if (atom_var && machine.heap.unbound(list)) {
    instantiation(machine, goal);
    return;
  }
  WalkedList walked;
  if (reject_atom_list(machine, goal, list, atom_var, codes, walked)) return;
  if (atom_var) bind_from_list(machine, goal, walked, codes);
  else bind_char_list(machine, goal, atom, codes);
}

void run_char_code(Impl& machine, const Goal& goal) {
  const Cell character = machine.heap.deref(goal.arg(machine, 0));
  const Cell code = machine.heap.deref(goal.arg(machine, 1));
  const bool character_var = machine.heap.unbound(character);
  const bool code_var = machine.heap.unbound(code);
  char32_t scalar = 0;
  if (character_var && code_var) {
    instantiation(machine, goal);
    return;
  }
  if (!character_var && !one_char_atom(machine, character, scalar)) {
    type_at(machine, goal, "character", character);
    return;
  }
  if (!code_var && !is_int_cell(code)) {
    type_at(machine, goal, "integer", code);
    return;
  }
  if (!code_var && !scalar_code(machine.heap.integer_value(code))) {
    raise_formal(machine, formal_representation(machine, "character_code"), goal.name, goal.arity);
    return;
  }
  Cell left = character;
  Cell right = code;
  if (character_var) {
    const auto value = static_cast<char32_t>(machine.heap.integer_value(code));
    left = machine.heap.atom(machine.atoms.intern_char(value));
  }
  if (code_var) right = machine.heap.integer(static_cast<std::int64_t>(scalar));
  if (!do_unify(machine, goal.arg(machine, 0), left) || !do_unify(machine, goal.arg(machine, 1), right)) {
    machine.failed = true;
    return;
  }
  succeed(machine, goal);
}

}  // namespace

void run_number_atoms(Impl& machine, const Goal& goal);
void run_atom_number(Impl& machine, const Goal& goal);

CodeText decode_codes(std::string_view text) {
  CodeText out;
  out.bytes.assign(text.begin(), text.end());
  out.cuts.push_back(0);
  std::size_t index = 0;
  while (index < text.size()) {
    const Step step = step_at(text, index);
    if (!step.ok) {
      out.codes.push_back(static_cast<unsigned char>(text[index]));
      out.cuts.push_back(++index);
      continue;
    }
    out.codes.push_back(step.code);
    index += step.bytes;
    out.cuts.push_back(index);
  }
  return out;
}

void append_utf8_code(std::string& text, char32_t code) {
  const auto push = [&text](unsigned int byte) {
    text.push_back(static_cast<char>(static_cast<unsigned char>(byte)));
  };
  if (code <= 0x7FU) {
    push(static_cast<unsigned int>(code));
    return;
  }
  if (code <= 0x7FFU) {
    push(0xC0U | static_cast<unsigned int>(code >> 6));
    push(0x80U | (static_cast<unsigned int>(code) & 0x3FU));
    return;
  }
  if (code <= 0xFFFFU) {
    push(0xE0U | static_cast<unsigned int>(code >> 12));
    push(0x80U | ((static_cast<unsigned int>(code) >> 6) & 0x3FU));
    push(0x80U | (static_cast<unsigned int>(code) & 0x3FU));
    return;
  }
  push(0xF0U | static_cast<unsigned int>(code >> 18));
  push(0x80U | ((static_cast<unsigned int>(code) >> 12) & 0x3FU));
  push(0x80U | ((static_cast<unsigned int>(code) >> 6) & 0x3FU));
  push(0x80U | (static_cast<unsigned int>(code) & 0x3FU));
}

WalkedList walk_list(const Impl& machine, Cell list) {
  WalkedList walked;
  Cell cursor = machine.heap.deref(list);
  if (machine.heap.unbound(cursor)) {
    walked.shape = ListShape::Variable;
    return walked;
  }
  // Brent の方法で巡回を見る。辿った節を全部覚えると、長いリストの sort/2 や length/2 が二乗になる。
  std::uint64_t mark = 0;
  bool marked = false;
  std::size_t power = 1;
  std::size_t steps = 1;
  while (true) {
    cursor = machine.heap.deref(cursor);
    if (machine.heap.unbound(cursor)) {
      walked.shape = ListShape::Partial;
      walked.tail = cursor;
      return walked;
    }
    if (cursor.tag() == Tag::Atom && machine.atoms.name(atom_id(cursor)) == "[]") {
      walked.shape = ListShape::Proper;
      return walked;
    }
    if (cursor.tag() != Tag::Lis || (marked && cursor.raw() == mark)) {
      walked.shape = ListShape::Bad;
      return walked;
    }
    if (steps == power) {
      mark = cursor.raw();
      marked = true;
      power *= 2;
      steps = 0;
    }
    ++steps;
    walked.items.push_back(machine.heap.deref(machine.heap.list_head(cursor)));
    cursor = machine.heap.list_tail(cursor);
  }
}

Cell list_from_cells(Impl& machine, const std::vector<Cell>& items) {
  Cell tail = machine.heap.atom(machine.atoms.intern("[]"));
  for (auto it = items.rbegin(); it != items.rend(); ++it) tail = machine.heap.list(*it, tail);
  return tail;
}

bool one_char_atom(const Impl& machine, Cell cell, char32_t& code) {
  if (cell.tag() != Tag::Atom) return false;
  const CodeText text = decode_codes(machine.atoms.name(atom_id(cell)));
  if (text.size() != 1) return false;
  code = text.codes[0];
  return true;
}

bool scalar_code(std::int64_t code) {
  return code >= 0 && code <= 0x10FFFF && (code < 0xD800 || code > 0xDFFF);
}

bool redo_atoms(Impl& machine) {
  if (machine.stack.empty() || machine.stack.back().pick != Pick::AtomGen) return false;
  if (machine.stack.back().gen_kind == 3) return redo_flag_gen(machine);
  if (machine.stack.back().gen_kind == 4) return redo_current_op(machine);
  if (machine.stack.back().gen_kind == 5) return redo_current_char_conversion(machine);
  if (machine.stack.back().gen_kind == 6) return redo_length_gen(machine);
  apply_catches(machine, machine.stack.back());
  const Entry top = machine.stack.back();
  const Goal goal = read_goal(machine, top.redo_goal);
  machine.stack.pop_back();
  return prove_gen(machine, goal, top.gen_kind, top.next, gen_total(machine, goal, top.gen_kind));
}

bool run_atoms(Impl& machine, const Goal& goal) {
  const auto name = machine.atoms.name(goal.name);
  if (name == "atom_length" && goal.arity == 2) {
    run_atom_length(machine, goal);
    return true;
  }
  if (name == "atom_concat" && goal.arity == 3) {
    run_atom_concat(machine, goal);
    return true;
  }
  if (name == "sub_atom" && goal.arity == 5) {
    run_sub_atom(machine, goal);
    return true;
  }
  if (name == "atom_chars" && goal.arity == 2) {
    run_atom_list(machine, goal, false);
    return true;
  }
  if (name == "atom_codes" && goal.arity == 2) {
    run_atom_list(machine, goal, true);
    return true;
  }
  if (name == "char_code" && goal.arity == 2) {
    run_char_code(machine, goal);
    return true;
  }
  if ((name == "number_chars" || name == "number_codes") && goal.arity == 2) {
    run_number_atoms(machine, goal);
    return true;
  }
  if (name == "atom_number" && goal.arity == 2) {
    run_atom_number(machine, goal);
    return true;
  }
  return false;
}

}  // namespace mosa
