#pragma once

// load.cpp と load_directive.cpp だけが共有する。読み込みの外からは使わない。
#include "internal.hpp"

#include "mosa/reader.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace mosa {

struct Snapshot {
  std::uint32_t module = 0;
  std::uint32_t name = 0;
  std::uint16_t arity = 0;
  std::vector<Clause> clauses;
  bool created = false;
  bool dynamic = false;
  bool abolished = false;
  std::vector<MetaArg> meta;
};

// この読み込みの中だけで隙間を許す。述語ベクタには入れない。宣言は手続きを作らない。
struct Apart {
  std::uint32_t module = 0;
  std::uint32_t name = 0;
  std::uint16_t arity = 0;
};

// 定義は load_path.cpp。
std::filesystem::path canonical_key(const std::filesystem::path& file);

void fail_load(Impl& machine, std::string message);
[[nodiscard]] bool allows_gap(const std::vector<Apart>& apart, PredKey key);
void note_declared(Impl& machine, PredKey key, bool shared);
void remember_pred(Impl& machine, std::vector<Snapshot>& snapshots, PredKey key);
[[nodiscard]] bool run_inits(Impl& machine, const std::vector<Cell>& goals);
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
