#pragma once

// 第1引数の索引（ADR 0015）。節の頭の鍵と節の番号だけを持ち、ヒープのセルを持たないので GC は見ない。
// Predicate::index が持つ。作って引くのは index.cpp、節を足し消すのに合わせて直すのは clause_store.cpp だけ。

#include <cstddef>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

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

// 一つの鍵の組。節の番号を昇順に持つ。deque は要素を 1 つ入れると固定の塊（libc++ で 4 KB、libstdc++ で 512 バイト）
// を取り、異なる鍵が多い述語では鍵の数だけ塊が要る（レビュー M3）。vector 2 本なら空の組は何も割り当てず、番号 1 つの
// 組の割り当ては 1 語で済む。
// asserta の番号は prepended に逆順で積み、それ以外は appended に積む。昇順は prepended を逆に読み、続けて
// appended を start から読んだ並び。先頭を外すのは start を進めるだけにする。
class IdBucket {
 public:
  void push_front(std::size_t id) { prepended_.push_back(id); }
  void push_back(std::size_t id) { appended_.push_back(id); }
  [[nodiscard]] std::size_t size() const { return prepended_.size() + appended_.size() - start_; }
  [[nodiscard]] bool empty() const { return size() == 0; }
  // 昇順で at 番目の番号。
  [[nodiscard]] std::size_t operator[](std::size_t at) const {
    const std::size_t front = prepended_.size();
    return at < front ? prepended_[front - 1 - at] : appended_[start_ + at - front];
  }
  // 最小（pop_front_id）か最大（pop_back_id）の番号が id なら外して真。違えば番号の並びを変えずに偽。
  bool pop_front_id(std::size_t id);
  bool pop_back_id(std::size_t id);
  void clear();

 private:
  std::vector<std::size_t> prepended_;
  std::vector<std::size_t> appended_;
  std::size_t start_ = 0;
};

// 組と keyless は節の番号を節の順（昇順）に持つ。死んだ節も節の列に残る間は持ち、引くときに世代で落とす。
struct FirstArgIndex {
  bool built = false;
  std::unordered_map<IndexKey, IdBucket, IndexKeyHash> buckets;
  // 第1引数が変数の節と、鍵を持たない節（Any・None）。どの呼び出しにも合う。
  IdBucket keyless;
};

struct Clause;

// 節の鍵（定義は index.cpp）。鍵の無い節（Any・Var・None）は nullopt。索引を作るときも直すときもこれで組を選ぶ。
[[nodiscard]] std::optional<IndexKey> clause_key(const Clause& clause);

}  // namespace mosa
