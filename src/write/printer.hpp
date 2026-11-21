#pragma once

// writer.cpp と writer_list.cpp が共有する書き手。リストの書き方を writer_list.cpp に分けたので、仕事の形と
// Printer の宣言をここに置く（ファイル 800 行の上限、レビュー H-16 の前に分けた）。

#include "mosa/writer.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace mosa::printing {

inline std::uint32_t atom_of(Cell cell) { return static_cast<std::uint32_t>(cell.raw() >> 3); }

inline bool is_nil(const Heap& heap, Cell cell) {
  cell = heap.deref(cell);
  return cell.tag() == Tag::Atom && heap.atoms().name(atom_of(cell)) == "[]";
}

// 書き出しの仕事。C++ の再帰を使わず work_ に積む（ADR 0011）。積む順は書く順の逆。
enum class Step : std::uint8_t { Term, Text, Infix, Postfix, Bar, ListRest, Clear, Leave };

// 字句を書いた後、次の字句の前に空白を置くかの判断の種類。Prefix は `(` で始まる引数にも空白を置く。
enum class Space : std::uint8_t { None, Glue, Prefix };

struct Work {
  Step step = Step::Term;
  Cell term{};
  int parent = 1200;
  bool allow_equal = true;
  // 括弧を足し直す再入では、失敗した portray を繰り返さない。
  bool allow_hook = true;
  Space then = Space::None;
  // Infix と Postfix で、左の被演算子を書き始めたときの out_ の長さ。
  std::size_t mark = 0;
  // Text、Infix、Bar が始める待ちの番号。Clear はこの番号の待ちだけを取り消す。
  std::size_t id = 0;
  std::string text;
  // ListRest で、背骨が巡回して戻る入口のセル（巡回しなければリストの先頭）と、入口を書き終えたか。
  Cell back{};
  bool passed = false;
  // ListRest の安全網（Brent の方法）。portray が書く途中で束縛して背骨を後から巡回させても止める。
  Cell anchor{};
  std::size_t lap = 0;
  std::size_t power = 1;
  // 書く項の深さ（全体が 1）。max_depth の判定に使う。ListRest では、次に書く要素の 1 つ手前の要素の深さ。
  int depth = 1;
};

// 待ち: 直前の字句と、この後に最初に書く字句の間の空白。古い writer の glue(左の字面, 右の字面) と同じく、
// 右の字面が空のまま終われば空白は置かない（Clear が取り消す）。
struct Pending {
  bool active = false;
  Space kind = Space::None;
  std::optional<char32_t> left;
  std::size_t id = 0;
};

class Printer final : public WriteHeld {
 public:
  Printer(const Heap& heap, OperatorView operators, WriteOptions options,
          std::span<const VarName> names, WriteReplace replace, void* context)
      : heap_(heap), operators_(operators), options_(options), names_(names), replace_(replace),
        context_(context) {
    // replace の間の GC は名前のセルも動かす。呼び手の表は const なので、写しを GC に見せる。
    if (replace_ != nullptr) {
      own_names_.assign(names.begin(), names.end());
      names_ = own_names_;
    }
    for (const VarName& var : names) {
      if (var.name.starts_with("_G")) given_.insert(var.name);
    }
    index_names();
  }

  void collect(Cell term);
  [[nodiscard]] std::string print(Cell term);
  void visit(WriteRoots& roots) override;

 private:
  void run(Work& item);
  void settle();
  void render(const Work& item);
  void render_compound(Cell term, int parent, bool allow_equal);
  void push_op(const OpDef& op, Cell term);
  void push_prefix(const OpDef& op, Cell term, std::string name);
  void push_operand(Cell arg, int priority, bool allow_equal);
  void push_list(Cell term);
  void list_rest(const Work& item);
  [[nodiscard]] Cell spine_entry(Cell list) const;
  void push_bar(Cell rest);
  void push_functor(Cell term, std::string_view name);
  void push_term(Cell term, int parent, bool allow_equal, bool allow_hook = true, int below = 1);
  void push_text(std::string text, Space then = Space::None, std::size_t id = 0);
  void push_clear(std::size_t id);
  void push_comma(std::size_t id);
  void emit(std::string_view text);
  void await(Space kind, std::size_t id);
  [[nodiscard]] bool left_glue(std::size_t mark, std::string_view next) const;
  [[nodiscard]] bool zero_quote(std::string_view next) const;
  [[nodiscard]] std::optional<OpDef> as_operator(Cell term) const;
  [[nodiscard]] bool operator_atom(Cell term) const;
  [[nodiscard]] bool cut_at(int depth) const;
  [[nodiscard]] bool unsigned_number(Cell term) const;
  [[nodiscard]] bool leftmost_unsigned_number(Cell term, int priority, bool allow_equal) const;
  [[nodiscard]] std::string spell(std::string_view name) const;
  [[nodiscard]] std::string spell_operator(std::string_view name) const;
  [[nodiscard]] std::string spell_functor(std::string_view name) const;
  void index_names();
  [[nodiscard]] const VarName* var_name(Cell term) const;
  [[nodiscard]] std::string var_text(Cell term);
  [[nodiscard]] static bool glue(std::optional<char32_t> end, std::optional<char32_t> start);
  [[nodiscard]] static bool needs_paren(int priority, int parent, bool allow_equal);

  const Heap& heap_;
  OperatorView operators_;
  WriteOptions options_;
  std::span<const VarName> names_;
  std::vector<VarName> own_names_;
  // セルの生値から names_ の添字を引く表。線形に探すと、名前の多い答えの書き出しが 2 乗になる
  // （全体レビュー推奨4）。GC でセルが動いた後は settle が作り直す。
  std::unordered_map<std::uint64_t, std::size_t> name_index_;
  // 呼び手が付けた _G で始まる名前。作る名前はこれを飛ばす（レビュー M9）。
  std::unordered_set<std::string> given_;
  std::unordered_map<std::uint64_t, int> counts_;
  std::unordered_map<std::uint64_t, int> assigned_;
  // 今書いている項の祖先。循環項はここで _ にして止める。
  std::unordered_set<std::uint64_t> open_;
  std::vector<Work> work_;
  std::string out_;
  Pending pending_;
  std::size_t next_id_ = 1;
  std::size_t zero_end_ = std::string::npos;  // 最後に書いた整数 0 の終わりの out_ の長さ
  int next_number_ = 1;
  WriteReplace replace_ = nullptr;
  void* context_ = nullptr;
  bool aborted_ = false;
  // run が今動かしている仕事の深さ。push_term と push_list が子の深さをこれから決める。
  int depth_ = 1;
  // replace に渡している項。GC の訪問が書き換える。
  Cell hooked_{};
  // replace の間に GC が来たら真。鍵の写しを訪ね、戻った後で settle が表を作り直す。
  bool held_ = false;
  std::vector<std::pair<std::uint64_t, int>> held_counts_;
  std::vector<std::pair<std::uint64_t, int>> held_assigned_;
};

}  // namespace mosa::printing
