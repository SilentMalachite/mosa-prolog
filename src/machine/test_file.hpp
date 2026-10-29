#pragma once

// mosatest の test/3 のファイル（tests/pl）。suite.cpp が読んで流し、diff.cpp（計画 Task 11）が同じ関数で差分の
// 原文を組み立てる。テストランナーの外には出さない。

#include "mosa/cell.hpp"
#include "mosa/heap.hpp"
#include "mosa/machine.hpp"

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace mosa {

struct Exam {
  std::string name;                // test の名前。差分の報告に使う
  std::string goal;                // 読んだときの演算子で書いた字面。run_exam がそのまま問い合わせにする
  std::string expect;
  std::string canonical;           // 演算子に頼らない字面（ignore_ops）。差分の節の本体にする
  std::vector<std::string> names;  // ゴールに出る名前のある変数（出現順）。差分の節の頭部の引数の順
  std::size_t solutions = 1;       // 期待が solutions([...]) ならその長さ。差分が比べる解の数
};

// test/3 の事実か。読み込みの横取り（Impl::term_hook）が節にしない項を選ぶ。diff.cpp も同じ形で選ぶ。
[[nodiscard]] bool is_test_fact(const Heap& heap, Cell term);

// source を machine に読み込みながら、test/3 の事実だけを節にせず exams に集める。指示はファイルの順に 1 回ずつ、
// 前の節がそろった所で動く。読めなければ偽を返し、error に診断を入れる。
[[nodiscard]] bool read_test_file(Machine& machine, std::string_view source, std::string_view directory,
                                  std::string_view origin, std::vector<Exam>& exams, std::string& error);

// run_suite が流すファイル。syntax.pl（往復の例）を除く *.pl を名前の順に並べる。
[[nodiscard]] std::vector<std::filesystem::path> test_files(const std::filesystem::path& dir);

}  // namespace mosa
