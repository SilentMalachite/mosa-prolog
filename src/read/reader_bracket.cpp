#include "mosa/reader.hpp"

#include <array>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace mosa {
namespace {

Cell cons_all(Heap& heap, const std::vector<Cell>& heads, Cell tail) {
  for (auto head = heads.rbegin(); head != heads.rend(); ++head) tail = heap.list(*head, tail);
  return tail;
}

bool solo(const Token& token, std::string_view text) {
  return token.kind == TokenKind::Solo && token.text == text;
}

}  // namespace

// 括弧の読み（ADR 0011 改訂 4）。中の項を parse_expr の再帰で読むと、入れ子 1 段が C++ の 1 フレームになり、
// 1 MiB のスタックで f(f(...)) や [[...]] は 600 段ほど、括弧は 1,000 段ほどで溢れた。開きを読んだら枠を積んで
// 中の項を待ち、閉じと区切りは項が読めて枠を降ろしたとき reduce_* が見る。字句を見る順と検査は再帰のときと同じ
// なので、誤りの位置とトークンは変わらない。

Reader::Parsed Reader::parse_paren(int outer_max) {
  consume();
  frames_.push_back(Frame{FrameKind::Paren, outer_max, 1200, OpDef{}, Cell{}, {}, {}});
  return pushed_mark();
}

Reader::Parsed Reader::reduce_paren(Parsed inner) {
  const Scan scan = peek();
  if (scan.failure) return failure_of(scan);
  if (!solo(scan.token, ")")) return bad_token(scan.token);
  consume();
  // 括弧で包んだ項の優先度は 0。外側の演算子はこの中を割らない。
  inner.priority = 0;
  return inner;
}

Reader::Parsed Reader::call_or_atom(std::string name, int outer_max) {
  const Scan next = peek();
  if (next.failure) return failure_of(next);
  // 空白を挟まない '(' だけが関数表記。[] (2,3) と {} (2,3) の '(' は括弧である。
  if (next.token.kind == TokenKind::Solo && next.token.text == "(" && !next.token.after_layout) {
    consume();
    return parse_arguments(std::move(name), outer_max);
  }
  return atom_named(name);
}

Reader::Parsed Reader::parse_curly(int outer_max) {
  consume();
  const Scan scan = peek();
  if (scan.failure) return failure_of(scan);
  if (solo(scan.token, "}")) {
    consume();
    return call_or_atom("{}", outer_max);
  }
  frames_.push_back(Frame{FrameKind::Curly, outer_max, 1200, OpDef{}, Cell{}, {}, {}});
  return pushed_mark();
}

Reader::Parsed Reader::reduce_curly(Parsed inner) {
  const Scan close = peek();
  if (close.failure) return failure_of(close);
  if (!solo(close.token, "}")) return bad_token(close.token);
  consume();
  Parsed parsed;
  parsed.term = heap_.structure(heap_.atoms().intern("{}"), std::array<Cell, 1>{inner.term});
  return parsed;
}

Reader::Parsed Reader::reject_empty_comma() {
  const Scan scan = peek();
  if (scan.failure) return failure_of(scan);
  if (!solo(scan.token, ",")) {
    Parsed ok;
    return ok;
  }
  // 区切りのコンマは原子ではない。槽の先頭だと、隣のコンマを項にしてしまう。
  return bad_token(scan.token);
}

Reader::Parsed Reader::open_argument(std::size_t filled) {
  if (filled == 1024) {
    const Scan extra = peek();
    if (extra.failure) return failure_of(extra);
    return max_arity(extra.token);
  }
  return reject_empty_comma();
}

Reader::Parsed Reader::parse_arguments(std::string functor, int outer_max) {
  Parsed gap = open_argument(0);
  if (gap.failure) return gap;
  frames_.push_back(Frame{FrameKind::Args, outer_max, 999, OpDef{}, Cell{}, std::move(functor), {}});
  return pushed_mark();
}

Reader::Parsed Reader::reduce_args(Frame frame, Parsed arg) {
  frame.items.push_back(arg.term);
  const Scan scan = peek();
  if (scan.failure) return failure_of(scan);
  if (solo(scan.token, ",")) {
    consume();
    Parsed gap = open_argument(frame.items.size());
    if (gap.failure) return gap;
    // ループは枠を降ろしてから reduce を呼ぶ。積み直すと、次の引数もこの枠の want の 999 で読む。
    frames_.push_back(std::move(frame));
    return pushed_mark();
  }
  if (!solo(scan.token, ")")) return bad_token(scan.token);
  consume();
  Parsed parsed;
  parsed.term = compound(frame.functor, frame.items);
  return parsed;
}

// '.'/2 はリストの構成子（6.3.5）なので、'.'(a, []) も [a] と同じセルにする。構造体のままだと = と is_list/1 が
// リストと見ない。前は実行時のゴールを IR に写すときに直していたが、M28 から写さずに元の項を指す（レビュー M28）。
Cell Reader::compound(std::string_view name, std::span<const Cell> args) {
  if (name == "." && args.size() == 2) return heap_.list(args[0], args[1]);
  return heap_.structure(heap_.atoms().intern(name), args);
}

Reader::Parsed Reader::take_bar_tail(Frame list) {
  consume();
  // 尾の先頭も槽と同じ。引用した ',' は Name なのでここを通る。
  Parsed gap = reject_empty_comma();
  if (gap.failure) return gap;
  list.kind = FrameKind::ListTail;
  frames_.push_back(std::move(list));
  return pushed_mark();
}

Reader::Parsed Reader::reduce_list_tail(Frame frame, Parsed rest) {
  const Scan close = peek();
  if (close.failure) return failure_of(close);
  // リストの | は演算子ではない。尾の後の 2 つ目の | も、] でないほかの字句と同じく不正トークンにする。
  if (!solo(close.token, "]")) return bad_token(close.token);
  consume();
  Parsed parsed;
  parsed.term = cons_all(heap_, frame.items, rest.term);
  return parsed;
}

Reader::Parsed Reader::reject_headless_bar() {
  const Scan scan = peek();
  if (scan.failure) return failure_of(scan);
  if (!solo(scan.token, "|")) {
    Parsed ok;
    return ok;
  }
  // 直後が ] のときだけ拒む。|+1 のように項が続く | は要素の先頭へ戻す。
  const Lexer saved = lexer_;
  const Scan saved_pending = pending_;
  const bool saved_has = has_pending_;
  consume();
  const Scan after = peek();
  if (after.failure || !solo(after.token, "]")) {
    lexer_ = saved;
    pending_ = saved_pending;
    has_pending_ = saved_has;
    Parsed ok;
    return ok;
  }
  return bad_token(after.token);
}

Reader::Parsed Reader::parse_list(int outer_max) {
  consume();
  const Scan scan = peek();
  if (scan.failure) return failure_of(scan);
  if (solo(scan.token, "]")) {
    consume();
    return call_or_atom("[]", outer_max);
  }
  Parsed headless = reject_headless_bar();
  if (headless.failure) return headless;
  Parsed gap = reject_empty_comma();
  if (gap.failure) return gap;
  frames_.push_back(Frame{FrameKind::List, outer_max, 999, OpDef{}, Cell{}, {}, {}});
  return pushed_mark();
}

Reader::Parsed Reader::reduce_list(Frame frame, Parsed elem) {
  frame.items.push_back(elem.term);
  const Scan sep = peek();
  if (sep.failure) return failure_of(sep);
  if (solo(sep.token, ",")) {
    consume();
    Parsed gap = reject_empty_comma();
    if (gap.failure) return gap;
    frames_.push_back(std::move(frame));
    return pushed_mark();
  }
  if (solo(sep.token, "|")) return take_bar_tail(std::move(frame));
  if (!solo(sep.token, "]")) return bad_token(sep.token);
  consume();
  Parsed parsed;
  parsed.term = cons_all(heap_, frame.items, heap_.atom(heap_.atoms().intern("[]")));
  return parsed;
}

}
