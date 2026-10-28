#include "load_internal.hpp"

#include "mosa/writer.hpp"

#include <optional>
#include <string>
#include <utility>
#include <vector>

// 読み込みの指令（op、dynamic、discontiguous、multifile、initialization、旗、文字変換）を振り分ける。
namespace mosa {

namespace {

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

std::string op_failure(OpResult result) {
  switch (result) {
    case OpResult::BadPriority: return "優先度が範囲外です";
    case OpResult::BadSpecifier: return "演算子の指定子が不正です";
    case OpResult::ModifyComma: return "コンマの演算子は変更できません";
    case OpResult::CreateEmpty: return "空リストや空の中括弧は演算子にできません";
    case OpResult::InfixAndPostfix: return "中置と後置は同時に宣言できません";
    case OpResult::CreateBar: return "この縦棒は演算子にできません";
    case OpResult::Ok: return {};
  }
  return "演算子を宣言できません";
}

bool define_name(Impl& machine, int priority, Specifier specifier, std::string name) {
  const OpResult result = machine.operators.define(priority, specifier, std::move(name));
  if (result == OpResult::Ok) return true;
  fail_load(machine, op_failure(result));
  return false;
}

bool define_op_names(Impl& machine, int priority, Specifier specifier, Cell names) {
  names = machine.heap.deref(names);
  if (names.tag() == Tag::Atom) {
    return define_name(machine, priority, specifier, std::string(machine.atoms.name(static_cast<std::uint32_t>(names.raw() >> 3))));
  }
  const auto nil = machine.atoms.intern("[]");
  while (names.tag() == Tag::Lis) {
    const Cell head = machine.heap.deref(machine.heap.list_head(names));
    if (head.tag() != Tag::Atom) {
      fail_load(machine, "演算子名がアトムではありません");
      return false;
    }
    if (!define_name(machine, priority, specifier,
                     std::string(machine.atoms.name(static_cast<std::uint32_t>(head.raw() >> 3))))) {
      return false;
    }
    names = machine.heap.deref(machine.heap.list_tail(names));
  }
  if (!(names.tag() == Tag::Atom && static_cast<std::uint32_t>(names.raw() >> 3) == nil)) {
    fail_load(machine, "演算子名がアトムではありません");
    return false;
  }
  return true;
}

bool apply_op(Impl& machine, Cell directive) {
  const Cell body = machine.heap.deref(machine.heap.arg(directive, 0));
  if (!(body.tag() == Tag::Str && machine.heap.functor_arity(body) == 3 &&
        machine.atoms.name(machine.heap.functor_atom(body)) == "op")) {
    return false;
  }
  const Cell priority_cell = machine.heap.deref(machine.heap.arg(body, 0));
  const Cell spec_cell = machine.heap.deref(machine.heap.arg(body, 1));
  if (priority_cell.tag() != Tag::Int && priority_cell.tag() != Tag::IntBox) return false;
  if (spec_cell.tag() != Tag::Atom) return false;
  const auto specifier = specifier_of(machine.atoms.name(static_cast<std::uint32_t>(spec_cell.raw() >> 3)));
  if (!specifier) {
    fail_load(machine, op_failure(OpResult::BadSpecifier));
    return false;
  }
  const auto priority64 = machine.heap.integer_value(priority_cell);
  if (priority64 < 0 || priority64 > 1200) {
    fail_load(machine, op_failure(OpResult::BadPriority));
    return false;
  }
  return define_op_names(machine, static_cast<int>(priority64), *specifier, machine.heap.arg(body, 2));
}

bool one_indicator(const Impl& machine, Cell cell, std::uint32_t& name, std::uint16_t& arity) {
  cell = machine.heap.deref(cell);
  if (cell.tag() != Tag::Str || machine.heap.functor_arity(cell) != 2) return false;
  if (machine.atoms.name(machine.heap.functor_atom(cell)) != "/") return false;
  const Cell left = machine.heap.deref(machine.heap.arg(cell, 0));
  const Cell right = machine.heap.deref(machine.heap.arg(cell, 1));
  if (left.tag() != Tag::Atom) return false;
  if (right.tag() != Tag::Int && right.tag() != Tag::IntBox) return false;
  const auto value = machine.heap.integer_value(right);
  if (value < 0 || value > 1024) return false;
  name = static_cast<std::uint32_t>(left.raw() >> 3);
  arity = static_cast<std::uint16_t>(value);
  return true;
}

bool collect_indicators(Impl& machine, Cell cell, std::vector<std::pair<std::uint32_t, std::uint16_t>>& out) {
  cell = machine.heap.deref(cell);
  std::uint32_t name = 0;
  std::uint16_t arity = 0;
  if (one_indicator(machine, cell, name, arity)) {
    out.emplace_back(name, arity);
    return true;
  }
  const auto nil = machine.atoms.intern("[]");
  if (cell.tag() == Tag::Atom && static_cast<std::uint32_t>(cell.raw() >> 3) == nil) return true;
  while (cell.tag() == Tag::Lis) {
    if (!one_indicator(machine, machine.heap.list_head(cell), name, arity)) return false;
    out.emplace_back(name, arity);
    cell = machine.heap.deref(machine.heap.list_tail(cell));
  }
  return cell.tag() == Tag::Atom && static_cast<std::uint32_t>(cell.raw() >> 3) == nil;
}

bool mark_dynamic(Impl& machine, std::vector<Snapshot>& snapshots, std::uint32_t name, std::uint16_t arity) {
  if (is_builtin_procedure(machine, name, arity)) {
    const Cell formal = formal_permission(machine, "modify", "static_procedure", cell_functor(machine, name, arity));
    fail_load(machine, write_term(machine.heap, machine.operators, formal, {}, {}));
    return false;
  }
  Predicate* predicate = find_pred(machine, name, arity);
  if (predicate != nullptr && !predicate->dynamic && !predicate->clauses.empty()) {
    fail_load(machine, "静的な述語は動的にできない");
    return false;
  }
  // 失敗した読み込みは、宣言の前の動的フラグへ戻す。
  remember_pred(machine, snapshots, name, arity);
  if (predicate == nullptr) {
    Predicate created;
    created.name = name;
    created.arity = arity;
    created.dynamic = true;
    created.dynamic_declared = true;
    machine.predicates.push_back(std::move(created));
    return true;
  }
  predicate->dynamic = true;
  predicate->dynamic_declared = true;
  return true;
}

bool dynamic_argument(Impl& machine, Cell directive, Cell& arg) {
  directive = machine.heap.deref(directive);
  if (directive.tag() != Tag::Str || machine.heap.functor_arity(directive) != 1) return false;
  const Cell body = machine.heap.deref(machine.heap.arg(directive, 0));
  if (body.tag() != Tag::Str || machine.heap.functor_arity(body) != 1) return false;
  if (machine.atoms.name(machine.heap.functor_atom(body)) != "dynamic") return false;
  arg = machine.heap.arg(body, 0);
  return true;
}

bool discontiguous_argument(Impl& machine, Cell directive, Cell& arg) {
  directive = machine.heap.deref(directive);
  if (directive.tag() != Tag::Str || machine.heap.functor_arity(directive) != 1) return false;
  const Cell body = machine.heap.deref(machine.heap.arg(directive, 0));
  if (body.tag() != Tag::Str || machine.heap.functor_arity(body) != 1) return false;
  if (machine.atoms.name(machine.heap.functor_atom(body)) != "discontiguous") return false;
  arg = machine.heap.arg(body, 0);
  return true;
}

bool multifile_argument(Impl& machine, Cell directive, Cell& arg) {
  directive = machine.heap.deref(directive);
  if (directive.tag() != Tag::Str || machine.heap.functor_arity(directive) != 1) return false;
  const Cell body = machine.heap.deref(machine.heap.arg(directive, 0));
  if (body.tag() != Tag::Str || machine.heap.functor_arity(body) != 1) return false;
  if (machine.atoms.name(machine.heap.functor_atom(body)) != "multifile") return false;
  arg = machine.heap.arg(body, 0);
  return true;
}

int apply_multifile_directive(Impl& machine, Cell directive) {
  Cell arg;
  if (!multifile_argument(machine, directive, arg)) return 0;
  std::vector<std::pair<std::uint32_t, std::uint16_t>> indicators;
  if (!collect_indicators(machine, arg, indicators)) {
    fail_load(machine, "述語指示子ではありません");
    return -1;
  }
  for (const auto& indicator : indicators) note_multifile(machine, indicator.first, indicator.second);
  return 1;
}

int apply_discontiguous_directive(Impl& machine, Cell directive, std::vector<Apart>& apart) {
  Cell arg;
  if (!discontiguous_argument(machine, directive, arg)) return 0;
  std::vector<std::pair<std::uint32_t, std::uint16_t>> indicators;
  if (!collect_indicators(machine, arg, indicators)) {
    fail_load(machine, "述語指示子ではありません");
    return -1;
  }
  for (const auto& indicator : indicators) {
    if (allows_gap(apart, indicator.first, indicator.second)) continue;
    apart.push_back(Apart{indicator.first, indicator.second});
  }
  return 1;
}

int apply_dynamic_directive(Impl& machine, Cell directive, std::vector<Snapshot>& snapshots) {
  Cell arg;
  if (!dynamic_argument(machine, directive, arg)) return 0;
  std::vector<std::pair<std::uint32_t, std::uint16_t>> indicators;
  if (!collect_indicators(machine, arg, indicators)) {
    fail_load(machine, "述語指示子ではありません");
    return -1;
  }
  for (const auto& indicator : indicators) {
    if (!mark_dynamic(machine, snapshots, indicator.first, indicator.second)) return -1;
  }
  return 1;
}

struct SavedExec {
  std::vector<Entry> stack;
  std::vector<Extra> extras;
  std::vector<std::uint64_t*> trail;
  const std::uint64_t* trail_mark = nullptr;
  std::uint64_t ip = kNoFrame;
  bool failed = false;
  bool exception = false;
  bool eval_abort = false;
  bool halt_now = false;
  int halt_code = 0;
  Cell ball{};
  std::vector<Binding> bindings;
};

// 実行中の consult の選択点を、初期化ゴールが潰さないように退避する。
SavedExec park_exec(Impl& machine) {
  SavedExec saved;
  saved.stack.swap(machine.stack);
  saved.extras.swap(machine.extras);
  saved.trail.swap(machine.trail.slots);
  saved.trail_mark = machine.trail.mark;
  machine.trail.mark = nullptr;
  saved.ip = machine.ip;
  machine.ip = kNoFrame;
  saved.failed = machine.failed;
  machine.failed = false;
  saved.exception = machine.exception;
  machine.exception = false;
  saved.eval_abort = machine.eval_abort;
  machine.eval_abort = false;
  saved.halt_now = machine.halt_now;
  saved.halt_code = machine.halt_code;
  machine.halt_now = false;
  saved.ball = machine.ball;
  machine.ball = Cell{};
  saved.bindings.swap(machine.bindings);
  return saved;
}

void unpark_exec(Impl& machine, SavedExec& saved, bool keep_halt) {
  const bool halted = machine.halt_now;
  const int code = machine.halt_code;
  machine.stack.swap(saved.stack);
  machine.extras.swap(saved.extras);
  machine.trail.slots.swap(saved.trail);
  machine.trail.mark = saved.trail_mark;
  machine.ip = saved.ip;
  machine.failed = saved.failed;
  machine.exception = saved.exception;
  machine.eval_abort = saved.eval_abort;
  machine.halt_now = saved.halt_now;
  machine.halt_code = saved.halt_code;
  machine.ball = saved.ball;
  machine.bindings.swap(saved.bindings);
  if (keep_halt && halted) {
    machine.halt_now = true;
    machine.halt_code = code;
  }
}

std::string formal_text(Impl& machine, Cell ball) {
  Cell term = machine.heap.deref(ball);
  if (term.tag() == Tag::Str && machine.heap.functor_arity(term) == 2 &&
      machine.atoms.name(machine.heap.functor_atom(term)) == "error") {
    term = machine.heap.arg(term, 0);
  }
  return write_term(machine.heap, machine.operators, term, {}, {});
}

bool initialization_argument(Impl& machine, Cell directive, Cell& arg) {
  directive = machine.heap.deref(directive);
  if (directive.tag() != Tag::Str || machine.heap.functor_arity(directive) != 1) return false;
  const Cell body = machine.heap.deref(machine.heap.arg(directive, 0));
  if (body.tag() != Tag::Str || machine.heap.functor_arity(body) != 1) return false;
  if (machine.atoms.name(machine.heap.functor_atom(body)) != "initialization") return false;
  arg = machine.heap.arg(body, 0);
  return true;
}

int apply_initialization(Impl& machine, Cell directive, std::vector<Cell>& goals) {
  Cell arg;
  if (!initialization_argument(machine, directive, arg)) return 0;
  arg = machine.heap.deref(arg);
  if (machine.heap.unbound(arg)) {
    fail_load(machine, write_term(machine.heap, machine.operators, formal_instantiation(machine), {}, {}));
    return -1;
  }
  if (arg.tag() != Tag::Atom && arg.tag() != Tag::Str && arg.tag() != Tag::Lis) {
    const Cell formal = formal_type(machine, "callable", arg);
    fail_load(machine, write_term(machine.heap, machine.operators, formal, {}, {}));
    return -1;
  }
  goals.push_back(arg);
  return 1;
}


// 失敗はエラーにしない。例外の Formal だけを診断に残し、節は呼び出し側が戻さない。
bool run_init_goal(Impl& machine, Cell goal) {
  SavedExec saved = park_exec(machine);
  const Compiled compiled = compile_goal(machine.heap, goal);
  const auto heap_mark = machine.heap.size();
  Entry cleanup;
  cleanup.pick = Pick::Cleanup;
  cleanup.heap_mark = heap_mark;
  cleanup.trail_mark = 0;
  cleanup.extras_mark = 0;
  machine.stack.push_back(std::move(cleanup));
  launch(machine, compiled.term, kNoFrame, 0, compiled.env);
  const Stop stop = run_loop(machine);
  const bool threw = stop.kind == StopKind::Exception;
  if (threw) fail_load(machine, formal_text(machine, stop.exception));
  restore_state(machine, heap_mark, 0, 0);
  unpark_exec(machine, saved, true);
  return !threw;
}

}  // namespace

bool run_inits(Impl& machine, const std::vector<Cell>& goals) {
  for (const Cell goal : goals) {
    if (!run_init_goal(machine, goal)) return false;
  }
  return true;
}

int consume_directive(Impl& machine, Reader& reader, Cell term, std::vector<Snapshot>& snapshots,
                      std::vector<Apart>& apart, std::vector<Cell>& inits) {
  if (!is_directive(machine.heap, term)) return 0;
  const int flag_directive = apply_flag_directive(machine, term);
  if (flag_directive < 0) return -1;
  if (flag_directive > 0) {
    reader.set_quotes(current_double_quotes(machine));
    return 1;
  }
  const int dynamic_directive = apply_dynamic_directive(machine, term, snapshots);
  if (dynamic_directive < 0) return -1;
  if (dynamic_directive > 0) return 1;
  const int apart_directive = apply_discontiguous_directive(machine, term, apart);
  if (apart_directive < 0) return -1;
  if (apart_directive > 0) return 1;
  const int shared = apply_multifile_directive(machine, term);
  if (shared < 0) return -1;
  if (shared > 0) return 1;
  const int init = apply_initialization(machine, term, inits);
  if (init < 0) return -1;
  if (init > 0) return 1;
  const int conversion = apply_char_conv_directive(machine, term);
  if (conversion < 0) return -1;
  if (conversion > 0) return 1;
  if (!apply_op(machine, machine.heap.deref(term))) {
    if (machine.diagnostic.empty()) {
      const auto text = write_term(machine.heap, machine.operators, term, {}, {});
      fail_load(machine, "この指示はフェーズ2では使えない: " + text);
    }
    return -1;
  }
  return 1;
}

}
