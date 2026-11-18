[English](../en/14-reference.md) | 日本語

# 14. リファレンス

この章は、このソースの木にあるものを調べるための一覧です。コマンドとそのオプション、環境変数、フラグ、ディレクティブ、すべての組み込みの述語、各ライブラリの述語を並べています。最新のリリースは 0.4.1 で、0.4.0 からの追加と変更は CHANGELOG にあります。順に読む必要はありません。

## コマンド

3 つのコマンドは、どれも同じバージョンの行を出します。

<!-- check: versions -->
```console
$ mosa --version
mosa 0.4.1
$ mosac --version
mosa 0.4.1
$ mosatest --version
mosa 0.4.1
```

### `mosa`

`mosa [ファイル...] [-g ゴール]... [-t halt] [--heap-max MiB] [-- 引数...]` で Mosa を起動します。引数がなければ、`?-` のプロンプトを出します（第 3 章）。

| 引数 | すること |
| --- | --- |
| `ファイル` | ほかの何よりも先に、`consult/1` と同じようにファイルを読み込みます（第 4 章）。いくつか書けば、書いた順に読み込みます。 |
| `-g ゴール` | ファイルを読み込んだ後、ゴールを 1 回実行し、プロンプトと同じように最初の答えを出します。`-g` をいくつか書けば、書いた順に実行します。`-t halt` を付けたときと、`mosa` の入力が端末でないときは、失敗したかエラーを起こしたゴールがあれば、そこで残りをやめます。`-t halt` を付けずに端末から動かせば、どのゴールも実行します。 |
| `-t halt` | `-g` のゴールの後、プロンプトを出さずに終わります。 |
| `--heap-max MiB` | 項が使ってよいメモリの上限で、単位は MiB です（既定は 4096）。それより多く要る問い合わせは、メッセージを出して止まります。`MOSA_HEAP_MAX` よりこちらが優先します。 |
| `--version` | `mosa 0.4.1` と出して終わります。 |
| `-- 引数...` | `--` より後ろはオプションとして読みません。原子のリストにして、フラグ `argv` にします。 |

ゴールは一重引用符で囲みます。そうすると、シェルはゴールを 1 つの引数として `mosa` に渡します。

<!-- check: goal-option -->
```console
$ mosa -g 'X is 6 * 7' -t halt
X = 42.
$ mosa -g 'current_prolog_flag(argv, A)' -t halt -- 一 二
A = [一,二].
```

`mosa` が終わるときの終了状態は、どう終わったかをシェルに知らせます（macOS と Linux では `echo $?` で見られます）。`halt.` の後と、`-t halt` で `-g` のゴールがすべて成功した後は 0 です。`-g` のゴールが失敗したとき、ファイルを読み込めなかったとき、コマンドラインが正しくないときは 1 です。ゴールが起こしたエラーをどこでも捕まえなかったときと、メモリの上限に着いたときは 2 です。`halt(N)` は N で終わります。`initialization(main, main)` のあるファイル（第 13 章）も、`main` が成功すれば 0、失敗すれば 1、エラーを起こせば 2 で終わります。

### `mosac`

`mosac` は、Prolog のプログラムを実行ファイルにします（第 13 章）。最初の引数で、何をするかを指定します。

| コマンド | すること |
| --- | --- |
| `mosac build ファイル -o 名前` | ファイルと、そのファイルが `use_module/1` で読み込むファイルをすべて C++ に訳してコンパイルし、実行ファイル `名前`（Windows では `名前.exe`）を作ります。`-o` がなければ、ファイル名から `.pl` を除いたものが名前になります。 |
| `mosac run ファイル -- 引数...` | `build` と同じように作り、すぐに動かします。`--` より後ろは、プログラムへの引数です。フォルダには実行ファイルを残さず、プログラムの終了状態で終わります。 |
| `mosac dist ファイル -o フォルダ` | `build` と同じように作り、実行ファイル、`README.txt`、`LICENSE`、`NOTICE` をフォルダ（`-o` がなければ `dist`）の書庫 1 つにまとめます。書庫の名前は `名前-システム-プロセッサ.tar.gz`、Windows では `.zip` です（第 13 章）。書庫のパスを出します。 |
| `mosac emit ファイル -o フォルダ` | C++ のファイルと `CMakeLists.txt` だけをフォルダに書き出します。それを読んだり、手で直したりするためのものです。 |
| `mosac emit --library ファイル -o フォルダ` | フォルダ `lib` のライブラリを 1 つ訳します。Mosa のビルドが使います。 |
| `mosac emit --units フォルダ -o フォルダ` | テストのフォルダを訳します。`mosatest --diff` が使います。 |
| `mosac toolchain` | `mosac` が使うコンパイラ、CMake、実行時ライブラリを出します。見つからなければ、入れるものを出します。Windows では、実行時ライブラリか `.mosac-cache` のパスに ASCII の外の文字があると、`mosac` は CMake を使わず、コンパイラを直接呼びます（第 13 章）。 |
| `mosac --version` | `mosa 0.4.1` と出します。 |

`mosac build`、`mosac run`、`mosac dist` には、次のオプションがあります。

| オプション | すること |
| --- | --- |
| `-o 名前` | `build` では実行ファイルの名前、`dist` では書庫を書くフォルダです。 |
| `--interactive` | `build` と `dist` だけ。実行ファイルは、`initialization(main, main)` を実行する代わりに、プログラムを読み込んだ状態で `?-` のプロンプトを出します。 |
| `--debug` | 最適化せずにコンパイルします。プログラムは、自分で書いたファイルの節を、実行するたびに 1 つずつ出します。ライブラリの節は出しません。 |
| `--verbose` | `mosac` のしていることを出します。生成した C++ がコンパイルできないときは、コンパイラのメッセージも出します。 |
| `--clean` | 先に `.mosac-cache` を消し、すべてを作り直します。 |
| `--keep` | `run` だけ。プログラムを動かした一時フォルダを残します。 |
| `--cxx コンパイラ` | コンパイラを探さずに、このコンパイラを使います。`mosac toolchain` にも使えます。 |
| `--runtime-source` | パッケージに実行時ライブラリのないコンパイラのために、パッケージに入っている源から実行時ライブラリを作ります（第 13 章）。 |

`mosac` は、`--cxx`、`MOSA_CXX`、`CXX`、決まった名前（macOS では `clang++`、Linux では `g++` と `clang++`、Windows では Visual Studio）の順にコンパイラを探します。うまくいけば 0 で終わり、うまくいかなければほかの値で終わります。プログラムに誤りがあると、`mosac build` は `mosa` と同じように行と桁を出して止まります。

### `mosatest`

`mosatest` はテストを実行します。下の表の 1 行目は自分で書いたテストのためのもので、最後の行はこのマニュアルの例を確かめます。ほかの行は、Mosa を開発する人のための道具です。

| コマンド | すること |
| --- | --- |
| `mosatest ファイル` | ファイルの事実 `test(Name, Goal, Expected)` を読み込み、ゴールを 1 つずつ実行します。`Expected` は `true`、`fail`、`error(Formal)`、`solutions(List)` のどれかで、`List` には解をすべて順に並べます。期待どおりにならなかったゴールを出し、1 つでもあれば 1 で終わります。フォルダを渡せば、その中の `.pl` のファイルをどれも実行します。テストの無いファイルと、`.pl` ファイルの無いフォルダも 1 で終わります。 |
| `mosatest --suite フォルダ --report ファイル` | ソースの木にある適合性のテストを実行し、2 つの欄を報告します。ISO の欄と、Mosa が ISO の外で提供するライブラリ述語のライブラリ欄です。報告はそのファイルに書き、各欄のバッジの元データをその隣に書きます。同梱のスイートなら、ISO の欄は `docs/conformance.json`、ライブラリ欄は `docs/conformance-library.json` です。 |
| `mosatest --diff フォルダ --report ファイル` | テストを `mosa` と `mosac` の両方で実行し、結果を比べます。 |
| `mosatest --bench ファイル --count 回数` | プログラムにかかる時間を、`mosa` と `mosac` で測ります。 |
| `mosatest --roundtrip ファイル` | ファイルの項を 1 つずつ書き出し、読み戻します。 |
| `mosatest --manual フォルダ` | このマニュアルの例を実行します。フォルダには、パッケージの `docs/manual` のように `en` と `ja` があります。`--work フォルダ` は例を動かす場所、`--mosa パス` は確かめる `mosa`、`--all-tables` はすべての述語の表を求めることを指定します。 |

## 環境変数

環境変数は、シェルが起動するプログラムに渡す設定です（第 11 章で `LANG` を設定しました）。

| 変数 | 読むもの | すること |
| --- | --- | --- |
| `LANG` | すべて | メッセージの言語です。値が `ja` で始まれば日本語、そうでなければ英語です。設定されていないか空なら、Windows の Mosa は Windows の表示言語に従い、macOS と Linux の Mosa は英語を使います（第 11 章）。 |
| `MOSA_HEAP_MAX` | `mosa`、`mosac` が作った実行ファイル | `--heap-max` と同じメモリの上限で、単位は MiB です。整数でない値は、警告を出して使いません。 |
| `MOSA_LIB` | `mosa`、`mosac` | `library(Name)` が、プログラムの隣のフォルダ `lib` より先に探すフォルダです（第 12 章）。 |
| `MOSA_CXX` | `mosac` | `--cxx` がないときに使う C++ のコンパイラです。 |
| `CXX` | `mosac` | `--cxx` も `MOSA_CXX` もないときに使う C++ のコンパイラです。 |
| `MOSA_NO_CMAKE` | `mosac` | `1` なら、CMake が入っていても、`mosac` は自分でコンパイラを呼びます。 |
| `MOSA_WERROR` | `mosac` | `1` なら、生成した C++ へのコンパイラの警告を誤りとして扱います。Mosa 自身のテストが設定します。 |
| `MOSA_GC_STRESS` | `mosa`、`mosac` が作った実行ファイル | Mosa のテストのためだけのものです。数 N を設定すると、N 歩ごとにメモリを回収します。プログラムはとても遅くなります。 |
| `MOSA_TRANSCRIPT` | `mosa` | Mosa のテストのためだけのものです。`1` なら、`mosa` は端末から打たれたのではない入力も端末からのものとして扱い、読んだ行を 1 行ずつ出します。`mosatest --manual` が設定します。 |

## フラグ

フラグは Mosa の設定です。`current_prolog_flag/2` で読み、`set_prolog_flag/2` で変えます。プロンプトで設定したフラグは、変えるか終わるまで続きます。

<!-- check: flags -->
```text
?- current_prolog_flag(double_quotes, X).
X = codes.
?- set_prolog_flag(double_quotes, atom).
true.
?- X = "あいう".
X = あいう.
```

「ほかの値」が「なし」のフラグは、変えられません。

| フラグ | 起動したときの値 | ほかの値 | 意味 |
| --- | --- | --- | --- |
| `bounded` | `false` | なし | 整数には最小と最大の値がありません。結果を止めるのはメモリの上限（`--heap-max`）だけです。 |
| `max_integer` | `9223372036854775807` | なし | 64 ビットでいちばん大きい整数です。それより大きい整数もあり、それを超える結果も正確に求まります。 |
| `min_integer` | `-9223372036854775808` | なし | 64 ビットでいちばん小さい整数です。それより小さい整数もあります。 |
| `integer_rounding_function` | `toward_zero` | なし | `//` の丸め方です。`-7 // 2` は `-3` です。 |
| `max_arity` | `1024` | なし | 複合項の引数の数の上限です。 |
| `double_quotes` | `codes` | `chars`、`atom` | `"abc"` を、文字コードのリスト、文字のリスト、原子のどれとして読むかです。 |
| `unknown` | `error` | `fail` | 定義されていない述語を呼んだとき、`existence_error` を起こすか、失敗するかです。 |
| `char_conversion` | `off` | `on` | 項を読むときに、`char_conversion/2` で文字を変えるかどうかです。 |
| `debug` | `off` | `on` | ISO が求めるフラグです。`on` にすると `debug/0` と、`off` にすると `nodebug/0` と同じことをします。 |
| `encoding` | `utf8` | なし | テキストは UTF-8 です。 |
| `invalid_utf8` | `error` | `replace` | UTF-8 でないバイトを読んだとき、`representation_error(character)` を起こすか、代わりに U+FFFD を読むかです。 |
| `japanese_punctuation` | `false` | `true` | 全角の記号を半角として読むかどうかです（第 11 章）。 |
| `message_language` | `LANG` で決まる（`LANG` の無い Windows では表示言語で決まる） | `ja`、`en` | メッセージの言語です（第 11 章）。 |
| `float_overflow` | `error` | なし | 大きすぎる浮動小数点数の結果は `evaluation_error(float_overflow)` になります。 |
| `float_zero_div` | `error` | なし | 浮動小数点数を 0 で割ると `evaluation_error(zero_divisor)` になります。 |
| `float_undefined` | `error` | なし | `log(0)` のように、浮動小数点数の結果がないときは `evaluation_error(undefined)` になります。 |
| `dialect` | `mosa` | なし | この Prolog の処理系の名前です。 |
| `version` | `401` | なし | バージョンを 1 つの整数で表したものです。0.4.1 のことです。 |
| `version_data` | `mosa(0,4,1,[])` | なし | バージョンを項で表したものです。 |
| `argv` | `[]` | なし | `mosa` のコマンドラインの `--` より後ろの語、または実行ファイルの名前より後ろの語です。 |
| `trace_gc` | `false` | `true` | `true` なら、メモリを回収するたびに 1 行出します。 |
| `iso` | `true` | なし | 中核の意味は ISO に従います。逸脱は既定でオフか、明示的に取り込んだときだけ現れます。別の厳密適合モードはありません。 |

`bounded` から `debug` までのフラグは ISO の 7.11 節のもので、ほかは Mosa のものです。ファイルの中でディレクティブで設定した `double_quotes` と `japanese_punctuation` は、そのファイルの終わりまで続きます。

## ディレクティブ

ディレクティブは、ファイルに書く `:- Goal.` の行で、ファイルを読み込む途中で実行されます。ほとんどは述語ではなく、ファイルの中でしか使えません。`?-` のプロンプトでは使えません。ただし `consult/1`、`op/3`、`char_conversion/2`、`set_prolog_flag/2` は組み込みの述語でもあります。`use_module/1`、`use_module/2`、`ensure_loaded/1` もゴールとして呼べ、プロンプトでも使えます。ゴールは相対の名前を、読み込み中のファイルのフォルダでなく今のフォルダから開き、ゴールのモジュールに取り込みます。`dynamic/1`、`discontiguous/1`、`multifile/1`、`op/3`、`char_conversion/2`、`initialization/1`、`include/1`、`ensure_loaded/1`、`set_prolog_flag/2` は、ISO の 7.4.2 節のディレクティブです。

| ディレクティブ | すること |
| --- | --- |
| `module/2` | 最初の行の `:- module(Name, Exports).` は、そのファイルをモジュール `Name` にし、リストの述語を公開します（第 12 章）。 |
| `use_module/1` | `:- use_module(File).` はモジュールを 1 回だけ読み込み、公開されたものをすべて取り込みます。`File` は `library(Name)` か、自分で書いたファイルです（第 12 章）。 |
| `use_module/2` | `:- use_module(File, Imports).` はリストの述語だけを取り込みます。`Name/Arity as New` は新しい名前を付け、`except(List)` はリストのもの以外をすべて取り込みます。 |
| `reexport/1` | `:- reexport(File).` は、`use_module/1` と同じくモジュールを読み込んで取り込み、取り込んだものをすべて、読み込み中のモジュールが公開するものに足します。 |
| `reexport/2` | `:- reexport(File, Imports).` は、`use_module/2` と同じように取り込み、取り込んだものを公開します。`as` で新しい名前を付けた述語は、その名前で公開します。 |
| `export/1` | `:- export(Name/Arity).` は、モジュールが公開する述語を足します。 |
| `ensure_loaded/1` | `:- ensure_loaded(File).` は、まだ読み込まれていなければ、ファイルの節をここに入れます。 |
| `include/1` | `:- include(File).` は、そのたびにファイルの節をここに入れます。 |
| `consult/1` | `:- consult(File).` は、プロンプトと同じようにファイルを読み込みます。`:- [File1, File2].` はいくつも読み込みます。 |
| `initialization/1` | `:- initialization(Goal).` は、ファイルを読み込んだ後でゴールを 1 回実行します。 |
| `initialization/2` | `:- initialization(main, main).` は、読み込んだ後で `main` を実行し、プログラムを終えます（第 13 章）。 |
| `dynamic/1` | `:- dynamic(Name/Arity).` は、`assertz/1` や `retract/1` などで述語を変えられるようにします。`Name//Arity` は非終端記号（`Name/(Arity+2)`）を表します。 |
| `discontiguous/1` | `:- discontiguous(Name/Arity).` は、述語の節がファイルの中で離れていてもよいことにします。 |
| `multifile/1` | `:- multifile(Name/Arity).` は、述語の節をいくつものファイルに書けるようにします。 |
| `meta_predicate/1` | `:- meta_predicate(Spec).` は、述語のどの引数がゴールかを示します。そのゴールは呼び出した側のモジュールで実行されます。 |
| `op/3` | `:- op(Priority, Type, Name).` は、ファイルの残りで使う演算子を定義します。 |
| `char_conversion/2` | `:- char_conversion(A, B).` は、フラグ `char_conversion` が `on` のとき、文字 A を B として読みます。 |
| `set_prolog_flag/2` | `:- set_prolog_flag(Flag, Value).` はフラグを設定します。 |
| `if/1` | `:- if(Goal).` は条件の塊を開きます。`Goal` が成功すればその後ろの行を読み、そうでなければ、塊の次の `elif`、`else`、`endif` まで読み飛ばします。`Goal` は、ほかのディレクティブと同じく 1 回だけ実行します。 |
| `elif/1` | `:- elif(Goal).` は、塊のそれまでの分岐をどれも読んでおらず、`Goal` が成功すれば、その後ろの行を読みます。前の分岐を読んでいれば、`Goal` は呼びません。 |
| `else/0` | `:- else.` は、塊のそれまでの分岐をどれも読んでいなければ、その後ろの行を読みます。 |
| `endif/0` | `:- endif.` は塊を閉じます。どの `if` も、同じファイルの `endif` で閉じます。 |

読み飛ばす部分も項ごとに読むので、中の `if` と `endif` も対になります。しかし、そのディレクティブはどれも実行せず、節も足しません。読み飛ばす部分の構文の誤りは、何も出さずに読み飛ばします。ですから、分岐にほかの Prolog の処理系の構文を書けます。読む部分の構文の誤りは、ほかの場所と同じく読み込みを止めます。`if` の無い `elif`、`else`、`endif`、`else` の後ろの `elif`、2 つ目の `else`、ファイルの終わりで閉じていない `if` は、読み飛ばす部分の中でも読み込みを止めます。`Goal` が失敗すれば、その分岐を読み飛ばします。`Goal` がエラーを起こすと、ファイルは読み込まれません。この 4 つのディレクティブは ISO になく、述語でもありません。`?- if(true).` とプロンプトで打つと `existence_error(procedure, if/1)` になります。`mosac build` も条件を実行します（第 13 章）。

ほかの `:- Goal.` は、ファイルのその場所でゴールを 1 回実行します。ゴールが失敗するかエラーを起こすと、ファイルは読み込まれません。

## 組み込みの述語

組み込みの述語は、`use_module` なしで、どのモジュールでも使えます。下の表に、そのすべてを並べています。ISO の列は、Prolog を定める規格 ISO/IEC 13211-1 の節の番号です。モジュールについての第 2 部（13211-2）のものは、そう書いています。ダッシュは、その述語が ISO にないことを表します。`Name/Arity` は、名前と引数の数で述語を表します。

### 制御

ゴールを実行し、どれを実行するかを選びます（第 8 章と第 9 章）。規格の第 2 部は、`:/2` を組み込みの述語に入れていません。その行の 6.5.2 は、モジュールで限定した項をゴールにする節です。

<!-- predicates: system -->
| 述語 | ISO | すること |
| --- | --- | --- |
| `true/0` | 7.8.1 | 成功します。 |
| `fail/0` | 7.8.2 | 失敗します。 |
| `false/0` | 8.15.5 | `fail` と同じく失敗します。 |
| `!/0` | 7.8.4 | カットです。節のほかの選択肢と、節でカットより前にあるゴールのほかの選択肢を捨てます。 |
| `','/2` | 7.8.5 | `(A, B)`: A を実行し、次に B を実行します。 |
| `;/2` | 7.8.6、7.8.8 | `(A ; B)`: A か、そうでなければ B です。`(C -> T ; E)`: C が成功すれば T、そうでなければ E です。 |
| `->/2` | 7.8.7 | `(C -> T)`: C の最初の答えの後で T を実行します。C が失敗すれば失敗します。 |
| `*->/2` | — | `(C *-> T ; E)`: C の答えのたびに T を実行します。C に答えがなければ E です。 |
| `if/3` | — | `if(C, T, E)`: `(C *-> T ; E)` と同じです。 |
| `\+/1` | 8.15.1 | `\+ G`: G に答えがないとき成功します。何も束縛しません。 |
| `call/1` | 7.8.3 | `call(G)`: ゴール G を実行します。G の中のカットは G の中にとどまります。 |
| `call/2` | 8.15.4 | `call(G, A)`: G の最後の引数として A を足してから実行します。 |
| `call/3` | 8.15.4 | 同じことを、引数を 2 つ足して行います。 |
| `call/4` | 8.15.4 | 同じことを、引数を 3 つ足して行います。 |
| `call/5` | 8.15.4 | 同じことを、引数を 4 つ足して行います。 |
| `call/6` | 8.15.4 | 同じことを、引数を 5 つ足して行います。 |
| `call/7` | 8.15.4 | 同じことを、引数を 6 つ足して行います。 |
| `call/8` | 8.15.4 | 同じことを、引数を 7 つ足して行います。 |
| `once/1` | 8.15.2 | `once(G)`: G の最初の答えだけです。 |
| `repeat/0` | 8.15.3 | バックトラックで戻ってくるたびに、また成功します。 |
| `forall/2` | — | `forall(C, A)`: C のどの答えについても A が成功するとき成功します。 |
| `phrase/2` | 13211-3 | `phrase(G, L)`: 文法規則の本体 G がリスト L の全部を読みます。 |
| `phrase/3` | 13211-3 | `phrase(G, L, R)`: G が L の初めを読み、残りを R にします。 |
| `catch/3` | 7.8.9 | `catch(G, Catcher, Recovery)`: G を実行し、G が Catcher と単一化できるエラーを起こせば Recovery を実行します。 |
| `throw/1` | 7.8.10 | 項を例外として投げます。 |
| `setup_call_cleanup/3` | — | `setup_call_cleanup(S, G, C)`: S を 1 回実行してから G を実行し、G が終わったとき、つまり最後の答え、失敗、エラー、カットのときに C を 1 回実行します。 |
| `:/2` | 13211-2 6.5.2 | `M:G`: G をモジュール M の中で実行します（第 12 章）。 |

### 単一化と比較

項を単一化し（第 5 章）、項の標準の順序（変数、数、原子、複合項の順）で比べ、並べます。

<!-- predicates: system -->
| 述語 | ISO | すること |
| --- | --- | --- |
| `=/2` | 8.2.1 | 2 つの項を単一化します。 |
| `\=/2` | 8.2.3 | 2 つの項が単一化できないとき成功します。 |
| `unify_with_occurs_check/2` | 8.2.2 | 単一化します。ただし `X = f(X)` のように、`=` なら自分自身を含む項を作るところでは失敗します。 |
| `subsumes_term/2` | 8.2.4 | 2 つ目の項が 1 つ目の項の具体例のとき成功します。単一化しても 2 つ目の項は変わりません。何も束縛しません。 |
| `==/2` | 8.4.1 | 2 つの項が同じです。何も束縛しません。 |
| `\==/2` | 8.4.1 | 2 つの項が同じではありません。 |
| `@</2` | 8.4.1 | 標準の順序で、1 つ目の項が 2 つ目より前にあります。 |
| `@=</2` | 8.4.1 | 前にあるか、同じです。 |
| `@>/2` | 8.4.1 | 後ろにあります。 |
| `@>=/2` | 8.4.1 | 後ろにあるか、同じです。 |
| `compare/3` | 8.4.2 | `compare(Order, A, B)`: A が B より前にあるか、同じか、後ろにあるかによって、Order は `<`、`=`、`>` です。 |
| `sort/2` | 8.4.3 | リストを標準の順序で並べ、重なりを除きます。 |
| `sort/4` | — | `sort(Key, Order, List, Sorted)`: 要素の Key 番目の引数（0 なら要素全体）で並べます。Order は `@<`、`@=<`、`@>`、`@>=` のどれかです。`@<` と `@>` は、鍵の等しい要素を除きます。 |
| `keysort/2` | 8.4.4 | `Key-Value` の組のリストを鍵で並べます。鍵の等しい組は、元の順に残します。 |

### 型

引数が今どんな種類の項かを確かめます。どれも何も束縛しません。

<!-- predicates: system -->
| 述語 | ISO | すること |
| --- | --- | --- |
| `var/1` | 8.3.1 | 値のない変数です。 |
| `nonvar/1` | 8.3.7 | 値のない変数ではありません。 |
| `atom/1` | 8.3.2 | 原子です。 |
| `integer/1` | 8.3.3 | 整数です。 |
| `float/1` | 8.3.4 | 浮動小数点数です。 |
| `number/1` | 8.3.8 | 整数か浮動小数点数です。 |
| `atomic/1` | 8.3.5 | 原子か数です。 |
| `compound/1` | 8.3.6 | `f(a)` や `[a]` のような複合項です。 |
| `callable/1` | 8.3.9 | 原子か複合項です。つまり、ゴールとして実行できるものです。 |
| `ground/1` | 8.3.10 | 値のない変数を中に含まない項です。 |
| `acyclic_term/1` | 8.3.11 | 自分自身を含む部分のない項です。 |
| `is_list/1` | — | `[]` で終わるリストです。 |

### 項

項を分け、組み立てます。

<!-- predicates: system -->
| 述語 | ISO | すること |
| --- | --- | --- |
| `functor/3` | 8.5.1 | `functor(T, Name, Arity)`: T の名前と引数の数です。または、新しい変数を引数にした新しい T を作ります。 |
| `arg/3` | 8.5.2 | `arg(N, T, A)`: A は T の N 番目の引数です。1 から数えます。 |
| `=../2` | 8.5.3 | `T =.. L`: L は、T の名前の後に引数を並べたリストです。`f(a, b) =.. [f, a, b]` のようになります。 |
| `copy_term/2` | 8.5.4 | 新しい変数を使った、項の写しです。 |
| `term_variables/2` | 8.5.5 | 項の中の変数を、左から 1 回ずつ並べたリストです。 |
| `numbervars/3` | — | `numbervars(T, Start, End)`: T の変数を `'$VAR'(Start)`、`'$VAR'(Start+1)` などに束縛します。`write/1` はそれを `A`、`B` などと書きます。End は Start に変数の数を足した数です。End が整数でも変数でもないと `type_error(integer, End)` になります。 |
| `length/2` | — | `length(L, N)`: N は L の要素の数です。L に値がなければ、N 個の新しい変数のリストを作ります（第 7 章）。 |

### 算術

`is/2` は算術の式を評価し、比較は両辺を評価します（第 6 章）。

<!-- predicates: system -->
| 述語 | ISO | すること |
| --- | --- | --- |
| `is/2` | 8.6.1 | `X is E`: E を評価し、その値と X を単一化します。 |
| `=:=/2` | 8.7.1 | 2 つの値が等しいです。 |
| `=\=/2` | 8.7.1 | 2 つの値が等しくありません。 |
| `</2` | 8.7.1 | 1 つ目の値が 2 つ目より小さいです。 |
| `=</2` | 8.7.1 | 小さいか、等しいです。 |
| `>/2` | 8.7.1 | 大きいです。 |
| `>=/2` | 8.7.1 | 大きいか、等しいです。 |

### 原子と文字

原子と数の文字を扱います。1 文字は Unicode のスカラー値 1 つです（第 11 章）。

<!-- predicates: system -->
| 述語 | ISO | すること |
| --- | --- | --- |
| `atom_length/2` | 8.16.1 | 原子の文字の数です。 |
| `atom_concat/3` | 8.16.2 | `atom_concat(A, B, AB)`: AB は A の後に B を続けたものです。AB だけを与えると、分け方を 1 つずつ答えます。 |
| `sub_atom/5` | 8.16.3 | `sub_atom(Atom, Before, Length, After, Sub)`: Sub は Atom の中の Length 文字の部分で、前に Before 文字、後ろに After 文字があります。 |
| `atom_chars/2` | 8.16.4 | 原子と、その文字のリストの間で変えます。 |
| `atom_codes/2` | 8.16.5 | 原子と、その文字コードのリストの間で変えます。 |
| `char_code/2` | 8.16.6 | 文字と、その文字コードの間で変えます。 |
| `number_chars/2` | 8.16.7 | 数と、それを書く文字のリストの間で変えます。 |
| `number_codes/2` | 8.16.8 | 数と、それを書く文字コードのリストの間で変えます。 |
| `atom_number/2` | — | `'42'` のような原子と、それが表す数の間で変えます。 |
| `term_to_atom/2` | — | 項と、その項を文字で書いた原子の間で変えます。 |

### 節とモジュール

述語の節を見て、変えます。モジュールも見ます。

<!-- predicates: system -->
| 述語 | ISO | すること |
| --- | --- | --- |
| `asserta/1` | 8.9.1 | 節を、その述語のほかの節の前に足します。 |
| `assertz/1` | 8.9.2 | 節を、その述語のほかの節の後に足します。 |
| `retract/1` | 8.9.3 | 引数と単一化できる最初の節を消します。 |
| `retractall/1` | 8.9.5 | `retractall(Head)`: 頭部が Head と単一化できる節をすべて消します。 |
| `abolish/1` | 8.9.4 | `abolish(Name/Arity)`: 動的な述語を、節ごとすべて消します。 |
| `clause/2` | 8.8.1 | `clause(Head, Body)`: プログラムの述語の節です。ファイルから読み込んだ節も、`assertz` で足した節も返します。組み込みの述語では `permission_error` を投げます。 |
| `current_predicate/1` | 8.8.2 | `current_predicate(Name/Arity)`: プログラムの述語のうち、節があるものと、動的なもの（`dynamic` と宣言したもの、`assertz` や `asserta` で作ったもの）です。組み込みの述語は入りません。 |
| `predicate_property/2` | 13211-2 7.2.2 | `predicate_property(Head, P)`: P は、`built_in`、`dynamic`、`exported` のような、述語の性質です。 |
| `current_module/1` | 13211-2 7.2.1 | あるモジュールです。 |
| `consult/1` | — | ファイルを読み込みます（第 4 章）。名前には `.pl` が要ります。 |
| `use_module/1` | — | `use_module(File)`: ディレクティブと同じく、モジュールを 1 回だけ読み込み、公開された述語を全部、ゴールのモジュールに取り込みます（第 12 章）。読み込みが失敗すると、Mosa は理由を書き、ゴールは失敗します。 |
| `use_module/2` | — | `use_module(File, Imports)`: ディレクティブと同じく、リストの述語だけを取り込みます。 |
| `ensure_loaded/1` | — | `ensure_loaded(File)`: ディレクティブと同じく、まだ読み込んでいなければファイルを読み込みます。 |
| `strip_module/3` | — | `strip_module(Term, M, Plain)`: Term の頭の `M:` を、M が原子である間だけ外します。M はそのうち一番内側のモジュールで、Term に `M:` が無ければゴールのモジュールです。Plain は残りです。`strip_module(a:b:f(x), M, P)` は `M = b, P = f(x)` になります。 |
| `context_module/1` | — | `context_module(M)`: M はゴールのモジュールです。プロンプトでは `user`、モジュール `m` の節の中では `m`、`meta_predicate` の引数に渡したゴールでは呼び手のモジュールです。 |
| `module_property/2` | — | `module_property(M, P)`: P は `module/2` で作ったモジュールの性質です。`file(F)` は読み込んだファイル（`mosac` で作った実行ファイルでは、ファイルの埋め込みの鍵を作業ディレクトリに対して解決したパス。[CHANGELOG](../../../CHANGELOG.ja.md) の既知の制限の 1 つです）、`exports(L)` は公開している述語の `Name/Arity` のリスト（再公開したものも含め、公開した順）、`exported_operators(L)` は演算子を公開しているときの `op(P, T, N)` のリストです。`user` と `system` には性質がありません。 |
| `import/1` | — | `import(M:Name/Arity)`: M が公開している述語を 1 つ、ゴールのモジュールに取り込みます。`use_module/2` のリストの 1 項目と同じです。`as` は使えません。`M:` が無ければ、ゴールのモジュールが自分から取り込むので、何も変わりません。`M1:M2:Name/Arity` は内側の `M2` から取り込みます。 |

### すべての解

ゴールの答えをリストに集めます（第 8 章）。

<!-- predicates: system -->
| 述語 | ISO | すること |
| --- | --- | --- |
| `findall/3` | 8.10.1 | `findall(T, G, L)`: L は、G のすべての答えについての T のリストです。答えがなければ `[]` です。 |
| `findall/4` | — | `findall(T, G, L, Tail)`: `findall/3` と同じですが、リストは `[]` の代わりに Tail で終わります。 |
| `bagof/3` | 8.10.2 | `findall/3` と同じですが、G に答えがなければ失敗します。G のほかの変数の値ごとに、リストを 1 つずつ答えます。 |
| `setof/3` | 8.10.3 | `bagof/3` と同じですが、リストを並べ、重なりを除きます。 |

### ストリーム

ストリームは、テキストを読む元や書く先です。ファイル、端末、メモリがあります（第 10 章）。ストリームの引数には、ストリームか、`user_input`、`user_output`、`user_error` のような別名を渡します。

<!-- predicates: system -->
| 述語 | ISO | すること |
| --- | --- | --- |
| `open/3` | 8.11.5 | `open(File, Mode, S)`: ファイルを `read`、`write`、`append` のどれかで開き、ストリームを S にします。 |
| `open/4` | 8.11.5 | `open/3` と同じですが、`type(binary)` や `alias(Name)` のようなオプションのリストを付けます。 |
| `open_string/2` | — | `open_string(Text, S)`: 原子かコードのリストである Text から読むストリームです。 |
| `close/1` | 8.11.6 | ストリームを閉じます。 |
| `close/2` | 8.11.6 | `close/1` と同じですが、オプションのリストを付けます。`force(true)` は、エラーが起きても閉じます。 |
| `flush_output/0` | 8.11.7 | 今の出力のバッファにたまっているものを書き出します。 |
| `flush_output/1` | 8.11.7 | 同じことをストリームに行います。 |
| `current_input/1` | 8.11.1 | 今の入力のストリームです。 |
| `current_output/1` | 8.11.2 | 今の出力のストリームです。 |
| `set_input/1` | 8.11.3 | ストリームを今の入力にします。 |
| `set_output/1` | 8.11.4 | ストリームを今の出力にします。 |
| `stream_property/2` | 8.11.8 | `stream_property(S, P)`: P は、`file_name(F)` や `alias(A)` のような、開いているストリーム S の性質です。 |
| `at_end_of_stream/0` | 8.11.8 | 今の入力に、もう読むものがありません。 |
| `at_end_of_stream/1` | 8.11.8 | 同じことをストリームについて確かめます。 |
| `set_stream_position/2` | 8.11.9 | `stream_property/2` が `position(P)` として答えた位置に、ストリームを動かします。 |
| `with_output_to/2` | — | `with_output_to(atom(A), G)`: G を 1 回実行し、G が書いたものを原子 A にします。`codes(C)` ならコードのリストです。 |

### 文字とバイト

1 文字か 1 バイトを読み書きします。引数が 1 つ少ない述語は、今の入力か出力を使います。

<!-- predicates: system -->
| 述語 | ISO | すること |
| --- | --- | --- |
| `get_char/1` | 8.12.1 | 次の文字を読みます。終わりなら `end_of_file` です。 |
| `get_char/2` | 8.12.1 | 同じことをストリームから行います。 |
| `get_code/1` | 8.12.1 | 次の文字のコードを読みます。終わりなら -1 です。 |
| `get_code/2` | 8.12.1 | 同じことをストリームから行います。 |
| `peek_char/1` | 8.12.2 | `get_char/1` と同じですが、その文字は読まずに残します。 |
| `peek_char/2` | 8.12.2 | 同じことをストリームから行います。 |
| `peek_code/1` | 8.12.2 | `get_code/1` と同じですが、その文字は読まずに残します。 |
| `peek_code/2` | 8.12.2 | 同じことをストリームから行います。 |
| `put_char/1` | 8.12.3 | 文字を書きます。 |
| `put_char/2` | 8.12.3 | 同じことをストリームに行います。 |
| `put_code/1` | 8.12.3 | コードの文字を書きます。 |
| `put_code/2` | 8.12.3 | 同じことをストリームに行います。 |
| `nl/0` | 8.12.3 | 改行します。 |
| `nl/1` | 8.12.3 | 同じことをストリームに行います。 |
| `tab/1` | — | `tab(N)`: 空白を N 個書きます。 |
| `tab/2` | — | 同じことをストリームに行います。 |
| `get_byte/1` | 8.13.1 | バイナリのストリームの次のバイト（0 から 255）を読みます。終わりなら -1 です。 |
| `get_byte/2` | 8.13.1 | 同じことをストリームから行います。 |
| `peek_byte/1` | 8.13.2 | `get_byte/1` と同じですが、そのバイトは読まずに残します。 |
| `peek_byte/2` | 8.13.2 | 同じことをストリームから行います。 |
| `put_byte/1` | 8.13.3 | バイトを書きます。 |
| `put_byte/2` | 8.13.3 | 同じことをストリームに行います。 |

### 項の読み書き

項をまるごと読み書きし、演算子を定義します（第 10 章）。

<!-- predicates: system -->
| 述語 | ISO | すること |
| --- | --- | --- |
| `read_term/2` | 8.14.1 | `read_term(T, Options)`: ピリオドで終わる項を、`variable_names(V)` のようなオプションを付けて読みます。 |
| `read_term/3` | 8.14.1 | 同じことをストリームから行います。 |
| `read/1` | 8.14.1 | 項を読みます。終わりなら `end_of_file` です。 |
| `read/2` | 8.14.1 | 同じことをストリームから行います。 |
| `write_term/2` | 8.14.2 | `write_term(T, Options)`: 項を、`quoted(true)` のようなオプションを付けて書きます。`max_depth(N)` は深さ `N` より下の項を `...` と書き、`0` は制限なしです（規格にはありません）。`quoted(true)` で深さを決めると、原子 `'...'` は引用して書きます。 |
| `write_term/3` | 8.14.2 | 同じことをストリームに行います。 |
| `write/1` | 8.14.2 | 項を文字で書きます。 |
| `write/2` | 8.14.2 | 同じことをストリームに行います。 |
| `writeq/1` | 8.14.2 | `read/1` で読み戻すのに要るところに引用符を付けて、項を書きます。 |
| `writeq/2` | 8.14.2 | 同じことをストリームに行います。 |
| `write_canonical/1` | 8.14.2 | 引用符を付け、演算子を使わずに、項を書きます。 |
| `write_canonical/2` | 8.14.2 | 同じことをストリームに行います。 |
| `print/1` | — | `write/1` と同じように項を書きます。プログラムが `portray/1` を定義していれば、それを使います。 |
| `print/2` | — | 同じことをストリームに行います。 |
| `op/3` | 8.14.3 | `op(Priority, Type, Name)`: `op(700, xfx, は)` のように、演算子を定義します。 |
| `current_op/3` | 8.14.4 | `current_op(Priority, Type, Name)`: 定義されている演算子です。 |
| `char_conversion/2` | 8.14.5 | `char_conversion(A, B)`: フラグ `char_conversion` が `on` のとき、文字 A を B として読みます。 |
| `current_char_conversion/2` | 8.14.6 | `char_conversion/2` が設定した変換です。 |

### フラグとシステム

フラグを読んで設定し、Mosa を終え、メモリと時間を見ます。

<!-- predicates: system -->
| 述語 | ISO | すること |
| --- | --- | --- |
| `current_prolog_flag/2` | 8.17.2 | `current_prolog_flag(Flag, Value)`: フラグの値です。 |
| `set_prolog_flag/2` | 8.17.1 | 変えられるフラグを設定します。 |
| `halt/0` | 8.17.3 | 終了状態 0 で Mosa を終えます。 |
| `halt/1` | 8.17.4 | `halt(N)`: 終了状態 N で Mosa を終えます。-2147483648 から 2147483647 まで（C++ の `int`）の外の N には `representation_error(max_integer)` を、N が負なら `representation_error(min_integer)` を投げます。 |
| `nb_setval/2` | — | `nb_setval(Key, Value)`: Value の写しを原子 Key の下に置きます。バックトラックしても戻りません。 |
| `nb_getval/2` | — | `nb_getval(Key, Value)`: `nb_setval/2` が Key の下に置いたものの写しです。 |
| `garbage_collect/0` | — | もう使われていない項のメモリを、今すぐ回収します。 |
| `statistics/2` | — | `statistics(Key, Value)`: 時間（`runtime`、`walltime`、`cputime`）、メモリ（`heap`）、その回収（`garbage_collection`）についての数です。 |

### デバッグ

デバッガを入り切りし、どこで止まるかを決めます。使い方は第 15 章にあります。`mosa` では、デバッガが入っているときに動くゴールは、ポートごとに 1 行を書き、止まるポートでは 1 行を読みます。leap のあいだは、スパイポイントのある述語のポートだけを書きます。`mosac` で作ったプログラムでは、状態を持つだけで、ポートの行は出ません。`trace` のような名前は組み込み述語なので、自分のファイルで同じ名前と引数の数の述語を定義すると、`permission_error(modify, static_procedure, debug/0)` で読み込みが止まります。

<!-- predicates: system -->
| 述語 | ISO | すること |
| --- | --- | --- |
| `trace/0` | — | デバッガを入れ、見せる呼び出しのたびに止まるようにします。 |
| `notrace/0` | — | 呼び出しのたびに止まるのをやめます。デバッガは入ったままなので、スパイポイントでは止まります。 |
| `debug/0` | — | デバッガを入れます。スパイポイントでだけ止まります。フラグ `debug` は `on` になります。 |
| `nodebug/0` | — | デバッガを切ります。スパイポイントは残ります。フラグ `debug` は `off` になります。 |
| `spy/1` | — | `spy(Spec)`: 述語にスパイポイントを置き、デバッガを入れます。Spec は `Name/Arity`、`Module:Name/Arity`、名前だけの `Name`（文脈のモジュールから見えるその名前の述語すべて）、またはそれらのリストです。`Name/Arity`（存在するモジュールの `Module:Name/Arity` も）はまだ定義していない述語も指せますが、名前だけの `Name` が届くのは既にある述語だけです。ポートの無いゴール（制御構文、`call/1` から `call/8`、`true`、デバッガ自身の述語）にはスパイポイントを置かず、`spy/1` はそう知らせます。1 つも置かなければ、デバッガはそのままです。 |
| `nospy/1` | — | `nospy(Spec)`: `spy(Spec)` が置いたスパイポイントを外します。名前だけの `Name` は、文脈のモジュールから見えるその名前のスパイポイントをすべて外します。まだ定義していない述語に置いたものも外します。文脈のモジュールから見えない述語に `Module:Name/Arity` で置いたものは残るので、その形で外します。 |
| `nospyall/0` | — | スパイポイントをすべて外します。 |
| `leash/1` | — | `leash(Ports)`: デバッガが止まるポートを決めます。Ports は `all`、`none`、`call`、`exit`、`redo`、`fail`、`exception`、`+Port` と、`+` を付けないポート名（そのポートを今の組に足す）、`-Port`（外す）、またはそれらのリストで、左から順に当てます。`all` と `none` は組を全部入れるか空にします。決めたポートだけで止めるには、`leash([none, call, fail])` のように `none` から始めます。既定は 5 つすべてです。 |
| `debugging/0` | — | デバッガの状態、スパイポイント、止まるポートを `user_error` に書きます。述語は `writeq/1` が項 `Name/Arity` を引数として書くのと同じに書くので、演算子の名前は括弧に入り（`(=)/2`）、名前は引用されます（`'a, b'/1`）。`op/3` で `/` を変えた後も、字面を `nospy/1` に貼り直せます。`user` から `Name/Arity` で届かない述語は `Module:Name/Arity` と書きます。`spy/1` と `nospy/1` の知らせも同じ書き方です。 |

#### ポートでのコマンド

デバッガが止まるポートで、`mosa` は 1 行を読み、空白でない最初の文字をコマンドにします。

| 打つもの | 働き |
| --- | --- |
| Enter、`c` | creep: 次に見せるポートへ進み、そこで止まります。 |
| `s` | skip: 呼出と再試行では、同じ呼び出しの次のポートまで走ります。ほかのポートでは creep です。 |
| `l` | leap: スパイポイントのある述語のポートまで走ります。 |
| `f` | fail: その呼び出しを失敗させます。失敗のポートが出ますが、例外のポートでは失敗のポートを出さずに失敗させます。 |
| `r` | retry: その呼び出しの呼出のポートに戻り、束縛を元に戻します。副作用は戻りません。 |
| `a` | abort: 問い合わせを止めて、プロンプトに戻ります。`catch/3` では捕まえられません。その中に残った `setup_call_cleanup/3` の後始末は、先にポートを出さずに動き、その失敗と誤りは捨てます。例外のときと同じく、後始末には Setup の束縛は見え、Goal の束縛は見えません。その中の `nodebug/0` や `trace/0` は、abort の後のデバッガを変えません。 |
| `n` | nodebug: デバッガを切って続けます。入力の終わりも、これと同じです。 |
| `g` | 開いている呼び出しを、内側から深さ付きで書き、もう一度尋ねます。 |
| `w` | ゴールを深さを切らずに書き、もう一度尋ねます。 |
| `+`、`-` | この呼び出しの述語にスパイポイントを置きます（`-` は外します）。もう一度尋ねます。 |
| `h`、`?` | コマンドの一覧を書き、もう一度尋ねます。 |
| ほかの入力 | 「知らないコマンドです」と `h` を勧める知らせを書き、もう一度尋ねます。 |

プロンプトは、デバッガが creep のときは `[trace] ?- `、leap か skip のときは `[debug] ?- `、切れているときは `?- ` です。ポートの行は、ポート名、深さ、`writeq` と同じ書き方で深さ 10 までのゴールの順で、` 呼出: (2) len([],_G1) ? ` のようになります。止まるポートでは `? ` で終わります。行は `user_error` に出ます。

## ライブラリ

ライブラリは、ファイルに `:- use_module(library(Name)).` と書いて読み込みます（第 12 章）。どの述語も ISO にはありません。

### `library(lists)`

<!-- predicates: lists -->
| 述語 | すること |
| --- | --- |
| `append/3` | `append(A, B, AB)`: AB は、リスト A の後に B を続けたものです。AB だけを与えると、分け方を 1 つずつ答えます。 |
| `member/2` | `member(X, L)`: X は L の要素です。要素ごとに 1 つずつ答えます。 |
| `memberchk/2` | `member/2` と同じですが、単一化できる最初の要素だけです。 |
| `length/2` | 組み込みの `length/2` を、もう一度公開したものです。 |
| `nth0/3` | `nth0(I, L, X)`: X は L の I 番目の要素です。0 から数えます。 |
| `nth1/3` | `nth1(I, L, X)`: 同じですが、1 から数えます。 |
| `last/2` | `last(L, X)`: X は L の最後の要素です。 |
| `reverse/2` | リストを逆の順にしたものです。 |
| `msort/2` | リストを標準の順序で並べます。重なりは残します。 |
| `sum_list/2` | 数のリストの合計です。 |
| `max_list/2` | 数のリストのいちばん大きいものです。 |
| `list_to_set/2` | 前の要素と同じ（`==`）要素を除いたリストです。 |

### `library(apply)`

リストの要素について、ゴール G を呼びます。G は `maplist(atom_length, [ab, c], L)` の `atom_length` のように、引数をいくつか省いたゴールです。呼ぶたびに、要素を最後の引数として足します。

<!-- predicates: apply -->
| 述語 | すること |
| --- | --- |
| `maplist/2` | `maplist(G, L)`: L のどの要素についても G が成功します。 |
| `maplist/3` | `maplist(G, L1, L2)`: 長さの同じ 2 つのリストの、どの要素の組についても G が成功します。 |
| `maplist/4` | 同じことを 3 つのリストで行います。 |
| `maplist/5` | 同じことを 4 つのリストで行います。 |
| `foldl/4` | `foldl(G, L, V0, V)`: 要素ごとに、要素、それまでの値、次の値で G を呼びます。値は V0 から始まり、V で終わります。 |
| `foldl/5` | 同じことを 2 つのリストで行います。 |
| `foldl/6` | 同じことを 3 つのリストで行います。 |
| `include/3` | `include(G, L, In)`: In は、L の要素のうち G が成功するもののリストです。 |
| `exclude/3` | `exclude(G, L, Out)`: Out は、L の要素のうち G が失敗するもののリストです。 |

### `library(assoc)`

<!-- predicates: assoc -->
| 述語 | すること |
| --- | --- |
| `empty_assoc/1` | `empty_assoc(Assoc)`: Assoc は空の連想配列（AVL 木）です。 |
| `list_to_assoc/2` | `list_to_assoc(Pairs, Assoc)`: Assoc は、リストの Key-Value の組を持ちます。2 つの組が同じ鍵を持つと `domain_error(unique_key_pairs, Pairs)` になります。 |
| `ord_list_to_assoc/2` | `ord_list_to_assoc(Pairs, Assoc)`: 鍵が標準順序で厳密に昇る組に対する `list_to_assoc/2` です。そうでなければ `domain_error(key_ordered_pairs, Pairs)` になります。 |
| `get_assoc/3` | `get_assoc(Key, Assoc, Value)`: Value は、Key のところにある値です。無ければ失敗します。 |
| `get_assoc/5` | `get_assoc(Key, Assoc0, Old, Assoc, New)`: Old は Assoc0 の Key の値で、Assoc は、そこに New を入れた Assoc0 です。Key が無ければ失敗します。 |
| `max_assoc/3` | `max_assoc(Assoc, Key, Value)`: Key-Value は、鍵が最大の組です。Assoc が空なら失敗します。 |
| `min_assoc/3` | `min_assoc(Assoc, Key, Value)`: Key-Value は、鍵が最小の組です。Assoc が空なら失敗します。 |
| `gen_assoc/3` | `gen_assoc(Key, Assoc, Value)`: 鍵の昇順に、Key-Value の組を 1 つずつ返します。Key が基底なら、引くだけで選択点を残しません。 |
| `put_assoc/4` | `put_assoc(Key, Assoc0, Value, Assoc)`: Assoc は、Assoc0 の Key のところに Value を入れたものです。古い値は置き換わります。 |
| `del_assoc/4` | `del_assoc(Key, Assoc0, Value, Assoc)`: Value は Key のところにある値で、Assoc は、その組を除いた Assoc0 です。Key が無ければ失敗します。 |
| `del_min_assoc/4` | `del_min_assoc(Assoc0, Key, Value, Assoc)`: Key-Value は鍵が最小の組で、Assoc は、それを除いた Assoc0 です。Assoc0 が空なら失敗します（SWI-Prolog のマニュアルは注意書きで、この場合は何も束縛せずに成功すると書いています）。 |
| `del_max_assoc/4` | `del_max_assoc(Assoc0, Key, Value, Assoc)`: 鍵が最大の組について同じことをします。Assoc0 が空なら失敗します（SWI-Prolog のマニュアルは注意書きで、この場合は何も束縛せずに成功すると書いています）。 |
| `assoc_to_list/2` | `assoc_to_list(Assoc, Pairs)`: Pairs は、Key-Value の組を鍵の昇順に並べたリストです。 |
| `assoc_to_keys/2` | `assoc_to_keys(Assoc, Keys)`: Keys は、鍵を昇順に並べたリストです。 |
| `assoc_to_values/2` | `assoc_to_values(Assoc, Values)`: Values は、値を鍵の順に並べたリストです。 |
| `is_assoc/1` | `is_assoc(Assoc)`: Assoc は正しい AVL 木です。鍵が厳密に昇り、各節の Balance が高さと合い、左右の高さの差が 1 以下です。 |
| `map_assoc/2` | `map_assoc(G, Assoc)`: すべての値について、鍵の順に `call(G, Value)` が成り立ちます。 |
| `map_assoc/3` | `map_assoc(G, Assoc0, Assoc)`: Assoc は Assoc0 と同じ鍵を持ち、各値は、Assoc0 の値 X に対して `call(G, X, Y)` が成り立つ Y です。 |

### `library(between)`

<!-- predicates: between -->
| 述語 | すること |
| --- | --- |
| `between/3` | `between(Low, High, X)`: X は Low から High までの整数で、1 つずつ答えます。 |
| `succ/2` | `succ(X, Y)`: Y は X + 1 です。X は 0 以上です。 |
| `plus/3` | `plus(X, Y, Z)`: Z は X + Y です。3 つのうちどれか 1 つは値がなくてもかまいません。 |

### `library(format)`

書式のテキストには、`~` で始まる指示を書きます。`~w` は引数を `write/1` のように、`~p` は `print/1` のように（自分で書いた `portray/1` を呼びます）、`~q` は `writeq/1` のように、`~k` は `write_canonical/1` のように書きます。`~W` は項と書き出しの選択肢のリストの 2 つの引数をとり、項を `write_term/2` のように書きます。`~a` は原子、`~d` は整数、`~f` と `~e` は浮動小数点数、`~s` はコードのリスト、`~c` は文字コードを書き、`~i` は引数を 1 つ読み飛ばします。`~E` は `~e` と同じく浮動小数点数を書き、指数の前の字を `E` にします。`~g` は C の書式 `%g` と同じく浮動小数点数を書きます。`g` の前の数（無ければ 6）が有効数字の桁数で、指数の大きさによって `~f` と `~e` のどちらの形にするかが決まり、小数部の末尾の 0 は書きません。`~G` は同じことをして、指数の前の字を `E` にします。`~D` は `~d` と同じく整数を書き、整数部を 3 桁ごとに `,` で区切ります。`r` の前に数を書いた `~r`（`~16r` など）は、整数をその数を基数（2〜36、数が無ければ 8）として書き、9 より上の桁を英小文字で書きます。`R` の前に数を書いた `~R`（`~16R` など）は、同じことを英大文字でします。`~n` は改行を書き、`~N` は出力がまだ行の頭になければ改行を書きます。`~t` と `~|` は桁をそろえます。`+` の前に数を書いた `~+`（`~10+` など）は、前の桁止め（前の `~|` か `~+` でそろえた桁と、行の頭のうち後のほう）から、その数だけ先の桁にそろえます（数が無ければ 8）。桁は、漢字などの東アジアの幅の広い字を 2 と数えます。`~~` は `~` を書きます。

たとえば、次の 1 行を書いたファイル `fmt.pl` を第12章と同じく読み込むと、次のようになります。

<!-- file: format-file fmt.pl -->
```prolog
:- use_module(library(format)).
```

<!-- check: format-group -->
```text
?- consult('fmt.pl').
true.
?- format("~D~n", [1234567]).
1,234,567
true.
?- format("~g ~2G~n", [0.00000123, 392.65]).
1.23e-06 3.9E+02
true.
?- format("~a~t~6+~a~t~6+|~n", [ab, '日本']).
ab    日本  |
true.
```

<!-- predicates: format -->
| 述語 | すること |
| --- | --- |
| `format/1` | `format(F)`: 書式のテキスト F を書きます。 |
| `format/2` | `format(F, Args)`: F を書きます。指示のところには、リスト Args の要素を書きます。 |
| `format/3` | `format(S, F, Args)`: 同じことをストリーム S に行います。 |

### `library(ordsets)`

<!-- predicates: ordsets -->
| 述語 | すること |
| --- | --- |
| `is_ordset/1` | `is_ordset(Set)`: Set は、要素が標準順序で厳密に昇る、真のリストです。 |
| `ord_empty/1` | `ord_empty(Set)`: Set は空の順序集合 `[]` です。 |
| `ord_seteq/2` | `ord_seteq(Set1, Set2)`: 2 つの集合が等しい（`==`）ことを確かめます。 |
| `list_to_ord_set/2` | `list_to_ord_set(List, Set)`: Set は、List を並べ替えて重なりを除いたものです。 |
| `ord_intersect/2` | `ord_intersect(Set1, Set2)`: 2 つの集合に共通の要素が 1 つ以上あります。 |
| `ord_disjoint/2` | `ord_disjoint(Set1, Set2)`: 2 つの集合に共通の要素がありません。 |
| `ord_intersection/2` | `ord_intersection(Sets, Intersection)`: Intersection は、リスト Sets のどの集合にもある要素です。Sets が `[]` のときは失敗します。 |
| `ord_intersection/3` | `ord_intersection(Set1, Set2, Intersection)`: Intersection は、両方の集合にある要素です。 |
| `ord_intersection/4` | `ord_intersection(Set1, Set2, Intersection, Difference)`: `ord_intersection/3` と同じで、Difference は `ord_subtract(Set2, Set1, Difference)` です。 |
| `ord_add_element/3` | `ord_add_element(Set, Element, NewSet)`: NewSet は、Set に Element を加えたものです。 |
| `ord_del_element/3` | `ord_del_element(Set, Element, NewSet)`: NewSet は、Set から Element を除いたものです。 |
| `ord_selectchk/3` | `ord_selectchk(Element, Set, Rest)`: Rest は、Set から Element を除いたものです。Set に Element が無ければ失敗します。2 つの集合は、どちらを束縛しないでおいてもかまいません。Set が束縛されていなければ、Set は Rest に Element を加えたものになり、Rest に Element が既にあれば失敗します。 |
| `ord_memberchk/2` | `ord_memberchk(Element, Set)`: Element が Set にあります（`compare/3` で確かめ、選択点を残しません）。 |
| `ord_range/4` | `ord_range(Set, Low, High, Range)`: Range は、Set の要素のうち Low から High まで（両端を含む）です。 |
| `ord_subset/2` | `ord_subset(Sub, Super)`: Sub の要素はすべて Super にあります。 |
| `ord_subtract/3` | `ord_subtract(Set, Remove, Rest)`: Rest は、Set の要素のうち Remove に無いものです。 |
| `ord_union/2` | `ord_union(Sets, Union)`: Union は、リスト Sets のどれかの集合にある要素です。 |
| `ord_union/3` | `ord_union(Set1, Set2, Union)`: Union は、どちらかの集合にある要素です。 |
| `ord_union/4` | `ord_union(Set1, Set2, Union, New)`: `ord_union/3` と同じで、New は `ord_subtract(Set2, Set1, New)` です。 |
| `ord_symdiff/3` | `ord_symdiff(Set1, Set2, Difference)`: Difference は、どちらか一方だけにある要素です。例えば `ord_symdiff([1, 2], [2, 3], X)` は `X = [1, 3]` になります。 |

### `library(pairs)`

<!-- predicates: pairs -->
| 述語 | すること |
| --- | --- |
| `pairs_keys_values/3` | `pairs_keys_values(Pairs, Keys, Values)`: Pairs は、鍵が Keys、値が Values の `Key-Value` のリストです。どちら向きにも使えます。 |
| `pairs_keys/2` | `pairs_keys(Pairs, Keys)`: Keys は、Pairs の鍵のリストです。 |
| `pairs_values/2` | `pairs_values(Pairs, Values)`: Values は、Pairs の値のリストです。 |
| `group_pairs_by_key/2` | `group_pairs_by_key(Pairs, Groups)`: 鍵が `==` で隣り合う組を、1 つの `Key-Values` にまとめます。鍵をすべて集めるには、先に Pairs を `keysort/2` で並べ替えます。`keysort/2` は、`sort/2` と違って同じ組も残します。 |
| `transpose_pairs/2` | `transpose_pairs(Pairs, Transposed)`: 各組の鍵と値を入れ替え、新しい鍵で `keysort/2` した結果にします。 |
| `map_list_to_pairs/3` | `map_list_to_pairs(F, List, Pairs)`: Pairs は、List の各 X について `Key-X` を並べたものです。鍵は `call(F, X, Key)` で求めます。 |

### `library(strings)`

<!-- predicates: strings -->
| 述語 | すること |
| --- | --- |
| `atomic_list_concat/2` | リストの原子と数をつないで、1 つの原子にします。 |
| `atomic_list_concat/3` | `atomic_list_concat(L, Sep, A)`: 間に Sep をはさんでつなぎます。L に値がなければ、A を Sep のところで分けます。 |
| `split_atom/4` | `split_atom(Text, Seps, Pad, Parts)`: Text を Seps のどの文字のところでも分け、分けたものの前後から Pad の文字を除きます。 |
| `upcase_atom/2` | 原子の英字などを大文字にしたものです。 |

### `library(dcg)`

<!-- predicates: dcg -->
| 述語 | すること |
| --- | --- |
| `expand_term/2` | `expand_term(T, E)`: 文法規則 `H --> B` を節 E に訳します。規則でない項はそのまま返します。T に値がなければ instantiation_error です。 |

### `library(japanese)`

<!-- predicates: japanese -->
| 述語 | すること |
| --- | --- |
| `ひらがな_カタカナ/2` | ひらがなをカタカナにします。2 つ目の引数だけを与えると、逆に変えます。 |
| `全角_半角/2` | 全角の英字、数字、記号を半角にします。2 つ目の引数だけを与えると、逆に変えます。 |
| `読み幅/2` | 画面でのテキストの幅です。漢字のような幅の広い文字は 2、ほかは 1 と数えます。 |

## ISO との違い

Mosa は ISO/IEC 13211-1 とその正誤表に従います。従っていないところと、ISO が処理系に任せているところは、次のとおりです。

- 整数の大きさに限りはなく、フラグ `bounded` は `false` です。結果を止めるのはメモリの上限（`--heap-max`）だけです。`^` と `<<`、そして大きな整数を引数にした 2 引数の `+`、`-`、`*` とビット演算（`/\`、`\/`、`xor`、`\`）は、先に結果の大きさを見積もり、見積もりが残りのメモリを超えるときは計算する前に問い合わせを止めます。ほかの演算（`//`、`mod`、`gcd/2`、`abs/1`、1 引数の `-` など）は、先に結果を計算し、それを置くときに問い合わせを止めます。`max_integer` と `min_integer` は、64 ビットでいちばん大きい整数といちばん小さい整数のままです。浮動小数点数の範囲を超える整数は、評価がそれを浮動小数点数に直さなければならないところで `evaluation_error(float_overflow)` になります。浮動小数点数と組んだ `+`、`-`、`*`、`/`、`^`、どんな引数でも浮動小数点数を返す `**` と `sqrt/1`、`log/1`、`sin/1` などの関数、浮動小数点数との算術比較（`<`、`=:=` など）、浮動小数点数との `min/2` と `max/2`、そして `float/1` です。2 つの整数の `/` は、商がその範囲を超えるときだけこの誤りになります。`2^2000 / 3` は誤りになり、`2^2000 / 2^1999` は `2.0` です。標準順序（`compare/3`、`@<`）は、そのような整数と浮動小数点数を誤りなしに比べます。浮動小数点数は 64 ビットで、Mosa は、読み戻すと同じ数になるいちばん短い字を書きます。
- `unknown` に使える値は `error` と `fail` だけで、3 つの `float_*` のフラグに使える値は `error` だけです。
- モジュール（第 12 章）は、規格の第 2 部（ISO/IEC 13211-2）の書き方を使います。モジュールが持つのは述語と演算子です。原子はどのモジュールでも同じです。
- 上の表でダッシュの付いた述語は、ISO にない追加のものです。`format/2`、`between/3`、`member/2`、`append/3` など、ライブラリの述語は組み込みではなく、`use_module` が要ります。
- 全角の記号を半角として読むのは、フラグ `japanese_punctuation` が `true` のときだけです（第 11 章）。
- 読みで開いたファイルは、開くときに全体を読まず、必要な分だけ読みます。後からほかのプログラムが足した分も、さらに読みが必要になったときには読めます。終わりを peek で見た後も同じです。開いている流れすべてが読んだ分は、読み足すたびにヒープと合わせてメモリの上限（`--heap-max`）に数え、超えて読むと問い合わせはその上限で止まります。Prolog のテキストの読み込み（`consult` など）は通常ファイルだけを受け、装置やパイプは受けません。同じ上限の中です。追記で開くときも、すでにある分については同じ規則です。

こう決めた理由は、節ごとに、ソースの木の [ISO の解釈の覚え書き](https://github.com/SilentMalachite/mosa-prolog/blob/main/docs/iso-notes.ja.md)に書いています。

## 既知の制限

今の版の既知の制限は [CHANGELOG](../../../CHANGELOG.ja.md) に並べています。制限が見つかったり、なくなったりすれば、そこを直します。主なものは次のとおりです。

- Windows のコンソールのウィンドウで日本語を打つことは未検証です。
- `mosac build --debug` は、最適化して作った実行時ライブラリにつなぎます。ライブラリの節は出しません。
- デバッガ（第 15 章）の変数の番号は、行ごとに振ります。2 つのポートの行の `_G1`（`g` が書く行も含みます）は、別の変数かもしれません。
- leap のあいだ、Mosa はスパイポイントのある述語の呼び出しにだけ箱を開くので、深さと `g` の一覧には、跳んでいるあいだの呼び出しが入りません。前に creep で開いた箱は、そのまま数えます。ファイルを読み込む途中の指令や `initialization/1` のゴールの中では、その指令の中で開いた箱だけを数えます。
- 入力が端末でない `mosa` は、コマンドを読みません。ポートの行を書いて先へ進みます。
- デバッガの 9 つの述語（`trace/0` や `spy/1` など）と同じ名前と引数の数の述語は、自分のファイルでは定義できません。
- トレース中は、末尾呼び出しも箱を残すので、深い再帰はメモリをより多く使います。
- `r`（retry）は、出力のような副作用を元に戻しません。
- `mosac build` で作ったプログラムには、ポートは出ません。`trace` などは状態を変えるだけです。`mosac build --debug`（節の追跡、第 13 章）は別のものです。`mosatest` と組み込みの `Machine` にもコンソールは無く、そこではデバッガの述語は状態を変えるだけです。
- `phrase/2,3` が呼ぶ文法規則は、変換後の本体の単一化を見せます。たとえば `[a,a]=[a|_G1]` です。
- 動いている `setup_call_cleanup/3` の後始末の中のポートで `a` を打つと、その後始末と、同じカットがその後に動かす後始末は動きません。
- `system:Goal` と書いたゴール、組み込みが自分のために走らせるゴール、ライブラリの述語が中で行う呼び出しは、トレースされません。`maplist/2` のようにライブラリの述語が呼ぶあなたのゴールと、`lists:append(X, Y, Z)` のようにライブラリの名前を付けて書いた 1 つのゴールは、トレースされます。`lists:` の後の制御構文の中のゴールと、ライブラリの述語に呼ばせる `lists:` 付きのゴールは、トレースされません。
- Windows では、入力が `NUL` デバイスの `mosa` は端末と見なされ、最初のポートで止まります。実行の記録はパイプで取ります（第 15 章）。
- leap で着いたポートで Enter を押すと、デバッガは creep に切り替わり、プロンプトが `[trace]` になります。
- `initialization/1` のゴールの中での `a`（abort）は、その読み込みを失敗させますが、読んだ節は残します。このようなゴールのエラーと同じです。指令で読んだファイルの中でも、どちらの形でも同じです。`:- Goal` の指令の中では、読み込みを元に戻し、その指令が変えたデバッガのオンとオフの切り替えとモードと止まるポート（たとえば `:- trace, foo.` の `trace`）も元に戻ります。スパイポイントは残ります。
- `:- Goal` の指令が失敗するかエラーになって読み込みが元に戻ると、その中で打った `n`（nodebug）も元に戻ります。デバッガのオンとオフの切り替えとモードと止まるポートは読み込みの前の状態に戻ります。`initialization/1` のゴールが失敗したときは節が残り、`n` もそのままです。
- ポートの行の深さの上限 10 は `write_term/2,3` の `max_depth/1` と同じなので、最初の要素に入れ子になったリストは、どれだけ深くても切られません。リストの i 番目の要素は、深さ d+i-1 と数えるからです。
- Windows では、Smart App Control が有効だと Mosa は動きません（第 2 章）。
- 大きな整数の算術の 1 歩では、Mosa は結果が残りのメモリに入るかを確かめます。この確かめは、もう使われていない項のメモリも使用中として数えます。そのため、ガベージコレクションをすれば空きができるときでも、その 1 歩がメモリの上限で止まることがあります。
- 算術は、読む大きな整数の写しで計算します。そのため、使える整数のいちばん大きなものはメモリの上限より小さくなります。`A > 0` や `A mod 7` のように読むだけなら、整数が上限の約半分を占めると止まります。`A + 0` のように結果も同じほど大きいなら、約 3 分の 1 で止まります。
- Mosa が読む字の中の整数は、メモリの上限と比べる前に、全体を数に直します。そのため、とても長い整数を読むと、上限で止まるまでに、その字の何倍ものメモリを使います。

---

[目次](README.md) ・ 前: [13. 実行ファイルを作る（`mosac`）](13-mosac.md) ・ 次: [15. デバッガ](15-debugger.md)
