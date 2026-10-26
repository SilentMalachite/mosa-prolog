// 本体は機械で実行する。特殊な呼び出しだけを関数子で拾う。文字列の部分一致では分けない。
// assertion の失敗は、結果が false でも合格にしない。選言のもう片方は試す。

#include "lgt_priv.hpp"

#include "lgt_io.hpp"
#include "internal.hpp"

#include "mosa/writer.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mosa {
namespace {

enum class NodeKind { Plain, Conj, Disj, Assert, Variant, Subsumes, Approx, Forall, Super };

struct Node {
  NodeKind kind = NodeKind::Plain;
  std::string text;
  std::string note;
  int left = -1;
  int right = -1;
};

struct Prog {
  std::vector<Node> nodes;
};

enum class Tri { Ok, No, Abort, Throw, Dead };

struct Saved {
  std::string name;
  std::string term;
};

struct Env {
  Tri tri = Tri::No;
  bool live = false;
  std::vector<Binding> bindings;
  std::vector<Saved> saved;
  std::string note;
  Stop stop;
  const LgtIo* io = nullptr;
  std::string minted_var;
  std::string minted_term;
};

void remember_saved(std::vector<Saved>& saved, std::string name, std::string term) {
  if (name.empty()) return;
  for (Saved& slot : saved) {
    if (slot.name != name) continue;
    slot.term = std::move(term);
    return;
  }
  saved.push_back(Saved{std::move(name), std::move(term)});
}

const Saved* find_saved(const std::vector<Saved>& saved, std::string_view name) {
  for (const Saved& slot : saved) {
    if (slot.name == name) return &slot;
  }
  return nullptr;
}

Cell parse_saved(Machine& machine, std::string_view text, bool& ok) {
  const LgtParsed parsed = lgt_parse_term(machine, text);
  ok = parsed.ok;
  return parsed.term;
}

Cell swap_list(Machine& machine, Cell cell, const std::vector<VarName>& names, const std::vector<Saved>& saved,
               std::vector<std::uint64_t>& stack);

Cell swap_saved(Machine& machine, Cell cell, const std::vector<VarName>& names, const std::vector<Saved>& saved,
                std::vector<std::uint64_t>& stack) {
  Heap& heap = machine.heap();
  cell = heap.deref(cell);
  if (heap.unbound(cell)) {
    for (const VarName& var : names) {
      if (var.cell.raw() != cell.raw()) continue;
      const Saved* slot = find_saved(saved, var.name);
      if (slot == nullptr) return cell;
      bool ok = false;
      const Cell term = parse_saved(machine, slot->term, ok);
      return ok ? term : cell;
    }
    return cell;
  }
  if (cell.tag() != Tag::Lis && cell.tag() != Tag::Str) return cell;
  for (const auto seen : stack) {
    if (seen == cell.raw()) return cell;
  }
  stack.push_back(cell.raw());
  const Cell built = cell.tag() == Tag::Lis ? swap_list(machine, cell, names, saved, stack)
                                            : [&] {
                                                std::vector<Cell> args;
                                                args.reserve(heap.functor_arity(cell));
                                                for (std::uint16_t index = 0; index < heap.functor_arity(cell); ++index) {
                                                  args.push_back(swap_saved(machine, heap.arg(cell, index), names, saved, stack));
                                                }
                                                return heap.structure(heap.functor_atom(cell), args);
                                              }();
  stack.pop_back();
  return built;
}

Cell swap_list(Machine& machine, Cell cell, const std::vector<VarName>& names, const std::vector<Saved>& saved,
               std::vector<std::uint64_t>& stack) {
  Heap& heap = machine.heap();
  const Cell head = swap_saved(machine, heap.list_head(cell), names, saved, stack);
  const Cell tail = swap_saved(machine, heap.list_tail(cell), names, saved, stack);
  return heap.list(head, tail);
}

std::string goal_with_saved(Machine& machine, std::string_view goal, const std::vector<Saved>& saved) {
  if (saved.empty()) return std::string{goal};
  const LgtParsed parsed = lgt_parse_term(machine, goal);
  if (!parsed.ok) return std::string{goal};
  std::vector<std::uint64_t> stack;
  const Cell term = swap_saved(machine, parsed.term, parsed.names, saved, stack);
  return write_term(machine.heap(), machine.operators(), term, {}, parsed.names);
}

void restore_saved(Machine& machine, Env& env) {
  for (const Saved& slot : env.saved) {
    bool found = false;
    for (const Binding& binding : env.bindings) {
      if (binding.name != slot.name) continue;
      found = true;
      break;
    }
    if (found) continue;
    bool ok = false;
    const Cell term = parse_saved(machine, slot.term, ok);
    if (!ok) continue;
    env.bindings.push_back(Binding{slot.name, term});
  }
}

void remember_stop(Machine& machine, const Stop& stop, Env& env) {
  env.bindings = stop.bindings;
  // 決定的な成功の次も、同じ機械で入出力を続ける。選択点が残るときだけ別機械で見る。
  env.live = stop.more;
  for (const Binding& binding : stop.bindings) {
    remember_saved(env.saved, binding.name, write_term(machine.heap(), machine.operators(), binding.variable, {}, {}));
  }
}

int add_node(Prog& prog, Node node) {
  prog.nodes.push_back(std::move(node));
  return static_cast<int>(prog.nodes.size() - 1);
}

bool assertion_term(const Heap& heap, Cell cell) {
  cell = heap.deref(cell);
  if (cell.tag() != Tag::Str || heap.atoms().name(heap.functor_atom(cell)) != "assertion") return false;
  const auto arity = heap.functor_arity(cell);
  return arity == 1 || arity == 2;
}

int build_node(Prog& prog, const Heap& heap, const OperatorTable& ops, Cell cell, const std::vector<VarName>& names) {
  cell = heap.deref(cell);
  if (cell.tag() == Tag::Str) {
    const auto name = heap.atoms().name(heap.functor_atom(cell));
    const auto arity = heap.functor_arity(cell);
    if ((name == "," || name == ";" || name == "forall") && arity == 2) {
      Node node;
      node.kind = name == "," ? NodeKind::Conj : name == ";" ? NodeKind::Disj : NodeKind::Forall;
      node.left = build_node(prog, heap, ops, heap.arg(cell, 0), names);
      node.right = build_node(prog, heap, ops, heap.arg(cell, 1), names);
      return add_node(prog, std::move(node));
    }
    if (name == "assertion" && (arity == 1 || arity == 2)) {
      Node node;
      node.kind = NodeKind::Assert;
      if (arity == 2) node.note = write_term(heap, ops, heap.arg(cell, 0), {}, names);
      node.left = build_node(prog, heap, ops, heap.arg(cell, arity == 1 ? 0 : 1), names);
      return add_node(prog, std::move(node));
    }
    if (name == "^^") {
      // 入出力表に渡すと未知の assertion/N になり、前のゴールの束縛が表明に届かない。
      if (arity == 1 && assertion_term(heap, heap.arg(cell, 0))) {
        return build_node(prog, heap, ops, heap.arg(cell, 0), names);
      }
      Node node;
      node.kind = NodeKind::Super;
      if (arity == 1) node.text = write_term(heap, ops, heap.arg(cell, 0), {}, names);
      else node.note = "^^/" + std::to_string(arity);
      return add_node(prog, std::move(node));
    }
    if (arity == 2 && (name == "variant" || name == "subsumes" || name == "subsumes_term" || name == "=~=")) {
      Node node;
      node.text = write_term(heap, ops, cell, {}, names);
      if (name == "variant") node.kind = NodeKind::Variant;
      else if (name == "=~=") node.kind = NodeKind::Approx;
      else node.kind = NodeKind::Subsumes;
      return add_node(prog, std::move(node));
    }
  }
  Node plain;
  plain.text = write_term(heap, ops, cell, {}, names);
  return add_node(prog, std::move(plain));
}

void exec_at(Machine& machine, const Prog& prog, int index, Env& env);

void exec_plain(Machine& machine, const Node& node, Env& env) {
  // 既に解を開いているときは query しない。選択点が消える。
  if (env.live) {
    env.tri = lgt_scratch_ok(machine, node.text, env.bindings) ? Tri::Ok : Tri::No;
    return;
  }
  const std::string text = goal_with_saved(machine, node.text, env.saved);
  const LgtStep step = lgt_ask(machine, text + ".");
  env.stop = step.stop;
  if (step.dead) {
    env.tri = Tri::Dead;
    return;
  }
  if (step.stop.kind == StopKind::Success) {
    remember_stop(machine, step.stop, env);
    env.tri = Tri::Ok;
    return;
  }
  env.live = false;
  env.tri = step.stop.kind == StopKind::Exception ? Tri::Throw : Tri::No;
}

void exec_check(Machine& machine, const Node& node, Env& env) {
  std::string note;
  env.tri = lgt_check_text(machine, node.text, env.bindings, note) ? Tri::Ok : Tri::No;
}

void exec_assert(Machine& machine, const Prog& prog, int index, Env& env) {
  const int child = prog.nodes[static_cast<std::size_t>(index)].left;
  const std::string note = prog.nodes[static_cast<std::size_t>(index)].note;
  exec_at(machine, prog, child, env);
  if (env.tri == Tri::Ok || env.tri == Tri::Abort || env.tri == Tri::Dead || env.tri == Tri::Throw) return;
  // ゴールの失敗は、false という結果では済まない。テスト自体が不合格。
  env.note = note;
  env.tri = Tri::Abort;
}

void exec_disj(Machine& machine, const Prog& prog, int index, Env& env) {
  const int left = prog.nodes[static_cast<std::size_t>(index)].left;
  const int right = prog.nodes[static_cast<std::size_t>(index)].right;
  exec_at(machine, prog, left, env);
  const Tri first = env.tri;
  if (first == Tri::Ok || first == Tri::Dead || first == Tri::Throw) return;
  const std::string note = env.note;
  exec_at(machine, prog, right, env);
  if (env.tri == Tri::Ok || env.tri == Tri::Dead || env.tri == Tri::Throw) return;
  if (first == Tri::Abort || env.tri == Tri::Abort) {
    env.tri = Tri::Abort;
    if (env.note.empty()) env.note = note;
    return;
  }
  env.tri = Tri::No;
}

void retry_right(Machine& machine, const Prog& prog, int right, Env& env) {
  while (env.tri == Tri::No && env.live) {
    const LgtStep step = lgt_again(machine);
    env.stop = step.stop;
    if (step.dead) {
      env.tri = Tri::Dead;
      return;
    }
    if (step.stop.kind != StopKind::Success) {
      env.live = false;
      env.tri = step.stop.kind == StopKind::Exception ? Tri::Throw : Tri::No;
      return;
    }
    env.bindings = step.stop.bindings;
    exec_at(machine, prog, right, env);
  }
}

void exec_conj(Machine& machine, const Prog& prog, int index, Env& env) {
  const int left = prog.nodes[static_cast<std::size_t>(index)].left;
  const int right = prog.nodes[static_cast<std::size_t>(index)].right;
  exec_at(machine, prog, left, env);
  if (env.tri != Tri::Ok) return;
  exec_at(machine, prog, right, env);
  if (env.tri != Tri::No) return;
  retry_right(machine, prog, right, env);
}

void exec_forall(Machine& machine, const Prog& prog, int index, Env& env) {
  const int generator = prog.nodes[static_cast<std::size_t>(index)].left;
  const int action = prog.nodes[static_cast<std::size_t>(index)].right;
  if (generator < 0 || prog.nodes[static_cast<std::size_t>(generator)].kind != NodeKind::Plain) {
    env.tri = Tri::No;
    return;
  }
  const std::string text = prog.nodes[static_cast<std::size_t>(generator)].text;
  LgtStep step = lgt_ask(machine, text + ".");
  env.stop = step.stop;
  if (step.dead) {
    env.tri = Tri::Dead;
    return;
  }
  if (step.stop.kind == StopKind::Failure) {
    env.live = false;
    env.tri = Tri::Ok;
    return;
  }
  if (step.stop.kind != StopKind::Success) {
    env.tri = Tri::Throw;
    return;
  }
  while (true) {
    env.live = true;
    env.bindings = step.stop.bindings;
    exec_at(machine, prog, action, env);
    if (env.tri != Tri::Ok) return;
    step = lgt_again(machine);
    env.stop = step.stop;
    if (step.dead) {
      env.tri = Tri::Dead;
      return;
    }
    if (step.stop.kind == StopKind::Failure) {
      env.live = false;
      env.tri = Tri::Ok;
      return;
    }
    if (step.stop.kind != StopKind::Success) {
      env.tri = Tri::Throw;
      return;
    }
  }
}

void remember_super(Machine& machine, const LgtSuper& ran, Env& env) {
  if (ran.binding_name.empty()) return;
  // 次の query がヒープを捨てても、表明の項を文字列で残す。
  env.minted_var = ran.binding_name;
  env.minted_term = write_term(machine.heap(), machine.operators(), ran.binding, {}, {});
  remember_saved(env.saved, ran.binding_name, env.minted_term);
  for (Binding& binding : env.bindings) {
    if (binding.name != ran.binding_name) continue;
    binding.variable = ran.binding;
    return;
  }
  env.bindings.push_back(Binding{ran.binding_name, ran.binding});
}

void exec_super(Machine& machine, const Node& node, Env& env) {
  if (!node.note.empty() || env.io == nullptr) {
    env.note = node.note.empty() ? "^^/1" : node.note;
    env.tri = Tri::Abort;
    return;
  }
  const LgtSuper ran = lgt_run_super(machine, *env.io, node.text);
  if (ran.kind == LgtSuperKind::Unknown) {
    env.note = ran.note.empty() ? "^^/1" : ran.note;
    env.tri = Tri::Abort;
    return;
  }
  if (ran.kind != LgtSuperKind::Ok) {
    env.tri = Tri::No;
    return;
  }
  remember_super(machine, ran, env);
  env.tri = Tri::Ok;
}

void exec_at(Machine& machine, const Prog& prog, int index, Env& env) {
  if (index < 0 || static_cast<std::size_t>(index) >= prog.nodes.size()) {
    env.tri = Tri::No;
    return;
  }
  const NodeKind kind = prog.nodes[static_cast<std::size_t>(index)].kind;
  switch (kind) {
    case NodeKind::Plain: exec_plain(machine, prog.nodes[static_cast<std::size_t>(index)], env); return;
    case NodeKind::Conj: exec_conj(machine, prog, index, env); return;
    case NodeKind::Disj: exec_disj(machine, prog, index, env); return;
    case NodeKind::Assert: exec_assert(machine, prog, index, env); return;
    case NodeKind::Forall: exec_forall(machine, prog, index, env); return;
    case NodeKind::Super: exec_super(machine, prog.nodes[static_cast<std::size_t>(index)], env); return;
    case NodeKind::Variant:
    case NodeKind::Subsumes:
    case NodeKind::Approx: exec_check(machine, prog.nodes[static_cast<std::size_t>(index)], env); return;
  }
}

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

enum class Verdict { Yes, No, Bad, Dead, Throw };

Verdict ask_text(Machine& machine, std::string_view goal) {
  Stop stop;
  try {
    stop = machine.query(goal);
  } catch (const ResourceLimit&) {
    return Verdict::Dead;
  }
  // 条件の halt を、次のテストのクエリに残さない。
  Impl& impl = engine_impl(machine);
  impl.halt_now = false;
  impl.halt_pending = false;
  if (stop.kind == StopKind::Success) return Verdict::Yes;
  if (stop.kind == StopKind::Exception) return Verdict::Throw;
  return Verdict::No;
}

// 呼び出しの位置だけ見る。普通の述語の引数はデータなので、旗があっても呼び出さない。
bool calls_special(const Heap& heap, Cell cell) {
  cell = heap.deref(cell);
  if (cell.tag() != Tag::Str) return false;
  const auto name = heap.atoms().name(heap.functor_atom(cell));
  const auto arity = heap.functor_arity(cell);
  if (name == "current_logtalk_flag" && arity == 2) return true;
  if (name == "::" || name == "^^") return true;
  if (name == "predicate_property" && arity == 2) return true;
  if ((name == "," || name == ";" || name == "->" || name == "*->") && arity == 2) {
    return calls_special(heap, heap.arg(cell, 0)) || calls_special(heap, heap.arg(cell, 1));
  }
  if ((name == "\\+" || name == "call" || name == "once" || name == "{}") && arity == 1) {
    return calls_special(heap, heap.arg(cell, 0));
  }
  if (name == "catch" && arity == 3) {
    return calls_special(heap, heap.arg(cell, 0)) || calls_special(heap, heap.arg(cell, 2));
  }
  return false;
}

Verdict ask_cell(Machine& machine, const Heap& heap, const OperatorTable& ops, Cell cell) {
  return ask_text(machine, write_term(heap, ops, cell, {}, {}) + ".");
}

Verdict property_cond(Machine& machine, const Heap& heap, Cell cell) {
  const Cell pred = heap.deref(heap.arg(cell, 0));
  const Cell prop = heap.deref(heap.arg(cell, 1));
  if (!atom_is(heap, prop, "built_in")) return Verdict::No;
  std::string_view name;
  std::uint16_t arity = 0;
  if (pred.tag() == Tag::Atom) {
    name = atom_text(heap, pred);
  } else if (pred.tag() == Tag::Str) {
    name = heap.atoms().name(heap.functor_atom(pred));
    arity = heap.functor_arity(pred);
  } else {
    return Verdict::No;
  }
  Impl& impl = engine_impl(machine);
  return is_builtin_procedure(impl, impl.atoms.intern(name), arity) ? Verdict::Yes : Verdict::No;
}

Verdict message_cond(const Heap& heap, Cell cell) {
  if (!functor_is(heap, cell, "::", 2) || !atom_is(heap, heap.arg(cell, 0), "os")) return Verdict::Bad;
  const Cell msg = heap.deref(heap.arg(cell, 1));
  if (!functor_is(heap, msg, "operating_system_type", 1)) return Verdict::Bad;
  if (!atom_is(heap, heap.arg(msg, 0), "windows")) return Verdict::Bad;
  // windows は Prolog として実行しない。成功したことにしない。
  return Verdict::No;
}

Verdict eval_goal(Machine& machine, const Heap& heap, const OperatorTable& ops, Cell cell);

Verdict eval_and(Machine& machine, const Heap& heap, const OperatorTable& ops, Cell cell) {
  const Verdict left = eval_goal(machine, heap, ops, heap.arg(cell, 0));
  // 左が失敗したら右は実行しない。旗の右で存在エラーを起こさない。
  if (left != Verdict::Yes) return left;
  return eval_goal(machine, heap, ops, heap.arg(cell, 1));
}

Verdict eval_or(Machine& machine, const Heap& heap, const OperatorTable& ops, Cell cell) {
  const Verdict left = eval_goal(machine, heap, ops, heap.arg(cell, 0));
  if (left != Verdict::No) return left;
  return eval_goal(machine, heap, ops, heap.arg(cell, 1));
}

Verdict eval_not(Machine& machine, const Heap& heap, const OperatorTable& ops, Cell cell) {
  const Verdict inner = eval_goal(machine, heap, ops, heap.arg(cell, 0));
  if (inner == Verdict::Yes) return Verdict::No;
  if (inner == Verdict::No) return Verdict::Yes;
  return inner;
}

Verdict eval_if(Machine& machine, const Heap& heap, const OperatorTable& ops, Cell cell) {
  const Verdict cond = eval_goal(machine, heap, ops, heap.arg(cell, 0));
  if (cond != Verdict::Yes) return cond == Verdict::No ? Verdict::No : cond;
  return eval_goal(machine, heap, ops, heap.arg(cell, 1));
}

Verdict eval_catch(Machine& machine, const Heap& heap, const OperatorTable& ops, Cell cell) {
  if (!calls_special(heap, heap.arg(cell, 0)) && !calls_special(heap, heap.arg(cell, 2))) {
    return ask_cell(machine, heap, ops, cell);
  }
  const Verdict ran = eval_goal(machine, heap, ops, heap.arg(cell, 0));
  if (ran != Verdict::Throw) return ran;
  if (!heap.unbound(heap.deref(heap.arg(cell, 1)))) return Verdict::Throw;
  return eval_goal(machine, heap, ops, heap.arg(cell, 2));
}

Verdict eval_goal(Machine& machine, const Heap& heap, const OperatorTable& ops, Cell cell) {
  cell = heap.deref(cell);
  if (atom_is(heap, cell, "true")) return Verdict::Yes;
  if (atom_is(heap, cell, "fail") || atom_is(heap, cell, "false")) return Verdict::No;
  // 旗も :: も無いゴールは一つの query。分けると束縛が消え、(-> ; ) の else が走る。
  if (!calls_special(heap, cell)) return ask_cell(machine, heap, ops, cell);
  if (cell.tag() != Tag::Str) return ask_cell(machine, heap, ops, cell);
  const auto name = heap.atoms().name(heap.functor_atom(cell));
  const auto arity = heap.functor_arity(cell);
  if (name == "," && arity == 2) return eval_and(machine, heap, ops, cell);
  if (name == ";" && arity == 2) return eval_or(machine, heap, ops, cell);
  if ((name == "->" || name == "*->") && arity == 2) return eval_if(machine, heap, ops, cell);
  if (name == "\\+" && arity == 1) return eval_not(machine, heap, ops, cell);
  if ((name == "call" || name == "once" || name == "{}") && arity == 1) {
    return eval_goal(machine, heap, ops, heap.arg(cell, 0));
  }
  if (name == "catch" && arity == 3) return eval_catch(machine, heap, ops, cell);
  // 旗は実行しない。失敗であり、存在エラーではない。
  if (name == "current_logtalk_flag" && arity == 2) return Verdict::No;
  if (name == "::") return message_cond(heap, cell);
  if (name == "^^") return Verdict::Bad;
  if (name == "predicate_property" && arity == 2) return property_cond(machine, heap, cell);
  return ask_cell(machine, heap, ops, cell);
}

LgtCond publish_cond(Verdict verdict) {
  if (verdict == Verdict::Yes) return LgtCond::Yes;
  if (verdict == Verdict::Bad) return LgtCond::Bad;
  if (verdict == Verdict::Dead) return LgtCond::Dead;
  return LgtCond::No;
}

bool condition_ops(OperatorTable& ops) {
  if (ops.define(600, Specifier::Xfy, ":") != OpResult::Ok) return false;
  return ops.define(600, Specifier::Xfy, "::") == OpResult::Ok;
}

void remint(Machine& machine, Env& env) {
  if (env.minted_var.empty() || env.minted_term.empty()) return;
  const LgtParsed parsed = lgt_parse_term(machine, env.minted_term);
  if (!parsed.ok) return;
  for (Binding& binding : env.bindings) {
    if (binding.name != env.minted_var) continue;
    binding.variable = parsed.term;
    return;
  }
  env.bindings.push_back(Binding{env.minted_var, parsed.term});
}

LgtEnd from_env(Machine& machine, LgtWant want, Env& env) {
  LgtEnd end;
  if (env.tri == Tri::Dead) return lgt_aborted();
  if (env.tri == Tri::Abort) {
    end.actual = env.note.empty() ? "assertion" : env.note;
    return end;
  }
  if (want.kind == LgtKind::False) {
    if (env.tri == Tri::Throw) end.actual = lgt_actual_of(machine, env.stop);
    else if (env.tri == Tri::No) end.pass = true;
    else end.actual = "true";
    return end;
  }
  if (env.tri == Tri::Throw) return lgt_failed_step(machine, LgtStep{false, env.stop});
  if (env.tri != Tri::Ok) {
    if (want.kind == LgtKind::All && env.tri == Tri::No) end.pass = true;
    else end.actual = "fail";
    return end;
  }
  restore_saved(machine, env);
  remint(machine, env);
  if (want.kind == LgtKind::True) {
    end.pass = true;
    return end;
  }
  Stop synthetic;
  synthetic.kind = StopKind::Success;
  synthetic.bindings = env.bindings;
  return lgt_finish(machine, want, synthetic);
}

}  // namespace

LgtCond lgt_condition(Machine& machine, std::string_view goal) {
  if (goal.empty()) return LgtCond::No;
  try {
    AtomTable atoms;
    Heap heap(atoms);
    OperatorTable ops = OperatorTable::iso_core();
    if (!condition_ops(ops)) return LgtCond::Bad;
    // Reader は string_view しか持たない。一時文字列のまま渡すと、next の時点で消える。
    const std::string source = std::string{goal} + ".";
    Reader reader(source, heap, ops);
    const ReadOutcome read = reader.next();
    if (!read.term) return LgtCond::Bad;
    return publish_cond(eval_goal(machine, heap, ops, read.term->term));
  } catch (const ResourceLimit&) {
    return LgtCond::Dead;
  }
}

LgtEnd lgt_judge_mixed(Machine& machine, std::string_view body, const LgtWant& want, const LgtIo* io) {
  const LgtParsed parsed = lgt_parse_term(machine, body);
  if (!parsed.ok) {
    LgtEnd end;
    end.actual = "unsupported";
    return end;
  }
  Prog prog;
  const int root = build_node(prog, machine.heap(), machine.operators(), parsed.term, parsed.names);
  Env env;
  env.io = io;
  exec_at(machine, prog, root, env);
  return from_env(machine, want, env);
}

}  // namespace mosa
