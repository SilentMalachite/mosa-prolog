// 結果の形は機械の next で解を辿る。二度目の query はやり直しではない。
// variant や assertion や =~= や forall は述語にしない。登録すると current_predicate が成功する。

#include "lgt_suite.hpp"
#include "lgt_priv.hpp"

#include "internal.hpp"

#include "mosa/bigint.hpp"
#include "mosa/machine.hpp"
#include "mosa/heap_option.hpp"
#include "mosa/reader.hpp"
#include "mosa/writer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <compare>
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

bool integer_cell(const Heap& heap, Cell cell) {
  cell = heap.deref(cell);
  return is_integer_tag(cell.tag());
}

struct Parsed {
  bool ok = false;
  Cell term{};
  std::vector<VarName> names;
};

// 本体の ^^ は利用者の演算子ではない。読むあいだだけ足して、クエリの前に外す。
struct SuperOp {
  OperatorTable& ops;
  bool added = false;
  explicit SuperOp(OperatorTable& table) : ops(table) {
    for (const OpDef& op : ops.lookup("^^")) {
      if (op.specifier == Specifier::Fx || op.specifier == Specifier::Fy) return;
    }
    added = ops.define(600, Specifier::Fy, "^^") == OpResult::Ok;
  }
  ~SuperOp() {
    if (!added) return;
    // 優先度 0 はその演算子だけを消す。拒否は表を変えない。
    const OpResult dropped = ops.define(0, Specifier::Fy, "^^");
    if (dropped != OpResult::Ok) return;
  }
};

// 期待の user:項 は読みの表では中置。残すと current_op が変わるので、読んだあと外す。
struct ColonOp {
  OperatorTable& ops;
  bool added = false;
  explicit ColonOp(OperatorTable& table) : ops(table) {
    for (const OpDef& op : ops.lookup(":")) {
      if (op.specifier == Specifier::Xfx || op.specifier == Specifier::Xfy ||
          op.specifier == Specifier::Yfx) {
        return;
      }
    }
    added = ops.define(600, Specifier::Xfy, ":") == OpResult::Ok;
  }
  ~ColonOp() {
    if (!added) return;
    const OpResult dropped = ops.define(0, Specifier::Xfy, ":");
    if (dropped != OpResult::Ok) return;
  }
};

Parsed parse_term(Machine& machine, std::string_view text) {
  Parsed parsed;
  std::string source{text};
  // 字面がもう . で終わっていても足す。+. の . は終止符でなく（6.4.2、レビュー L17）、foo. なら足した終止符は
  // 読まない 2 つめの項の終わりになるだけである。
  source += full_stop_after(source);
  const SuperOp super_op(machine.operators());
  const ColonOp colon_op(machine.operators());
  Reader reader(source, machine.heap(), machine.operators());
  const ReadOutcome outcome = reader.next();
  if (!outcome.term) return parsed;
  parsed.ok = true;
  parsed.term = outcome.term->term;
  parsed.names = outcome.term->names;
  return parsed;
}

const Binding* find_binding(const std::vector<Binding>& bindings, std::string_view name) {
  for (const Binding& binding : bindings) {
    if (binding.name == name) return &binding;
  }
  return nullptr;
}

const VarName* find_var(const std::vector<VarName>& names, Cell cell) {
  for (const VarName& var : names) {
    if (var.cell.raw() == cell.raw()) return &var;
  }
  return nullptr;
}

bool on_stack(const std::vector<std::uint64_t>& stack, std::uint64_t raw) {
  for (const auto seen : stack) {
    if (seen == raw) return true;
  }
  return false;
}

Cell resolve_compound(Heap& heap, Cell cell, const std::vector<VarName>& names, const std::vector<Binding>& bindings,
                      std::vector<std::uint64_t>& stack);

// クエリの変数と、結果の形を読み直した変数は別セル。名前で解のセルへ戻す。
Cell resolve_term(Heap& heap, Cell cell, const std::vector<VarName>& names, const std::vector<Binding>& bindings,
                  std::vector<std::uint64_t>& stack) {
  cell = heap.deref(cell);
  if (heap.unbound(cell)) {
    if (const VarName* var = find_var(names, cell)) {
      if (const Binding* binding = find_binding(bindings, var->name)) return heap.deref(binding->variable);
    }
    return cell;
  }
  if (cell.tag() != Tag::Lis && cell.tag() != Tag::Str) return cell;
  if (on_stack(stack, cell.raw())) return cell;
  return resolve_compound(heap, cell, names, bindings, stack);
}

Cell resolve_compound(Heap& heap, Cell cell, const std::vector<VarName>& names, const std::vector<Binding>& bindings,
                      std::vector<std::uint64_t>& stack) {
  stack.push_back(cell.raw());
  Cell built;
  if (cell.tag() == Tag::Lis) {
    const Cell head = resolve_term(heap, heap.list_head(cell), names, bindings, stack);
    const Cell tail = resolve_term(heap, heap.list_tail(cell), names, bindings, stack);
    built = heap.list(head, tail);
  } else {
    const auto arity = heap.functor_arity(cell);
    std::vector<Cell> args;
    args.reserve(arity);
    for (std::uint16_t index = 0; index < arity; ++index) {
      args.push_back(resolve_term(heap, heap.arg(cell, index), names, bindings, stack));
    }
    built = heap.structure(heap.functor_atom(cell), args);
  }
  stack.pop_back();
  return built;
}

struct Step {
  bool dead = false;
  Stop stop;
};

void clear_halt(Machine& machine) {
  Impl& impl = engine_impl(machine);
  impl.halt_now = false;
  impl.halt_pending = false;
}

Step ask(Machine& machine, std::string_view goal) {
  Step step;
  try {
    step.stop = query_written(machine, goal);
  } catch (...) {
    if (!is_heap_exhaustion()) throw;
    step.dead = true;
    return step;
  }
  clear_halt(machine);
  return step;
}

Step again(Machine& machine) {
  Step step;
  try {
    step.stop = machine.next();
  } catch (...) {
    if (!is_heap_exhaustion()) throw;
    step.dead = true;
    return step;
  }
  clear_halt(machine);
  return step;
}

std::string actual_of(const Machine& machine, const Stop& stop) {
  if (stop.kind == StopKind::Halt) return "halt(" + std::to_string(stop.halt_code) + ")";
  if (stop.kind == StopKind::Success) return "true";
  if (stop.kind != StopKind::Exception) return "fail";
  const Heap& heap = machine.heap();
  const Cell term = heap.deref(stop.exception);
  if (functor_is(heap, term, "error", 2)) {
    return write_term(heap, machine.operators(), heap.arg(term, 0), lgt_write_options(), {});
  }
  return write_term(heap, machine.operators(), term, lgt_write_options(), {});
}

bool special_call(const Heap& heap, Cell cell) {
  cell = heap.deref(cell);
  if (cell.tag() != Tag::Str) return false;
  const auto name = heap.atoms().name(heap.functor_atom(cell));
  const auto arity = heap.functor_arity(cell);
  if (name == "assertion" && (arity == 1 || arity == 2)) return true;
  // forall/2 は組込にしない。表明が無い本体は機械へ落ち、unknown=fail では失敗になる。
  if (name == "forall" && arity == 2) return true;
  if (name == "^^") return true;
  if (name == "variant" && arity == 2) return true;
  if (name == "subsumes" && arity == 2) return true;
  return name == "=~=" && arity == 2;
}

bool contains_special(const Heap& heap, Cell cell, std::vector<std::uint64_t>& stack) {
  cell = heap.deref(cell);
  if (special_call(heap, cell)) return true;
  if (cell.tag() != Tag::Lis && cell.tag() != Tag::Str) return false;
  if (on_stack(stack, cell.raw())) return false;
  stack.push_back(cell.raw());
  bool found = false;
  if (cell.tag() == Tag::Lis) {
    found = contains_special(heap, heap.list_head(cell), stack) ||
            contains_special(heap, heap.list_tail(cell), stack);
  } else {
    const auto arity = heap.functor_arity(cell);
    for (std::uint16_t index = 0; index < arity && !found; ++index) {
      found = contains_special(heap, heap.arg(cell, index), stack);
    }
  }
  stack.pop_back();
  return found;
}

// 試験が変えた Prolog の旗をスクラッチの機械にも写す。前は既定の旗で始め、set_prolog_flag(unknown, fail) の後の
// all(current_prolog_flag(unknown, error)) が通った（レビュー M-51）。デバッガの状態は写さない。
void copy_prolog_flags(const Machine& main, Machine& scratch) {
  const Impl& from = engine_impl(main);
  Impl& to = engine_impl(scratch);
  to.quotes = from.quotes;
  to.unknown_fail = from.unknown_fail;
  to.utf8_replace = from.utf8_replace;
  to.float_overflow_error = from.float_overflow_error;
  to.float_zero_div_error = from.float_zero_div_error;
  to.float_undefined_error = from.float_undefined_error;
  to.japanese_punct = from.japanese_punct;
}

// 開いている query を潰さない。ゴールは字面を通さずセルで写すので、Big も字にせず箱のまま渡る（lgt_copy.cpp）。
LgtScratch scratch_run(Machine& main, std::string_view goal, const std::vector<Binding>& bindings) {
  const Parsed parsed = parse_term(main, goal);
  if (!parsed.ok) return {};
  std::vector<std::uint64_t> stack;
  const Cell resolved = resolve_term(main.heap(), parsed.term, parsed.names, bindings, stack);
  auto scratch = Machine::create();
  for (const OpDef& op : main.operators().entries()) {
    if (scratch->operators().define(op.priority, op.specifier, op.name) != OpResult::Ok) continue;
  }
  copy_prolog_flags(main, *scratch);
  try {
    return lgt_run_copied_goal(main, *scratch, resolved);
  } catch (...) {
    if (!is_heap_exhaustion()) throw;
    LgtScratch dead;
    dead.kind = LgtScratchKind::Dead;
    return dead;
  }
}

// 表明の判定は成否だけを見る。表明の中の例外と halt は、今までどおり不合格になる。
bool scratch_ok(Machine& main, std::string_view goal, const std::vector<Binding>& bindings) {
  return scratch_run(main, goal, bindings).kind == LgtScratchKind::Ok;
}

bool holds(Machine& machine, const LgtWant& want, const Stop& stop) {
  if (want.kind == LgtKind::Det) return true;
  if (lgt_text_has_special(machine, want.detail)) {
    std::string note;
    return lgt_check_text(machine, want.detail, stop.bindings, note);
  }
  return scratch_ok(machine, want.detail, stop.bindings);
}

bool error_formal(const Machine& machine, Cell formal, Cell ball) {
  const Heap& heap = machine.heap();
  ball = heap.deref(ball);
  if (!functor_is(heap, ball, "error", 2)) return false;
  return lgt_subsumes(heap, formal, heap.arg(ball, 0));
}

bool thrown_ok(Machine& machine, const LgtWant& want, const Stop& stop) {
  if (stop.kind != StopKind::Exception) return false;
  const Parsed parsed = parse_term(machine, want.detail);
  if (!parsed.ok) return false;
  if (want.kind == LgtKind::Ball) return lgt_subsumes(machine.heap(), parsed.term, stop.exception);
  if (want.kind != LgtKind::Errors) return false;
  Cell list = machine.heap().deref(parsed.term);
  while (list.tag() == Tag::Lis) {
    if (error_formal(machine, machine.heap().list_head(list), stop.exception)) return true;
    list = machine.heap().deref(machine.heap().list_tail(list));
  }
  return false;
}

LgtEnd aborted() {
  LgtEnd end;
  end.dead = true;
  end.actual = "aborted";
  return end;
}

LgtEnd failed_step(const Machine& machine, const Step& step) {
  if (step.dead) return aborted();
  LgtEnd end;
  end.actual = actual_of(machine, step.stop);
  return end;
}

LgtEnd scan_exists(Machine& machine, const LgtWant& want) {
  while (true) {
    const Step step = again(machine);
    if (step.dead || step.stop.kind != StopKind::Success) return failed_step(machine, step);
    if (holds(machine, want, step.stop)) {
      LgtEnd end;
      end.pass = true;
      return end;
    }
  }
}

LgtEnd scan_all(Machine& machine, const LgtWant& want) {
  while (true) {
    const Step step = again(machine);
    if (step.dead) return aborted();
    if (step.stop.kind == StopKind::Failure) {
      LgtEnd end;
      end.pass = true;
      return end;
    }
    if (step.stop.kind != StopKind::Success || !holds(machine, want, step.stop)) return failed_step(machine, step);
  }
}

// 表明が失敗したあとの解は見ない。成功したやり直しは、このテストの失敗。
LgtEnd finish_det(Machine& machine, const LgtWant& want, const Stop& stop) {
  if (want.kind == LgtKind::DetAssert && !holds(machine, want, stop)) {
    LgtEnd end;
    end.actual = "fail";
    return end;
  }
  const Step step = again(machine);
  if (step.dead) return aborted();
  if (step.stop.kind == StopKind::Failure) {
    LgtEnd end;
    end.pass = true;
    return end;
  }
  return failed_step(machine, step);
}

LgtEnd finish_success(Machine& machine, const LgtWant& want, const Stop& stop) {
  if (want.kind == LgtKind::Det || want.kind == LgtKind::DetAssert) return finish_det(machine, want, stop);
  if (want.kind == LgtKind::Exists) {
    if (holds(machine, want, stop)) {
      LgtEnd end;
      end.pass = true;
      return end;
    }
    return scan_exists(machine, want);
  }
  if (want.kind == LgtKind::All) {
    if (!holds(machine, want, stop)) {
      LgtEnd end;
      end.actual = "fail";
      return end;
    }
    return scan_all(machine, want);
  }
  LgtEnd end;
  if (holds(machine, want, stop)) end.pass = true;
  else end.actual = "fail";
  return end;
}

LgtEnd judge_query(Machine& machine, std::string_view body, const LgtWant& want) {
  const Step step = ask(machine, std::string{body} + full_stop_after(body));
  if (step.dead) return aborted();
  if (want.kind == LgtKind::Errors || want.kind == LgtKind::Ball) {
    LgtEnd end;
    end.pass = thrown_ok(machine, want, step.stop);
    if (!end.pass) end.actual = actual_of(machine, step.stop);
    return end;
  }
  // 解が無い all は合格。最初の結果が失敗のときだけ、解ゼロとみる。
  if (want.kind == LgtKind::All && step.stop.kind == StopKind::Failure) {
    LgtEnd end;
    end.pass = true;
    return end;
  }
  if (step.stop.kind != StopKind::Success) return failed_step(machine, step);
  // 次の解を求めて巻き戻す前に、本体の束縛を字面にする。cleanup(Goal) に貼る（レビュー M-50）。
  std::vector<LgtValue> kept = want.keep ? lgt_values_of(machine, step.stop.bindings) : std::vector<LgtValue>{};
  LgtEnd end = finish_success(machine, want, step.stop);
  end.kept = std::move(kept);
  return end;
}

std::optional<Specifier> specifier_of(std::string_view text) {
  if (text == "xfx") return Specifier::Xfx;
  if (text == "xfy") return Specifier::Xfy;
  if (text == "yfx") return Specifier::Yfx;
  if (text == "fx") return Specifier::Fx;
  if (text == "fy") return Specifier::Fy;
  if (text == "xf") return Specifier::Xf;
  if (text == "yf") return Specifier::Yf;
  return std::nullopt;
}

bool take_op(Heap& heap, OperatorTable& operators, Cell cell, std::vector<LgtOp>& ops) {
  cell = heap.deref(cell);
  if (!functor_is(heap, cell, "op", 3)) return false;
  const Cell priority = heap.deref(heap.arg(cell, 0));
  const Cell specifier = heap.deref(heap.arg(cell, 1));
  const Cell name = heap.deref(heap.arg(cell, 2));
  if (!integer_cell(heap, priority) || specifier.tag() != Tag::Atom || name.tag() != Tag::Atom) return false;
  // Big と int に入らない int64 は優先順位の範囲の外である。切り詰めると別の優先順位で定義してしまうので先に退ける。
  if (priority.tag() == Tag::Big) return false;
  const auto value = heap.integer_value(priority);
  if (value < 0 || value > std::numeric_limits<int>::max()) return false;
  const auto parsed = specifier_of(atom_text(heap, specifier));
  if (!parsed) return false;
  LgtOp op;
  op.priority = static_cast<int>(value);
  op.specifier = *parsed;
  op.name = std::string{atom_text(heap, name)};
  if (operators.define(op.priority, op.specifier, op.name) != OpResult::Ok) return false;
  ops.push_back(std::move(op));
  return true;
}

LgtKind kind_of_one(std::string_view name) {
  if (name == "true") return LgtKind::Assert;
  if (name == "error") return LgtKind::Error;
  if (name == "deterministic") return LgtKind::DetAssert;
  if (name == "errors") return LgtKind::Errors;
  if (name == "ball") return LgtKind::Ball;
  if (name == "exists") return LgtKind::Exists;
  if (name == "all") return LgtKind::All;
  return LgtKind::Other;
}

}  // namespace

LgtParsed lgt_parse_term(Machine& machine, std::string_view text) {
  const Parsed parsed = parse_term(machine, text);
  return LgtParsed{parsed.ok, parsed.term, parsed.names};
}

Cell lgt_resolve_term(Heap& heap, Cell cell, const std::vector<VarName>& names, const std::vector<Binding>& bindings) {
  std::vector<std::uint64_t> stack;
  return resolve_term(heap, cell, names, bindings, stack);
}

LgtScratch lgt_scratch_run(Machine& machine, std::string_view goal, const std::vector<Binding>& bindings) {
  return scratch_run(machine, goal, bindings);
}

LgtStep lgt_ask(Machine& machine, std::string_view goal) {
  const Step step = ask(machine, goal);
  return LgtStep{step.dead, step.stop};
}

LgtStep lgt_again(Machine& machine) {
  const Step step = again(machine);
  return LgtStep{step.dead, step.stop};
}

std::string lgt_actual_of(const Machine& machine, const Stop& stop) { return actual_of(machine, stop); }

LgtEnd lgt_aborted() { return aborted(); }

LgtEnd lgt_failed_step(const Machine& machine, const LgtStep& step) {
  return failed_step(machine, Step{step.dead, step.stop});
}

LgtEnd lgt_finish(Machine& machine, const LgtWant& want, const Stop& stop) {
  return finish_success(machine, want, stop);
}

LgtWant classify_lgt_outcome(Machine& machine, std::string_view outcome) {
  LgtWant want;
  const Parsed parsed = parse_term(machine, outcome);
  if (!parsed.ok) return want;
  const Heap& heap = machine.heap();
  const Cell term = heap.deref(parsed.term);
  if (atom_is(heap, term, "true")) {
    want.kind = LgtKind::True;
    return want;
  }
  if (atom_is(heap, term, "false") || atom_is(heap, term, "fail")) {
    want.kind = LgtKind::False;
    return want;
  }
  if (atom_is(heap, term, "deterministic")) {
    want.kind = LgtKind::Det;
    return want;
  }
  if (term.tag() != Tag::Str) return want;
  const auto name = heap.atoms().name(heap.functor_atom(term));
  const auto arity = heap.functor_arity(term);
  if (arity == 2 && (name == "variant" || name == "subsumes")) {
    want.detail = std::string{outcome};
    want.kind = name == "variant" ? LgtKind::Variant : LgtKind::Subsumes;
    return want;
  }
  if (arity != 1) return want;
  want.kind = kind_of_one(name);
  if (want.kind == LgtKind::Other) return want;
  // 結果の {ゴール} は本体と同じく実行する。述語 {}/1 にすると存在エラーになる。
  Cell shown = heap.arg(term, 0);
  if (want.kind == LgtKind::Assert || want.kind == LgtKind::DetAssert || want.kind == LgtKind::Exists ||
      want.kind == LgtKind::All) {
    shown = lgt_unwrap_curlies(machine.heap(), shown);
  }
  want.detail = write_term(machine.heap(), machine.operators(), shown, lgt_write_options(), parsed.names);
  return want;
}

LgtEnd judge_lgt_case(Machine& machine, std::string_view body, const LgtWant& want, const LgtIo* io) {
  if (lgt_text_has_special(machine, body)) return lgt_judge_mixed(machine, body, want, io);
  return judge_query(machine, body, want);
}

bool lgt_text_has_special(Machine& machine, std::string_view text) {
  if (text.empty()) return false;
  const Parsed parsed = parse_term(machine, text);
  if (!parsed.ok) return false;
  std::vector<std::uint64_t> stack;
  return contains_special(machine.heap(), parsed.term, stack);
}

// (=~=)/2 は演算子だけを、このファイルの表に足す。述語としては登録しない。
bool absorb_lgt_uses(Heap& heap, OperatorTable& operators, Cell term, std::vector<LgtOp>& ops) {
  if (!functor_is(heap, term, ":-", 1)) return false;
  const Cell inner = heap.deref(heap.arg(heap.deref(term), 0));
  if (!functor_is(heap, inner, "uses", 2)) return false;
  if (!atom_is(heap, heap.arg(inner, 0), "lgtunit")) return false;
  Cell list = heap.deref(heap.arg(inner, 1));
  while (list.tag() == Tag::Lis) {
    take_op(heap, operators, heap.list_head(list), ops);
    list = heap.deref(heap.list_tail(list));
  }
  return true;
}

void install_lgt_ops(Machine& machine, const std::vector<LgtOp>& ops) {
  for (const LgtOp& op : ops) {
    // 読み込みのときに一度通っている。ここでは機械の表へ写すだけ。
    if (machine.operators().define(op.priority, op.specifier, op.name) != OpResult::Ok) continue;
  }
}

}  // namespace mosa
