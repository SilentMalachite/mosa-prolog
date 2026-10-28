#pragma once

// 翻訳器の内部。emit.cpp（単位と述語と節の選び方）、emit_head.cpp（節の変数、頭部、項の組み立て）、
// emit_body.cpp（本体の連言と継続の状態）が共有する。生成コードの名前は native_units.cpp の手書きに合わせる。

#include "../machine/internal.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mosa::codegen {

// 単位の中で振る番号。r.atoms[i] の i と call_goal の number になる（ADR 0009）。
struct UnitTables {
  std::vector<std::uint32_t> atoms;
  std::vector<PredKey> calls;

  [[nodiscard]] std::size_t atom(std::uint32_t id);
  [[nodiscard]] std::size_t call(const PredKey& key);
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

// 本体を平らにした連言。goals は呼ぶ順で、fails は最後の goals のあとで失敗する（fail/0 で終わる）。
struct BodyPlan {
  std::vector<const IrTerm*> goals;
  bool fails = false;
};

// 節の関数の名前の元。proc は生成関数（p_名前_アリティ）、clause は 1 から数える節の番号。
struct ClauseName {
  std::string proc;
  std::uint32_t clause = 0;
};

// 生成した節の関数と、述語の switch に入れる状態ごとの文。
struct ClauseCode {
  std::string functions;
  std::vector<std::pair<std::uint32_t, std::string>> cases;
};

// 文字列の補助（emit.cpp）。
[[nodiscard]] std::string hex64(std::uint64_t value);
[[nodiscard]] std::string int_literal(std::int64_t value);
[[nodiscard]] std::string float_literal(double value);
[[nodiscard]] std::string string_literal(std::string_view text);
[[nodiscard]] std::string state_text(std::uint32_t clause, std::uint32_t goal);
[[nodiscard]] bool is_small_int(std::int64_t value);
[[nodiscard]] bool is_dot(const Impl& machine, const IrTerm& term);

// emit_head.cpp
void plan_slots(const Clause& clause, const BodyPlan& body, ClauseEmit& emit);
void emit_slot_cells(ClauseEmit& emit);
void emit_head(ClauseEmit& emit, const IrTerm& head);
[[nodiscard]] std::string emit_build(ClauseEmit& emit, const IrTerm& term);
[[nodiscard]] std::string var_name(std::uint16_t slot);

// emit_body.cpp
[[nodiscard]] std::optional<BodyPlan> plan_body(const IrTerm& body);
[[nodiscard]] ClauseCode emit_clause(const Impl& machine, UnitTables& tables, const Predicate& predicate,
                                     const ClauseName& name, const BodyPlan& plan);

}  // namespace mosa::codegen
