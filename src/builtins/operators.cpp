// 規格 8.14.3 op/3、8.14.4 current_op/3、8.14.5 char_conversion/2、8.14.6 current_char_conversion/2。
// op(+integer, +operator_specifier, +atom_or_atom_list)
// current_op(?integer, ?operator_specifier, ?atom)
// op/3 は具体化、型、ドメイン、許可の順。名前のリストは全部を確かめてから表を替える。
// 拒否のときは表を変えない。同じ類の指定子は define が置き換える。
// 優先度 0 はその指定子だけを消す。| の優先度 0 も削除として成功する。
// current_op/3 は優先度、指定子、名前の順。優先度 0 は領域に入り、該当が無ければ失敗する。
// 候補は呼び出しのときに写す。やり直しの途中で表が変わっても、写した候補は残る。
// char_conversion(+character, +character)。来が変数、来の型、先が変数、先の型の順。
// 同じ文字は写像を消す。旗が off のあいだ、読みは写像を使わない。
// current_char_conversion(?character, ?character)。来の型、先の型の順。変数は許す。
// 恒等でない組だけを、登録順に返す。旗の on と off は問い合わせに影響しない。
// instantiation_error, type_error(integer), type_error(atom), type_error(list),
// type_error(character),
// domain_error(operator_priority), domain_error(operator_specifier),
// permission_error(modify, operator), permission_error(create, operator)
// 選択点より後ろの変更は失敗で戻す。assert と retract は戻さない。
// テスト: tests/pl/operators.pl, tests/pl/charconv.pl, tests/pl/charconv_dir.pl,
// tests/pl/curchar.pl, tests/pl/japanese.pl

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

void raise_type(Impl& machine, const Goal& goal, std::string_view type, Cell culprit) {
  raise_formal(machine, formal_type(machine, type, culprit), goal.name, goal.arity);
}

void raise_domain(Impl& machine, const Goal& goal, std::string_view domain, Cell culprit) {
  raise_formal(machine, formal_domain(machine, domain, culprit), goal.name, goal.arity);
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

std::optional<Cell> op_types(Impl& machine, Cell priority, Cell specifier, Cell names, WalkedList& walked) {
  if (!is_int(priority)) return formal_type(machine, "integer", priority);
  if (specifier.tag() != Tag::Atom) return formal_type(machine, "atom", specifier);
  if (names.tag() == Tag::Atom) return std::nullopt;
  walked = walk_list(machine, names);
  if (walked.shape != ListShape::Proper) return formal_type(machine, "list", names);
  for (const Cell item : walked.items) {
    if (item.tag() != Tag::Atom) return formal_type(machine, "atom", item);
  }
  return std::nullopt;
}

std::optional<Cell> op_domain(Impl& machine, Cell priority, Cell specifier, Specifier& spec) {
  const auto value = machine.heap.integer_value(priority);
  if (value < 0 || value > 1200) return formal_domain(machine, "operator_priority", priority);
  const std::optional<Specifier> parsed = specifier_of(atom_text(machine, specifier));
  if (!parsed) return formal_domain(machine, "operator_specifier", specifier);
  spec = *parsed;
  return std::nullopt;
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

// 局所の層では優先度 0 も印として残す。拒否は user の表と重ねた見え方で決める。
OpResult define_in(const Impl& machine, OperatorTable& trial, bool local, const OpRequest& request,
                   const std::string& name) {
  if (!local) return trial.define(request.priority, request.specifier, name);
  const OpResult result = OperatorView{machine.operators, &trial}.admits(request.priority, request.specifier, name);
  if (result == OpResult::Ok) trial.define_local(request.priority, request.specifier, name);
  return result;
}

void run_op(Impl& machine, const Goal& goal) {
  OpRequest request;
  auto formal = read_op_request(machine, goal.arg(machine, 0), goal.arg(machine, 1), goal.arg(machine, 2), request);
  // 文脈モジュールの層へ入れる。user なら全体の表で、どのモジュールからも見える。
  if (!formal) formal = commit_op_request(machine, goal.module, request);
  if (formal) {
    raise_formal(machine, *formal, goal.name, goal.arity);
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

// 文脈モジュールの見え方で返す。固有層の印で隠した演算子は出さない。
std::vector<Cell> matching_ops(Impl& machine, std::uint32_t module, Cell priority, Cell specifier, Cell name) {
  const OperatorView view = op_view(machine, module);
  const std::vector<OpDef> ops = name.tag() == Tag::Atom ? view.lookup(atom_text(machine, name)) : view.entries();
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
  entry.env = make_env(flat);
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
  if (!prove_ops(machine, goal, matching_ops(machine, goal.module, priority, specifier, name), 0)) machine.failed = true;
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

constexpr std::uint8_t kCharGen = 5;

bool character_arg(const Impl& machine, Cell cell) {
  char32_t code = 0;
  return machine.heap.unbound(cell) || one_char_atom(machine, cell, code);
}

Cell char_atom(Impl& machine, char32_t code) { return machine.heap.atom(machine.atoms.intern_char(code)); }

std::vector<Cell> matching_chars(Impl& machine, Cell in, Cell out) {
  char32_t in_code = 0;
  char32_t out_code = 0;
  const bool in_bound = one_char_atom(machine, in, in_code);
  const bool out_bound = one_char_atom(machine, out, out_code);
  std::vector<Cell> flat;
  for (const auto& pair : machine.char_conv.map) {
    if (in_bound && pair.first != in_code) continue;
    if (out_bound && pair.second != out_code) continue;
    flat.push_back(char_atom(machine, pair.first));
    flat.push_back(char_atom(machine, pair.second));
  }
  return flat;
}

bool bind_char(Impl& machine, const Goal& goal, const std::vector<Cell>& flat, std::size_t index) {
  const auto at = index * 2;
  return do_unify(machine, goal.arg(machine, 0), flat[at]) &&
         do_unify(machine, goal.arg(machine, 1), flat[at + 1]);
}

void push_char(Impl& machine, const Goal& goal, const std::vector<Cell>& flat, std::size_t next,
               std::size_t heap_mark, std::size_t trail_mark) {
  Entry entry;
  entry.pick = Pick::AtomGen;
  entry.gen_kind = kCharGen;
  entry.next = next;
  entry.env = make_env(flat);
  entry.redo_goal = goal.at;
  entry.heap_mark = heap_mark;
  entry.trail_mark = trail_mark;
  push_frame(machine, std::move(entry));
}

bool prove_chars(Impl& machine, const Goal& goal, const std::vector<Cell>& flat, std::size_t begin) {
  const auto count = flat.size() / 2;
  for (std::size_t index = begin; index < count; ++index) {
    const auto heap_mark = machine.heap.size();
    const auto trail_mark = machine.trail.slots.size();
    const auto extras_mark = machine.extras.size();
    const bool more = index + 1 < count;
    if (more) push_char(machine, goal, flat, index + 1, heap_mark, trail_mark);
    if (bind_char(machine, goal, flat, index)) {
      machine.ip = goal.cont;
      return true;
    }
    if (more) machine.stack.pop_back();
    restore_state(machine, heap_mark, trail_mark, extras_mark);
  }
  return false;
}

void run_current_char_conversion(Impl& machine, const Goal& goal) {
  const Cell in = machine.heap.deref(goal.arg(machine, 0));
  const Cell out = machine.heap.deref(goal.arg(machine, 1));
  if (!character_arg(machine, in)) {
    raise_type(machine, goal, "character", in);
    return;
  }
  if (!character_arg(machine, out)) {
    raise_type(machine, goal, "character", out);
    return;
  }
  if (!prove_chars(machine, goal, matching_chars(machine, in, out), 0)) machine.failed = true;
}

}  // namespace

std::optional<Cell> read_op_request(Impl& machine, Cell priority, Cell specifier, Cell names, OpRequest& request) {
  priority = machine.heap.deref(priority);
  specifier = machine.heap.deref(specifier);
  names = machine.heap.deref(names);
  if (machine.heap.unbound(priority) || machine.heap.unbound(specifier) || machine.heap.unbound(names) ||
      name_list_unbound(machine, names)) {
    return formal_instantiation(machine);
  }
  WalkedList walked;
  if (const auto formal = op_types(machine, priority, specifier, names, walked)) return formal;
  Specifier spec = Specifier::Xfx;
  if (const auto formal = op_domain(machine, priority, specifier, spec)) return formal;
  request.priority = static_cast<int>(machine.heap.integer_value(priority));
  request.specifier = spec;
  request.names = op_names(machine, names, walked);
  return std::nullopt;
}

std::optional<Cell> commit_op_request(Impl& machine, std::uint32_t module, const OpRequest& request) {
  Module* local = module == machine.user_module ? nullptr : find_module(machine, module);
  OperatorTable trial = local == nullptr ? machine.operators : local->ops;
  for (const OpName& name : request.names) {
    const OpResult result = define_in(machine, trial, local != nullptr, request, name.text);
    if (result == OpResult::Ok) continue;
    return formal_permission(machine, result == OpResult::ModifyComma ? "modify" : "create", "operator", name.cell);
  }
  // 検査が通ってから表を替える。戻すのはこの呼び出しの名前だけ。
  std::vector<std::string> touched;
  touched.reserve(request.names.size());
  for (const OpName& name : request.names) touched.push_back(name.text);
  trail_operators(machine, local == nullptr ? machine.user_module : module, request.priority, request.specifier,
                  touched);
  if (local == nullptr) machine.operators = std::move(trial);
  else local->ops = std::move(trial);
  return std::nullopt;
}

bool redo_current_op(Impl& machine) {
  if (machine.stack.empty() || machine.stack.back().pick != Pick::AtomGen ||
      machine.stack.back().gen_kind != kOpGen) {
    return false;
  }
  apply_catches(machine, machine.stack.back());
  const Entry top = machine.stack.back();
  const Goal goal = read_goal(machine, top.redo_goal);
  const std::vector<Cell> flat = top.env->cells;
  const auto next = top.next;
  machine.stack.pop_back();
  return prove_ops(machine, goal, flat, next);
}

bool redo_current_char_conversion(Impl& machine) {
  if (machine.stack.empty() || machine.stack.back().pick != Pick::AtomGen ||
      machine.stack.back().gen_kind != kCharGen) {
    return false;
  }
  apply_catches(machine, machine.stack.back());
  const Entry top = machine.stack.back();
  const Goal goal = read_goal(machine, top.redo_goal);
  const std::vector<Cell> flat = top.env->cells;
  const auto next = top.next;
  machine.stack.pop_back();
  return prove_chars(machine, goal, flat, next);
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
  if (name == "current_char_conversion" && goal.arity == 2) {
    run_current_char_conversion(machine, goal);
    return true;
  }
  return false;
}

}  // namespace mosa
