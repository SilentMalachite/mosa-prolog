#pragma once

// 翻訳器の内部。emit.cpp（単位と述語と節の選び方）、emit_head.cpp（節の変数、頭部、項の組み立て）、
// emit_body.cpp（本体の並びと再開の状態）、emit_control.cpp（制御構文）が共有する。
// 生成コードの名前は native_units.cpp の手書きに合わせる。

#include "../machine/internal.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace mosa::codegen {

// 呼び先 1 つ。名前はアトムの番号ではなく字面で持つ。call/1 や '.'/2 のように原文に無い呼び先も出すため
// （翻訳器は Impl を const で見るので intern できない）。
struct CalleeRef {
  std::uint32_t module = 0;
  std::string name;
  std::uint16_t arity = 0;
};

// 単位の中で振る番号。r.atoms[i] の i と call_goal の number になる（ADR 0009）。
struct UnitTables {
  std::vector<std::uint32_t> atoms;
  std::vector<CalleeRef> calls;

  [[nodiscard]] std::size_t atom(std::uint32_t id);
  [[nodiscard]] std::size_t call(std::uint32_t module, std::string_view name, std::uint16_t arity);
};

// 節の変数（スロット）ごとの状態。値は生成コードの局所変数 v<スロット> に入る。
struct SlotState {
  // 節の入口で put_ref で作る。頭部の引数そのものとして最初に出る変数は、引数のセルを値にして作らない。
  bool allocated = false;
  // 頭部で一度出て、v<スロット> がその値を持つ。
  bool seen = false;
  // まだ訳していない出現の数。0 になった変数には代入も宣言も出さない（未使用の警告を出さない）。
  int left = 0;
};

// 1 つの関数を書いている間の状態。
struct ClauseEmit {
  const Impl& machine;
  UnitTables& tables;
  std::vector<SlotState> slots;
  std::string out;
  std::size_t temp = 0;
  int depth = 1;

  void line(std::string_view text);
  [[nodiscard]] std::string fresh(std::string_view stem);
};

// 節の関数の名前の元。proc は生成関数（p名前_アリティ）、clause は 1 から数える節の番号。
struct ClauseName {
  std::string proc;
  std::uint32_t clause = 0;
};

// 生成した節の関数と、述語の switch に入れる状態ごとの文。
struct ClauseCode {
  std::string functions;
  std::vector<std::pair<std::uint32_t, std::string>> cases;
  // 使った再開点の番号の次（ClauseCtx::next_state）。
  std::uint32_t next_state = 1;
  // 再開点の番号が state_of の 16 ビットに入らないか、継続枠の変数が kFrameCells を超えた。呼び手は生成コードを捨てて
  // IR のまま動かす。
  bool oversized = false;
};

// 本体のゴール 1 つと、それを動かす文脈モジュール（ADR 0006）。M:G は G の並びを M で置く。
struct GoalRef {
  const IrTerm* term = nullptr;
  std::uint32_t module = 0;
};

// 本体の関数の中の、カットの境界と継続の C++ の式。節の入口では r.cut、再開の状態では
// rt::frame_cut(r, frame)、入れ子の構文の中（条件や \+ の中）では局所変数になる。
struct BodyEnv {
  std::string cut;
  std::string cont;
};

// rt::state_of は節の番号と再開点の番号を 16 ビットずつに詰める（ADR 0009）。入らない番号を出すと case が重なり、
// 生成した C++ が組み立てられないか、別の再開関数へ飛ぶ（Muse のレビューの中）。
inline constexpr std::uint32_t kStateField = 0xFFFF;

// 今の関数の中に出す入れ子（条件、素の ; の左、\+ と catch/3 のゴール、M:G のゴール）の深さの上限。これより深い節は
// IR のまま動かす（ADR 0011 の改訂 5。改訂 1 の自前の再帰の深さと同じ 64）。
inline constexpr int kInlineDepth = 64;

// 節の継続枠が運ぶ変数のセルの合計（並びの接尾ごとの生きた変数の数の和）の上限。継続枠はその後の本体が使う変数を
// すべて運ぶので、入れ子の 1 段ごとに新しい変数が増える本体（catch(_, _, catch(_, _, ...)) の _）は、深さの 2 乗の
// 生成コードと覚えになる（16,000 段で 6.9 GB の C++ と 23 GB のメモリ）。超えた節は出すのを止め、IR のまま動かす
// （ADR 0011 の改訂 5）。2^20 のセルの生成コードは数十 MB で、コンパイラが扱える大きさを既に超えている。
inline constexpr std::size_t kFrameCells = std::size_t{1} << 20;

// 再開の状態 1 つの中身。枠の変数の先頭 skip 個は制御用の整数セルで、prologue がそれを読む。
struct StateSpec {
  // 節が持つ並び（ClauseCtx::lists）の接尾。状態は待ち行列から出すので、出すまで並びが生きている必要がある。
  std::span<const GoalRef> goals;
  std::size_t skip = 0;
  std::string prologue;
  // 回復ゴールのように、カットの境界を状態に入った時点の選択点の高さにする。
  bool local_cut = false;
  // prologue のあと失敗する（\+ の中のゴールが成功したとき）。
  bool fails = false;
  // prologue が rt::cut_to で切る（-> の then）。外した後始末があれば goals を次の歩みへ回す（yield_after_cut）。
  bool prologue_cuts = false;
};

// 状態を共有する鍵のうち、並びの先頭の番地のほかの部分（ADR 0011 の改訂 5）。状態の関数は仕様の全体だけで決まる。
using StateRest = std::tuple<std::size_t, std::size_t, std::string, bool, bool, bool>;

// 番号を振ってまだ関数を出していない状態。
struct PendingState {
  StateSpec spec;
  std::uint32_t state = 0;
  std::vector<std::uint32_t> live;
};

// 1 つの節を訳す間に共有する。状態の番号は節の中の再開の点を 1 から順に数える。ADR 0009 の
// 「ゴール g のあと」は、入れ子の構文があるので「g 番目の再開の点」と読む。番号は生成関数の中だけで意味を持つ。
// 表はどれも ADR 0011 の改訂 5 のためにある。本体の長さや深さで C++ の呼び出しを重ねず、同じ物を 2 度作らない。
struct ClauseCtx {
  // 表が多いので、集成体の初期化で全部を並べずに済むよう、参照の 5 つだけを受ける（-Wmissing-field-initializers）。
  ClauseCtx(const Impl& machine_in, UnitTables& tables_in, const Predicate& predicate_in, ClauseName name_in,
            ClauseCode& code_in)
      : machine(machine_in), tables(tables_in), predicate(predicate_in), name(std::move(name_in)), code(code_in) {}

  const Impl& machine;
  UnitTables& tables;
  const Predicate& predicate;
  ClauseName name;
  ClauseCode& code;
  std::uint32_t next_state = 1;
  // \+ と M:G の中のゴール。IR はデータの形で持つので、ゴールの形に直した項をここに置いて指す。
  std::deque<IrTerm> owned;
  // データの形のゴール（\+ G と M:G の G）の元の節点ごとに、直した物（owned の中）。直せない形は nullptr。
  std::map<const IrTerm*, const IrTerm*> owned_of;
  // 平らにした並び。項とモジュールごとに 1 度だけ作る（flat）。状態の仕様と生きたスロットの覚えは、この中を指す。
  std::deque<std::vector<GoalRef>> lists;
  std::map<const IrTerm*, std::map<std::uint32_t, std::span<const GoalRef>>> flat;
  // 条件が呼び出し無しに fail で終わる構文の代わりに置く fail（1 つを共有する）。
  const IrTerm* fail_goal = nullptr;
  // 継続枠が始まりうる接尾（並びの先頭と、算術でないゴールの後）の先頭の番地ごとに、その接尾が使う変数（昇順）。並びは
  // 終わりから 1 度に覚え（filled に先頭の番地を置く）、list_of_end は接尾の終わりの番地からその並びの全体を引く。
  // live_cells は覚えた変数の数の和で、kFrameCells を超えたら oversized を立てて覚えるのを止める。
  std::map<const GoalRef*, std::vector<std::uint32_t>> live;
  std::map<const GoalRef*, std::span<const GoalRef>> list_of_end;
  std::set<const GoalRef*> filled;
  std::size_t live_cells = 0;
  bool oversized = false;
  // LiveAcc が見た変数の印。並びごとに節の変数の数だけ確保して 0 にすると、変数の多い節の枝の数だけ掛かった
  // （レビュー R-M11）。節ごとに 1 本持ち、並びごとに世代の番号を進める。LiveAcc は同時に 2 つ使わない。
  std::vector<std::uint32_t> seen_marks;
  std::uint32_t seen_generation = 0;
  // 状態の番号と枠が運ぶ変数。仕様の全体が同じ状態は 1 つにまとめる。
  std::map<const GoalRef*, std::map<StateRest, std::pair<std::uint32_t, std::vector<std::uint32_t>>>> states;
  // まだ関数を出していない状態。emit_clause が空になるまで出す。
  std::deque<PendingState> queue;
};

// 文字列の補助（emit.cpp）。
[[nodiscard]] std::string hex64(std::uint64_t value);
[[nodiscard]] std::string int_literal(std::int64_t value);
[[nodiscard]] std::string float_literal(double value);
[[nodiscard]] std::string string_literal(std::string_view text);
[[nodiscard]] std::string state_text(std::uint32_t clause, std::uint32_t goal);
[[nodiscard]] bool is_small_int(std::int64_t value);
[[nodiscard]] bool is_dot(const Impl& machine, const IrTerm& term);
[[nodiscard]] bool is_name(const Impl& machine, const IrTerm& term, std::string_view name, std::size_t arity);

// 項を前順にたどり、visit(項, 深さ) が true を返した項だけ引数へ進む。深さは根を 0 とする。長いリストの原文は
// '.'/2 の尾が何万段も入れ子になるので、C++ の呼び出しを積まず手で積む（native.cpp の hash_term と同じ理由）。
template <typename Visit>
void walk_term(const IrTerm& root, Visit&& visit) {
  std::vector<std::pair<const IrTerm*, int>> pending{{&root, 0}};
  while (!pending.empty()) {
    const auto [term, depth] = pending.back();
    pending.pop_back();
    if (!visit(*term, depth)) continue;
    for (auto it = term->args.rbegin(); it != term->args.rend(); ++it) pending.emplace_back(&*it, depth + 1);
  }
}

// emit_head.cpp。body_counts[slot] は本体に出る回数。
void plan_slots(const Clause& clause, const std::vector<int>& body_counts, ClauseEmit& emit);
void emit_slot_cells(ClauseEmit& emit);
void emit_head(ClauseEmit& emit, const IrTerm& head, const Predicate& predicate);
[[nodiscard]] std::string emit_build(ClauseEmit& emit, const IrTerm& term);
[[nodiscard]] std::string var_name(std::uint32_t slot);

// emit_walk.cpp。本体を判定し、平らにし、変数を数える歩き。どれも手で積むスタックで動く（ADR 0011 の改訂 5）。
// 本体の全部を生成コードで動かせるか。動かせない節は interpret_clause に回す。
[[nodiscard]] bool translatable_body(const Impl& machine, const IrTerm& body);
// 今の関数の中に出す入れ子が kInlineDepth 段に収まるか。収まらない節も interpret_clause に回す。
[[nodiscard]] bool inline_nesting_fits(const Impl& machine, const IrTerm& body);
// compile.cpp の Build::goal と同じ変換。データの形の項（\+ の引数、M:G の G）をゴールの形にする。
[[nodiscard]] IrTerm goal_view(const Impl& machine, const IrTerm& data);
// 連言を呼ぶ順に並べた並び（ctx.lists の中）。true は飛ばし、fail と throw のあとは届かないので切る。
[[nodiscard]] std::span<const GoalRef> flat_list(ClauseCtx& ctx, const IrTerm& term, std::uint32_t module);
// body が呼び出しを 1 つも経ずに終わるなら、その fail か throw のゴール（! だけを挟んでよい）。他は nullptr。
// そのような並びは継続を使わないので、成功の側の継続枠を作ると未使用の変数で生成コードが通らない。
[[nodiscard]] const IrTerm* dead_end(ClauseCtx& ctx, const IrTerm& body, std::uint32_t module);
// \+ の中を生成コードに展開できるなら、ゴールの形に直した中身。組み込みの \+/1 に任せる形は nullptr。
[[nodiscard]] const IrTerm* negated_goal(ClauseCtx& ctx, const IrTerm& term);
// goals が使う変数を、出現の数で数える（節の入口の変数の計画）。
void count_body(ClauseCtx& ctx, std::span<const GoalRef> goals, std::vector<int>& counts);
// goals（ctx.lists の中の並びの接尾）が使う変数を昇順に。継続枠が運ぶ変数で、接尾ごとに 1 度だけ求める。
[[nodiscard]] const std::vector<std::uint32_t>& live_of(ClauseCtx& ctx, std::span<const GoalRef> goals);

// emit_body.cpp
// is/2 と算術比較か。発出はその場で動かして同じ関数で続けるので、その後ろからは継続枠が始まらない（live_of）。
[[nodiscard]] bool arith_goal(const Impl& machine, const IrTerm& goal);
// goals のあとに動くものを置く状態の継続の式。goals が空なら env.cont をそのまま返す。
[[nodiscard]] std::string make_rest(ClauseCtx& ctx, ClauseEmit& emit, std::span<const GoalRef> rest,
                                    const BodyEnv& env);
// spec の状態の関数を出し、その状態から再開する継続枠を作る文を置いて、枠の式（局所変数）を返す。
// prefix は枠の変数の先頭に置くセルの式（spec.skip 個）。
[[nodiscard]] std::string make_frame(ClauseCtx& ctx, ClauseEmit& emit, std::string_view stem, const StateSpec& spec,
                                     const std::string& cut, const std::string& cont,
                                     const std::vector<std::string>& prefix);
// 整数セルの式。選択点の高さなどを枠の変数として運ぶ。
[[nodiscard]] std::string mark_cell(std::string_view value);
// 生成した文面が識別子 ident を使うか。使わない局所変数や引数を出さないために見る（警告を誤りにしてある）。
[[nodiscard]] bool uses_identifier(std::string_view text, std::string_view ident);
void emit_seq(ClauseCtx& ctx, ClauseEmit& emit, std::span<const GoalRef> goals, const BodyEnv& env);
// カットの後。外した後始末があれば rest を次の歩みへ回す文を出す（ADR 0016）。
void yield_after_cut(ClauseCtx& ctx, ClauseEmit& emit, std::span<const GoalRef> rest, const BodyEnv& env);
[[nodiscard]] ClauseCode emit_clause(const Impl& machine, UnitTables& tables, const Predicate& predicate,
                                     const ClauseName& name);

// emit_control.cpp。どれも rest（構文のあとのゴール）まで出し切り、呼び手は戻るだけ。
void emit_or(ClauseCtx& ctx, ClauseEmit& emit, const GoalRef& goal, std::span<const GoalRef> rest,
             const BodyEnv& env);
void emit_if_then(ClauseCtx& ctx, ClauseEmit& emit, const GoalRef& goal, std::span<const GoalRef> rest,
                  const BodyEnv& env);
void emit_soft(ClauseCtx& ctx, ClauseEmit& emit, const GoalRef& goal, std::span<const GoalRef> rest,
               const BodyEnv& env);
// \+ A。inner はゴールの形に直した A。
void emit_not(ClauseCtx& ctx, ClauseEmit& emit, const IrTerm& inner, std::uint32_t module,
              std::span<const GoalRef> rest, const BodyEnv& env);
void emit_catch(ClauseCtx& ctx, ClauseEmit& emit, const GoalRef& goal, std::span<const GoalRef> rest,
                const BodyEnv& env);

}  // namespace mosa::codegen
