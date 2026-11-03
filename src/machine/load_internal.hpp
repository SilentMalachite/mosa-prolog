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
  Snapshot(PredKey key, const Predicate* predicate);
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
void remember_pred(Impl& machine, std::vector<Snapshot>& snapshots, PredKey key);
[[nodiscard]] bool run_inits(Impl& machine, const std::vector<Cell>& goals);
// 定義は load_clauses.cpp。テキストに置けない節なら、assertz/1 がその節に出すのと同じ Formal を返す（レビュー M10）。
// term は module の修飾を外した節、clause はそれを訳したもの。
[[nodiscard]] std::optional<Cell> refused_clause(Impl& machine, Cell term, const Clause& clause);
// 定義は load_clauses.cpp。節は module に入る。last は直前に節を足した述語の添字。
[[nodiscard]] bool add_clause(Impl& machine, std::uint32_t module, Clause clause, std::size_t& last,
                              const std::vector<Apart>& apart);
void remember_clause(Impl& machine, std::vector<Snapshot>& snapshots, std::uint32_t module, const Clause& clause);
// 失敗した読み込みの述語を写しへ戻し、この読み込みで作った述語を捨てる。
void rollback_preds(Impl& machine, const std::vector<Snapshot>& snapshots, std::size_t previous_size);
// first は、この項がファイルの最初の項であること。module/2 はそこでしか受けない。
[[nodiscard]] int consume_directive(Impl& machine, Reader& reader, Cell term, std::vector<Snapshot>& snapshots,
                                    std::vector<Apart>& apart, std::vector<Cell>& inits, bool first,
                                    std::string_view origin);

}
