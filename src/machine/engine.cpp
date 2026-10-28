#include "internal.hpp"

#include "mosa/goal_word.hpp"
#include "mosa/reader.hpp"

#include <array>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mosa {
namespace {

std::uint64_t push_extra(Impl& machine, Extra extra) {
  machine.extras.push_back(std::move(extra));
  const auto at = machine.heap.allocate_words(kFrameHead);
  machine.heap.set_word(at, 0);
  machine.heap.set_word(at + 1, machine.extras.size() - 1);
  machine.heap.set_word(at + 2, 0);
  machine.heap.set_word(at + 3, kNoFrame);
  return at;
}

bool has_more(const Impl& machine) {
  for (const Entry& entry : machine.stack) {
    if (entry.pick == Pick::Cleanup || entry.pick == Pick::Catch) continue;
    if (entry.pick == Pick::Clauses || entry.pick == Pick::Repeat || entry.pick == Pick::AtomGen) return true;
    if ((entry.pick == Pick::Retract || entry.pick == Pick::ClauseScan || entry.pick == Pick::PredGen ||
         entry.pick == Pick::StreamGen || entry.pick == Pick::Native) &&
        entry.next < entry.candidates.size()) {
      return true;
    }
    if (entry.pick == Pick::ElseGoal && !(entry.soft_id != 0 && entry.cond_ok)) return true;
  }
  return false;
}

void step_control(Impl& machine) {
  const auto index = static_cast<std::size_t>(machine.heap.word_at(machine.ip + 1));
  const Extra extra = machine.extras[index];
  if (extra.ctrl == Ctrl::Interpret) {
    launch(machine, extra.goal, extra.cont, extra.cut, extra.env, extra.module);
    return;
  }
  if (extra.ctrl == Ctrl::Commit) {
    trim_stack(machine, extra.trim_to);
    launch(machine, extra.then, extra.cont, extra.cut, extra.env, extra.module);
    return;
  }
  if (extra.ctrl == Ctrl::SoftCommit) {
    if (extra.soft_id != 0) {
      for (Entry& entry : machine.stack) {
        if (entry.soft_id == extra.soft_id) entry.cond_ok = true;
      }
    }
    launch(machine, extra.then, extra.cont, extra.cut, extra.env, extra.module);
    return;
  }
  if (extra.ctrl == Ctrl::NegDone) {
    trim_stack(machine, extra.trim_to);
    machine.failed = true;
    return;
  }
  if (extra.ctrl == Ctrl::CatchExit) {
    const auto frame = extra.trim_to;
    if (frame < machine.stack.size() && machine.stack[frame].pick == Pick::Catch) {
      machine.stack[frame].armed = false;
    }
    machine.ip = extra.cont;
    return;
  }
  trim_stack(machine, extra.trim_to);
  machine.ip = extra.cont;
}

void reject_callable(Impl& machine, Cell culprit) {
  const auto name = machine.atoms.intern("call");
  raise_formal(machine, formal_type(machine, "callable", culprit), name, 1);
}

void launch_var(Impl& machine, const IrTerm& goal, std::uint64_t cont, std::size_t /*cut*/,
                const std::vector<Cell>& env, std::uint32_t module) {
  const Cell cell = machine.heap.deref(env.at(goal.slot));
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
  if (!body_callable(compiled.term)) {
    reject_callable(machine, cell);
    return;
  }
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

struct BuiltinPi {
  std::string_view name;
  std::uint16_t arity;
};

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
    {"predicate_property", 2},
    {"length", 2},
    {"sort", 2},
    {"keysort", 2},
    {"$format_float", 4},
    {"$upcase_atom", 2},
    {"$char_width", 2},
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

}  // namespace

bool body_callable(const IrTerm& term) {
  switch (term.tag) {
    case IrTag::Int:
    case IrTag::Float:
    case IrTag::Anon: return false;
    case IrTag::And:
    case IrTag::Or:
    case IrTag::IfThen:
    case IrTag::SoftIf:
      return term.args.size() >= 2 && body_callable(term.args[0]) && body_callable(term.args[1]);
    case IrTag::Catch:
      return term.args.size() >= 3 && body_callable(term.args[0]) && body_callable(term.args[2]);
    case IrTag::Call: return term.args.empty() || body_callable(term.args[0]);
    default: return true;
  }
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

Cell materialize(Impl& machine, const IrTerm& term, const std::vector<Cell>& env) {
  switch (term.tag) {
    case IrTag::Var: return env.at(term.slot);
    case IrTag::Anon: return machine.heap.fresh_ref();
    case IrTag::Atom: return machine.heap.atom(term.atom);
    case IrTag::Int: return machine.heap.integer(term.integer);
    case IrTag::Float: return machine.heap.floating(term.floating);
    case IrTag::List:
      return machine.heap.list(materialize(machine, term.args[0], env),
                               materialize(machine, term.args[1], env));
    case IrTag::True: return machine.heap.atom(machine.atoms.intern("true"));
    case IrTag::Fail: return machine.heap.atom(machine.atoms.intern("fail"));
    case IrTag::Cut: return machine.heap.atom(machine.atoms.intern("!"));
    case IrTag::And:
    case IrTag::Or:
    case IrTag::IfThen:
    case IrTag::SoftIf:
    case IrTag::Catch:
    case IrTag::Throw:
    case IrTag::Halt:
    case IrTag::Call:
    case IrTag::Fun: {
      const char* name = "!";
      if (term.tag == IrTag::And) name = ",";
      else if (term.tag == IrTag::Or) name = ";";
      else if (term.tag == IrTag::IfThen) name = "->";
      else if (term.tag == IrTag::SoftIf) name = "*->";
      else if (term.tag == IrTag::Catch) name = "catch";
      else if (term.tag == IrTag::Throw) name = "throw";
      else if (term.tag == IrTag::Halt) name = "halt";
      else if (term.tag == IrTag::Call || term.tag == IrTag::Fun) {
        std::vector<Cell> args;
        args.reserve(term.args.size());
        for (const IrTerm& arg : term.args) args.push_back(materialize(machine, arg, env));
        if (term.args.size() == 2 && machine.atoms.name(term.atom) == ".") {
          return machine.heap.list(args[0], args[1]);
        }
        return machine.heap.structure(term.atom, args);
      }
      std::vector<Cell> args;
      args.reserve(term.args.size());
      for (const IrTerm& arg : term.args) args.push_back(materialize(machine, arg, env));
      return machine.heap.structure(machine.atoms.intern(name), args);
    }
  }
  return machine.heap.atom(machine.atoms.intern("[]"));
}

std::uint64_t embody(Impl& machine, const IrTerm& goal, std::uint64_t cont, std::size_t cut,
                     const std::vector<Cell>& env, std::uint32_t module) {
  if (goal.tag == IrTag::Atom || goal.tag == IrTag::Fun || goal.tag == IrTag::Call) {
    std::vector<Cell> args;
    if (goal.tag != IrTag::Atom) {
      args.reserve(goal.args.size());
      for (const IrTerm& arg : goal.args) args.push_back(materialize(machine, arg, env));
    }
    const auto at = machine.heap.allocate_words(kFrameHead + args.size());
    const auto name = goal.tag == IrTag::Atom ? goal.atom : goal.atom;
    machine.heap.set_word(at, name);
    machine.heap.set_word(at + 1, pack_goal_word(static_cast<std::uint16_t>(args.size()), module));
    machine.heap.set_word(at + 2, cut);
    machine.heap.set_word(at + 3, cont);
    for (std::size_t index = 0; index < args.size(); ++index) {
      machine.heap.set_word(at + kFrameHead + index, args[index].raw());
    }
    return at;
  }
  Extra extra;
  extra.ctrl = Ctrl::Interpret;
  extra.goal = goal;
  extra.env = env;
  extra.cont = cont;
  extra.cut = cut;
  extra.module = module;
  return push_extra(machine, std::move(extra));
}

void launch_list(Impl& machine, const IrTerm& goal, std::uint64_t cont, std::size_t cut,
                 const std::vector<Cell>& env, std::uint32_t module) {
  // リストは '.'/2 である。未定義なら existence_error であり、callable の型誤りではない。
  IrTerm call;
  call.tag = IrTag::Fun;
  call.atom = machine.atoms.intern(".");
  call.arity = 2;
  call.args = goal.args;
  machine.ip = embody(machine, call, cont, cut, env, module);
}

void launch(Impl& machine, const IrTerm& goal, std::uint64_t cont, std::size_t cut,
            const std::vector<Cell>& env, std::uint32_t module) {
  switch (goal.tag) {
    case IrTag::True: machine.ip = cont; return;
    case IrTag::Fail: machine.failed = true; return;
    case IrTag::Cut: trim_stack(machine, cut); machine.ip = cont; return;
    case IrTag::And: {
      const auto right = embody(machine, goal.args[1], cont, cut, env, module);
      launch(machine, goal.args[0], right, cut, env, module);
      return;
    }
    case IrTag::Or: launch_or(machine, goal, cont, cut, env, module); return;
    case IrTag::IfThen: launch_if_then(machine, goal, cont, cut, env, module); return;
    case IrTag::SoftIf: launch_soft(machine, goal, cont, cut, env, module); return;
    case IrTag::Catch: arm_catch(machine, goal, cont, cut, env, module); return;
    case IrTag::Throw: arm_throw(machine, goal, env); return;
    case IrTag::Halt: arm_halt(machine, goal, env); return;
    case IrTag::Var: launch_var(machine, goal, cont, cut, env, module); return;
    case IrTag::Atom:
    case IrTag::Fun:
    case IrTag::Call: machine.ip = embody(machine, goal, cont, cut, env, module); return;
    case IrTag::List: launch_list(machine, goal, cont, cut, env, module); return;
    case IrTag::Anon:
    case IrTag::Int:
    case IrTag::Float: reject_callable(machine, materialize(machine, goal, env)); return;
  }
}

bool is_builtin_procedure(const Impl& machine, std::uint32_t name, std::uint16_t arity) {
  const auto text = machine.atoms.name(name);
  if (text == "call" && arity >= 1 && arity <= 8) return true;
  if (text == "halt" && arity <= 1) return true;
  for (const BuiltinPi& item : kBuiltins) {
    if (item.arity == arity && item.name == text) return true;
  }
  return false;
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
      run_flags(machine, goal) || run_database(machine, goal) || run_module_prop(machine, goal) ||
      run_lists(machine, goal) || run_format_float(machine, goal) || run_case(machine, goal) || run_char_width(machine, goal) || run_solutions(machine, goal) ||
      run_streams(machine, goal) || run_char_io(machine, goal) || run_byte_io(machine, goal) ||
      run_write(machine, goal) || run_read(machine, goal) || run_operators(machine, goal)) {
    return true;
  }
  return false;
}

Stop run_loop(Impl& machine) {
  while (true) {
    if (machine.halt_now) return Stop{StopKind::Halt, machine.halt_code, {}, {}, false};
    if (machine.exception) return Stop{StopKind::Exception, 0, machine.ball, {}, false};
    if (machine.failed) {
      if (!backtrack(machine)) return Stop{StopKind::Failure, 0, {}, {}, false};
      continue;
    }
    if (machine.ip == kNoFrame) {
      return Stop{StopKind::Success, 0, {}, machine.bindings, has_more(machine)};
    }
    step(machine);
  }
}

Machine::Machine(std::size_t heap_limit) : impl_(std::make_unique<Impl>(heap_limit)) {
  init_streams(*impl_);
}

Machine::~Machine() = default;

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
}

Stop Machine::query(std::string_view goal_text) {
  Impl& machine = *impl_;
  if (machine.halt_pending) {
    machine.halt_pending = false;
    return Stop{StopKind::Halt, machine.halt_code, {}, {}, false};
  }
  machine.halt_now = false;
  machine.stack.clear();
  // 前のクエリの選択点はもう無い。戻さずに捨てる。成功した op/3 は表に残す。
  machine.op_undos.clear();
  machine.trail.slots.clear();
  machine.trail.mark = nullptr;
  machine.extras.clear();
  machine.heap.truncate(0);
  machine.failed = false;
  machine.exception = false;
  machine.eval_abort = false;
  machine.native_next = nullptr;
  machine.diagnostic.clear();
  machine.ip = kNoFrame;
  machine.bindings.clear();
  Reader reader(goal_text, machine.heap, machine.operators, current_double_quotes(machine),
                &machine.char_conv);
  reader.set_fullwidth(machine.japanese_punct);
  const ReadOutcome outcome = reader.next();
  if (!outcome.term) return Stop{StopKind::Failure, 0, {}, {}, false};
  for (const VarName& name : outcome.term->names) {
    machine.bindings.push_back(Binding{name.name, name.cell});
  }
  // 失敗したクエリの束縛を、選択点が無くても戻すための水位。解の候補には数えない。
  Entry cleanup;
  cleanup.pick = Pick::Cleanup;
  cleanup.heap_mark = machine.heap.size();
  cleanup.trail_mark = machine.trail.slots.size();
  cleanup.extras_mark = machine.extras.size();
  machine.stack.push_back(std::move(cleanup));
  const Compiled compiled = compile_goal(machine.heap, outcome.term->term);
  launch(machine, compiled.term, kNoFrame, 0, compiled.env, machine.user_module);
  return run_loop(machine);
}

Stop Machine::next() {
  Impl& machine = *impl_;
  if (machine.halt_now) return Stop{StopKind::Halt, machine.halt_code, {}, {}, false};
  if (!backtrack(machine)) return Stop{StopKind::Failure, 0, {}, {}, false};
  return run_loop(machine);
}

}
