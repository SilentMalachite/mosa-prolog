#include "mosa/reader.hpp"

#include <array>
#include <cstddef>
#include <utility>

// 読みのループと枠の還元（ADR 0011 改訂 4）。入れ子 1 段を C++ の 1 フレームにすると、1 MiB のスタックで
// 前置 850 段、xfy 1,600 段ほどで溢れた。待っている被演算子を frames_ に積み、1 つのループで読む。

namespace mosa {

// 括弧の中はまだ parse_expr を再帰で呼ぶ。入れ子のループどうしが frames_ を分け合えるよう、入口の底の印より
// 上の枠だけをこのループの物とする。
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
      // 誤りの位置とトークンは have が持つ。積み残しを捨て、次の read/1 が古い枠を見ないようにする。
      frames_.resize(base);
      return have;
    }
    if (!have.pushed) return have;
  }
}

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
    case FrameKind::List:
    case FrameKind::ListTail:
    case FrameKind::Paren:
    case FrameKind::Curly:
      // 括弧の枠はまだ積まない。括弧は parse_expr を再帰で呼び、閉じまで自分で読む。
      break;
  }
  return have;
}

}
