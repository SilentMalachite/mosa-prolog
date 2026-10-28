#include "load_internal.hpp"

#include "mosa/writer.hpp"
#include "mosa/path_text.hpp"

#include <array>
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

// M:N/A は M の述語を指す。M の誤りは節操作と同じ Formal を formal に入れる。
bool one_indicator(Impl& machine, Cell cell, PredKey& key, std::optional<Cell>& formal) {
  key.module = machine.load_module;
  formal = strip_module(machine, cell, key.module);
  if (formal) return false;
  if (cell.tag() != Tag::Str || machine.heap.functor_arity(cell) != 2) return false;
  if (machine.atoms.name(machine.heap.functor_atom(cell)) != "/") return false;
  const Cell left = machine.heap.deref(machine.heap.arg(cell, 0));
  const Cell right = machine.heap.deref(machine.heap.arg(cell, 1));
  if (left.tag() != Tag::Atom) return false;
  if (right.tag() != Tag::Int && right.tag() != Tag::IntBox) return false;
  const auto value = machine.heap.integer_value(right);
  if (value < 0 || value > 1024) return false;
  key.name = static_cast<std::uint32_t>(left.raw() >> 3);
  key.arity = static_cast<std::uint16_t>(value);
  return true;
}

bool collect_indicators(Impl& machine, Cell cell, std::vector<PredKey>& out, std::optional<Cell>& formal) {
  cell = machine.heap.deref(cell);
  PredKey key;
  if (one_indicator(machine, cell, key, formal)) {
    out.push_back(key);
    return true;
  }
  if (formal) return false;
  const auto nil = machine.atoms.intern("[]");
  if (cell.tag() == Tag::Atom && static_cast<std::uint32_t>(cell.raw() >> 3) == nil) return true;
  while (cell.tag() == Tag::Lis) {
    if (!one_indicator(machine, machine.heap.list_head(cell), key, formal)) return false;
    out.push_back(key);
    cell = machine.heap.deref(machine.heap.list_tail(cell));
  }
  return cell.tag() == Tag::Atom && static_cast<std::uint32_t>(cell.raw() >> 3) == nil;
}

bool read_declared(Impl& machine, Cell arg, std::vector<PredKey>& indicators) {
  std::optional<Cell> formal;
  if (collect_indicators(machine, arg, indicators, formal)) return true;
  fail_load(machine, formal ? write_term(machine.heap, machine.operators, *formal, {}, {}) : "述語指示子ではありません");
  return false;
}

bool mark_dynamic(Impl& machine, std::vector<Snapshot>& snapshots, PredKey key) {
  const auto name = key.name;
  const auto arity = key.arity;
  if (is_builtin_procedure(machine, name, arity)) {
    const Cell formal = formal_permission(machine, "modify", "static_procedure", cell_functor(machine, name, arity));
    fail_load(machine, write_term(machine.heap, machine.operators, formal, {}, {}));
    return false;
  }
  Predicate* predicate = find_pred(machine, key);
  // 宣言で作る述語も局所の述語である。取り込みとぶつかる順序によらず同じエラーにする。
  if (predicate == nullptr) {
    if (const auto formal = check_local(machine, key.module, name, arity)) {
      fail_load(machine, write_term(machine.heap, machine.operators, *formal, {}, {}));
      return false;
    }
  }
  if (predicate != nullptr && !predicate->dynamic && !predicate->clauses.empty()) {
    fail_load(machine, "静的な述語は動的にできない");
    return false;
  }
  // 失敗した読み込みは、宣言の前の動的フラグへ戻す。
  remember_pred(machine, snapshots, key);
  if (predicate == nullptr) {
    Predicate created;
    created.module = key.module;
    created.name = name;
    created.arity = arity;
    created.dynamic = true;
    created.dynamic_declared = true;
    machine.predicates.push_back(std::move(created));
    adopt_meta(machine, machine.predicates.back());
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
  std::vector<PredKey> indicators;
  if (!read_declared(machine, arg, indicators)) return -1;
  for (const PredKey& indicator : indicators) note_multifile(machine, indicator);
  return 1;
}

int apply_discontiguous_directive(Impl& machine, Cell directive, std::vector<Apart>& apart) {
  Cell arg;
  if (!discontiguous_argument(machine, directive, arg)) return 0;
  std::vector<PredKey> indicators;
  if (!read_declared(machine, arg, indicators)) return -1;
  for (const PredKey& indicator : indicators) {
    if (allows_gap(apart, indicator)) continue;
    apart.push_back(Apart{indicator.module, indicator.name, indicator.arity});
  }
  return 1;
}

int apply_dynamic_directive(Impl& machine, Cell directive, std::vector<Snapshot>& snapshots) {
  Cell arg;
  if (!dynamic_argument(machine, directive, arg)) return 0;
  std::vector<PredKey> indicators;
  if (!read_declared(machine, arg, indicators)) return -1;
  for (const PredKey& indicator : indicators) {
    if (!mark_dynamic(machine, snapshots, indicator)) return -1;
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
  // モジュールのファイルの初期化は、そのモジュールの文脈で動かす。
  if (machine.load_module != machine.user_module) {
    const std::array<Cell, 2> parts{machine.heap.atom(machine.load_module), arg};
    arg = machine.heap.structure(machine.atoms.intern(":"), parts);
  }
  goals.push_back(arg);
  return 1;
}

// 関手 name/arity の指令なら、その本体を返す。
std::optional<Cell> directive_body(Impl& machine, Cell directive, std::string_view name, std::uint16_t arity) {
  directive = machine.heap.deref(directive);
  if (directive.tag() != Tag::Str || machine.heap.functor_arity(directive) != 1) return std::nullopt;
  const Cell body = machine.heap.deref(machine.heap.arg(directive, 0));
  if (body.tag() != Tag::Str || machine.heap.functor_arity(body) != arity) return std::nullopt;
  if (machine.atoms.name(machine.heap.functor_atom(body)) != name) return std::nullopt;
  return body;
}

// meta_predicate A, B と meta_predicate [A, B] の両方を受ける。
void collect_heads(Impl& machine, Cell cell, std::vector<Cell>& heads) {
  cell = machine.heap.deref(cell);
  if (cell.tag() == Tag::Str && machine.heap.functor_arity(cell) == 2 &&
      machine.atoms.name(machine.heap.functor_atom(cell)) == ",") {
    collect_heads(machine, machine.heap.arg(cell, 0), heads);
    collect_heads(machine, machine.heap.arg(cell, 1), heads);
    return;
  }
  const WalkedList walked = walk_list(machine, cell);
  if (cell.tag() == Tag::Lis && walked.shape == ListShape::Proper) {
    for (const Cell item : walked.items) collect_heads(machine, item, heads);
    return;
  }
  heads.push_back(cell);
}

bool fail_with(Impl& machine, Cell formal) {
  fail_load(machine, write_term(machine.heap, machine.operators, formal, {}, {}));
  return false;
}

// 手続きがあればその述語へ、無ければモジュールの保留の表へ置く。節を足すとき adopt_meta が移す。
bool declare_meta(Impl& machine, std::vector<Snapshot>& snapshots, Cell head) {
  PredKey key{machine.load_module, 0, 0};
  std::vector<MetaArg> args;
  auto formal = strip_module(machine, head, key.module);
  if (!formal) formal = read_meta_spec(machine, head, args);
  if (formal) return fail_with(machine, *formal);
  key.name = head.tag() == Tag::Atom ? static_cast<std::uint32_t>(head.raw() >> 3) : machine.heap.functor_atom(head);
  key.arity = static_cast<std::uint16_t>(args.size());
  if (is_builtin_procedure(machine, key.name, key.arity)) {
    return fail_with(machine, formal_permission(machine, "modify", "static_procedure",
                                                cell_functor(machine, key.name, key.arity)));
  }
  Predicate* predicate = find_pred(machine, key);
  if (predicate == nullptr) {
    if (const auto conflict = check_local(machine, key.module, key.name, key.arity)) return fail_with(machine, *conflict);
    // 宣言は手続きを作らない。読み込みの失敗はモジュール表ごと戻すので、保留の表も戻る。
    Module* module = find_module(machine, key.module);
    if (module == nullptr) return true;
    std::erase_if(module->pending_meta, [&](const PendingMeta& item) {
      return item.name == key.name && item.arity == key.arity;
    });
    module->pending_meta.push_back(PendingMeta{key.name, key.arity, std::move(args)});
    return true;
  }
  remember_pred(machine, snapshots, key);
  predicate->meta = std::move(args);
  predicate->meta_declared = true;
  return true;
}

int apply_meta_directive(Impl& machine, Cell directive, std::vector<Snapshot>& snapshots) {
  const auto body = directive_body(machine, directive, "meta_predicate", 1);
  if (!body) return 0;
  std::vector<Cell> heads;
  collect_heads(machine, machine.heap.arg(*body, 0), heads);
  for (const Cell head : heads) {
    if (!declare_meta(machine, snapshots, head)) return -1;
  }
  return 1;
}

int apply_module_directive(Impl& machine, Cell directive, bool first, std::string_view origin) {
  std::optional<Cell> failure;
  if (const auto body = directive_body(machine, directive, "module", 2)) {
    // ファイルとモジュールの対応は正規化したパスで持つ。use_module がパスの書き方によらず見つける。
    const auto key = origin.empty() ? std::string{}
                                    : utf8_from_path(canonical_key(path_from_utf8(origin)));
    failure = declare_module(machine, machine.heap.arg(*body, 0), machine.heap.arg(*body, 1), first, key);
  } else if (const auto listed = directive_body(machine, directive, "export", 1)) {
    failure = add_exports(machine, machine.heap.arg(*listed, 0));
  } else {
    return 0;
  }
  if (!failure) return 1;
  fail_load(machine, write_term(machine.heap, machine.operators, *failure, {}, {}));
  return -1;
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
  launch(machine, compiled.term, kNoFrame, 0, compiled.env, machine.user_module);
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
                      std::vector<Apart>& apart, std::vector<Cell>& inits, bool first, std::string_view origin) {
  if (!is_directive(machine.heap, term)) return 0;
  const int module_directive = apply_module_directive(machine, term, first, origin);
  if (module_directive != 0) return module_directive;
  const int flag_directive = apply_flag_directive(machine, term);
  if (flag_directive < 0) return -1;
  if (flag_directive > 0) {
    reader.set_quotes(current_double_quotes(machine));
    return 1;
  }
  const int dynamic_directive = apply_dynamic_directive(machine, term, snapshots);
  if (dynamic_directive < 0) return -1;
  if (dynamic_directive > 0) return 1;
  const int meta_directive = apply_meta_directive(machine, term, snapshots);
  if (meta_directive != 0) return meta_directive;
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
