#pragma once

// 空白は、隣のトークンが 1 つに融合するときと、コンマの直後だけ置く。
// same_term はユニフィケーションを呼ばない。変数は名前か出現の対応で見る。

#include "mosa/heap.hpp"
#include "mosa/operator_table.hpp"
#include "mosa/reader.hpp"

#include <span>
#include <string>

namespace mosa {

struct WriteOptions {
  bool quoted = true;
  bool ignore_ops = false;
  // 7.10.4。C++ の既定は偽のまま。write/1 が真にして渡す。
  bool numbervars = false;
};

// Swap の out がその項の字面で、子は書かない。Stop は残りの書き出しを捨てる。
enum class WriteCut { Keep, Swap, Stop };

using WriteReplace = WriteCut (*)(void* context, Cell term, std::string& out);

[[nodiscard]] std::string write_term(const Heap& heap, OperatorView operators, Cell term,
                                     WriteOptions options, std::span<const VarName> names,
                                     WriteReplace replace = nullptr, void* context = nullptr);

[[nodiscard]] bool same_term(const Heap& left_heap, Cell left, std::span<const VarName> left_names,
                             const Heap& right_heap, Cell right, std::span<const VarName> right_names);

}
