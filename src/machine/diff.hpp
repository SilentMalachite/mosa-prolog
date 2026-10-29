#pragma once

// 計画 Task 11（持ち主の決定 1 の B）。mosatest --diff の中身。差分の原文の組み立て（diff_source）は mosac emit --units
// と差分の実行ファイル（rt::diff_main）が同じ関数で呼ぶので、両方の側が同じバイト列を読む。実行（run_diff_file）は
// 読み方を TestLoader で受け、インタプリタは組み立てた原文を今の読み込み器で読み、翻訳はその単位の install_unit で読む。
// テストランナーの外には出さない。

#include "lgt_suite.hpp"
#include "test_loader.hpp"

#include "mosa/machine.hpp"

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace mosa {

// 試験のゴール 1 つ。節 '$mosa_diff_goal'(K, 名前のある変数...) :- ゴール. にしたものは query が空でない。
struct DiffGoal {
  std::string name;
  std::string plain;          // 節にしないときの問い合わせ（末尾の . は無い）
  std::string query;          // '$mosa_diff_goal'(K, X, Y)。節にしなかったら空
  std::string shape;          // 節にしなかった形（judge、unsupported、unreadable）。節にしたら空
  std::size_t solutions = 1;  // 比べる解の数。tests/pl の solutions([...]) の長さ、ほかは 1
  bool enumerate = false;     // solutions([...]) の試験。期待の数の後にもう 1 つ next() を比べ、余分な解を見せる
};

struct DiffSource {
  std::string source;  // 両方の側が読む原文。組み立てた原文に節を足した物
  std::vector<DiffGoal> goals;
  std::string error;  // 組み立てられなければ診断。空なら組み立てた
  bool loads = true;  // 偽なら原文がどの機械でも読めない（離れた節など）。両方の側が同じに読めず、節も単位も無い
  bool lgt = false;
  LgtFile file;  // .lgt のとき。節にした case の diff_goal を埋めてある
};

// 比べる 1 行。label は試験の名前（.pl のゴール、.lgt の case）か、load や summary のような欄の名前。
struct DiffLine {
  std::string label;
  std::string text;
};

// dir の試験のファイル。logtalk/prolog があれば対象の tests.lgt（lgt_suite_files）、無ければ *.pl（test_files）。
[[nodiscard]] std::vector<std::filesystem::path> diff_files(const std::filesystem::path& dir);
[[nodiscard]] DiffSource diff_source(const std::filesystem::path& file);
// mosatest と同じく、file から読む test/3 の事実を節にしない横取りを machine に掛ける（test/3 が助けの節と交ざる
// ファイルを、離れた節の誤りにしない）。翻訳の機械と両方の側に同じに掛け、読み込みの結果を同じにする。
void hook_test_facts(Machine& machine, const std::filesystem::path& file);
// 1 件ごとの結果（答えかエラー項、その間の標準出力と標準エラー）。読めなければ load の 1 行だけ返す。
// dir は tests/conformance のような根で、.lgt の実行器に渡す logtalk/prolog からの相対ディレクトリを作る。
[[nodiscard]] std::vector<DiffLine> run_diff_file(const std::filesystem::path& file, const std::filesystem::path& dir,
                                                  const DiffSource& source, const TestLoader& loader);

// 比べた結果の集計。ディレクトリごとに docs/agreement.{json,md} へ書く（diff_report.cpp）。
struct DiffShape {
  std::string shape;
  std::size_t count = 0;
};

struct DiffStats {
  std::size_t files = 0;
  std::size_t compared = 0;     // 比べた行の数（ゴールごとの出力と、読み込みの行）
  std::size_t agreed = 0;
  std::size_t differences = 0;  // 違った行の数。組み立てられなかったファイルも 1 と数える
  std::size_t translated_goals = 0;
  std::size_t predicates = 0;
  std::size_t translated_predicates = 0;  // 生成関数を出した述語
  std::size_t attached_predicates = 0;    // そのうち翻訳側の機械で install_unit が指紋で付けた物（ADR 0009 の 3）
  std::vector<DiffShape> untranslated;
};

void count_shape(DiffStats& stats, std::string_view shape);
// json の既存の内容に label の欄を足し（同じ label は置き換え）、json と、拡張子を .md に替えた報告を書く。
[[nodiscard]] bool write_agreement(const std::filesystem::path& json, std::string_view label, const DiffStats& stats);

}  // namespace mosa
