# フェーズ3 出口 設計

日付: 2026-10-26
状態: 承認済み（2026-10-26）

## 意図

フェーズ3の出口は、同梱した Logtalk の Prolog 準拠スイートを Mosa で測り、仕様第9節の完了条件を機械的に満たすことである。成功は、分母にしたテストの合格率が 90% 以上になり、`mosatest --suite` が `docs/conformance.md` を書き、そのあと版が `mosa 0.0.1 (phase 3)` になることである。

読む人は、このあとランナーと不合格の修正を実装するエージェントである。スイートのファイルは改変しない。Logtalk の処理系は作らない。

## 規模

設計はこの文書 1 枚である。実装は一つのフェーズの中で、ランナー、旗 `invalid_utf8`、不合格の修正、版と CI の順にセッションを分ける。90% に届くまでの修正は、報告書が指した述語を 1 つずつ直す。届く前に版も CI の合格率ゲートも変えない。

## 目標

- `mosatest --suite tests/conformance` が `tests/conformance/logtalk/prolog` の `tests.lgt` を読む
- 下の「分母」に入れたテストだけを実行し、合格数 ÷ 分母が 0.9 以上
- `docs/conformance.md` をコマンドが上書きする。手で数値を書かない
- 決まっている旗 `invalid_utf8` を、測定の前に登録する。既定は `error`。`replace` のときは U+FFFD を入れる
- 合格率が 0.9 以上で、既存の CTest が通ったコミットで、版を `mosa 0.0.1 (phase 3)` にし、CI がこのコマンドの終了コードを見る

## 非目標

- Logtalk のオブジェクト、メッセージ、`lgtunit` 本体、`logtalk_load`
- 段階 F のモジュールと、段階 G の `library(*)`
- 多倍長整数、UTF-8 以外のエンコーディング、`encoding/1`
- スイートのファイルを編集すること、期待値を実装に合わせること
- `mosac`、差分テスト、GC
- `include/mosa/mosa.hpp` に関数を足すこと
- 合格率 90% より前に版を `phase 3` にすること
- `docs/superpowers/plans/2026-10-25-phase3-builtins.md` を書き換えること。スイートの形式は、この設計がタスク 13 を上書きする

## 失敗になるもの

- 分母を広げるために段階 F 以降の述語を実装する
- 不合格を消すためにスイートを改変する、テストを飛ばす、期待値を書き換える
- 別欄のテストを分母に入れて 90% を計算する
- `halt/1` で `mosatest` のプロセスを終了させる
- 一時ファイルを `tests/conformance/` の中に書く
- ハーネス用の述語を利用者の述語表に残し、`current_predicate/1` の解を増やす
- 規格の解釈が一つに決まらない不合格を、推測で実装して通す

## 制約

- 言語は C++20。依存は標準ライブラリと CMake だけ
- 警告はエラー扱い。1 関数 60 行以内、1 ファイル 800 行以内
- 識別子は英語。コメント、コミット、文書は日本語
- Prolog の失敗と `throw/1` は機械の状態で表す。C++ の例外は `ResourceLimit` だけ
- エラーの比較は Formal だけ。Context は比べない
- コミットメッセージは `[phase3] ` で始まる日本語 1 行
- 各セッションの終わりに `cmake --build build && ctest --test-dir build` を通す。`docs/STATUS.md` は 15 行以内
- Cell と IR は ADR 0001、0002、0003 のまま
- 同梱スイートのコミットは `4decb64b598feff542a926a085eef01ec51f5a72` のまま。ファイルは改変しない

## アプローチ

出口の定義と分母は会話で決まっている。選んだのは、`.lgt` の実行の仕方である。

**1. `mosatest` が `tests.lgt` を直接読み、判定もランナーが行う（採用）**

オブジェクトの枠は無視する。`{ゴール}` は中の Prolog として今の機械で実行する。`^^` の入出力は、一時ファイルと今のストリームで同じ観測を作る。`true` や `error` などの結果の形は、Logtalk 3 の `lgtunit`（同じコミット）の意味でランナーが判定する。Logtalk 自体はロードしない。

採用の理由は、判定の語彙が既存の `test/3` より広いことと、スイートのファイルも機械もそのまま使えることである。

**2. メモリ上で `test/3` にほどいて、今のランナーに渡す（不採用）**

実行の道は再利用できる。今の期待は `true`、`fail`、`error(Formal)`、`solutions` の 4 種だけで、`variant`、`errors`、`=~=` をそこへ潰すと意味が落ちる。

**3. Logtalk を載せて `tester.lgt` をそのまま起動する（不採用）**

作者のランナーそのものになる。オブジェクトと `lgtunit` はフェーズ3の外である。

## 決めたこと

会話で決めたことである。

- 90% の分母は、仕様第7節の段階 A〜E の述語・構文・指示のテストである
- 段階 F、段階 G、多倍長、UTF-8 以外のエンコーディング、行頭が `-` のテストは、報告書の別欄に書く。分母に入れない。実行しない
- ファイルは触らない
- 不合格のうち、規格の解釈が一つに決まらないものだけ `docs/iso-notes.md` に「A か B か」で 1 つ残す。決まるものは実装を直す
- ランナーはアプローチ 1 である
- 測定の前に旗 `invalid_utf8` を登録する

## 分母

`tests.lgt` の親ディレクトリで決める。実行するディレクトリは次である。

- `control/` の全部。`soft_cut_2_3` を含む。`*->/2` は段階 A にある
- `directives/` のうち `conditional_compilation` 以外
- `functions/` のうち `acosh_1`、`asinh_1`、`atanh_1`、`cosh_1`、`sinh_1`、`tanh_1`、`log10_1`、`lsb_1`、`popcount_1` 以外。`integer_power_2` は `(^)/2` なので入れる。ビット演算のフォルダは段階 B の `/\` `\/` `xor` `<<` `>>` `\` である
- `logical_update_semantics/`。`assert` と `retract` の規定である
- `predicates/` のうち、段階 A〜E の表にあるもの。表は下に書く
- `syntax/` の全部。ただし `syntax/directives/meta_predicate_1` は外す
- `unicode/builtins/`、`unicode/case_variables/`、`unicode/escape_sequences/`

実行しないディレクトリは次である。

- `unbounded/`
- `unicode/encodings/`、`unicode/directives/`
- `directives/conditional_compilation/`
- `syntax/directives/meta_predicate_1/`
- `predicates/` のうち `acyclic_term_1`、`atomic_concat_3`、`atomic_list_concat_2`、`atomic_list_concat_3`、`between_3`、`call_nth_2`、`clause_references`、`evaluable_property_2`、`forall_2`、`format_2`、`format_3`、`ground_1`、`keysort_2`、`length_2`、`msort_2`、`not_unifiable_2`、`numbervars_3`、`plus_3`、`portray_1`、`predicate_property_2`、`setup_call_cleanup_3`、`sort_2`、`subsumes_term_2`、`succ_2`

`ground/1`、`sort/2`、`length/2` は規格にある。仕様第7節の段階 A〜E の表には無く、`keysort/2` は段階 G にある。分母は表に従う。

実行するディレクトリの中でも、次のテストは別欄である。実行しない。

- 行頭が `-` の `test`
- 選ばれなかった `:- if` の枝
- 選ばれた枝が `- test` であるもの
- `condition(...)` が失敗するもの。`current_logtalk_flag/2` は下の固定の答えで失敗するので、`max_depth` と `portrayed` のテストは別欄である。この二つのオプションはフェーズ3の `write_term` に足さない
- ゴールが、そのファイルで定義されていない段階 F または G の述語を呼ぶもの。ただしランナーが判定する `variant/2`、`subsumes_term/2`、`assertion/1`、`assertion/2`、`(=~=)/2` は、この除外にしない

別欄はフォルダとテスト名を書く。実行しないフォルダの件数は、`test(` の行を数えるだけでよい。

## ランナー

`mosatest --suite <ディレクトリ>` は `run_iso_suite` を呼ぶ。ディレクトリは `tests/conformance` である。その下の `logtalk/prolog` が無いときは終了コード 1 とし、合格率を作らない。既存の `mosatest <tests/pl>` と `mosatest --roundtrip` は残す。使い方の文に `--suite <ディレクトリ>` を 1 行足す。

`tests.lgt` を 1 ファイルずつ、新しい機械で、ソースの順に実行する。ファイルの途中で機械を作り直さない。`op/3` のように、前のテストの状態を次が使うためである。オブジェクトの `setup` は各テストの前、`cleanup` は成功でも失敗でも後に実行する。

`tester.lgt` は Logtalk として実行しない。同じディレクトリの `tester.lgt` が `'$lgt_load_prolog_file'(File)` で名前を作っているときだけ、その Prolog ファイルを `tests.lgt` より先に `consult` する。`logtalk_load` は無視する。相対パスは、その `tester.lgt` のあるディレクトリから解く。

読み方は次のとおりである。

- `:- object`、`:- end_object`、`:- info`、`:- set_logtalk_flag` は無視する
- オブジェクトの外と中にある、テスト以外の節は、その機械の利用者述語として一度だけ載せる
- `:- dynamic` と `user::名前` は、モジュール修飾を外した利用者述語として載せる
- `:- uses(lgtunit, [op(700, xfx, =~=), (=~=)/2])` は、その機械の演算子表に `=~=` を足す。述語 `(=~=)/2` は利用者の述語表に入れない
- `:- uses(user, ...)` は、名前を利用者述語として呼ぶ印である
- `{ゴール}` は、テスト本体のどこに現れても、中のゴールにほどく
- `^^名前(...)` は、下のヘルパを呼ぶ。利用者の述語にはしない
- 読めない実行対象のファイルは、そのファイルを不合格 1 件として数える。別欄のファイルが読めなくても不合格にしない

`halt/0` と `halt/1` はプロセスを終えず、機械の停止の印と終了コードをそのテストの結果にする。次のテストの前に印を戻す。

一時ファイルは `tests/tmp/suite/` だけに作る。ファイルの実行が終わったら消す。`tests/conformance/` には書かない。

## 判定

意味は Logtalk 3 の `lgtunit.lgt`（コミット `4decb64`）の `convert_test_outcome` と NOTES の Test dialects に合わせる。

| 結果の形 | 成功 |
| --- | --- |
| `true` | ゴールが一度成功する。やり直しは見ない |
| `true(Assertion)` | 最初の解で `Assertion` が成功する。失敗したら次の解は試さない |
| `deterministic` | 最初の成功のあと、やり直しが失敗する |
| `deterministic(Assertion)` | 上に加え、最初の解で `Assertion` が成功する |
| `false` と `fail` | ゴールが失敗する |
| `error(Formal)` | 投げられた項が `error(Actual, _)` で、`Formal` が `Actual` を包摂する |
| `errors(List)` | リストのどれかの Formal が、同じ包摂で合う |
| `ball(Term)` | 投げられた項全体を `Term` が包摂する |
| `variant(A, B)` | ゴールが成功し、`A` と `B` が変数の名前替えで一致する |
| `subsumes(Expected, Result)` | ゴールが成功し、`Expected` が `Result` を包摂する |
| `exists(Assertion)` | どれかの解で `Assertion` が成功する |
| `all(Assertion)` | すべての解で `Assertion` が成功する。解がゼロでも成功する |

`deterministic` は、選択点が残っていても、やり直しがすぐに失敗すれば成功とする。機械は選択点の有無を述語として出していない。

`variant` と包摂と `(=~=)/2` はランナーが項の上で判定する。`numbervars/3` も `subsumes_term/2` も組込述語として足さない。`(=~=)/2` は `abs(A-B) =< max(abs(A), abs(B)) * epsilon` である。`epsilon` は評価関数 `epsilon` の値である。

`variant/2`、`subsumes_term/2`、`(=~=)/2`、`assertion/1`、`assertion/2` は、結果の形だけでなく、テスト本体と `^^assertion(...)` の中でもランナーが判定する。選言の中でも同じである。利用者の述語表にも組込述語にも入れない。`assertion(Goal)` と `assertion(説明, Goal)` は、`Goal` が成功すれば成功し、失敗すればそのテストを不合格にする。説明は報告書に書く。

## ヘルパ

`^^` は lgtunit の入出力ヘルパのうち、実行対象の `tests.lgt` が実際に呼ぶものだけを実装する。呼ばれないヘルパは作らない。

観測は lgtunit と同じである。`set_text_output/1` は標準出力を一時ファイルへ切り替え、`text_output_assertion(Expected, Assertion)` はその中身を読んで `Assertion` を `Expected == Contents` に束縛する。別名つきの `/2` と `/3` も同じで、別名はその機械のストリーム別名である。テキスト入力、バイナリ入出力、`file_path/2`、`create_text_file/2`、`create_binary_file/2`、`clean_*`、`closed_*_stream`、`stream_position` も、実行対象が呼ぶなら同じ観測にする。

`file_path(Name, Path)` の `Path` は `tests/tmp/suite/` の下である。同梱ディレクトリには書かない。テストが `consult` や `include` で隣の `.pl` を開くときは、同梱ディレクトリのまま開く。

改行は、どの OS でも U+000A 1 個である。

## `:- if`

`:- if` / `:- elif` / `:- else` / `:- endif` は、Logtalk を起動せずに枝を選ぶ。選ばなかった枝は実行しない。ファイルの残りのテストは実行する。条件が失敗しても、ファイル全体を不合格 1 件にはしない。`condition(...)` も同じ答えを使う。条件が失敗したテストは別欄である。

固定の答えは次である。

- `current_logtalk_flag(coinduction, supported)` は失敗する。循環項の枝は `- test` になり、別欄に入る。連言は左から評価し、ここで失敗したら右は実行しない。`clause/2` の `occurs_check` の枝は、このために選ばれない。旗 `occurs_check` は足さない
- `current_logtalk_flag(prolog_dialect, 名前)` は、どの名前でも失敗する。Mosa はその方言ではない。`op/3` が中置と後置の両方を許す方言の枝は選ばない。Mosa はその組を `permission_error` で拒む
- `os::operating_system_type(windows)` は失敗する。改行はどの OS でも U+000A であり、期待が U+000A の枝を選ぶ
- `predicate_property(ゴール, built_in)` は、その述語が組込として登録されているときだけ成功する。それ以外の `predicate_property` は失敗する
- `current_op(...)` と、`catch/3` を含むそれ以外の Prolog のゴールは、その時点の機械で実行する

`catch(1^true, _, fail)` は機械で実行する。`(^)/2` は単独の述語にしない。ISO が定めているのは `bagof/3` と `setof/3` の引数の先頭に並ぶ量化だけであり、ゴールの中に埋まった `^` は量化ではない。したがってこの条件は失敗し、`bagof` と `setof` の else の枝が分母に入る。その期待は、`unknown` が `fail` のとき `S == [3]` である。if の枝は実行しない。

上のどれでもない Logtalk のゴールが条件に現れたときは、そのテストを不合格 1 件とする。推測で真にしない。

## 報告書

`docs/conformance.md` を上書きする。先頭は「このファイルは自動生成である。手で編集しない。」である。

本文に、分母、合格数、合格率、不合格の一覧、別欄の一覧を書く。不合格の 1 行は、相対パス、テスト名、期待した形、実際の結果である。実際のエラーは Formal だけを書く。

合格率の表示は百分率で、小数第 1 位に四捨五入する。終了コードの 90% は、合格数 ÷ 分母が 0.9 以上かどうかで決める。表示が 90.0 でも、割合が 0.9 未満なら終了コードは 1 である。分母が 0 のときは終了コード 1 とする。

C++ のテストは、同梱スイートではない小さな `tests.lgt` を `tests/tmp/` に作り、`run_iso_suite` の合格数と総数がその実行と一致することを見る。同梱スイートの合格率を期待値に埋め込まない。

## 90% と版

ランナーが報告書を書けるようになってから、不合格を直す。直す順序は報告書の述語ごとである。1 セッションで 1 述語を超えない。

解釈が一つに決まらない不合格は、スイートを弱めずに `docs/iso-notes.md` へ条番号と「A か B か」を 1 つ書く。回答まで、その述語の該当箇所は推測で埋めない。その不合格は残る。残ったままで割合が 0.9 未満なら、版は変えない。

割合が 0.9 以上になり、往復、`nrev`、`queens`、段階 A、算術、フェーズ3の `tests/pl/` を含む CTest が通ってから、`src/version.cpp` と `CMakeLists.txt` の版の正規表現を `mosa 0.0.1 (phase 3)` にする。それより前のコミットでは版テストを壊さない。

同じコミットで、CI の Job Summary を `conformance: n/a (phase 0)` から、このコマンドが書いた合格率の 1 行に変える。CI は `mosatest --suite tests/conformance` を実行し、終了コードが 0 のときだけ成功する。スイートが同梱されていないジョブは失敗させる。このゲートより前のコミットは、CI を変えない。

## ビルダーに任せる

- 800 行を超える前のファイルの分割
- ヘルパのうち、実行対象が呼ばないものの省略
- 報告書の列の空白と見出しの字面。上に書いた項目は欠かさない
- 不合格を直すときの、述語の中の修正順
- `run_iso_suite` の引数の並び。ディレクトリと報告書のパスを受け、終了コードを返す
