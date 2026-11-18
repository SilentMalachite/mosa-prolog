#pragma once
// 入出力の流れの値（Stream）と note_edit。internal.hpp を 800 行の上限の内に保つため分けた。
// 移しただけで、振る舞いは変えない。

#include <cstddef>
#include <fstream>
#include <memory>
#include <string>

namespace mosa {

// 入出力は値のベクタ。抽象クラスにはしない（ADR 0005）。添字が '$stream'(N) の N。
struct Stream {
  enum class Kind { File, Standard, Memory };
  enum class Data { Text, Binary };
  enum class Mode { Read, Write, Append };
  enum class EofAction { Error, EofCode, Reset };
  Kind kind = Kind::Memory;
  Data data = Data::Text;
  Mode mode = Mode::Read;
  EofAction eof_action = EofAction::EofCode;
  std::string bytes;
  std::size_t cursor = 0;
  std::size_t char_no = 0;
  std::size_t line = 1;
  std::size_t line_pos = 1;
  bool closed = false;
  // 終端の印を一度返したあと。開いた直後の空は cursor が末尾で、こちらは false。
  bool past_end = false;
  bool reposition = true;
  // 書いてからまだファイルへ保存していない。halt と終わりはこれが立つ流れだけを書き出す。開いただけの流れや
  // 保存の後に書いていない流れが、後で書かれたファイルを古い写しで潰さないため（レビュー M5 の A1〜A3）。
  bool unsaved = false;
  // bytes の先頭のうち、ファイルと同じだと分かっている長さ。保存はその続きだけを書き、append の元の中身を
  // 切り詰めない（Grok のレビューの Medium）。0 ならファイル全体を書き直す。
  std::size_t on_disk = 0;
  // 読みのファイルは開いた読み取り口から必要な分だけ bytes へ足す（全体レビュー推奨2）。pulled は読み取り口から
  // 読んだ累計で、bytes を書き換える splice の後でもファイルの位置を失わない（Claude のレビューの L3）。
  std::unique_ptr<std::ifstream> source;
  std::size_t pulled = 0;
  std::string alias;
  std::string path;
};

// 書き込みの前、cursor を末尾までに収めた後に呼ぶ。cursor から後ろはファイルと違ってくる。
inline void note_edit(Stream& stream) {
  stream.unsaved = true;
  if (stream.cursor < stream.on_disk) stream.on_disk = stream.cursor;
}

}  // namespace mosa
