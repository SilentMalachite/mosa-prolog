#pragma once

// mosatest --manual がマニュアルの 1 行を GitHub と同じ形に読むための関数（リリース 0.1.0 Task 5 の修正 4 の追加で
// manual_parse.cpp から分けた）。行の並びを読む Parser（manual_parse.cpp）が呼び、ここの関数は行をまたぐ状態を
// 持たない（TagScan だけは段落と注釈のブロックをまたいで読む）。規則は tests/manual/README.md。テストランナーの
// 外には出さない。

#include "mosa/manual.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mosa::markdown {

struct Lines {
  std::vector<std::string_view> text;
  std::vector<bool> lone_cr;  // その行は \n の付かない \r で終わった
};

[[nodiscard]] Lines split_marked_lines(std::string_view text);
[[nodiscard]] std::vector<std::string_view> split_lines(std::string_view text);
[[nodiscard]] bool blank(char letter);
[[nodiscard]] std::string_view trim(std::string_view text);
[[nodiscard]] std::size_t list_marker(std::string_view text);
[[nodiscard]] bool heading(std::string_view line);
[[nodiscard]] std::optional<PredicateIndicator> table_entry(std::string_view line);
[[nodiscard]] std::vector<std::string_view> table_cells(std::string_view row);
[[nodiscard]] bool delimiter_row(std::string_view row);
[[nodiscard]] bool angle_outside_code(std::string_view row);
[[nodiscard]] std::optional<unsigned char> control_byte(std::string_view line, bool lone_cr);
[[nodiscard]] std::string code_point(unsigned char byte);
[[nodiscard]] std::string shown(std::string_view line);
[[nodiscard]] bool starts_with_folded(std::string_view text, std::string_view prefix);
[[nodiscard]] bool comment_ends_on(std::string_view line);

// 生の HTML のタグを持つ行を探す（修正 4 の追加）。タグは < の後ろに英字、/、?、! が続くもの（<!-- を除く）で、
// GitHub は HTML として映す（<details> や <script> は後ろの表を隠す）。コードスパン、囲い、<!-- … --> の注釈の中は
// 除くが、どこまでがそうかは GitHub の読み方に合わせ、読み方が決まらないところは除かない。Parser は囲いと区切りの
// 外の行を前から渡し、渡さなかった行があればそこで読みを切る。
class TagScan {
 public:
  void line(std::size_t index, std::string_view text);
  [[nodiscard]] std::vector<std::size_t> finish();  // タグを持つ行の位置（前から）

 private:
  enum class Block { None, Sure, Maybe };
  void reset();

  std::vector<std::size_t> found_;
  std::size_t next_ = 0;       // 続きとして読む次の行の位置
  Block block_ = Block::None;  // <!-- で始まった HTML のブロックの中（--> を含む行まで）
  bool open_ = false;          // ブロックの中の注釈が閉じていない
  bool loose_ = false;         // 段落の ` の組みが行の中だけでは決まらない
};

}  // namespace mosa::markdown
