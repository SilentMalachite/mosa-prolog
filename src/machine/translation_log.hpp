#pragma once
// mosac の翻訳の読み込みで書き留める物（TranslationLog、EmbeddedSource）。internal.hpp を 800 行の上限の内に
// 保つため分けた。移しただけで、振る舞いは変えない。

#include "pred_table.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace mosa {

// mosac の翻訳の読み込み（計画 Task 5）。Impl::translation が在る間、読み込み器は宣言だけを実行し
// （一般の指示のゴールと initialization/1 は呼ばない）、開いたファイルの原文と節の位置を書き留める。
struct TranslationLog {
  struct File {
    std::string key;  // 正規化したパス。実行ファイルの埋め込みの鍵になる
    std::string text;
    std::size_t line = 0;  // 最初の項の位置。module/2 の診断に使う
    std::size_t column = 0;
    bool library = false;  // library(_) で開いたファイルと、そこから開いたファイル（同梱の単位として結ぶ。Task 10）
    bool skipped_goal = false;  // 飛ばした指示のゴールがある。そのゴールの op/3 が後の項の読みを変えうる
    // 飛ばしたゴールの後の構文エラーか、起動時に任せた項の失敗（自分のモジュールを持たない、読んだテキストのものも）。
    // 述語は IR のまま動かす
    bool opaque = false;
    bool untaken_branch = false;  // このファイルか、ここから開いたファイルで、条件が項を読み飛ばした（ADR 0027）
    bool in_branch = false;       // 開いた時に、読んでいるテキストのどれかに閉じていない if があった（ADR 0027）
    // ライブラリなら、その根（library(Name) を見つけた $MOSA_LIB か lib/）の正規化したパス。埋め込みの鍵は
    // この根からの相対を擬似ディレクトリ library/ の下に置いた物（library/sub/util.pl）。プログラムのファイルは空。
    std::string library_root{};
    // このファイルが module/2 で宣言したモジュール（読んだ機械の原子）。別のファイルが後で同じ名前を宣言して奪っても
    // 残る（Module::file は奪った側に替わる）。同梱ライブラリの単位の名前空間はこれから作る（H-28 の敵対的レビュー A1）。
    std::optional<std::uint32_t> module{};
  };
  // library(Name) で開いたファイルと、その探索の根。FileNote が開いたファイルを library と印を付けるのに使う。
  struct LibraryOpen {
    std::string key;
    std::string root;
  };
  struct Site {
    PredKey pred;
    std::size_t clause = 0;
    std::size_t file = 0;
    std::size_t line = 0;
  };
  std::vector<File> files;
  std::vector<Site> sites;
  std::vector<LibraryOpen> library_opens;
  // 読んだ項のどこかに字面で書いた use_module(library(N)) と use_module(library(N), _) の N（問い 2 の A）。
  std::vector<std::string> goal_libraries;
  std::size_t current = static_cast<std::size_t>(-1);  // 読んでいるファイルの files の添字
  std::size_t open_conditions = 0;  // 読んでいるテキスト全部の、閉じていない if の数（CondFrames が足し引きする）
  // 起動時には別の分岐を取りうるブロック（CondFrames::Frame::doubtful）を、この読み込みのどこかで閉じた。その後は
  // 節の並びと取り込みが起動時と違いうる（I2 の直し 4 回目）。
  bool closed_doubtful = false;
  // take_term が読んでいる節の行き先のモジュール。限定した頭（r:p）の節を起動時に任せるとき、r を宣言したファイルにも
  // 印を付けるのに使う（I2 の直し 4 回目のレビュー A3）。指令と、節にならなかった項では空。
  std::optional<std::uint32_t> clause_module;
};

// 実行ファイルに埋め込んだ原文。include/1 などはファイルシステムより先にこれを正規化したパスで引く。
struct EmbeddedSource {
  std::string key;
  std::string text;
};

}  // namespace mosa
