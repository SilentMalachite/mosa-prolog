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

Reader::Parsed Reader::parse_paren() {
  consume();
  Parsed inner = parse_expr(1200);
  if (inner.failure) return inner;
  const Scan scan = peek();
  if (scan.failure) return failure_of(scan);
  if (!solo(scan.token, ")")) return bad_token(scan.token);
  consume();
  // 括弧で包んだ項の優先度は 0。外側の演算子はこの中を割らない。
  inner.priority = 0;
  return inner;
}

Reader::Parsed Reader::call_or_atom(std::string name) {
  const Scan next = peek();
  if (next.failure) return failure_of(next);
  // 空白を挟まない '(' だけが関数表記。[] (2,3) と {} (2,3) の '(' は括弧である。
  if (next.token.kind == TokenKind::Solo && next.token.text == "(" && !next.token.after_layout) {
    consume();
    return parse_arguments(std::move(name));
  }
  return atom_named(name);
}

Reader::Parsed Reader::parse_curly() {
  consume();
  const Scan scan = peek();
  if (scan.failure) return failure_of(scan);
  if (solo(scan.token, "}")) {
    consume();
    return call_or_atom("{}");
  }
  Parsed inner = parse_expr(1200);
  if (inner.failure) return inner;
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

Reader::Parsed Reader::parse_arguments(std::string functor) {
  std::vector<Cell> args;
  while (true) {
    if (args.size() == 1024) {
      const Scan extra = peek();
      if (extra.failure) return failure_of(extra);
      return max_arity(extra.token);
    }
    Parsed gap = reject_empty_comma();
    if (gap.failure) return gap;
    Parsed arg = parse_expr(999);
    if (arg.failure) return arg;
    args.push_back(arg.term);
    const Scan scan = peek();
    if (scan.failure) return failure_of(scan);
    if (solo(scan.token, ",")) {
      consume();
      continue;
    }
    if (solo(scan.token, ")")) {
      consume();
      break;
    }
    return bad_token(scan.token);
  }
  Parsed parsed;
  parsed.term = heap_.structure(heap_.atoms().intern(functor), args);
  return parsed;
}

Reader::Parsed Reader::take_bar_tail(Cell& tail) {
  consume();
  Parsed rest = parse_expr(999);
  if (rest.failure) return rest;
  const Scan again = peek();
  if (again.failure) return failure_of(again);
  // リストの | は演算子ではない。2 つ目は項の一部にせず不正トークンにする。
  if (solo(again.token, "|")) return bad_token(again.token);
  tail = rest.term;
  Parsed ok;
  return ok;
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

Reader::Parsed Reader::parse_list() {
  consume();
  const Scan scan = peek();
  if (scan.failure) return failure_of(scan);
  if (solo(scan.token, "]")) {
    consume();
    return call_or_atom("[]");
  }
  Parsed headless = reject_headless_bar();
  if (headless.failure) return headless;
  std::vector<Cell> heads;
  Cell tail;
  bool barred = false;
  while (!barred) {
    Parsed gap = reject_empty_comma();
    if (gap.failure) return gap;
    Parsed elem = parse_expr(999);
    if (elem.failure) return elem;
    heads.push_back(elem.term);
    const Scan sep = peek();
    if (sep.failure) return failure_of(sep);
    if (solo(sep.token, ",")) {
      consume();
      continue;
    }
    if (solo(sep.token, "|")) {
      Parsed taken = take_bar_tail(tail);
      if (taken.failure) return taken;
      barred = true;
      break;
    }
    if (solo(sep.token, "]")) break;
    return bad_token(sep.token);
  }
  const Scan close = peek();
  if (close.failure) return failure_of(close);
  if (!solo(close.token, "]")) return bad_token(close.token);
  consume();
  if (!barred) tail = heap_.atom(heap_.atoms().intern("[]"));
  Parsed parsed;
  parsed.term = cons_all(heap_, heads, tail);
  return parsed;
}

}
