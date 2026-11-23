// 本体は機械で実行する。特殊な呼び出しだけを関数子で拾う。文字列の部分一致では分けない。
// assertion の失敗は、結果が false でも合格にしない。選言のもう片方は試す。
// 本体の項を木にするのは lgt_build.cpp。

#include "lgt_prog.hpp"

#include "lgt_io.hpp"
#include "internal.hpp"

#include "mosa/writer.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mosa {
namespace {

using NodeKind = LgtNodeKind;
using Node = LgtNode;
using Prog = LgtProg;

// Throw は例外か halt で止まったこと。どちらかは env.stop が持つ。halt を No にすると、false と all の試験が
// 失敗と数えて通る（レビュー H-36）。
enum class Tri { Ok, No, Abort, Throw, Dead };

// 成功は呼び手が先に見る。失敗だけが No で、ほかの止まり方は Throw。
Tri stopped(StopKind kind) { return kind == StopKind::Failure ? Tri::No : Tri::Throw; }

Tri scratch_tri(LgtScratchKind kind) {
  switch (kind) {
    case LgtScratchKind::Ok: return Tri::Ok;
    case LgtScratchKind::No: return Tri::No;
    case LgtScratchKind::Stopped: return Tri::Throw;
    case LgtScratchKind::Dead: return Tri::Dead;
  }
  return Tri::Dead;
}

// 名前と値の字面。cleanup(Goal) に貼る束縛（LgtEnd::kept）も同じ形で返す。
using Saved = LgtValue;

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
  // ^^ が束縛した変数の名前。本体の最後に saved の字面から読み直す。
  std::vector<std::string> minted;
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
WriteCut paste_saved(void* context, Cell term, std::string& out, WriteHeld& /*held*/) {
  const auto* text = static_cast<const SavedText*>(context);
  if (text == nullptr || text->machine == nullptr || term.tag() != Tag::Ref) return WriteCut::Keep;
  for (const VarName& var : *text->names) {
    if (var.cell.raw() != term.raw()) continue;
    const Saved* slot = find_saved(*text->saved, var.name);
    if (slot == nullptr) return WriteCut::Keep;
    // 未束縛のまま 1 度しか出ない変数は _ と写る。(_) を貼ると名前の変数と別の変数になり、後のゴールの束縛が名前の
    // 変数に届かない（レビュー R-M17）。名前のまま残す。別名にした変数は _A のような名前で写るので、ここに来ない。
    if (slot->term == "_") return WriteCut::Keep;
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
  return write_term(machine.heap(), machine.operators(), parsed.term, lgt_write_options(), parsed.names, paste_saved, &text);
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
                     write_term(machine.heap(), machine.operators(), args[index], lgt_write_options(), parsed.names));
    }
    return;
  }
  for (const Binding& binding : stop.bindings) {
    remember_saved(env.saved, binding.name, write_term(machine.heap(), machine.operators(), binding.variable, lgt_write_options(), {}));
  }
}

// 後の問いの束縛は、その問いに出た変数だけ。前の問いで束縛した変数は字面で貼ったので出てこない。後の判定と、解を
// 開いたままスクラッチで流す問いが見るよう、写しから足す（レビュー R-L19）。前は X = 1 の X が後の variant/2 から消えた。
void remember_all(Machine& machine, const Stop& stop, Env& env) {
  remember_stop(machine, stop, env);
  restore_saved(machine, env);
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
  env.bundle = write_term(machine.heap(), machine.operators(), tail, lgt_write_options(), {});
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
    const LgtScratch ran = lgt_scratch_run(machine, node.text, env.bindings);
    env.tri = scratch_tri(ran.kind);
    if (ran.kind == LgtScratchKind::Stopped) env.stop = ran.stop;
    return;
  }
  const std::string text = goal_with_saved(machine, node.text, env.saved);
  snapshot_bundle(machine, env);
  const LgtStep step = lgt_ask(machine, text + full_stop_after(text));
  env.stop = step.stop;
  if (step.dead) {
    env.tri = Tri::Dead;
    return;
  }
  if (step.stop.kind == StopKind::Success) {
    remember_all(machine, step.stop, env);
    env.tri = Tri::Ok;
    return;
  }
  env.live = false;
  env.tri = stopped(step.stop.kind);
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
  const bool live = env.live;
  exec_at(machine, prog, node.left, env);
  if (env.tri != Tri::Ok) return;
  // 選択点を残すと then の失敗で親が条件をやり直す。成功した解だけで then を実行する。外の開いた query（前の
  // member/2 や forall/2 の生成側）は切らない。前は live を偽にしたので、then が主の機械で query を開いて外の query を
  // 置き換え、後のゴールが失敗しても外へ戻らず、forall/2 は残りの解を試さなかった（レビュー RV-I01）。
  env.live = live && env.live;
  exec_at(machine, prog, node.right, env);
}

void exec_soft(Machine& machine, const Prog& prog, int index, Env& env) {
  const Node& node = prog.nodes[static_cast<std::size_t>(index)];
  const bool live = env.live;
  exec_at(machine, prog, node.left, env);
  if (env.tri == Tri::Ok) {
    // 条件の成功で else を切る。then が失敗しても、この節点は else を持ったまま返さない。外の開いた query は
    // exec_if と同じく残す（レビュー RV-I01）。
    env.live = live && env.live;
    exec_at(machine, prog, node.right, env);
    return;
  }
  if (env.tri == Tri::Dead || env.tri == Tri::Throw) return;
  take_else(machine, prog, node.other, env.tri, env);
}

// 印の変数を束縛と写しから消す。写しに残ると、同じ節点を次に動かすときに goal_with_saved が前の値を貼り、
// (else) = then のような query になる（レビュー RV-I02）。
void forget_marker(Env& env, const std::string& marker) {
  std::erase_if(env.saved, [&](const Saved& slot) { return slot.name == marker; });
  std::erase_if(env.bindings, [&](const Binding& binding) { return binding.name == marker; });
}

bool marker_says_else(const Machine& machine, const Env& env, const std::string& marker) {
  const Heap& heap = machine.heap();
  for (const Binding& binding : env.bindings) {
    if (binding.name != marker) continue;
    const Cell value = heap.deref(binding.variable);
    return value.tag() == Tag::Atom && heap.atoms().name(static_cast<std::uint32_t>(value.raw() >> 3)) == "else";
  }
  return false;
}

// 開いた query があると、主の機械で問うとその query を置き換え、スクラッチは束縛を持ち帰らないので印を読めない。
// (C -> T) を試し、成り立たなければ C だけを試して、C も成り立たないときだけ else へ進む。スクラッチは毎回新しい
// 機械なので、C を 2 度動かしても前の副作用は残らない。
void scratch_end(const LgtScratch& ran, Env& env) {
  env.tri = scratch_tri(ran.kind);
  if (ran.kind == LgtScratchKind::Stopped) env.stop = ran.stop;
}

void pick_scratch(Machine& machine, const Prog& prog, const Node& node, Env& env) {
  const LgtScratch whole = lgt_scratch_run(machine, node.arrow, env.bindings);
  if (whole.kind != LgtScratchKind::No) return scratch_end(whole, env);
  const LgtScratch cond = lgt_scratch_run(machine, node.cond, env.bindings);
  if (cond.kind == LgtScratchKind::Ok) {
    env.tri = Tri::No;
    return;
  }
  if (cond.kind != LgtScratchKind::No) return scratch_end(cond, env);
  take_else(machine, prog, node.other, Tri::No, env);
}

// else にだけランナー目標がある (C -> T ; E)（lgt_build.cpp の build_pick）。印の付いた 1 つの query が成功し、印が
// else なら E を動かす。query が失敗したのは C が成り立って T が失敗したときなので、E へ進まない。
void exec_pick(Machine& machine, const Prog& prog, int index, Env& env) {
  const Node& node = prog.nodes[static_cast<std::size_t>(index)];
  if (env.live) {
    pick_scratch(machine, prog, node, env);
    return;
  }
  forget_marker(env, node.marker);
  exec_plain(machine, node, env);
  if (env.tri != Tri::Ok) return;
  const bool otherwise = marker_says_else(machine, env, node.marker);
  forget_marker(env, node.marker);
  if (otherwise) take_else(machine, prog, node.other, Tri::No, env);
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
      env.tri = stopped(step.stop.kind);
      return;
    }
    // 次の解で束縛の写しと live も書き直す。前は束縛だけを替え、字面の写しが最初の解のまま残った（レビュー M-49）。
    remember_all(machine, step.stop, env);
    exec_at(machine, prog, right, env);
  }
}

// 開いた query のやり直しは、その query を開いた連言だけが行う。後ろの連言がやり直すと、query より後でその連言
// より前に動いた目標（assertion など）を新しい解で動かし直さない（レビュー M-49）。owner は左の前に live でないこと。
void run_then(Machine& machine, const Prog& prog, int first, int rest, bool owner, Env& env) {
  exec_at(machine, prog, first, env);
  if (env.tri != Tri::Ok) return;
  exec_at(machine, prog, rest, env);
  if (env.tri != Tri::No || !owner) return;
  retry_right(machine, prog, rest, env);
}

void exec_disj_then(Machine& machine, const Prog& prog, int disj, int rest, Env& env);

// 枝がまた選言なら、その枝も後ろと組にして試す。(A ; B ; C) は A ; (B ; C) で、(B ; C) を run_then に渡すと B の
// 成功で exec_disj が返り、後ろが失敗しても C を試さなかった（レビュー R-M12）。左の (A ; B) ; C も同じ。
void run_branch(Machine& machine, const Prog& prog, int branch, int rest, bool owner, Env& env) {
  if (prog.nodes[static_cast<std::size_t>(branch)].kind == NodeKind::Disj) {
    exec_disj_then(machine, prog, branch, rest, env);
    return;
  }
  run_then(machine, prog, branch, rest, owner, env);
}

// (A ; B), C は、A の解で C を動かし、尽きたら B の解で C を動かす。前は exec_disj が A の成功で返り、C が失敗しても
// B へ戻らなかった（レビュー M-49）。B の前に、A の前の束縛を字面の写しから読み直す（exec_forall と同じ）。
// A の表明の失敗の後も B を試すのは、exec_disj の take_else と同じ。
void exec_disj_then(Machine& machine, const Prog& prog, int disj, int rest, Env& env) {
  const Node& node = prog.nodes[static_cast<std::size_t>(disj)];
  const bool live = env.live;
  snapshot_bundle(machine, env);
  const std::string bundle = env.bundle;
  const std::vector<std::string> names = env.bundle_names;
  const std::vector<Saved> saved = env.saved;
  run_branch(machine, prog, node.left, rest, !live, env);
  const Tri first = env.tri;
  if (first == Tri::Ok || first == Tri::Dead || first == Tri::Throw) return;
  const std::string note = env.note;
  // A の中の forall/2 は開いた query を置き換える（open-items）。尽きた query を live に戻して問い直さない。
  env.live = live && env.live;
  env.bundle = bundle;
  env.bundle_names = names;
  env.saved = saved;
  rehydrate_bindings(machine, env);
  if (env.tri == Tri::Dead) return;
  run_branch(machine, prog, node.right, rest, !live, env);
  if (env.tri == Tri::Ok || env.tri == Tri::Dead || env.tri == Tri::Throw) return;
  if (first == Tri::Abort || env.tri == Tri::Abort) {
    if (env.tri != Tri::Abort) env.note = note;
    env.tri = Tri::Abort;
    return;
  }
  env.tri = Tri::No;
}

void exec_conj(Machine& machine, const Prog& prog, int index, Env& env) {
  const int left = prog.nodes[static_cast<std::size_t>(index)].left;
  const int right = prog.nodes[static_cast<std::size_t>(index)].right;
  if (prog.nodes[static_cast<std::size_t>(left)].kind == NodeKind::Disj) {
    exec_disj_then(machine, prog, left, right, env);
    return;
  }
  run_then(machine, prog, left, right, !env.live, env);
}

// forall/2 は \+ (Cond, \+ Action) なので、生成側は前の束縛を見て動き、その束縛は Action の間だけ使う。
// 終わったら前の束縛へ戻す。戻さないと、生成側と同じ名前の前の束縛が未束縛のセルに隠れる（Muse のレビューの中）。
// 生成側の問い合わせはヒープを捨てるので、前の束縛は字面に写しておき、終わってから今のヒープへ読み直す
// （exec_plain と同じ）。入れ子の forall が写しを上書きしても戻せるよう、写しはここでも持つ。
void exec_forall(Machine& machine, const Prog& prog, int index, Env& env) {
  const int generator = prog.nodes[static_cast<std::size_t>(index)].left;
  const int action = prog.nodes[static_cast<std::size_t>(index)].right;
  // ランナー目標を含む生成側は解を並べられない。前は失敗にしたので、false の試験が偽合格した（レビュー M-49）。
  if (generator < 0 || prog.nodes[static_cast<std::size_t>(generator)].kind != NodeKind::Plain) {
    env.note = "unsupported";
    env.tri = Tri::Abort;
    return;
  }
  const std::string text = goal_with_saved(machine, prog.nodes[static_cast<std::size_t>(generator)].text, env.saved);
  snapshot_bundle(machine, env);
  const std::string bundle = env.bundle;
  const std::vector<std::string> names = env.bundle_names;
  LgtStep step = lgt_ask(machine, text + full_stop_after(text));
  while (true) {
    env.stop = step.stop;
    if (step.dead) {
      env.tri = Tri::Dead;
      return;
    }
    if (step.stop.kind == StopKind::Failure) {
      env.live = false;
      env.tri = Tri::Ok;
      env.bundle = bundle;
      env.bundle_names = names;
      rehydrate_bindings(machine, env);
      return;
    }
    if (step.stop.kind != StopKind::Success) {
      env.tri = stopped(step.stop.kind);
      return;
    }
    env.live = true;
    env.bindings = step.stop.bindings;
    restore_saved(machine, env);
    exec_at(machine, prog, action, env);
    if (env.tri != Tri::Ok) {
      // 失敗した forall に選択点は無い。開いたままだと、連言の retry_right がこの生成側を問い直し続けて止まらない。
      // 行動の表明が落ちた（Abort）ときも、選言の else は forall の前の束縛で動く。前は生成側の束縛と live を残し、
      // else をスクラッチで動かして束縛を失い、生成側の束縛を判定へ漏らした（レビュー R-M13）。
      if (env.tri == Tri::No || env.tri == Tri::Abort) {
        env.live = false;
        env.bundle = bundle;
        env.bundle_names = names;
        rehydrate_bindings(machine, env);
      }
      return;
    }
    step = lgt_again(machine);
  }
}

void set_binding(std::vector<Binding>& bindings, const std::string& name, Cell value) {
  for (Binding& binding : bindings) {
    if (binding.name != name) continue;
    binding.variable = value;
    return;
  }
  bindings.push_back(Binding{name, value});
}

// 値の入った出力引数を単一化すると、中の変数がいくつも束縛されうる。どれも同じく残す（レビュー M-54）。
void remember_super(Machine& machine, const LgtSuper& ran, Env& env) {
  for (const LgtBound& bound : ran.bindings) {
    // 次の query がヒープを捨てても、表明の項を文字列で残す。
    remember_saved(env.saved, bound.name,
                   write_term(machine.heap(), machine.operators(), bound.value, lgt_write_options(), {}));
    if (std::find(env.minted.begin(), env.minted.end(), bound.name) == env.minted.end()) {
      env.minted.push_back(bound.name);
    }
    set_binding(env.bindings, bound.name, bound.value);
  }
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
    case NodeKind::Pick: exec_pick(machine, prog, index, env); return;
    case NodeKind::Assert: exec_assert(machine, prog, index, env); return;
    case NodeKind::Forall: exec_forall(machine, prog, index, env); return;
    case NodeKind::Super: exec_super(machine, prog.nodes[static_cast<std::size_t>(index)], env); return;
    case NodeKind::Variant:
    case NodeKind::Subsumes:
    case NodeKind::Approx: exec_check(machine, prog.nodes[static_cast<std::size_t>(index)], env); return;
  }
}

// 字面は saved から読む。後戻りで saved を戻すと、外れた枝の ^^ の名前はそこから消えるので、読み直さない。
// 字面を名前と一緒に別に持つと、(^^file_path(foo, P), fail ; true) の後でも P が束縛されて戻った（レビュー M-54）。
void remint(Machine& machine, Env& env) {
  for (const std::string& name : env.minted) {
    const Saved* slot = find_saved(env.saved, name);
    if (slot == nullptr || slot->term.empty()) continue;
    const LgtParsed parsed = lgt_parse_term(machine, slot->term);
    if (!parsed.ok) continue;
    set_binding(env.bindings, name, parsed.term);
  }
}

bool error_ball(const Heap& heap, Cell ball, Cell formal) {
  ball = heap.deref(ball);
  const bool shaped = ball.tag() == Tag::Str && heap.functor_arity(ball) == 2 &&
                      heap.atoms().name(heap.functor_atom(ball)) == "error";
  if (!shaped) return false;
  // Context は比べない。許された Formal が実際の Formal を包摂すれば足りる。
  return lgt_subsumes(heap, formal, heap.arg(ball, 0));
}

bool expects_throw(LgtKind kind) { return kind == LgtKind::Error || kind == LgtKind::Errors || kind == LgtKind::Ball; }

// 次の query はヒープを捨てる。自由なままの Stream は別の犯人まで包摂するので、残した字面を戻す。
// errors/1 のときだけ列を歩く。error/1 の項がリストでも、候補の一つにはしない。
// ball/1 は error(Formal, _) に限らず、投げられた項そのものを包摂で比べる（judge_query の thrown_ok と同じ）。
bool thrown_expected(Machine& machine, const Env& env, const LgtWant& want) {
  // Tri::Throw は halt でも立つ。そのとき exception は空の番地なので、読むとプロセスが落ちる（レビュー C-04）。
  // 投げられた項が無ければ期待に当たらず、呼び手が halt(N) を実際として書く。
  if (env.stop.kind != StopKind::Exception) return false;
  const LgtParsed parsed = lgt_parse_term(machine, want.detail);
  if (!parsed.ok) return false;
  std::vector<std::uint64_t> stack;
  const Cell expected = swap_saved(machine, parsed.term, parsed.names, env.saved, stack);
  const Heap& heap = machine.heap();
  if (want.kind == LgtKind::Ball) return lgt_subsumes(heap, expected, env.stop.exception);
  if (want.kind == LgtKind::Error) return error_ball(heap, env.stop.exception, expected);
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
    if (expects_throw(want.kind) && thrown_expected(machine, env, want)) {
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
  // 投げるはずの本体が成功した。lgt_finish に回すと期待の項をゴールとして走らせ、ball(true) が通る（レビュー H-37）。
  if (expects_throw(want.kind)) {
    end.actual = "true";
    return end;
  }
  restore_saved(machine, env);
  remint(machine, env);
  if (want.kind == LgtKind::True) {
    end.pass = true;
    if (want.keep) end.kept = env.saved;
    return end;
  }
  Stop synthetic;
  synthetic.kind = StopKind::Success;
  synthetic.bindings = env.bindings;
  end = lgt_finish(machine, want, synthetic);
  // 本体の束縛は字面の写しにある。cleanup(Goal) に貼る（レビュー M-50）。exists が後の解で成り立ったなら、lgt_finish は
  // その解の束縛を kept に写してある。その写しは最後の query の変数だけなので、本体の写しに重ねる。前は本体の最初の
  // 解の写しで上書きし、cleanup が成り立たなかった解を見た（レビュー RV-m03）。
  if (want.keep) {
    std::vector<Saved> kept = env.saved;
    for (const Saved& slot : end.kept) remember_saved(kept, slot.name, slot.term);
    end.kept = std::move(kept);
  }
  return end;
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
  const int root = lgt_build_node(prog, machine.heap(), machine.operators(), parsed.term, parsed.names);
  Env env;
  env.io = io;
  exec_at(machine, prog, root, env);
  return from_env(machine, want, env);
}

std::vector<LgtValue> lgt_values_of(Machine& machine, const std::vector<Binding>& bindings) {
  Stop stop;
  stop.kind = StopKind::Success;
  stop.bindings = bindings;
  Env env;
  remember_stop(machine, stop, env);
  return env.saved;
}

std::string lgt_paste_values(Machine& machine, std::string_view goal, const std::vector<LgtValue>& values) {
  return goal_with_saved(machine, goal, values);
}

}  // namespace mosa
