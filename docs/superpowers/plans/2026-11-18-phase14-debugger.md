# フェーズ14 デバッガ Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `mosa` に、5 つのポート（呼出・成功・再試行・失敗・例外）を持つトレース、スパイポイント、行単位のコマンドを足す。`system` に `trace/0`、`notrace/0`、`debug/0`、`nodebug/0`、`spy/1`、`nospy/1`、`nospyall/0`、`leash/1`、`debugging/0` を置く。`write_term/2,3` に `max_depth/1` を足す。`mosac` の実行ファイルの中では、デバッガの述語は状態を変えるだけにする。

**Architecture:**
- 箱は `catch/3` と同じ形で作る。`step` が見せるゴールを読んだら、次の 3 つを積む。番兵の選択点 `Pick::DebugBox`、出口の印の枠 `Ctrl::DebugExit`、語 3 をその枠にしたゴール枠の写しである。成功は `DebugExit` の枠で、失敗はバックトラックが番兵に着いたときに、例外は `propagate_throw` が番兵を外すときに捕まえる。再試行は、成功の時点で中に選択点が残るときに積む `Pick::DebugRedo` で捕まえる。
- ポートの行を書き、コマンドを読むのは `DebugConsole`（新しい `src/machine/debug_state.hpp`）である。これを入れるのは `mosa` のセッション（`run_with_machine`）だけで、`mosatest`、`mosac` の実行ファイル、埋め込みの `Machine` には無い。`DebugConsole` が無ければ `step` は箱を作らない。
- `abort` は `halt` と同じ形の新しい止まり方 `StopKind::Abort` にする。`Impl::abort_now` を `run_loop` が `halt_now` の次に見る。
- 深さの切り詰めは、書き出し（`WriteOptions::max_depth`）の働きにする。`write_term` の選択肢と、ポートの行（深さ 10）が同じ働きを使う。

**Tech Stack:** C++20 標準ライブラリ、CMake 3.25。テストには次を使う。
- `mosatest`: `tests/pl` の `test(Name, Goal, Expect)` 行。
- `tests/cpp/harness.hpp`: `mosa_test::check`。
- 端末と同じ表示のトランスクリプト: `tests/cpp/transcript_test.cmake` と、`tests/cpp/data/` の `.in` と `.out`。
- `mosac` の単独実行のテスト: `tests/mosac/standalone_test.cmake`。

**Spec:** `docs/superpowers/specs/2026-11-18-phase14-debugger-design.md`。所有者は 2026-11-18 に問い 1 と 2 をどちらも A に決め、実装計画を頼んだ。これを設計の承認とみなした。ADR 0028 も、同じ依頼で承認されたものとして扱う（これまでのフェーズと同じ。ADR の Status にそう書く）。

## 設計との違い（計画を書くときに決めた）

1. **スパイポイントの引き方。** `DebugState::spies` を線形に走査する。設計は `Predicate` の印で引くとしていた。線形にすれば、定義の前の `Name/Arity`、`consult` での定義し直し、`abolish` のときに印を付け直す道が要らない。速さはタスク 10 の 1.2 倍の条件で確かめ、越えたら印に変える。
2. **ポートの行の出し先。** ポートの行は `DebugConsole::write` に渡す。セッションはそれを `Echo::warn` に繋ぐので、`SessionResult.err` と `std::cerr`（`user_error` と同じ所）に出る。`spy/1` の知らせと `debugging/0` の出力は、`user_error` の流れに書く。この 2 つは実行ファイルでも同じに出る。
3. **箱を持たないゴール。** 次のゴールは箱を持たない。
    - `launch` が IR の節点として扱う物と、同じ名前の枠: `true/0`、`fail/0`、`false/0`、`!/0`、`,/2`、`;/2`、`->/2`、`*->/2`、`:/2`、`catch/3`、`throw/1`、`halt/0,1`。
    - `call/1`〜`call/8`。中のゴールが箱を持つので、利用者には中のゴールが見える。
    - デバッガの 9 つの述語と、名前が `$` で始まる述語。
4. **再試行のポートの表示。** 再試行のポートでは、バックトラックが戻る先の選択点まで束縛を戻してからゴールを書く（SWI-Prolog と同じく `c(_G1)` と見える）。
5. **`r` と `f` の動き。** どちらも番兵より上を `trim_stack` で切り、`failed` を立ててバックトラックに任せる。`r` は番兵に「やり直す」の印を付け、`backtrack` が番兵に着いたら失敗のポートを出さずに箱を開き直す。後始末（ADR 0016）は、`run_loop` が次の歩みの前に今の順で動かす。
6. **例外のポートでの `f`。** その箱を失敗させる。失敗のポートは出さない。
7. **`DebugExit` に着いたときに番兵が無い場合**（カットで切られた後など）。成功のポートは出すが、外す物は無い。
8. **入力の終わり。** 端末でコマンドを読んでいて入力の終わりに着いたら、改行を 1 つ書いてから `n` と同じに扱う。
9. **マニュアル。** 学ぶ人向けの章は `docs/manual/{en,ja}/15-debugger.md` に置く。`14-reference.md` の名前は変えない（公開したパスで、出した版の CHANGELOG もそこを指す）。README の章の一覧に 15 章を足し、「第15章は第8章の後ならいつ読んでもよい」と書く。
10. **`debugging/0` と知らせの字面。** 下の「文面」の表の鍵で決める。
11. **写しの枠は、箱を作った同じ歩みの中で動かす**（`run_boxed`）。設計は「写しに箱を作らない印を付ける」としていた。`Impl` にヒープの番地を持つと、次の歩みの前の GC の安全点で古くなる。
12. **`nodebug/0` と `n` は、進み方を leap に戻す。** `trace, nodebug` の後の `spy/1` が creep で動き出さないようにする。
13. **期待値を変えるテストは無い。** 2026-11-18 に次のことを確かめた。
    - 組込の一覧を見るテストは `tests/manual_tests.cmake` 53 行目の型だけである。9 つの述語を `kBuiltins` の末尾（`statistics/2` の後）に足せば、`call/1` より前の `[^~]*` に入るので、型はそのまま合う。
    - 旗 `debug` を `on` にするのは `tests/cpp/module_load_test.cpp` 319 行目と `tests/pl/modules.pl` 161 行目だけで、どちらもデバッガの口の無い機械で動く。
    - マニュアルとトランスクリプトに、デバッグを入れる例は無い。
    - 文書で直すのは、第14章の旗の表の `debug` の行（「今は `on` にしても何も変わりません」）である。タスク 4 で直す。
14. **タスクは 10 に分けた。** 設計の 9 タスクのうち、abort（`StopKind::Abort`、16 か所、読み込みを元に戻すこと）を独立させてタスク 7 にした。レビュアーがほかと分けて見られるようにするためである。

## Global Constraints

- 依存は C++20 標準ライブラリと CMake 3.25 以上だけにする。`-Wall -Wextra -Wpedantic`（MSVC は `/W4`）の警告はエラーとして扱う。3 OS（macOS、Linux の GCC と Clang、Windows の MSVC）で通す。
- 生の `new`/`delete`、`reinterpret_cast`、C 配列、マクロ、コンパイラ拡張を使わない。OS に依る書き方は `src/platform/` の外に書かない。関数は 60 行以内、ファイルは 800 行以内にする。
  - `src/machine/internal.hpp`（794 行）と `src/builtins/write_pred.cpp`（799 行）には、タスク 2 で分けてから足す。
  - `src/machine/load.cpp`（759 行）には、`Module::library` を付ける 3 行ほどだけを足す。
  - `tests/tests.cmake`（747 行）には、foreach の行を足すだけにする。
- エラーは `throw_error`/`make_*_error` の族と今の `formal_*`/`raise_*` の助け手だけで作る。アトムの名前を `Cell` に直に組まない。
- 組込の C++ 関数は、引数の確かめ → 本体 → 束縛の順に書く。ファイルの頭のコメントには、規格の条項（無ければ「13211-1 に無い」と ADR 0028）、引数のモード、投げうる誤り、テストのファイル名を書く。
- テストを先に書き、赤を確かめてから緑にする。エラーのテストは `Formal` だけを比べる。期待値を実装に合わせて変えない。
- `tests/pl` のテストのファイルは、すべて `mosatest --diff` に入る（`diff_files`）。ポートの行を比べるテストは `tests/pl` に置かない。`tests/pl` の行を足すか変えるコミットでは `ctest --preset macos-clang -L diff` を流し、`docs/agreement.md` と `docs/agreement.json` を同じコミットに入れる。
- suite の範囲は変えない。`max_depth_option_supported` の 14 件は別欄に残し、ISO 列の分母は 2326 のまま変えない。
- 識別子は英語、コメントとコミットメッセージは日本語で書く。文書は英語が正で、`*.ja.md` を同じコミットで直す。`docs/STATUS.md` と `docs/open-items.md` は英語だけにする。
- コミットは論理単位ごとに 1 つ。メッセージは 1 行で、`[phase14] 内容（確認方法）` の形にする。
- 1 セッション 1 タスク。始めに `docs/STATUS.md` の「In progress」にタスクを書く。終わりに `cmake --build --preset macos-clang && ctest --preset macos-clang` を通し、STATUS を直す。
- レビュアーは opus を使う。

## Review Focus

1. **追跡しても答えが同じ。** カット（`!`、`once/1`、`->`、`*->`、`\+`）、`findall/3`、`catch/3`、`setup_call_cleanup/3`、`call/N` を含むゴールを、creep で最後まで追った答えと、デバッグなしの答えが同じである。決定的な問い合わせの後の `Machine::choice_depth()` も同じである。タスク 5 の `check_trace_same_answers` と `check_trace_choice_depth` で確かめる。
2. **トップレベルの `;` の後の再試行。** `trace, c(X).` の最初の答えで `;` を打つと、` Redo: (1) c(_G1) ? ` が出て、次の答えに進む。タスク 5 の `debug_more` で確かめる。
3. **入れ子の箱を抜ける例外。** `q :- p. p :- throw(oops).` で `trace, catch(q, E, true).` とすると、例外のポートが内側から `p`、`q` の順に出て、`E = oops` になる。タスク 6 の `debug_exception` で確かめる。
4. **後始末と再試行。** `setup_call_cleanup/3` の選択点を中に残した箱を、成功か再試行のポートで `r` か `f` にしても、後始末は外された回数だけ動き、ヒープを壊さない。タスク 6 の `check_retry_cleanup`、`check_redo_retry_cleanup`、`check_redo_fail_cleanup` で確かめる。
5. **読み込みの中の abort。** トレース中の `consult/1` の指令の中で `a` を打つと、その読み込みは元に戻り、プロンプトに戻る。読み込んだはずの述語は無い。タスク 7 の `debug_abort_load` で確かめる。

---

## 共通の型と名前

タスク 4〜8 で使う名前を、ここで決める。各タスクの Interfaces はここを指す。

`src/machine/debug_state.hpp`（タスク 4 で作る。`internal.hpp` が取り込み、`Machine::Impl` に `DebugState debug;` を持たせる）:

```cpp
namespace mosa {
enum class DebugMode : std::uint8_t { Creep, Skip, Leap };
enum class DebugPort : std::uint8_t { Call, Exit, Redo, Fail, Exception };
// leashed の 1 ビットが 1 つのポート。ビットは 1 << static_cast<int>(port)。
inline constexpr std::uint8_t kAllPorts = 0x1F;
struct SpyKey { std::uint32_t module = 0; std::uint32_t name = 0; std::uint16_t arity = 0; };
struct DebugConsole {
  std::function<void(std::string_view)> write;   // ポートの行と、コマンドが書く行
  std::function<bool(std::string&)> read_line;   // 1 行読む。偽なら入力の終わり。空の関数なら読まない
};
struct DebugState {
  DebugMode mode = DebugMode::Leap;
  std::uint8_t leashed = kAllPorts;
  std::vector<SpyKey> spies;
  std::optional<DebugConsole> console;
  std::uint64_t next_box = 1;
  std::uint64_t skip_box = 0;                        // 0 は skip の対象なし
  std::array<std::string, 5> port_names;             // 文面の表から 1 度だけ引く
  std::optional<MessageLanguage> names_lang;
};
}
```

デバッグの入り切りは、今の `Impl::debug_on`（旗 `debug`）をそのまま使う。

`src/machine/internal.hpp` に足す物（タスク 5〜7）:
- `Pick::DebugBox`、`Pick::DebugRedo`、`Ctrl::DebugExit`
- `bool abort_now = false;`（`Impl`。タスク 7）
- 宣言の 5 つ:
  - `bool debug_wants_box(Impl&, const Goal&)`
  - `void open_box(Impl&, const Goal&)`（番兵を積み、`run_boxed` を呼ぶ）
  - `void run_boxed(Impl&, std::size_t box_index)`（写しの枠と `DebugExit` を積み、呼出のポートを出し、`Go` なら同じ歩みの中で写しを `run_builtin`、だめなら `enter_user` に渡す。再試行で箱を開き直すときも使う）
  - `void exit_box(Impl&, std::size_t extra_index)`
  - `bool backtrack_box(Impl&)`（`backtrack` の中で、一番上が `DebugBox` か `DebugRedo` のときに呼ぶ。真なら `backtrack` も真を返す）
  - `PortAction throw_box(Impl&, std::size_t box_index, const ExportedTerm& ball)`（`propagate_throw` が一番上の `armed` の `DebugBox` を外す前に呼ぶ）

`src/machine/debug_port.hpp`（タスク 5 で作る）:
- `enum class PortAction : std::uint8_t { Go, Fail, Retry, Abort };`
- `PortAction debug_port(Impl&, DebugPort, std::size_t box_index, const Cell* ball);`

`debug_port` は、ポートを見せるか、止まるか、コマンドを読むか、進み方を変えるかを 1 か所で決める。

番兵の欄は `Entry` に足さず、今の欄を使う。足すなら、タスク 5 の速さの確かめで 1.05 倍を見る。`armed` と `cond_ok` を読む所は、どれも `pick` が `Catch` か、`soft_id` が 0 でないことで絞っている（`engine.cpp` 33、36、50 行目、`choice.cpp` 110、255 行目、`native_redo.cpp` 32 行目、`write_pred.cpp` 646 行目）。番兵の `soft_id` は 0 のままにする。
- `redo_goal`: 元のゴール枠（GC は今のまま付け替える。`gc.cpp` 105 行目）
- `next`: 箱の番号
- `armed`: 抜けていない
- `cond_ok`: やり直す
- heap、trail、extras の水位: 写しの枠と `DebugExit` を積む前の水位
- `DebugRedo` は、`next` に箱の番号、`cut` に番兵の添字を持つ。
- `DebugExit` の `Extra` は、`trim_to` に番兵の添字、`soft_id` に箱の番号、`cont` に元の継続を持つ。

**ポートの行**は、行頭の空白 1 つ、ポート名、`: (`、深さ、`) `、ゴールの順に並べる。
- ゴールは `quoted(true)`、`numbervars(true)`、`max_depth(10)` で書く。
- 例外のポートでは、ゴールの後ろに空白 1 つと投げられた項を足す。
- 止まるポートでは `" ? "` で終え、改行は書かずに 1 行読む。止まらないポートでは `"\n"` で終える。
- 深さは、`box_index` 以下にある `armed` の `DebugBox` の数である。

**文面**（`lib/messages_en.pl` と `lib/messages_ja.pl` に足す鍵。引くのは `message_text`。`render_message` は問い合わせを捨てるので使わない）:

| 鍵 | en | ja |
| --- | --- | --- |
| `debug_port_call` 〜 `debug_port_exception` | `Call`、`Exit`、`Redo`、`Fail`、`Exception` | `呼出`、`成功`、`再試行`、`失敗`、`例外` |
| `debug_aborted` | `% Execution aborted` | `% 実行を中断しました` |
| `debug_unknown_command` | `Unknown command: ~w (h for help)` | `知らないコマンドです: ~w（h で一覧）` |
| `debug_help` | コマンドの表の 1 行ずつ（字面はタスク 8 で書く） | 同じ |
| `debug_spy_set` / `debug_spy_removed` / `debug_spy_none` | `% Spy point on ~w` / `% Spy point removed from ~w` / `% No predicate named ~w` | `% ~w にスパイポイントを置きました` / `% ~w のスパイポイントを外しました` / `% ~w という名前の述語はありません` |
| `debug_status_off` / `debug_status_on` | `% Debug mode: off` / `% Debug mode: on (~w)` | `% デバッグ: 切り` / `% デバッグ: 入り（~w）` |
| `debug_status_spies` / `debug_status_leash` / `debug_status_none` | `% Spy points: ~w` / `% Leashed ports: ~w` / `none` | `% スパイポイント: ~w` / `% 止まるポート: ~w` / `なし` |

`debug_status_on` の引数は `creep`、`skip`、`leap` のどれかの原子である。スパイポイントは `Name/Arity` を `, ` で区切り、ポートは `call exit redo fail exception` の順に空白で区切る。

---

### Task 1: フェーズを開く（文書だけ）

**Files:**
- Modify: `docs/SPEC.md` と `.ja.md`
  - 第3節の旗の項目: 旗 `debug` の意味。`on` は `debug/0` と、`off` は `nodebug/0` と同じ。
  - 第6節: デバッガの箱、ポートを出すのは `mosa` のセッションだけ、`StopKind::Abort`。
  - 第7節: デバッガの述語の行。
  - 第9節の表に 14 行目。
- Modify: `docs/iso-notes.md` と `.ja.md`
  - 新しい節「Debugger (not in 13211-1)」: 旗 `debug` の意味、`max_depth/1` の問いと決定 A（2026-11-18、所有者）、ISO 5.5 の処理系固有の機能として受けること。
- Create: `docs/adr/0028-debugger-box.md` と `.ja.md`。Modify: `docs/adr/README.md` と `.ja.md`（索引に 1 行）
- Modify: `docs/STATUS.md`

- [ ] **Step 1: SPEC 第9節の 14 行目を書く。** 内容の欄と完了条件の欄は次のとおり。
  - 内容: "Debugger: the ports call, exit, redo, fail and exception in `mosa` (ADR 0028); `trace/0`, `notrace/0`, `debug/0`, `nodebug/0`, `spy/1`, `nospy/1`, `nospyall/0`, `leash/1` and `debugging/0` in `system`; the commands of the port prompt; the write option `max_depth/1`"
  - 完了条件: 設計の「完了条件」の 1〜8 を、問い 2 の決定 A に合わせて 1.2 倍の条件を入れて書く。
- [ ] **Step 2: 旗 `debug` の条項の番号を確かめる。** 規格の本文はリポジトリに無い。読める資料で 7.11.2.2 と確かめられたときだけ、その番号を書く。確かめられなければ「7.11.2（flag `debug`）」とだけ書く。
- [ ] **Step 3: ADR 0028 を書く。** Status は「Approved (2026-11-18, owner; the owner's request for the phase 14 plan counts as approval)」。4 つの部分はそれぞれ 3 行以内にする。
  - Context: ポートの出口を捕まえる場所が無いこと（継続のトランポリン、継続を枠の語 3 から読み直す所）。
  - Decision: 番兵 `DebugBox`、出口の枠 `DebugExit`、写しの枠、`DebugRedo`、`DebugConsole` があるときだけ箱を作ること、`StopKind::Abort`。
  - Rejected alternatives: メタインタプリタ、節の書き換え、`Goal` の値だけの差し替え。
  - Consequences: 切っている間の負担は分岐 1 つ、leap はスパイポイントにだけ箱、トレース中の末尾呼び出しはメモリを食うこと、テスト。
- [ ] **Step 4: 確かめる。** `cmake --build --preset macos-clang && ctest --preset macos-clang` が通る。`./build/mosatest --manual docs/manual --all-tables` が 0 failed。
- [ ] **Step 5: Commit。** メッセージは `[phase14] フェーズ14を開き SPEC 第3・6・7節と第9節の 14 行目、iso-notes、ADR 0028 を日英で書く（文書だけ、manual 0 failed）`。

---

### Task 2: `internal.hpp` と `write_pred.cpp` を分ける（振る舞いは変えない）

**Files:**
- Create: `src/machine/translation_log.hpp`（`TranslationLog` と `EmbeddedSource` を移す。`internal.hpp` の 217〜265 行目）
- Create: `src/machine/stream_state.hpp`（`Stream` を移す。301〜338 行目）
- Modify: `src/machine/internal.hpp`（移した物を取り込む）
- Create: `src/builtins/write_capture.cpp`（`with_output_to/2` とその助け手。`write_pred.cpp` の 573〜767 行目のうち、`print/1,2` の `portray/1` と分け合わない物）
- Modify: `src/builtins/write_pred.cpp`、`src/machine/builtin_api.hpp`（`bool run_with_output(Impl&, const Goal&)`）、`src/machine/engine.cpp`（`run_builtin` の列に足す）、`CMakeLists.txt`（`mosa_lib` の源の一覧に新しいファイルを足す。一覧は手で書いてある）

**Interfaces:**
- Produces: `[[nodiscard]] bool run_with_output(Impl& machine, const Goal& goal);`。`internal.hpp` は 720 行以下、`write_pred.cpp` は 700 行以下になる。

- [ ] **Step 1: 移す。** 中身は 1 字も変えない。`portray/1` の道（`apply_portray`）が `run_goal` などを使うなら、その分は `write_pred.cpp` に残すか、共通の内部の宣言を `src/builtins/write_internal.hpp` に置く。
- [ ] **Step 2: 確かめる。** `cmake --build --preset macos-clang && ctest --preset macos-clang` が全部通る。`wc -l` で行数の上限を見る。
- [ ] **Step 3: Commit。** メッセージは `[phase14] internal.hpp から TranslationLog と Stream を、write_pred.cpp から with_output_to/2 を分ける（振る舞いは変えない、ctest 全通過）`。

---

### Task 3: 書き出しの深さの切り詰めと `write_term` の `max_depth/1`

**Files:**
- Modify: `include/mosa/writer.hpp`（`WriteOptions` に `int max_depth = 0;`。0 は制限なし）
- Modify: `src/write/writer.cpp`（`Printer` が深さを数える。723 行なので、越えるなら先に分ける）
- Modify: `src/builtins/write_pred.cpp`（`Chosen` に `max_depth`、`apply_option` に `max_depth`）
- Test: `tests/pl/write.pl`

**Interfaces:**
- Produces: `WriteOptions::max_depth`。タスク 5 のポートの行は `max_depth = 10` で書く。

- [ ] **Step 1: 失敗するテストを書く。** suite の 14 件（`lgt_write_term_3_123`〜`136`）を 1 行ずつ写す。字面は `with_output_to(atom(A), write_term(T, Opts))` で取る。
  - `wd_inst`: `write_term(1, [max_depth(_)])` → `error(instantiation_error)`
  - `wd_foo`: `write_term(1, [max_depth(foo)])` → `error(domain_error(write_option, max_depth(foo)))`
    - suite は `type_error(integer, foo)` も認める。今の選択肢の誤りに揃えて `domain_error` にする。
  - `wd_neg`: `max_depth(-1)` → `error(domain_error(write_option, max_depth(-1)))`
  - `wd_foo_silent`: 誤りの前に何も書かない。`with_output_to(atom(A), catch(write_term(1, [max_depth(foo)]), _, true))` → `A = ''`
  - `wd_zero`、`wd_ten`: `1` を `max_depth(0)`、`max_depth(10)` で書くと `'1'`
  - `wd_deep6`、`wd_list6`: `max_depth(6)` で `'a(b(c(d(e))))'`、`'[1,2,3,4,5]'`
  - `wd_deep3`、`wd_list3`: `max_depth(3)` で `'a(b(c(...)))'`、`'[1,2,3|...]'`
  - `wd_deep1`、`wd_list1`: `max_depth(1)` で `'a(...)'`、`'[1|...]'`
  - `wd_zero_deep`、`wd_zero_list`: 13 段の `a(b(…(m)…))` と 13 要素のリストを `max_depth(0)` で書くと、全部が出る
  - 足す行: `wd_with_quoted`。`write_term('A'(b(c)), [quoted(true), max_depth(2)])` → `'\'A\'(b(...))'`
- [ ] **Step 2: 赤を確かめる。** `./build/mosatest tests/pl/write.pl` で、新しい行が `domain_error(write_option, max_depth(_))` で落ちる。
- [ ] **Step 3: 作る。** 深さ 1 は書く項そのものである。複合項の引数は、深さを 1 つ増やして書く。上限を越えた部分項は `...` と書く。深さ `d` のリストの `i` 番目（1 から）の要素は、深さ `d + i - 1` にあるとみなす。上限を越える最初の要素から後ろを `|...` と書く（`[1,2,3,4,5]` を深さ 3 で `[1,2,3|...]`、深さ 1 で `[1|...]`）。
- [ ] **Step 4: 緑を確かめる。**
  - `./build/mosatest tests/pl/write.pl` が全部通る。
  - `ctest --preset macos-clang -L diff` の差分が 0。
  - `./build/mosatest --suite tests/conformance` の ISO 列の分母が 2326 のままで、合格率が下がらない。`max_depth` の 14 件は別欄のまま。
- [ ] **Step 5: Commit。** `docs/agreement.md` と `docs/agreement.json` を入れる。メッセージは `[phase14] write_term に max_depth/1 を足す（suite の 14 件の期待を write.pl に写して全通過、diff 0、ISO 分母 2326）`。

---

### Task 4: デバッガの状態と 9 つの述語（ポートはまだ出さない）

**Files:**
- Create: `src/machine/debug_state.hpp`（「共通の型と名前」のとおり）
- Create: `src/builtins/debugger_pred.cpp`（9 つの述語）
- Modify: `src/machine/internal.hpp`（`#include "debug_state.hpp"` と `DebugState debug;`）
- Modify: `src/machine/builtin_api.hpp`（`bool run_debugger(Impl&, const Goal&)`）
- Modify: `src/machine/engine.cpp`（`kBuiltins` の末尾、`statistics/2` の後に 9 行。`run_builtin` の列に `run_debugger`）
- Modify: `src/builtins/flags.cpp`（旗 `debug` を `on` にすると `debug/0`、`off` にすると `nodebug/0` と同じ働きにする。読むのは今の `debug_on`）
- Modify: `src/module/module.hpp`（`Module` に `bool library = false;`）
- Modify: `src/machine/load.cpp`（`use_file` で、`names_library(machine, arg)` が真か `bundled` に値があるとき、読んだモジュールの `library` を真にする）
- Modify: `lib/messages_en.pl`、`lib/messages_ja.pl`（`debug_spy_*` と `debug_status_*` の鍵）
- Modify: `docs/manual/en/14-reference.md` と `ja/`
  - 「Built-in predicates」に「Debugging」の表を足す。`--all-tables` は `system` の組込がすべて表にあることを求める。
  - 旗の表の `debug` の行を、`on` が `debug/0`、`off` が `nodebug/0` と同じ働きになると書き直す。
- Test: `tests/pl/debugger.pl`（新しいファイル）、`tests/pl/japanese.pl`、`tests/pl/fixtures/dbg_redefine.pl`
- Test: `tests/mosac/debugger_state.pl`、`tests/mosac/standalone_test.cmake`（新しい場合 `debugger_state`）、`tests/tests.cmake`（foreach に `debugger_state`）

**Interfaces:**
- Consumes: 「共通の型と名前」の `DebugState`、`SpyKey`、`DebugMode`、`kAllPorts`
- Produces:
  - `[[nodiscard]] bool run_debugger(Impl& machine, const Goal& goal);`
  - `void set_debug(Impl& machine, bool on, DebugMode mode);`: `trace/0`、`debug/0`、旗が共有する。`nodebug/0` と旗の `off` は `set_debug(m, false, DebugMode::Leap)` を呼ぶ。
  - `[[nodiscard]] bool spied(const Impl& machine, std::uint32_t module, std::uint32_t name, std::uint16_t arity);`: タスク 6 の leap が使う。
  - `Module::library`: タスク 5 の箱の判定が使う。

- [ ] **Step 1: 失敗するテストを書く（`tests/pl/debugger.pl`）。**
  - どの行も、終わりに `nodebug, nospyall, leash(all)` で状態を戻す。
  - ファイルの頭で `dbg_p/1` と `dbg_q/0` を定義する。
  - 状態の行:
    - `dbg_trace_on`: `trace` の後、旗 `debug` は `on`
    - `dbg_notrace_keeps`: `trace, notrace` の後も `on`
    - `dbg_debug_on`: `debug` の後は `on`
    - `dbg_nodebug_off`: `debug, nodebug` の後は `off`
    - `dbg_flag_off`: `trace, set_prolog_flag(debug, off)` の後、旗 `debug` は `off`
    - `dbg_spy_debug`: `spy(dbg_p/1)` の後、旗は `on`
    - `dbg_spy_undefined`: `spy(dbg_nowhere/3)` は `true`
    - `dbg_spy_name`: `spy(dbg_p)` は `true`
    - `dbg_spy_name_none`: `spy(dbg_no_such)` は `true`（知らせを書く）
    - `dbg_spy_list`: `spy([dbg_p/1, dbg_q/0])` は `true`
    - `dbg_spy_qualified`: `spy(user:dbg_p/1)` は `true`
    - `dbg_nospy`: `spy(dbg_p/1), nospy(dbg_p/1)` は `true`
    - `dbg_leash_forms`: `leash(all)`、`leash(none)`、`leash([call, -exit, +fail])`、`leash(-redo)` はどれも `true`
    - `dbg_debugging`: `debugging` は `true`
  - 誤りの行:
    - `spy(_)` → `instantiation_error`
    - `spy(1)` → `type_error(predicate_indicator, 1)`
    - `spy(dbg_p/_)` → `instantiation_error`
    - `spy(dbg_p/a)` → `type_error(integer, a)`
    - `spy(dbg_p/(-1))` → `domain_error(not_less_than_zero, -1)`
    - `spy(1/2)` → `type_error(atom, 1)`
    - `nospy(_)` → `instantiation_error`
    - `leash(_)` → `instantiation_error`
    - `leash(foo)` → `domain_error(port, foo)`
    - `leash(+foo)` → `domain_error(port, foo)`
    - `leash(1)` → `type_error(atom, 1)`
    - `leash([call|_])` → `instantiation_error`
  - 定義し直しの行 `dbg_redefine`: `consult('tests/pl/fixtures/dbg_redefine.pl')` は `fail`。このファイルは `debug.` の 1 節だけを持つ。読み込みは `permission_error(modify, static_procedure, debug/0)` で止まる。
  - `tests/pl/japanese.pl` に、事実 `デバッグ用(1).` と次の 3 行を足す。
    - `ja_spy_pi`: `spy(デバッグ用/1)` → `true`
    - `ja_spy_name`: `spy(デバッグ用)` → `true`
    - `ja_spy_bad`: `spy(デバッグ用/あ)` → `error(type_error(integer, あ))`
- [ ] **Step 2: `mosac` の単独実行のテストを書く。** `tests/mosac/debugger_state.pl` は `initialization(main, main)` で次のことをする。
  - `spy(p/1), trace, p(X), write(X), nl, notrace, nodebug, current_prolog_flag(debug, F), write(F), nl, debugging` を動かす。`p(1).` を定義する。
  - 標準出力の期待は `"1\noff\n"`。
  - 標準エラーは比べない（`debugging/0` と知らせが書く）。
- [ ] **Step 3: 赤を確かめる。** `./build/mosatest tests/pl/debugger.pl` が `existence_error(procedure, trace/0)` などで落ちる。`ctest --preset macos-clang -R standalone_debugger_state` も落ちる。
- [ ] **Step 4: 作る。**
  - `spy(Name)` は、文脈のモジュールから見える `Name/_` の述語を、`resolve_pred` で定義のモジュールまで引いて置く。
  - `spy(Name/Arity)` は、定義が無ければ文脈のモジュールで置く。
  - 組込は `system` で置く。
  - 知らせと `debugging/0` は `user_error` の流れに書く。
  - `leash/1` は要素を左から順に当てる。`all` と `none` は全部を入れるか切り、`Port` と `+Port` は足し、`-Port` は外す。
- [ ] **Step 5: 緑を確かめる。**
  - `./build/mosatest tests/pl/debugger.pl` と `tests/pl/japanese.pl` と `tests/pl/flags.pl` が通る。
  - `ctest --preset macos-clang -L diff` の差分が 0。
  - `ctest --preset macos-clang -R "standalone_debugger_state|manual"` が通る。
  - `./build/mosatest --manual docs/manual --all-tables` が 0 failed。
- [ ] **Step 6: Commit。** `docs/agreement.*` を入れる。メッセージは `[phase14] デバッガの 9 つの述語と状態を system に足す（debugger.pl と japanese.pl の新しい行、diff 0、standalone_debugger_state、manual 0 failed）`。

---

### Task 5: 箱と 4 つのポート（creep だけ）

**Files:**
- Create: `src/machine/debug_box.cpp`（`debug_wants_box`、`open_box`、`exit_box`、`backtrack_box`）
- Create: `src/machine/debug_port.hpp`、`src/machine/debug_port.cpp`（`debug_port`、ポートの行、深さ、文面の取り置き）
- Modify: `src/machine/internal.hpp`（`Pick::DebugBox`、`Pick::DebugRedo`、`Ctrl::DebugExit`、宣言）
- Modify: `src/machine/engine.cpp`
  - `step`: `read_goal` の後、`run_builtin` の前に、`if (machine.debug_on && machine.debug.console && debug_wants_box(machine, goal)) { open_box(machine, goal); return; }`
  - `step_control`: `Ctrl::DebugExit` のときに `exit_box`
- Modify: `src/machine/choice.cpp`（`backtrack` の中で `DebugBox` と `DebugRedo` を `backtrack_box` に渡す）
- Modify: `src/machine/session.cpp`
  - `run_with_machine` で `DebugConsole` を入れる。`write` は `echo.warn`、`read_line` は `tty` のときだけ `cursor.line` にする。
  - 促しを `prompt_text(machine)` にする。`debug_on` で creep なら `"[trace] ?- "`、ほかのデバッグ中は `"[debug] ?- "`、切りなら `"?- "`。
- Modify: `lib/messages_en.pl`、`lib/messages_ja.pl`（`debug_port_*`）
- Modify: `tests/cpp/transcript_test.cmake`（`-DPROGRAM=<.pl>` があれば `mosa` に引数として渡す）
- Modify: `tests/tests.cmake`
  - `foreach(debug_case …)` で `transcript_<case>` を登録する（たとえば `transcript_debug_creep`）。
  - `-DDATA=${CMAKE_SOURCE_DIR}/tests/cpp/data/debugger` と `-DPROGRAM=${CMAKE_SOURCE_DIR}/tests/cpp/data/debugger/dbg.pl`（`debug_creep_ja` だけ `ja.pl`）を渡す。
  - `WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}` にする（`.in` の中の相対パスはリポジトリの根から開く）。
- Create: `tests/cpp/data/debugger/dbg.pl`、`tests/cpp/data/debugger/ja.pl`、各場合の `.in` と `.out`
- Create: `tests/cpp/debugger_test.cpp`（`tests/tests.cmake` に `debugger` として登録。`machine/internal.hpp` を見る）

**Interfaces:**
- Consumes: タスク 4 の `DebugState`、`Module::library`、タスク 3 の `WriteOptions::max_depth`
- Produces: 「共通の型と名前」の `debug_port`、`PortAction`、`run_boxed`、`Pick::DebugBox`、`Pick::DebugRedo`、`Ctrl::DebugExit`、`prompt_text`。タスク 6 は `debug_port` に例外のポートとコマンドを足す。

`tests/cpp/data/debugger/dbg.pl`（タスク 6〜8 も使う）:

```prolog
len([], 0).
len([_|T], N) :- len(T, M), N is M + 1.
c(1).
c(2).
q :- p.
p :- throw(oops).
double(X, Y) :- Y is X * 2.
deep(f(f(f(f(f(f(f(f(f(f(f(a)))))))))))).
:- use_module(library(apply)).
:- use_module(library(lists)).
```

`.in` は、打った行を順に並べた物である。問い合わせ、ポートのコマンド（空のコマンドは空の行）、答えの後の `;` が入る。`.out` の ` ? ` の行は、末尾に空白が 1 つある。どの場合も、最後に `notrace, nodebug.` を打って（スパイポイントを置いた場合は `nospyall, nodebug.`）、`?- ` で終える。

- [ ] **Step 1: 失敗するトランスクリプトを書く**（`LANG=en_US.UTF-8`。`debug_creep_ja` だけ `ja_JP.UTF-8` で、`PROGRAM` は `ja.pl`）。
  - `debug_creep`:
    ```text
    ?- trace, len([a], N).
     Call: (1) len([a],_G1) ? 
     Call: (2) len([],_G1) ? 
     Exit: (2) len([],0) ? 
     Call: (2) _G1 is 0+1 ? 
     Exit: (2) 1 is 0+1 ? 
     Exit: (1) len([a],1) ? 
    N = 1.
    [trace] ?- notrace.
    true.
    [debug] ?- nodebug.
    true.
    ?- 
    ```
  - `debug_creep_ja`: `ja.pl` は `長さ([], 0). 長さ([_|T], N) :- 長さ(T, M), N is M + 1.` で、問い合わせは `trace, 長さ([あ], N).`。行は `debug_creep` と同じ形で、ポート名が `呼出`・`成功` になり、`len` が `長さ`、`a` が `あ` になる。
  - `debug_more`:
    ```text
    ?- trace, c(X).
     Call: (1) c(_G1) ? 
     Exit: (1) c(1) ? 
    X = 1;

     Redo: (1) c(_G1) ? 
     Exit: (1) c(2) ? 
    X = 2.
    [trace] ?- notrace, nodebug.
    true.
    ?- 
    ```
  - `debug_fail`: `trace, c(3).` → ` Call: (1) c(3) ? `、` Fail: (1) c(3) ? `、`false.`
  - `debug_library`: `trace, append([1], [2], L).` → ` Call: (1) append([1],[2],_G1) ? `、` Exit: (1) append([1],[2],[1,2]) ? `、`L = [1,2].`。続いて `trace, maplist(double, [1], L).` の行を順に確かめる。
    - ` Call: (1) maplist(double,[1],_G1) ? `
    - ` Call: (2) double(1,_G1) ? `
    - ` Call: (3) _G1 is 1*2 ? `
    - ` Exit: (3) 2 is 1*2 ? `
    - ` Exit: (2) double(1,2) ? `
    - ` Exit: (1) maplist(double,[1],[2]) ? `
    - `L = [2].`
- [ ] **Step 2: 失敗する C++ のテストを書く（`tests/cpp/debugger_test.cpp`）。** `DebugConsole` の `read_line` は、いつも空の行を返す（creep）。
  - `check_trace_same_answers`: 次のゴールを、`trace` の後と、デバッグなしとで解く。`format_answer` の字面が同じであることを確かめる。
    - `findall(X, (member(X, [1,2,3]), X > 1), L)`
    - `once(member(X, [a,b]))`
    - `(member(X, [1,2]) -> Y = X ; Y = none)`
    - `(member(X, [1,2]) *-> true ; X = none)`
    - `\+ member(z, [a])`
    - `catch((member(X, [1,2]), X > 1, throw(t(X))), t(Y), true)`
    - `setup_call_cleanup(true, member(X, [1,2]), true), X > 1`
    - `call(len, [a,b], N)`
    - `(member(X, [1,2,3]), X >= 2, !)`
  - `check_trace_choice_depth`: `trace, len([a,b], N)` の後の `choice_depth()` が、デバッグなしの `len([a,b], N)` の後と同じである。
  - `check_no_console`: `DebugConsole` の無い機械では、`trace, len([a], N)` で `write` が一度も呼ばれず、`choice_depth()` も同じである。
- [ ] **Step 3: 赤を確かめる。** `ctest --preset macos-clang -R "transcript_debug|debugger"` が落ちる。
- [ ] **Step 4: 作る。**
  - `debug_wants_box` は、安い判定から順に見る。leap の速さ（1.2 倍）を守るためである。
    1. 進み方が `Skip` なら偽。`Leap` はタスク 6 で決める（ここでは偽）。
    2. 文脈のモジュールの `Module::library` が真なら偽。
    3. 「設計との違い」3 の名前なら偽。
    4. ここまで来れば真。
  - `open_box` は番兵を積み、`run_boxed` を呼ぶ。`run_boxed` は、`DebugExit` と写しの枠を積み、`debug_port(Call)` を呼ぶ。`Go` なら `ip` を写しにし、同じ歩みの中で `read_goal(写し)` を `run_builtin` に、だめなら `enter_user` に渡す。`run_builtin` と `enter_user` の宣言が `debug_box.cpp` から見えなければ、`internal.hpp` に足す。
  - `exit_box` は、番兵が一番上なら外し、上に選択点があれば `armed = false` にして `DebugRedo` を積み、`ip` を元の継続にする。
  - `backtrack_box` は次のように動く。
    - `DebugRedo` なら外し、次の一番上の水位まで状態を戻し、`armed = true` にして再試行のポートを出し、偽を返してバックトラックを続けさせる。
    - `DebugBox` なら失敗のポートを出して外し、偽を返す。
  - `debug_port` は、ポートの行を書き、止まるポートなら 1 行読む。このタスクでは、何を読んでも creep として扱う。
- [ ] **Step 5: 緑を確かめる。**
  - `ctest --preset macos-clang -R "transcript_debug|debugger"` が通る。
  - `MOSA_GC_STRESS=1 ctest --preset macos-clang -R "transcript_debug|debugger"` が通る。
  - `ctest --preset macos-clang` が全部通る。
- [ ] **Step 6: 切っている間の速さを確かめる。**
  - `git worktree add ../mosa-0.4.1 v0.4.1` で 0.4.1 を Release で作る。
  - 両方の `mosatest --bench bench/nrev.pl` と `bench/queens.pl` を 3 回ずつ走らせる。
  - インタプリタの時間の中央値が、0.4.1 の 1.05 倍以内であることを確かめる。越えたら、`step` の分岐を `[[unlikely]]` にする、`DebugState` を `Impl` の後ろに置く、の順に直す。
  - 作業木は消す。
- [ ] **Step 7: Commit。** メッセージは `[phase14] 箱と呼出・成功・再試行・失敗のポートを足す（transcript_debug_* と debugger が GC stress でも通過、nrev と queens が 0.4.1 の 1.05 倍以内）`。

---

### Task 6: 例外のポート、コマンド `s` `l` `f` `r` `n`、leash、leap

**Files:**
- Modify: `src/machine/catch.cpp`（`propagate_throw` で、一番上が `armed` の `DebugBox` なら、外す前に `throw_box` を呼ぶ）
- Modify: `src/machine/debug_box.cpp`（`throw_box`、retry と fail、leap の判定）
- Modify: `src/machine/debug_port.cpp`（コマンド、leash、skip と leap の見せ方）
- Test: トランスクリプトの場合を足す。`tests/cpp/debugger_test.cpp` に 3 つ足す。

**Interfaces:**
- Consumes: タスク 5 の `debug_port`、`PortAction`、`run_boxed`、番兵の欄、タスク 4 の `spied`
- Produces: `PortAction::Abort` を返す道（`a` を読んだとき）。このタスクでは、`Abort` を受けた所は `Go` と同じに扱う。止まり方はタスク 7 で作る。

コマンドは次のとおり。ポートはどれも、行の最初の空白でない文字で決める。

| 入力 | 呼出 | 成功 | 再試行 | 失敗 | 例外 |
| --- | --- | --- | --- | --- | --- |
| 空、`c` | creep | creep | creep | creep | creep |
| `s` | その箱まで skip | creep | その箱まで skip | creep | creep |
| `l` | leap | leap | leap | leap | leap |
| `f` | 失敗させる | 失敗させる | 失敗させる | creep | 失敗させる（ポートなし） |
| `r` | creep | 呼出に戻る | 呼出に戻る | 呼出に戻る | 呼出に戻る |
| `a` | abort（タスク 7） | 同じ | 同じ | 同じ | 同じ |
| `n` | デバッグを切り、進み方を leap に戻して続ける | 同じ | 同じ | 同じ | 同じ |

`backtrack_box`（再試行と失敗のポート）の中で `r` か `f` を受けて `trim_stack` を呼んだら、`failed = true` にして真を返す。`run_loop` は、切った後始末を動かしてからもう一度バックトラックし、番兵に着く。`backtrack` の中で番兵の水位まで戻すと、後始末の項が載ったヒープを切り詰めてしまうからである（ADR 0016、敵対的レビュー A1 と同じ）。

leap と skip の見せ方は次のとおり。
- leap で箱を作るのは、`spied` が真の呼び出しだけである（問い 2 の決定 A）。
- leap 中は、スパイポイントの述語の箱のポートでだけ止まる。
- skip 中は、`skip_box` の箱のポートでだけ止まり、止まったら `skip_box` を 0 にして creep に戻る。

止まらないポート（`leashed` のビットが無い）は、行を `"\n"` で終えて進む。

- [ ] **Step 1: 失敗するトランスクリプトを書く。**
  - `debug_exception`:
    ```text
    ?- trace, catch(q, E, true).
     Call: (1) q ? 
     Call: (2) p ? 
     Exception: (2) p oops ? 
     Exception: (1) q oops ? 
    E = oops.
    ```
  - `debug_skip`: `trace, len([a,b], N).` で、` Call: (1) len([a,b],_G1) ? s`、` Exit: (1) len([a,b],2) ? `、`N = 2.`。
  - `debug_leap`:
    - `spy(len/2).` は、`% Spy point on len/2` と `true.` を書く。促しは `[debug] ?- ` になる。
    - `len([a], N).` で `l` を 4 回打つと、行は ` Call: (1) len([a],_G1) ? l`、` Call: (2) len([],_G1) ? l`、` Exit: (2) len([],0) ? l`、` Exit: (1) len([a],1) ? l` になり、`N = 1.` で終わる。`is` は出ない。
    - 最後に `nospyall, nodebug.` を打つ。
  - `debug_retry`: `trace, len([a], N).` で、成功のポート ` Exit: (1) len([a],1) ? r` の後に ` Call: (1) len([a],_G1) ? ` からもう一度同じ 6 行が出て、`N = 1.` で終わる。
  - `debug_force_fail`: `trace, len([a], N).` の ` Call: (1) len([a],_G1) ? f` の後に ` Fail: (1) len([a],_G1) ? ` が出て、`false.` で終わる。
  - `debug_nodebug`: ` Call: (1) len([a],_G1) ? n` の後に `N = 1.` が出て、促しは `?- ` になる。続く `spy(c/1), len([], N).` は、ポートを出さずに `N = 0.` になる（`n` で進み方が leap に戻った）。最後に `nospyall, nodebug.` を打つ。
  - `debug_leash`: `leash(-exit).` の後、`trace, len([], N).` で ` Call: (1) len([],_G1) ? ` と ` Exit: (1) len([],0)`（` ?` なし、改行で終わる）が出て、`N = 0.` になる。最後に `leash(all), notrace, nodebug.` を打つ。
- [ ] **Step 2: 失敗する C++ のテストを書く。** 3 つとも、`:- use_module(library(lists)).`、`:- dynamic(cleaned/0).` と次の 2 節を読む。
  - `r :- setup_call_cleanup(true, member(_, [1,2]), assertz(cleaned)).`
  - `s :- setup_call_cleanup(true, member(_, [1,2]), assertz(cleaned)).`

  `read_line` は、直前に `write` された行が決まった行のときに 1 回だけコマンドを返し、ほかは空の行を返す。後始末の回数は、`query("nodebug, findall(x, cleaned, L), length(L, N)")` の `N` で数える。この問い合わせは前の問い合わせを閉じるので、閉じたときの後始末も数に入る。
  - `check_retry_cleanup`: `trace, r` で、` Exit: (1) r ? ` に `r` を返す。再試行で番兵より上を切るので 1 回、閉じて 1 回で、`N = 2`。
  - `check_redo_retry_cleanup`: `trace, s, fail` で、最初の ` Redo: (1) s ? ` に `r` を返す。再試行の切り詰めで 1 回、2 回目の `s` が `member` を使い切って 1 回で、`N = 2`。答えは `false`。
  - `check_redo_fail_cleanup`: `trace, s, fail` で、最初の ` Redo: (1) s ? ` に `f` を返す。切り詰めで 1 回だけで、`N = 1`。答えは `false`。
- [ ] **Step 3: 赤を確かめる。** `ctest --preset macos-clang -R "transcript_debug|debugger"` の新しい場合が落ちる。
- [ ] **Step 4: 作る。** 「設計との違い」の 5〜7 のとおりにする。`throw_box` は番兵を外す前に呼ばれるので、深さは今のまま数えられる。投げられた項は `import_term` でヒープに戻して書く。`Retry` のときは、番兵を外さずに `cond_ok` を立て、`exception = false`、`failed = true`、`ip = kNoFrame` にして巻き戻しを止める。`run_loop` のバックトラックが番兵に着いて、箱を開き直す。
- [ ] **Step 5: 緑を確かめる。**
  - `ctest --preset macos-clang -R "transcript_debug|debugger"` が通る。
  - `MOSA_GC_STRESS=1` を付けても通る。
  - `ctest --preset macos-clang` が全部通る。
- [ ] **Step 6: Commit。** メッセージは `[phase14] 例外のポート、s l f r n、leash、leap を足す（transcript_debug_* と debugger が GC stress でも通過、ctest 全通過）`。

---

### Task 7: abort

**Files:**
- Modify: `include/mosa/machine.hpp`（`StopKind::Abort`）
- Modify: `src/machine/internal.hpp`（`Impl` に `bool abort_now = false;`）
- Modify: `src/machine/engine.cpp`（`run_loop` は `halt_now` の次に `abort_now` を見て、`Stop{StopKind::Abort, 0, {}, {}, false}` を返す。後始末は、`halt` と同じく動かさない）
- Modify: `src/machine/debug_box.cpp`、`debug_port.cpp`（`PortAction::Abort` で `abort_now` を立てる）
- Modify: `StopKind::Halt` を見る 16 か所（`git grep -n "StopKind::Halt" -- src include`）
  - `session.cpp`: `play_goal` は `debug_aborted` を書いて 1 を返す。`play_main` と、起動時のファイルの読み込みは、失敗と同じ値で終わる。
  - `query.cpp`: `drop_run` で `abort_now` を消す。
  - `load_directive.cpp`、`load_cond.cpp`: その読み込みを、読み込みの誤りと同じく失敗させて元に戻す。診断は書かず、`abort_now` は残す。外側の `run_loop` が次に見て止まる。
  - `write_pred.cpp`: `portray/1` の中の abort は、`halt` と同じく書くのを止める。
  - `diff.cpp`、`lgt_*`: `Failure` と同じに扱う（デバッガの口が無いので起きない）。
- Modify: `lib/messages_*.pl`（`debug_aborted`）
- Test: トランスクリプトの 2 つの場合、`tests/cpp/debugger_test.cpp` に 1 つ

**Interfaces:**
- Consumes: タスク 6 の `PortAction::Abort`
- Produces: `StopKind::Abort`、`Impl::abort_now`

- [ ] **Step 1: 失敗するテストを書く。**
  - `debug_abort`: ` Call: (2) len([],_G1) ? a` の後に `% Execution aborted` が出る。促しは `[trace] ?- ` に戻る（デバッグの状態は変えない）。
  - `debug_abort_load`:
    - `tests/cpp/data/debugger/load.pl` は `dbg_init. loaded(1). :- dbg_init.` をこの順に持つ（指令より前に `dbg_init/0` を定義する）。
    - `trace, consult('tests/cpp/data/debugger/load.pl').` では、まず ` Call: (1) consult('tests/cpp/data/debugger/load.pl') ? ` で空の行を打つ。次の ` Call: (1) dbg_init ? a` で中断する。指令はスタックを入れ替えて脇で動くので、深さは 1 から数え直す。
    - `% Execution aborted` が出て、プロンプトに戻る。その後の `notrace, nodebug, current_predicate(loaded/1).` は `false.` になる（読み込みは元に戻った）。
  - `check_abort_stop`（C++）: 呼出のポートで `a` を返すと、`query` が `StopKind::Abort` を返す。次の `query("true")` は `Success` で、`abort_now` は消えている。
- [ ] **Step 2: 赤を確かめる。** `ctest --preset macos-clang -R "transcript_debug_abort|debugger"` が落ちる。コンパイラが `switch` の抜けを警告する所も、ここで全部見つける。
- [ ] **Step 3: 作る。**
- [ ] **Step 4: 緑を確かめる。**
  - `ctest --preset macos-clang -R "transcript_debug|debugger"` が通る。
  - `MOSA_GC_STRESS=1` を付けても通る。
  - `ctest --preset macos-clang` が全部通る。
- [ ] **Step 5: Commit。** メッセージは `[phase14] abort を StopKind::Abort として足し、読み込みの中では読み込みを元に戻す（transcript_debug_abort*、check_abort_stop、ctest 全通過）`。

---

### Task 8: コマンド `g` `w` `+` `-` `h` `?`、知らない入力、入力の終わり、端末でない `mosa`

**Files:**
- Modify: `src/machine/debug_port.cpp`
- Modify: `lib/messages_*.pl`（`debug_help`、`debug_unknown_command`）
- Modify: `tests/cpp/transcript_test.cmake`（`-DARGS=` と `-DTRANSCRIPT=0` を受ける）
- Test: トランスクリプトの場合を足す。

**Interfaces:**
- Consumes: タスク 5、6 の `debug_port`、タスク 4 の `spy/1` の置き方（`+` と `-` は同じ関数を呼ぶ）

`g`、`w`、`+`、`-`、`h`、`?`、知らない入力は、書いた後でもう一度同じポートの行を書いて問う。

- [ ] **Step 1: 失敗するトランスクリプトを書く。**
  - `debug_goals`: ` Call: (2) len([],_G1) ? g` の後に ` (2) len([],_G1)` と ` (1) len([a],_G1)`（内側から。行頭は空白 1 つ）が出て、同じポートの行がもう一度出る。
  - `debug_write`:
    - `trace, deep(X).` の ` Exit: (1) deep(f(f(f(f(f(f(f(f(f(...)))))))))) ? w` の後に、`deep(f(f(f(f(f(f(f(f(f(f(f(a))))))))))))` が 1 行出る。
    - 続けて同じポートの行がもう一度出る。
  - `debug_spy_toggle`: ` Call: (1) len([a],_G1) ? +` の後に `% Spy point on len/2` が出て、同じ行がもう一度出る。続けて `-` を打つと `% Spy point removed from len/2` が出る。
  - `debug_help`: `h`、`?`、`x` を順に打つ。`h` と `?` の後には `debug_help` の行が、`x` の後には `Unknown command: x (h for help)` が出る。
  - `debug_eof`: `.in` が ` Call: (1) len([a],_G1) ? ` の所で終わる。改行が 1 つ書かれ、デバッグを切って `N = 1.` が出る。
  - `debug_batch`:
    - `TRANSCRIPT=0` で、`ARGS` は `tests/cpp/data/debugger/dbg.pl -g "trace, len([a], N)" -t halt`、`.in` は空にする。
    - 出力は、`debug_creep` の 6 つのポートの行（どれも ` ?` が無く改行で終わる）と、`-g` の答えの `N = 1.` である。
- [ ] **Step 2: 赤を確かめる。**
- [ ] **Step 3: 作る。** `g` は、`box_index` 以下の `armed` の `DebugBox` を上から順に、` (深さ) ゴール` の形で書く。`w` は `max_depth = 0` で書く。
- [ ] **Step 4: 緑を確かめる。**
  - `ctest --preset macos-clang -R "transcript_debug|debugger"` が通る。
  - `MOSA_GC_STRESS=1` を付けても通る。
  - `ctest --preset macos-clang` が全部通る。
- [ ] **Step 5: Commit。** メッセージは `[phase14] g w + - h ? と入力の終わり、端末でない mosa のポートを足す（transcript_debug_* 全通過）`。

---

### Task 9: マニュアル

**Files:**
- Create: `docs/manual/en/15-debugger.md` と `ja/`
- Modify: `docs/manual/{en,ja}/README.md`（章の一覧に 15 章、「第15章は第8章の後ならいつ読んでもよい」）
- Modify: `13-mosac.md`（実行ファイルではポートが出ないこと、`--debug` の節の追跡との違い）
- Modify: `14-reference.md`（「Debugging」の表にコマンドの表を足す、「Known limits」に設計の既知の制限、終わりの「Next」のリンクに 15 章）
- Modify: `08-control.md`（終わりに「デバッガは第15章」の 1 行）

- [ ] **Step 1: 章を書く。** 次の 5 つの節に分ける。どの例も `<!-- check: id ... -->` の入力付きで、`mosatest --manual` が動かす。
  1. `len/2` を creep で追う（`debug_creep` と同じ形）。
  2. ポートの意味を、呼出・成功・再試行・失敗・例外の図にする。
  3. 再試行を見る（`c/1` と `;`）。
  4. `spy/1` と leap で、大きな探索の途中から追う。
  5. コマンドの表と、`leash/1`、`debugging/0`。
  - 各節の終わりは `notrace.` か `nodebug.` で、促しを `?- ` に戻す。
- [ ] **Step 2: 確かめる。** `./build/mosatest --manual docs/manual --all-tables` が 0 failed。`ctest --preset macos-clang -R manual` が通る。
- [ ] **Step 3: Commit。** メッセージは `[phase14] マニュアルにデバッガの章（第15章）を日英で書く（manual 0 failed）`。

---

### Task 10: 文書を閉じる

**Files:**
- Modify: `CHANGELOG.md` と `.ja.md` の `## Unreleased`
  - 新しい述語とコマンド、`write_term` の `max_depth/1`。
  - 互換性: 9 つの名前を利用者が定義できなくなること、旗 `debug` が `mosa` で働くこと。
  - 既知の制限: 設計の「既知の制限」。
- Modify: `README.md` と `.ja.md`、`docs/iso-notes.md` と `.ja.md`、`docs/open-items.md`、`docs/STATUS.md`
- Create: `bench/nrev_debug.pl`、`bench/queens_debug.pl`（`:- debug. :- spy(bench_unused/0). bench_unused. :- include('nrev.pl').` の形）
- Modify: `docs/bench.md` と `.ja.md`

- [ ] **Step 1: 測る。** `bench/nrev.pl`、`queens.pl`、`nrev_debug.pl`、`queens_debug.pl` を `mosatest --bench` で 3 回ずつ走らせ、中央値を取る。次の 3 つを確かめる。
  - デバッグを切った状態の時間が、0.4.1 の 1.05 倍以内である（タスク 5 と同じ作業木で測る）。
  - `*_debug.pl` の時間が、切った状態の 1.2 倍以内である（`bench_unused/0` は呼ばれない）。
  - `mosac` の `nrev` の比が 3 以上である。
  - 越えたら直す。leap の 1.2 倍を越えたら、「設計との違い」1 のスパイポイントを `Predicate` の印に変える。
- [ ] **Step 2: 書く。** 上の Files の文書を日英で書く。
- [ ] **Step 3: 確かめる。** 次のすべてが通ることを確かめる。
  - `ctest --preset macos-clang`
  - `./build/mosatest --suite tests/conformance`（ISO 列 98.4% 以上で分母 2326、ライブラリ列 98.0% 以上）
  - `./build/mosatest --manual docs/manual --all-tables`
  - push の後の CI の全ジョブ（3 OS と GC-stress）
- [ ] **Step 4: Commit。** メッセージは `[phase14] CHANGELOG、README、iso-notes、bench を日英で書き、フェーズ14を閉じる準備をする（ctest 全通過、suite、manual 0 failed、bench の値）`。STATUS に CI の結果を書き、フェーズを閉じるかを所有者に問う。
