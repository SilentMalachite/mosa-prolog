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
  Reader(std::string_view source, Heap& heap, OperatorTable& operators,
         DoubleQuotes quotes = DoubleQuotes::Codes);

  [[nodiscard]] ReadOutcome next();
  [[nodiscard]] std::size_t consumed() const { return lexer_.offset(); }
  void set_quotes(DoubleQuotes quotes) { quotes_ = quotes; }

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
  [[nodiscard]] Parsed take_bar_tail(Cell& tail);
  [[nodiscard]] Parsed parse_curly();
  [[nodiscard]] Parsed parse_arguments(std::string functor);
  [[nodiscard]] Parsed atom_named(std::string_view name);
  [[nodiscard]] Cell intern_variable(std::string name);
  [[nodiscard]] Cell make_string(std::string_view text);
  [[nodiscard]] bool can_start(const Token& token) const;
  [[nodiscard]] std::optional<OpDef> find_prefix(std::string_view name, int max_priority) const;
  [[nodiscard]] std::optional<OpDef> find_follower(std::string_view name, int left_priority,
                                                   int max_priority) const;
  [[nodiscard]] Parsed failure_of(const Scan& scan) const;
  [[nodiscard]] Parsed bad_token(const Token& token) const;
  [[nodiscard]] Parsed max_arity(const Token& token) const;
  [[nodiscard]] static bool is_closer(std::string_view text);

  Lexer lexer_;
  Heap& heap_;
  OperatorTable& operators_;
  DoubleQuotes quotes_;
  Scan pending_{};
  bool has_pending_ = false;
  std::vector<VarRec> vars_;
  std::vector<Cell> occurrences_;
};

}
