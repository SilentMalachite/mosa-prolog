[English](CHANGELOG.md) | 日本語

# 変更履歴

この文書は [英語版](CHANGELOG.md) の日本語訳です。食い違うときは英語版に従います。

## 未リリース

### 足したこと

- `subsumes_term/2`（8.2.4）と `acyclic_term/1`（8.3.11）を組み込みました。`subsumes_term/2` は何も束縛しません。`subsumes_term/2` や `acyclic_term/1` を自分で定義していたプログラムは、その定義を消してください。読み込むと `permission_error(modify,static_procedure,subsumes_term/2)`（または `acyclic_term/1`）になります。
- 公開リストと取り込みリスト、および `dynamic`、`discontiguous`、`multifile` のディレクティブで、非終端記号の `Name//Arity` を `Name/(Arity+2)` として読みます。`abolish/1` は ISO のままです。
- 文法規則の本体の `*->` はソフトカットです。
- `library(dcg)` が `expand_term/2` を公開します。文法規則 `H --> B` を 1 つの節に訳し、変数でないほかの項はそのまま返します。`:- use_module(library(dcg)).` で読み込みます。
- とても深く入れ子になった項を、落ちずに読みます。1 MiB のスタックで、引数、リスト、前置演算子、`xfy` の項、括弧、波括弧、`|` の尾で、100 万段の項を読めます。

### 既知の制限

0.1.2 の既知の制限は、このリリースでなくなった 4 つ（深く入れ子になった項で落ちること、`subsumes_term/2` と `acyclic_term/1` が無いこと、公開リストの `Name//Arity`、文法規則の本体の `*->`）を除いて、そのまま残ります。`assertz/1` は文法規則を訳しません。先に `library(dcg)` の `expand_term/2` で訳した節を渡します。`term_expansion/2` と `goal_expansion/2` はありません。ソースの節が 10 万段入れ子になっていても `mosac build` は通りますが、macOS では clang が約 80 分と 1.2 GB のメモリを使いました。

## 0.1.2（2026-11-11）

文法規則を足したリリースです。動くコンピュータは 0.1.1 と同じです（[第 2 章](docs/manual/ja/02-install.md)）。[マニュアル](docs/manual/ja/README.md) の例は、どれもこのリリースで確かめています。

### 足したこと

- 文法規則: ファイルの `Head --> Body` の節を、ファイルを読み込むとき通常の節に訳します。`mosa` でも `mosac` で作ったプログラムでも同じです。`phrase/2` と `phrase/3` を組み込みました（[第 14 章](docs/manual/ja/14-reference.md)）。文法規則の本体の `{G}` は `call/1` と同じく `G` を呼ぶので、`{}` の中のカットはその中にしか効きません。

### 既知の制限

0.1.1 の既知の制限は、このリリースでなくなった 1 つ（文法規則）を除いて、そのまま残ります。文法規則には独自の制限があり、下の初めの 3 つがそれです。

- `assertz/1` に渡した文法規則は訳しません。`term_expansion/2`、`goal_expansion/2`、`expand_term/2` は組み込みにありません。
- エクスポートの一覧で、非終端記号を `Name//Arity` と書けません。`:- module(m, [greet//0])` は `type_error(predicate_indicator,greet//0)` になります。代わりに、規則が訳された先の述語を、引数を 2 つ足して `greet/2` と書きます。
- 文法規則の本体の `*->` はソフトカットになりません。`([x] *-> [y] ; [z])` は非終端記号 `(*->)/4` を呼びます。
- `subsumes_term/2`（8.2.4）と `acyclic_term/1`（8.3.11）は組み込みにありません。
- `use_module/1` はファイルの中でしか使えません。`?-` のプロンプトで打つと、`existence_error(procedure,use_module/1)` の警告が出ます（[第 12 章](docs/manual/ja/12-modules-libraries.md)）。
- Windows のコンソールのウィンドウで日本語を打つこと（`chcp 932` と `chcp 437`）は未検証です。
- `mosac build --debug` は、最適化して作った実行時ライブラリにつなぎます。ライブラリの節は出しません。
- `f(f(f(…)))` のようにとても深く入れ子になった項がファイルにあると、Mosa は読む途中で落ちることがあります。Windows では 600 段ほどから、macOS と Linux では数千段からです。
- Windows では、Smart App Control が有効だと Mosa は動きません（[第 2 章](docs/manual/ja/02-install.md)）。

## 0.1.1（2026-11-10）

`mosac dist` を足し、下の不具合を直したリリースです。動くコンピュータは 0.1.0 と同じです（[第 2 章](docs/manual/ja/02-install.md)）。[マニュアル](docs/manual/ja/README.md) の例は、どれもこのリリースで確かめています。

### 足したこと

- `mosac dist app.pl -o dist` は、プログラムを作ってほかの人に渡せる形にまとめます。書庫 `app-<システム>-<プロセッサ>.tar.gz`（Windows では `.zip`）を 1 つ書き、実行ファイル、英語と日本語の `README.txt`、実行ファイルが含む Mosa の実行時ライブラリの `LICENSE` と `NOTICE` を入れます（[第 13 章](docs/manual/ja/13-mosac.md)）。書庫は圧縮しません。

### 直したこと

- `format/1`、`format/2`、`format/3` は選択点を残さなくなりました。`?-` のプロンプトで `format("~w~n", [a])` のような問い合わせをすると、Mosa は `true.` と答えて次の問い合わせに進みます。Mosa 0.1.0 では、ほかの答えがあるかもしれないときと同じく、`true` をピリオドなしで出して待っていました。また、呼ぶたびに選択点が 1 つ残ったので、`format` を何度も呼ぶ再帰のループはヒープの上限で止まりました。
- 単独の `,` をアトムとして読まなくなりました。ISO のとおり、`f((,))` と `,(a, b)` は構文エラーです（6.4）。代わりに `','` と書きます。単独の `|` がアトムになるのは、`|` が演算子のときだけです（6.3.4.3）。
- 両方の引数を与えた `char_code/2` は、2 つを比べるようになりました。`char_code(a, 98)` は失敗します。Mosa 0.1.0 では成功していました。
- `bagof/3` と `setof/3` は、自由変数が `'$rename'(0)` のような項に束縛された解を、その変数が未束縛の解と同じ組に入れなくなりました。
- ファイルを読む流れを閉じても、別の流れが同じファイルに書いてまだ保存していない中身を失わなくなりました。`open(F, write, W), write(W, hello), open(F, read, R), close(R)` の後、Mosa が終わるとファイルは空になっていました。
- `call/2` などの `call/N` は、実行時に `user:` を 100 万回重ねて作ったゴールのように、モジュールの修飾がとても多いゴールで落ちなくなりました。
- `?-` のプロンプトで打った `:- halt(N).` は、終了ステータス `N` で Mosa を終わらせるようになりました。Mosa 0.1.0 はこれを無視して、空の警告を出していました。
- 構文エラーで読み込まなかったファイルの `initialization(G, main)` は、動かなくなりました。
- 構文エラーのあるファイルは、そのファイルの指示が入れ子の `consult/1` の誤りを捕まえた後でも、読み込む前にあった節を残すようになりました。
- `mosac build` と `mosac run` は、節が 65,536 個以上ある述語でも動くようになりました。Mosa 0.1.0 では生成した C++ がコンパイルできませんでした。`mosac emit` は、書くファイルを最後まで書けないと誤りを知らせるようになりました。
- `mosatest`：期待する答えの `solutions(L)` は、ゴールの解が `L` とちょうど同じで、同じ順のときだけ通ります。Mosa 0.1.0 は初めのほうの解だけを見ていて、`solutions([])` では成功するゴールも通していました。読めないテストのフォルダは、落ちずに誤りのメッセージになります。

### 既知の制限

0.1.0 の既知の制限は、このリリースでなくなった 2 つ（`format` と `mosac dist`）を除いて、そのまま残ります。

- `-->` で書く文法規則は使えません。それを含むファイルは読み込まれません。
- `subsumes_term/2`（8.2.4）と `acyclic_term/1`（8.3.11）は組み込みにありません。
- `use_module/1` はファイルの中でしか使えません。`?-` のプロンプトで打つと、`existence_error(procedure,use_module/1)` の警告が出ます（[第 12 章](docs/manual/ja/12-modules-libraries.md)）。
- Windows のコンソールのウィンドウで日本語を打つこと（`chcp 932` と `chcp 437`）は未検証です。
- `mosac build --debug` は、最適化して作った実行時ライブラリにつなぎます。ライブラリの節は出しません。
- `f(f(f(…)))` のようにとても深く入れ子になった項がファイルにあると、Mosa は読む途中で落ちることがあります。Windows では 600 段ほどから、macOS と Linux では数千段からです。
- Windows では、Smart App Control が有効だと Mosa は動きません（[第 2 章](docs/manual/ja/02-install.md)）。

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
