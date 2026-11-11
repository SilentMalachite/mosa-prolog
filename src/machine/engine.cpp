#include "internal.hpp"

#include "mosa/goal_word.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mosa {
namespace {

bool has_more(const Impl& machine) {
  for (const Entry& entry : machine.stack) {
    if (entry.pick == Pick::Cleanup || entry.pick == Pick::Catch) continue;
    if (entry.pick == Pick::Clauses || entry.pick == Pick::Repeat || entry.pick == Pick::AtomGen) return true;
    // retract と clause/2 のカーソルは、次に合う節を先に試して見つけたときだけ積む（ADR 0015 とその改訂）。
    if (entry.pick == Pick::Retract || entry.pick == Pick::ClauseScan) {
      if (entry.cursor != kNoClause) return true;
      continue;
    }
    if ((entry.pick == Pick::PredGen || entry.pick == Pick::StreamGen) && entry.next < entry.candidates.size()) {
      return true;
    }
    // 生成コードの *-> の else も、条件が成功していれば解ではない（ElseGoal と同じ）。
    if (entry.pick == Pick::Native && entry.next < entry.candidates.size() && !(entry.soft_id != 0 && entry.cond_ok)) {
      return true;
    }
    if (entry.pick == Pick::ElseGoal && !(entry.soft_id != 0 && entry.cond_ok)) return true;
  }
  return false;
}

// launch は extras を push_back で伸ばし、伸びると中への参照が無効になる。ゴールと env は共有のポインタなので、
// 先に局所へ写しておく（ADR 0011 の改訂 3）。
void launch_control(Impl& machine, std::size_t index) {
  const Extra& extra = machine.extras[index];
  if (extra.ctrl == Ctrl::Commit) trim_stack(machine, extra.trim_to);
  // 条件の選択点が残らなければ else に戻る道は無いので、印を付けずに外す（ADR 0012）。
  if (extra.ctrl == Ctrl::SoftCommit && extra.soft_id != 0 && extra.trim_to < machine.stack.size()) {
    Entry& entry = machine.stack[extra.trim_to];
    if (entry.soft_id == extra.soft_id && extra.trim_to + 1 == machine.stack.size()) trim_stack(machine, extra.trim_to);
    else if (entry.soft_id == extra.soft_id) entry.cond_ok = true;
  }
  GoalRef goal = extra.ctrl == Ctrl::Interpret ? extra.goal : extra.then;
  const EnvRef env = extra.env;
  // 確定が外した後始末を then の頭（launch はこの歩みで動かす）より先にするため、枠にして回す（ADR 0016）。
  if (!machine.pending_cleanups.empty()) {
    machine.ip = embody(machine, goal, extra.cont, extra.cut, env, extra.module);
    return;
  }
  launch(machine, std::move(goal), extra.cont, extra.cut, env, extra.module);
}

void step_control(Impl& machine) {
  const auto index = static_cast<std::size_t>(machine.heap.word_at(machine.ip + 1));
  const Ctrl ctrl = machine.extras[index].ctrl;
  if (ctrl == Ctrl::Interpret || ctrl == Ctrl::Commit || ctrl == Ctrl::SoftCommit) {
    launch_control(machine, index);
    return;
  }
  const std::size_t trim_to = machine.extras[index].trim_to;
  const std::uint64_t cont = machine.extras[index].cont;
  if (ctrl == Ctrl::NegDone) {
    trim_stack(machine, trim_to);
    machine.failed = true;
    return;
  }
  if (ctrl == Ctrl::CatchExit) {
    disarm_catch(machine, trim_to);
    machine.ip = cont;
    return;
  }
  if (ctrl == Ctrl::Collect) {
    collect_step(machine, trim_to);
    return;
  }
  if (ctrl == Ctrl::CleanupExit) {
    exit_cleanup(machine, trim_to, cont);
    return;
  }
  trim_stack(machine, trim_to);
  machine.ip = cont;
}

void reject_callable(Impl& machine, Cell culprit) {
  const auto name = machine.atoms.intern("call");
  raise_formal(machine, formal_type(machine, "callable", culprit), name, 1);
}

// 左の子が自分に戻る巡回するゴール（X = (X, true)）は launch_var と launch を交互に呼び続ける。C++ の再帰は
// この段数までにし、より深くは call/1 の枠を run_loop に渡す（ADR 0011 と同じ考え、レビュー M13）。枠は
// ヒープに載るので、無限の左再帰はヒープの上限で止まる。
constexpr int kVarNest = 64;

struct VarNest {
  Impl& machine;
  explicit VarNest(Impl& owner) : machine(owner) { ++machine.var_depth; }
  ~VarNest() { --machine.var_depth; }
  VarNest(const VarNest&) = delete;
  VarNest& operator=(const VarNest&) = delete;
};

void defer_var(Impl& machine, const IrTerm& goal, std::uint64_t cont, const EnvRef& env, std::uint32_t module) {
  IrTerm call;
  call.tag = IrTag::Call;
  call.atom = machine.atoms.intern("call");
  call.arity = 1;
  call.args.push_back(goal);
  machine.ip = embody(machine, make_run_goal(machine.heap, std::move(call)), cont, machine.stack.size(), env, module);
}

void launch_var(Impl& machine, const IrTerm& goal, std::uint64_t cont, const EnvRef& env, std::uint32_t module) {
  if (machine.var_depth >= kVarNest) {
    defer_var(machine, goal, cont, env, module);
    return;
  }
  // ゴールの根は共有の項にしない（ADR 0022）ので、変数のゴールはいつも env のスロット。
  const Cell cell = machine.heap.deref(env->cells.at(goal.slot));
  if (machine.heap.unbound(cell)) {
    raise_formal(machine, formal_instantiation(machine), machine.atoms.intern("call"), 1);
    return;
  }
  if (cell.tag() != Tag::Atom && cell.tag() != Tag::Str && cell.tag() != Tag::Lis) {
    reject_callable(machine, cell);
    return;
  }
  const Compiled compiled = compile_goal(machine.heap, cell);
  // 変数ゴールは call(G) と同じで、中のカットは局所である（7.8.3）。
  if (!body_callable(*compiled.term)) {
    reject_callable(machine, cell);
    return;
  }
  const VarNest nest(machine);
  launch(machine, compiled.term, cont, machine.stack.size(), compiled.env, module);
}

void step(Impl& machine) {
  machine.eval_abort = false;
  // 生成関数どうしの呼び出し（ADR 0009）。枠は普通のゴール枠のまま、run_builtin と述語の引き当てを飛ばす。
  if (machine.native_next != nullptr) {
    const rt::NativeProc proc = machine.native_next;
    machine.native_next = nullptr;
    call_native(machine, proc, machine.ip);
    return;
  }
  if (machine.heap.word_at(machine.ip) == 0) {
    // 第 63 ビットが立った語 1 は extras の添字ではない。step_control より先に見る。
    if ((machine.heap.word_at(machine.ip + 1) & rt::kNativeBit) != 0) {
      resume_native(machine);
      return;
    }
    step_control(machine);
    return;
  }
  const Goal goal = read_goal(machine, machine.ip);
  if (run_builtin(machine, goal)) return;
  enter_user(machine, goal);
}

// call/N と halt/N は表に置かず、引数の数の範囲で引く。is_builtin_procedure と builtin_procedures が同じ値を使う。
constexpr std::uint16_t kCallArityMax = 8;
constexpr std::uint16_t kHaltArityMax = 1;

// call/1..8、halt/0..1、findall、open、close、at_end_of_stream、get_char、get_code、peek_char、
// peek_code、put_char、put_code、nl、get_byte、peek_byte、put_byte、read_term、read、
// write_term、write、writeq、write_canonical、print はアリティが二つ以上。
// 制御構文は goal と launch が先に取る。
constexpr auto kBuiltins = std::to_array<BuiltinPi>({
    {"=", 2},
    {"\\=", 2},
    {"true", 0},
    {"fail", 0},
    {"false", 0},
    {"consult", 1},
    {"!", 0},
    {"var", 1},
    {"nonvar", 1},
    {"atom", 1},
    {"integer", 1},
    {"float", 1},
    {"atomic", 1},
    {"compound", 1},
    {"number", 1},
    {"callable", 1},
    {"is_list", 1},
    {"ground", 1},
    {"acyclic_term", 1},
    {"unify_with_occurs_check", 2},
    {"@<", 2},
    {"@=<", 2},
    {"==", 2},
    {"@>=", 2},
    {"@>", 2},
    {"\\==", 2},
    {"compare", 3},
    {"functor", 3},
    {"arg", 3},
    {"=..", 2},
    {"copy_term", 2},
    {"term_variables", 2},
    {"numbervars", 3},
    {"\\+", 1},
    {"once", 1},
    {"repeat", 0},
    {"is", 2},
    {"=:=", 2},
    {"=\\=", 2},
    {"<", 2},
    {"=<", 2},
    {">", 2},
    {">=", 2},
    {"atom_length", 2},
    {"atom_concat", 3},
    {"sub_atom", 5},
    {"atom_chars", 2},
    {"atom_codes", 2},
    {"char_code", 2},
    {"number_chars", 2},
    {"number_codes", 2},
    {"atom_number", 2},
    {"current_prolog_flag", 2},
    {"set_prolog_flag", 2},
    {"asserta", 1},
    {"assertz", 1},
    {"retract", 1},
    {"retractall", 1},
    {"abolish", 1},
    {"clause", 2},
    {"current_predicate", 1},
    {"findall", 3},
    {"findall", 4},
    {"open", 3},
    {"open", 4},
    {"open_string", 2},
    {"close", 1},
    {"close", 2},
    {"flush_output", 0},
    {"flush_output", 1},
    {"current_input", 1},
    {"current_output", 1},
    {"set_input", 1},
    {"set_output", 1},
    {"stream_property", 2},
    {"at_end_of_stream", 0},
    {"at_end_of_stream", 1},
    {"set_stream_position", 2},
    {"get_char", 1},
    {"get_char", 2},
    {"get_code", 1},
    {"get_code", 2},
    {"peek_char", 1},
    {"peek_char", 2},
    {"peek_code", 1},
    {"peek_code", 2},
    {"put_char", 1},
    {"put_char", 2},
    {"put_code", 1},
    {"put_code", 2},
    {"nl", 0},
    {"nl", 1},
    {"get_byte", 1},
    {"get_byte", 2},
    {"peek_byte", 1},
    {"peek_byte", 2},
    {"put_byte", 1},
    {"put_byte", 2},
    {"tab", 1},
    {"tab", 2},
    {"read_term", 2},
    {"read_term", 3},
    {"read", 1},
    {"read", 2},
    {"write_term", 2},
    {"write_term", 3},
    {"write", 1},
    {"write", 2},
    {"writeq", 1},
    {"writeq", 2},
    {"write_canonical", 1},
    {"write_canonical", 2},
    {"print", 1},
    {"print", 2},
    {"term_to_atom", 2},
    {"nb_setval", 2},
    {"nb_getval", 2},
    {"setup_call_cleanup", 3},
    {"$call_cleanup", 2},
    {"with_output_to", 2},
    {"op", 3},
    {"current_op", 3},
    {"char_conversion", 2},
    {"current_char_conversion", 2},
    {"bagof", 3},
    {"setof", 3},
    {"catch", 3},
    {"throw", 1},
    {",", 2},
    {";", 2},
    {"->", 2},
    {"*->", 2},
    {":", 2},
    {"current_module", 1},
    {"forall", 2},
    {"if", 3},
    {"phrase", 2},
    {"phrase", 3},
    {"subsumes_term", 2},
    {"predicate_property", 2},
    {"length", 2},
    {"sort", 2},
    {"sort", 4},
    {"keysort", 2},
    {"$format_float", 4},
    {"$dcg_translate", 2},
    {"$upcase_atom", 2},
    {"$char_width", 2},
    {"garbage_collect", 0},
    {"statistics", 2},
});

// 節の ^^ は判定器を通らない。ここで閉じないと put_byte/1 がバイナリへ書いて成功する。
bool run_suite_super(Impl& machine, const Goal& goal) {
  if (machine.suite_super == nullptr || machine.suite_self == nullptr || machine.suite_io == nullptr) return false;
  if (machine.atoms.name(goal.name) != "^^" || goal.arity != 1) return false;
  const SuiteSuper code = machine.suite_super(*machine.suite_self, *machine.suite_io, goal.arg(machine, 0));
  if (code == SuiteSuper::Skip) return false;
  if (code == SuiteSuper::Ok) machine.ip = goal.cont;
  else machine.failed = true;
  return true;
}

// この深さまでは C++ の再帰で辿る。より深い部分木は手で積んで辿る（ADR 0011 と改訂 2）。浅い項の確保と
// 辿る順は前の再帰と同じになる。
constexpr int kShallow = 64;

// 深さ kShallow に達した部分木は later に回し、呼び手が深さ 0 から辿り直す。偽になるのはどれか 1 つの葉が
// 偽のときだけなので、辿る順は答えを変えない。
bool leaves_at(const IrTerm& term, GoalLeafOk ok, int depth, std::vector<const IrTerm*>& later) {
  if (depth >= kShallow) {
    later.push_back(&term);
    return true;
  }
  switch (term.tag) {
    case IrTag::And:
    case IrTag::Or:
    case IrTag::IfThen:
    case IrTag::SoftIf:
      return term.args.size() >= 2 && leaves_at(term.args[0], ok, depth + 1, later) &&
             leaves_at(term.args[1], ok, depth + 1, later);
    // catch/3 は呼べる項であり、7.6.2 はその引数を変換しない。呼べない Goal と Recovery は Call の節点に
    // なり（compile.cpp の runs_late、レビュー M15）、catch が動くときに call/1 が拒む。
    case IrTag::Catch: return term.args.size() >= 3;
    case IrTag::Call: return term.args.empty() || leaves_at(term.args[0], ok, depth + 1, later);
    default: return ok(term.tag);
  }
}

}  // namespace

bool goal_leaves_all(const IrTerm& root, GoalLeafOk ok) {
  std::vector<const IrTerm*> later;
  if (!leaves_at(root, ok, 0, later)) return false;
  while (!later.empty()) {
    const IrTerm* next = later.back();
    later.pop_back();
    if (!leaves_at(*next, ok, 0, later)) return false;
  }
  return true;
}

bool body_callable(const IrTerm& term) {
  return goal_leaves_all(term, [](IrTag tag) { return tag != IrTag::Int && tag != IrTag::Float && tag != IrTag::Anon; });
}

Impl& engine_impl(Machine& machine) { return *machine.impl_; }

const Impl& engine_impl(const Machine& machine) { return *machine.impl_; }

Goal read_goal(const Impl& machine, std::uint64_t at) {
  Goal goal;
  goal.at = at;
  goal.name = static_cast<std::uint32_t>(machine.heap.word_at(at));
  const auto word = machine.heap.word_at(at + 1);
  goal.arity = goal_word_arity(word);
  goal.module = goal_word_module(word);
  goal.cut = static_cast<std::size_t>(machine.heap.word_at(at + 2));
  goal.cont = machine.heap.word_at(at + 3);
  return goal;
}

bool is_call_leaf(IrTag tag) { return tag == IrTag::Atom || tag == IrTag::Fun || tag == IrTag::Call; }

std::uint64_t embody_leaf(Impl& machine, const IrTerm& goal, std::uint64_t cont, std::size_t cut,
                          const std::vector<Cell>& cells, std::uint32_t module, const SharedTable* shared) {
  std::vector<Cell> args;
  if (goal.tag != IrTag::Atom) {
    args.reserve(goal.args.size());
    // 共有の項（ADR 0022）は、このゴールの引数の間で 1 度だけ組む。前の写しと同じくゴールごとに組み直すので、
    // ゴールが終われば組んだ項は残らない。
    if (shared == nullptr) {
      for (const IrTerm& arg : goal.args) args.push_back(materialize(machine, arg, cells));
    } else {
      SharedMemo memo{machine, *shared};
      for (const IrTerm& arg : goal.args) args.push_back(materialize(machine, arg, cells, memo));
    }
  }
  const auto at = machine.heap.allocate_words(kFrameHead + args.size());
  machine.heap.set_word(at, goal.atom);
  machine.heap.set_word(at + 1, pack_goal_word(static_cast<std::uint16_t>(args.size()), module));
  machine.heap.set_word(at + 2, cut);
  machine.heap.set_word(at + 3, cont);
  for (std::size_t index = 0; index < args.size(); ++index) {
    machine.heap.set_word(at + kFrameHead + index, args[index].raw());
  }
  return at;
}

std::uint64_t embody(Impl& machine, const GoalRef& goal, std::uint64_t cont, std::size_t cut, const EnvRef& env,
                     std::uint32_t module) {
  if (is_call_leaf(goal->tag)) return embody_leaf(machine, *goal, cont, cut, env->cells, module, env->shared.get());
  Extra extra;
  extra.ctrl = Ctrl::Interpret;
  extra.goal = goal;
  extra.env = env;
  extra.cont = cont;
  extra.cut = cut;
  extra.module = module;
  return push_extra(machine, std::move(extra));
}

void launch_list(Impl& machine, const IrTerm& goal, std::uint64_t cont, std::size_t cut, const EnvRef& env,
                 std::uint32_t module) {
  // リストは '.'/2 である。未定義なら existence_error であり、callable の型誤りではない。
  IrTerm call;
  call.tag = IrTag::Fun;
  call.atom = machine.atoms.intern(".");
  call.arity = 2;
  call.args = goal.args;
  machine.ip = embody(machine, make_run_goal(machine.heap, std::move(call)), cont, cut, env, module);
}

// 制御構造は枠を積んで最初の下位ゴールを返し、ここで次の周に回す。連言は右から継続を作るので、ヒープの並びは
// 再帰で降りたときと同じになる（ADR 0011 の改訂 3）。
void launch(Impl& machine, GoalRef goal, std::uint64_t cont, std::size_t cut, const EnvRef& env,
            std::uint32_t module) {
  while (true) {
    LaunchNext next;
    switch (goal->tag) {
      case IrTag::True: machine.ip = cont; return;
      case IrTag::Fail: machine.failed = true; return;
      case IrTag::Cut: trim_stack(machine, cut); machine.ip = cont; return;
      case IrTag::And:
        cont = embody(machine, sub_goal(goal, 1), cont, cut, env, module);
        goal = sub_goal(goal, 0);
        continue;
      case IrTag::Or: next = open_or(machine, goal, cont, cut, env, module); break;
      case IrTag::IfThen: next = open_if_then(machine, goal, cont, cut, env, module); break;
      case IrTag::SoftIf: next = open_soft(machine, goal, cont, cut, env, module); break;
      case IrTag::Catch: next = open_catch(machine, goal, cont, cut, env, module); break;
      case IrTag::Throw: arm_throw(machine, *goal, env->cells, env->shared.get()); return;
      case IrTag::Halt: arm_halt(machine, *goal, env->cells, env->shared.get()); return;
      case IrTag::Var: launch_var(machine, *goal, cont, env, module); return;
      case IrTag::Atom:
      case IrTag::Fun:
      case IrTag::Call: machine.ip = embody(machine, goal, cont, cut, env, module); return;
      case IrTag::List: launch_list(machine, *goal, cont, cut, env, module); return;
      case IrTag::Anon:
      case IrTag::Int:
      case IrTag::Float: reject_callable(machine, materialize(machine, *goal, env->cells)); return;
    }
    goal = std::move(next.goal);
    cont = next.cont;
    cut = next.cut;
  }
}

bool is_builtin_procedure(const Impl& machine, std::uint32_t name, std::uint16_t arity) {
  const auto text = machine.atoms.name(name);
  if (text == "call" && arity >= 1 && arity <= kCallArityMax) return true;
  if (text == "halt" && arity <= kHaltArityMax) return true;
  for (const BuiltinPi& item : kBuiltins) {
    if (item.arity == arity && item.name == text) return true;
  }
  return false;
}

std::vector<BuiltinPi> builtin_procedures() {
  std::vector<BuiltinPi> all(kBuiltins.begin(), kBuiltins.end());
  for (std::uint16_t arity = 1; arity <= kCallArityMax; ++arity) all.push_back(BuiltinPi{"call", arity});
  for (std::uint16_t arity = 0; arity <= kHaltArityMax; ++arity) all.push_back(BuiltinPi{"halt", arity});
  return all;
}

bool run_builtin(Impl& machine, const Goal& goal) {
  if (run_suite_super(machine, goal)) return true;
  const auto text = machine.atoms.name(goal.name);
  if (text == "=" && goal.arity == 2) {
    if (!do_unify(machine, goal.arg(machine, 0), goal.arg(machine, 1))) machine.failed = true;
    else machine.ip = goal.cont;
    return true;
  }
  if (text == "\\=" && goal.arity == 2) {
    const auto heap_mark = machine.heap.size();
    const auto trail_mark = machine.trail.slots.size();
    const auto extras_mark = machine.extras.size();
    if (do_unify(machine, goal.arg(machine, 0), goal.arg(machine, 1))) {
      restore_state(machine, heap_mark, trail_mark, extras_mark);
      machine.failed = true;
    } else {
      machine.ip = goal.cont;
    }
    return true;
  }
  if (text == "true" && goal.arity == 0) {
    machine.ip = goal.cont;
    return true;
  }
  if ((text == "fail" || text == "false") && goal.arity == 0) {
    machine.failed = true;
    return true;
  }
  if (text == "consult" && goal.arity == 1) return run_consult(machine, goal);
  if (text == "!" && goal.arity == 0) {
    trim_stack(machine, goal.cut);
    machine.ip = goal.cont;
    return true;
  }
  if (run_types(machine, goal) || run_order(machine, goal) || run_terms(machine, goal) ||
      run_control(machine, goal) || run_qualify(machine, goal) || run_arith(machine, goal) || run_atoms(machine, goal) ||
      run_flags(machine, goal) || run_statistics(machine, goal) || run_database(machine, goal) || run_module_prop(machine, goal) ||
      run_lists(machine, goal) || run_format_float(machine, goal) || run_case(machine, goal) || run_char_width(machine, goal) || run_solutions(machine, goal) ||
      run_streams(machine, goal) || run_char_io(machine, goal) || run_byte_io(machine, goal) ||
      run_write(machine, goal) || run_read(machine, goal) || run_operators(machine, goal) || run_term_text(machine, goal) ||
      run_global_vars(machine, goal) || run_cleanup(machine, goal)) {
    return true;
  }
  return false;
}

Stop run_loop(Impl& machine) {
  while (true) {
    if (machine.halt_now) {
      // 埋め込みでは halt の後も Machine が生きるので、終わりを待たずにここで書き出す（レビュー M5）。
      flush_open_files(machine);
      return Stop{StopKind::Halt, machine.halt_code, {}, {}, false};
    }
    // 歩みの途中のカットが外した後始末は、次の歩み（失敗や例外の伝わりも含む）の前に動かす（ADR 0016）。
    if (!machine.pending_cleanups.empty()) settle_cleanups(machine);
    if (machine.exception) return Stop{StopKind::Exception, 0, machine.ball, {}, false};
    if (machine.failed) {
      if (!backtrack(machine)) {
        // 戻した水位より上の枠は空いた。ip を残すと、問い合わせの後の load_program の指示（run_parked）がそれを外の
        // 枠として退避し、GC が空いた番地の語を枠と読んで、指示のゴール枠の引数を捨てる（レビュー M28）。
        machine.ip = kNoFrame;
        return Stop{StopKind::Failure, 0, {}, {}, false};
      }
      continue;
    }
    // 2 歩の間だけが GC の機会である（ADR 0010）。組込と生成関数の C++ の局所はここでは何も握っていない。
    // 呼び出しを避けるため、通常の「まだ早い」判定はここで済ませる（gc_trigger は stress 中だけ 0 なので比較 1 回で済む）。
    // ヒープの外の状態の語（ADR 0010 の改訂 3）もここで足す。trigger は閾値から持った語を引いた値である。
    // 答えを返す前にも通る。最後の歩みで上限を超えた状態（大きなゴールを訳した選択点など）を、答えと一緒に残さない
    // （Codex レビュー P2）。
    if (machine.heap.size() + machine.heap.state() >= machine.gc_trigger) gc_safe_point(machine);
    if (machine.ip == kNoFrame) {
      return Stop{StopKind::Success, 0, {}, machine.bindings, has_more(machine)};
    }
    step(machine);
  }
}

Machine::Machine(std::size_t heap_limit) : impl_(std::make_unique<Impl>(heap_limit)) {
  init_streams(*impl_);
}

Machine::~Machine() {
  // close を書かずに終わったプログラムの出力を残す（レビュー M5）。halt を通らない終わり方
  // （ゴールの成功・失敗・例外、ヒープの上限で巻き戻る途中）もここを通る。デストラクタから例外は出さない。
  try {
    flush_open_files(*impl_);
  } catch (...) {
  }
}

std::unique_ptr<Machine> Machine::create(std::size_t heap_limit) {
  return std::make_unique<Machine>(heap_limit);
}

AtomTable& Machine::atoms() { return impl_->atoms; }

Heap& Machine::heap() { return impl_->heap; }

const Heap& Machine::heap() const { return impl_->heap; }

OperatorTable& Machine::operators() { return impl_->operators; }

const OperatorTable& Machine::operators() const { return impl_->operators; }

std::size_t Machine::choice_depth() const { return impl_->stack.size(); }

std::string_view Machine::diagnostic() const { return impl_->diagnostic; }

void Machine::halt(int code) {
  impl_->halt_pending = true;
  impl_->halt_code = code;
  flush_open_files(*impl_);
}

// 問い合わせ 1 つ分の実行状態を捨てる。選択点はもう無いので戻さずに捨てる。
// 選択点が無ければ catch_log と scan_log の記録はどれも指す先が無い。
void drop_run(Impl& machine) {
  machine.stack.clear();
  machine.catch_log.clear();
  machine.scan_log.clear();
  machine.trail.slots.clear();
  machine.trail.mark = nullptr;
  machine.extras.clear();
  machine.heap.truncate(0);
  machine.failed = false;
  machine.exception = false;
  machine.eval_abort = false;
  machine.native_next = nullptr;
  machine.ip = kNoFrame;
  machine.bindings.clear();
  machine.pending_cleanups.clear();
}

}
