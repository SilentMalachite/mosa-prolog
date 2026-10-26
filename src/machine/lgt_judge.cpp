// 結果の形は機械の next で解を辿る。二度目の query はやり直しではない。
// variant や assertion や =~= や forall は述語にしない。登録すると current_predicate が成功する。

#include "lgt_suite.hpp"
#include "lgt_priv.hpp"

#include "internal.hpp"

#include "mosa/machine.hpp"
#include "mosa/reader.hpp"
#include "mosa/writer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
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
  return cell.tag() == Tag::Int || cell.tag() == Tag::IntBox;
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
  if (source.empty() || source.back() != '.') source.push_back('.');
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

Cell resolved_arg(Machine& machine, Cell cell, std::size_t index, const std::vector<VarName>& names,
                  const std::vector<Binding>& bindings) {
  std::vector<std::uint64_t> stack;
  return resolve_term(machine.heap(), machine.heap().arg(cell, index), names, bindings, stack);
}

int stamp_id(std::vector<std::pair<std::uint64_t, int>>& map, std::uint64_t raw, int& next) {
  for (const auto& [id, number] : map) {
    if (id == raw) return number;
  }
  const int number = next;
  ++next;
  map.emplace_back(raw, number);
  return number;
}

Cell stamp_compound(Heap& heap, Cell cell, std::vector<std::pair<std::uint64_t, int>>& map, int& next,
                    std::vector<std::uint64_t>& stack);

// 番号を整数のまま置くと、項の中の整数と区別できない。コピーなので解の変数は残る。
Cell stamp_term(Heap& heap, Cell cell, std::vector<std::pair<std::uint64_t, int>>& map, int& next,
                std::vector<std::uint64_t>& stack) {
  cell = heap.deref(cell);
  if (heap.unbound(cell)) {
    const std::array<Cell, 1> args{heap.integer(stamp_id(map, cell.raw(), next))};
    return heap.structure(heap.atoms().intern("$n"), args);
  }
  if (cell.tag() != Tag::Lis && cell.tag() != Tag::Str) return cell;
  if (on_stack(stack, cell.raw())) return cell;
  return stamp_compound(heap, cell, map, next, stack);
}

Cell stamp_compound(Heap& heap, Cell cell, std::vector<std::pair<std::uint64_t, int>>& map, int& next,
                    std::vector<std::uint64_t>& stack) {
  stack.push_back(cell.raw());
  Cell built;
  if (cell.tag() == Tag::Lis) {
    const Cell head = stamp_term(heap, heap.list_head(cell), map, next, stack);
    const Cell tail = stamp_term(heap, heap.list_tail(cell), map, next, stack);
    built = heap.list(head, tail);
  } else {
    const auto arity = heap.functor_arity(cell);
    std::vector<Cell> args;
    args.reserve(arity);
    for (std::uint16_t index = 0; index < arity; ++index) {
      args.push_back(stamp_term(heap, heap.arg(cell, index), map, next, stack));
    }
    built = heap.structure(heap.functor_atom(cell), args);
  }
  stack.pop_back();
  return built;
}

bool terms_variant(Heap& heap, Cell left, Cell right) {
  std::vector<std::pair<std::uint64_t, int>> left_map;
  std::vector<std::pair<std::uint64_t, int>> right_map;
  std::vector<std::uint64_t> stack;
  int left_next = 0;
  int right_next = 0;
  const Cell stamped_left = stamp_term(heap, left, left_map, left_next, stack);
  stack.clear();
  const Cell stamped_right = stamp_term(heap, right, right_map, right_next, stack);
  return lgt_same_shape(heap, stamped_left, stamped_right);
}

std::optional<double> as_number(const Heap& heap, Cell cell) {
  cell = heap.deref(cell);
  if (integer_cell(heap, cell)) return static_cast<double>(heap.integer_value(cell));
  if (cell.tag() == Tag::Float) return heap.float_value(cell);
  // 表明の -125.0 は前置のまま残る。=~= は算術なので、数の符号だけ畳む。
  if (cell.tag() != Tag::Str || heap.functor_arity(cell) != 1) return std::nullopt;
  const auto name = heap.atoms().name(heap.functor_atom(cell));
  if (name != "-" && name != "+") return std::nullopt;
  const Cell arg = heap.deref(heap.arg(cell, 0));
  std::optional<double> inner;
  if (integer_cell(heap, arg)) inner = static_cast<double>(heap.integer_value(arg));
  else if (arg.tag() == Tag::Float) inner = heap.float_value(arg);
  else return std::nullopt;
  if (name == "+") return inner;
  return -*inner;
}

bool ordered(std::string_view name, double left, double right) {
  if (name == "<") return left < right;
  if (name == "=<") return left <= right;
  if (name == ">") return left > right;
  if (name == ">=") return left >= right;
  return false;
}

// epsilon/0 と同じ定数。ここで query すると、本体の選択点が消える。
bool approx(const Heap& heap, Cell left, Cell right) {
  const auto first = as_number(heap, left);
  const auto second = as_number(heap, right);
  if (!first || !second) return false;
  const double epsilon = std::numeric_limits<double>::epsilon();
  const double scale = std::max(std::fabs(*first), std::fabs(*second));
  return std::fabs(*first - *second) <= scale * epsilon;
}

bool compare_args(Machine& machine, std::string_view name, Cell cell, const std::vector<VarName>& names,
                  const std::vector<Binding>& bindings) {
  const Cell left = resolved_arg(machine, cell, 0, names, bindings);
  const Cell right = resolved_arg(machine, cell, 1, names, bindings);
  if (name == "==") return lgt_same_shape(machine.heap(), left, right);
  if (name == "=~=") return approx(machine.heap(), left, right);
  const auto first = as_number(machine.heap(), left);
  const auto second = as_number(machine.heap(), right);
  if (!first || !second) return false;
  return ordered(name, *first, *second);
}

bool check_goal(Machine& machine, Cell cell, const std::vector<VarName>& names, const std::vector<Binding>& bindings,
                std::string& note);

bool check_variant(Machine& machine, Cell cell, const std::vector<VarName>& names,
                   const std::vector<Binding>& bindings) {
  const Cell left = resolved_arg(machine, cell, 0, names, bindings);
  const Cell right = resolved_arg(machine, cell, 1, names, bindings);
  return terms_variant(machine.heap(), left, right);
}

bool check_subsumes(Machine& machine, Cell cell, const std::vector<VarName>& names,
                    const std::vector<Binding>& bindings) {
  const Cell general = resolved_arg(machine, cell, 0, names, bindings);
  const Cell specific = resolved_arg(machine, cell, 1, names, bindings);
  return lgt_subsumes(machine.heap(), general, specific);
}

bool check_compound(Machine& machine, Cell cell, const std::vector<VarName>& names,
                    const std::vector<Binding>& bindings, std::string& note) {
  const Heap& heap = machine.heap();
  const auto name = heap.atoms().name(heap.functor_atom(cell));
  const auto arity = heap.functor_arity(cell);
  if (name == "," && arity == 2) {
    return check_goal(machine, heap.arg(cell, 0), names, bindings, note) &&
           check_goal(machine, heap.arg(cell, 1), names, bindings, note);
  }
  // 選言の左右は同じ判定。左が失敗しても、その説明で右を不合格にしない。
  if (name == ";" && arity == 2) {
    std::string ignored;
    if (check_goal(machine, heap.arg(cell, 0), names, bindings, ignored)) return true;
    return check_goal(machine, heap.arg(cell, 1), names, bindings, note);
  }
  if (name == "assertion" && arity == 1) return check_goal(machine, heap.arg(cell, 0), names, bindings, note);
  if (name == "assertion" && arity == 2) {
    if (check_goal(machine, heap.arg(cell, 1), names, bindings, note)) return true;
    note = write_term(heap, machine.operators(), heap.arg(cell, 0), {}, names);
    return false;
  }
  if (name == "variant" && arity == 2) return check_variant(machine, cell, names, bindings);
  if ((name == "subsumes" || name == "subsumes_term") && arity == 2) {
    return check_subsumes(machine, cell, names, bindings);
  }
  if (arity == 2 && (name == "==" || name == "=~=" || name == "<" || name == "=<" || name == ">" || name == ">=")) {
    return compare_args(machine, name, cell, names, bindings);
  }
  return false;
}

bool check_goal(Machine& machine, Cell cell, const std::vector<VarName>& names, const std::vector<Binding>& bindings,
                std::string& note) {
  const Heap& heap = machine.heap();
  cell = heap.deref(cell);
  if (atom_is(heap, cell, "true")) return true;
  if (atom_is(heap, cell, "fail") || atom_is(heap, cell, "false")) return false;
  if (cell.tag() != Tag::Str) return false;
  return check_compound(machine, cell, names, bindings, note);
}

bool check_text(Machine& machine, std::string_view text, const std::vector<Binding>& bindings, std::string& note) {
  const Parsed parsed = parse_term(machine, text);
  if (!parsed.ok) return false;
  return check_goal(machine, parsed.term, parsed.names, bindings, note);
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
    step.stop = machine.query(goal);
  } catch (const ResourceLimit&) {
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
  } catch (const ResourceLimit&) {
    step.dead = true;
    return step;
  }
  clear_halt(machine);
  return step;
}

std::string actual_of(const Machine& machine, const Stop& stop) {
  if (stop.kind == StopKind::Success || stop.kind == StopKind::Halt) return "true";
  if (stop.kind != StopKind::Exception) return "fail";
  const Heap& heap = machine.heap();
  const Cell term = heap.deref(stop.exception);
  if (functor_is(heap, term, "error", 2)) {
    return write_term(heap, machine.operators(), heap.arg(term, 0), {}, {});
  }
  return write_term(heap, machine.operators(), term, {}, {});
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
  if ((name == "subsumes" || name == "subsumes_term") && arity == 2) return true;
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

const Cell* copied_seen(const std::vector<std::pair<std::uint64_t, Cell>>& seen, std::uint64_t raw) {
  for (const auto& item : seen) {
    if (item.first == raw) return &item.second;
  }
  return nullptr;
}

Cell copy_atom(Impl& dst, const Impl& src, Cell cell) {
  const auto name = src.atoms.name(static_cast<std::uint32_t>(cell.raw() >> 3));
  return dst.heap.atom(dst.atoms.intern(name));
}

Cell copy_number(Impl& dst, const Impl& src, Cell cell) {
  if (cell.tag() == Tag::Float) return dst.heap.floating(src.heap.float_value(cell));
  return dst.heap.integer(src.heap.integer_value(cell));
}

Cell copy_goal(Impl& src, Impl& dst, Cell cell, std::vector<std::pair<std::uint64_t, Cell>>& seen);

Cell copy_compound(Impl& src, Impl& dst, Cell cell, std::vector<std::pair<std::uint64_t, Cell>>& seen) {
  const Cell hole = dst.heap.fresh_ref();
  seen.emplace_back(cell.raw(), hole);
  Cell real;
  if (cell.tag() == Tag::Lis) {
    const Cell head = copy_goal(src, dst, src.heap.list_head(cell), seen);
    const Cell tail = copy_goal(src, dst, src.heap.list_tail(cell), seen);
    real = dst.heap.list(head, tail);
  } else {
    const auto arity = src.heap.functor_arity(cell);
    std::vector<Cell> args;
    args.reserve(arity);
    for (std::uint16_t index = 0; index < arity; ++index) {
      args.push_back(copy_goal(src, dst, src.heap.arg(cell, index), seen));
    }
    const auto name = src.atoms.name(src.heap.functor_atom(cell));
    real = dst.heap.structure(dst.atoms.intern(name), args);
  }
  dst.heap.bind(hole, real);
  return hole;
}

Cell copy_goal(Impl& src, Impl& dst, Cell cell, std::vector<std::pair<std::uint64_t, Cell>>& seen) {
  cell = src.heap.deref(cell);
  const bool share = src.heap.unbound(cell) || cell.tag() == Tag::Lis || cell.tag() == Tag::Str;
  if (share) {
    if (const Cell* found = copied_seen(seen, cell.raw())) return *found;
  }
  if (src.heap.unbound(cell)) {
    const Cell fresh = dst.heap.fresh_ref();
    seen.emplace_back(cell.raw(), fresh);
    return fresh;
  }
  if (cell.tag() == Tag::Atom) return copy_atom(dst, src, cell);
  if (cell.tag() == Tag::Int || cell.tag() == Tag::IntBox || cell.tag() == Tag::Float) {
    return copy_number(dst, src, cell);
  }
  if (cell.tag() != Tag::Lis && cell.tag() != Tag::Str) return {};
  return copy_compound(src, dst, cell, seen);
}

bool copied_succeeds(Machine& scratch, Cell goal) {
  Impl& machine = engine_impl(scratch);
  Entry cleanup;
  cleanup.pick = Pick::Cleanup;
  cleanup.heap_mark = machine.heap.size();
  cleanup.trail_mark = machine.trail.slots.size();
  cleanup.extras_mark = machine.extras.size();
  machine.stack.push_back(std::move(cleanup));
  const Compiled compiled = compile_goal(machine.heap, goal);
  launch(machine, compiled.term, kNoFrame, 0, compiled.env);
  return run_loop(machine).kind == StopKind::Success;
}

// 開いている query を潰さない。-2^63 は字面にすると正のトークンが溢れて読めず、値が変数になる。
bool scratch_ok(Machine& main, std::string_view goal, const std::vector<Binding>& bindings) {
  const Parsed parsed = parse_term(main, goal);
  if (!parsed.ok) return false;
  std::vector<std::uint64_t> stack;
  const Cell resolved = resolve_term(main.heap(), parsed.term, parsed.names, bindings, stack);
  auto scratch = Machine::create();
  for (const OpDef& op : main.operators().entries()) {
    if (scratch->operators().define(op.priority, op.specifier, op.name) != OpResult::Ok) continue;
  }
  try {
    std::vector<std::pair<std::uint64_t, Cell>> seen;
    const Cell copied = copy_goal(engine_impl(main), engine_impl(*scratch), resolved, seen);
    return copied_succeeds(*scratch, copied);
  } catch (const ResourceLimit&) {
    return false;
  }
}

bool holds(Machine& machine, const LgtWant& want, const Stop& stop) {
  if (want.kind == LgtKind::Det) return true;
  if (lgt_text_has_special(machine, want.detail)) {
    std::string note;
    return check_text(machine, want.detail, stop.bindings, note);
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
  const Step step = ask(machine, std::string{body} + ".");
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
  return finish_success(machine, want, step.stop);
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
  const auto parsed = specifier_of(atom_text(heap, specifier));
  if (!parsed) return false;
  LgtOp op;
  op.priority = static_cast<int>(heap.integer_value(priority));
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

bool lgt_check_text(Machine& machine, std::string_view text, const std::vector<Binding>& bindings, std::string& note) {
  return check_text(machine, text, bindings, note);
}

bool lgt_scratch_ok(Machine& machine, std::string_view goal, const std::vector<Binding>& bindings) {
  return scratch_ok(machine, goal, bindings);
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
  want.detail = write_term(heap, machine.operators(), heap.arg(term, 0), {}, parsed.names);
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
