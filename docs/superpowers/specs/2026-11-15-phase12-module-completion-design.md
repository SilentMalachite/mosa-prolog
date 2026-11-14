# フェーズ12 モジュールの仕上げ 設計

日付: 2026-11-15
状態: 下書き（所有者の承認待ち。問い 1〜3 は 2026-11-15 に所有者が 3 つとも A に決めた）

## 意図

フェーズ 0〜11 は閉じ、0.2.2 を公開した。SPEC 第9節に次のフェーズは無い。所有者は 2026-11-15 に「モジュール周りを拡張したい」と依頼し、私の出した案のうち次の組み合わせを選んだ。ゴールからの読み込み（A）、`reexport/1,2`（C）、内省の述語（B。ただし `add_import_module/3` を除く）である。ライブラリの追加（D）と `term_expansion/2`・`goal_expansion/2`（E）は、このフェーズに入れない。

2026-11-15 に、HEAD c842021 の `build/mosa` で次のことを確かめた。

- `use_module/1,2` と `ensure_loaded/1` は、読み込みの指令としてしか使えない。`mosa -g 'use_module(library(lists))'` は `existence_error(procedure, use_module/1)` になる。iso-notes の「7.4.2 ensure_loaded/1」は「指令だけ」と決めている。マニュアル第12章（日英）は「`use_module/1` はプロンプトでは打てない」と書き、ファイルに書いて読み込むよう案内している。
- `strip_module/3`、`context_module/1`、`module_property/2`、`import/1`、`reexport/1`、`@/2`、`add_import_module/3` は、どれも `existence_error` になる。`current_module/1` と `predicate_property/2` はある（`src/builtins/module_prop.cpp`）。
- ゴールの `consult/1`（`run_consult`、`src/machine/consult.cpp`）について、STATUS の疑問点は 2 つの件を挙げている。調べると、どちらも今の決まりから来ていた。
  - 同じファイルを、選択点が残っている間や `findall/3` の中で読み直すと失敗する。再現: `c1.pl` に `p(1). p(2).` を書き、`consult('c1.pl'), findall(x, consult('c1.pl'), L)` を走らせると `L = []` になる。原因は `take_over`（`src/machine/load_clauses.cpp`）の断りである。前の読み込みの節が残る述語を置き換えるとき、選択点のスタックに `catch/3` と後始末のほかの項目が 1 つでもあれば、読み込みを `predicate_running`（「実行中の述語は置き換えられない」）で止める。失敗した読み込みを写しから戻す ADR 0017 が、走査の無い状態を前提にしているためである（ADR 0017 の Consequences）。`tests/cpp/session_test.cpp` の `check_consult_failure_reports` が、この振る舞いを確かめている。初めて読むファイルは、`findall` の中でも成功する。
  - 失敗の理由が出ないことがある。問い合わせ全体が失敗したときは、トップレベルが `Machine::diagnostic()` を `user_error` に書く（`src/machine/session.cpp`）。理由が出ないのは、`;`、`findall/3`、`\+` が失敗を吸ったときだけである。

ゴールからの読み込みは、この `consult/1` の道に載せる。問い 3 の決定 A により、選択点の下の読み直しの断りは残し、断ったことを利用者に見えるようにする（問い 1 の決定 A）。

この文書を読むのは、このあと実装計画を書き、実装するエージェントである。所有者がこの文書を承認したあとに、SPEC 第9節の 12 行目、ADR 0026 と実装計画を書く。承認までは計画も実装もしない。

## 目標

- `use_module/1`、`use_module/2`、`ensure_loaded/1` をゴールとして呼べる。プロンプトでも呼べる。取り込み先は、そのゴールの文脈モジュールである。
- ゴールの読み込み（`consult/1` と上の 3 つ）が失敗したら、`;`、`findall/3`、`\+` の中でも、その時点で診断を `user_error` に書く（問い 1 の決定 A）。選択点の下の読み直しの断りも、こうして見えるようになる。
- `:- reexport(File)` と `:- reexport(File, Imports)` で、取り込んだ述語を自分のモジュールの公開述語として出せる。
- `system` に `strip_module/3`、`context_module/1`、`module_property/2`、`import/1` が入る。
- どの新しい述語でも、インタプリタと `mosac` が訳したコードの結果が同じになる（`mosatest --diff` の差分 0）。`mosac build` で作った実行ファイルも（問い 2 の決定 A）、ゴールで字面に書いたライブラリを同梱から見つける。

## 非目標

- `@/2`。Mosa のゴールの枠は文脈モジュールを 1 つだけ持ち、述語を引く先もその値で決まる（ADR 0006）。`@/2` は「述語を引くモジュール」と「文脈モジュール」を分ける必要があるので、枠の形と ADR 0006 を変えることになる。このフェーズでは入れない（私の判断）。
- `add_import_module/3` と `delete_import_module/2`。既定の取り込み元の連鎖（SPEC 第5節の解決の 3 段目）を変えるので、所有者の選択どおり外す。
- `term_expansion/2`、`goal_expansion/2`、条件付きコンパイル、ライブラリの追加（`pairs`、`assoc` など）。次のフェーズの候補として残す。
- 次の決定は変えない。自動読み込みをしない（iso-notes「Section 5 Autoload」、決定 A）。モジュールを作るのは `module/2` だけ（「Section 5 Clause operations on an unknown module」、決定 A）。`use_module` はモジュールのファイルだけを受け付ける（「Section 5 use_module of a file that is not a module」、決定 A）。ゴールの `use_module` にも、この 3 つをそのまま当てる。
- `[File]` の形をゴールで呼ぶこと（`[File]` は指令のまま）、`load_files/2`、`make/0`、`consult(library(Name))`。
- 読み直しの規則を変えること。`use_module` と `ensure_loaded` は、ゴールでも 1 回だけ読む（準備済みのファイルは読まず、取り込みだけ足す）。
- 選択点の下での読み直しを許すこと（問い 3 の決定 A）。`take_over` の断りと ADR 0017 はそのまま残す。`check_consult_failure_reports` の期待値も変えない。
- `reexport` をゴールで呼ぶこと。`reexport` は読み込み中のモジュールに公開述語を足す指令なので、指令に限る。
- 13211-2 の条項に合わせること。リポジトリには 13211-2 の本文が無く、確かめられるのは `module_prop.cpp` の見出しにある 7.2.1 と 7.2.2 だけである。新しい述語の振る舞いは SWI-Prolog 10.0.2 の文書の部分集合にする。ファイルの見出しには「13211-1 に条項は無い。13211-2 での位置は未確認」と書く。
- このフェーズでの公開。公開は所有者の依頼で別に行う。

## アンチ目標（技術的に動いても、これをやったら失敗）

- SPEC 第5節の解決の規則（`system`、自分の定義と取り込み、既定の取り込み元）を変えること。
- 上の非目標に挙げた 3 つの決定を、黙って変えること。
- ゴールの読み込みが失敗したとき、節、モジュール、取り込み、演算子を半端に残すこと。指令の読み込みと同じく、ADR 0017 の巻き戻しにかける。
- インタプリタと `mosac` の結果が違うこと。
- テストの期待値を実装に合わせて書き換えること。問い 1 の決定 A でも、既存のテストが確かめている成功と失敗は変わらない。変わるのは `user_error` の出力だけである。出力を比べるテストがあれば、iso-notes に決定を書いた同じコミットで直す。そのテストが何を確かめていたかは変えない。

## 制約

- AGENTS.md のとおり。
  - C++20 標準ライブラリと CMake だけを使う。
  - 関数は 60 行以内、ファイルは 800 行以内にする。
  - 生の `new`/`delete`、`reinterpret_cast`、C 配列、マクロを使わない。
  - エラーは `throw_error`/`make_*_error` の族と、今の `formal_*` の助け手だけで作る。アトムの名前を文字列で直に書かない。
  - 組み込み述語は、引数の検査、本体、束縛の順に書く。
  - 組み込み述語のファイルの見出しには、節番号、引数のモード、投げうる誤り、テストのファイル名を書く。
- `src/machine/internal.hpp` は 800 行ある。新しい宣言を足す前に、振る舞いを変えずに分ける（STATUS の「上限に近いファイル」）。`src/builtins/database.cpp`（762 行）には足さない。新しい組み込み述語は新しいファイルに置く。
- ゴールの読み込みと再公開の形は、モジュールの解決に関わる。AGENTS.md の規則どおり、実装の前に ADR 0026 を書く。所有者が設計を承認して実装を頼めば、それを ADR の承認とみなす（これまでのフェーズと同じ扱い）。ADR の Status にそう書く。
- テストを先に書く（TDD）。モジュールのテストは `tests/pl/modules.pl` に、その読み込み先は `tests/pl/modules/` に置く。日本語の名前のテストは `tests/pl/japanese.pl` に書く（`mosatest --diff` に必ず入る）。エラーのテストは `Formal` だけを比べる。新しく作るファイルは `tests/tmp/` の下に置く。
- 識別子は英語で書く。コメントとコミットメッセージは日本語で書く。文書は英語が正で、`*.ja.md` を同じコミットで直す。

## 所有者への問い（iso-notes に「A か B か」と決定を書く。2026-11-15 に所有者が 3 つとも A に決めた）

1. **ゴールの読み込みが失敗したときの知らせ方**（`consult/1`、`use_module/1,2`、`ensure_loaded/1`。引数の誤りは、今と同じく例外にする）
   - A: 読み込みの診断を、コマンド行から読んだときと同じ字面で `user_error` に書き、ゴールは失敗する。ファイルが開けないときも同じにする。
   - B: A に加えて、次の 3 つは診断を書かずに Formal を投げる。開けないファイルは `existence_error(source_sink, File)`、取り込みの誤りはその Formal（`permission_error(import_into, procedure, From:N/A)`、`existence_error(procedure, M:N/A)`、`permission_error(load, source_sink, F)`）。構文の誤り、失敗した指令、拒んだ節は A と同じく診断を書いて失敗する。
   - 決定: A（所有者、2026-11-15）。私の推した理由: 規則が 1 つで済み、M10（拒んだ節）と M24（入れ子の読み込みの失敗）のテストが確かめている「ゴールの `consult/1` は失敗する」を変えない。また、iso-notes の「7.4.2 ensure_loaded/1」の「`existence_error(source_sink, File)` は使わない」とも揃う。B は SWI-Prolog に近いが、読み込みの誤りが 2 通りの知らせ方に分かれる。
   - 前提の訂正（決定の後に分かった）: 問いを書いたときは「今は何も出ない」としていた。実際には、問い合わせ全体が失敗すれば、トップレベルが `Machine::diagnostic()` を `user_error` に書く。理由が出ないのは、`;`、`findall/3`、`\+` が失敗を吸ったときだけである。決定 A はそのまま入れる。そのうえで、トップレベルは、ゴールがすでに書いた診断をもう一度書かない。`Machine::diagnostic()` は今と同じく最後の読み込みの診断を返す（公開の API の振る舞いは変えない。私の判断）。
2. **`mosac` が同梱するライブラリ**
   - A: 翻訳の読み込みで、節の本体に字面で書かれた `use_module(library(Name))` と `use_module(library(Name), _)` を見つけたら（一般の指令と `initialization/1` のゴールも見る。「決めたこと」の `mosac` の節）、そのライブラリも同梱の閉包に入れる。`Name` が変数のときは入れず、実行時に `$MOSA_LIB` と `lib/` を探す。
     - 問いを出したときは `ensure_loaded(library(Name))` も例に挙げていた。しかし `ensure_loaded/1` は原子しか受け付けない（iso-notes「7.4.2 ensure_loaded/1」。`:- ensure_loaded(library(lists))` は `type_error(atom, library(lists))`）。そのため例から外した（私の判断）。`ensure_loaded/1` の引数の規則は変えない。
   - B: 同梱するのは、今と同じく指令の閉包だけにする。ゴールで読むライブラリは、実行時に `$MOSA_LIB` と `lib/` だけを探す。既知の制限として書く。
   - 決定: A（所有者、2026-11-15）。私の推した理由: フェーズ6の「`mosac build` の実行ファイルは、コンパイラも mosac も無い環境で動く」を、ゴールで読むライブラリにも広げる。B だと、`mosac dist` で配った実行ファイルの中で `use_module(library(lists))` が見つからず、インタプリタと結果が分かれる。
3. **選択点の下での読み直し**（設計書を書いた後、実装者の目での確認で分かった）
   - 事実: 前の読み込みの節が残る述語を、選択点のスタックに `catch/3` と後始末のほかの項目があるときに置き換えようとすると、`take_over` が読み込みを `predicate_running` で止める。ADR 0017 の写しからの巻き戻しが、走査の無い状態を前提にしているためである。`check_consult_failure_reports` がこれを確かめている。
   - A: 断りを残す。このフェーズでは、断ったことが問い 1 の決定 A で見えるようにするだけにする。
   - B: 断りをやめる。ADR 0017 を改め、走査が残っていても成り立つ巻き戻しに作り直す。断りを確かめる今のテストも書き換える。
   - 決定: A（所有者、2026-11-15）。私の推した理由: B は規模が読めず、ADR 0017 の作り直しはモジュールの拡張の外にある。初めて読むファイルは断られず、`use_module` と `ensure_loaded` は 1 回しか読まないので、このフェーズの述語はふつう断りに当たらない。iso-notes に、この断りを「7.4.2 Consulting a path again」の続きとして書く。

## 決めたこと（私の判断）

### ゴールの読み込み

| 述語 | モード | 意味 |
| --- | --- | --- |
| `use_module/1` | `use_module(+Spec)` | 指令の `:- use_module(Spec)` と同じ。`Spec` は `library(Name)` か原子 |
| `use_module/2` | `use_module(+Spec, +Imports)` | 指令の `:- use_module(Spec, Imports)` と同じ。`Imports` は PI のリスト（`as` を含む）か `except(List)` |
| `ensure_loaded/1` | `ensure_loaded(+File)` | 指令の `:- ensure_loaded(File)` と同じ |

- 引数の検査と Formal は、指令と同じにする（`instantiation_error`、`type_error(atom, _)`、`type_error(list, _)`、`type_error(predicate_indicator, _)`）。ゴールでは、これを例外として投げる。
- 相対パスは、今の作業ディレクトリから開く。ゴールの `consult/1` と同じである。読み込み中のファイルの指令から呼ばれたゴールでも同じで、そのファイルのディレクトリは見ない。指令として書いた `:- use_module(...)` は今のまま、読み込み中のファイルのディレクトリから開く。
- 取り込み先は、ゴールの文脈モジュールである（`run_consult` と同じく `load_module` をその値に替える）。プロンプトなら `user`、モジュール `m` の節の本体なら `m` になる。
- ゴール 1 回を、外側の読み込み 1 回として扱う。ADR 0017 の巻き戻し、ADR 0020 の読み直しの印、`initialization/1` のゴールの実行が、指令の読み込みと同じ形でかかる。読み込みの中から呼ばれたときは、`run_consult` と同じく入れ子の読み込みになる。
- `library(...)` で見つけたファイルは、指令と同じく ISO の核と自分のモジュールの演算子で読む（ADR 0006 の改訂 1）。同梱のライブラリの訳したコードも、指令と同じ道で付ける（`attach_bundled_library`）。
- 成功すれば決定的に成功し、選択点を残さない。
- iso-notes の「7.4.2 ensure_loaded/1」の「指令だけ」を改める。ゴールでも呼べるようにし、そのほかの決定（拡張子、1 回だけ読むこと）はそのまま残す。
- 仕組み（指令の `apply_use`・`apply_file` を直接呼ぶか、`messages.cpp` のように指令の字面を読み込むか）は、実装者に任せる。ただし、実行時に束縛された引数（たとえば `use_module(library(N))` で `N` が束縛済み）を正しく渡すこと。GC の根の登録（`run_consult` の `RootScope`）も同じ形にすること。

### ゴールの読み込みの失敗の知らせ方

- 問い 1 の決定 A に従い、`consult/1` と新しい 3 つで同じにする。読み込みが失敗した時点で、コマンド行から読んだときと同じ字面（`パス: 理由`。構文の誤りなら `パス:行:桁: 理由`）を `user_error` に書く。そのうえでゴールは失敗する。
- 診断を書いたゴールのあとで問い合わせ全体が失敗しても、トップレベル（`src/machine/session.cpp`）は同じ診断をもう一度書かない。見分け方は実装者に任せる。`Machine::diagnostic()` の返す値は変えない。
- 選択点の下の読み直しの断り（問い 3）も、この規則で見える。`(true ; fail), consult(F)` は、今と同じく失敗する。`user_error` に `F: 実行中の述語は置き換えられない` が 1 回だけ出る。

### `reexport/1,2`

- `:- reexport(Spec)` と `:- reexport(Spec, Imports)`。`Spec` と `Imports` の形と誤りは、`use_module/1,2` と同じにする。
- 意味: `use_module` と同じく読んで取り込み、取り込んだ述語を、読み込み中のモジュールの公開述語の一覧に足す。`reexport/1` と `except(...)` では、取り込み元の公開の演算子も公開する（`use_module/1` が演算子を入れる規則、iso-notes「Section 5 Operators in modules」の決定 A と同じ）。PI のリストのときは、演算子は公開しない。
- `as` で別名を付けた述語は、その別名で公開する。
- 公開の一覧の 1 項目は、その述語を定義したモジュールとそこでの名前を持つ（ADR 0026）。だから `reexport` した述語を取り込んだ側の `Import::from` は、定義したモジュールになる。`resolve_pred` は今と同じく 1 回で定義に着き、再公開を何段重ねてもたどる段数は増えない。
- 同じ述語を 2 つの道で取り込むのは衝突にしない。たとえば `lists` を直接取り込み、`lists:append/3` を再公開するモジュールからも取り込む場合である。iso-notes の「Section 5 Conflict between two imports」（2026-10-28 の決定 A）の「取り込み元」は、定義したモジュールで比べると読む。この読みを iso-notes に書く。違う述語が同じ名前で来たときは、今と同じく `permission_error(import_into, procedure, From:N/A)` にする。
- `predicate_property(P, imported_from(M))` の `M` は、定義したモジュールにする。
- モジュールのファイルでないテキスト（`user` へ読むもの）の `reexport` は、`use_module` と同じに扱う。`user` には公開の一覧が無いので、何も足さない。
- `mosac`: `reexport` で開いたファイルも、`use_module` と同じ道（`locate_use` と `TranslationLog`）で同梱の閉包に入る。

### 内省の述語（`system` に置く。新しいファイル、例 `src/builtins/module_goal.cpp`）

| 述語 | モード | 意味 | 誤り |
| --- | --- | --- | --- |
| `strip_module/3` | `strip_module(?Term, ?Module, ?Plain)` | `Term` の先頭の `M:` を、`M` が原子である間だけ外す。`Module` は最も内側の `M`、`M:` が無ければ文脈モジュール。`Plain` は残り | 無し（SWI-Prolog と同じ） |
| `context_module/1` | `context_module(?M)` | 文脈モジュール。プロンプトでは `user`、モジュール `m` の節の本体では `m`、`meta_predicate` の引数として呼ばれたゴールでは呼び手のモジュール | 無し |
| `module_property/2` | `module_property(?M, ?P)` | `M` が変数なら `current_module/1` の順で列挙する。性質は `file(F)`（ファイルのモジュールだけ。`F` は `Module::file` の原子）、`exports(L)`（公開述語の `N/A` を公開した順に並べたリスト。再公開した述語も入る）、`exported_operators(L)`（`op(P, T, N)` のリスト。公開の演算子があるときだけ） | `type_error(atom, M)`、`existence_error(module, M)`（`predicate_property/2` の決定 A と揃える）、`domain_error(module_property, P)` |
| `import/1` | `import(+M:PI)` | `M` の公開述語 `PI` を文脈モジュールへ取り込む。`use_module/2` の PI のリスト 1 項目と同じ規則にする | `instantiation_error`、`type_error(predicate_indicator, PI)`、`type_error(atom, M)`、`existence_error(module, M)`、`existence_error(procedure, M:N/A)`（公開していない述語。iso-notes「Section 5 Importing a predicate that is not exported」の決定 A）、`permission_error(import_into, procedure, From:N/A)` |

- `predicate_property/2` には、`imported_from(M)` の値のほかは手を入れない。`current_predicate/1` には手を入れない。
- `import/1` は PI を 1 つだけ取る。リストは受け付けない（私の判断。リストが要るなら `use_module/2` を使う）。
- `module_property(M, file(F))` の `F` は `Module::file` をそのまま返す。`mosac build` の実行ファイルでは、同梱したファイルの `F` は同梱の鍵（擬似ディレクトリ `library/` の下のパス）になり、インタプリタの実際のパスと違う。テストでは、同梱したファイルの `F` の全体を比べない。この違いは iso-notes に書く（私の判断）。

### 新しく `system` に入る名前

- `use_module/1,2`、`ensure_loaded/1`、`strip_module/3`、`context_module/1`、`module_property/2`、`import/1` は組み込みになる。SPEC 第5節の解決の 1 段目により、利用者がどのモジュールでこの名前の節を書いても、`permission_error(modify, static_procedure, PI)` になる。今は利用者が `user:import/1` などを定義できるので、これは振る舞いの変化である。CHANGELOG に書く。
- 2026-11-15 に `tests/conformance`、`tests/pl`、`bench`、`lib`、`docs/manual` を探した。この名前を頭に持つ節は 1 つも無かった。

### `mosac`

- 生成コードは、新しい組み込みを今の組み込みと同じく `rt::call_goal` で呼ぶ。訳す側に新しい制御は要らない。
- 問い 2 の決定 A により、翻訳の読み込みの後で字面のゴールを探す。探す段は、`src/codegen/check.cpp` が本体を見て未定義の呼び出しを探すのと同じ段にする。見つけたライブラリは、`TranslationLog::library_opens` と同じ形で閉包に足す。置き場所は実装者に任せる。
- 探す範囲は、節の本体に加えて、一般の指令 `:- G` のゴールと `initialization/1` のゴールである。翻訳の読み込みは一般の指令を呼ばず（`run_directive_goal` は `skipped_goal` の印だけを付ける）、それらのゴールは実行ファイルの起動時の読み込みが呼ぶ。だから、指令に書いた `use_module(library(Name))` を拾わないと、同梱から漏れる。

### ADR 0026（ゴールの読み込みと再公開）

フェーズを開くタスクで書く。中身は次のとおり。

- 決定: ゴールの読み込みは、ゴールの文脈モジュールへの外側の読み込み 1 回である。公開の一覧の項目は、名前、項数、定義したモジュール、そこでの名前を持つ。
- 退けた案: 取り込みを、再公開したモジュールを経由してたどる形（`resolve_pred` が取り込みの取り込みをたどることになり、衝突の判定が取り込み元の名前で誤る）。再公開を、取り込む側で展開し直す形（取り込むたびに再公開の元を読み直すことになる）。

## 規模と順序

9 タスク。AGENTS.md の「1 セッション 1 タスク」に合わせて切る。順は私の判断である。

1. フェーズを開く（文書だけ）: SPEC 第9節の 12 行目、第5節（ゴールの `use_module`・`ensure_loaded`、`reexport`、内省の述語）を日英で書く。iso-notes には次を書く: 問い 1〜3 とその決定（3 つとも A）、問い 1 の前提の訂正、「7.4.2 ensure_loaded/1」の改訂、「Conflict between two imports」の読み、`@/2` と `add_import_module/3` を入れない理由。ADR 0026 を日英で書く。STATUS の疑問点の 2 つの件を、問い 3 の決定と問い 1 の決定に置き換える。
2. `src/machine/internal.hpp` を、振る舞いを変えずに 2 つ以上のファイルに分ける。
3. ゴールの読み込みの失敗の知らせ方（問い 1 の決定 A）を `consult/1` とトップレベルに入れる。`;`、`findall/3`、`\+` の中で失敗したときに 1 回だけ出ること、問い合わせ全体が失敗したときに二重に出ないことを、テストで先に確かめる。`tests/cpp/session_test.cpp`（792 行）に足すなら、足す前に分ける。
4. `use_module/1,2` と `ensure_loaded/1` をゴールにする（インタプリタ）。プロンプトで使えることも確かめる。
5. `mosac`: 字面のライブラリを同梱に足す（問い 2 の決定 A）。ctest で、`$MOSA_LIB` が無く `lib/` も隣に無い場所で、`mosac build` の実行ファイルがゴールの `use_module(library(lists))` を通すことを確かめる。
6. `reexport/1,2`（ADR 0026 の公開の一覧の項目）。日本語の名前のモジュールの再公開のテストを `tests/pl/japanese.pl` に足す。
7. `strip_module/3` と `context_module/1`。
8. `module_property/2` と `import/1`。
9. 文書を閉じる: 次を日英で書く。
    - マニュアル第12章: 「プロンプトでは打てない」の文と、`libs.pl` を作る案内を直す。読み直しの記述は変わらないので残す。
    - マニュアル第14章の表
    - CHANGELOG の `## Unreleased`
    - README
    - iso-notes
    - STATUS

    CI の全ジョブが通ることを確かめ、フェーズを閉じるかを所有者に問う。

タスク 3 はタスク 4 より前に置く。タスク 4 の新しい述語が、同じ知らせ方をそのまま使えるようにするためである。タスク 5 はタスク 4 の後に置く。タスク 6〜8 は、タスク 1 の後なら互いに順を入れ替えてよい。

## 完了条件（SPEC 第9節の 12 行目にする）

1. `tests/pl/modules.pl` の新しい行と `tests/pl/japanese.pl` が `mosa` で通る。対象はゴールの `use_module/1,2`・`ensure_loaded/1`、`reexport/1,2`、`strip_module/3`、`context_module/1`、`module_property/2`、`import/1` である。さらに `mosatest --diff` の差分が 0 である。
2. ゴールの読み込みが `;`、`findall/3`、`\+` の中で失敗したとき、その診断が `user_error` に 1 回だけ出るテストが通る。たとえば `consult(F), findall(x, consult(F), L)` で、`L = []` と `F: 実行中の述語は置き換えられない` が 1 行出ること。問い合わせ全体が失敗したときに診断が二重に出ないテストも通る。`check_consult_failure_reports` は、期待値を変えずに通る。
3. ゴールで `use_module(library(lists))` を呼ぶプログラムを `mosac build` で作る。呼ぶ場所は、節の本体と `initialization/1` のゴールの 2 通りにする。そのうえで、その実行ファイルが `$MOSA_LIB` の無い、隣に `lib/` も無い場所で動く（ctest）。
4. `mosatest --suite tests/conformance` の本欄の合格率が 98.4% を、ライブラリ欄の合格率が 98.0% を下回らない。
5. `mosatest --manual docs/manual --all-tables` が通る。
6. CI の全ジョブが 3 OS で通る。

## 実装者に任せること

- ゴールの読み込みの仕組み（指令の関数を直接呼ぶか、指令の字面を読むか）。
- 新しい組み込みのファイルの置き場所と名前、内部の関数の名前、公開の一覧の項目の型の名前。
- `module_property/2` で、1 つのモジュールの性質を並べる順。
- `internal.hpp` の分け方。

## リスク

- ゴールの `use_module` が、選択点の下で、前の読み込みの節が残る述語に当たると、`take_over` の断りで止まる（問い 3 の決定 A）。モジュールのファイルの述語は初めて読むときに作られ、`use_module` は準備済みのファイルを読み直さないので、ふつうは当たらない。当たる場合があるかをタスク 4 で調べ、あればそのときに診断が出て失敗することをテストで確かめる。
- 問い 1 の決定 A では、ゴールの読み込みが失敗すると `user_error` に書く。`tests/pl/failed_load.pl` のように失敗を確かめるテストの出力が増える。`mosatest` と `--diff` が `user_error` を比べるなら、比べ方に合わせる必要がある。タスク 3 で確かめる。
- `--diff` で `mosac` が訳した側を走らせるとき、ゴールで読む素のパスのファイル（同梱の閉包の外）は実行時にディスクから読む。作業ディレクトリが両側で同じであることを、タスク 4 で確かめる。
- 問い 2 の決定 A で翻訳の読み込みが開くライブラリが増えると、翻訳の時間と未定義の呼び出しの警告が変わりうる。
