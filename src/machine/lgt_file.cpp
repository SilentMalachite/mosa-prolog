// tests.lgt は Logtalk として動かさない。ファイルごとに 1 台の機械で実行する。
// halt はプロセスを終えず、印を戻してから次のテストへ進む。

#include "lgt_suite.hpp"

#include "lgt_io.hpp"
#include "internal.hpp"

#include "mosa/machine.hpp"
#include "mosa/reader.hpp"
#include "mosa/syntax_error.hpp"
#include "mosa/writer.hpp"

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mosa {
namespace {

std::string_view atom_text(const Heap& heap, Cell cell) {
  return heap.atoms().name(static_cast<std::uint32_t>(cell.raw() >> 3));
}

bool functor_is(const Heap& heap, Cell cell, std::string_view name, std::uint16_t arity) {
  cell = heap.deref(cell);
  return cell.tag() == Tag::Str && heap.functor_arity(cell) == arity &&
         heap.atoms().name(heap.functor_atom(cell)) == name;
}

bool atom_is(const Heap& heap, Cell cell, std::string_view name) {
  cell = heap.deref(cell);
  return cell.tag() == Tag::Atom && atom_text(heap, cell) == name;
}

bool dropped_directive(const Heap& heap, Cell term) {
  if (!functor_is(heap, term, ":-", 1)) return false;
  const Cell inner = heap.deref(heap.arg(term, 0));
  if (atom_is(heap, inner, "end_object")) return true;
  if (inner.tag() != Tag::Str) return false;
  const auto name = heap.atoms().name(heap.functor_atom(inner));
  return name == "object" || name == "end_object" || name == "info" || name == "set_logtalk_flag";
}

// 連言とメタ呼び出しのゴール引数だけが呼び出し位置。それ以外はデータなので {} を残す。
bool goal_argument(std::string_view name, std::uint16_t arity, std::uint16_t index) {
  if (arity == 2 && (name == "," || name == ";" || name == "->" || name == "*->" || name == "forall")) return true;
  if (arity == 1 && (name == "\\+" || name == "once" || name == "assertion")) return true;
  if (name == "call" && arity >= 1) return index == 0;
  if (name == "catch" && arity == 3) return index == 0 || index == 2;
  // 第1引数は説明。第2引数が、ランナーが実行するゴール。
  if (name == "assertion" && arity == 2) return index == 1;
  if (index != 1) return false;
  if (name == "findall" && (arity == 3 || arity == 4)) return true;
  return arity == 3 && (name == "bagof" || name == "setof");
}

// 呼び出し位置の {} だけを外す。arg/3 の {1,2,3} まで外すと項が (1,2,3) になる。
Cell unwrap_curlies(Heap& heap, Cell cell) {
  cell = heap.deref(cell);
  if (functor_is(heap, cell, "{}", 1)) return unwrap_curlies(heap, heap.arg(cell, 0));
  if (cell.tag() != Tag::Str) return cell;
  const auto name = heap.atoms().name(heap.functor_atom(cell));
  const auto arity = heap.functor_arity(cell);
  bool any = false;
  for (std::uint16_t index = 0; index < arity; ++index) {
    if (goal_argument(name, arity, index)) any = true;
  }
  if (!any) return cell;
  std::vector<Cell> args;
  args.reserve(arity);
  for (std::uint16_t index = 0; index < arity; ++index) {
    const Cell arg = heap.arg(cell, index);
    args.push_back(goal_argument(name, arity, index) ? unwrap_curlies(heap, arg) : arg);
  }
  return heap.structure(heap.functor_atom(cell), args);
}

bool layout_byte(char ch) {
  return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\v' || ch == '\f';
}

std::size_t skip_gap(std::string_view source, std::size_t index) {
  while (index < source.size()) {
    if (layout_byte(source[index])) {
      ++index;
      continue;
    }
    if (source[index] == '%') {
      while (index < source.size() && source[index] != '\n') ++index;
      continue;
    }
    if (source[index] == '/' && index + 1 < source.size() && source[index + 1] == '*') {
      index += 2;
      while (index + 1 < source.size() && !(source[index] == '*' && source[index + 1] == '/')) ++index;
      index = index + 1 < source.size() ? index + 2 : source.size();
      continue;
    }
    break;
  }
  return index;
}

// 前置の - は test の外側ではない。行頭の - だけを別欄にする。
bool line_minus(std::string_view source, std::size_t begin) {
  const auto at = skip_gap(source, begin);
  std::size_t line = at;
  while (line > 0 && source[line - 1] != '\n') --line;
  while (line < source.size() && (source[line] == ' ' || source[line] == '\t' || source[line] == '\r')) ++line;
  return line < source.size() && source[line] == '-';
}

bool split_clause(const Heap& heap, Cell term, Cell& head, Cell& body) {
  if (!functor_is(heap, term, ":-", 2)) {
    head = heap.deref(term);
    return false;
  }
  term = heap.deref(term);
  head = heap.arg(term, 0);
  body = heap.arg(term, 1);
  return true;
}

Cell strip_minus(const Heap& heap, Cell head) {
  if (!functor_is(heap, head, "-", 1)) return heap.deref(head);
  return heap.deref(heap.arg(heap.deref(head), 0));
}

// 旗を節に残すと存在エラーになる。失敗に替えてから載せると、condition は別欄に入る。
Cell silence_flag(Heap& heap, Cell cell, bool& found) {
  cell = heap.deref(cell);
  if (cell.tag() == Tag::Lis) {
    bool head_found = false;
    bool tail_found = false;
    const Cell head = silence_flag(heap, heap.list_head(cell), head_found);
    const Cell tail = silence_flag(heap, heap.list_tail(cell), tail_found);
    found = found || head_found || tail_found;
    if (!head_found && !tail_found) return cell;
    return heap.list(head, tail);
  }
  if (cell.tag() != Tag::Str) return cell;
  if (functor_is(heap, cell, "current_logtalk_flag", 2)) {
    found = true;
    return heap.atom(heap.atoms().intern("fail"));
  }
  bool any = false;
  std::vector<Cell> args;
  args.reserve(heap.functor_arity(cell));
  for (std::uint16_t index = 0; index < heap.functor_arity(cell); ++index) {
    bool child = false;
    args.push_back(silence_flag(heap, heap.arg(cell, index), child));
    any = any || child;
  }
  if (!any) return cell;
  found = true;
  return heap.structure(heap.functor_atom(cell), args);
}

Cell silence_flag(Heap& heap, Cell cell) {
  bool found = false;
  return silence_flag(heap, cell, found);
}

enum class GateMark { None, If, Elif, Else, Endif };

struct Parse {
  std::vector<LgtGuard> stack;
};

GateMark gate_mark(const Heap& heap, Cell term, Cell& goal) {
  if (!functor_is(heap, term, ":-", 1)) return GateMark::None;
  const Cell inner = heap.deref(heap.arg(term, 0));
  if (atom_is(heap, inner, "else")) return GateMark::Else;
  if (atom_is(heap, inner, "endif")) return GateMark::Endif;
  if (functor_is(heap, inner, "if", 1)) {
    goal = heap.arg(inner, 0);
    return GateMark::If;
  }
  if (functor_is(heap, inner, "elif", 1)) {
    goal = heap.arg(inner, 0);
    return GateMark::Elif;
  }
  return GateMark::None;
}

std::string written_goal(Heap& heap, const OperatorTable& operators, Cell goal, const std::vector<VarName>& names) {
  return write_term(heap, operators, unwrap_curlies(heap, goal), {}, names);
}

void open_gate(Parse& parse, LgtFile& file, std::string cond) {
  LgtGate gate;
  gate.parent = parse.stack;
  gate.arms.push_back(LgtArm{std::move(cond), false});
  const int group = static_cast<int>(file.gates.size());
  file.gates.push_back(std::move(gate));
  parse.stack.push_back(LgtGuard{group, 0});
}

void next_arm(Parse& parse, LgtFile& file, std::string cond, bool otherwise) {
  if (parse.stack.empty()) {
    file.error = "項が閉じていません";
    return;
  }
  LgtGate& gate = file.gates[static_cast<std::size_t>(parse.stack.back().group)];
  // else のあとには枝が続かない。endif が無いのと同じで、ファイルは読めない。
  if (!gate.arms.empty() && gate.arms.back().otherwise) {
    file.error = "項が閉じていません";
    return;
  }
  parse.stack.back().arm = static_cast<int>(gate.arms.size());
  gate.arms.push_back(LgtArm{std::move(cond), otherwise});
}

void close_gate(Parse& parse, LgtFile& file) {
  if (parse.stack.empty()) {
    file.error = "項が閉じていません";
    return;
  }
  parse.stack.pop_back();
}

bool take_gate(Heap& heap, const OperatorTable& operators, Cell term, const std::vector<VarName>& names, Parse& parse,
               LgtFile& file) {
  Cell goal;
  const GateMark mark = gate_mark(heap, term, goal);
  if (mark == GateMark::None) return false;
  if (mark == GateMark::Endif) {
    close_gate(parse, file);
    return true;
  }
  if (mark == GateMark::If) {
    open_gate(parse, file, written_goal(heap, operators, goal, names));
    return true;
  }
  const bool otherwise = mark == GateMark::Else;
  const std::string cond = otherwise ? std::string{} : written_goal(heap, operators, goal, names);
  next_arm(parse, file, cond, otherwise);
  return true;
}

std::string condition_of(Heap& heap, const OperatorTable& operators, Cell list, const std::vector<VarName>& names) {
  std::string text;
  list = heap.deref(list);
  while (list.tag() == Tag::Lis) {
    const Cell item = heap.deref(heap.list_head(list));
    if (functor_is(heap, item, "condition", 1)) {
      if (!text.empty()) text += ", ";
      // 連言より弱い演算子が、隣の condition と一続きにならないように括る。
      text += "(" + written_goal(heap, operators, heap.arg(item, 0), names) + ")";
    }
    list = heap.deref(heap.list_tail(list));
  }
  return text;
}

bool test_arity(const Heap& heap, Cell head, int& arity) {
  if (functor_is(heap, head, "test", 2)) {
    arity = 2;
    return true;
  }
  if (functor_is(heap, head, "test", 3)) {
    arity = 3;
    return true;
  }
  return false;
}

// user::目標は利用者述語。モジュール修飾を残すと、機械は :: を知らないので読めない。
Cell strip_user_scope(Heap& heap, Cell cell) {
  cell = heap.deref(cell);
  if (cell.tag() == Tag::Lis) {
    return heap.list(strip_user_scope(heap, heap.list_head(cell)), strip_user_scope(heap, heap.list_tail(cell)));
  }
  if (cell.tag() != Tag::Str) return cell;
  const auto arity = heap.functor_arity(cell);
  std::vector<Cell> args;
  args.reserve(arity);
  for (std::uint16_t index = 0; index < arity; ++index) {
    args.push_back(strip_user_scope(heap, heap.arg(cell, index)));
  }
  if (arity == 2 && heap.atoms().name(heap.functor_atom(cell)) == "::" && atom_is(heap, args[0], "user")) {
    return args[1];
  }
  return heap.structure(heap.functor_atom(cell), args);
}

std::string machine_text(Heap& heap, OperatorTable operators, Cell term, const std::vector<VarName>& names,
                         bool clause) {
  // 実行する機械の表に :: は無い。中置のまま書くと、節も本体も構文エラーになる。
  if (operators.define(0, Specifier::Xfy, "::") != OpResult::Ok) {
    return write_term(heap, operators, term, {}, names);
  }
  // 節の前置 ^^ は load が読めない。正準形にする。本体は判定が演算子を戻す。
  if (clause && operators.define(0, Specifier::Fy, "^^") != OpResult::Ok) {
    return write_term(heap, operators, term, {}, names);
  }
  return write_term(heap, operators, term, {}, names);
}

bool dropped_user_uses(const Heap& heap, Cell term) {
  if (!functor_is(heap, term, ":-", 1)) return false;
  const Cell inner = heap.deref(heap.arg(heap.deref(term), 0));
  if (!functor_is(heap, inner, "uses", 2)) return false;
  // 印だけ。機械に渡すと未知の指示で load が落ち、ファイルのテストが全部止まる。
  return atom_is(heap, heap.arg(inner, 0), "user");
}

void add_piece(Heap& heap, const OperatorTable& operators, Cell term, const std::vector<VarName>& names,
               const Parse& parse, LgtFile& file) {
  const Cell prepared = strip_user_scope(heap, silence_flag(heap, term));
  std::string text = machine_text(heap, operators, prepared, names, true);
  text += ".\n";
  if (parse.stack.empty()) file.program += text;
  file.pieces.push_back(LgtPiece{std::move(text), parse.stack});
}

void add_case(Heap& heap, const OperatorTable& operators, Cell looked, bool rule, Cell body, bool outside, int arity,
              const std::vector<VarName>& names, const Parse& parse, LgtFile& file) {
  LgtCase item;
  item.outside = outside;
  item.guard = parse.stack;
  const Cell head = heap.deref(looked);
  item.name = machine_text(heap, operators, strip_user_scope(heap, heap.arg(head, 0)), names, false);
  item.outcome = machine_text(heap, operators, strip_user_scope(heap, heap.arg(head, 1)), names, false);
  if (arity == 3) item.condition = condition_of(heap, operators, heap.arg(head, 2), names);
  const Cell goal = rule ? unwrap_curlies(heap, body) : heap.atom(heap.atoms().intern("true"));
  item.body = machine_text(heap, operators, strip_user_scope(heap, goal), names, false);
  file.cases.push_back(std::move(item));
}

void keep_term(Heap& heap, OperatorTable& operators, std::string_view source, std::size_t begin, const ReadTerm& read,
               Parse& parse, LgtFile& file) {
  const Cell term = heap.deref(read.term);
  if (absorb_lgt_uses(heap, operators, term, file.operators)) return;
  if (dropped_user_uses(heap, term)) return;
  if (dropped_directive(heap, term)) return;
  if (take_gate(heap, operators, term, read.names, parse, file)) return;
  Cell head;
  Cell body;
  const bool rule = split_clause(heap, term, head, body);
  const bool outside = line_minus(source, begin);
  const Cell looked = outside ? strip_minus(heap, head) : head;
  int arity = 0;
  if (test_arity(heap, looked, arity)) {
    add_case(heap, operators, looked, rule, body, outside, arity, read.names, parse, file);
    return;
  }
  add_piece(heap, operators, term, read.names, parse, file);
}

void wipe_read(LgtFile& file, std::string reason) {
  file.cases.clear();
  file.program.clear();
  file.pieces.clear();
  file.gates.clear();
  file.error = std::move(reason);
}

bool integer_cell(const Heap& heap, Cell cell) {
  cell = heap.deref(cell);
  return cell.tag() == Tag::Int || cell.tag() == Tag::IntBox;
}

// 一度合わせた項の再出現は、この形と変数セルで見る。名前の無い same_term は別の自由変数を同じにしてしまう。
bool same_shape(const Heap& heap, Cell left, Cell right) {
  left = heap.deref(left);
  right = heap.deref(right);
  if (heap.unbound(left) || heap.unbound(right)) {
    return heap.unbound(left) && heap.unbound(right) && left.raw() == right.raw();
  }
  if (integer_cell(heap, left) && integer_cell(heap, right)) {
    return heap.integer_value(left) == heap.integer_value(right);
  }
  if (left.tag() != right.tag()) return false;
  if (left.tag() == Tag::Atom) return atom_text(heap, left) == atom_text(heap, right);
  if (left.tag() == Tag::Float) return heap.float_value(left) == heap.float_value(right);
  if (left.tag() == Tag::Lis) {
    return same_shape(heap, heap.list_head(left), heap.list_head(right)) &&
           same_shape(heap, heap.list_tail(left), heap.list_tail(right));
  }
  if (left.tag() != Tag::Str || heap.functor_arity(left) != heap.functor_arity(right)) return false;
  if (heap.atoms().name(heap.functor_atom(left)) != heap.atoms().name(heap.functor_atom(right))) return false;
  const auto arity = heap.functor_arity(left);
  for (std::uint16_t index = 0; index < arity; ++index) {
    if (!same_shape(heap, heap.arg(left, index), heap.arg(right, index))) return false;
  }
  return true;
}

struct NumberValue {
  bool integer = false;
  std::int64_t as_int = 0;
  double as_float = 0.0;
};

bool fill_number(const Heap& heap, Cell cell, bool negative, NumberValue& out) {
  cell = heap.deref(cell);
  if (integer_cell(heap, cell)) {
    auto value = heap.integer_value(cell);
    if (negative) {
      if (value == std::numeric_limits<std::int64_t>::min()) return false;
      value = -value;
    }
    out.integer = true;
    out.as_int = value;
    return true;
  }
  if (cell.tag() != Tag::Float) return false;
  out.integer = false;
  out.as_float = negative ? -heap.float_value(cell) : heap.float_value(cell);
  return true;
}

// 数そのものか、その前の + と - だけ。式は評価しない。
std::optional<NumberValue> signed_number(const Heap& heap, Cell cell) {
  cell = heap.deref(cell);
  NumberValue out;
  if (fill_number(heap, cell, false, out)) return out;
  if (cell.tag() != Tag::Str || heap.functor_arity(cell) != 1) return std::nullopt;
  const auto name = heap.atoms().name(heap.functor_atom(cell));
  if (name != "+" && name != "-") return std::nullopt;
  if (!fill_number(heap, heap.arg(cell, 0), name == "-", out)) return std::nullopt;
  return out;
}

bool sign_of_number(const Heap& heap, Cell cell) {
  cell = heap.deref(cell);
  if (cell.tag() != Tag::Str || heap.functor_arity(cell) != 1) return false;
  const auto name = heap.atoms().name(heap.functor_atom(cell));
  if (name != "+" && name != "-") return false;
  const Cell arg = heap.deref(heap.arg(cell, 0));
  return integer_cell(heap, arg) || arg.tag() == Tag::Float;
}

bool same_number(const NumberValue& left, const NumberValue& right) {
  if (left.integer != right.integer) return false;
  if (left.integer) return left.as_int == right.as_int;
  return left.as_float == right.as_float;
}

bool subsumes(const Heap& heap, Cell general, Cell specific, std::vector<std::pair<std::uint64_t, Cell>>& bound) {
  general = heap.deref(general);
  specific = heap.deref(specific);
  if (heap.unbound(general)) {
    for (const auto& [id, value] : bound) {
      if (id == general.raw()) return same_shape(heap, value, specific);
    }
    bound.emplace_back(general.raw(), specific);
    return true;
  }
  if (heap.unbound(specific)) return false;
  if (integer_cell(heap, general) && integer_cell(heap, specific)) {
    return heap.integer_value(general) == heap.integer_value(specific);
  }
  // 期待の -1 は前置の複合項。投げた整数とタグが違うので、数の符号だけ揃える。
  if (sign_of_number(heap, general) || sign_of_number(heap, specific)) {
    const auto left = signed_number(heap, general);
    const auto right = signed_number(heap, specific);
    if (left && right) return same_number(*left, *right);
  }
  if (general.tag() != specific.tag()) return false;
  if (general.tag() == Tag::Atom) return atom_text(heap, general) == atom_text(heap, specific);
  if (general.tag() == Tag::Float) return heap.float_value(general) == heap.float_value(specific);
  if (general.tag() == Tag::Lis) {
    return subsumes(heap, heap.list_head(general), heap.list_head(specific), bound) &&
           subsumes(heap, heap.list_tail(general), heap.list_tail(specific), bound);
  }
  if (general.tag() != Tag::Str || heap.functor_arity(general) != heap.functor_arity(specific)) return false;
  if (heap.atoms().name(heap.functor_atom(general)) != heap.atoms().name(heap.functor_atom(specific))) return false;
  const auto arity = heap.functor_arity(general);
  for (std::uint16_t index = 0; index < arity; ++index) {
    if (!subsumes(heap, heap.arg(general, index), heap.arg(specific, index), bound)) return false;
  }
  return true;
}

bool thrown_matches(Machine& machine, Cell ball, std::string_view formal) {
  const Heap& heap = machine.heap();
  ball = heap.deref(ball);
  if (!functor_is(heap, ball, "error", 2)) return false;
  const std::string text = std::string{formal} + ".";
  Reader reader(text, machine.heap(), machine.operators());
  const ReadOutcome read = reader.next();
  if (!read.term) return false;
  std::vector<std::pair<std::uint64_t, Cell>> bound;
  return subsumes(machine.heap(), read.term->term, heap.arg(ball, 0), bound);
}

std::string goal_text(const LgtCase& item, const LgtWant& want) {
  std::string goal = item.body;
  // 表明が失敗しても、本体の次の解は試さない。
  if (want.kind == LgtKind::Assert) goal = "(" + item.body + ", !), (" + want.detail + ")";
  goal.push_back('.');
  return goal;
}

bool routes_to_judge(Machine& machine, std::string_view body, const LgtWant& want) {
  if (want.kind != LgtKind::True && want.kind != LgtKind::Assert && want.kind != LgtKind::False &&
      want.kind != LgtKind::Error) {
    return true;
  }
  if (lgt_text_has_special(machine, body)) return true;
  return want.kind == LgtKind::Assert && lgt_text_has_special(machine, want.detail);
}

bool defined_here(const Machine& machine, std::string_view name) {
  const Impl& impl = engine_impl(machine);
  for (const Predicate& predicate : impl.predicates) {
    if (predicate.arity == 0 && !predicate.clauses.empty() && impl.atoms.name(predicate.name) == name) return true;
  }
  return false;
}

struct Query {
  bool dead = false;
  Stop stop;
};

Query run_goal(Machine& machine, std::string_view goal) {
  Query query;
  try {
    query.stop = machine.query(goal);
  } catch (const ResourceLimit&) {
    query.dead = true;
    return query;
  }
  // 次のクエリが停止の印で空振りしないように、結果を取ったあとで戻す。
  Impl& impl = engine_impl(machine);
  impl.halt_now = false;
  impl.halt_pending = false;
  return query;
}

std::string actual_text(const Machine& machine, const Stop& stop) {
  if (stop.kind == StopKind::Success || stop.kind == StopKind::Halt) return "true";
  if (stop.kind != StopKind::Exception) return "fail";
  const Heap& heap = machine.heap();
  const Cell term = heap.deref(stop.exception);
  if (functor_is(heap, term, "error", 2)) {
    return write_term(heap, machine.operators(), heap.arg(term, 0), {}, {});
  }
  return write_term(heap, machine.operators(), term, {}, {});
}

bool succeeded(Machine& machine, const LgtWant& want, const Stop& stop) {
  // halt はそのテストの成功にする。期待の形が違ってもプロセスは終わらせない。
  if (stop.kind == StopKind::Halt) return true;
  if (want.kind == LgtKind::True || want.kind == LgtKind::Assert) return stop.kind == StopKind::Success;
  if (want.kind == LgtKind::False) return stop.kind == StopKind::Failure;
  if (want.kind != LgtKind::Error || stop.kind != StopKind::Exception) return false;
  return thrown_matches(machine, stop.exception, want.detail);
}

struct CaseEnd {
  bool pass = false;
  bool dead = false;
  std::string actual;
};

bool call_hook(Machine& machine, bool enabled, std::string_view goal) {
  if (!enabled) return true;
  return !run_goal(machine, goal).dead;
}

CaseEnd run_one(Machine& machine, const LgtCase& item, bool setup, bool cleanup, const LgtIo* io) {
  CaseEnd end;
  try {
    if (!call_hook(machine, setup, "setup.")) {
      end.dead = true;
      end.actual = "aborted";
      return end;
    }
    const LgtWant want = classify_lgt_outcome(machine, item.outcome);
    if (want.kind == LgtKind::Other) {
      end.actual = "unsupported";
      end.dead = !call_hook(machine, cleanup, "cleanup.");
      return end;
    }
    if (routes_to_judge(machine, item.body, want)) {
      const LgtEnd judged = judge_lgt_case(machine, item.body, want, io);
      end.pass = judged.pass;
      end.dead = judged.dead;
      end.actual = judged.actual;
      if (!call_hook(machine, cleanup, "cleanup.")) end.dead = true;
      return end;
    }
    const Query query = run_goal(machine, goal_text(item, want));
    if (query.dead) {
      end.dead = true;
      end.actual = "aborted";
      return end;
    }
    end.pass = succeeded(machine, want, query.stop);
    if (!end.pass) end.actual = actual_text(machine, query.stop);
    if (!call_hook(machine, cleanup, "cleanup.")) end.dead = true;
    return end;
  } catch (const ResourceLimit&) {
    end.dead = true;
    end.actual = "aborted";
    return end;
  }
}

bool load_ok(Machine& machine, std::string_view program) {
  if (program.empty()) return true;
  try {
    return machine.load_program(program).ok;
  } catch (const ResourceLimit&) {
    return false;
  }
}

void note_failure(LgtRun& run, const LgtCase& item, std::string_view actual) {
  run.failures.push_back(item.name + " " + item.outcome + " " + std::string{actual});
  ++run.failed;
}

}  // namespace

bool lgt_same_shape(const Heap& heap, Cell left, Cell right) { return same_shape(heap, left, right); }

bool lgt_subsumes(const Heap& heap, Cell general, Cell specific) {
  std::vector<std::pair<std::uint64_t, Cell>> bound;
  return subsumes(heap, general, specific, bound);
}

LgtFile read_lgt_file(std::string_view source) {
  LgtFile file;
  try {
    AtomTable atoms;
    Heap heap(atoms);
    OperatorTable operators = OperatorTable::iso_core();
    // info の 1:0:0 は、: が演算子でないと項の途中で終わる。実行する機械の表には足さない。
    if (operators.define(600, Specifier::Xfy, ":") != OpResult::Ok) {
      file.error = "演算子を足せません";
      return file;
    }
    // os::目標は図形 :: なので、読みの表だけに足す。実行する機械の表には足さない。
    if (operators.define(600, Specifier::Xfy, "::") != OpResult::Ok) {
      file.error = "演算子を足せません";
      return file;
    }
    // ^^ を実行する機械の表に足すと current_op が変わる。読みのときだけ前置にする。
    if (operators.define(600, Specifier::Fy, "^^") != OpResult::Ok) {
      file.error = "演算子を足せません";
      return file;
    }
    Reader reader(source, heap, operators);
    Parse parse;
    while (true) {
      const auto begin = reader.consumed();
      const ReadOutcome outcome = reader.next();
      if (outcome.failure) {
        const auto reason = outcome.failure->reason == SyntaxReason::IncompleteTerm ? "項が閉じていません" : "予期しないトークン";
        wipe_read(file, reason);
        return file;
      }
      if (!outcome.term) break;
      keep_term(heap, operators, source, begin, *outcome.term, parse, file);
      if (!file.error.empty()) {
        wipe_read(file, file.error);
        return file;
      }
    }
    // endif が無いと、選ばなかった枝の終わりが無い。ファイル全体を不合格 1 件にする。
    if (!parse.stack.empty()) wipe_read(file, "項が閉じていません");
    return file;
  } catch (const ResourceLimit&) {
    file = {};
    file.error = "ヒープの上限を超えました";
    return file;
  }
}

namespace {

bool guard_holds(const std::vector<LgtGuard>& guard, const std::vector<int>& chosen) {
  for (const LgtGuard& item : guard) {
    if (item.group < 0 || static_cast<std::size_t>(item.group) >= chosen.size()) return false;
    if (chosen[static_cast<std::size_t>(item.group)] != item.arm) return false;
  }
  return true;
}

std::string selected_text(const LgtFile& file, const std::vector<int>& chosen) {
  std::string text;
  for (const LgtPiece& piece : file.pieces) {
    if (guard_holds(piece.guard, chosen)) text += piece.text;
  }
  return text;
}

bool choose_gates(Machine& machine, const LgtFile& file, std::vector<int>& chosen) {
  chosen.assign(file.gates.size(), -1);
  for (std::size_t index = 0; index < file.gates.size(); ++index) {
    const LgtGate& gate = file.gates[index];
    // 選ばれていない枝の中の条件は実行しない。
    if (!guard_holds(gate.parent, chosen)) continue;
    for (std::size_t arm = 0; arm < gate.arms.size(); ++arm) {
      const LgtArm& item = gate.arms[arm];
      if (item.otherwise) {
        chosen[index] = static_cast<int>(arm);
        break;
      }
      const LgtCond cond = lgt_condition(machine, item.cond);
      if (cond == LgtCond::Dead) return false;
      if (cond != LgtCond::Yes) continue;
      chosen[index] = static_cast<int>(arm);
      break;
    }
  }
  return true;
}

void mark_load(LgtRun& run, const LgtCase& item, bool held) {
  if (!held || item.outside) run.outside.push_back(item.name);
  else note_failure(run, item, "load");
}

void apply_condition(Machine& machine, const LgtCase& item, bool& dead, bool& skip, LgtRun& run) {
  if (item.condition.empty()) return;
  const LgtCond cond = lgt_condition(machine, item.condition);
  if (cond == LgtCond::Dead) {
    note_failure(run, item, "aborted");
    dead = true;
    skip = true;
    return;
  }
  if (cond == LgtCond::Bad) {
    note_failure(run, item, "unsupported");
    skip = true;
    return;
  }
  if (cond != LgtCond::Yes) {
    run.outside.push_back(item.name);
    skip = true;
  }
}

void run_cases(Machine& machine, const LgtFile& file, const std::vector<int>& chosen, bool setup, bool cleanup,
               const LgtIo* io, LgtRun& run) {
  bool dead = false;
  for (const LgtCase& item : file.cases) {
    if (!guard_holds(item.guard, chosen) || item.outside) {
      run.outside.push_back(item.name);
      continue;
    }
    if (dead) {
      note_failure(run, item, "aborted");
      continue;
    }
    bool skip = false;
    apply_condition(machine, item, dead, skip, run);
    if (skip) continue;
    const CaseEnd end = run_one(machine, item, setup, cleanup, io);
    if (end.pass) ++run.passed;
    else note_failure(run, item, end.actual);
    dead = end.dead;
  }
}

}  // namespace

LgtRun execute_lgt(const LgtFile& file, std::string_view tests_path, std::string_view relative_dir) {
  struct Clean {
    LgtIo io;
    ~Clean() { lgt_io_cleanup(io); }
  } clean{lgt_io_context(tests_path, relative_dir)};
  LgtRun run;
  auto machine = Machine::create();
  std::vector<int> chosen;
  if (!choose_gates(*machine, file, chosen)) {
    for (const LgtCase& item : file.cases) note_failure(run, item, "aborted");
    return run;
  }
  if (!load_ok(*machine, selected_text(file, chosen))) {
    for (const LgtCase& item : file.cases) mark_load(run, item, guard_holds(item.guard, chosen));
    return run;
  }
  // =~= はこのファイルの機械だけ。読み込みの節には入れない。
  install_lgt_ops(*machine, file.operators);
  // テストより先に隣の Prolog を載せる。tester.lgt 自体は実行しない。
  lgt_load_tester(*machine, clean.io);
  // 定義の無い setup は存在エラーにしない。テストの assert で後から足した節は、ここには入らない。
  const bool setup = defined_here(*machine, "setup");
  const bool cleanup = defined_here(*machine, "cleanup");
  run_cases(*machine, file, chosen, setup, cleanup, &clean.io, run);
  return run;
}

}  // namespace mosa
