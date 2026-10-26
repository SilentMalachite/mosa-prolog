// tests.lgt は Logtalk として動かさない。true / false / error だけを、ファイルごとに 1 台の機械で実行する。
// halt はプロセスを終えず、印を戻してから次のテストへ進む。

#include "lgt_suite.hpp"

#include "internal.hpp"

#include "mosa/machine.hpp"
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

// `{ゴール}` は Logtalk の呼び出しなので、クエリする前に中のゴールへ戻す。
Cell unwrap_curlies(Heap& heap, Cell cell) {
  cell = heap.deref(cell);
  if (cell.tag() == Tag::Lis) {
    return heap.list(unwrap_curlies(heap, heap.list_head(cell)), unwrap_curlies(heap, heap.list_tail(cell)));
  }
  if (cell.tag() != Tag::Str) return cell;
  const auto arity = heap.functor_arity(cell);
  std::vector<Cell> args;
  args.reserve(arity);
  for (std::uint16_t index = 0; index < arity; ++index) {
    args.push_back(unwrap_curlies(heap, heap.arg(cell, index)));
  }
  if (arity == 1 && heap.atoms().name(heap.functor_atom(cell)) == "{}") return args[0];
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

void keep_term(Heap& heap, const OperatorTable& operators, std::string_view source, std::size_t begin,
               const ReadTerm& read, LgtFile& file) {
  const Cell term = heap.deref(read.term);
  if (dropped_directive(heap, term)) return;
  Cell head;
  Cell body;
  const bool rule = split_clause(heap, term, head, body);
  const bool outside = line_minus(source, begin);
  const Cell looked = outside ? strip_minus(heap, head) : head;
  if (functor_is(heap, looked, "test", 2)) {
    LgtCase item;
    item.outside = outside;
    item.name = write_term(heap, operators, heap.arg(heap.deref(looked), 0), {}, read.names);
    item.outcome = write_term(heap, operators, heap.arg(heap.deref(looked), 1), {}, read.names);
    const Cell goal = rule ? unwrap_curlies(heap, body) : heap.atom(heap.atoms().intern("true"));
    item.body = write_term(heap, operators, goal, {}, read.names);
    file.cases.push_back(std::move(item));
    return;
  }
  file.program += write_term(heap, operators, term, {}, read.names);
  file.program += ".\n";
}

enum class Kind { True, Assert, False, Error, Other };

struct Want {
  Kind kind = Kind::Other;
  std::string detail;
};

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

Want classify(Machine& machine, std::string_view outcome) {
  Want want;
  const std::string text = std::string{outcome} + ".";
  Reader reader(text, machine.heap(), machine.operators());
  const ReadOutcome read = reader.next();
  if (!read.term) return want;
  const Heap& heap = machine.heap();
  const Cell term = heap.deref(read.term->term);
  if (atom_is(heap, term, "true")) {
    want.kind = Kind::True;
    return want;
  }
  if (atom_is(heap, term, "false") || atom_is(heap, term, "fail")) {
    want.kind = Kind::False;
    return want;
  }
  if (!functor_is(heap, term, "true", 1) && !functor_is(heap, term, "error", 1)) return want;
  want.detail = write_term(heap, machine.operators(), heap.arg(term, 0), {}, read.term->names);
  want.kind = functor_is(heap, term, "true", 1) ? Kind::Assert : Kind::Error;
  return want;
}

std::string goal_text(const LgtCase& item, const Want& want) {
  std::string goal = item.body;
  // 表明が失敗しても、本体の次の解は試さない。
  if (want.kind == Kind::Assert) goal = "(" + item.body + ", !), (" + want.detail + ")";
  goal.push_back('.');
  return goal;
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

bool succeeded(Machine& machine, const Want& want, const Stop& stop) {
  // halt はそのテストの成功にする。期待の形が違ってもプロセスは終わらせない。
  if (stop.kind == StopKind::Halt) return true;
  if (want.kind == Kind::True || want.kind == Kind::Assert) return stop.kind == StopKind::Success;
  if (want.kind == Kind::False) return stop.kind == StopKind::Failure;
  if (want.kind != Kind::Error || stop.kind != StopKind::Exception) return false;
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

CaseEnd run_one(Machine& machine, const LgtCase& item, bool setup, bool cleanup) {
  CaseEnd end;
  try {
    if (!call_hook(machine, setup, "setup.")) {
      end.dead = true;
      end.actual = "aborted";
      return end;
    }
    const Want want = classify(machine, item.outcome);
    if (want.kind == Kind::Other) {
      end.actual = "unsupported";
      end.dead = !call_hook(machine, cleanup, "cleanup.");
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
    Reader reader(source, heap, operators);
    while (true) {
      const auto begin = reader.consumed();
      const ReadOutcome outcome = reader.next();
      if (outcome.failure) {
        file.cases.clear();
        file.program.clear();
        file.error = outcome.failure->reason == SyntaxReason::IncompleteTerm ? "項が閉じていません" : "予期しないトークン";
        return file;
      }
      if (!outcome.term) return file;
      keep_term(heap, operators, source, begin, *outcome.term, file);
    }
  } catch (const ResourceLimit&) {
    file = {};
    file.error = "ヒープの上限を超えました";
    return file;
  }
}

LgtRun execute_lgt(const LgtFile& file) {
  LgtRun run;
  auto machine = Machine::create();
  if (!load_ok(*machine, file.program)) {
    for (const LgtCase& item : file.cases) {
      if (item.outside) run.outside.push_back(item.name);
      else note_failure(run, item, "load");
    }
    return run;
  }
  // 定義の無い setup は存在エラーにしない。テストの assert で後から足した節は、ここには入らない。
  const bool setup = defined_here(*machine, "setup");
  const bool cleanup = defined_here(*machine, "cleanup");
  bool dead = false;
  for (const LgtCase& item : file.cases) {
    if (item.outside) {
      run.outside.push_back(item.name);
      continue;
    }
    if (dead) {
      note_failure(run, item, "aborted");
      continue;
    }
    const CaseEnd end = run_one(*machine, item, setup, cleanup);
    if (end.pass) ++run.passed;
    else note_failure(run, item, end.actual);
    dead = end.dead;
  }
  return run;
}

}  // namespace mosa
