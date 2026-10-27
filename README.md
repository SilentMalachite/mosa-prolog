# Mosa Prolog

[![CI](https://github.com/SilentMalachite/mosa-prolog/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/SilentMalachite/mosa-prolog/actions/workflows/ci.yml)
[![ISO conformance](https://img.shields.io/endpoint?url=https%3A%2F%2Fraw.githubusercontent.com%2FSilentMalachite%2Fmosa-prolog%2Fmain%2Fdocs%2Fconformance.json)](docs/conformance.md)
[![mosac diff](https://img.shields.io/badge/mosac_diff-not_measured-lightgrey)](docs/SPEC.md)
[![Phase](https://img.shields.io/badge/phase-3_done-blue)](docs/STATUS.md)
[![Version](https://img.shields.io/badge/version-0.0.1-blue)](CMakeLists.txt)
[![ISO/IEC 13211-1](https://img.shields.io/badge/ISO%2FIEC-13211--1-informational)](docs/SPEC.md)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus&logoColor=white)](CMakeLists.txt)
[![CMake](https://img.shields.io/badge/CMake-3.25%2B-064F8C?logo=cmake&logoColor=white)](CMakePresets.json)
[![Platforms](https://img.shields.io/badge/platform-macOS_%7C_Linux_%7C_Windows-lightgrey)](.github/workflows/ci.yml)
[![Unicode](https://img.shields.io/badge/Unicode-18.0.0-orange)](docs/unicode-version.md)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue)](LICENSE)

ISO/IEC 13211-1 に沿った Prolog 処理系です。C++20 で書いており、依存は C++20 標準ライブラリと CMake だけです。日本語のアトムと UTF-8 の入出力を最初から扱います。

## 名前の由来

Mosa は **M**odular **O**ptimized **S**ymbolic **A**rchitecture の略です。Mosa Prolog は Modular Optimized Symbolic Architecture Prolog を縮めた名前です。

## 現在の状態

フェーズ 3（組込述語の拡充）まで終わっています。次はフェーズ 4（モジュール）です。進み具合は [`docs/STATUS.md`](docs/STATUS.md) にあります。

| フェーズ | 内容 | 状態 |
| --- | --- | --- |
| 0 | 骨組み（CMake、CI、テストハーネス） | 完了 |
| 1 | 項、リーダ、ライタ | 完了 |
| 2 | 実行の核（IR、節コンパイラ、選択点、カット、`catch/throw`、REPL、`consult/1`） | 完了 |
| 3 | 組込述語、ストリーム、フラグ、ディレクティブ、エラー体系 | 完了 |
| 4 | モジュールと `library(*)` | 未着手 |
| 5 | 準拠率の追い込み、全角記号モード、メッセージの日本語化 | 未着手 |
| 6 | `mosac`（Prolog から C++20 への変換とビルド）、差分テスト | 未着手 |
| 7 | GC、マニュアル、配布パッケージ | 未着手 |

`mosac` は今のところ `--version` だけに応えます。変換器はフェーズ 6 で作ります。そのため「mosac diff」のバッジは未計測です。

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
mosa [ファイル...] [-g ゴール] [-t halt]
```

端末から引数なしで起動すると対話モードになります。`-g` のゴールは末尾に `.` を付けます。

```console
$ ./build/mosa tests/pl/nrev.pl -g "nrev([1,2,3], R)." -t halt
R = [3,2,1].
$ ./build/mosa -g "atom_length('𠮷野家', N)." -t halt
N = 3.
```

文字はすべて Unicode のスカラー値 1 個として数えます。`𠮷` は 1 文字です。

### `mosatest`（テストランナー）

```sh
mosatest tests/pl/                    # 自前テスト（tests/pl/*.pl）
mosatest --suite tests/conformance    # 準拠スイート。docs/conformance.md と .json を書き出す
mosatest --suite <ディレクトリ> --report tests/tmp/x.md   # 別のスイート。x.md と x.json を書き出す
mosatest --roundtrip tests/pl/syntax.pl   # 読み書きの往復
```

`--report` を省けるのは、リポジトリ直下で同梱の `tests/conformance` を渡したときだけです。別のスイートで `docs/` の報告書とバッジを上書きしないためです。

## 日本語の変数

日本語には大文字と小文字の区別がありません。そのため `親(太郎, 一郎).` の `太郎` はアトムです。日本語名の変数は `_太郎` のように下線で始めます。詳しい字句の規則は [`docs/SPEC.md`](docs/SPEC.md) 第 4 節にあります。

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

同梱した Logtalk の準拠スイート（`tests/conformance/logtalk/`）の合格率は上のバッジのとおりです。フェーズ 3 の完了時点で 2171 件中 2028 件（93.4%）が通りました。不合格の一覧は [`docs/conformance.md`](docs/conformance.md) にあります。`mosatest --suite` はこのファイルと、バッジ用の [`docs/conformance.json`](docs/conformance.json)（shields.io の endpoint 形式）を書き出します。どちらも手で編集しません。

準拠率のバッジは `main` の `docs/conformance.json` を読むので、コミットするたびに自動で変わります。CI は 4 つのジョブすべてでスイートを実行して合格率を Job Summary に出し、macOS のジョブでは生成した JSON がコミット済みのものと違えば失敗します。スイートの結果が変わる変更では、`mosatest --suite tests/conformance` を実行して JSON も一緒にコミットしてください。

## テストの構成

| 層 | 置き場所 | 実行方法 |
| --- | --- | --- |
| C++ 部品テスト | `tests/cpp/` | `ctest` |
| Prolog 自前テスト | `tests/pl/` | `mosatest tests/pl/`（`ctest` からも動く） |
| ISO 準拠スイート | `tests/conformance/` | `mosatest --suite tests/conformance` |
| 差分テスト | フェーズ 6 で追加 | `mosatest --diff` |

## 文書

| 文書 | 中身 |
| --- | --- |
| [`docs/SPEC.md`](docs/SPEC.md) | 仕様の本文（目的、ISO の範囲、日本語、モジュール、アーキテクチャ、テスト、フェーズ） |
| [`docs/STATUS.md`](docs/STATUS.md) | 今のフェーズと次の作業 |
| [`docs/adr/`](docs/adr/README.md) | 後から変えにくい設計の決定記録 |
| [`docs/iso-notes.md`](docs/iso-notes.md) | 規格の解釈メモ（条番号付き） |
| [`docs/conformance.md`](docs/conformance.md) | 準拠スイートの結果（自動生成） |
| [`docs/conformance.json`](docs/conformance.json) | 準拠率バッジの元データ（自動生成） |
| [`docs/unicode-version.md`](docs/unicode-version.md) | 文字分類表が使う Unicode の版 |
| [`docs/bench.md`](docs/bench.md) | ベンチマークの記録 |
| [`AGENTS.md`](AGENTS.md) | コーディングエージェント向けの作業規則 |

## ディレクトリ

```text
include/mosa/   公開ヘッダ
src/            core, read, write, machine, builtins, platform（module, compile, codegen は後のフェーズ）
lib/            Prolog 製の標準ライブラリ（フェーズ 4）
tests/          cpp, pl, conformance
tools/          開発者用の生成スクリプト（ビルドには不要）
docs/           仕様、状態、ADR、解釈メモ
```

## ライセンス

Apache-2.0 です。本文は [`LICENSE`](LICENSE)、著作権表示は [`NOTICE`](NOTICE) にあります。`tests/conformance/logtalk/` も同じライセンスで、Logtalk の著作権表示はそこの `NOTICE.txt` にあります。
