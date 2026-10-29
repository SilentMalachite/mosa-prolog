#pragma once

// 標準出力と標準エラーを一時的に文字列へ向ける。Prolog の標準の流れは std::cout と std::cerr に書く
// （builtins/chars.cpp の emit_os、write_pred.cpp の emit_text）ので、差分テストは 1 件ごとにここで受けて両側で比べる。
// 入れ子にはしない。テストランナーの外には出さない。

#include <iostream>
#include <sstream>
#include <streambuf>
#include <string>

namespace mosa {

class StdCapture {
 public:
  StdCapture() : out_(std::cout.rdbuf(captured_out_.rdbuf())), err_(std::cerr.rdbuf(captured_err_.rdbuf())) {}
  ~StdCapture() {
    std::cout.rdbuf(out_);
    std::cerr.rdbuf(err_);
  }
  StdCapture(const StdCapture&) = delete;
  StdCapture& operator=(const StdCapture&) = delete;

  // 何も出ていなければ空。出ていれば、どちらの流れかを頭に付けて返す（比べた結果の表示にそのまま使う）。
  [[nodiscard]] std::string text() const {
    std::string text;
    if (!captured_out_.str().empty()) text += "\nstdout: " + captured_out_.str();
    if (!captured_err_.str().empty()) text += "\nstderr: " + captured_err_.str();
    return text;
  }

 private:
  std::ostringstream captured_out_;
  std::ostringstream captured_err_;
  std::streambuf* out_;
  std::streambuf* err_;
};

}  // namespace mosa
