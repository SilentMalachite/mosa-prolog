// レビュー 2026-11-20 の M-01。AtomTable::intern は names_、ascii_、ids_ の 3 つを順に伸ばす。前は途中の確保が
// std::bad_alloc を投げると、先に伸びた入れ物だけが長いまま残った。返していない番号が valid になり、次の intern は
// 番号を 1 つ飛ばし、ascii(id) は ascii_ の終わりの先を読んだ。ここでは intern の中の k 番目の確保を失敗させ、
// k を 1 から確保が尽きるまで回して、失敗した intern が表を何も変えないことを見る。deque の塊の境も通るよう、
// 原子を 3000 個足す。失敗させるために、この試験の実行ファイルだけ大域の operator new と operator delete を置き換える。

#include "harness.hpp"
#include "mosa/atom.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <new>
#include <string>
#include <utility>

namespace {

// 0 なら失敗させない。正なら、その回数目の operator new が std::bad_alloc を投げる。試験は 1 本のスレッドで動く。
std::size_t g_fail_at = 0;
std::size_t g_news = 0;

}  // namespace

void* operator new(std::size_t size) {
  if (g_fail_at != 0 && ++g_news == g_fail_at) throw std::bad_alloc{};
  void* block = std::malloc(size == 0 ? 1 : size);
  if (block == nullptr) throw std::bad_alloc{};
  return block;
}

void operator delete(void* block) noexcept { std::free(block); }

void operator delete(void* block, std::size_t) noexcept { operator delete(block); }

namespace {

// 小さな文字列の最適化に入らない長さにし、ASCII でない原子も混ぜる。
std::string name_of(std::size_t index) {
  std::string text = (index % 2 == 0 ? "atom_" : "原子_") + std::to_string(index);
  text.append(32, 'x');
  return text;
}

bool ascii_text(const std::string& text) {
  for (const char unit : text) {
    if (static_cast<unsigned char>(unit) >= 0x80U) return false;
  }
  return true;
}

// k 番目の確保で失敗させて intern を呼ぶ。投げれば偽、通れば番号を id に入れて真。
bool intern_failing_at(mosa::AtomTable& table, const std::string& text, std::size_t fail_at, std::uint32_t& id) {
  g_news = 0;
  g_fail_at = fail_at;
  try {
    id = table.intern(text);
  } catch (const std::bad_alloc&) {
    g_fail_at = 0;
    return false;
  }
  g_fail_at = 0;
  return true;
}

void failed_intern_changes_nothing() {
  mosa::AtomTable table;
  std::uint32_t next = 1;
  std::size_t failures = 0;
  bool consistent = true;
  for (std::size_t index = 0; index < 3000 && consistent; ++index) {
    const std::string text = name_of(index);
    std::uint32_t id = 0;
    std::size_t fail_at = 1;
    while (!intern_failing_at(table, text, fail_at, id)) {
      ++failures;
      // 失敗した原子の番号は空いたまま（valid でない）。
      if (table.valid(next)) consistent = false;
      ++fail_at;
    }
    if (id != next || table.name(id) != text || table.ascii(id) != ascii_text(text)) consistent = false;
    ++next;
  }
  mosa_test::check(failures > 0, "some allocations failed");
  mosa_test::check(consistent, "a failed intern leaves the table as it was");
  // 失敗の後も、前の原子は同じ番号で引ける。
  std::uint32_t id = 0;
  mosa_test::check(intern_failing_at(table, name_of(0), 0, id) && id == 1, "the first atom keeps its number");
}

}  // namespace

// レビュー N-01。ids_ の鍵は names_ の要素を指す view になった。写しは自分の要素から ids_ を作り直すので、写し元を
// 壊した後も同じ番号で引け、移した表も引ける。前は鍵に名前の写しを持っていた。
void copy_keeps_own_keys() {
  std::optional<mosa::AtomTable> source(std::in_place);
  const auto alpha = source->intern("alpha");
  const auto beta = source->intern("ベータ");
  mosa::AtomTable copy(*source);
  mosa::AtomTable assigned;
  assigned = *source;
  source.reset();
  mosa_test::check(copy.intern("alpha") == alpha && copy.intern("ベータ") == beta && copy.name(beta) == "ベータ",
                   "a copy finds the names after its source is gone");
  mosa_test::check(assigned.intern("alpha") == alpha && assigned.name(alpha) == "alpha", "an assigned copy too");
  const auto gamma = copy.intern("gamma");
  mosa_test::check(gamma == beta + 1 && assigned.intern("gamma") == gamma, "both copies go on from the same number");
  mosa::AtomTable moved(std::move(copy));
  mosa_test::check(moved.intern("gamma") == gamma && moved.intern("alpha") == alpha, "a moved table finds its names");
}

// 自分への移しは別の関数を通す。同じ式に書くと -Wself-move が止める。
void move_assign(mosa::AtomTable& to, mosa::AtomTable& from) { to = std::move(from); }

// レビュー R-M02。移した後の元の表も、番号 0 を欠番にした空の表として使える。自分への移しは何も変えない。前は
// 既定の移しで、元の names_ が空になって次の intern が番号 0 を返し、自分への移しは表を空にした。移しは空の表を
// 作ってから入れ替えるので、確保が投げても元の表と ids_ の view は前のまま残る。
void move_keeps_invariants() {
  mosa::AtomTable source;
  const auto alpha = source.intern("alpha");
  mosa::AtomTable moved(std::move(source));
  mosa_test::check(moved.intern("alpha") == alpha, "the moved table keeps its names");
  mosa_test::check(!source.valid(0) && source.intern("again") != 0, "the moved-from table keeps number 0 unused");
  mosa::AtomTable assigned;
  assigned = std::move(moved);
  mosa_test::check(assigned.intern("alpha") == alpha && assigned.name(alpha) == "alpha", "move assignment keeps names");
  mosa_test::check(!moved.valid(0) && moved.intern("next") != 0, "the assigned-from table keeps number 0 unused");
  move_assign(assigned, assigned);
  mosa_test::check(assigned.valid(alpha) && assigned.name(alpha) == "alpha" && assigned.intern("alpha") == alpha,
                   "a self move changes nothing");
}

int main() {
  failed_intern_changes_nothing();
  copy_keeps_own_keys();
  move_keeps_invariants();
  return mosa_test::failures();
}
