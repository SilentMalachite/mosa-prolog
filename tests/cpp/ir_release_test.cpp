// レビュー 2026-11-20 の H-01。IrTerm の破棄（~IrTerm とムーブ代入）は noexcept の中で動くので、そこで確保すると
// std::bad_alloc が std::terminate になり、ADR 0010 のヒープの上限の止まり方にならない。前は深さ 64 より深い所を
// 壊すのに、作業の列（std::vector<std::vector<IrTerm>>）を確保した。ここでは破棄の間の operator new の回数を数え、
// 返していない確保の数が作る前に戻ることも見る（手で積む破棄が節点を取りこぼせば戻らない）。
// 数えるために、この試験の実行ファイルだけ大域の operator new と operator delete を置き換える。

#include "harness.hpp"
#include "mosa/ir.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <new>
#include <optional>
#include <string>
#include <utility>

namespace {

// operator new の呼ばれた回数と、まだ返していない確保の数。試験は 1 本のスレッドで動く。
std::size_t g_news = 0;
std::size_t g_live = 0;

}  // namespace

void* operator new(std::size_t size) {
  ++g_news;
  void* block = std::malloc(size == 0 ? 1 : size);
  if (block == nullptr) throw std::bad_alloc{};
  ++g_live;
  return block;
}

void operator delete(void* block) noexcept {
  if (block == nullptr) return;
  --g_live;
  std::free(block);
}

void operator delete(void* block, std::size_t) noexcept { operator delete(block); }

namespace {

mosa::IrTerm leaf(std::int64_t value) {
  mosa::IrTerm term;
  term.tag = mosa::IrTag::Int;
  term.integer = value;
  return term;
}

mosa::IrTerm pair(mosa::IrTerm left, mosa::IrTerm right) {
  mosa::IrTerm term;
  term.tag = mosa::IrTag::Fun;
  term.arity = 2;
  term.args.reserve(2);
  term.args.push_back(std::move(left));
  term.args.push_back(std::move(right));
  return term;
}

mosa::IrTerm small_tree() { return pair(pair(leaf(1), leaf(2)), pair(leaf(3), pair(leaf(4), leaf(5)))); }

// 深さ depth の鎖。tail_last なら次の段を最後の子に（リストの args[1] と同じ）、偽なら最初の子に置く。
// side は各段のもう 1 つの子で、small なら子を持つ小さな木、偽なら葉。
mosa::IrTerm chain(int depth, bool tail_last, bool small) {
  mosa::IrTerm tail = leaf(0);
  for (int level = 1; level <= depth; ++level) {
    mosa::IrTerm side = small ? small_tree() : leaf(level);
    tail = tail_last ? pair(std::move(side), std::move(tail)) : pair(std::move(tail), std::move(side));
  }
  return tail;
}

// 作った項を壊す間に確保しないこと。by_assign が真ならムーブ代入で、偽なら破棄で壊す。
void check_release(const std::string& label, int depth, bool tail_last, bool small, bool by_assign) {
  const std::size_t live = g_live;
  std::optional<mosa::IrTerm> term{chain(depth, tail_last, small)};
  const std::size_t before = g_news;
  if (by_assign) *term = mosa::IrTerm{};
  else term.reset();
  const std::size_t news = g_news - before;
  term.reset();
  mosa_test::check(news == 0, label + ": allocated " + std::to_string(news) + " times while releasing");
  mosa_test::check(g_live == live, label + ": every block returned");
}

// 自分の子からのムーブ代入（term = std::move(term.args[1])）は、頭の節点だけを壊して残りを受け取る。
void check_self_child() {
  const std::size_t live = g_live;
  std::optional<mosa::IrTerm> term{chain(100000, true, false)};
  const std::size_t before = g_news;
  *term = std::move(term->args[1]);
  const std::size_t news = g_news - before;
  mosa_test::check(news == 0 && term->args.size() == 2 && term->args[0].integer == 99999, "self child: moved in place");
  term.reset();
  mosa_test::check(g_live == live, "self child: every block returned");
}

}  // namespace

int main() {
  // 守り。深さ 64 までは C++ の再帰で壊し、前から確保しない。
  check_release("shallow", 40, true, true, false);
  check_release("list 100000", 100000, true, false, false);
  check_release("list 100000 by assign", 100000, true, false, true);
  check_release("first child 100000", 100000, false, false, false);
  check_release("small trees 20000", 20000, true, true, false);
  check_release("small trees first 20000", 20000, false, true, true);
  check_self_child();
  return mosa_test::failures();
}
