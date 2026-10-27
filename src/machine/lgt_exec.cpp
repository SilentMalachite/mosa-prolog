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

enum class NodeKind { Plain, Conj, Disj, If, Soft, Assert, Variant, Subsumes, Approx, Forall, Super };

struct Node {
  NodeKind kind = NodeKind::Plain;
  std::string text;
  std::string note;
  int left = -1;
  int right = -1;
  // Soft の else。If だけでは else を持たない。
  int other = -1;
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
  // 次の query がヒープを捨てる前の束縛全体。項をまたぐ同じ変数を 1 つの _GN で残す。
  std::string bundle;
  std::vector<std::string> bundle_names;
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

struct SavedText {
  Machine* machine = nullptr;
  const std::vector<VarName>* names = nullptr;
  const std::vector<Saved>* saved = nullptr;
};

// 読み直すと _GN が分かれる。今の演算子で読めない字面は、変数のまま残す。
WriteCut paste_saved(void* context, Cell term, std::string& out) {
  const auto* text = static_cast<const SavedText*>(context);
  if (text == nullptr || text->machine == nullptr || term.tag() != Tag::Ref) return WriteCut::Keep;
  for (const VarName& var : *text->names) {
    if (var.cell.raw() != term.raw()) continue;
    const Saved* slot = find_saved(*text->saved, var.name);
    if (slot == nullptr) return WriteCut::Keep;
    bool ok = false;
    parse_saved(*text->machine, slot->term, ok);
    if (!ok) return WriteCut::Keep;
    out = "(" + slot->term + ")";
    return WriteCut::Swap;
  }
  return WriteCut::Keep;
}

std::string goal_with_saved(Machine& machine, std::string_view goal, const std::vector<Saved>& saved) {
  if (saved.empty()) return std::string{goal};
  const LgtParsed parsed = lgt_parse_term(machine, goal);
  if (!parsed.ok) return std::string{goal};
  SavedText text{&machine, &parsed.names, &saved};
  return write_term(machine.heap(), machine.operators(), parsed.term, {}, parsed.names, paste_saved, &text);
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

void snapshot_bundle(Machine& machine, Env& env);
bool proper_list(const Heap& heap, Cell cell, std::vector<Cell>& args);

void remember_stop(Machine& machine, const Stop& stop, Env& env) {
  env.bindings = stop.bindings;
  // 決定的な成功の次も、同じ機械で入出力を続ける。選択点が残るときだけ別機械で見る。
  env.live = stop.more;
  // 束縛ごとに書くと、一度しか出ない変数が _ になり、次の == が共有を見ない。
  snapshot_bundle(machine, env);
  const LgtParsed parsed = lgt_parse_term(machine, env.bundle);
  std::vector<Cell> args;
  if (parsed.ok && proper_list(machine.heap(), parsed.term, args) && args.size() == env.bundle_names.size()) {
    for (std::size_t index = 0; index < args.size(); ++index) {
      remember_saved(env.saved, env.bundle_names[index],
                     write_term(machine.heap(), machine.operators(), args[index], {}, parsed.names));
    }
    return;
  }
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

enum class GoalMix { Plain, Update, Runner };

bool runner_functor(std::string_view name, std::uint16_t arity) {
  if (name == "^^") return true;
  if (name == "assertion" && (arity == 1 || arity == 2)) return true;
  if (name == "forall" && arity == 2) return true;
  if (arity != 2) return false;
  return name == "variant" || name == "subsumes" || name == "subsumes_term" || name == "=~=";
}

bool update_functor(std::string_view name, std::uint16_t arity) {
  // op/3 を別の query に分けると、o/1 の選択点が消えてから xf が走る。戻す場所が無くなる。
  if (name == "op" && arity == 3) return true;
  if (arity != 1) return false;
  return name == "asserta" || name == "assertz" || name == "retract" || name == "retractall" || name == "abolish";
}

bool control_goal(std::string_view name, std::uint16_t arity) {
  if (arity == 2 && (name == "," || name == ";" || name == "->" || name == "*->")) return true;
  return arity == 1 && (name == "\\+" || name == "once" || name == "call" || name == "{}");
}

GoalMix mix_of(const Heap& heap, Cell cell, std::vector<std::uint64_t>& seen) {
  cell = heap.deref(cell);
  if (cell.tag() != Tag::Str) return GoalMix::Plain;
  for (const auto raw : seen) {
    if (raw == cell.raw()) return GoalMix::Plain;
  }
  const auto name = heap.atoms().name(heap.functor_atom(cell));
  const auto arity = heap.functor_arity(cell);
  if (runner_functor(name, arity)) return GoalMix::Runner;
  if (update_functor(name, arity)) return GoalMix::Update;
  if (!control_goal(name, arity)) return GoalMix::Plain;
  seen.push_back(cell.raw());
  GoalMix found = GoalMix::Plain;
  for (std::uint16_t index = 0; index < arity; ++index) {
    const GoalMix child = mix_of(heap, heap.arg(cell, index), seen);
    if (child == GoalMix::Runner) {
      found = GoalMix::Runner;
      break;
    }
    if (child == GoalMix::Update) found = GoalMix::Update;
  }
  seen.pop_back();
  return found;
}

// 7.5.4 は、その呼出しが始まったときの節列だけを候補にする。
// 更新を別の機械へ分けると、開いている走査は追加も削除も見ない。
bool one_activation(const Heap& heap, Cell cell) {
  std::vector<std::uint64_t> seen;
  return mix_of(heap, cell, seen) == GoalMix::Update;
}

// ランナー目標が無い -> は -1。機械の if-then に残し、条件のカットを移さない。
int build_runner_if(Prog& prog, const Heap& heap, const OperatorTable& ops, Cell cell,
                    const std::vector<VarName>& names);
// 左がランナーの -> でない ; は -1。else を同じ節点に置き、成功した条件が切れるようにする。
int build_runner_soft(Prog& prog, const Heap& heap, const OperatorTable& ops, Cell cell,
                      const std::vector<VarName>& names);

int build_node(Prog& prog, const Heap& heap, const OperatorTable& ops, Cell cell, const std::vector<VarName>& names) {
  cell = heap.deref(cell);
  if (cell.tag() == Tag::Str) {
    const auto name = heap.atoms().name(heap.functor_atom(cell));
    const auto arity = heap.functor_arity(cell);
    if (name == "->" && arity == 2) {
      const int arrow = build_runner_if(prog, heap, ops, cell, names);
      if (arrow >= 0) return arrow;
    }
    if (name == ";" && arity == 2) {
      const int soft = build_runner_soft(prog, heap, ops, cell, names);
      if (soft >= 0) return soft;
    }
    if ((name == "," || name == ";") && arity == 2 && one_activation(heap, cell)) {
      Node plain;
      plain.text = write_term(heap, ops, cell, {}, names);
      return add_node(prog, std::move(plain));
    }
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

int build_runner_if(Prog& prog, const Heap& heap, const OperatorTable& ops, Cell cell,
                    const std::vector<VarName>& names) {
  std::vector<std::uint64_t> seen;
  // 子にランナー目標が無い。条件のカットは機械の if-then に残す。
  if (mix_of(heap, cell, seen) != GoalMix::Runner) return -1;
  Node node;
  node.kind = NodeKind::If;
  node.left = build_node(prog, heap, ops, heap.arg(cell, 0), names);
  node.right = build_node(prog, heap, ops, heap.arg(cell, 1), names);
  return add_node(prog, std::move(node));
}

int build_runner_soft(Prog& prog, const Heap& heap, const OperatorTable& ops, Cell cell,
                      const std::vector<VarName>& names) {
  const Cell arrow = heap.deref(heap.arg(cell, 0));
  if (arrow.tag() != Tag::Str || heap.functor_arity(arrow) != 2) return -1;
  if (heap.atoms().name(heap.functor_atom(arrow)) != "->") return -1;
  std::vector<std::uint64_t> seen;
  // 機械だけの -> は選言の左の query のままにする。カットをランナーへ移さない。
  if (mix_of(heap, arrow, seen) != GoalMix::Runner) return -1;
  Node node;
  node.kind = NodeKind::Soft;
  node.left = build_node(prog, heap, ops, heap.arg(arrow, 0), names);
  node.right = build_node(prog, heap, ops, heap.arg(arrow, 1), names);
  node.other = build_node(prog, heap, ops, heap.arg(cell, 1), names);
  return add_node(prog, std::move(node));
}

void exec_at(Machine& machine, const Prog& prog, int index, Env& env);

// 別々に書くと、束縛をまたぐ同じ自由変数が _ になり、読み戻すと別セルになる。
void snapshot_bundle(Machine& machine, Env& env) {
  env.bundle_names.clear();
  std::vector<Cell> cells;
  cells.reserve(env.bindings.size());
  for (const Binding& binding : env.bindings) {
    env.bundle_names.push_back(binding.name);
    cells.push_back(binding.variable);
  }
  Cell tail = machine.heap().atom(machine.atoms().intern("[]"));
  for (auto it = cells.rbegin(); it != cells.rend(); ++it) tail = machine.heap().list(*it, tail);
  env.bundle = write_term(machine.heap(), machine.operators(), tail, {}, {});
}

bool proper_list(const Heap& heap, Cell cell, std::vector<Cell>& args) {
  cell = heap.deref(cell);
  while (cell.tag() == Tag::Lis) {
    args.push_back(heap.list_head(cell));
    cell = heap.deref(heap.list_tail(cell));
  }
  if (cell.tag() != Tag::Atom) return false;
  return heap.atoms().name(static_cast<std::uint32_t>(cell.raw() >> 3)) == "[]";
}

void rehydrate_bindings(Machine& machine, Env& env) {
  const LgtParsed parsed = lgt_parse_term(machine, env.bundle);
  std::vector<Cell> args;
  if (!parsed.ok || !proper_list(machine.heap(), parsed.term, args) || args.size() != env.bundle_names.size()) {
    // 読めない束縛は消さない。黙って落とすと、共有を見る判定が偽のまま残る。
    env.note = "bindings";
    env.tri = Tri::Dead;
    return;
  }
  std::vector<Binding> fresh;
  fresh.reserve(args.size());
  for (std::size_t index = 0; index < args.size(); ++index) {
    fresh.push_back(Binding{env.bundle_names[index], args[index]});
  }
  env.bindings = std::move(fresh);
}

void exec_plain(Machine& machine, const Node& node, Env& env) {
  // 既に解を開いているときは query しない。選択点が消える。
  if (env.live) {
    env.tri = lgt_scratch_ok(machine, node.text, env.bindings) ? Tri::Ok : Tri::No;
    return;
  }
  const std::string text = goal_with_saved(machine, node.text, env.saved);
  snapshot_bundle(machine, env);
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
  if (env.tri == Tri::No) rehydrate_bindings(machine, env);
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

void take_else(Machine& machine, const Prog& prog, int right, Tri first, Env& env) {
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

void exec_disj(Machine& machine, const Prog& prog, int index, Env& env) {
  const int left = prog.nodes[static_cast<std::size_t>(index)].left;
  const int right = prog.nodes[static_cast<std::size_t>(index)].right;
  exec_at(machine, prog, left, env);
  const Tri first = env.tri;
  if (first == Tri::Ok || first == Tri::Dead || first == Tri::Throw) return;
  take_else(machine, prog, right, first, env);
}

void exec_if(Machine& machine, const Prog& prog, int index, Env& env) {
  const Node& node = prog.nodes[static_cast<std::size_t>(index)];
  exec_at(machine, prog, node.left, env);
  if (env.tri != Tri::Ok) return;
  // 選択点を残すと then の失敗で親が条件をやり直す。成功した解だけで then を実行する。
  env.live = false;
  exec_at(machine, prog, node.right, env);
}

void exec_soft(Machine& machine, const Prog& prog, int index, Env& env) {
  const Node& node = prog.nodes[static_cast<std::size_t>(index)];
  exec_at(machine, prog, node.left, env);
  if (env.tri == Tri::Ok) {
    // 条件の成功で else を切る。then が失敗しても、この節点は else を持ったまま返さない。
    env.live = false;
    exec_at(machine, prog, node.right, env);
    return;
  }
  if (env.tri == Tri::Dead || env.tri == Tri::Throw) return;
  take_else(machine, prog, node.other, env.tri, env);
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
  // 前のゴールが束縛した変数を渡す。字面の Path のままだと create が失敗する。
  const std::string text = goal_with_saved(machine, node.text, env.saved);
  const LgtSuper ran = lgt_run_super(machine, *env.io, text);
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
    case NodeKind::If: exec_if(machine, prog, index, env); return;
    case NodeKind::Soft: exec_soft(machine, prog, index, env); return;
    case NodeKind::Assert: exec_assert(machine, prog, index, env); return;
    case NodeKind::Forall: exec_forall(machine, prog, index, env); return;
    case NodeKind::Super: exec_super(machine, prog.nodes[static_cast<std::size_t>(index)], env); return;
    case NodeKind::Variant:
    case NodeKind::Subsumes:
    case NodeKind::Approx: exec_check(machine, prog.nodes[static_cast<std::size_t>(index)], env); return;
  }
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

bool error_ball(const Heap& heap, Cell ball, Cell formal) {
  ball = heap.deref(ball);
  const bool shaped = ball.tag() == Tag::Str && heap.functor_arity(ball) == 2 &&
                      heap.atoms().name(heap.functor_atom(ball)) == "error";
  if (!shaped) return false;
  // Context は比べない。許された Formal が実際の Formal を包摂すれば足りる。
  return lgt_subsumes(heap, formal, heap.arg(ball, 0));
}

// 次の query はヒープを捨てる。自由なままの Stream は別の犯人まで包摂するので、残した字面を戻す。
// listed のときだけ列を歩く。error/1 の項がリストでも、候補の一つにはしない。
bool thrown_error(Machine& machine, const Env& env, std::string_view detail, bool listed) {
  const LgtParsed parsed = lgt_parse_term(machine, detail);
  if (!parsed.ok) return false;
  std::vector<std::uint64_t> stack;
  const Cell expected = swap_saved(machine, parsed.term, parsed.names, env.saved, stack);
  const Heap& heap = machine.heap();
  if (!listed) return error_ball(heap, env.stop.exception, expected);
  Cell list = heap.deref(expected);
  while (list.tag() == Tag::Lis) {
    if (error_ball(heap, env.stop.exception, heap.list_head(list))) return true;
    list = heap.deref(heap.list_tail(list));
  }
  return false;
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
  if (env.tri == Tri::Throw) {
    const bool listed = want.kind == LgtKind::Errors;
    if ((want.kind == LgtKind::Error || listed) && thrown_error(machine, env, want.detail, listed)) {
      end.pass = true;
      return end;
    }
    return lgt_failed_step(machine, LgtStep{false, env.stop});
  }
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
