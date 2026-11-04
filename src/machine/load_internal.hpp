#pragma once

// load.cpp と load_directive.cpp だけが共有する。読み込みの外からは使わない。
#include "internal.hpp"

#include "mosa/reader.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

namespace mosa {

struct Snapshot {
  // predicate が null なら、この読み込みが作る述語の印。在れば節を番号ごと写す（定義は load_clauses.cpp）。
  // taken_at は写した時の Impl::db_generation。
  Snapshot(PredKey key, const Predicate* predicate, std::uint64_t taken_at);
  // Predicate と同じく、写しの vector が伸びるたびに節を写さないよう移動を noexcept にする。
  Snapshot(const Snapshot&) = default;
  Snapshot& operator=(const Snapshot&) = default;
  Snapshot(Snapshot&&) noexcept = default;
  Snapshot& operator=(Snapshot&&) noexcept = default;
  ~Snapshot() = default;

  std::uint32_t module = 0;
  std::uint32_t name = 0;
  std::uint16_t arity = 0;
  ClauseList clauses;
  // 写した枠と定義（ADR 0017）。戻すのは同じ定義が残る枠だけで、テキストが始めた定義はその枠の最後の写しへ戻す。
  std::size_t serial = 0;
  std::size_t defined_at = 0;
  // 写した時の世代と Predicate::text_clause_at。後にテキストが節を足したかを比べ、丸ごと戻すときは印も戻す。
  std::uint64_t taken_at = 0;
  std::uint64_t text_clause_at = 0;
  bool defined_by_goal = false;
  bool created = false;
  bool dynamic = false;
  bool abolished = false;
  std::vector<MetaArg> meta;
};

static_assert(std::is_nothrow_move_constructible_v<Snapshot>);
static_assert(std::is_nothrow_move_assignable_v<Snapshot>);

// この読み込みの中だけで隙間を許す。述語ベクタには入れない。宣言は手続きを作らない。
struct Apart {
  std::uint32_t module = 0;
  std::uint32_t name = 0;
  std::uint16_t arity = 0;
};

void fail_load(Impl& machine, std::string message);
[[nodiscard]] bool allows_gap(const std::vector<Apart>& apart, PredKey key);
void note_declared(Impl& machine, PredKey key, bool shared);
// predicate は key を表で引いた結果（無ければ null）。
void remember_pred(const Impl& machine, std::vector<Snapshot>& snapshots, PredKey key, const Predicate* predicate);
[[nodiscard]] bool run_inits(Impl& machine, const std::vector<Cell>& goals);
// 定義は load_clauses.cpp。テキストに置けない節なら、assertz/1 がその節に出すのと同じ Formal を返す（レビュー M10）。
// term は module の修飾を外した節、clause はそれを訳したもの。
[[nodiscard]] std::optional<Cell> refused_clause(Impl& machine, Cell term, const Clause& clause);
// 定義は load_clauses.cpp。節は module に入り、足す前の述語を snapshots へ写す。last は直前に節を足した述語の名前。
[[nodiscard]] bool add_clause(Impl& machine, std::uint32_t module, Clause clause, std::vector<Snapshot>& snapshots,
                              std::optional<PredKey>& last, const std::vector<Apart>& apart);
// 失敗した読み込みの述語を写しへ戻し、この読み込みで作った述語（serial が previous_serial より後）を捨てる。
void rollback_preds(Impl& machine, const std::vector<Snapshot>& snapshots, std::size_t previous_serial);
// 成功した入れ子の読み込みの写しを親の写しへ足す。親が同じ定義を写していれば、親の古い写しを残す（ADR 0017）。
void hand_up_snapshots(std::vector<Snapshot>& parent, std::vector<Snapshot>& own);
// first は、この項がファイルの最初の項であること。module/2 はそこでしか受けない。
[[nodiscard]] int consume_directive(Impl& machine, Reader& reader, Cell term, std::vector<Snapshot>& snapshots,
                                    std::vector<Apart>& apart, std::vector<Cell>& inits, bool first,
                                    std::string_view origin);

}
