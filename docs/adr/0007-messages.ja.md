[English](0007-messages.md) | 日本語

状態: 承認済み（2026-10-28）

## 背景

SPEC 第7節は文面を `lib/messages_ja.pl` と `lib/messages_en.pl` に置き、旗 `message_language`（既定は `LANG` から）で選ぶと定める。今は `session.cpp` と `main_mosa.cpp` が日本語の文面を持つ。
`read_pred.cpp` は行、列、トークン入りの日本語の文を Formal に入れるので、言語で Formal が変わり、位置が二度書かれる。
SPEC 第4節は構文エラーの表示に問題のトークンを求め、第7節は Extra を `stream(S, Line, LinePos, CharNo)` に定め、7.12.2 j は Formal を `syntax_error(Atom)` とするので、トークンの置き場がまだ無い。

## 決定

モジュール `messages_ja` と `messages_en` がそれぞれ `message(Key, Format)` を持つ（Format は `format/2` の書式原子。今足す鍵は `goal_raised`、`heap_limit`、`help_japanese_variables`）。`render_message` は初めて使うときに `library(messages_<lang>)` を読み、`user` には取り込まず、同じマシンで catch の内側で `format/3` を動かす。
旗 `message_language` は `ja` か `en` を取り、他の原子は `domain_error(flag_value, message_language+V)`。既定は `LANG`（`src/platform/` で読む）が `ja` で始まれば `ja`、それ以外は `en`。カタログが無い、鍵が無い、表示が失敗したときは `Key: ` の後に各引数を `writeq` で空白区切りに出す。
`syntax_error(Key)` は理由ごとに英語の原子を 1 つ（`invalid_utf8`、`illegal_character`、`unterminated_quoted`、`unterminated_comment`、`bad_escape`、`integer_overflow`、`unexpected_token`、`max_arity`、`incomplete_term`）。Extra は `syntax(stream(S, Line, LinePos, CharNo), Token)` で、Token は問題のトークンの原子、入力の終わりでは `''`（持ち主が `stream/5` より A を選んだ）。

## 退けた案

言語ごとの文字列を C++ の表に持つ。SPEC 第7節が C++ に文面を置くことを禁じ、利用者が作り直さずに言語を足せない。
1 つのモジュール `messages` に `message(Lang, Key, Format)` を置く。毎回両方の言語を読み、3 つめの言語が共有のファイルを書き換える。
文を Formal に残す。`catch/3` の型が言語に左右され、列が二度書かれる。

## 結果

`goal_raised` は `~q` を使うので、表示する例外は今と同じく引用付きになる（変数は `writeq/1` と同じく `numbervars` で書く）。`main_mosa.cpp`、`session.cpp` の `warn_resource`、使い方の表示、読み込みの診断（マシンが読み込みの最中に作る）は問い合わせを動かせるマシンを持たないので、`render_only` の印を付けた新しいマシンで文面を作る。そのマシン自身の診断は代わりの表示にする。
CMake は `session_test` を `LANG=ja_JP.UTF-8` で流し、今ある日本語の確認を機械に左右させない。もう一度、`MOSA_LIB` を `tests/tmp/messages_empty/` にして流す。そこの `messages_en.pl`（構成時に書く）は鍵を持たない。`MOSA_LIB` は実行ファイルの `lib/` より先に探すので、その回が代わりの表示を確かめる。
`tests/pl/read.pl` の `read_syntax`、`read_incomplete`、`read_syntax_stream` は Task 12 のコミットで上の Key と Extra に変わる。SPEC 第7節は承認から、この Extra を書く。承認までコードを書かない。
