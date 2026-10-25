// 規格 8.14.3 op/3、8.14.4 current_op/3、8.14.5 char_conversion/2。
// op(+integer, +operator_specifier, +atom_or_atom_list)
// current_op(?integer, ?operator_specifier, ?atom)
// op/3 は具体化、型、ドメイン、許可の順。名前のリストは全部を確かめてから表を替える。
// 拒否のときは表を変えない。同じ類の指定子は define が置き換える。
// 優先度 0 はその指定子だけを消す。| の優先度 0 も削除として成功する。
// current_op/3 は優先度、指定子、名前の順。優先度 0 は領域に入り、該当が無ければ失敗する。
// 候補は呼び出しのときに写す。やり直しの途中で表が変わっても、写した候補は残る。
// char_conversion(+character, +character)。来が変数、来の型、先が変数、先の型の順。
// 同じ文字は写像を消す。旗が off のあいだ、読みは写像を使わない。
// instantiation_error, type_error(integer), type_error(atom), type_error(list),
// type_error(character),
// domain_error(operator_priority), domain_error(operator_specifier),
// permission_error(modify, operator), permission_error(create, operator)
// テスト: tests/pl/operators.pl, tests/pl/charconv.pl, tests/pl/charconv_dir.pl, tests/pl/japanese.pl

#include "../machine/internal.hpp"

#include "mosa/writer.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mosa {
namespace {

struct OpName {
  Cell cell;
  std::string text;
};

bool is_int(Cell cell) { return cell.tag() == Tag::Int || cell.tag() == Tag::IntBox; }

std::string_view atom_text(const Impl& machine, Cell cell) {
  return machine.atoms.name(static_cast<std::uint32_t>(cell.raw() >> 3));
}

std::optional<Specifier> specifier_of(std::string_view text) {
  if (text == "xf") return Specifier::Xf;
  if (text == "yf") return Specifier::Yf;
  if (text == "xfx") return Specifier::Xfx;
  if (text == "xfy") return Specifier::Xfy;
  if (text == "yfx") return Specifier::Yfx;
  if (text == "fx") return Specifier::Fx;
  if (text == "fy") return Specifier::Fy;
  return std::nullopt;
}

void raise_inst(Impl& machine, const Goal& goal) {
  raise_formal(machine, formal_instantiation(machine), goal.name, goal.arity);
}

void raise_type(Impl& machine, const Goal& goal, std::string_view type, Cell culprit) {
  raise_formal(machine, formal_type(machine, type, culprit), goal.name, goal.arity);
}

void raise_domain(Impl& machine, const Goal& goal, std::string_view domain, Cell culprit) {
  raise_formal(machine, formal_domain(machine, domain, culprit), goal.name, goal.arity);
}

void raise_permit(Impl& machine, const Goal& goal, std::string_view action, Cell culprit) {
  raise_formal(machine, formal_permission(machine, action, "operator", culprit), goal.name, goal.arity);
}

bool name_list_unbound(const Impl& machine, Cell names) {
  if (names.tag() == Tag::Atom) return false;
  const WalkedList walked = walk_list(machine, names);
  if (walked.shape == ListShape::Partial) return true;
  if (walked.shape != ListShape::Proper) return false;
  for (const Cell item : walked.items) {
    if (machine.heap.unbound(item)) return true;
  }
  return false;
}

bool reject_types(Impl& machine, const Goal& goal, Cell priority, Cell specifier, Cell names,
                  WalkedList& walked) {
  if (!is_int(priority)) {
    raise_type(machine, goal, "integer", priority);
    return true;
  }
  if (specifier.tag() != Tag::Atom) {
    raise_type(machine, goal, "atom", specifier);
    return true;
  }
  if (names.tag() == Tag::Atom) return false;
  walked = walk_list(machine, names);
  if (walked.shape != ListShape::Proper) {
    raise_type(machine, goal, "list", names);
    return true;
  }
  for (const Cell item : walked.items) {
    if (item.tag() != Tag::Atom) {
      raise_type(machine, goal, "atom", item);
      return true;
    }
  }
  return false;
}

bool reject_domain(Impl& machine, const Goal& goal, Cell priority, Cell specifier, Specifier& spec) {
  const auto value = machine.heap.integer_value(priority);
  if (value < 0 || value > 1200) {
    raise_domain(machine, goal, "operator_priority", priority);
    return true;
  }
  const std::optional<Specifier> parsed = specifier_of(atom_text(machine, specifier));
  if (!parsed) {
    raise_domain(machine, goal, "operator_specifier", specifier);
    return true;
  }
  spec = *parsed;
  return false;
}

std::vector<OpName> op_names(const Impl& machine, Cell names, const WalkedList& walked) {
  std::vector<OpName> found;
  if (names.tag() == Tag::Atom) {
    found.push_back(OpName{names, std::string(atom_text(machine, names))});
    return found;
  }
  for (const Cell item : walked.items) {
    found.push_back(OpName{item, std::string(atom_text(machine, item))});
  }
  return found;
}

bool commit_ops(Impl& machine, const Goal& goal, int priority, Specifier spec,
                const std::vector<OpName>& names) {
  OperatorTable trial = machine.operators;
  for (const OpName& name : names) {
    const OpResult result = trial.define(priority, spec, name.text);
    if (result == OpResult::Ok) continue;
    raise_permit(machine, goal, result == OpResult::ModifyComma ? "modify" : "create", name.cell);
    return false;
  }
  machine.operators = std::move(trial);
  return true;
}

void run_op(Impl& machine, const Goal& goal) {
  const Cell priority = machine.heap.deref(goal.arg(machine, 0));
  const Cell specifier = machine.heap.deref(goal.arg(machine, 1));
  const Cell names = machine.heap.deref(goal.arg(machine, 2));
  if (machine.heap.unbound(priority) || machine.heap.unbound(specifier) || machine.heap.unbound(names) ||
      name_list_unbound(machine, names)) {
    raise_inst(machine, goal);
    return;
  }
  WalkedList walked;
  if (reject_types(machine, goal, priority, specifier, names, walked)) return;
  Specifier spec = Specifier::Xfx;
  if (reject_domain(machine, goal, priority, specifier, spec)) return;
  const auto priority_value = machine.heap.integer_value(priority);
  if (!commit_ops(machine, goal, static_cast<int>(priority_value), spec, op_names(machine, names, walked))) {
    return;
  }
  machine.ip = goal.cont;
}

constexpr std::uint8_t kOpGen = 4;

std::string_view specifier_name(Specifier specifier) {
  switch (specifier) {
    case Specifier::Xf: return "xf";
    case Specifier::Yf: return "yf";
    case Specifier::Xfx: return "xfx";
    case Specifier::Xfy: return "xfy";
    case Specifier::Yfx: return "yfx";
    case Specifier::Fx: return "fx";
    case Specifier::Fy: return "fy";
  }
  return "xfx";
}

bool priority_in_domain(const Impl& machine, Cell cell) {
  if (!is_int(cell)) return false;
  const auto value = machine.heap.integer_value(cell);
  return value >= 0 && value <= 1200;
}

bool specifier_atom(const Impl& machine, Cell cell) {
  return cell.tag() == Tag::Atom && specifier_of(atom_text(machine, cell)).has_value();
}

bool keep_op(const Impl& machine, Cell priority, Cell specifier, Cell name, const OpDef& op) {
  if (!machine.heap.unbound(priority) && machine.heap.integer_value(priority) != op.priority) return false;
  if (!machine.heap.unbound(specifier) && atom_text(machine, specifier) != specifier_name(op.specifier)) {
    return false;
  }
  return machine.heap.unbound(name) || atom_text(machine, name) == op.name;
}

std::vector<Cell> matching_ops(Impl& machine, Cell priority, Cell specifier, Cell name) {
  const std::vector<OpDef> ops = name.tag() == Tag::Atom ? machine.operators.lookup(atom_text(machine, name))
                                                          : machine.operators.entries();
  std::vector<Cell> flat;
  for (const OpDef& op : ops) {
    if (!keep_op(machine, priority, specifier, name, op)) continue;
    flat.push_back(machine.heap.integer(op.priority));
    flat.push_back(machine.heap.atom(machine.atoms.intern(specifier_name(op.specifier))));
    flat.push_back(machine.heap.atom(machine.atoms.intern(op.name)));
  }
  return flat;
}

bool bind_op(Impl& machine, const Goal& goal, const std::vector<Cell>& flat, std::size_t index) {
  const auto at = index * 3;
  return do_unify(machine, goal.arg(machine, 0), flat[at]) &&
         do_unify(machine, goal.arg(machine, 1), flat[at + 1]) &&
         do_unify(machine, goal.arg(machine, 2), flat[at + 2]);
}

void push_op(Impl& machine, const Goal& goal, const std::vector<Cell>& flat, std::size_t next,
             std::size_t heap_mark, std::size_t trail_mark) {
  Entry entry;
  entry.pick = Pick::AtomGen;
  entry.gen_kind = kOpGen;
  entry.next = next;
  entry.env = flat;
  entry.redo_goal = goal.at;
  entry.heap_mark = heap_mark;
  entry.trail_mark = trail_mark;
  push_frame(machine, std::move(entry));
}

bool prove_ops(Impl& machine, const Goal& goal, const std::vector<Cell>& flat, std::size_t begin) {
  const auto count = flat.size() / 3;
  for (std::size_t index = begin; index < count; ++index) {
    const auto heap_mark = machine.heap.size();
    const auto trail_mark = machine.trail.slots.size();
    const auto extras_mark = machine.extras.size();
    const bool more = index + 1 < count;
    if (more) push_op(machine, goal, flat, index + 1, heap_mark, trail_mark);
    if (bind_op(machine, goal, flat, index)) {
      machine.ip = goal.cont;
      return true;
    }
    if (more) machine.stack.pop_back();
    restore_state(machine, heap_mark, trail_mark, extras_mark);
  }
  return false;
}

void run_current_op(Impl& machine, const Goal& goal) {
  const Cell priority = machine.heap.deref(goal.arg(machine, 0));
  const Cell specifier = machine.heap.deref(goal.arg(machine, 1));
  const Cell name = machine.heap.deref(goal.arg(machine, 2));
  if (!machine.heap.unbound(priority) && !priority_in_domain(machine, priority)) {
    raise_domain(machine, goal, "operator_priority", priority);
    return;
  }
  if (!machine.heap.unbound(specifier) && !specifier_atom(machine, specifier)) {
    raise_domain(machine, goal, "operator_specifier", specifier);
    return;
  }
  if (!machine.heap.unbound(name) && name.tag() != Tag::Atom) {
    raise_type(machine, goal, "atom", name);
    return;
  }
  if (!prove_ops(machine, goal, matching_ops(machine, priority, specifier, name), 0)) machine.failed = true;
}

std::optional<Cell> char_conv_problem(Impl& machine, Cell in, Cell out, char32_t& from, char32_t& to) {
  in = machine.heap.deref(in);
  out = machine.heap.deref(out);
  if (machine.heap.unbound(in)) return formal_instantiation(machine);
  if (!one_char_atom(machine, in, from)) return formal_type(machine, "character", in);
  if (machine.heap.unbound(out)) return formal_instantiation(machine);
  if (!one_char_atom(machine, out, to)) return formal_type(machine, "character", out);
  return std::nullopt;
}

void store_char_conv(Impl& machine, char32_t from, char32_t to) {
  auto& map = machine.char_conv.map;
  std::erase_if(map, [from](const std::pair<char32_t, char32_t>& pair) { return pair.first == from; });
  if (from != to) map.emplace_back(from, to);
}

void run_char_conversion(Impl& machine, const Goal& goal) {
  char32_t from = 0;
  char32_t to = 0;
  if (const std::optional<Cell> problem =
          char_conv_problem(machine, goal.arg(machine, 0), goal.arg(machine, 1), from, to)) {
    raise_formal(machine, *problem, goal.name, goal.arity);
    return;
  }
  store_char_conv(machine, from, to);
  machine.ip = goal.cont;
}

}  // namespace

bool redo_current_op(Impl& machine) {
  if (machine.stack.empty() || machine.stack.back().pick != Pick::AtomGen ||
      machine.stack.back().gen_kind != kOpGen) {
    return false;
  }
  apply_catches(machine, machine.stack.back());
  const Entry top = machine.stack.back();
  const Goal goal = read_goal(machine, top.redo_goal);
  const std::vector<Cell> flat = top.env;
  const auto next = top.next;
  machine.stack.pop_back();
  return prove_ops(machine, goal, flat, next);
}

int apply_char_conv_directive(Impl& machine, Cell directive) {
  directive = machine.heap.deref(directive);
  if (!(directive.tag() == Tag::Str && machine.heap.functor_arity(directive) == 1 &&
        machine.atoms.name(machine.heap.functor_atom(directive)) == ":-")) {
    return 0;
  }
  const Cell body = machine.heap.deref(machine.heap.arg(directive, 0));
  if (!(body.tag() == Tag::Str && machine.heap.functor_arity(body) == 2 &&
        machine.atoms.name(machine.heap.functor_atom(body)) == "char_conversion")) {
    return 0;
  }
  char32_t from = 0;
  char32_t to = 0;
  if (const std::optional<Cell> problem =
          char_conv_problem(machine, machine.heap.arg(body, 0), machine.heap.arg(body, 1), from, to)) {
    machine.diagnostic = write_term(machine.heap, machine.operators, *problem, {}, {});
    machine.syntax_diag = false;
    return -1;
  }
  store_char_conv(machine, from, to);
  return 1;
}

bool run_operators(Impl& machine, const Goal& goal) {
  const auto name = machine.atoms.name(goal.name);
  if (name == "op" && goal.arity == 3) {
    run_op(machine, goal);
    return true;
  }
  if (name == "current_op" && goal.arity == 3) {
    run_current_op(machine, goal);
    return true;
  }
  if (name == "char_conversion" && goal.arity == 2) {
    run_char_conversion(machine, goal);
    return true;
  }
  return false;
}

}  // namespace mosa
