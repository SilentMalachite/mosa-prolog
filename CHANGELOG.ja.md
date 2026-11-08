[English](CHANGELOG.md) | 日本語

# 変更履歴

この文書は [英語版](CHANGELOG.md) の日本語訳です。食い違うときは英語版に従います。

## 0.1.0（2026-11-08）

Mosa Prolog の最初のリリースです。Mosa Prolog は ISO/IEC 13211-1 に沿った Prolog 処理系です。下に挙げることは、どれも [マニュアル](docs/manual/ja/README.md) で順を追って説明しています。マニュアルの例はすべて、このリリースで確かめています。実行しないのは、インストールの手順のように、使う機械によって変わるものだけです。

### できること

- `mosa` は Prolog を動かします。`?-` のプロンプト、コマンド行で渡したファイル、`-g` で渡したゴール、それらの後で終わる `-t halt` を使えます。`initialization(main, main)` を書いたファイルは `main` を動かし、`main` が成功すれば 0、失敗すれば 1、エラーを送出すれば 2 で終わります。
- `mosac` は Prolog のプログラムを、`mosa` も `.pl` のファイルもなしで動く実行ファイルにします。`mosac build` は実行ファイルを書き、`mosac run` は作ってすぐに動かし、`mosac emit` は C++ のファイルと `CMakeLists.txt` だけを書き、`mosac toolchain` は `mosac` が使う C++ のコンパイラか、入れるべきものを示します。`mosac` には C++ のコンパイラが要ります。macOS では Xcode Command Line Tools、Linux では `g++`、Windows では Visual Studio 2022 です（[第 13 章](docs/manual/ja/13-mosac.md)）。
- `mosatest` は `test(名前, ゴール, 期待)` の事実で書いたテストを動かし、マニュアルの例を確かめます。
- 日本語: Mosa は UTF-8 で読み書きし、字数を Unicode の符号位置で数えます。原子と変数に日本語の名前を付けられます。フラグ `japanese_punctuation` が `true` のときは全角の記号を半角として読みます。`LANG` が `ja` で始まるとメッセージは日本語です。`LANG` の無い Windows では、Windows の表示言語に従います（[第 11 章](docs/manual/ja/11-japanese.md)）。
- メモリ: ヒープは必要に応じて上限まで伸びます。上限の既定は 4 GiB です（`--heap-max`、`MOSA_HEAP_MAX`）。プログラムがもう使わない項は、ガベージコレクタが空けます。`garbage_collect/0` と `statistics/2` は組み込みです。
- ライブラリ: `lists`、`apply`、`between`、`format`、`strings`、`japanese` を、ファイルの中の `:- use_module(library(名前)).` で読み込めます（[第 12 章](docs/manual/ja/12-modules-libraries.md)）。
- 準拠率: ソースに同梱した Logtalk の準拠スイートで、2171 件中 2136 件（98.4%）が通ります。

### 動作環境

| コンピュータ | パッケージ |
| --- | --- |
| macOS（Apple Silicon） | `mosa-0.1.0-macos-arm64.tar.gz` |
| Linux（x86_64） | `mosa-0.1.0-linux-x86_64.tar.gz` |
| Windows（x64） | `mosa-0.1.0-windows-x86_64.zip` |

- macOS 26 以降（Apple Silicon）
- Linux（x86_64）。glibc が Ubuntu 24.04 と同じか、それより新しいもの
- Windows 10 22H2 以降と Windows 11（x64）。Smart App Control が無効のもの

パッケージには署名がありません。macOS と Windows で OS が起動を止めたときの進め方は、[第 2 章](docs/manual/ja/02-install.md) に書いています。

### 既知の制限

- `-->` で書く文法規則は使えません。それを含むファイルは読み込まれません。
- `subsumes_term/2`（8.2.4）と `acyclic_term/1`（8.3.11）は組み込みにありません。
- `use_module/1` はファイルの中でしか使えません。`?-` のプロンプトで打つと、`existence_error(procedure,use_module/1)` の警告が出ます（[第 12 章](docs/manual/ja/12-modules-libraries.md)）。
- `?-` のプロンプトで `format/1`、`format/2`、`format/3` を使った問い合わせをすると、Mosa は `true` をピリオドなしで出して待ちます。ほかの答えがあるかもしれないときと同じです（[第 3 章](docs/manual/ja/03-first-query.md)）。Enter を押すと、Mosa はピリオドを出します。`;` を打つと `false.` が出ます。
- Windows のコンソールのウィンドウで日本語を打つこと（`chcp 932` と `chcp 437`）は未検証です。
- `mosac dist` は Mosa 0.1.0 にはありません。
- `mosac build --debug` は、最適化して作った実行時ライブラリにつなぎます。ライブラリの節は出しません。
- `f(f(f(…)))` のようにとても深く入れ子になった項がファイルにあると、Mosa は読む途中で落ちることがあります。Windows では 600 段ほどから、macOS と Linux では数千段からです。
- Windows では、Smart App Control が有効だと Mosa は動きません（[第 2 章](docs/manual/ja/02-install.md)）。

### ISO との違い

Mosa は ISO/IEC 13211-1 とその正誤表に従います。従っていないところと、ISO が処理系に任せているところは、次のとおりです。

- 整数は 64 ビットです。それより大きい整数はなく、`max_integer` を超えると `evaluation_error(int_overflow)` になります。浮動小数点数は 64 ビットで、Mosa は、読み戻すと同じ数になるいちばん短い字を書きます。
- `unknown` に使える値は `error` と `fail` だけで、3 つの `float_*` のフラグに使える値は `error` だけです。
- モジュールは、規格の第 2 部（ISO/IEC 13211-2）の書き方を使います。モジュールが持つのは述語と演算子です。原子はどのモジュールでも同じです。
- `format/2`、`between/3`、`member/2`、`append/3` など、ライブラリの述語は組み込みではなく、`use_module` が要ります。
- 全角の記号を半角として読むのは、フラグ `japanese_punctuation` が `true` のときだけです。
- `subsumes_term/2` と `acyclic_term/1` はありません（上の既知の制限を見てください）。

[第 14 章](docs/manual/ja/14-reference.md) にも、組み込みの述語と一緒にこれらを載せ、ISO にない述語に印を付けています。こう決めた理由は、節ごとに、ソースの木の [ISO の解釈の覚え書き](https://github.com/SilentMalachite/mosa-prolog/blob/main/docs/iso-notes.ja.md)に書いています。
