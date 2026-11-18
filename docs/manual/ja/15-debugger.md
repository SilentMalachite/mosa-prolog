[English](../en/15-debugger.md) | 日本語

# 15. デバッガ

この章では、Mosa がゴールを 1 歩ずつ実行するようすを見て、どの述語にいつ入り、いつ出るのかを確かめます。大きな探索の途中でも、気になる述語にすぐ飛べるようになります。

この章は、第 8 章より後ならいつ読んでもかまいません。事実、規則、リスト、再帰、それに `;` で出会ったバックトラックを使います。

## 手順

1. フォルダ `prolog` に移り、`mosa` を起動します。リストの長さを返す `len/2` を書いたファイル `len.pl` を作ります。第 7 章の `数える/2` と同じ形です。1 つ目の節は「空のリストの長さは 0」と言っています。2 つ目の節は、先に残りの長さを数えてから、1 を足します。

<!-- file: len-file len.pl -->
```prolog
len([], 0).
len([_|T], N) :- len(T, M), N is M + 1.
```

2. 読み込んで、`trace` のあとで `len([a], N)` を実行します。`trace` はデバッガを入れます。すると Mosa は、ゴールを呼ぶたびと、ゴールから出るたびに止まります。止まるたびに 1 行が出て、行の終わりで Mosa は Enter を待ちます。どの行でも Enter を押して、ゴールが過ぎていくのを見てください。最後に、いつもの答えが出ます。デバッガが入っているあいだ、プロンプトは `[trace] ?- ` です。`notrace.` と `nodebug.` で切ります。

<!-- check: creep
consult('len.pl').
trace, len([a], N).






notrace.
nodebug.
-->
```text
?- consult('len.pl').
true.
?- trace, len([a], N).
 呼出: (1) len([a],_G1) ? 
 呼出: (2) len([],_G1) ? 
 成功: (2) len([],0) ? 
 呼出: (2) _G1 is 0+1 ? 
 成功: (2) 1 is 0+1 ? 
 成功: (1) len([a],1) ? 
N = 1.
[trace] ?- notrace.
true.
[debug] ?- nodebug.
true.
```

   行の終わりが `?` になっているところで、Enter を押しています。空の入力は画面に出ません。

3. 上から読みます。` 呼出: (1) len([a],_G1) ? ` は、Mosa が `len([a], N)` を呼ぼうとしていて、開いている呼び出しの 1 つ目（`(1)`）だということです。`_G1` は、まだ値のない変数 `N` です。次の `(2) len([],_G1)` は、2 つ目の節の `len(T, M)` の呼び出しで、1 段深いところにあります。` 成功: (2) len([],0) ` は、この呼び出しが成功して、変数に 0 が入ったことを表します。足し算のあと、` 成功: (1) len([a],1) ` で最初の呼び出しが `N = 1` で終わります。

## 何が起きたか

プログラムが呼ぶゴールは、どれも 1 つの箱です。実行が箱の 4 つの辺のどれかを越えるか、エラーで箱の下から出るたびに、Mosa は 1 行を書きます。これらをポートと呼びます。

<!-- skip: ports-diagram -->
```text
            +-----------------------+
   呼出 --> |                       | --> 成功
            |         ゴール        |
   失敗 <-- |                       | <-- 再試行
            +-----------------------+
                       |
                       v
                     例外
```

- **呼出**（Call）: Mosa がゴールを始めます。
- **成功**（Exit）: ゴールが成功しました。まだ試していない選択肢が残っていることもあります。
- **再試行**（Redo）: ゴールのあとで何かが失敗したので、別の答えを探しにゴールへ戻ってきました。
- **失敗**（Fail）: ゴールにはもう答えがありません。
- **例外**（Exception）: ゴールが例外を起こし、例外が箱を通り抜けます。行には、投げられた項も書かれます。

かっこの中の数は深さです。この箱を含めて、今いくつの箱が開いているかを表します。ゴールは `writeq` と同じ書き方で、深さ 10 で切って書かれます（`w` なら切らずに書きます。後で出てきます）。変数は、その行の中で出てきた順に `_G1`、`_G2` と名前が付きます。次の行では、同じ変数に別の名前が付くことがあります。行をまたいで名前を比べないでください。

自分で書いた述語のゴールは見えます。`is/2` や `write/1` のような組み込み述語も見えますが、組み込みの中には入りません。見えるのは、`findall/3`、`forall/2`、`setup_call_cleanup/3` のように組み込みに渡して動かすゴールだけで、1 段深く見えます。ライブラリの述語が中で行う呼び出しも見えません。たとえば `append/3` は 1 つの箱で、中の再帰は隠れます。`lists:append(X, Y, Z)` と書いても同じです。ただし、`maplist/2` のようにライブラリの述語が呼ぶあなたのゴールは見えます。次のゴールは見えません。ライブラリの名前の後の制御構文の中のゴール（`lists:(append(X, Y, Z), member(A, Z))` のように）、ライブラリの名前を付けて、ライブラリの述語に呼ばせるゴール（`maplist(lists:append([a]), Ls, Rs)` のように）、ライブラリの名前を 2 つ重ねたゴール（`lists:lists:append(X, Y, Z)` のように）、ライブラリの名前の後に書いた組み込みに渡すゴール（`lists:findall(X, member(X, L), Xs)` や `lists:once(G)` のように。渡したゴールはライブラリの中で動くからです）です。制御構文（`,`、`;`、`->`、`*->`、`!`、`catch/3`）と、いくつかのゴール（`call/1` から `call/8`、`true`、`fail`、`false`、`throw/1`、`halt/0,1`、`M:Goal`、デバッガ自身の述語）には、ポートがありません。`\+` にはあり、否定するゴールを包む箱になります。

行は標準エラーに出るので、プログラムが書く出力とは混ざりません。

最初の実行で見えなかったポートを見ましょう。`ports.pl` というファイルに、事実を 2 つと、`throw/1` でエラーを起こす `bad/0` を書きます。

<!-- file: ports-file ports.pl -->
```prolog
c(1).
c(2).
bad :- throw(oops).
```

ない事実を尋ねると、ゴールはすぐ失敗します。

<!-- check: fail-port
consult('ports.pl').
trace, c(3).


notrace.
nodebug.
-->
```text
?- consult('ports.pl').
true.
?- trace, c(3).
 呼出: (1) c(3) ? 
 失敗: (1) c(3) ? 
false.
[trace] ?- notrace.
true.
[debug] ?- nodebug.
true.
```

`c(X)` のあとのゴールが失敗すると、Mosa は `c(X)` へバックトラックします。再試行と、次の答えが見えます。

<!-- check: redo-port
consult('ports.pl').
trace, (c(X), X > 1).








notrace.
nodebug.
-->
```text
?- consult('ports.pl').
true.
?- trace, (c(X), X > 1).
 呼出: (1) c(_G1) ? 
 成功: (1) c(1) ? 
 呼出: (1) 1>1 ? 
 失敗: (1) 1>1 ? 
 再試行: (1) c(_G1) ? 
 成功: (1) c(2) ? 
 呼出: (1) 2>1 ? 
 成功: (1) 2>1 ? 
X = 2.
[trace] ?- notrace.
true.
[debug] ?- nodebug.
true.
```

エラーは例外の行として見え、そのあと呼び出し側へ渡されます。ここでは `catch/3` が捕まえるので、問い合わせは答えまで進みます。

<!-- check: exception-port
consult('ports.pl').
trace, catch(bad, E, true).


notrace.
nodebug.
-->
```text
?- consult('ports.pl').
true.
?- trace, catch(bad, E, true).
 呼出: (1) bad ? 
 例外: (1) bad oops ? 
E = oops.
[trace] ?- notrace.
true.
[debug] ?- nodebug.
true.
```

## 別の答えを取りに戻る

4. `;` は、問い合わせの次の答えを Mosa に頼みます。デバッガはそれを内側から見せます。答えは成功の行のあとに出て、`;` は再試行になります。

<!-- check: retry-more
consult('ports.pl').
trace, c(X).


;


notrace.
nodebug.
-->
```text
?- consult('ports.pl').
true.
?- trace, c(X).
 呼出: (1) c(_G1) ? 
 成功: (1) c(1) ? 
X = 1;

 再試行: (1) c(_G1) ? 
 成功: (1) c(2) ? 
X = 2.
[trace] ?- notrace.
true.
[debug] ?- nodebug.
true.
```

   最初の答え `X = 1` は、成功の行のあとに出ます。`;` を打ったので、次に止まるのは再試行の行で、そのあと `c(2)` の成功です。選択肢はもう残っていないので、Mosa はピリオドで答えを終えます。

## 大きな探索に飛び込む

`trace` はすべてのポートを見せ、既定ではそのたびに止まるので、プログラムが大きくなると多すぎます。スパイポイントを置くと、気になる述語でだけ止まります。`spy(名前/引数の数)` は述語にスパイポイントを置き、置いたときはデバッガも入れます。スパイポイントのあいだは、Mosa は止まらずに走ります。これを leap（跳ぶ）と呼びます。

5. ファイル `route.pl` を作ります。`path/3` は、`edge/2` の道をたどって、ある町から別の町への道を見つけます。

<!-- file: route-file route.pl -->
```prolog
edge(a, b).
edge(a, c).
edge(b, d).
edge(c, d).
edge(d, e).

path(X, X, [X]).
path(X, Z, [X|P]) :- edge(X, Y), path(Y, Z, P).
```

6. `edge/2` にスパイポイントを置いて、`a` から `e` への道を尋ねます。止まるたびに、次のスパイポイントまで跳ぶコマンド `l` を打ちます。見えるのは `edge/2` のポートだけです。デバッガは入っていても、あらゆる所で止まるわけではないので、プロンプトは `[debug] ?- ` になっています。最後の答えで Enter を押して問い合わせを終え、スパイポイントを外してデバッガを切ります。

<!-- check: leap
consult('route.pl').
spy(edge/2).
path(a, e, P).
l
l
l
l
l
l

nospy(edge/2).
nodebug.
-->
```text
?- consult('route.pl').
true.
?- spy(edge/2).
% edge/2 にスパイポイントを置きました
true.
[debug] ?- path(a, e, P).
 呼出: (1) edge(a,_G1) ? l
 成功: (1) edge(a,b) ? l
 呼出: (1) edge(b,_G1) ? l
 成功: (1) edge(b,d) ? l
 呼出: (1) edge(d,_G1) ? l
 成功: (1) edge(d,e) ? l
P = [a,b,d,e]
.
[debug] ?- nospy(edge/2).
% edge/2 のスパイポイントを外しました
true.
[debug] ?- nodebug.
true.
```

   `path/3` には箱を開かなかったので、ここでは深さがいつも `(1)` です。leap のあいだは、スパイポイントのある述語の呼び出しにだけ箱を開くので、跳んでいるあいだの呼び出しは、深さにも `g` の一覧にも入りません。前に creep で開いた箱は、そのまま数えます。`l` の代わりに Enter を押すと、デバッガは creep（あらゆる所で止まる）に戻り、プロンプトは `[trace]` になります。

## 止まったときに打てるもの

止まるポートで、Mosa は 1 行を読みます。空白でない最初の文字で、することが決まります。コマンドは次のとおりです。

| 打つもの | 働き |
| --- | --- |
| Enter、`c` | creep: 次のポートへ進み、そこで止まります。 |
| `s` | skip: 呼出か再試行のポートでは、この箱の次のポートまで止まらずに実行します。ほかのポートでは creep と同じです。 |
| `l` | leap: スパイポイントのある述語のポートまで走ります。 |
| `f` | fail: このゴールを失敗させ、失敗のポートを見せます。例外のポートでは、失敗のポートを出さずに失敗させます。 |
| `r` | retry: このゴールの呼出のポートに戻り、束縛を元に戻します。出力のような副作用は戻りません。呼出のポートそのものでは creep と同じです。 |
| `a` | abort: 問い合わせを止めます。その中に残った `setup_call_cleanup/3` の後始末は、先にポートを出さずに動きます。例外のときと同じく、後始末には Setup の束縛は見え、Goal の束縛は見えません。デバッガのモードはそのままなので、プロンプトはモードに合わせて `[trace] ?- `、`[debug] ?- `、`?- ` のどれかになります。 |
| `n` | nodebug: デバッガを切って続けます。 |
| `g` | 開いている箱を、内側から深さ付きで書き、もう一度尋ねます。 |
| `w` | ゴールを深さを切らずに書き（例外のポートでは投げられた項も）、もう一度尋ねます。 |
| `+`、`-` | このゴールの述語にスパイポイントを置きます（`-` は外します）。もう一度尋ねます。 |
| `h`、`?` | この一覧を書き、もう一度尋ねます。 |

ほかの入力には「知らないコマンドです」と、`h` を勧める知らせが返ります。入力の終わり（端末なら Ctrl-D）では、`n` と同じになります。

7. `g` と `s` を試します。`g` は開いている箱を書きます。呼出のポートでの `s` は、中で何が起きるかを見せずにゴールを実行するので、次に止まるのはその成功です。

<!-- check: cmd-goals
consult('len.pl').
trace, len([a], N).

g
s
l
notrace.
nodebug.
-->
```text
?- consult('len.pl').
true.
?- trace, len([a], N).
 呼出: (1) len([a],_G1) ? 
 呼出: (2) len([],_G1) ? g
 (2) len([],_G1)
 (1) len([a],_G1)
 呼出: (2) len([],_G1) ? s
 成功: (2) len([],0) ? l
N = 1.
[debug] ?- notrace.
true.
[debug] ?- nodebug.
true.
```

8. `f` はゴールをわざと失敗させます。`r` は呼出のポートに戻るので、1 歩をもう一度見られます。

<!-- check: cmd-fail
consult('len.pl').
trace, len([], N).
f

notrace.
nodebug.
-->
```text
?- consult('len.pl').
true.
?- trace, len([], N).
 呼出: (1) len([],_G1) ? f
 失敗: (1) len([],_G1) ? 
false.
[trace] ?- notrace.
true.
[debug] ?- nodebug.
true.
```

<!-- check: cmd-retry
consult('len.pl').
trace, len([], N).

r


notrace.
nodebug.
-->
```text
?- consult('len.pl').
true.
?- trace, len([], N).
 呼出: (1) len([],_G1) ? 
 成功: (1) len([],0) ? r
 呼出: (1) len([],_G1) ? 
 成功: (1) len([],0) ? 
N = 0.
[trace] ?- notrace.
true.
[debug] ?- nodebug.
true.
```

9. `w` はゴール全体を書きます。`a` は問い合わせ全体を止め、「実行を中断しました」と知らせます。

<!-- check: cmd-write
consult('len.pl').
trace, len([a], N).

w
a
nodebug.
-->
```text
?- consult('len.pl').
true.
?- trace, len([a], N).
 呼出: (1) len([a],_G1) ? 
 呼出: (2) len([],_G1) ? w
len([],_G1)
 呼出: (2) len([],_G1) ? a
% 実行を中断しました
[trace] ?- nodebug.
true.
```

## 止まるポートを選ぶ、状態を見る

`leash(Ports)` は、止まるポートを決めます。`leash([none, fail])` は失敗のポートでだけ止まり、`leash(all)` は 5 つのポートに戻します。止まらないポートも、行は書かれます。待たないだけです。

<!-- check: leash
consult('ports.pl').
leash([none, fail]).
trace, (c(X), X > 1).

leash(all).
notrace.
nodebug.
-->
```text
?- consult('ports.pl').
true.
?- leash([none, fail]).
true.
?- trace, (c(X), X > 1).
 呼出: (1) c(_G1)
 成功: (1) c(1)
 呼出: (1) 1>1
 失敗: (1) 1>1 ? 
 再試行: (1) c(_G1)
 成功: (1) c(2)
 呼出: (1) 2>1
 成功: (1) 2>1
X = 2.
[trace] ?- leash(all).
true.
[trace] ?- notrace.
true.
[debug] ?- nodebug.
true.
```

`debugging/0` は、デバッガが入っているか、どう進むか、スパイポイント、止まるポートを書きます。`nospy(Spec)` はスパイポイントを 1 つ外し、`nospyall/0` はすべて外します。

<!-- check: debugging
consult('len.pl').
spy(len/2).
debugging.
nospy(len/2).
debugging.
nodebug.
-->
```text
?- consult('len.pl').
true.
?- spy(len/2).
% len/2 にスパイポイントを置きました
true.
[debug] ?- debugging.
% デバッグ: 入り（leap）
% スパイポイント: len/2
% 止まるポート: call exit redo fail exception
true.
[debug] ?- nospy(len/2).
% len/2 のスパイポイントを外しました
true.
[debug] ?- debugging.
% デバッグ: 入り（leap）
% スパイポイント: なし
% 止まるポート: call exit redo fail exception
true.
[debug] ?- nodebug.
true.
```

## 知っておくこと

- 入力が端末でない `mosa` は、コマンドを読みません。ポートの行を書いて先へ進むので、実行の記録が取れます。`mosa len.pl -g 'trace, len([a], N)' -t halt < /dev/null 2> trace.log` のようにします。入力が端末だと、最初のポートで待ちます。
- Windows では、コマンドプロンプト（cmd.exe）で、パイプを使った次の行を実行してください。Mosa は `NUL` デバイスを端末と見なすので、`< NUL` のリダイレクトでは最初のポートで止まってしまいます。`type nul | mosa len.pl -g "trace, len([a], N)" -t halt 2> trace.log` のようにします。
- デバッガの述語 `trace/0`、`notrace/0`、`debug/0`、`nodebug/0`、`spy/1`、`nospy/1`、`nospyall/0`、`leash/1`、`debugging/0` は Mosa のものなので、自分のファイルでは同じ名前と引数の数の述語を定義できません。
- `mosac build` で作ったプログラムには、ポートは出ません。第 13 章を見てください。
- ほかの制限は、第 14 章の「既知の制限」にあります。

## 練習問題

1. `trace` のあとで `len([a, b], N)` を実行し、どのポートでも Enter を押します。呼出の行はいくつあり、深さの最大はいくつですか。
2. `len/2` にスパイポイントを置いて `len([a, b], N)` を実行し、止まるたびに `l` を打ちます。どのポートが見えますか。

### 答え

1. `len([a, b], N)` は `len/2` を 3 回、`is/2` を 2 回呼ぶので、呼出の行は 5 つです。深さの最大は 3 です。3 つ目の `len/2` は 2 つ目の中に、2 つ目は 1 つ目の中にあります。

<!-- check: exercise-trace
consult('len.pl').
trace, len([a,b], N).










notrace.
nodebug.
-->
```text
?- consult('len.pl').
true.
?- trace, len([a,b], N).
 呼出: (1) len([a,b],_G1) ? 
 呼出: (2) len([b],_G1) ? 
 呼出: (3) len([],_G1) ? 
 成功: (3) len([],0) ? 
 呼出: (3) _G1 is 0+1 ? 
 成功: (3) 1 is 0+1 ? 
 成功: (2) len([b],1) ? 
 呼出: (2) _G1 is 1+1 ? 
 成功: (2) 2 is 1+1 ? 
 成功: (1) len([a,b],2) ? 
N = 2.
[trace] ?- notrace.
true.
[debug] ?- nodebug.
true.
```

2. `len/2` のポートだけです。呼出が 3 行、成功が 3 行です。`is/2` にはスパイポイントがないので、跳んで通り過ぎます。

<!-- check: exercise-spy
consult('len.pl').
spy(len/2).
len([a,b], N).
l
l
l
l
l
l
nospyall.
nodebug.
-->
```text
?- consult('len.pl').
true.
?- spy(len/2).
% len/2 にスパイポイントを置きました
true.
[debug] ?- len([a,b], N).
 呼出: (1) len([a,b],_G1) ? l
 呼出: (2) len([b],_G1) ? l
 呼出: (3) len([],_G1) ? l
 成功: (3) len([],0) ? l
 成功: (2) len([b],1) ? l
 成功: (1) len([a,b],2) ? l
N = 2.
[debug] ?- nospyall.
true.
[debug] ?- nodebug.
true.
```

---

[目次](README.md) ・ 前: [14. リファレンス](14-reference.md)
