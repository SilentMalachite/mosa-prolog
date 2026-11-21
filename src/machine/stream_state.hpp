#pragma once
// 入出力の流れの値（Stream）と note_edit、標準の流れの字を持たないための keeps_written と drop_consumed。
// internal.hpp を 800 行の上限の内に保つため分けた。

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <memory>
#include <optional>
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
  // 別名の無い流れは nullopt。前は "" で表したので '' が別名の無い最初の流れを指した（レビュー 2026-11-20 の H-21）。
  // 引き当て（各ファイルの find_alias）は optional の == で比べるので、別名の無い流れには当たらない。
  std::optional<std::string> alias;
  std::string path;
};

// 標準の流れ（user_input、user_output、user_error）は位置を持たない（reposition(false)、position の性質も無い）ので、
// 通った字を後で見る道が無い。前は bytes に持ち続け、通った量だけメモリと、ファイルの読みと分け合うヒープ上限の
// バイト予算（stream_bytes_used）を食った（レビュー 2026-11-20 の H-26）。書きは OS へ渡すだけで bytes に置かない。
// cursor と行の数え（format の ~N が見る line_pos）は今までどおり進む。
inline bool keeps_written(const Stream& stream) { return stream.kind != Stream::Kind::Standard; }

// 標準入力の読み終えた字（cursor の前）を捨てる。読む述語の頭でだけ呼ぶ。read の途中で捨てると、give_back の had や
// U+FFFD の置き換え（read_pred.cpp の deliver）の持つ位置がずれる。peek で取り込んだ字は cursor の後ろなので残る。
inline void drop_consumed(Stream& stream) {
  if (stream.kind != Stream::Kind::Standard || stream.cursor == 0) return;
  stream.bytes.erase(0, std::min(stream.cursor, stream.bytes.size()));
  stream.cursor = 0;
}

// 書き込みの前、cursor を末尾までに収めた後に呼ぶ。cursor から後ろはファイルと違ってくる。
inline void note_edit(Stream& stream) {
  stream.unsaved = true;
  if (stream.cursor < stream.on_disk) stream.on_disk = stream.cursor;
}

}  // namespace mosa
