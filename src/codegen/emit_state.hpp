#pragma once

// 翻訳器の内部。emit.cpp（単位と述語と節の選び方）、emit_head.cpp（節の変数、頭部、項の組み立て）、
// emit_body.cpp（本体の並びと再開の状態）、emit_control.cpp（制御構文）が共有する。
// 生成コードの名前は native_units.cpp の手書きに合わせる。

#include "../machine/internal.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <span>
#include <string>
#include <string_view>
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

// 再開の状態 1 つの中身。枠の変数の先頭 skip 個は制御用の整数セルで、prologue がそれを読む。
struct StateSpec {
  std::vector<GoalRef> goals;
  std::size_t skip = 0;
  std::string prologue;
  // 回復ゴールのように、カットの境界を状態に入った時点の選択点の高さにする。
  bool local_cut = false;
  // prologue のあと失敗する（\+ の中のゴールが成功したとき）。
  bool fails = false;
};

// 1 つの節を訳す間に共有する。状態の番号は節の中の再開の点を 1 から順に数える。ADR 0009 の
// 「ゴール g のあと」は、入れ子の構文があるので「g 番目の再開の点」と読む。番号は生成関数の中だけで意味を持つ。
struct ClauseCtx {
  const Impl& machine;
  UnitTables& tables;
  const Predicate& predicate;
  ClauseName name;
  ClauseCode& code;
  std::uint32_t next_state = 1;
  // \+ と M:G の中のゴール。IR はデータの形で持つので、ゴールの形に直した項をここに置いて指す。
  std::deque<IrTerm> owned;
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

// emit_head.cpp。body_counts[slot] は本体に出る回数。
void plan_slots(const Clause& clause, const std::vector<int>& body_counts, ClauseEmit& emit);
void emit_slot_cells(ClauseEmit& emit);
void emit_head(ClauseEmit& emit, const IrTerm& head);
[[nodiscard]] std::string emit_build(ClauseEmit& emit, const IrTerm& term);
[[nodiscard]] std::string var_name(std::uint16_t slot);

// emit_body.cpp
// 本体の全部を生成コードで動かせるか。動かせない節は interpret_clause に回す。
[[nodiscard]] bool translatable_body(const Impl& machine, const IrTerm& body);
// compile.cpp の Build::goal と同じ変換。データの形の項（\+ の引数、M:G の G）をゴールの形にする。
[[nodiscard]] IrTerm goal_view(const Impl& machine, const IrTerm& data);
// 連言を呼ぶ順に並べる。true は飛ばし、fail と throw のあとは届かないので切る。
void flatten(ClauseCtx& ctx, const IrTerm& term, std::uint32_t module, std::vector<GoalRef>& out);
// goals が使う変数を、発出と同じ順に数える。
void count_body(ClauseCtx& ctx, std::span<const GoalRef> goals, std::vector<int>& counts);
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
[[nodiscard]] ClauseCode emit_clause(const Impl& machine, UnitTables& tables, const Predicate& predicate,
                                     const ClauseName& name);

// emit_control.cpp。どれも rest（構文のあとのゴール）まで出し切り、呼び手は戻るだけ。
// 構文の枝（連言）が使う変数を数える。
void count_branch(ClauseCtx& ctx, const IrTerm& branch, std::uint32_t module, std::vector<int>& counts);
void emit_or(ClauseCtx& ctx, ClauseEmit& emit, const GoalRef& goal, std::span<const GoalRef> rest,
             const BodyEnv& env);
void emit_if_then(ClauseCtx& ctx, ClauseEmit& emit, const GoalRef& goal, std::span<const GoalRef> rest,
                  const BodyEnv& env);

}  // namespace mosa::codegen
