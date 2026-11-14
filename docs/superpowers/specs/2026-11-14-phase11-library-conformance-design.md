# フェーズ11 準拠率の範囲の拡大と format の指示 設計

日付: 2026-11-14
状態: 承認済み（2026-11-14、所有者。フェーズの選択は同じ日の決定 A、作り方は決定 3。問い 1 と問い 2 は既定 A のまま）

## 意図

フェーズ 0〜10 は閉じ、0.2.1 を公開した。SPEC 第9節に次のフェーズは無い。所有者は 2026-11-14 に、次のフェーズとして「準拠率の分母の穴を埋める」（A）を選んだ（もう 1 つの候補は `term_expansion/2`・`goal_expansion/2` と条件付きコンパイル）。そのうえで作り方として、「測り直し、既存の誤りの修正、`format/2,3` の指示をそろえる」（3）を選んだ。

準拠スイートの範囲は、段階 A〜E のディレクトリだけである（`src/machine/lgt_suite.hpp` の `kPredicateScope`）。2026-11-14 に、範囲の外のディレクトリをスクラッチへ写して測った（付録 A）。その結果、次の 2 つが分かった。

- ISO の組み込み述語のディレクトリのうち 4 つ（`sort_2`、`keysort_2`、`ground_1`、`not_unifiable_2`。最後の 1 つの中身は `\=/2`）は分母の外にある。この 4 つの 52 件はすべて合格する。
- mosa が実装している ISO 外の述語のディレクトリは 566 件ある。合格は 369 件（65.2%）である。不合格の大半は `format/2,3` の 189 件で、そのうち 183 件は SPEC 第5節の表に無い指示（`~+` `~r` `~R` `~E` `~g` `~G` `~D` `~p` `~i` `~W` `~k` `~N`）によるものである。

このフェーズでは、ISO の 4 つを本欄の分母に入れる。実装済みのライブラリ述語は、ISO の率と混ぜずに別の欄（ライブラリ欄）で測る。そのうえで、ライブラリ欄の不合格を直す。中心は `format/2,3` の指示をスイートが試す範囲までそろえることである。

読む人は、このあと実装計画を書き、実装するエージェントである。所有者がこの文書を承認したあとに、SPEC 第9節の 11 行目と実装計画を書く。承認までは計画も実装もしない。

## 目標

- 本欄（ISO）の分母に `sort_2`、`keysort_2`、`ground_1`、`not_unifiable_2` が入る。
- ライブラリ欄が `docs/conformance.md` に入り、独自の分母、合格数、合格率、不合格の一覧を持つ。ライブラリ欄の率にも後退の判定がかかる。
- ライブラリ欄の `format_2`・`format_3` に、`domain_error(format_directive, _)` の不合格が 1 件も残らない。
- `numbervars/3`、`length/2`、`setup_call_cleanup/3` の不合格を直す。直さないものは、理由を iso-notes に書く。
- `mosatest --diff` がライブラリ欄のディレクトリも流し、インタプリタと `mosac` の結果が同じになる（差分 0）。
- `~+` の桁揃えは、`~t~|` と同じく東アジア幅（W と F を幅 2）で数える。
- `~p` は `print/1` と同じく利用者の `portray/1` を呼ぶ（`print/1` は 2026-10-27 の iso-notes 8.14 の決定から呼んでいる）。スイートの `print_portray_01`〜`04` が通るように、試験器が `tester.lgt` に直接書かれた節を読む（今は `tester.lgt` が名指しするファイルだけを読む。`src/machine/lgt_io.cpp` の `lgt_load_tester`。節を持つ `tester.lgt` は `format_2` と `format_3` の 2 つだけ）。

## 非目標

- `portray/1` を `user` で `dynamic`・`multifile` と宣言すること。`portray_1` ディレクトリ（2 件。この 2 つの性質だけを見る）は範囲の外のままにする（私の判断。iso-notes 8.14 の「`portray/1` は組み込みにしない」を変えない）。
- 本欄の今のディレクトリを見直すこと。本欄は「ISO の率」と呼んでいるが、実際は SPEC 第7節の段階 A〜E の範囲で、13211-1 に無い `print/1,2`（`print_1`、`print_2`）と `findall/4`（`findall_4`）も段階 C・D の述語として入っている（計 33 件）。外すと率が動き、後退の判定の物差しを作り直すことになる。このフェーズでは触らない（私の判断）。
- ADR 0016 を変えること（問い 2 が B のときを除く）。
- mosa に無い述語を足すこと: `call_nth/2`、節の参照（`assert/2`、`clause/3`、`erase/1`）、`evaluable_property/2`、`atomic_concat/3`、条件付きコンパイル（`:- if/elif/else/endif`）、ISO 外の関数子（`acosh/1` ほか `kFunctionExcluded` の 9 つ）。どれも範囲の外のままにする。
- スイートが試していない `format` の指示（`~@`、`~I`、`~:`、`~c` の変種ほか）。
- `~t~|` の桁を、出力先の流れの今の桁から数えること。今は `format` の呼び出しの始まりを桁 0 とする。流れが数えているのは字の数で、東アジア幅ではないので、そのままでは使えない。今の振る舞いを残し、既知の制限として iso-notes に書く（私の判断）。
- `~D` の区切りの字を変えること（ロケール）。区切りは常に `,` にする。
- `format` を速くすること。速さの完了条件は置かない（`format` は `bench/` のプログラムに無い）。
- 準拠スイートの本欄の今の不合格 35 件の掘り下げ。
- このフェーズでの公開。公開は所有者の依頼で別に行う。

## アンチ目標（技術的に動いても、これをやったら失敗）

- 合格率を見て、範囲に入れるディレクトリを選ぶこと。範囲は下の「範囲の規則」だけで決める。`suite_in_scope` の注釈「一部だけを入れると準拠率を選べてしまう」と同じ考え方である。
- ライブラリ欄の結果を、本欄（ISO の率とバッジ）に混ぜること。
- スイートのファイルを書き換えること。件を手で別欄へ移すこと。
- `format` が誤りの途中まで書いた出力を残すこと。今は出力を全部組んでから一度に書く。これを崩さない。
- `~t~|` の東アジア幅の扱い（iso-notes の Section 4 format/2 fill characters by width）を壊すこと。
- インタプリタと `mosac` が訳したコードの結果が違うこと。

## 制約

- AGENTS.md のとおり。
  - C++20 標準ライブラリと CMake だけを使う。
  - 関数は 60 行以内、ファイルは 800 行以内にする。
  - 生の `new`/`delete`、`reinterpret_cast`、C 配列、マクロを使わない。
  - C++ のエラーは `throw_error`/`make_*_error` の族だけで投げる。`lib/format.pl` は今と同じく `error(Formal, context(PI, _))` を投げる。
  - 組み込み述語のファイルの見出しには、節番号（無ければ「13211-1 に条項は無い」）、引数のモード、投げうる誤り、テストのファイル名を書く。
- 自前のテストを先に `tests/pl/lib_format.pl` に書いてから実装する（TDD）。`format` は規格に無いので Examples は無い。スイートの件と同じ入力と期待値を、自前のテストにも書く。エラーのテストは `Formal` だけを比べる。
- 識別子は英語、コメントとコミットメッセージは日本語で書く。文書は英語が正で、`*.ja.md` を同じコミットで直す。
- スイートの結果を変えるコミットでは、同じコミットで `docs/conformance.md`、`docs/conformance.json`（ライブラリ欄の JSON も）、`docs/agreement.md`・`agreement.json` を作り直す。CI の macOS のジョブの「Check badges」は JSON の差分を見る（フェーズ10の規則と同じ）。
- 800 行の上限に近いファイルのうち、このフェーズで触るのは `tests/cpp/conformance_report_test.cpp`（778 行）である。書き足す前に分ける。`src/machine/internal.hpp`（800 行）と `src/builtins/write_pred.cpp`（799 行）には触らない。新しい内部述語は別のファイルに置く。

## 所有者への問い（iso-notes に「A か B か」で書く。確認までは既定で進める）

どちらも既定のまま進めても、このフェーズは閉じられる。

1. **`length/2` は ISO の組み込みか（SPEC 第5節とマニュアル第14章の食い違い）**
   - SPEC 第5節のライブラリ表は「`length/2`（ISO core なので system に置き、再エクスポートする）」と書いている。一方、マニュアル第14章の組み込み述語の表は、`length/2` の節番号を「—」（13211-1 に無い）としている。私の理解でも、13211-1 と Cor.1〜3 の組み込み述語に `length/2` は無い。リポジトリに本文が無いので確かめられない（AGENTS.md の「SPEC が規格と食い違うときは iso-notes に書いて問う」に当たる）。
   - A: マニュアルが正しい。SPEC 第5節の「ISO core」の文言を「13211-1 に無いが system に置く」に直す（日英）。置き場所（system）は変えない。`length_2` はライブラリ欄で数える。
   - B: SPEC が正しい。マニュアル第14章の表に節番号を書き（日英）、`length_2` を本欄で数える。
   - 既定は A（私の判断）。確認までは SPEC の文言に触れず、`length_2` は下の「範囲の規則」どおりライブラリ欄で数える（所有者が作り方 3 を選んだときに見た分け方と同じ）。B に決まれば本欄に移す。移すと本欄の率は 2291/2326 から 2314/2350 になり、分数としてわずかに下がるので、その回は後退の判定の物差しを作り直す。
2. **カットが走らせた後始末の束縛（ADR 0016）**
   - スイートの `lgt_setup_call_cleanup_3_26` は、`setup_call_cleanup(true, ndet(_), D = true)` のあとに `var(D)` を期待する。試験器はゴールを `(本体, !), (確かめ)` と組む（`src/machine/lgt_file.cpp` の `goal_text`）。このカットが後始末を走らせ、ADR 0016 では後始末の束縛は残るので、`D` は `true` になって落ちる。
   - A: ADR 0016 のまま。この 1 件は不合格のまま、理由を iso-notes に書く。
   - B: ADR 0016 を改め、カットが走らせた後始末の束縛を戻す。ADR の改訂と承認が先に要り、タスクが 1 つ増える。
   - 既定は A（私の判断）。後始末の意味を変えるのはこのフェーズの目的（測ることと `format`）の外であり、`tests/pl/utilities.pl` の `scc_*` の期待値にも関わる。試験器の側でカットの前に確かめる形に変える案は採らない。範囲内のすべての `true(Assertion)` の件の流れが変わり、本欄の結果も動きうる。

## 範囲の規則（私の判断）

- **本欄**: 今の範囲（SPEC 第7節の段階 A〜E のディレクトリ）に加えて、名前の述語にマニュアル第14章の表が 13211-1（Cor.1〜3 を含む）の節番号を付けているディレクトリ。このフェーズで足すのは `predicates/sort_2`（8.4.3）、`keysort_2`（8.4.4）、`ground_1`（8.3.10）、`not_unifiable_2`（中身は `\=/2`、8.2.3）の 4 つである。
- **ライブラリ欄**: `predicates/` の直下のディレクトリのうち、本欄に入らず、名前の述語を mosa が `system` か同梱のライブラリ（`lib/*.pl`）で提供しているもの。2026-11-14 の時点では `atomic_list_concat_2`、`atomic_list_concat_3`、`between_3`、`format_2`、`format_3`、`forall_2`（0 件）、`length_2`（問い 1 が B なら本欄）、`msort_2`、`numbervars_3`、`plus_3`、`predicate_property_2`、`setup_call_cleanup_3`、`succ_2` の 13 個である。
- 規則は一覧として `lgt_suite.hpp` に書き、一覧の各行に、その述語の置き場所（`system` かライブラリ名）を注釈で添える。
- 範囲の外のままのもの: 上の非目標に挙げたディレクトリ、`directives/conditional_compilation`、`unicode/encodings`、`unicode/directives`、`syntax/directives/meta_predicate_1`、`functions/` の 9 つ。
- 試験器が `user` に取り込む 6 つのライブラリ（`lgt_libs.cpp`）は変えない。ライブラリ欄の述語はこの取り込みで見える。

## 決めたこと

### 報告書と後退の判定

- `docs/conformance.md` の今の節（分母、合格数、合格率、不合格、別欄）は本欄のまま変えない。先頭の `合格率:` の行は本欄の率のままにする（CI の Job Summary が `grep -m 1 '^合格率:'` で読む）。
- その後ろに、ライブラリ欄の節を足す。見出しと、分母、合格数、合格率、不合格の並びを本欄と同じ形にする。ライブラリ欄の別欄の件は、今の「別欄」の一覧にまとめてよい（実装者の判断）。
- ライブラリ欄の率は、別の JSON（`docs/conformance-library.json`。shields.io の endpoint 形式、ラベルは `library conformance`）に書く。後退の判定（`conformance_badge_test` の「前回の JSON が物差し」）を、この JSON にも同じ形でかける。README には 2 つ目のバッジとして足す（日英）。
- CI の Job Summary に、ライブラリ欄の率の行を足す。「Check badges」の `git diff --exit-code` に新しい JSON を足す。
- SPEC 第8節の「数は 2 つ」の文を 3 つに直す（日英）。

### 差分テスト

- `mosatest --diff` は `lgt_suite_files`（`suite_in_scope`）でファイルを選ぶ。本欄とライブラリ欄の両方を流す。`agreement.md` の件数は増える。`mosac` が訳した側も `library(format)` を使うので、新しい指示はインタプリタと同じ結果になる必要がある。

### `format` の指示

参照は、同梱のスイート `format_2`・`format_3` の件である。件が決めない点は SWI-Prolog の `format/2` の文書に従う。スイートと SWI-Prolog が食い違う点はスイートに従い、iso-notes の Section 5 に書く（例: `~r` に桁の数が無いときは 8 進。スイートの `radix_default` が `16` に `20` を期待する）。

| 指示 | 引数 | 意味 | 置き場所（私の判断） |
| --- | --- | --- | --- |
| `~i` | 1 つ | 引数を読み飛ばす。桁の数は無視する | `lib/format.pl` |
| `~k` | 1 つ | `write_canonical/1` と同じ | `lib/format.pl` |
| `~W` | 2 つ（項と選択肢のリスト） | `write_term/2` と同じ。選択肢の誤りは `write_term/2` の誤り | `lib/format.pl` |
| `~p` | 1 つ | `print/1` と同じ。利用者の `portray/1` があれば呼ぶ | `lib/format.pl` |
| `~N` | 無し | 出力が行頭でなければ改行する。桁の数は無視する | `lib/format.pl` と内部述語 `'$stream_column'/2` |
| `~r` `~R` | 1 つの整数 | 桁の数を基数（2〜36、既定 8）として書く。`~R` は英字を大文字にする。多倍長の整数も書く | 内部述語 `'$format_radix'/4` |
| `~D` | 1 つの整数 | `~d` と同じ桁の数の意味で、整数部を 3 桁ごとに `,` で区切る | `lib/format.pl` |
| `~E` | 1 つの数 | `~e` の指数の字を `E` にする | `'$format_float'/4` を広げる |
| `~g` `~G` | 1 つの数 | C の `%g`、`%G` と同じ（`std::to_chars` の `chars_format::general`） | `'$format_float'/4` を広げる |
| `~+` | 無し | 前の桁止めから桁の数（既定 8）だけ先に桁止めを置く。埋め方は `~|` と同じ | `lib/format.pl` |

- 引数の誤りは、今の `~d` と同じ規則にする。数の引数は `is/2` と同じく評価し（`type_error(evaluable, F/N)` など）、整数が要る指示は `type_error(integer, X)` を投げる。`~*` の負の数は今と同じく `domain_error(not_less_than_zero, N)` にする。
- `~r` の基数が 2〜36 の外なら `domain_error(radix, N)` を投げる（私の判断。スイートは `domain_error(_, _)` ならよい）。
- `'$format_radix'/4` は、整数の文字列化に `BigInt` の基数つきの変換（フェーズ10）を使う。int64 の値はその速い道を通る。
- `~N` は、`format` の呼び出しの始まりの桁を `'$stream_column'/2` で読む。この述語は流れの `line_pos`（書き込みでも `advance_out` が進める。`src/builtins/write_pred.cpp`）から桁を返す。そのあとは、`format` が組んでいる出力の最後の字が改行かどうかで決める。`format/3` の出力先が `atom(A)` などの仮の流れなら、始まりは桁 0 である。
- `'$stream_column'/2` と `'$format_radix'/4` は新しいファイル（例: `src/builtins/format_int.cpp`）に置く。`write_pred.cpp` と `stream_prop.cpp` を大きくしない（実装者が置き場所を変えてよい。800 行を越えないこと）。
- `lib/format.pl` が 800 行を越えそうなら、桁止めの処理（`~t`、`~|`、`~+`）を別のファイルに分ける。分け方は実装者に任せる。

### 既存のライブラリ述語の不合格

付録 A の不合格 9 件（`format` の 189 件を除く）と、`format` の `print_portray` の 8 件を 1 件ずつ調べる。原因が mosa にあれば直す。試験器（Logtalk の `^^variant/2` などの補助を読めない）にあれば、試験器を直す。直せないものは iso-notes に理由を書く。試験器を直すときは、Logtalk の lgtunit が同じ件をどう流すかに合わせる。期待値を mosa に合わせて変える形にはしない。

- `numbervars_3` の 6 件: `commons_numbervars_3_08`〜`12`（`^^assertion` を含む本体）と `_15`（`numbervars(_, 1, a)` に `type_error(integer, a)` を期待。今は失敗する）。
- `length_2` の 1 件: `commons_length_2_06`（`deterministic` と `^^variant/2`）。
- `setup_call_cleanup_3` の 1 件: `lgt_setup_call_cleanup_3_26`。問い 2 を参照。既定の A なら直さない。
- `format_2`・`format_3` の `write_numbervars` 2 件: mosa の `format("~w", [T])` は `numbervars/3` の後の `a(_)` を `a(A)` と書くのに、スイートでは失敗する。原因は試験器の側にあると見ている（未確認）。
- `format_2`・`format_3` の `print_portray_01`〜`04` の 8 件: `portray/1` の節が `tester.lgt` に直接書かれていて、試験器が読まない。`~p` を足したうえで、試験器が `tester.lgt` の節と `dynamic`・`multifile` の指令を読むようにする。`initialization` の指令（`logtalk_load` を呼ぶ）は今と同じく読まない。

## 規模と順序

10 タスク。AGENTS.md の「1 セッション 1 タスク」に合わせて切る。範囲と報告書（タスク 3）を先に入れる。こうすれば、後のタスクの進みがライブラリ欄の率に出て、後退の判定がかかる。順は私の判断である。

1. フェーズを開く（文書だけ）: SPEC 第9節の 11 行目、第5節の `library(format)` の指示の一覧、第8節の数の文（日英）。iso-notes に問い 1 と問い 2、非目標の `portray_1` と `~|` の始まりの桁、`format` の参照の決め方。
2. `tests/cpp/conformance_report_test.cpp`（778 行）を、振る舞いを変えずに 2 つのファイルに分ける。
3. 範囲と報告書: 本欄に 4 つのディレクトリ（問い 1 が B なら `length_2` も）を足す。ライブラリ欄の一覧、報告書の節、JSON、後退の判定、CI の Job Summary と「Check badges」、README の 2 つ目のバッジ（日英）。`conformance.md`・JSON・`agreement` を作り直す。
4. 試験器の不足を調べて直す（`^^variant/2`、`numbervars_3` の `^^assertion`、`write_numbervars`、`tester.lgt` に直接書かれた節）。`print_portray` の 8 件は `~p` が入るタスク 6 で通る。
5. ライブラリ述語の誤りを直す（`numbervars/3`、タスク 4 で mosa の側と分かったもの）。問い 2 が B なら、ADR 0016 の改訂（承認待ちで止まる）と `setup_call_cleanup/3` の直しを別のタスクとしてここに足す。
6. `format` 1: `~i`、`~k`、`~W`、`~p`、`~N`（`'$stream_column'/2`）。
7. `format` 2: `~r`、`~R`、`~D`（`'$format_radix'/4`）。
8. `format` 3: `~E`、`~g`、`~G`（`'$format_float'/4` を広げる）。
9. `format` 4: `~+`。`tests/pl/japanese.pl` に全角の字を含む `~+` と `~t~|` のテストを足す。
10. 文書を閉じる: マニュアル第14章（と第12章の `library(format)` の記述があれば）、CHANGELOG、README、iso-notes、STATUS（日英）。CI の全ジョブが通ることを確かめ、フェーズを閉じるかを所有者に問う。

タスク 6〜9 は、互いに順を入れ替えてよい。どれもタスク 3 の後に置く。

## 完了条件（SPEC 第9節の 11 行目にする）

1. `mosatest --suite tests/conformance` の本欄の分母に `sort_2`、`keysort_2`、`ground_1`、`not_unifiable_2` が入り（問い 1 が B なら `length_2` も）、本欄の合格率が 98.4% を下回らない。
2. `docs/conformance.md` にライブラリ欄があり、範囲の規則のディレクトリがすべて入っている。`format_2`・`format_3` に `domain_error(format_directive, _)` の不合格が無い。ライブラリ欄の残りの不合格はすべて iso-notes に理由がある。ライブラリ欄の合格率が 98.0% 以上である。
3. `tests/pl/lib_format.pl` の新しい指示のテストと、`tests/pl/japanese.pl` が `mosa` で通り、`mosatest --diff` の差分が 0 である（ライブラリ欄のディレクトリと `japanese.pl` を含む）。
4. `mosatest --manual --all-tables` が通る。
5. CI の全ジョブが 3 OS で通る。

完了条件 2 の 98.0% の見込み: ライブラリ欄 566 件（問い 1 が A のとき）のうち、残る見込みの不合格は Logtalk の `::` を使う 4 件（`tab_table_left_aligned`・`right_aligned`。試験器では動かない）と、問い 2 が A のときの `lgt_setup_call_cleanup_3_26` の 1 件である。全部で 5 件なら 561/566（99.1%）になる。下限を 98.0% にしたのは、タスク 4 で直せない試験器の不足が見つかったときの余地である（約 10 件）。

## 実装者に任せること

- ライブラリ欄の節の見出しの字面と、別欄をまとめるか分けるか。
- `'$stream_column'/2` と `'$format_radix'/4` の置き場所と名前（内部述語の `$` の付け方は今の慣例に合わせる）。
- `~+` で前の区切りの文字が桁止めを越えたときの振る舞い。スイートの件が決めない点は `~|` と同じにする。
- 試験器の不足の直し方（タスク 4）。

## リスク

- 試験器の不足が思ったより多いと、タスク 4 が膨らむ。膨らんだら、タスク 4 を「調べて iso-notes に書く」と「直す」の 2 つに割る。
- `~g` の字面は、標準ライブラリの `std::to_chars` に依る。3 OS で同じ字面になるかを、タスク 8 の CI で確かめる（`'$format_float'/4` は今も `to_chars` で 3 OS が一致している）。
- `mosatest --diff` の件数が増えるので（付録 A で 887 件）、ctest の `diff_conformance` が長くなる。CI の時間切れに近ければ、タスク 3 で制限時間を見直す。タスク 3 で差分が出たら、そこで止めて原因を調べる。
- 付録 A の数は、ディレクトリを `functions/` の下に写して測った値である。本来の場所で流すと、相対パスに依る件の結果が変わりうる。本欄の後退の判定は分数で比べるので、予想外の不合格が 1 件でも出ると判定に掛かる。タスク 3 では、範囲を広げた直後に本来の場所で流し、付録 A と同じ数になることを確かめてから物差しを作り直す。

## 付録 A: 範囲の外のディレクトリの測定（2026-11-14、HEAD 688ba14、読んだだけ）

方法: `src/machine/lgt_suite.hpp` を変えずに測った。範囲の外のディレクトリを 1 つずつスクラッチの `logtalk/prolog/functions/x_<名前>/` に写し、`./build/mosatest --suite <スクラッチ> --report <スクラッチ>/r.md` を流した（`functions/` の直下は、`kFunctionExcluded` の 9 つを除いてすべて範囲に入る）。HEAD の CI（run 43493436720）は全ジョブ通過していた。

| ディレクトリ | 分母 | 合格 | 別欄 | 欄 |
| --- | --- | --- | --- | --- |
| `sort_2` | 17 | 17 | 4 | 本欄 |
| `keysort_2` | 18 | 18 | 5 | 本欄 |
| `ground_1` | 7 | 7 | 2 | 本欄 |
| `not_unifiable_2`（`\=/2`） | 10 | 10 | 10 | 本欄 |
| `length_2` | 24 | 23 | 5 | ライブラリ（問い 1 が B なら本欄） |
| `atomic_list_concat_2` | 7 | 7 | 0 | ライブラリ |
| `atomic_list_concat_3` | 10 | 10 | 0 | ライブラリ |
| `between_3` | 13 | 13 | 0 | ライブラリ |
| `format_2` | 198 | 103 | 7 | ライブラリ |
| `format_3` | 200 | 106 | 7 | ライブラリ |
| `forall_2` | 0 | 0 | 0 | ライブラリ |
| `msort_2` | 14 | 14 | 2 | ライブラリ |
| `numbervars_3` | 15 | 9 | 4 | ライブラリ |
| `plus_3` | 15 | 15 | 1 | ライブラリ |
| `predicate_property_2` | 29 | 29 | 0 | ライブラリ |
| `setup_call_cleanup_3` | 31 | 30 | 0 | ライブラリ |
| `succ_2` | 10 | 10 | 1 | ライブラリ |
| `call_nth_2` | 10 | 1 | 1 | 外（未実装） |
| `clause_references` | 20 | 0 | 6 | 外（未実装） |
| `evaluable_property_2` | 16 | 0 | 0 | 外（未実装） |
| `atomic_concat_3` | 8 | 0 | 0 | 外（未実装） |
| `portray_1` | 2 | 0 | 0 | 外（非目標） |
| `directives/conditional_compilation` | 39 | 0 | 0 | 外（未実装） |

- 本欄に 4 つを足すと 2291/2326（98.50%）になる（今は 2239/2274 で 98.46%）。問い 1 が B で `length_2` も足すと 2314/2350（98.47%）。
- ライブラリ欄は `length_2` を含めて 566 件中 369 件（65.2%）。
- 差分テスト: 上の表の本欄とライブラリ欄の 16 ディレクトリ（`forall_2` を除く）をスクラッチに写し、`mosatest --diff` を流した。887/887 件が一致し、違いは 0 件、翻訳した述語は 47/47 だった（今のコード。新しい指示はまだ無い）。
- 出力の桁: 文字の書き込み（`chars.cpp` の `advance`。`put_char/1`、`nl/0`、`tab/1`）と項の書き込み（`write_pred.cpp` の `advance_out`）の両方が `line_pos` を進める。`print/1` は `with_output_to/2` の中でも利用者の `portray/1` を呼ぶ（`portray(X) :- atom(X), write(X), write(X).` のもとで `print(f(foo))` は `f(foofoo)`）。
- `format_2`・`format_3` の不合格 189 件の内訳: 知らない指示が 183 件（`~+` 51、`~r` 24、`~R` 20、`~E` 16、`~g` 14、`~G` 14、`~D` 12、`~p` 10、`~i` 8、`~W` 6、`~k` 4、`~N` 4）、Logtalk の `::` が 4 件、`write_numbervars` が 2 件。
- `stream_property(user_output, position(P))` は失敗するので、`~N` には内部述語が要る。
