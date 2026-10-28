#pragma once

// 項はヒープ上の Cell だけ。別の構文木は作らない。優先度は ISO 6.3.4。

#include "mosa/heap.hpp"
#include "mosa/lexer.hpp"
#include "mosa/operator_table.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mosa {

enum class DoubleQuotes { Codes, Chars, Atom };

struct VarName {
  std::string name;
  Cell cell;
};

struct ReadTerm {
  Cell term{};
  std::vector<VarName> names;
  std::vector<VarName> singletons;
  // 匿名を含む、最初の出現順。`_` は出現のたびに別の要素。
  std::vector<Cell> variables;
};

struct ReadOutcome {
  std::optional<ReadTerm> term;
  bool eof = false;
  std::optional<SyntaxFailure> failure;
};

class Reader {
 public:
  Reader(std::string_view source, Heap& heap, OperatorView operators,
         DoubleQuotes quotes = DoubleQuotes::Codes, const CharConv* conv = nullptr);

  [[nodiscard]] ReadOutcome next();
  [[nodiscard]] std::size_t consumed() const { return lexer_.offset(); }
  void set_quotes(DoubleQuotes quotes) { quotes_ = quotes; }
  // 読み込み中に module/2 で文脈が替わる。次の項から、その見え方で読む。
  void set_operators(OperatorView operators) { operators_ = operators; }

 private:
  struct Parsed {
    Cell term{};
    int priority = 0;
    std::optional<SyntaxFailure> failure;
  };

  struct VarRec {
    std::string name;
    Cell cell;
    int count = 0;
  };

  [[nodiscard]] Scan peek();
  Scan consume();
  [[nodiscard]] Parsed parse_expr(int max_priority);
  [[nodiscard]] Parsed parse_prefix(int max_priority);
  [[nodiscard]] Parsed parse_literal();
  [[nodiscard]] Parsed parse_name(int max_priority);
  [[nodiscard]] Parsed apply_prefix(const OpDef& op);
  [[nodiscard]] Parsed parse_rest(Parsed left, int max_priority);
  [[nodiscard]] Parsed parse_paren();
  [[nodiscard]] Parsed parse_list();
  // [|] の | は原子にしない。直後が項なら字句を戻して今の読みに任せる。
  [[nodiscard]] Parsed reject_headless_bar();
  // 槽の先頭のコンマは原子にしない。区切りのままでは項が無い。
  [[nodiscard]] Parsed reject_empty_comma();
  [[nodiscard]] Parsed take_bar_tail(Cell& tail);
  [[nodiscard]] Parsed parse_curly();
  [[nodiscard]] Parsed parse_arguments(std::string functor);
  [[nodiscard]] Parsed call_or_atom(std::string name);
  [[nodiscard]] Parsed atom_named(std::string_view name);
  [[nodiscard]] Cell intern_variable(std::string name);
  [[nodiscard]] Cell make_string(std::string_view text);
  [[nodiscard]] bool can_start(const Token& token) const;
  [[nodiscard]] std::optional<OpDef> find_prefix(std::string_view name, int max_priority) const;
  [[nodiscard]] std::optional<OpDef> find_follower(std::string_view name, int left_priority,
                                                   int max_priority) const;
  // 不正な項の終止符まで進める。確認の空白はトークンに含めない。
  void absorb_full_stop(const Scan& offending);
  [[nodiscard]] Parsed failure_of(const Scan& scan) const;
  [[nodiscard]] Parsed bad_token(const Token& token) const;
  [[nodiscard]] Parsed max_arity(const Token& token) const;
  [[nodiscard]] static bool is_closer(std::string_view text);

  Lexer lexer_;
  Heap& heap_;
  OperatorView operators_;
  DoubleQuotes quotes_;
  Scan pending_{};
  bool has_pending_ = false;
  std::vector<VarRec> vars_;
  std::vector<Cell> occurrences_;
};

}
