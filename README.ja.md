[English](README.md) | 日本語

# Mosa Prolog

[![CI](https://github.com/SilentMalachite/mosa-prolog/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/SilentMalachite/mosa-prolog/actions/workflows/ci.yml)
[![ISO conformance](https://img.shields.io/endpoint?url=https%3A%2F%2Fraw.githubusercontent.com%2FSilentMalachite%2Fmosa-prolog%2Fmain%2Fdocs%2Fconformance.json)](https://github.com/SilentMalachite/mosa-prolog/blob/main/docs/conformance.md)
[![mosac diff](https://img.shields.io/endpoint?url=https%3A%2F%2Fraw.githubusercontent.com%2FSilentMalachite%2Fmosa-prolog%2Fmain%2Fdocs%2Fagreement.json)](https://github.com/SilentMalachite/mosa-prolog/blob/main/docs/agreement.md)
[![Phase](https://img.shields.io/badge/phase-9_done-blue)](https://github.com/SilentMalachite/mosa-prolog/blob/main/docs/STATUS.md)
[![Version](https://img.shields.io/badge/version-0.1.3-blue)](CHANGELOG.ja.md)
[![ISO/IEC 13211-1](https://img.shields.io/badge/ISO%2FIEC-13211--1-informational)](https://github.com/SilentMalachite/mosa-prolog/blob/main/docs/SPEC.ja.md)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus&logoColor=white)](https://github.com/SilentMalachite/mosa-prolog/blob/main/CMakeLists.txt)
[![CMake](https://img.shields.io/badge/CMake-3.25%2B-064F8C?logo=cmake&logoColor=white)](https://github.com/SilentMalachite/mosa-prolog/blob/main/CMakePresets.json)
[![Platforms](https://img.shields.io/badge/platform-macOS_%7C_Linux_%7C_Windows-lightgrey)](https://github.com/SilentMalachite/mosa-prolog/blob/main/.github/workflows/ci.yml)
[![Unicode](https://img.shields.io/badge/Unicode-18.0.0-orange)](https://github.com/SilentMalachite/mosa-prolog/blob/main/docs/unicode-version.md)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue)](LICENSE)

この文書は [英語版](README.md) の日本語訳です。食い違うときは英語版に従います。

ISO/IEC 13211-1 に沿った Prolog 処理系です。コマンドは 3 つあります。`mosa` は `?-` のプロンプトやファイルから Prolog を動かし、`mosac` は Prolog のプログラムを `mosa` なしで動く実行ファイルにし、`mosatest` はテストを動かします。文字は UTF-8 で読み書きするので、原子や変数に日本語の名前を付けられ、メッセージも日本語で出せます。C++20 で書いており、依存は C++20 標準ライブラリと CMake だけです。

## ダウンロード

Mosa Prolog 0.1.3 は [Releases のページ](https://github.com/SilentMalachite/mosa-prolog/releases) にあります。自分のコンピュータに合うパッケージをダウンロードしてください。

| コンピュータ | パッケージ |
| --- | --- |
| macOS（Apple Silicon） | `mosa-0.1.3-macos-arm64.tar.gz` |
| Linux（x86_64） | `mosa-0.1.3-linux-x86_64.tar.gz` |
| Windows（x64） | `mosa-0.1.3-windows-x86_64.zip` |

パッケージが動くのは、次のコンピュータです。

- macOS 26 以降（Apple Silicon）
- Linux（x86_64）。glibc が Ubuntu 24.04 と同じか、それより新しいもの
- Windows 10 22H2 以降と Windows 11（x64）。Smart App Control が無効のもの

パッケージには署名がありません。パッケージの展開、`bin` フォルダを PATH に足すこと、macOS と Windows で OS が起動を止めたときの進め方は、[マニュアルの第 2 章](docs/manual/ja/02-install.md) に書いています。`mosa` を動かすのに、ほかのものは要りません。C++ のコンパイラが要るのは `mosac`（[第 13 章](docs/manual/ja/13-mosac.md)）だけです。

## 最初の一歩

シェルで `mosa` と打ちます。Mosa はプロンプト `?- ` を出して待ちます。`?- ` の後ろに問い合わせを 1 つずつ打ち、Enter を押します。次の 3 つは [第 3 章](docs/manual/ja/03-first-query.md) から取りました。

```console
$ mosa
?- X = こんにちは.
X = こんにちは.
?- atom_length(こんにちは, N).
N = 5.
?- halt.
```

`X = こんにちは.` は変数 `X` に値 `こんにちは` を与え、`atom_length(こんにちは, N).` は `こんにちは` の字数を数え、`halt.` は Mosa を終えます。問い合わせはどれもピリオドで終わります。

## マニュアル

[マニュアル](docs/manual/ja/README.md)（[English](docs/manual/en/README.md)）は、Prolog を使ったことのない人に向けて書いています。Mosa の入手から始めて、プログラムを書くところ、`mosac` で実行ファイルを作るところまで進みます。第 14 章は、コマンド、フラグ、組み込みの述語を調べるためのリファレンスです。マニュアルの例はすべて Mosa 0.1.3 で確かめています。実行しないのは、インストールの手順のように、使う機械によって変わるものだけです。各リリースの中身と既知の制限は [CHANGELOG](CHANGELOG.ja.md) にあります。

## ソースからのビルド

これが要るのは、Mosa を自分でビルドするとき（Mosa そのものに手を入れるときなど）だけです。上のパッケージはビルド済みです。

### 必要なもの

- ソースを取るための Git
- CMake 3.25 以上と Ninja（macOS と Linux）
- 次のどれかのコンパイラ
  - macOS 26 以降: Xcode 26 以降か、その Command Line Tools（Apple Clang 17）
  - Linux: GCC 13 以上、または Clang 17 以上
  - Windows: Visual Studio 2022（MSVC v17.8 以上）

macOS では、`xcode-select --install` で Command Line Tools が入り、残りは [Homebrew](https://brew.sh) の `brew install cmake ninja` で入ります。Ubuntu 24.04 では、`sudo apt update && sudo apt install git g++ cmake ninja-build` ですべて入ります。

### ビルドとテスト

ソースを取ります。

```sh
git clone https://github.com/SilentMalachite/mosa-prolog.git
cd mosa-prolog
```

OS に合うプリセットを選びます。プリセットは `macos-clang`、`linux-gcc`、`linux-clang`、`windows-msvc` の 4 つです。macOS では次のとおりです。

```sh
cmake --preset macos-clang
cmake --build --preset macos-clang
ctest --preset macos-clang
```

実行ファイルは `build/` に出ます。Windows では `build/Release/` です。`ctest --preset` はテストを 1 つずつ動かすので、数分かかります。`-j` は付けないでください。2 つのテストが作業用のフォルダを共有していて、同時に動くと失敗することがあります。

## 名前の由来

Mosa は **M**odular **O**ptimized **S**ymbolic **A**rchitecture の略です。Mosa Prolog は Modular Optimized Symbolic Architecture Prolog を縮めた名前です。

## 現在の状態

フェーズ 0〜9 は終わっています。フェーズ 9（ISO の穴埋めと堅牢性）は、完了条件を確かめて 2026-11-11 に終えました。8.2.4.4 と 8.3.11.4 の Examples が通ること、準拠率が 98.4%（2150/2185）のままであること、ctest `stack_deep_read` が 1 MiB のスタックで通ること、`tests/pl/dcg.pl` と `lib_dcg.pl` と `modules.pl` が通って `mosatest --diff` の差分がゼロであることを確かめています（macOS、Linux、Windows の CI）。フェーズ 8（文法規則: `-->` と `phrase/2,3`）は、完了条件を確かめて 2026-11-11 に終えました。`tests/pl/dcg.pl` が `mosa` で通って `mosatest --diff` の差分がゼロであることと、準拠率が下がらないこと（macOS、Linux、Windows の CI）を確かめています。フェーズ 7（GC、ライタとローダの反復化、マニュアル、配布パッケージ、`mosac dist`）は、完了条件を確かめて 2026-11-10 に終えました。長時間ループでメモリが頭打ちになること（[`docs/bench.ja.md`](https://github.com/SilentMalachite/mosa-prolog/blob/main/docs/bench.ja.md#長いループのメモリ)）と、この README の手順だけで Mosa をビルドしてテストが通ること（Linux と Windows は CI、macOS は新しい clone）を確かめています。進み具合は [`docs/STATUS.md`](https://github.com/SilentMalachite/mosa-prolog/blob/main/docs/STATUS.md) にあります。

| フェーズ | 内容 | 状態 |
| --- | --- | --- |
| 0 | 骨組み（CMake、CI、テストハーネス） | 完了 |
| 1 | 項、リーダ、ライタ | 完了 |
| 2 | 実行の核（IR、節コンパイラ、選択点、カット、`catch/throw`、REPL、`consult/1`） | 完了 |
| 3 | 組込述語、ストリーム、フラグ、ディレクティブ、エラー体系 | 完了 |
| 4 | モジュールと `library(*)` | 完了 |
| 5 | 準拠率の追い込み、全角記号モード、メッセージの日本語化 | 完了 |
| 6 | `mosac`（Prolog から C++20 への変換とビルド）、差分テスト | 完了 |
| 7 | GC、マニュアル、`mosac dist`、配布パッケージ | 完了 |
| 8 | 文法規則（`-->`、`phrase/2,3`） | 完了 |
| 9 | ISO の穴埋めと堅牢性（`subsumes_term/2`、`acyclic_term/1`、枠で動く reader、`Name//Arity`、文法規則の本体の `*->`、`expand_term/2`） | 完了 |

「mosac diff」のバッジは、`mosatest --diff` が `tests/pl` と `tests/conformance` で測った、インタプリタと `mosac` の翻訳の一致率です（生成物 `docs/agreement.md`）。

## 使い方

### `mosa`（インタプリタ）

```sh
mosa [ファイル...] [-g ゴール]... [-t halt] [--heap-max MiB] [-- 引数...]
```

端末から `-t halt` なしで起動すると、`-g` のゴールを動かしたあとで対話モードになります。`-g` のゴールの末尾の `.` は、付けても省いてもかまいません。読めないゴールは構文エラーを表示し、`-t halt` のときは終了コード 2 で終わります。答えに別の解がありうるときは、`;` と Enter で次の答えを出します。ほかの行は、空の行も含めて止めます。この返事は 1 行で読み、空白とタブを除いた最初の字で決めます。`read/1` が項の終わりを確かめるために `foo.` の後で読んだ改行は、返事にしません。ゴールが行の途中までしか読まなかったとき（`get_char/1` など）の行の残りは返事になります。`mosac build --interactive` で作った実行ファイルも同じように尋ね、標準入力がパイプのときもそうします。

`-g` は何度でも書けて、ゴールは書いた順に動きます。`-t halt` のとき、または標準入力が端末でないときは、失敗したゴールか例外を送出したゴール（読めないゴールも）の最初の 1 つで、終了コード 1 か 2 で終わり、後のゴールは動きません。端末ではどのゴールもプロンプトに打ったのと同じに動き、そのあと対話モードになります。`-g` が無ければ、`-t halt` のときと端末でないときは、ファイルを読み込んだあとで終了コード 0 で終わります。`mosac` で作った実行ファイルと同じです。端末でないときは、ファイル、`-g`、`-t halt` のどれか 1 つが要ります。どれも無ければ使い方を表示して終了コード 1 で終わります。答えの行はゴールが終わるたびに書くので、プログラム自身の出力との順序は崩れません。

`--` より後ろの引数はすべてプログラムに渡され、読み取り専用のフラグ `argv`（原子のリスト。`--` が無ければ `[]`）で読めます。`:- initialization(main, main).` を書いたファイルは、読み込みのあとで `main` を 1 回呼んで終了します。終了コードは、成功が 0、失敗が 1、捕捉されない例外は文面を表示して 2、`main` が `halt/1` したときはその値です。対話モードにも `-g` にも進みません。

```console
$ ./build/mosa tests/pl/nrev.pl -g "nrev([1,2,3], R)." -t halt
R = [3,2,1].
$ ./build/mosa -g "atom_length('𠮷野家', N)." -t halt
N = 3.
```

文字はすべて Unicode のスカラー値 1 個として数えます。`𠮷` は 1 文字です。

#### ヒープ、GC、`statistics/2`

ヒープは必要に応じて上限まで伸び、ガベージコレクション（印付けと詰め直し、ADR 0010）されます。既定の上限は 4 GiB です。`mosa` では `--heap-max <MiB>`、`mosa` と、`mosac` で作った実行ファイルでは、環境変数 `MOSA_HEAP_MAX`（同じく MiB）で決めます。読めない `MOSA_HEAP_MAX` は既定のままで、読めない `--heap-max` は使い方の誤りです。上限には、動いている間に機械がヒープの外に持つ物も数えます。動いている `findall/3`、`bagof/3`、`setof/3` が集めた答え、生きている選択点、継続と節の環境、`call/1` が訳したゴールです（ADR 0010 の改訂 2 と 3）。段ごとに選択点を残す再帰も上限で止まります。ヒープが上限に達すると、標準エラーに `ヒープが上限（N MiB）に達したので、問い合わせを止めました` を出して問い合わせを止めます。`mosa` の REPL は次の問い合わせに進み、生成した実行ファイルは終了コード 2 で終わります。上限の近くで、空けた量が上限の 1/32 に満たない回収が 3 回続いたときも、上限に達する前に同じように止めます。そのまま続けると這うように遅くなるからで、このときは上限を上げてください。生きている項が小さいプログラムは小さいヒープで走ります。1000000 まで数える `loop/1` も `tak(18, 12, 6)` も `--heap-max 1` で完走します。

`statistics(鍵, 値)` の鍵は次のとおりです。`heap` は `[使用, 容量, 上限]`（バイト。容量は、ふつう回収を始める柔らかい容量。上限の近くでは回収の閾値がこれより上になりうる）、`garbage_collection` は `[回数, 回収したバイト, ミリ秒]`、`runtime` と `walltime` は `[合計ミリ秒, 前回からの差]`、`cputime` は CPU 秒（浮動小数）です。`garbage_collect/0` は回収を 1 回強制します。フラグ `trace_gc`（`set_prolog_flag(trace_gc, true)`）は回収のたびに 1 行を標準エラーに出します。環境変数 `MOSA_GC_STRESS=N` は N 回の安全点ごとに回収します。試験用です。

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

そのほかのオプション: `--debug` は `-O0 -g` でビルドし、プログラム自身の節を追跡します（同梱ライブラリの単位は追跡しません）。`--verbose` はコンパイラの命令を表示し、`--clean` はビルドの前に `.mosac-cache/` を消します。`mosac run --keep` は実行ファイルの写しを置いた一時ディレクトリを残し、`--runtime-source` は同梱の静的ライブラリではなくソースから実行時ライブラリをビルドします。`mosac emit --library` と `mosac emit --units` は同梱ライブラリのビルドと `mosatest --diff` のためにあり、普通は使いません。

コンパイラは `--cxx`、`MOSA_CXX`、`CXX`、OS の既定の順に探します（macOS は `clang++`、Linux は `g++` の次に `clang++`、Windows は `vswhere` で見つけた MSVC）。CMake が無いとき、`MOSA_NO_CMAKE=1` のとき、Windows の MSVC で実行時ライブラリか `.mosac-cache/` のパスに ASCII 以外の文字があるとき（CMake の NMake でのビルドはその文字を落とすことがあります）は、`mosac` が生成したファイルを 1 つずつ直接コンパイルします。`MOSA_WERROR=1` は生成した C++ を警告をエラーにしてコンパイルします（テストと CI が立てます）。`:- initialization(main, main).` があれば、実行ファイルは `mosa` と同じく `main` を実行して終わります。`--interactive` ではそれを実行せず REPL を立ち上げます。どこにも定義の無い述語を呼んでいると、行番号付きの警告で `:- dynamic(名前/アリティ).` を案内し、翻訳は続けます。実行ファイルは `mosa` と同じく、そのゴールが動くときに `existence_error` を投げるか、フラグ `unknown` に従います。途中のファイルは `.mosac-cache/` に置きます。詳しくは [`docs/SPEC.ja.md`](https://github.com/SilentMalachite/mosa-prolog/blob/main/docs/SPEC.ja.md) の第 6 節です。

### `mosatest`（テストランナー）

```sh
mosatest tests/pl/                    # 自前テスト（tests/pl/*.pl）
mosatest --suite tests/conformance    # 準拠スイート。docs/conformance.md と .json を書き出す
mosatest --suite <ディレクトリ> --report tests/tmp/x.md   # 別のスイート。x.md と x.json を書き出す
mosatest --roundtrip tests/pl/syntax.pl   # 読み書きの往復
mosatest --diff tests/pl              # インタプリタと mosac の出力を比べる。docs/agreement.md と .json を書き出す
mosatest --bench bench/nrev.pl --count 20000 --report tests/tmp/bench.json   # インタプリタと mosac の時間（5 回の中央値）
mosatest --manual docs/manual         # マニュアルの例を、英語版と日本語版で動かす
```

`--report` を省けるのは、リポジトリ直下で同梱の `tests/conformance`（`--diff` では `tests/pl` も）を渡したときだけです。`--diff` は `--work`、`--cmake`、`--config` も取ります。`--bench` は [`docs/bench.ja.md`](https://github.com/SilentMalachite/mosa-prolog/blob/main/docs/bench.ja.md) を見てください。別のスイートで `docs/` の報告書とバッジを上書きしないためです。

## 日本語の変数

日本語には大文字と小文字の区別がありません。そのため `親(太郎, 一郎).` の `太郎` は原子です。日本語名の変数は `_太郎` のように下線で始めます。詳しい字句の規則は [`docs/SPEC.ja.md`](https://github.com/SilentMalachite/mosa-prolog/blob/main/docs/SPEC.ja.md) 第 4 節にあり、[マニュアルの第 11 章](docs/manual/ja/11-japanese.md) で順を追って説明しています。

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

同梱した Logtalk の準拠スイート（`tests/conformance/logtalk/`）の合格率は上のバッジのとおりです。フェーズ 3 の完了時点で 2171 件中 2028 件（93.4%）、フェーズ 4 の完了時点で 2041 件（94.0%）、フェーズ 5 の完了時点で 2135 件（98.3%）、0.1.0 で 2136 件（98.4%）が通りました。不合格の一覧は [`docs/conformance.md`](https://github.com/SilentMalachite/mosa-prolog/blob/main/docs/conformance.md) にあります。`mosatest --suite` はこのファイルと、バッジ用の [`docs/conformance.json`](https://github.com/SilentMalachite/mosa-prolog/blob/main/docs/conformance.json)（shields.io の endpoint 形式）を書き出します。どちらも手で編集しません。

準拠率のバッジは `main` の `docs/conformance.json` を読むので、コミットするたびに自動で変わります。CI は 4 つのジョブすべてでスイートを実行して合格率を Job Summary に出し、macOS のジョブでは生成した JSON がコミット済みのものと違えば失敗します。スイートの結果が変わる変更では、`mosatest --suite tests/conformance` を実行して JSON も一緒にコミットしてください。

「mosac diff」のバッジも同じ仕組みで `main` の [`docs/agreement.json`](https://github.com/SilentMalachite/mosa-prolog/blob/main/docs/agreement.json) を読みます。CI は 3 つの OS で差分テスト（`ctest -L diff`、つまり `mosatest --diff tests/pl` と `mosatest --diff tests/conformance`。生成した C++ は警告を誤りにしてコンパイルします）を流して一致率を Job Summary に出し、macOS のジョブでは生成した JSON がコミット済みのものと違えば失敗します。各 OS のジョブは `mosac toolchain` と `mosac build tests/mosac/hello.pl` も実行し、別のジョブがその実行ファイルを落として、素の `ubuntu:24.04` コンテナで動かし、macOS では `otool -L` の依存が `/usr/lib/` と `/System/` だけ、Windows では `dumpbin /dependents` の依存が OS の DLL だけであることを確かめます。一致率が変わる変更では、`mosatest --diff tests/pl` と `mosatest --diff tests/conformance` を実行して JSON も一緒にコミットしてください。

## テストの構成

| 層 | 置き場所 | 実行方法 |
| --- | --- | --- |
| C++ 部品テスト | `tests/cpp/` | `ctest` |
| Prolog 自前テスト | `tests/pl/` | `mosatest tests/pl/`（`ctest` からも動く） |
| ISO 準拠スイート | `tests/conformance/` | `mosatest --suite tests/conformance` |
| 差分テスト | `tests/pl/`、`tests/conformance/` | `mosatest --diff tests/pl`（`ctest` からも動く。ラベル `diff`） |
| マニュアルの例 | `docs/manual/` | `mosatest --manual docs/manual`（`ctest` からも動く） |

## 文書

英語版が正で、日本語訳は同じ場所に `*.ja.md` として置きます（マニュアルは `docs/manual/ja/`）。

| 文書 | 中身 |
| --- | --- |
| [`docs/manual/`](docs/manual/ja/README.md) | マニュアル（入手と起動、Prolog の基本、日本語、モジュール、`mosac`、リファレンスの章） |
| [`CHANGELOG.ja.md`](CHANGELOG.ja.md) | 各リリースの中身、動作環境、既知の制限、ISO との違い |
| [`docs/SPEC.ja.md`](https://github.com/SilentMalachite/mosa-prolog/blob/main/docs/SPEC.ja.md) | 仕様の本文（目的、ISO の範囲、日本語、モジュール、アーキテクチャ、テスト、フェーズ） |
| [`docs/STATUS.md`](https://github.com/SilentMalachite/mosa-prolog/blob/main/docs/STATUS.md) | 今のフェーズと次の作業（英語のみ） |
| [`docs/adr/`](https://github.com/SilentMalachite/mosa-prolog/blob/main/docs/adr/README.ja.md) | 後から変えにくい設計の決定記録 |
| [`docs/iso-notes.ja.md`](https://github.com/SilentMalachite/mosa-prolog/blob/main/docs/iso-notes.ja.md) | 規格の解釈メモ（条番号付き） |
| [`docs/conformance.md`](https://github.com/SilentMalachite/mosa-prolog/blob/main/docs/conformance.md) | 準拠スイートの結果（自動生成、日本語のみ） |
| [`docs/conformance.json`](https://github.com/SilentMalachite/mosa-prolog/blob/main/docs/conformance.json) | 準拠率バッジの元データ（自動生成） |
| [`docs/agreement.md`](https://github.com/SilentMalachite/mosa-prolog/blob/main/docs/agreement.md) | `mosatest --diff` が測ったインタプリタと翻訳の一致（自動生成） |
| [`docs/agreement.json`](https://github.com/SilentMalachite/mosa-prolog/blob/main/docs/agreement.json) | 一致率バッジの元データ（自動生成） |
| [`docs/unicode-version.md`](https://github.com/SilentMalachite/mosa-prolog/blob/main/docs/unicode-version.md) | 文字分類表が使う Unicode の版 |
| [`docs/bench.ja.md`](https://github.com/SilentMalachite/mosa-prolog/blob/main/docs/bench.ja.md) | ベンチマークの記録 |
| [`docs/bench.json`](https://github.com/SilentMalachite/mosa-prolog/blob/main/docs/bench.json) | macOS の CI が比べるベンチマークの結果（自動生成） |
| [`AGENTS.ja.md`](https://github.com/SilentMalachite/mosa-prolog/blob/main/AGENTS.ja.md) | コーディングエージェント向けの作業規則 |

## ディレクトリ

```text
include/mosa/   公開ヘッダ
src/            core, read, write, machine, builtins, module, codegen, platform（compile は予約で空）
lib/            Prolog 製の標準ライブラリとメッセージカタログ
bench/          mosatest --bench 用のベンチマークのプログラム
tests/          cpp, pl, mosac, conformance, tmp
tools/          開発者用の生成スクリプト（ビルドには不要）
docs/           マニュアル、仕様、状態、ADR、解釈メモ
```

## ライセンス

Apache-2.0 です。本文は [`LICENSE`](LICENSE)、著作権表示は [`NOTICE`](NOTICE) にあります。`tests/conformance/logtalk/` も同じライセンスで、Logtalk の著作権表示はそこの `NOTICE.txt` にあります。
