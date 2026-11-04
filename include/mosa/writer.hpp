#pragma once

// 空白は、隣のトークンが 1 つに融合するときと、コンマの直後だけ置く。
// same_term はユニフィケーションを呼ばない。変数は名前か出現の対応で見る。

#include "mosa/heap.hpp"
#include "mosa/operator_table.hpp"
#include "mosa/reader.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace mosa {

struct WriteOptions {
  bool quoted = true;
  bool ignore_ops = false;
  // 7.10.4。C++ の既定は偽のまま。write/1 が真にして渡す。
  bool numbervars = false;
  // 1 度だけ出る名前の無い変数を _ と書く。利用者向けの書き出しは偽で、すべての変数を _G1 などと書く
  // （iso-notes 7.10.5 a の決定 B）。スイートの実行器は、書いた字面を貼り合わせて読み直すので真にする。
  bool underscore_once = false;
  // 引数とリストの要素を区切るコンマの後に、糊付けが要らなくても空白を置く。mosac の生成コードのコメントが使う。
  bool space_args = false;
  // 旗 japanese_punctuation の値（SPEC 第4節、レビュー M8）。真なら、全角記号モードの読み手が別の字句に読む原子を引用する。
  // 書いた字面を同じ旗で読み戻す呼び手（writeq などの述語、REPL）が機械の旗を写す。
  bool fullwidth = false;
};

// Swap の out がその項の字面で、子は書かない。Stop は残りの書き出しを捨てる。
enum class WriteCut { Keep, Swap, Stop };

// writer が握るセルを訪ねる側。portray の入れ子で GC が動くとき、呼び手がこれを GC の訪問につなぐ（ADR 0011）。
class WriteRoots {
 public:
  virtual void cell(Cell& slot) = 0;
  virtual void weak(std::uint64_t& raw) = 0;  // 辿らない。生きていれば新しい語、死んでいれば 0

 protected:
  WriteRoots() = default;
  WriteRoots(const WriteRoots&) = default;
  WriteRoots& operator=(const WriteRoots&) = default;
  ~WriteRoots() = default;
};

// replace の間に writer の状態を見せる口。visit は GC が呼び、writer は replace から戻った後で番地の表を直す。
class WriteHeld {
 public:
  virtual void visit(WriteRoots& roots) = 0;

 protected:
  WriteHeld() = default;
  WriteHeld(const WriteHeld&) = default;
  WriteHeld& operator=(const WriteHeld&) = default;
  ~WriteHeld() = default;
};

using WriteReplace = WriteCut (*)(void* context, Cell term, std::string& out, WriteHeld& held);

[[nodiscard]] std::string write_term(const Heap& heap, OperatorView operators, Cell term,
                                     WriteOptions options, std::span<const VarName> names,
                                     WriteReplace replace = nullptr, void* context = nullptr);

// terms を 1 つの番号付けで続けて書き、項ごとの字面を返す。作る名前（_G1 など）は、同じ変数には項をまたいでも
// 同じ名前、違う変数には違う名前になる。underscore_once の「1 度だけ」は全部の項を通して数える。トップレベルの
// 答えが束縛ごとに書くのに使う（レビュー M22）。portray を呼ばないので、書く間に GC は動かない。
[[nodiscard]] std::vector<std::string> write_terms(const Heap& heap, OperatorView operators, std::span<const Cell> terms,
                                                   WriteOptions options, std::span<const VarName> names);

[[nodiscard]] bool same_term(const Heap& left_heap, Cell left, std::span<const VarName> left_names,
                             const Heap& right_heap, Cell right, std::span<const VarName> right_names);

}
