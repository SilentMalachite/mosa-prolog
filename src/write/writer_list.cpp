// 書き手のうちリストの書き方（push_list、spine_entry、list_rest、push_bar）。writer.cpp が 800 行の上限に
// 近づいたので分けた（レビュー H-16 の前）。
#include "printer.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <optional>
#include <utility>

namespace mosa::printing {

void Printer::push_list(Cell term) {
  if (options_.ignore_ops) {
    // 演算子を使うときと同じく、i 番目の要素を深さ d+i-1 とみなす。頭はリストと同じ深さ、尾のセルは 1 つ深く、
    // 終わりの [] は切らない（list_rest が ] を深さより先に見るのと同じ）。
    emit(spell_functor(".") + "(");
    const std::size_t id = next_id_++;
    push_text(")");
    push_clear(id);
    const Cell tail = heap_.list_tail(term);
    push_term(tail, 999, true, true, is_nil(heap_, heap_.deref(tail)) ? 0 : 1);
    push_comma(id);
    return push_term(heap_.list_head(term), 999, true, true, 0);
  }
  emit("[");
  Work rest;
  rest.step = Step::ListRest;
  rest.term = term;
  rest.back = spine_entry(term);
  rest.passed = rest.back.raw() == term.raw();
  rest.anchor = term;
  rest.depth = depth_;
  work_.push_back(std::move(rest));
  push_term(heap_.list_head(term), 999, true, true, 0);
}

// 背骨の巡回の入口（Floyd の方法）。巡回しなければ list を返す。背骨のセルを祖先の集まりに足すと、長い
// リストで集まりと Leave が要素の数だけ膨らむので、入口だけを覚える。前は先頭のセルしか祖先になく、
// [f|X] で X = [a|X] のように途中から巡回するリストを書き続けて止まらなかった（レビュー M7）。
// 開いている祖先の尾と名前表の REF の尾で先読みを止める。list_rest もそこで |_] か名前で閉じるので、その先は
// 書かない。止めないと、要素がみな [x|L] の L や、答えで要素がみな [x|T] を指す L を書くとき、要素ごとに尾を
// 全部なめて 2 乗になる（M7 の Codex レビュー 1 回目と 2 回目）。
Cell Printer::spine_entry(Cell list) const {
  const auto next = [this](Cell cell) -> std::optional<Cell> {
    const Cell raw_tail = heap_.list_tail(cell);
    if (raw_tail.tag() == Tag::Ref && var_name(raw_tail) != nullptr) return std::nullopt;
    const Cell tail = heap_.deref(raw_tail);
    if (tail.tag() != Tag::Lis || open_.contains(tail.raw())) return std::nullopt;
    return tail;
  };
  // max_depth(N) で書く要素の数には限りがある。巡回がその先で閉じるなら、list_rest は入口に着く前に |...] で閉じる。
  // 巡回の始まりまでと一周の長さの和がこの数以下なら、slow がこの数だけ進む間に fast と出会う。出会わなければ書く範囲に
  // 巡回は無い。限らないと、深さで切るポートの行でも長いリストを毎回なめ、トレースが 2 乗になる（PR #4 の敵対的レビュー）。
  std::size_t steps = std::numeric_limits<std::size_t>::max();
  if (options_.max_depth > 0) steps = static_cast<std::size_t>(std::max(options_.max_depth - depth_, 0)) + 2;
  Cell slow = list;
  Cell fast = list;
  while (true) {
    if (steps-- == 0) return list;
    const std::optional<Cell> one = next(fast);
    if (!one) return list;
    const std::optional<Cell> two = next(*one);
    if (!two) return list;
    fast = *two;
    slow = *next(slow);
    if (slow.raw() == fast.raw()) break;
  }
  slow = list;
  while (slow.raw() != fast.raw()) {
    slow = *next(slow);
    fast = *next(fast);
  }
  return slow;
}

void Printer::list_rest(const Work& item) {
  const Cell cell = item.term;
  const Cell raw_tail = heap_.list_tail(cell);
  // 名前表の REF は束縛より先に書く。先に辿ると X = [a|X] が [a|_] になる。
  if (raw_tail.tag() == Tag::Ref && var_name(raw_tail) != nullptr) return push_bar(raw_tail);
  const Cell tail = heap_.deref(raw_tail);
  if (is_nil(heap_, tail)) return emit("]");
  // i 番目の要素は深さ d+i-1 にあるとみなす。次の要素が上限を越えるなら、そこから後ろを |... で閉じる。
  // 循環より先に深さを見る（複合項の render と同じ順、2026-11-18 の持ち主の決定 A）。深さの内なら循環が止める。
  if (options_.max_depth > 0 && item.depth + 1 > options_.max_depth && tail.tag() == Tag::Lis) return emit("|...]");
  // 背骨の入口は、書き終えた後に戻ってきたら祖先である。背骨が変わらなければ、最初に戻るセルは入口なので
  // 安全網の anchor より先にこちらが止める。anchor で止まるのは portray が背骨を変えたときだけである。
  if (open_.contains(tail.raw()) || (item.passed && tail.raw() == item.back.raw())) return emit("|_]");
  if (tail.raw() == item.anchor.raw()) return emit("|_]");
  if (tail.tag() != Tag::Lis) return push_bar(tail);
  Work rest;
  rest.step = Step::ListRest;
  rest.term = tail;
  rest.back = item.back;
  rest.passed = item.passed || tail.raw() == item.back.raw();
  rest.anchor = item.anchor;
  rest.depth = item.depth + 1;
  rest.lap = item.lap + 1;
  rest.power = item.power;
  if (rest.lap == rest.power) {
    rest.anchor = tail;
    rest.lap = 0;
    rest.power *= 2;
  }
  work_.push_back(std::move(rest));
  const std::size_t id = next_id_++;
  push_clear(id);
  push_term(heap_.list_head(tail), 999, true);
  push_comma(id);
}

void Printer::push_bar(Cell rest) {
  const std::size_t id = next_id_++;
  push_text("]");
  push_clear(id);
  push_term(rest, 999, true);
  Work bar;
  bar.step = Step::Bar;
  bar.id = id;
  work_.push_back(std::move(bar));
}

}  // namespace mosa::printing
