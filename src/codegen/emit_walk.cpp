// ADR 0011 の改訂 5（レビュー 2026-11-20 の H-30）。節の本体を判定し、平らにし、変数を数える歩き。前は本体の入れ子と
// 連言の 1 段ごとに C++ の関数を呼び、1 MiB のスタックでは 800 段で落ちた。どれも手で積むスタックで動く。
// 呼び出しが重なるのは、条件の中の fail を調べる dead_end が flat_list を呼ぶ所だけで、その深さは今の関数の中に出す
// 入れ子の深さと同じなので、inline_nesting_fits が kInlineDepth 段で抑える（深い節は IR のまま動かす）。
// 平らにした並びと、データの形のゴールを直した物は、節の中で 1 度だけ作る。前は使うたびに作り直したので、入れ子の \+
// と M:G の翻訳が段の数の 3 乗と 2 乗の時間になった。

#include "emit_state.hpp"

#include "../module/module.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace mosa::codegen {
namespace {

// 2 引数の制御構文の名前（, ; -> *->）なら、そのゴールの形の印。データの形（\+ の中、M:G の G）にだけ現れる。
std::optional<IrTag> control_name(std::string_view name) {
  if (name == ",") return IrTag::And;
  if (name == ";") return IrTag::Or;
  if (name == "->") return IrTag::IfThen;
  if (name == "*->") return IrTag::SoftIf;
  return std::nullopt;
}

// 項の構文の印。ゴールの形の印はそのまま、データの形の , ; -> *-> と catch/3 はゴールの形の印にする。
// 引数の数が合わない物は構文として扱わない。
IrTag construct_tag(const Impl& machine, const IrTerm& term) {
  switch (term.tag) {
    case IrTag::And:
    case IrTag::Or:
    case IrTag::IfThen:
    case IrTag::SoftIf: return term.args.size() == 2 ? term.tag : IrTag::Fun;
    case IrTag::Catch: return term.args.size() == 3 ? term.tag : IrTag::Fun;
    case IrTag::Fun: break;
    default: return term.tag;
  }
  const auto name = machine.atoms.name(term.atom);
  if (term.args.size() == 2) return control_name(name).value_or(IrTag::Fun);
  return term.args.size() == 3 && name == "catch" ? IrTag::Catch : IrTag::Fun;
}

// data を goal の形にして into に置く。直す下位の項は pending に積み、呼び手のループが続ける（goal_view）。
void view_node(const Impl& machine, const IrTerm& data, IrTerm& into,
               std::vector<std::pair<const IrTerm*, IrTerm*>>& pending) {
  if (data.tag == IrTag::Atom) {
    const auto name = machine.atoms.name(data.atom);
    if (name == "true") into.tag = IrTag::True;
    else if (name == "fail" || name == "false") into.tag = IrTag::Fail;
    else if (name == "!") into.tag = IrTag::Cut;
    else if (name == "halt") into.tag = IrTag::Halt;
    else into = data;
    return;
  }
  if (data.tag != IrTag::Fun) {
    into = data;
    return;
  }
  const auto name = machine.atoms.name(data.atom);
  const std::size_t arity = data.args.size();
  const auto control = arity == 2 ? control_name(name) : std::nullopt;
  if (control || (arity == 3 && name == "catch")) {
    // 引数の場所を先に作ってから指す。後で足すと前の要素の番地が動く。
    into.tag = control.value_or(IrTag::Catch);
    into.args.resize(arity);
    if (arity == 3) into.args[1] = data.args[1];
    pending.emplace_back(&data.args[arity - 1], &into.args[arity - 1]);
    pending.emplace_back(&data.args[0], &into.args[0]);
  } else if (arity == 1 && (name == "throw" || name == "halt")) {
    into.tag = name == "throw" ? IrTag::Throw : IrTag::Halt;
    into.args.push_back(data.args[0]);
  } else {
    into = data;
  }
}

// ADR 0014。データのゴールは組み込みが動くときに、そのときの束縛で訳される。goal_view がゴールとして訳す位置
// （, ; -> *-> の下と catch/3 の Goal と Recovery）に変数があれば、そのとき ! になりうるので先に訳さない。
bool goal_var_below(const Impl& machine, const IrTerm& root) {
  std::vector<const IrTerm*> pending{&root};
  while (!pending.empty()) {
    const IrTerm* term = pending.back();
    pending.pop_back();
    if (term->tag == IrTag::Var) return true;
    if (term->tag != IrTag::Fun) continue;
    const auto name = machine.atoms.name(term->atom);
    const std::size_t arity = term->args.size();
    if (arity == 2 && control_name(name)) {
      pending.push_back(&term->args[1]);
      pending.push_back(&term->args[0]);
    } else if (arity == 3 && name == "catch") {
      pending.push_back(&term->args[2]);
      pending.push_back(&term->args[0]);
    }
  }
  return false;
}

// データの形で持つゴール（\+ の中、M:G の G）をゴールの形に直して ctx.owned に置く。変数や数や、(true, 1) のように
// 動かせないゴールを含む形は、呼び出しのときに組み込みが確かめる（instantiation_error、type_error）ので nullptr。
// 元の節点ごとに 1 度だけ直す。前は使うたびに部分木を写した。
const IrTerm* owned_goal(ClauseCtx& ctx, const IrTerm& data) {
  if (const auto found = ctx.owned_of.find(&data); found != ctx.owned_of.end()) return found->second;
  const IrTerm* made = nullptr;
  if ((data.tag == IrTag::Atom || data.tag == IrTag::Fun) && !goal_var_below(ctx.machine, data)) {
    IrTerm goal = goal_view(ctx.machine, data);
    if (translatable_body(ctx.machine, goal)) {
      ctx.owned.push_back(std::move(goal));
      made = &ctx.owned.back();
    }
  }
  ctx.owned_of.emplace(&data, made);
  return made;
}

// 実行ファイルが起動時に読むモジュールか。翻訳の機械はプログラムの後に文面の表とゴールのライブラリも読むが、
// 実行ファイルはそれを起動時に読まない（Impl::startup_modules）。
bool startup_module(const Impl& machine, std::uint32_t name) {
  if (find_module(machine, name) == nullptr) return false;
  const auto& known = machine.startup_modules;
  return !known || std::find(known->begin(), known->end(), name) != known->end();
}

// M:G の G。M が起動時に読み込み済みのモジュールのときだけ。無いモジュール、変数の M、後で読むモジュールは、
// 組み込みの :/2 が呼び出しのときに確かめる（mosa と同じ existence_error(module, M) など）。
const IrTerm* qualified_goal(ClauseCtx& ctx, const IrTerm& term) {
  if (!is_name(ctx.machine, term, ":", 2) || term.args[0].tag != IrTag::Atom) return nullptr;
  if (!startup_module(ctx.machine, term.args[0].atom)) return nullptr;
  return owned_goal(ctx, term.args[1]);
}

// 構文の「条件」。-> と *-> の左、それを左に持つ選言の左の左、\+ の中、catch のゴール。他は nullptr。
const IrTerm* condition_of(ClauseCtx& ctx, const IrTerm& term) {
  const IrTerm* construct = &term;
  if (term.tag == IrTag::Or && term.args.size() == 2) construct = &term.args[0];
  if ((construct->tag == IrTag::IfThen || construct->tag == IrTag::SoftIf) && construct->args.size() == 2) {
    return &construct->args[0];
  }
  if (term.tag == IrTag::Catch && term.args.size() == 3) return &term.args[0];
  return negated_goal(ctx, term);
}

const IrTerm* shared_fail(ClauseCtx& ctx) {
  if (ctx.fail_goal == nullptr) {
    // 指示付き初期化は GCC 13 と Clang 17 が -Wmissing-field-initializers を出すので、既定で作ってから tag を替える。
    IrTerm fail;
    fail.tag = IrTag::Fail;
    ctx.owned.push_back(std::move(fail));
    ctx.fail_goal = &ctx.owned.back();
  }
  return ctx.fail_goal;
}

// root を呼ぶ順に並べて out に足す。連言は右の枝を先に積み、左から取り出す。
void flatten_into(ClauseCtx& ctx, const IrTerm& root, std::uint32_t root_module, std::vector<GoalRef>& out) {
  std::vector<GoalRef> pending{GoalRef{&root, root_module}};
  while (!pending.empty()) {
    // fail と throw のあとに届かないゴールは、変数を数える側も発出も見ない。見ると使わない変数を宣言してしまう。
    if (!out.empty() && (out.back().term->tag == IrTag::Fail || out.back().term->tag == IrTag::Throw)) return;
    const GoalRef item = pending.back();
    pending.pop_back();
    const IrTerm& term = *item.term;
    if (term.tag == IrTag::True) continue;
    if (term.tag == IrTag::And && term.args.size() == 2) {
      pending.push_back(GoalRef{&term.args[1], item.module});
      pending.push_back(GoalRef{&term.args[0], item.module});
      continue;
    }
    // M:G は G の並びを M の文脈で置く（run_qualify、ADR 0006）。カットに透明なので境界はそのまま。
    if (const IrTerm* inner = qualified_goal(ctx, term); inner != nullptr) {
      pending.push_back(GoalRef{inner, term.args[0].atom});
      continue;
    }
    // 条件が呼び出し無しに終わる構文は、条件の中のカットが条件に閉じるので（7.8.7、7.8.8、8.15.1）、throw で終われば
    // その throw、fail で終われば else があればその枝、\+ なら true、他は fail と同じ。数える側と発出が同じ並びを
    // 見るように、ここで置き換える。
    const IrTerm* condition = condition_of(ctx, term);
    const IrTerm* end = condition != nullptr ? dead_end(ctx, *condition, item.module) : nullptr;
    // catch のゴールが throw で終わるのは例外を捕らえる普通の形。構文を残し、emit_catch が抜ける枠を作らない。
    if (end != nullptr && !(term.tag == IrTag::Catch && end->tag == IrTag::Throw)) {
      if (end->tag == IrTag::Throw) {
        out.push_back(GoalRef{end, item.module});
      } else if (term.tag == IrTag::Or) {
        pending.push_back(GoalRef{&term.args[1], item.module});
      } else if (term.tag != IrTag::Fun) {
        out.push_back(GoalRef{shared_fail(ctx), item.module});
      }
      continue;
    }
    out.push_back(item);
  }
}

// ゴール 1 つが変数を使う所。構文の枝（branch に平らにした並び）と、そのまま数える項（term）に分ける。
// count_body と live_of が同じ分け方で数える。
template <typename Branch, typename Term>
void goal_parts(ClauseCtx& ctx, const GoalRef& ref, Branch&& branch, Term&& term) {
  const IrTerm& goal = *ref.term;
  const auto flat = [&](const IrTerm& part) { branch(flat_list(ctx, part, ref.module)); };
  switch (goal.tag) {
    case IrTag::Fail:
    case IrTag::Cut: return;
    case IrTag::Or: {
      // (A -> B ; C) と (A *-> B ; C) は 3 つの枝。then と else の枠は構文の入口で作るので、全部を数える。
      const IrTerm& left = goal.args[0];
      if (left.tag == IrTag::IfThen || left.tag == IrTag::SoftIf) {
        flat(left.args[0]);
        flat(left.args[1]);
      } else {
        flat(left);
      }
      flat(goal.args[1]);
      return;
    }
    case IrTag::IfThen:
    case IrTag::SoftIf:
      flat(goal.args[0]);
      flat(goal.args[1]);
      return;
    case IrTag::Catch:
      flat(goal.args[0]);
      term(goal.args[1]);
      flat(goal.args[2]);
      return;
    default:
      if (const IrTerm* inner = negated_goal(ctx, goal); inner != nullptr) {
        flat(*inner);
      } else {
        term(goal);
      }
  }
}

std::size_t clause_vars(const ClauseCtx& ctx) { return ctx.predicate.clauses[ctx.name.clause - 1].vars; }

// 生きた変数を足していく入れ物。seen で重複を除くので、足す 1 つにつき定数の手間で済む。
struct LiveAcc {
  std::vector<std::uint32_t>* seen = nullptr;
  std::uint32_t generation = 0;
  std::vector<std::uint32_t> slots;

  void add(std::uint32_t slot) {
    if (slot >= seen->size() || (*seen)[slot] == generation) return;
    (*seen)[slot] = generation;
    slots.push_back(slot);
  }
};

// 新しい世代の LiveAcc。印の配列は節の最初の並びで 1 度だけ確保し、世代の番号が一回りしたときだけ 0 に戻す。
LiveAcc fresh_acc(ClauseCtx& ctx) {
  if (ctx.seen_marks.size() != clause_vars(ctx)) ctx.seen_marks.assign(clause_vars(ctx), 0);
  if (++ctx.seen_generation == 0) {
    std::fill(ctx.seen_marks.begin(), ctx.seen_marks.end(), 0);
    ctx.seen_generation = 1;
  }
  LiveAcc acc;
  acc.seen = &ctx.seen_marks;
  acc.generation = ctx.seen_generation;
  return acc;
}

// ref が使う変数を acc に足す。枝の並びの変数は、呼び手（fill_lists）が先に覚えてある。
void add_goal(ClauseCtx& ctx, const GoalRef& ref, LiveAcc& acc) {
  goal_parts(
      ctx, ref,
      [&](std::span<const GoalRef> branch) {
        if (branch.empty()) return;
        for (const std::uint32_t slot : ctx.live.at(branch.data())) acc.add(slot);
      },
      [&](const IrTerm& term) {
        walk_term(term, [&](const IrTerm& sub, int) {
          if (sub.tag == IrTag::Var) acc.add(sub.slot);
          return true;
        });
      });
}

// at から始まる接尾の変数として acc の写しを昇順で覚える。合計が上限を超えたら覚えずに oversized を立てる。
void remember(ClauseCtx& ctx, const GoalRef* at, const LiveAcc& acc) {
  ctx.live_cells += acc.slots.size();
  if (ctx.live_cells > kFrameCells) {
    ctx.oversized = true;
    return;
  }
  std::vector<std::uint32_t> sorted = acc.slots;
  std::sort(sorted.begin(), sorted.end());
  ctx.live.emplace(at, std::move(sorted));
}

// list の接尾のうち継続枠が始まりうる物（先頭と、算術でないゴールの後）の変数を、終わりから 1 度に覚える。
// 算術はその場で動いて同じ関数で続くので、その後ろの接尾は覚えない（g, X1 is 1, ..., Xn is n のような本体の覚えが
// n の 2 乗にならないように）。
void fill_list(ClauseCtx& ctx, std::span<const GoalRef> list) {
  LiveAcc acc = fresh_acc(ctx);
  for (std::size_t index = list.size(); index-- > 0;) {
    add_goal(ctx, list[index], acc);
    if (index == 0 || !arith_goal(ctx.machine, *list[index - 1].term)) remember(ctx, &list[index], acc);
    if (ctx.oversized) return;
  }
  ctx.filled.insert(list.data());
}

// root と、その中の構文の枝の並びを覚える。枝を先に覚えてから、その枝を持つ並びを覚える。枝の入れ子が深くても
// C++ の呼び出しを重ねない。
void fill_lists(ClauseCtx& ctx, std::span<const GoalRef> root) {
  std::vector<std::span<const GoalRef>> pending{root};
  while (!pending.empty() && !ctx.oversized) {
    const auto list = pending.back();
    if (ctx.filled.contains(list.data())) {
      pending.pop_back();
      continue;
    }
    bool waiting = false;
    for (const GoalRef& ref : list) {
      goal_parts(
          ctx, ref,
          [&](std::span<const GoalRef> branch) {
            if (branch.empty() || ctx.filled.contains(branch.data())) return;
            pending.push_back(branch);
            waiting = true;
          },
          [](const IrTerm&) {});
    }
    if (waiting) continue;
    pending.pop_back();
    fill_list(ctx, list);
  }
}

}  // namespace

// 機械は前の再帰が渡していただけで、印だけで決まる。呼び手の形は変えない。
bool translatable_body(const Impl& /*machine*/, const IrTerm& body) {
  std::vector<const IrTerm*> pending{&body};
  while (!pending.empty()) {
    const IrTerm& term = *pending.back();
    pending.pop_back();
    switch (term.tag) {
      case IrTag::True:
      case IrTag::Fail:
      case IrTag::Cut:
      case IrTag::Atom:
      case IrTag::Fun:
      case IrTag::Var:
      case IrTag::List:
      case IrTag::Throw:
      case IrTag::Call: continue;
      case IrTag::And:
      case IrTag::Or:
      case IrTag::IfThen:
      case IrTag::SoftIf:
        if (term.args.size() != 2) return false;
        pending.push_back(&term.args[1]);
        pending.push_back(&term.args[0]);
        continue;
      case IrTag::Catch:
        if (term.args.size() != 3) return false;
        pending.push_back(&term.args[2]);
        pending.push_back(&term.args[0]);
        continue;
      // 数はゴールにならない（7.6.2）。読み込みが本体の Big を断るので、ここへ来るのは \+ (true, Big) のようなデータの
      // ゴールだけで、組み込みの \+/1 が呼ぶときに型エラーにする。
      case IrTag::Big:
      default: return false;
    }
  }
  return true;
}

// 発出が今の関数の中で入れ子にする所（-> と *-> の条件、素の ; の左、\+ と catch/3 のゴール、M:G のゴール）で深さを
// 1 つ足す。then と else、回復のゴール、構文の後のゴールは待ち行列の状態になるが、条件が fail で終わると else は
// 今の並びに入るので、同じ深さのまま数える。データの形の構文も同じに数える（\+ と M:G の中は直してから出す）。
bool inline_nesting_fits(const Impl& machine, const IrTerm& body) {
  std::vector<std::pair<const IrTerm*, int>> pending{{&body, 0}};
  while (!pending.empty()) {
    const auto [term, depth] = pending.back();
    pending.pop_back();
    if (depth > kInlineDepth) return false;
    const auto& args = term->args;
    switch (construct_tag(machine, *term)) {
      case IrTag::And:
        pending.emplace_back(&args[1], depth);
        pending.emplace_back(&args[0], depth);
        break;
      case IrTag::Or: {
        const IrTerm& left = args[0];
        const IrTag inner = construct_tag(machine, left);
        if (inner == IrTag::IfThen || inner == IrTag::SoftIf) {
          pending.emplace_back(&left.args[1], depth);
          pending.emplace_back(&left.args[0], depth + 1);
        } else {
          pending.emplace_back(&left, depth + 1);
        }
        pending.emplace_back(&args[1], depth);
        break;
      }
      case IrTag::IfThen:
      case IrTag::SoftIf:
        pending.emplace_back(&args[1], depth);
        pending.emplace_back(&args[0], depth + 1);
        break;
      case IrTag::Catch:
        pending.emplace_back(&args[2], depth);
        pending.emplace_back(&args[0], depth + 1);
        break;
      case IrTag::Fun:
        if (is_name(machine, *term, "\\+", 1)) pending.emplace_back(&args[0], depth + 1);
        if (is_name(machine, *term, ":", 2)) pending.emplace_back(&args[1], depth + 1);
        break;
      default: break;
    }
  }
  return true;
}

IrTerm goal_view(const Impl& machine, const IrTerm& data) {
  IrTerm root;
  std::vector<std::pair<const IrTerm*, IrTerm*>> pending{{&data, &root}};
  while (!pending.empty()) {
    const auto [from, into] = pending.back();
    pending.pop_back();
    view_node(machine, *from, *into, pending);
  }
  return root;
}

std::span<const GoalRef> flat_list(ClauseCtx& ctx, const IrTerm& term, std::uint32_t module) {
  // std::map の要素は後から足しても動かないので、平らにする間に別の項の並びを足しても by_module は指したまま。
  auto& by_module = ctx.flat[&term];
  if (const auto found = by_module.find(module); found != by_module.end()) return found->second;
  std::vector<GoalRef> goals;
  flatten_into(ctx, term, module, goals);
  ctx.lists.push_back(std::move(goals));
  const std::span<const GoalRef> list{ctx.lists.back()};
  by_module.emplace(module, list);
  // 接尾は並びと終わりの番地が同じなので、live_of はそこから並びの全体を引く。
  if (!list.empty()) ctx.list_of_end.emplace(list.data() + list.size(), list);
  return list;
}

const IrTerm* dead_end(ClauseCtx& ctx, const IrTerm& body, std::uint32_t module) {
  for (const GoalRef& ref : flat_list(ctx, body, module)) {
    if (ref.term->tag == IrTag::Fail || ref.term->tag == IrTag::Throw) return ref.term;
    if (ref.term->tag != IrTag::Cut) return nullptr;
  }
  return nullptr;
}

const IrTerm* negated_goal(ClauseCtx& ctx, const IrTerm& term) {
  return is_name(ctx.machine, term, "\\+", 1) ? owned_goal(ctx, term.args[0]) : nullptr;
}

void count_body(ClauseCtx& ctx, std::span<const GoalRef> goals, std::vector<int>& counts) {
  std::vector<std::span<const GoalRef>> pending{goals};
  while (!pending.empty()) {
    const auto list = pending.back();
    pending.pop_back();
    for (const GoalRef& ref : list) {
      goal_parts(
          ctx, ref, [&](std::span<const GoalRef> branch) { pending.push_back(branch); },
          [&](const IrTerm& term) {
            walk_term(term, [&](const IrTerm& sub, int) {
              if (sub.tag == IrTag::Var && sub.slot < counts.size()) counts[sub.slot] += 1;
              return true;
            });
          });
    }
  }
}

const std::vector<std::uint32_t>& live_of(ClauseCtx& ctx, std::span<const GoalRef> goals) {
  static const std::vector<std::uint32_t> none;
  // 上限を超えた節の生成コードは捨てるので、空で答えて覚えるのを止める。
  if (goals.empty() || ctx.oversized) return none;
  if (const auto found = ctx.live.find(goals.data()); found != ctx.live.end()) return found->second;
  fill_lists(ctx, ctx.list_of_end.at(goals.data() + goals.size()));
  if (ctx.oversized) return none;
  if (const auto found = ctx.live.find(goals.data()); found != ctx.live.end()) return found->second;
  // 継続枠が始まらないはずの接尾（算術の後）を求められた。発出と覚え方がずれても誤らないよう、次に覚えた接尾まで
  // 足して覚える。
  std::size_t next = 1;
  while (next < goals.size() && !ctx.live.contains(&goals[next])) ++next;
  LiveAcc acc = fresh_acc(ctx);
  if (next < goals.size()) {
    for (const std::uint32_t slot : ctx.live.at(&goals[next])) acc.add(slot);
  }
  for (std::size_t index = next; index-- > 0;) add_goal(ctx, goals[index], acc);
  remember(ctx, goals.data(), acc);
  return ctx.oversized ? none : ctx.live.at(goals.data());
}

}  // namespace mosa::codegen
