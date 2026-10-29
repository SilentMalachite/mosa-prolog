[English](README.md) | 日本語

# Mosa Prolog

[![CI](https://github.com/SilentMalachite/mosa-prolog/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/SilentMalachite/mosa-prolog/actions/workflows/ci.yml)
[![ISO conformance](https://img.shields.io/endpoint?url=https%3A%2F%2Fraw.githubusercontent.com%2FSilentMalachite%2Fmosa-prolog%2Fmain%2Fdocs%2Fconformance.json)](docs/conformance.md)
[![mosac diff](https://img.shields.io/endpoint?url=https%3A%2F%2Fraw.githubusercontent.com%2FSilentMalachite%2Fmosa-prolog%2Fmain%2Fdocs%2Fagreement.json)](docs/agreement.md)
[![Phase](https://img.shields.io/badge/phase-6_done-blue)](docs/STATUS.md)
[![Version](https://img.shields.io/badge/version-0.0.1-blue)](CMakeLists.txt)
[![ISO/IEC 13211-1](https://img.shields.io/badge/ISO%2FIEC-13211--1-informational)](docs/SPEC.md)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus&logoColor=white)](CMakeLists.txt)
[![CMake](https://img.shields.io/badge/CMake-3.25%2B-064F8C?logo=cmake&logoColor=white)](CMakePresets.json)
[![Platforms](https://img.shields.io/badge/platform-macOS_%7C_Linux_%7C_Windows-lightgrey)](.github/workflows/ci.yml)
[![Unicode](https://img.shields.io/badge/Unicode-18.0.0-orange)](docs/unicode-version.md)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue)](LICENSE)

この文書は [英語版](README.md) の日本語訳です。食い違うときは英語版に従います。

ISO/IEC 13211-1 に沿った Prolog 処理系です。C++20 で書いており、依存は C++20 標準ライブラリと CMake だけです。日本語のアトムと UTF-8 の入出力を最初から扱います。

## 名前の由来

Mosa は **M**odular **O**ptimized **S**ymbolic **A**rchitecture の略です。Mosa Prolog は Modular Optimized Symbolic Architecture Prolog を縮めた名前です。

## 現在の状態

フェーズ 6（`mosac`）まで終わっています。フェーズ 7（GC、マニュアル、配布パッケージ）はオーナーの合図を待っています。進み具合は [`docs/STATUS.md`](docs/STATUS.md) にあります。

| フェーズ | 内容 | 状態 |
| --- | --- | --- |
| 0 | 骨組み（CMake、CI、テストハーネス） | 完了 |
| 1 | 項、リーダ、ライタ | 完了 |
| 2 | 実行の核（IR、節コンパイラ、選択点、カット、`catch/throw`、REPL、`consult/1`） | 完了 |
| 3 | 組込述語、ストリーム、フラグ、ディレクティブ、エラー体系 | 完了 |
| 4 | モジュールと `library(*)` | 完了 |
| 5 | 準拠率の追い込み、全角記号モード、メッセージの日本語化 | 完了 |
| 6 | `mosac`（Prolog から C++20 への変換とビルド）、差分テスト | 完了 |
| 7 | GC、マニュアル、配布パッケージ | 未着手 |

「mosac diff」のバッジは、`mosatest --diff` が `tests/pl` と `tests/conformance` で測った、インタプリタと `mosac` の翻訳の一致率です（生成物 `docs/agreement.md`）。

## 必要なもの

- CMake 3.25 以上と Ninja（macOS と Linux）
- 次のどれかのコンパイラ
  - macOS: Apple Clang 15 以上
  - Linux: GCC 13 以上、または Clang 17 以上
  - Windows: Visual Studio 2022（MSVC v17.8 以上）

## ビルドとテスト

OS に合うプリセットを選びます。プリセットは `macos-clang`、`linux-gcc`、`linux-clang`、`windows-msvc` の 4 つです。

```sh
cmake --preset macos-clang
cmake --build --preset macos-clang
ctest --preset macos-clang
```

実行ファイルは `build/` に出ます。Windows では `build/Release/` です。

## 使い方

### `mosa`（インタプリタ）

```sh
mosa [ファイル...] [-g ゴール] [-t halt] [-- 引数...]
```

端末から引数なしで起動すると対話モードになります。`-g` のゴールの末尾の `.` は、SWI-Prolog と同じく付けても省いてもかまいません。読めないゴールは構文エラーを表示し、`-t halt` のときは終了コード 2 で終わります。

`--` より後ろの引数はすべてプログラムに渡され、読み取り専用の旗 `argv`（アトムのリスト。`--` が無ければ `[]`）で読めます。`:- initialization(main, main).` を書いたファイルは、読み込みのあとで `main` を 1 回呼んで終了します。終了コードは、成功が 0、失敗が 1、捕捉されない例外は文面を表示して 2、`main` が `halt/1` したときはその値です。対話モードにも `-g` にも進みません。

```console
$ ./build/mosa tests/pl/nrev.pl -g "nrev([1,2,3], R)." -t halt
R = [3,2,1].
$ ./build/mosa -g "atom_length('𠮷野家', N)." -t halt
N = 3.
```

文字はすべて Unicode のスカラー値 1 個として数えます。`𠮷` は 1 文字です。

標準入力が端末でないとき、その入力は Prolog の `user_input` のものです。パイプで渡したデータをゴールが読めます。

```console
$ printf 'foo.\n' | ./build/mosa -g "read(X)." -t halt
X = foo.
```

### `mosac`（Prolog から C++20 への変換とビルド）

`mosac` は Prolog のファイルを単独で動く実行ファイルにします。必要なのは C++20 のコンパイラ 1 つだけです（CMake は有っても無くても構いません）。できた実行ファイルはコンパイラも `mosac` も Prolog のファイルも要りません。

```sh
mosac build app.pl -o app          # 変換、コンパイル、リンク（Windows では app.exe）
mosac run app.pl -- 引数...         # ./.mosac-cache で作り、写しを一時ディレクトリで実行し、終了コードをそのまま返す
mosac emit app.pl -o out/          # C++ のソースと CMakeLists.txt だけを出す
mosac build app.pl --interactive   # app.pl を読み込んだ状態で REPL が立ち上がる実行ファイルにする
mosac toolchain                    # コンパイラ、CMake、同梱の実行時ライブラリを調べ、困ったら対処を示す
```

そのほかの旗: `--debug` は `-O0 -g` でビルドし、プログラム自身の節を追跡します（同梱ライブラリの単位は追跡しません）。`--verbose` はコンパイラの命令を表示し、`--clean` はビルドの前に `.mosac-cache/` を消します。`mosac run --keep` は実行ファイルの写しを置いた一時ディレクトリを残し、`--runtime-source` は同梱の静的ライブラリではなくソースから実行時ライブラリをビルドします。`mosac emit --library` と `mosac emit --units` は同梱ライブラリのビルドと `mosatest --diff` のためにあり、普通は使いません。

コンパイラは `--cxx`、`MOSA_CXX`、`CXX`、OS の既定の順に探します（macOS は `clang++`、Linux は `g++` の次に `clang++`、Windows は `vswhere` で見つけた MSVC）。CMake が無いとき、または `MOSA_NO_CMAKE=1` のときは、`mosac` が生成したファイルを 1 つずつ直接コンパイルします。`MOSA_WERROR=1` は生成した C++ を警告をエラーにしてコンパイルします（テストと CI が立てます）。`:- initialization(main, main).` があれば、実行ファイルは `mosa` と同じく `main` を実行して終わります。`--interactive` ではそれを実行せず REPL を立ち上げます。どこにも定義の無い述語を呼んでいると、`mosac emit` と `mosac build` は止まり、`:- dynamic(名前/アリティ).` を案内します。途中のファイルは `.mosac-cache/` に置きます。詳しくは `docs/SPEC.ja.md` の第 6 節です。

### `mosatest`（テストランナー）

```sh
mosatest tests/pl/                    # 自前テスト（tests/pl/*.pl）
mosatest --suite tests/conformance    # 準拠スイート。docs/conformance.md と .json を書き出す
mosatest --suite <ディレクトリ> --report tests/tmp/x.md   # 別のスイート。x.md と x.json を書き出す
mosatest --roundtrip tests/pl/syntax.pl   # 読み書きの往復
mosatest --diff tests/pl              # インタプリタと mosac の出力を比べる。docs/agreement.md と .json を書き出す
mosatest --bench bench/nrev.pl --count 20000 --report tests/tmp/bench.json   # インタプリタと mosac の時間（5 回の中央値）
```

`--report` を省けるのは、リポジトリ直下で同梱の `tests/conformance`（`--diff` では `tests/pl` も）を渡したときだけです。`--diff` は `--work`、`--cmake`、`--config` も取ります。`--bench` は [`docs/bench.ja.md`](docs/bench.ja.md) を見てください。別のスイートで `docs/` の報告書とバッジを上書きしないためです。

## 日本語の変数

日本語には大文字と小文字の区別がありません。そのため `親(太郎, 一郎).` の `太郎` はアトムです。日本語名の変数は `_太郎` のように下線で始めます。詳しい字句の規則は [`docs/SPEC.ja.md`](docs/SPEC.ja.md) 第 4 節にあります。

```prolog
親(太郎, 一郎).
親(一郎, 花子).
祖父母(_祖, _孫) :- 親(_祖, _親), 親(_親, _孫).
```

```console
$ ./build/mosa 家系.pl -g "祖父母(X, 花子)." -t halt
X = 太郎.
```

## 準拠率

同梱した Logtalk の準拠スイート（`tests/conformance/logtalk/`）の合格率は上のバッジのとおりです。フェーズ 3 の完了時点で 2171 件中 2028 件（93.4%）、フェーズ 4 の完了時点で 2041 件（94.0%）、フェーズ 5 の完了時点で 2135 件（98.3%）が通りました。不合格の一覧は [`docs/conformance.md`](docs/conformance.md) にあります。`mosatest --suite` はこのファイルと、バッジ用の [`docs/conformance.json`](docs/conformance.json)（shields.io の endpoint 形式）を書き出します。どちらも手で編集しません。

準拠率のバッジは `main` の `docs/conformance.json` を読むので、コミットするたびに自動で変わります。CI は 4 つのジョブすべてでスイートを実行して合格率を Job Summary に出し、macOS のジョブでは生成した JSON がコミット済みのものと違えば失敗します。スイートの結果が変わる変更では、`mosatest --suite tests/conformance` を実行して JSON も一緒にコミットしてください。

「mosac diff」のバッジも同じ仕組みで `main` の [`docs/agreement.json`](docs/agreement.json) を読みます。CI は 3 つの OS で差分テスト（`ctest -L diff`、つまり `mosatest --diff tests/pl` と `mosatest --diff tests/conformance`。生成した C++ は警告を誤りにしてコンパイルします）を流して一致率を Job Summary に出し、macOS のジョブでは生成した JSON がコミット済みのものと違えば失敗します。各 OS のジョブは `mosac toolchain` と `mosac build tests/mosac/hello.pl` も実行し、別のジョブがその実行ファイルを落として、素の `ubuntu:24.04` コンテナで動かし、macOS では `otool -L` の依存が `/usr/lib/` と `/System/` だけ、Windows では `dumpbin /dependents` の依存が OS の DLL だけであることを確かめます。一致率が変わる変更では、`mosatest --diff tests/pl` と `mosatest --diff tests/conformance` を実行して JSON も一緒にコミットしてください。

## テストの構成

| 層 | 置き場所 | 実行方法 |
| --- | --- | --- |
| C++ 部品テスト | `tests/cpp/` | `ctest` |
| Prolog 自前テスト | `tests/pl/` | `mosatest tests/pl/`（`ctest` からも動く） |
| ISO 準拠スイート | `tests/conformance/` | `mosatest --suite tests/conformance` |
| 差分テスト | `tests/pl/`、`tests/conformance/` | `mosatest --diff tests/pl`（`ctest` からも動く。ラベル `diff`） |

## 文書

英語版が正で、日本語訳は同じ場所に `*.ja.md` として置きます。

| 文書 | 中身 |
| --- | --- |
| [`docs/SPEC.ja.md`](docs/SPEC.ja.md) | 仕様の本文（目的、ISO の範囲、日本語、モジュール、アーキテクチャ、テスト、フェーズ） |
| [`docs/STATUS.md`](docs/STATUS.md) | 今のフェーズと次の作業（英語のみ） |
| [`docs/adr/`](docs/adr/README.ja.md) | 後から変えにくい設計の決定記録 |
| [`docs/iso-notes.ja.md`](docs/iso-notes.ja.md) | 規格の解釈メモ（条番号付き） |
| [`docs/conformance.md`](docs/conformance.md) | 準拠スイートの結果（自動生成、日本語のみ） |
| [`docs/conformance.json`](docs/conformance.json) | 準拠率バッジの元データ（自動生成） |
| [`docs/agreement.md`](docs/agreement.md) | `mosatest --diff` が測ったインタプリタと翻訳の一致（自動生成） |
| [`docs/agreement.json`](docs/agreement.json) | 一致率バッジの元データ（自動生成） |
| [`docs/unicode-version.md`](docs/unicode-version.md) | 文字分類表が使う Unicode の版 |
| [`docs/bench.ja.md`](docs/bench.ja.md) | ベンチマークの記録 |
| [`docs/bench.json`](docs/bench.json) | macOS の CI が比べるベンチマークの結果（自動生成） |
| [`AGENTS.ja.md`](AGENTS.ja.md) | コーディングエージェント向けの作業規則 |

## ディレクトリ

```text
include/mosa/   公開ヘッダ
src/            core, read, write, machine, builtins, module, codegen, platform（compile は予約で空）
lib/            Prolog 製の標準ライブラリとメッセージカタログ
bench/          mosatest --bench 用のベンチマークのプログラム
tests/          cpp, pl, mosac, conformance, tmp
tools/          開発者用の生成スクリプト（ビルドには不要）
docs/           仕様、状態、ADR、解釈メモ
```

## ライセンス

Apache-2.0 です。本文は [`LICENSE`](LICENSE)、著作権表示は [`NOTICE`](NOTICE) にあります。`tests/conformance/logtalk/` も同じライセンスで、Logtalk の著作権表示はそこの `NOTICE.txt` にあります。
