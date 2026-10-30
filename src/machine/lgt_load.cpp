// tests.lgt を読む側。Logtalk として動かさず、object の中の節と test/1,2,3 を Prolog の原文と case に分ける。
// 実行は lgt_file.cpp（execute_lgt）。読みの表（: :: ^^）は実行する機械の表には足さない。

#include "lgt_suite.hpp"

#include "lgt_io.hpp"
#include "internal.hpp"

#include "mosa/machine.hpp"
#include "mosa/heap_option.hpp"
#include "mosa/reader.hpp"
#include "mosa/syntax_error.hpp"
#include "mosa/writer.hpp"

#include <cstdint>
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
  return lgt_functor_is(heap, cell, name, arity);
}

bool atom_is(const Heap& heap, Cell cell, std::string_view name) {
  cell = heap.deref(cell);
  return cell.tag() == Tag::Atom && atom_text(heap, cell) == name;
}

// 読み込み元は既に UTF-8。lgt_io の utf8_name と同じ三つだけ捨て、latin1 などは未知のまま失敗させる。
bool dropped_utf8_encoding(const Heap& heap, Cell inner) {
  if (!functor_is(heap, inner, "encoding", 1)) return false;
  const Cell name = heap.arg(inner, 0);
  return atom_is(heap, name, "utf8") || atom_is(heap, name, "UTF-8") || atom_is(heap, name, "utf-8");
}

bool dropped_directive(const Heap& heap, Cell term) {
  if (!functor_is(heap, term, ":-", 1)) return false;
  const Cell inner = heap.deref(heap.arg(term, 0));
  if (atom_is(heap, inner, "end_object")) return true;
  if (dropped_utf8_encoding(heap, inner)) return true;
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
  // ^ の左は存在量化した変数。右だけがゴールで、ここに {} を残すと {}/1 になる。
  if (name == "^" && arity == 2) return true;
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
  return write_term(heap, operators, unwrap_curlies(heap, goal), lgt_write_options(), names);
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
    file.error = "incomplete_term";
    return;
  }
  LgtGate& gate = file.gates[static_cast<std::size_t>(parse.stack.back().group)];
  // else のあとには枝が続かない。endif が無いのと同じで、ファイルは読めない。
  if (!gate.arms.empty() && gate.arms.back().otherwise) {
    file.error = "incomplete_term";
    return;
  }
  parse.stack.back().arm = static_cast<int>(gate.arms.size());
  gate.arms.push_back(LgtArm{std::move(cond), otherwise});
}

void close_gate(Parse& parse, LgtFile& file) {
  if (parse.stack.empty()) {
    file.error = "incomplete_term";
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
  // 実行する機械の表に : と :: は無い。中置のまま書くと、節も本体も構文エラーになる。
  if (operators.define(0, Specifier::Xfy, ":") != OpResult::Ok ||
      operators.define(0, Specifier::Xfy, "::") != OpResult::Ok) {
    return write_term(heap, operators, term, lgt_write_options(), names);
  }
  // 節の前置 ^^ は load が読めない。正準形にする。本体は判定が演算子を戻す。
  if (clause && operators.define(0, Specifier::Fy, "^^") != OpResult::Ok) {
    return write_term(heap, operators, term, lgt_write_options(), names);
  }
  return write_term(heap, operators, term, lgt_write_options(), names);
}

bool dropped_import_uses(const Heap& heap, Cell term) {
  if (!functor_is(heap, term, ":-", 1)) return false;
  const Cell inner = heap.deref(heap.arg(heap.deref(term), 0));
  if (!functor_is(heap, inner, "uses", 2)) return false;
  // lgtunit は先に吸収する。残りは Logtalk の輸入で、未知の指示として load を落とす。
  return true;
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
  if (dropped_import_uses(heap, term)) return;
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

// error は空かどうかだけを見て、表示しない。文面ではなく鍵の名前を入れる（ADR 0007）。
void wipe_read(LgtFile& file, std::string reason) {
  file.cases.clear();
  file.program.clear();
  file.pieces.clear();
  file.gates.clear();
  file.error = std::move(reason);
}

}  // namespace

Cell lgt_unwrap_curlies(Heap& heap, Cell cell) { return unwrap_curlies(heap, cell); }

LgtFile read_lgt_file(std::string_view source) {
  LgtFile file;
  try {
    AtomTable atoms;
    Heap heap(atoms);
    OperatorTable operators = OperatorTable::iso_core();
    // info の 1:0:0 は、: が演算子でないと項の途中で終わる。実行する機械の表には足さない。
    if (operators.define(600, Specifier::Xfy, ":") != OpResult::Ok) {
      file.error = "op_failed";
      return file;
    }
    // os::目標は図形 :: なので、読みの表だけに足す。実行する機械の表には足さない。
    if (operators.define(600, Specifier::Xfy, "::") != OpResult::Ok) {
      file.error = "op_failed";
      return file;
    }
    // ^^ を実行する機械の表に足すと current_op が変わる。読みのときだけ前置にする。
    if (operators.define(600, Specifier::Fy, "^^") != OpResult::Ok) {
      file.error = "op_failed";
      return file;
    }
    Reader reader(source, heap, operators);
    Parse parse;
    while (true) {
      const auto begin = reader.consumed();
      const ReadOutcome outcome = reader.next();
      if (outcome.failure) {
        const auto reason = outcome.failure->reason == SyntaxReason::IncompleteTerm ? "incomplete_term" : "unexpected_token";
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
    if (!parse.stack.empty()) wipe_read(file, "incomplete_term");
    return file;
  } catch (...) {
    if (!is_heap_exhaustion()) throw;
    file = {};
    file.error = "heap_limit";
    return file;
  }
}

}  // namespace mosa
