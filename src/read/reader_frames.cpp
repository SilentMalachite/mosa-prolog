#include "mosa/reader.hpp"

#include <array>
#include <cstddef>
#include <utility>

// 読みのループと枠の還元（ADR 0011 改訂 4）。入れ子 1 段を C++ の 1 フレームにすると、1 MiB のスタックで
// f(f(...)) や [[...]] は 600 段、前置 850 段、括弧 1,000 段、xfy 1,600 段ほどで溢れた。待っている被演算子と
// 括弧の中身を frames_ に積み、1 つのループで読む。

namespace mosa {

// 括弧も枠に載り、parse_expr を呼ぶのは next だけである。それでも入口の底の印より上の枠だけをこのループの物とし、
// 出るときは frames_ を入口の形に戻す。この約束を 1 か所で守れば、呼び手は frames_ の中身を気にしなくてよい。
Reader::Parsed Reader::parse_expr(int max_priority) {
  const std::size_t base = frames_.size();
  while (true) {
    int current_max = frames_.size() == base ? max_priority : frames_.back().want;
    Parsed have = parse_primary(current_max);
    while (!have.failure && !have.pushed) {
      have = follow(std::move(have), current_max);
      if (have.failure || have.pushed || frames_.size() == base) break;
      Frame frame = std::move(frames_.back());
      frames_.pop_back();
      // 還元した項は、その枠が積まれた所の被演算子である。続きの演算子もその所の上限で探す。
      current_max = frame.outer_max;
      have = reduce(std::move(frame), std::move(have));
    }
    if (have.failure) {
      // 誤りの位置とトークンは have が持つ。積み残しを捨て、frames_ を入口の形に戻す。consult のファイルを読む
      // Reader は終わりまで生きるので、捨てないと誤りの枠の引数や関数子がその間ずっと残る。
      frames_.resize(base);
      return have;
    }
    if (!have.pushed) return have;
  }
}

Reader::Parsed Reader::pushed_mark() {
  Parsed parsed;
  parsed.pushed = true;
  return parsed;
}

// 括弧の枠は閉じか区切りを見る。区切りなら枠を積み直して pushed を返し、ループが次の項を読む。
Reader::Parsed Reader::reduce(Frame frame, Parsed have) {
  switch (frame.kind) {
    case FrameKind::Prefix:
      have.term = heap_.structure(heap_.atoms().intern(frame.op.name), std::array<Cell, 1>{have.term});
      have.priority = frame.op.priority;
      return have;
    case FrameKind::Infix:
      // 中置の '.' はリストの構成子にする（6.3.5）。compound がそれを決める。
      have.term = compound(frame.op.name, std::array<Cell, 2>{frame.left, have.term});
      have.priority = frame.op.priority;
      return have;
    case FrameKind::Args:
      return reduce_args(std::move(frame), std::move(have));
    case FrameKind::List:
      return reduce_list(std::move(frame), std::move(have));
    case FrameKind::ListTail:
      return reduce_list_tail(std::move(frame), std::move(have));
    case FrameKind::Paren:
      return reduce_paren(std::move(have));
    case FrameKind::Curly:
      return reduce_curly(std::move(have));
  }
  // どの種類も上で返る。列挙の外の値は積まないが、switch の後に返さないと MSVC の C4715 が出る。
  return have;
}

}
