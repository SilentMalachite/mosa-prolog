#pragma once

// mosatest --manual（リリース 0.1.0 Task 4）。docs/manual/{en,ja}/*.md の例を抜き出して動かし、書いてある出力と
// 1 字ずつ比べる。印の書き方は tests/manual/README.md。読み取りは src/machine/manual_parse.cpp、動かして比べるのは
// src/machine/manual_check.cpp。試験は tests/cpp/manual_parse_test.cpp と tests/manual の見本（tests/tests.cmake）。
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace mosa {

// Repl は ?- で始まる text の check:、Console は console の check:、File は file:、Skip は skip:。
enum class ExampleKind { Repl, Console, File, Skip };

struct ManualExample {
  std::string id;
  ExampleKind kind = ExampleKind::Skip;
  std::string file;                  // parse_manual に渡した名前。診断の file:line に出す
  std::size_t line = 0;              // 開きの囲いの行（1 から）
  std::string body;                  // 囲いの中。どの行も \n で終わり、\r は除いてある
  std::optional<std::string> input;  // 複数行の check: に書いた入力（\r を除いた行ごとに \n）
  std::string file_name;             // file: の NAME
};

// std::expected の代わり（C++20）。errors が空なら examples がそのファイルの例のすべて。
struct ParsedManual {
  std::vector<ManualExample> examples;
  std::vector<std::string> errors;  // "file:line: 文面"。文面は lib/messages_*.pl の鍵（ADR 0007）
};

[[nodiscard]] ParsedManual parse_manual(std::string_view text, std::string_view file);

// console の囲いの 1 つのコマンド。argv[0] は mosa、mosac、mosatest か ./NAME のまま（置き換えは動かす側）。
struct ConsoleCommand {
  std::vector<std::string> argv;
  std::string output;      // 次の "$ " の行までの行。どの行も \n で終わる
  std::size_t offset = 0;  // 囲いの中での "$ " の行の位置（0 から）
};

// "$ " の行で区切る。引数は空白で分け、'…' と "…" はまとめる。最初の "$ " より前の行は捨てる。
// 引用が閉じない行と、シェルが特別に扱う字を引用の外か "…" の中に持つ行の argv は空にする（parse_manual がその行を
// 誤りにするので、正しく読めた囲いでは起きない）。
[[nodiscard]] std::vector<ConsoleCommand> console_commands(std::string_view body);
// 書き換えの無い入力。"?- " で始まる行の残りを、1 行ずつ \n を付けて並べる。
[[nodiscard]] std::string default_input(std::string_view expected);
// 版の突き合わせに使う並び。"check:a skip:b file:c" の形で、Repl と Console はどちらも check。
[[nodiscard]] std::string id_sequence(std::span<const ManualExample> examples);

struct ManualArgs {
  std::string dir;                   // en/ と ja/ を持つディレクトリ
  std::string mosa;                  // 空なら動いている mosatest の隣の mosa
  std::string work = "tests/tmp/manual";
  bool ok = false;
};

// mosatest --manual の後ろ。<ディレクトリ> [--mosa <パス>] [--work <ディレクトリ>]。
[[nodiscard]] ManualArgs parse_manual_args(std::span<const std::string_view> args);
// 例を動かして比べる。失敗ごとに 1 塊と、最後に件数の 1 行を標準出力に出す。終了コードは失敗が無ければ 0、
// あれば 1。<ディレクトリ> に en/ か ja/ が無いか読めないとき、どこにも check: の例が無いときは 2（使い方の誤りと
// 同じ。何も確かめずに通ったことにしない）。
[[nodiscard]] int run_manual(const ManualArgs& args);

}
