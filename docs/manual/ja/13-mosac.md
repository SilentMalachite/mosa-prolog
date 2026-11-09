[English](../en/13-mosac.md) | 日本語

# 13. 実行ファイルを作る（`mosac`）

この章では、C++ のコンパイラを入れ、`mosac` で Prolog のプログラムを実行ファイルにして、それを動かします。

実行ファイルは、コンピュータがプログラムとして動かすファイルです。`mosa` も実行ファイルです。`mosac` は、Prolog のプログラムを、別のプログラミング言語である C++ に訳し、それから C++ のコンパイラを呼びます。コンパイラは、C++ から実行ファイルを作るプログラムです。できた実行ファイルは、`mosa` がなくても、`.pl` のファイルがなくても動きます。

## 手順

1. C++ のコンパイラを入れます。これは 1 回だけです。macOS では Xcode Command Line Tools を入れます。次の行を打ち、出てきた画面で「インストール」を押して、使用許諾に同意します。Command Line Tools がすでに入っていれば、コマンドがそう知らせます。そのときは、何もしなくてかまいません。

<!-- skip: compiler-macos -->
```console
$ xcode-select --install
```

Linux（Ubuntu）では `g++` を入れます。`apt update` は、先に Ubuntu のパッケージの一覧を新しくします。`sudo` がパスワードを聞き、`apt install` が続けてよいかを聞くので、`Y` と打ちます。

<!-- skip: compiler-linux -->
```console
$ sudo apt update && sudo apt install g++
```

Windows では、[Build Tools for Visual Studio 2022 のインストーラー](https://aka.ms/vs/17/release/vs_BuildTools.exe)をダウンロードして実行します。インストーラーで「C++ によるデスクトップ開発」のワークロードを選び、「インストール」を押します。特別なウィンドウは要りません。`mosac` は、ふつうの PowerShell のウィンドウからコンパイラを見つけます。インストーラーが終わったら、PowerShell を閉じて開き直します。Mosa 0.1.0 は Visual Studio 2022 で動作を確かめています。それより新しい Visual Studio も入っていると、`mosac` はいちばん新しいものを使いますが、その組み合わせは確かめていません。

2. `mosac` がコンパイラを見つけられるか確かめます。`mosac toolchain` は、コンパイラとそのバージョン、CMake、実行時ライブラリのフォルダを出します。CMake は、入っていれば `mosac` が使い、入っていなければ使わずに済ませるビルドの道具です。実行時ライブラリは、どの実行ファイルにも入る Mosa の一部です。これらの行には自分のコンピュータのフォルダ名が出るので、コンピュータごとに違います。コンパイラが見つからないときは、1 行目が `mosac: C++20 のコンパイラが見つかりません` になり、次の行に入れるものが出ます。Windows では、実行時ライブラリのパスか、`mosac build` が作るフォルダ `.mosac-cache`（手順 4）のパスに、日本語の字のような ASCII の外の文字があるときも、`mosac` は CMake を使いません。Visual Studio のコンパイラ向けの CMake のビルドは、そうした字を落とすことがあるので、`mosac` はコンパイラを直接呼びます。

<!-- skip: toolchain -->
```console
$ mosac toolchain
```

3. 第 4 章の `cd` の行でフォルダ `prolog` に移ります。そのフォルダに `こんにちは.pl` というファイルを作り、次の行を書きます。1 行目はディレクティブ（第 11 章）です。`initialization(main, main)` は、ファイルを読み込んだらゴール `main` を実行し、`main` が終わったらプログラムを終えるように Mosa に伝えます。規則は `main` のすることを表します。`こんにちは` を書き出して改行します。

<!-- file: hello-file こんにちは.pl -->
```prolog
:- initialization(main, main).

main :- write(こんにちは), nl.
```

4. 実行ファイルを作り、動かします。`mosac build こんにちは.pl -o こんにちは` は、うまくいくと何も出さず、1、2 秒で終わります。`-o こんにちは` は実行ファイルの名前で、Windows では `こんにちは.exe` になります。`./こんにちは` は、このフォルダのプログラム `こんにちは` を動かします。`clang++` はあるが `g++` はない Linux では、`mosac build` が「このコンパイラ用の実行時ライブラリがありません」と出して止まります。どうすればよいかは[何が起きたか](#何が起きたか)に書いています。

<!-- check: build -->
```console
$ mosac build こんにちは.pl -o こんにちは
$ ./こんにちは
こんにちは
```

`./` を忘れずに打ってください。`./` がないと、シェルは PATH のフォルダ（第 2 章）だけで `こんにちは` を探し、今のフォルダは探さないので、見つけられません。macOS の標準のシェルの zsh は `zsh: command not found: こんにちは` のように、bash は `bash: こんにちは: command not found` のように出します。Ubuntu の bash は、パッケージを入れるように勧めることもありますが、入れる必要はありません。Windows の PowerShell では `.\こんにちは` と打ちます（`./こんにちは` でも動きます）。コマンドプロンプト（cmd）では、`こんにちは` だけで動きます。

`mosac build` は、フォルダ `prolog` に `.mosac-cache` というフォルダも作りました。ここには、`mosac` が途中で作った C++ のファイルなどが入っています。このフォルダがあるので、同じプログラムをもう一度作るときは速く終わります。名前がピリオドで始まるので、`ls` や macOS の Finder には出ません（`ls -a` なら出ます）。このフォルダは、いつ消してもかまいません。実行ファイルはそのまま動き、次の `mosac build` がまたフォルダを作ります。そのときは、また 1、2 秒かかります。

5. 自分で実行ファイルを作らずに、プログラムを動かします。`mosa こんにちは.pl` はファイルを読み込みます。するとディレクティブが `main` を実行して Mosa を終えるので、`?-` のプロンプトは出ません。`mosac run こんにちは.pl` は、`mosac build` と同じように `.mosac-cache` の中でプログラムを作って動かし、フォルダ `prolog` には実行ファイルを残しません。

<!-- check: run -->
```console
$ mosa こんにちは.pl
こんにちは
$ mosac run こんにちは.pl
こんにちは
```

6. いくつかのファイルからプログラムを作ります。第 12 章のファイル `家系.pl` がフォルダ `prolog` になくなっていたら、作り直します。

<!-- file: module-file 家系.pl -->
```prolog
:- module(家系, [祖父母/2]).

親(太郎, 一郎).
親(太郎, 花子).
親(一郎, 桃子).
親(一郎, 次郎).
親(次郎, 三郎).

祖父母(X, Z) :- 親(X, Y), 親(Y, Z).
```

それから、フォルダ `prolog` に `孫.pl` というファイルを作り、次の行を書きます。この `main` は、第 12 章の `孫を書く/1` と同じように、太郎の孫を書き出します。

<!-- file: grandchildren-file 孫.pl -->
```prolog
:- initialization(main, main).
:- use_module(家系).
:- use_module(library(format)).

main :- forall(祖父母(太郎, Z), format('~wは太郎の孫です~n', [Z])).
```

7. 実行ファイルを作って動かします。`mosac` は、プログラムが `use_module/1` で読み込むファイルを、ライブラリも含めてすべて実行ファイルに入れます。

<!-- check: build-modules -->
```console
$ mosac build 孫.pl -o 孫
$ ./孫
桃子は太郎の孫です
次郎は太郎の孫です
```

8. `main` がエラーを起こしたとき、プログラムがどうなるかを見ます。フォルダ `prolog` に `割り算.pl` というファイルを作って次の行を書き、実行ファイルを作って動かします。プログラムは、`?-` のプロンプトと同じ警告（第 9 章）を出して終わります。

<!-- file: divide-file 割り算.pl -->
```prolog
:- initialization(main, main).

main :- X is 10 / 0, write(X), nl.
```

<!-- check: build-error -->
```console
$ mosac build 割り算.pl -o 割り算
$ ./割り算
警告: ゴールは例外を送出しました: error(evaluation_error(zero_divisor),context((is)/2,_G1))
```

## 何が起きたか

`mosac build` は、`mosa` と同じようにプログラムを読み、`.mosac-cache` の中で C++ に訳して、コンパイラを呼びました。コンパイラは実行ファイルを作り、それに実行時ライブラリをつなぎました。実行時ライブラリは、プロンプトで `mosa` がしていることを、実行ファイルの中でします。ゴールを実行し、単一化し、バックトラックし、書き出します。ですから実行ファイルには、`mosa` も `.pl` のファイルも要りません。ファイルに誤りがあると、`mosac build` は `mosa` と同じように行と桁を出し（第 4 章）、実行ファイルを作らずにシェルに戻ります。

`initialization(main, main)` があると、そのファイルはプログラムになります。`main` が成功すると、プログラムは終わります。`main` が失敗すると、プログラムは何も出さずに終わります。`main` が例外を投げ、どこでも捕まえられなければ、手順 8 のように、プログラムは警告を出して終わります。このディレクティブがないと、`main` を呼ぶものがないので、実行ファイルは起動してすぐに終わり、何も出しません。

実行ファイルは、`mosa` と同じようにメッセージの言語を選びます（第 11 章）。`LANG` で決まり、`LANG` が設定されていなければ、Windows の表示言語で決まります。ですから手順 8 の警告は、`LANG` が `en` で始まっていれば英語で出ます。

`mosac` は、決まった名前でコンパイラを探します。macOS では `clang++`、Linux では `g++`、なければ `clang++`、Windows では Visual Studio のコンパイラです。ファイルを書き換えたら、もう一度 `mosac build` を実行します。このときは、また 1、2 秒かかります。書き換えていないプログラムなら、`.mosac-cache` にあるものを使うので、すぐに終わります。

Linux のパッケージには、`g++` 用の実行時ライブラリだけが入っています。`clang++` はあるが `g++` はない Linux では、`mosac` は `clang++` を見つけ、`mosac build` は次のような 2 行を出して止まります。行の中のフォルダは、Mosa を展開した場所です。

<!-- skip: clang-linux -->
```console
$ mosac build こんにちは.pl -o こんにちは
mosac: このコンパイラ用の実行時ライブラリがありません: /home/<名前>/mosa-0.1.0-linux-x86_64/lib/mosa/linux-x86_64-clang
mosac と同じコンパイラを使うか、mosac build --runtime-source で同梱の源から作ってください（結果は .mosac-cache に残ります）
```

いちばん簡単なのは、手順 1 のとおり `g++` を入れることです。そうしないなら、`mosac build` と `mosac run` のたびに `--runtime-source` を付けます。すると `mosac` は、パッケージに入っている源から `clang++` 用の実行時ライブラリを作ります。これには CMake 3.25 以上が要ります（Ubuntu では `sudo apt update && sudo apt install cmake`）。最初のビルドは 1 分以上かかります。ほかのビルドと同じく、うまくいけば何も出しません。作った実行時ライブラリは `.mosac-cache` に残るので、同じフォルダでの次からのビルドはまた速くなります。ただし `.mosac-cache` を消すと、作った実行時ライブラリも消えるので、次のビルドはまた 1 分以上かかります。`--clean` を付けたビルドは、初めに `.mosac-cache` を消すので、そのビルド自身がそれだけかかります。

<!-- skip: runtime-source -->
```console
$ mosac build こんにちは.pl -o こんにちは --runtime-source
$ ./こんにちは
こんにちは
```

プログラムをほかの人に渡すときは、`mosac dist` でまとめます。`mosac dist` は `mosac build` と同じようにプログラムを作り、同じオプションを受けて、`-o` で指したフォルダ（なければ `dist`）に書庫を 1 つ書きます。書庫は、macOS と Linux では `.tar.gz` のファイル、Windows では `.zip` のファイルです。名前はプログラムの名前、システム、プロセッサを並べたもので、`mosac` がそれを出します（次は Apple シリコンの Mac の場合です）。

<!-- skip: dist -->
```console
$ mosac dist こんにちは.pl
dist/こんにちは-macos-arm64.tar.gz
```

書庫にはフォルダが 1 つあり、その中に実行ファイル、動かし方を英語と日本語で書いた `README.txt`、それに `LICENSE` と `NOTICE` のファイルが入っています。実行ファイルは Mosa の実行時ライブラリを含み、そのライセンスである Apache License 2.0 が、この 2 つのファイルを添えることを求めているからです。書庫を受け取った人は、それを展開して実行ファイルを動かします。Mosa もコンパイラも要りませんが、システムとプロセッサは名前のとおりのものが要ります。書庫は圧縮していないので、大きさは実行ファイルとほぼ同じです。実行ファイルには署名がありません。macOS と Windows では、第 2 章で `mosa` について説明したやり方で動かすことになる場合があります。

## 練習問題

1. 1、2、3 の 2 乗を 1 行に 1 つずつ書き出すプログラム `二乗.pl` を、フォルダ `prolog` に書いてください。`forall/2`、`member/2`、`format/2`（第 12 章）を使います。実行ファイルを作って動かしてください。
2. ディレクティブを書かずに `main :- write('こんにちは 世界'), nl.` だけを、`ディレクティブなし.pl` という名前でフォルダ `prolog` に保存します。実行ファイルを作って動かしてください。何が出ますか。

### 答え

1. `forall/2` は、要素ごとに、かっこの中の 2 つのゴールを実行します。`is/2` が 2 乗を計算し、`format/2` がそれを書き出します。

<!-- file: squares-file 二乗.pl -->
```prolog
:- initialization(main, main).
:- use_module(library(lists)).
:- use_module(library(format)).

main :- forall(member(X, [1, 2, 3]), (Y is X * X, format('~w~n', [Y]))).
```

<!-- check: exercise-squares -->
```console
$ mosac build 二乗.pl -o 二乗
$ ./二乗
1
4
9
```

2. 何も出ません。実行ファイルはできますが、`main` を呼ぶものがないので、プログラムはすぐに終わります。

<!-- file: nodirective-file ディレクティブなし.pl -->
```prolog
main :- write('こんにちは 世界'), nl.
```

<!-- check: exercise-nodirective -->
```console
$ mosac build ディレクティブなし.pl -o ディレクティブなし
$ ./ディレクティブなし
```

---

[目次](README.md) ・ 前: [12. モジュールとライブラリ](12-modules-libraries.md) ・ 次: [14. リファレンス](14-reference.md)
