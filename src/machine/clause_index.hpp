#pragma once

// 第1引数の索引（ADR 0015）。節の頭の鍵と節の番号だけを持ち、ヒープのセルを持たないので GC は見ない。
// Predicate::index が持つ。作って引くのは index.cpp、節を足し消すのに合わせて直すのは clause_store.cpp だけ。
// 走査が候補を探す ClauseWalk もここで宣言する（ADR 0015 の改訂、定義は index.cpp）。

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <unordered_map>
#include <vector>

namespace mosa {

// Entry が走査の鍵を持つので 1 バイトにする（ADR 0015 の改訂）。
enum class KeyKind : std::uint8_t { Any, None, Var, Atom, Int, Float, List, Fun, Big };

// 8 節より少ない述語は、全部の節の鍵を照らしても組を引くのと変わらない。索引の記憶と作る手間を省く。
inline constexpr std::size_t kIndexMinClauses = 8;

// Atom はアトムの番号、Int は値、Float はビット列（-0.0 は 0.0、どの NaN も一つの NaN に揃える）、List は 0、
// Fun は名前の番号とアリティ。浮動小数点数もビット列で比べるので、NaN の鍵でも等しさが反射的になる。Big は値でなく
// big_key の指紋（ADR 0025）なので、鍵の違う節を飛ばせるだけで、鍵が等しくても値が等しいとは限らない。合うかは頭の
// 単一化が決める。kind が Int と分けるので、同じビットの整数の鍵とも等しくならない。
// 走査の鍵（Entry::key）では、Any は全部の節（第1引数が未束縛）、None は鍵の無い節だけ（鍵の型でない値に束縛）。
// どちらも節の鍵（clause_key）には現れないので、組を引いても見つからない。kind を後ろに置いて 16 バイトに収める。
struct IndexKey {
  std::uint64_t value = 0;
  std::uint32_t arity = 0;
  KeyKind kind = KeyKind::Any;
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
  // 番号が id 以上の最初の要素の添字（昇順の位置）。無ければ size()。
  [[nodiscard]] std::size_t lower_bound(std::size_t id) const;
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

// 開いた Clauses・ClauseScan の枠のスタック位置と effect_id（ADR 0015）。外した枠の位置には別の枠が積まれうるので、
// effect_id が合わない記録はもう開いていない。
struct ScanRecord {
  std::size_t index = 0;
  std::uint64_t id = 0;
};

struct Clause;
struct Predicate;

// 節の鍵（定義は index.cpp）。鍵の無い節（Any・Var・None）は nullopt。索引を作るときも直すときもこれで組を選ぶ。
[[nodiscard]] std::optional<IndexKey> clause_key(const Clause& clause);
// 鍵 key の走査が節を候補にするか。Any はどの節も、ほかは鍵の等しい節と鍵の無い節を選ぶ。
[[nodiscard]] bool key_selects(const Clause& clause, const IndexKey& key);

// 走査が節を試してよいか（ADR 0015 の改訂）。retract は走査より後に足した節と、別の retract が消した節を飛ばす。
// 呼び出しと clause/2 は世代 generation に見える節を試す（7.5.4）。
struct ScanView {
  std::uint64_t generation = 0;
  bool retract = false;
};
[[nodiscard]] bool scan_usable(const Clause& clause, ScanView view);

// ClauseWalk::hint の「組の添字を知らない」。
inline constexpr std::size_t kNoHint = std::numeric_limits<std::size_t>::max();

// 番号 from 以上で、鍵 key に合い view で試せる節を節の順に返す（定義は index.cpp）。8 節以上で鍵があれば索引の
// 組と鍵の無い節を番号の二分探索から歩き、ほかは節の列を歩く。どちらも死んだ節をどの走査も見ないときは、飛び越し
// の幅で跳ぶ。歩く間に述語の節を足したり消したりしない。hint は前の歩みの hint()。組のその添字の番号が from なら
// 二分探索を省く（やり直しのたびに組を探すと、鍵の走査の一歩が組の大きさの対数になる）。違えば探す。
class ClauseWalk {
 public:
  ClauseWalk(const Predicate& predicate, const IndexKey& key, std::size_t from, ScanView view,
             std::size_t hint = kNoHint);
  // 次の候補の位置。尽きたら clauses.size()。
  [[nodiscard]] std::size_t next();
  // 最後に返した候補が鍵の組から来たならその組の添字、ほかは kNoHint。
  [[nodiscard]] std::size_t hint() const { return hint_; }

 private:
  [[nodiscard]] std::size_t next_in_list();
  [[nodiscard]] std::size_t next_in_index();
  const Predicate* predicate_;
  IndexKey key_;
  ScanView view_;
  bool skip_ = false;
  bool by_index_ = false;
  // 索引の鍵の組。鍵の組が無ければ null。
  const IdBucket* bucket_ = nullptr;
  // 節の列では次に見る位置、索引では組の次の添字。keyless_ は鍵の無い節の列の次の添字。
  std::size_t at_ = 0;
  std::size_t keyless_ = 0;
  std::size_t hint_ = kNoHint;
};

}  // namespace mosa
