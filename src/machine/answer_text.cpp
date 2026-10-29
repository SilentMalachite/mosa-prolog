// 計画 Task 11。answer_text.hpp の注を見よ。

#include "answer_text.hpp"

#include "mosa/writer.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace mosa {
namespace {

// 後入れ先出しの仕事。Term は項を書く、Tail はリストの残りを書く、Text はその字面を足す、Close は開いた構造を閉じる。
struct Step {
  enum class Kind { Term, Tail, Text, Close };
  Kind kind;
  Cell cell;
  std::string_view text;
};

class AnswerWriter {
 public:
  AnswerWriter(const Heap& heap, OperatorView operators)
      : heap_(heap), operators_(operators), nil_(heap.atom(heap.atoms().intern("[]"))) {}
  [[nodiscard]] std::string run(Cell root);

 private:
  void term(Cell cell);
  void tail(Cell cell);
  void open_list(Cell cell);
  [[nodiscard]] std::string leaf(Cell cell) const;
  [[nodiscard]] std::string var_text(Cell cell);
  void push(Step::Kind kind, Cell cell, std::string_view text = {}) { stack_.push_back(Step{kind, cell, text}); }

  const Heap& heap_;
  OperatorView operators_;
  Cell nil_;
  std::vector<Step> stack_;
  std::unordered_set<std::uint64_t> open_;
  std::unordered_map<std::uint64_t, int> assigned_;
  int next_number_ = 1;
  std::string out_;
};

std::string AnswerWriter::run(Cell root) {
  push(Step::Kind::Term, root);
  while (!stack_.empty()) {
    const Step step = stack_.back();
    stack_.pop_back();
    switch (step.kind) {
      case Step::Kind::Term: term(step.cell); break;
      case Step::Kind::Tail: tail(step.cell); break;
      case Step::Kind::Text: out_ += step.text; break;
      case Step::Kind::Close: open_.erase(step.cell.raw()); break;
    }
  }
  return std::move(out_);
}

// 葉は writer に書かせ、引用と数の字面を合わせる。入れ子が無いので再帰は 1 段で止まる。
std::string AnswerWriter::leaf(Cell cell) const {
  WriteOptions options;
  options.quoted = true;
  return write_term(heap_, operators_, cell, options, {});
}

std::string AnswerWriter::var_text(Cell cell) {
  const auto found = assigned_.find(cell.raw());
  const int number = found == assigned_.end() ? next_number_++ : found->second;
  if (found == assigned_.end()) assigned_.emplace(cell.raw(), number);
  return "_G" + std::to_string(number);
}

// 子は書く順の逆に積む。Close を最初に積み、子をすべて書き終えてから閉じる。
void AnswerWriter::term(Cell cell) {
  cell = heap_.deref(cell);
  if (heap_.unbound(cell)) {
    out_ += var_text(cell);
    return;
  }
  if (cell.tag() != Tag::Str && cell.tag() != Tag::Lis) {
    out_ += leaf(cell);
    return;
  }
  if (open_.contains(cell.raw())) {
    out_ += '_';
    return;
  }
  if (cell.tag() == Tag::Lis) {
    out_ += '[';
    open_list(cell);
    return;
  }
  open_.insert(cell.raw());
  push(Step::Kind::Close, cell);
  out_ += leaf(heap_.atom(heap_.functor_atom(cell)));
  out_ += '(';
  push(Step::Kind::Text, cell, ")");
  const std::uint16_t arity = heap_.functor_arity(cell);
  for (std::uint16_t index = arity; index > 0; --index) {
    push(Step::Kind::Term, heap_.arg(cell, index - 1));
    if (index > 1) push(Step::Kind::Text, cell, ",");
  }
}

// リストの節を 1 つ開き、頭と残りを積む。節ごとに開くので、途中の節へ戻る環（T = [b|T]）も _ で止まる。
void AnswerWriter::open_list(Cell cell) {
  open_.insert(cell.raw());
  push(Step::Kind::Close, cell);
  push(Step::Kind::Tail, heap_.list_tail(cell));
  push(Step::Kind::Term, heap_.list_head(cell));
}

// [a,b|T]。残りがリストならコンマで続け、[] なら閉じ、ほかは | の後に書く。開いた節へ戻った残りも | の後の _ になる。
void AnswerWriter::tail(Cell cell) {
  cell = heap_.deref(cell);
  if (cell.raw() == nil_.raw()) {
    out_ += ']';
    return;
  }
  if (cell.tag() == Tag::Lis && !open_.contains(cell.raw())) {
    out_ += ',';
    open_list(cell);
    return;
  }
  out_ += '|';
  push(Step::Kind::Text, cell, "]");
  push(Step::Kind::Term, cell);
}

}  // namespace

std::string answer_text(const Heap& heap, OperatorView operators, Cell term) {
  AnswerWriter writer(heap, operators);
  return writer.run(term);
}

}  // namespace mosa
