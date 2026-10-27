#include "internal.hpp"

#include "mosa/reader.hpp"
#include "mosa/syntax_error.hpp"
#include "mosa/writer.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace mosa {

// 定義は load_path.cpp。include と ensure_loaded は同じ経路で開く。
std::filesystem::path resolve_include(std::string_view directory, std::string_view atom);
std::filesystem::path canonical_key(const std::filesystem::path& file);
std::filesystem::path prolog_source_path(const std::filesystem::path& file);

namespace {

struct Snapshot {
  std::uint32_t home = 0;
  std::uint32_t name = 0;
  std::uint16_t arity = 0;
  std::vector<Clause> clauses;
  bool created = false;
  bool dynamic = false;
};

// この読み込みの中だけで隙間を許す。述語ベクタには入れない。宣言は手続きを作らない。
struct Apart {
  std::uint32_t name = 0;
  std::uint16_t arity = 0;
};

std::string reason_text(SyntaxReason reason) {
  switch (reason) {
    case SyntaxReason::InvalidUtf8: return "不正な UTF-8";
    case SyntaxReason::IllegalCharacter: return "使えない文字";
    case SyntaxReason::UnterminatedQuote: return "引用が閉じていない";
    case SyntaxReason::UnterminatedComment: return "コメントが閉じていない";
    case SyntaxReason::BadEscape: return "不正なエスケープ";
    case SyntaxReason::IntegerOverflow: return "整数が範囲を超えている";
    case SyntaxReason::UnexpectedToken: return "予期しないトークン";
    case SyntaxReason::MaxArity: return "引数が多すぎる";
    case SyntaxReason::IncompleteTerm: return "項が閉じていません";
  }
  return "予期しないトークン";
}

void fail_load(Impl& machine, std::string message) {
  machine.diagnostic = std::move(message);
  machine.syntax_diag = false;
}

std::uint32_t head_name(const Clause& clause) { return clause.head.atom; }

std::uint16_t head_arity(const Clause& clause) {
  if (clause.head.tag == IrTag::Atom) return 0;
  return clause.head.arity;
}

bool is_rule_arrow(const Heap& heap, Cell cell) {
  cell = heap.deref(cell);
  return cell.tag() == Tag::Str && heap.functor_arity(cell) == 2 && heap.atoms().name(heap.functor_atom(cell)) == "-->";
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
  const OpResult result = machine.operators.define(priority, specifier, name);
  if (result != OpResult::Ok) {
    fail_load(machine, op_failure(result));
    return false;
  }
  if (machine.load_home != machine.user_mod) note_declared_op(machine, priority, specifier, std::move(name));
  return true;
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

bool allows_gap(const std::vector<Apart>& apart, std::uint32_t name, std::uint16_t arity) {
  for (const Apart& item : apart) {
    if (item.name == name && item.arity == arity) return true;
  }
  return false;
}

bool add_clause(Impl& machine, Clause clause, std::size_t& last, const std::vector<Apart>& apart) {
  const auto name = head_name(clause);
  const auto arity = head_arity(clause);
  std::size_t index = machine.predicates.size();
  for (std::size_t cursor = 0; cursor < machine.predicates.size(); ++cursor) {
    const Predicate& candidate = machine.predicates[cursor];
    if (candidate.home == machine.load_home && candidate.name == name && candidate.arity == arity) index = cursor;
  }
  if (index == machine.predicates.size()) {
    machine.predicates.push_back(Predicate{});
    machine.predicates.back().home = machine.load_home;
    machine.predicates.back().name = name;
    machine.predicates.back().arity = arity;
  }
  Predicate& predicate = machine.predicates[index];
  // 宣言の無い述語は、これまでと同じ診断のまま失敗する。
  if (predicate.seen && last != index && !allows_gap(apart, name, arity)) {
    fail_load(machine, "述語 " + std::string(machine.atoms.name(name)) + "/" + std::to_string(arity) +
                            " の節が離れている");
    return false;
  }
  if (!predicate.seen && !predicate.clauses.empty() && !multifile_keeps(machine, machine.load_home, name, arity)) {
    // 足す宣言がある述語は消さない。選択点が残っていても置き換えではない。
    bool busy = false;
    for (const Entry& entry : machine.stack) {
      if (entry.pick != Pick::Cleanup && entry.pick != Pick::Catch) busy = true;
    }
    if (busy) {
      fail_load(machine, "実行中の述語は置き換えられない");
      return false;
    }
    predicate.clauses.clear();
    // 宣言が無い読み込みは、以前動的でも置き換えた述語を静的に戻す。
    if (!predicate.dynamic_declared) predicate.dynamic = false;
  }
  predicate.seen = true;
  last = index;
  predicate.clauses.push_back(std::move(clause));
  return true;
}

void remember_pred(Impl& machine, std::vector<Snapshot>& snapshots, std::uint32_t name, std::uint16_t arity) {
  const bool known = std::any_of(snapshots.begin(), snapshots.end(), [&](const Snapshot& shot) {
    return shot.name == name && shot.arity == arity;
  });
  if (known) return;
  Predicate* predicate = predicate_in(machine, machine.load_home, name, arity);
  Snapshot shot;
  shot.home = machine.load_home;
  shot.name = name;
  shot.arity = arity;
  shot.created = predicate == nullptr;
  if (predicate != nullptr) {
    shot.clauses = predicate->clauses;
    shot.dynamic = predicate->dynamic;
  }
  snapshots.push_back(std::move(shot));
}

void remember(Impl& machine, std::vector<Snapshot>& snapshots, const Clause& clause) {
  remember_pred(machine, snapshots, head_name(clause), head_arity(clause));
}

void rollback(Impl& machine, const std::vector<Snapshot>& snapshots, std::size_t previous_size) {
  for (const Snapshot& shot : snapshots) {
    if (shot.created) continue;
    Predicate* predicate = predicate_in(machine, shot.home, shot.name, shot.arity);
    if (predicate != nullptr) {
      predicate->clauses = shot.clauses;
      predicate->dynamic = shot.dynamic;
    }
  }
  machine.predicates.resize(previous_size);
}

struct SavedFlags {
  DoubleQuotes quotes = DoubleQuotes::Codes;
  bool unknown_fail = false;
  bool float_overflow_error = true;
  bool float_zero_div_error = true;
  bool float_undefined_error = true;
  bool char_conv_on = false;
  bool utf8_replace = false;  // 失敗した読み込みは、途中の set_prolog_flag を残さない。
  bool debug_on = false;
  std::vector<std::pair<char32_t, char32_t>> char_map;
};

SavedFlags save_flags(const Impl& machine) {
  return SavedFlags{machine.quotes,
                    machine.unknown_fail,
                    machine.float_overflow_error,
                    machine.float_zero_div_error,
                    machine.float_undefined_error,
                    machine.char_conv.enabled,
                    machine.utf8_replace,
                    machine.debug_on,
                    machine.char_conv.map};
}

void restore_flags(Impl& machine, const SavedFlags& saved) {
  machine.quotes = saved.quotes;
  machine.unknown_fail = saved.unknown_fail;
  machine.float_overflow_error = saved.float_overflow_error;
  machine.float_zero_div_error = saved.float_zero_div_error;
  machine.float_undefined_error = saved.float_undefined_error;
  machine.char_conv.enabled = saved.char_conv_on;
  machine.utf8_replace = saved.utf8_replace;
  machine.debug_on = saved.debug_on;
  machine.char_conv.map = saved.char_map;
}

LoadStatus reject_load(Impl& machine, const OperatorTable& saved_ops, const SavedFlags& saved_flags,
                       const std::vector<Snapshot>& snapshots, std::size_t previous_size,
                       std::size_t heap_base, const std::vector<SharedPred>& saved_multi,
                       const std::vector<std::string>& saved_prepared) {
  machine.operators = saved_ops;
  restore_flags(machine, saved_flags);
  machine.multifile = saved_multi;
  machine.prepared = saved_prepared;
  rollback(machine, snapshots, previous_size);
  machine.heap.truncate(heap_base);
  return LoadStatus{false, machine.diagnostic};
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

bool mark_dynamic(Impl& machine, std::vector<Snapshot>& snapshots, std::uint32_t home, std::uint32_t name, std::uint16_t arity) {
  if (is_builtin_procedure(machine, name, arity)) {
    const Cell formal = formal_permission(machine, "modify", "static_procedure", cell_functor(machine, name, arity));
    fail_load(machine, write_term(machine.heap, machine.operators, formal, {}, {}));
    return false;
  }
  Predicate* predicate = predicate_in(machine, home, name, arity);
  if (predicate != nullptr && !predicate->dynamic && !predicate->clauses.empty()) {
    fail_load(machine, "静的な述語は動的にできない");
    return false;
  }
  remember_pred(machine, snapshots, name, arity);
  if (predicate == nullptr) {
    Predicate created;
    created.home = home;
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
  // Module:PI は、モジュールを剥がしてから述語指示子として読む。
  std::uint32_t home = machine.load_home;
  std::vector<std::pair<std::uint32_t, std::uint16_t>> indicators;
  if (!directive_pi_home(machine, arg, home) || !collect_indicators(machine, arg, indicators)) {
    fail_load(machine, "述語指示子ではありません");
    return -1;
  }
  for (const auto& indicator : indicators) note_multifile(machine, home, indicator.first, indicator.second);
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
  std::uint32_t home = machine.load_home;
  const bool peeled = directive_pi_home(machine, arg, home);
  std::vector<std::pair<std::uint32_t, std::uint16_t>> indicators;
  if (!peeled || !collect_indicators(machine, arg, indicators)) {
    fail_load(machine, "述語指示子ではありません");
    return -1;
  }
  for (const auto& indicator : indicators) {
    if (!mark_dynamic(machine, snapshots, home, indicator.first, indicator.second)) return -1;
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

bool run_inits(Impl& machine, const std::vector<Cell>& goals) {
  for (const Cell goal : goals) {
    if (!run_init_goal(machine, goal)) return false;
  }
  return true;
}

LoadStatus finish_load(Impl& machine, std::size_t heap_base, const std::vector<Cell>& inits) {
  if (!run_inits(machine, inits)) {
    machine.heap.truncate(heap_base);
    // 節は残す。親の reject が、この例外で読んだ節まで消さないようにする。
    machine.load_kept = true;
    return LoadStatus{false, machine.diagnostic};
  }
  machine.heap.truncate(heap_base);
  machine.diagnostic.clear();
  // 成功した読み込みの印を残すと、親の構文エラーが節を戻さなくなる。
  machine.load_kept = false;
  return LoadStatus{true, {}};
}

int consume_directive(Impl& machine, Reader& reader, Cell term, std::vector<Snapshot>& snapshots,
                      std::vector<Apart>& apart, std::vector<Cell>& inits) {
  if (!is_directive(machine.heap, term)) return 0;
  const int named = apply_module_directive(machine, term);
  if (named != 0) return named;
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
  const int phase4 = apply_module_load_directive(machine, term);
  if (phase4 != 0) return phase4;
  if (!apply_op(machine, machine.heap.deref(term))) {
    if (machine.diagnostic.empty()) {
      const auto text = write_term(machine.heap, machine.operators, term, {}, {});
      fail_load(machine, "この指示はフェーズ2では使えない: " + text);
    }
    return -1;
  }
  return 1;
}

struct LoadScan {
  std::vector<Snapshot>& snapshots;
  std::vector<Apart>& apart;
  std::vector<Cell>& inits;
  std::size_t& last;
  std::vector<std::string>& opening;
};

struct CycleGuard {
  std::vector<std::string>* opening = nullptr;
  ~CycleGuard() {
    if (opening != nullptr && !opening->empty()) opening->pop_back();
  }
};

bool file_argument(Impl& machine, Cell directive, std::string_view functor, Cell& arg) {
  // 事実 p(include(f)) は指示ではない。関手が :- か ?- のときだけ開く。
  if (!is_directive(machine.heap, directive)) return false;
  directive = machine.heap.deref(directive);
  if (directive.tag() != Tag::Str || machine.heap.functor_arity(directive) != 1) return false;
  const Cell body = machine.heap.deref(machine.heap.arg(directive, 0));
  if (body.tag() != Tag::Str || machine.heap.functor_arity(body) != 1) return false;
  if (machine.atoms.name(machine.heap.functor_atom(body)) != functor) return false;
  arg = machine.heap.arg(body, 0);
  return true;
}

bool file_ready(const Impl& machine, const LoadScan& scan, const std::filesystem::path& file) {
  const auto key = canonical_key(file).generic_string();
  if (std::find(scan.opening.begin(), scan.opening.end(), key) != scan.opening.end()) return true;
  return std::find(machine.prepared.begin(), machine.prepared.end(), key) != machine.prepared.end();
}

bool note_syntax(Impl& machine, std::string_view origin, bool nested, const SyntaxFailure& failure) {
  const auto reason = reason_text(failure.reason);
  if (!nested) {
    machine.syntax_diag = true;
    machine.diag_line = failure.line;
    machine.diag_column = failure.column;
    machine.diagnostic = reason;
    return false;
  }
  // 親の consult は自分のパスを付ける。入れ子の位置は、ここにファイル名を残す。
  machine.syntax_diag = false;
  machine.diagnostic = std::string{origin} + ":" + std::to_string(failure.line) + ":" +
                       std::to_string(failure.column) + ": " + reason;
  return false;
}

bool read_terms(Impl& machine, std::string_view source, std::string_view directory, std::string_view origin,
                bool nested, LoadScan& scan);

int open_prolog_file(Impl& machine, const std::filesystem::path& file, LoadScan& scan) {
  std::ifstream input{file, std::ios::binary};
  if (!input) {
    fail_load(machine, file.string() + ": ファイルを開けません");
    return -1;
  }
  std::ostringstream buffer;
  buffer << input.rdbuf();
  // 同じ走査に足す。別の load にすると、節の置き換えと初期化の区切りが変わる。
  if (!read_terms(machine, buffer.str(), file.parent_path().string(), file.string(), true, scan)) return -1;
  return 1;
}

int apply_file(Impl& machine, Cell term, std::string_view directory, LoadScan& scan, std::string_view functor,
               bool once) {
  Cell arg;
  if (!file_argument(machine, term, functor, arg)) return 0;
  arg = machine.heap.deref(arg);
  if (machine.heap.unbound(arg)) {
    fail_load(machine, write_term(machine.heap, machine.operators, formal_instantiation(machine), {}, {}));
    return -1;
  }
  if (arg.tag() != Tag::Atom) {
    const Cell formal = formal_type(machine, "atom", arg);
    fail_load(machine, write_term(machine.heap, machine.operators, formal, {}, {}));
    return -1;
  }
  const auto name = machine.atoms.name(static_cast<std::uint32_t>(arg.raw() >> 3));
  const auto file = prolog_source_path(resolve_include(directory, name));
  // include は毎回足す。ensure_loaded は、準備済みのパスを節も初期化も足さない。
  if (once && file_ready(machine, scan, file)) return 1;
  return open_prolog_file(machine, file, scan);
}

bool read_terms(Impl& machine, std::string_view source, std::string_view directory, std::string_view origin,
                bool nested, LoadScan& scan) {
  CycleGuard guard;
  if (!origin.empty()) {
    // 開き済みのファイルをもう一度開くと、読み込みが終わらない。
    const auto key = canonical_key(std::filesystem::path{std::string{origin}}).generic_string();
    if (std::find(scan.opening.begin(), scan.opening.end(), key) != scan.opening.end()) {
      fail_load(machine, std::string{origin} + ": ファイルが循環しています");
      return false;
    }
    scan.opening.push_back(key);
    guard.opening = &scan.opening;
    // 開き始めたパスは準備済みにする。失敗した読み込みは reject_load が戻す。
    if (std::find(machine.prepared.begin(), machine.prepared.end(), key) == machine.prepared.end()) {
      machine.prepared.push_back(key);
    }
  }
  Reader reader(source, machine.heap, machine.operators, current_double_quotes(machine), &machine.char_conv);
  while (true) {
    const ReadOutcome outcome = reader.next();
    if (outcome.failure) return note_syntax(machine, origin, nested, *outcome.failure);
    if (!outcome.term) return true;
    Cell term = outcome.term->term;
    ++machine.load_terms;
    if (is_rule_arrow(machine.heap, term)) {
      fail_load(machine, "この指示はフェーズ2では使えない: " +
                             write_term(machine.heap, machine.operators, term, {}, {}));
      return false;
    }
    const int included = apply_file(machine, term, directory, scan, "include", false);
    if (included < 0) return false;
    if (included > 0) continue;
    const int ensured = apply_file(machine, term, directory, scan, "ensure_loaded", true);
    if (ensured < 0) return false;
    if (ensured > 0) continue;
    // [File] は別の読み込みで既存の節を置き換える。親の写しに無いと、親の失敗で戻せない。
    if (is_directive(machine.heap, term)) {
      const Cell body = machine.heap.deref(machine.heap.arg(term, 0));
      if (body.tag() == Tag::Lis || (body.tag() == Tag::Atom &&
                                     machine.atoms.name(static_cast<std::uint32_t>(body.raw() >> 3)) == "[]")) {
        for (const Predicate& predicate : machine.predicates) {
          remember_pred(machine, scan.snapshots, predicate.name, predicate.arity);
        }
      }
    }
    const int listed = apply_file_list(machine, term, directory, scan.opening);
    if (listed < 0) return false;
    if (listed > 0) continue;
    const int directed = consume_directive(machine, reader, term, scan.snapshots, scan.apart, scan.inits);
    if (directed < 0) return false;
    if (directed > 0) continue;
    std::uint32_t clause_home = machine.load_home;
    if (!retarget_multifile_clause(machine, term, clause_home)) return false;
    Clause clause;
    if (!compile_clause(machine.heap, term, clause)) {
      fail_load(machine, "この指示はフェーズ2では使えない: " +
                             write_term(machine.heap, machine.operators, term, {}, {}));
      return false;
    }
    const auto saved_home = machine.load_home;
    machine.load_home = clause_home;
    remember(machine, scan.snapshots, clause);
    const bool added = add_clause(machine, std::move(clause), scan.last, scan.apart);
    machine.load_home = saved_home;
    if (!added) return false;
  }
}

}  // namespace

LoadStatus load_source(Impl& machine, std::string_view source, std::string_view directory,
                       std::string_view origin, std::vector<std::string>* shared_opening) {
  // 実行中の consult は、クエリの変数と継続枠より上だけを捨てる。
  const auto heap_base = machine.heap.size();
  LoadHome home_guard{machine};
  // 入れ子が終わったら LoadHome が戻す。モジュールの出所はこのパスのまま残す。
  machine.load_origin = origin.empty()
                            ? std::string{}
                            : canonical_key(std::filesystem::path{std::string{origin}}).generic_string();
  machine.diagnostic.clear();
  machine.syntax_diag = false;
  machine.load_kept = false;
  machine.load_perm = false;
  for (Predicate& predicate : machine.predicates) {
    predicate.seen = false;
    predicate.dynamic_declared = false;
  }
  const auto saved_ops = machine.operators;
  const SavedFlags saved_flags = save_flags(machine);
  const auto saved_multi = machine.multifile;
  const auto saved_prepared = machine.prepared;
  std::vector<Snapshot> snapshots;
  // 次の読み込みには渡さない。宣言の無い述語は、これまでどおり置き換える。
  std::vector<Apart> apart;
  // テキストを準備し終えてから、ソースの順に実行する。
  std::vector<Cell> inits;
  const auto previous_size = machine.predicates.size();
  std::size_t last = static_cast<std::size_t>(-1);
  std::vector<std::string> local_opening;
  std::vector<std::string>& opening = shared_opening == nullptr ? local_opening : *shared_opening;
  LoadScan scan{snapshots, apart, inits, last, opening};
  if (!read_terms(machine, source, directory, origin, false, scan)) {
    if (machine.load_kept) {
      home_guard.keep();
      machine.heap.truncate(heap_base);
      return LoadStatus{false, machine.diagnostic};
    }
    return reject_load(machine, saved_ops, saved_flags, snapshots, previous_size, heap_base, saved_multi,
                       saved_prepared);
  }
  const LoadStatus done = finish_load(machine, heap_base, inits);
  if (done.ok || machine.load_kept) home_guard.keep();
  // モジュールの :- op は、続きの項を読むあいだだけ共有の表に載せる。
  if (done.ok && machine.load_home != machine.user_mod) machine.operators = saved_ops;
  return done;
}

LoadStatus Machine::load_program(std::string_view source) { return load_source(*impl_, source); }

}
