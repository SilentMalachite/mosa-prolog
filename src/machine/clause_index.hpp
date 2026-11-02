#pragma once

// 第1引数の索引（ADR 0015）。節の頭の鍵と節の番号だけを持ち、ヒープのセルを持たないので GC は見ない。
// Predicate::index が持つ。作って引くのは index.cpp、節を足し消すのに合わせて直すのは clause_store.cpp だけ。

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <unordered_map>

namespace mosa {

enum class KeyKind { Any, None, Var, Atom, Int, Float, List, Fun };

// 8 節より少ない述語は、全部の節の鍵を照らしても組を引くのと変わらない。索引の記憶と作る手間を省く。
inline constexpr std::size_t kIndexMinClauses = 8;

// Atom はアトムの番号、Int は値、Float はビット列（-0.0 は 0.0、どの NaN も一つの NaN に揃える）、List は 0、
// Fun は名前の番号とアリティ。浮動小数点数もビット列で比べるので、NaN の鍵でも等しさが反射的になる。
struct IndexKey {
  KeyKind kind = KeyKind::Any;
  std::uint64_t value = 0;
  std::uint32_t arity = 0;
  friend bool operator==(const IndexKey&, const IndexKey&) = default;
};

struct IndexKeyHash {
  [[nodiscard]] std::size_t operator()(const IndexKey& key) const noexcept;
};

// 組と keyless は節の番号を節の順（昇順）に持つ。死んだ節も節の列に残る間は持ち、引くときに世代で落とす。
struct FirstArgIndex {
  bool built = false;
  std::unordered_map<IndexKey, std::deque<std::size_t>, IndexKeyHash> buckets;
  // 第1引数が変数の節と、鍵を持たない節（Any・None）。どの呼び出しにも合う。
  std::deque<std::size_t> keyless;
};

struct Clause;

// 節の鍵（定義は index.cpp）。鍵の無い節（Any・Var・None）は nullopt。索引を作るときも直すときもこれで組を選ぶ。
[[nodiscard]] std::optional<IndexKey> clause_key(const Clause& clause);

}  // namespace mosa
