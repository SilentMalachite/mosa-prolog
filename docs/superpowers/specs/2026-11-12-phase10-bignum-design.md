# フェーズ10 多倍長整数 設計

日付: 2026-11-12
状態: 所有者の承認待ち（フェーズの選択は所有者の決定 A、2026-11-12）

## 意図

フェーズ 0〜9 は閉じ、0.1.4 を公開した。SPEC 第9節に次のフェーズは無い。所有者は 2026-11-12 に、次のフェーズとして多倍長整数（A）を選んだ（もう 1 つの候補は `term_expansion/2` と `goal_expansion/2`）。フェーズ9の設計が「フェーズ10 の候補」として挙げていたものである。

今の整数は 64 ビットで、あふれると `evaluation_error(int_overflow)` になり、2^63 以上の字面は `syntax_error(integer_overflow)` になる。旗 `bounded` は `true` である。このフェーズで整数を上限なし（`bounded` が `false`）にする。値が int64 に収まる間は今の表現（61 ビットの即値 Int と 1 語の IntBox）と今の速さのまま動かし、収まらない値だけを Cell の予約タグ Big（ADR 0001、ADR 0002）の箱に入れる。標準ライブラリだけで書き、GMP は使わない（SPEC 第1節、AGENTS.md）。

読む人は、このあと実装計画を書き、実装するエージェントである。所有者がこの文書を承認するまで、計画は書いても実装はしない。タスク 1 の ADR 0025 は、所有者の承認を別に得てからコードに入る（所有者が頼んだのは計画まで）。

## 目標

- 整数の値に上限が無い。int64 を越える値の読み書き、算術、比較、単一化、標準順序、複製（`assertz`、`findall`、`copy_term`、`nb_setval`、`throw`）、GC、`mosac` が訳したコードが、int64 の値と同じ規則で動く。
- 旗 `bounded` が `false` になる（7.11.1.1）。`max_integer` と `min_integer` は所有者の決定に従う（下の「所有者への問い」1）。
- int64 の値は今と同じ表現で、今と同じ道を通る。int64 だけを使うプログラムは遅くならない（完了条件 5）。
- 準拠スイートの `unbounded/` ディレクトリが範囲に入り、動く件がすべて通る（通らない件は iso-notes に解釈の違いとして書き、所有者が決める）。
- インタプリタと `mosac` が訳したコードの結果が同じになる。`mosatest --diff` の差分 0 を守る。

## 非目標

- `max_arity` を上限なしにすること。1024 のまま（SPEC 第3節）。スイートの `max_arity` が `unbounded` の件（`lgt_unbounded_functor_01`・`02`、`arg_02`・`03`）は別欄に残る。
- 有理数、`prefer_rationals`、`rational/1`、`rationalize/1`。
- ISO に無い評価可能関数子 `lsb/1`、`popcount/1`（`lgt_suite.hpp` の `kFunctionExcluded` のまま）。
- `succ/2` と `plus/3` を `system` に移すこと。`library(between)` にあるので、スイートの `predicate_property(succ(_,_), built_in)` を条件にした 5 件は別欄に残る（`is` を通るので、値はこのフェーズで上限なしになる）。
- スイートのランナーを広げること。`- test` の形の「実装依存の結果」5 件（`iso_bitwise_and_2_04` ほか、2 の補数の表現）を数えるには許可表が要り、別の仕事である。
- 多倍長の数を 10 進の字面にする処理や読む処理を、2 乗より速いアルゴリズムにすること。2 乗のままにし、所要時間を `bench.md` に書く（私の判断）。
- 準拠スイートの今の不合格 35 件の掘り下げ（フェーズ9と同じ扱い）。
- このフェーズでの公開。公開は所有者の依頼で別に行う。
- Cell の表現と IR を、ADR 0025 に書く範囲の外で変えること。

## アンチ目標（技術的に動いても、これをやったら失敗）

- Big が黙って誤って扱われること。調べた結果、今のコードには、新しいタグを黙って誤る場所がある（付録 A の 6）。
  - `assertz` された Big が `true` になる（`compile.cpp` の `data_leaf` の既定の枝）。
  - writer が何も書かない（`writer.cpp` の `render`）。
  - `integer_value` が先頭の 1 語だけを読んで値を切り詰める（`heap.cpp`）。
  - GC が Big を即値として扱い、印を付けずに掃除する（`gc_mark.cpp`、`gc_compact.cpp`）。

  どの道も、Big を通すテスト（往復、または値の比較）で守る。
- 期待値を実装に合わせて書き替えること。仕様の変更で期待値が変わるテスト（下の「仕様の変更で期待値が変わるテスト」）は、この文書の承認を根拠に、実装より先に新しい期待値へ直す（赤にしてから実装で緑にする）。それ以外の期待値は 1 つも変えない。
- int64 の速さを落とすこと。例として、評価の値を毎回ヒープの外に確保する形にすることを含む。
- 1 つのタスクの途中で、Big を作る側（字句解析、算術、`number_codes/2`）を、Big を受ける側より先に入れること。各セッションの終わりに ctest が通らなくなる。
- 準拠率を守るために、スイートの件を別欄へ移したり、`unbounded/` の一部の件だけを範囲に入れたりすること。

## 制約

- AGENTS.md のとおり。
  - C++20 標準ライブラリと CMake だけを使う。`__int128` やコンパイラの組み込み（`_umul128`、`__builtin_clzll` など）は使わない。64 ビットどうしの積は 32 ビットに割って計算する（`<bit>` の `std::countl_zero` などは標準なので使ってよい）。
  - 関数は 60 行以内、ファイルは 800 行以内にする。
  - 生の `new`/`delete`、`reinterpret_cast`、C 配列、マクロを使わない。
  - エラーは `throw_error`/`make_*_error` の族だけで投げる。
  - OS 依存のコードを `src/platform/` の外に書かない。
- 規格の Examples と、自前のテストを先に `tests/pl/` に書いてから実装する（TDD）。エラーのテストは `Formal` だけを比べる。
- 識別子は英語、コメントとコミットメッセージは日本語で書く。文書は英語が正で、`*.ja.md` を同じコミットで直す。
- 変えにくい決定（Big の箱の形、IR の多倍長定数、複製の節点、`mosac` の定数）は ADR 0025 に先に書き、所有者の承認を得てからコードに入る。
- MSVC、GCC、Clang の 3 つで、`-Wall -Wextra -Wpedantic`（`/W4`）の警告ゼロを守る。

## 所有者への問い（iso-notes に「A か B か」で書く。確認までは既定で進める）

リポジトリには 13211-1 の本文が無い（付録 C の 5）。下の 2 つは規格の字面で決まらないので、iso-notes に節番号つきで書き、所有者に問う。

1. **`bounded` が `false` のときの `max_integer` と `min_integer`（7.11.1.2、7.11.1.3）**
   - A: 旗を残し、int64 の上限と下限を返す。SWI-Prolog と Scryer Prolog はこの形である。
   - B: 旗を無くす。`current_prolog_flag(max_integer, X)` は失敗する。
   - 既定は A（私の判断）。理由は 2 つある。スイートは `bounded` が `true` のときにしかこの 2 つを読まない（`iso_current_prolog_flag_2_03`・`04`）。`tests/pl/read.pl` の補助述語 `m1_min` を使う 22 件と `flags.pl` がこの 2 つを読む。フェーズ9の設計の「7.11.1.2 により外す」は本文を確かめた記述ではないので、根拠にしない。A なら ADR 0001 の「`max_integer` は 2^63−1 のまま」は文として残り、意味を「int64 の境界」と書き足す。
2. **double の範囲を越える整数と浮動小数点数の比較、`min/2` と `max/2`（8.7、9.3.8、9.3.9）**
   - A: 今の規則（整数を浮動小数点数に変えて比べる。9.3.8 の決定 B）のまま進める。変えられない値は `evaluation_error(float_overflow)` にする。
   - B: 正確に比べる。`2^2000 > 1.0` は真になる。
   - 既定は A（私の判断）。理由は 2 つある。スイートの `lgt_unbounded_float_03`（`float(7^7^7)`）は `float_overflow` を期待し、浮動小数点数を混ぜる 4 件（`13.42e+33` との `+`、`-`、`*`、`/`）は double の範囲の中にある。また、9.3.8 の決定 B と同じ規則になる。B なら `cmp_nums` と `min`/`max` に正確な比較が要る。

## 規模と順序

13 タスク。AGENTS.md の「1 セッション 1 タスク」に合わせて切る。Big を受ける側（タスク 3〜4）を、Big を作る側（タスク 5〜8）より先に入れる。こうすれば、どのセッションの終わりにも ctest が通る。順は私の判断である。

1. ADR 0025（Big の箱、IR の多倍長定数、複製の節点、`mosac` の定数、索引の鍵、ADR 0001 と ADR 0002 への追記）、iso-notes の問い 2 つ、SPEC 第9節の 10 行目と第1節の非目標の書き替え（日英）。文書だけ。所有者の承認を得てからタスク 2 に入る。
2. 値の型 `BigInt`（`src/core/bigint.hpp`、`bigint.cpp`。800 行を越えるなら `bigint_div.cpp` と `bigint_text.cpp` に分ける）と、その C++ 単体テスト。マシンには触らない。
3. ヒープの Big の箱と、それを受ける側の 1 つ目: GC の印付けと移動、単一化、標準順序、`==` と変種、writer、第 1 引数の索引の鍵、型の検査（`integer/1`、`number/1`、`atomic/1`）。`integer_value` が Big を黙って切り詰めない形にする。評価で Big を読んだら、タスク 6 までの暫定として `evaluation_error(int_overflow)` にする（今の「int64 を越えた」と同じ見え方）。テストは C++ のハーネスで Big を直に作って行う。
4. 受ける側の 2 つ目: 項から IR へ（`assertz`、consult、ゴール）、IR から項へ（`materialize`）、節の指紋、`clause/2`、`ExportedTerm`（`copy_term`、`nb_setval`、`throw`、`bagof` の鍵、`call_cleanup`）、`CopyNode`（`findall`）、`--heap-max` の勘定、「呼び出せる項」の判定 4 箇所。`mosac` はタスク 10 まで、Big の定数を持つ節を `native_eligible` から外す（ADR 0022 の共有項と同じ扱い）。
5. Big を作る側の 1 つ目: 字句解析と reader、`number_codes/2`、`number_chars/2`、`atom_number/2`。2^63 以上の字面を Big として読む。`syntax_error(integer_overflow)` と `needs_minus` を無くす。`read.pl`、`atoms.pl`、`lexer_test.cpp` の期待値を直す（仕様の変更）。
6. 算術 1: 評価の値の型、`+ - * // rem div mod abs sign min max`、比較 6 つ、整数と浮動小数点数の変換（`float/1`、`/`、浮動小数点数から整数への `truncate`、`round`、`ceiling`、`floor`、`integer`）、結果の大きさの事前の見積もり。
7. 算術 2: `gcd`、`msb`、`^`、`>>`、`<<`、`/\`、`\/`、`xor`、`\`（2 の補数で無限に符号を延ばす意味）、`succ_or_zero`。`arith.pl` と `lib_between.pl` の `int_overflow` の期待値 14 件と、`machine_test.cpp` の 1 件を直す（仕様の変更）。
8. 旗と準拠スイート: `bounded` を `false` にする。`max_integer` と `min_integer` は問い 1 の決定に従う。`excluded_tree` から `unbounded` を外す。ランナーの数の扱い（`lgt_shape.cpp`、`lgt_copy.cpp`、`lgt_approx.cpp`、`suite.cpp`、`lgt_judge.cpp`）を Big に広げる。`conformance_report_test.cpp` の固定値と `flags.pl` を直す。`docs/conformance.md` を作り直す。
9. 整数の引数を取る組み込み述語（付録 B の 5）: Big を型の検査で通し、範囲の外の正の値は「大きすぎる int64」と同じ結果にし、負の値は定義域エラーにする。黙った `static_cast` を無くす（`sub_atom/5`、`length/2`、`set_stream_position/2`、`halt/1`、`op/3`）。`numbervars/3` の `representation_error(max_integer)` を無くし、`'$VAR'(N)` の書き出しを Big に広げる。`library(format)` の `~d` と桁の読み取りを確かめる。
10. `mosac`: Big の定数を頭部と本体に出す。索引の鍵の検査を足す。`runtime.hpp` に箱を作る関数を足す。タスク 4 の `native_eligible` の除外を外す。`tests/mosac/` に Big の字面を持つプログラムを 1 つ足す。
11. GC と資源: `MOSA_GC_STRESS` の下で `tests/pl/bigint.pl` を通す。Big を作り続けるループでメモリが頭打ちになることを確かめる（フェーズ7の試験と同じ形）。巨大な結果（`1 << (1 << 62)`、`2^(2^62)`）が確保の前に止まることを確かめる。
12. 速さ: `bench.md` に Big の行を足す（`7^7^7`、1000 の階乗とその書き出し）。int64 の道が遅くなっていないことを、0.1.4 と同じ機械で測って確かめる。
13. 文書を閉じる: CHANGELOG（日英）、マニュアルの第6章・第9章・第14章（日英。`check: limits` の塊を含む）、SPEC 第3節の 5 と 7 と処理系定義の表（日英）、README の準拠率、iso-notes、STATUS。CI の全ジョブが通ることを確かめ、フェーズを閉じるかを所有者に問う。

タスク 6〜7 と 9 は、所有者の判断で順を入れ替えてよい。タスク 8 は 6〜7 の後に置く（スイートの件が算術を使う）。タスク 10 は 4 と 5 の後ならいつでもよい。

## 決めたこと

### 1. ヒープの箱（ADR 0025 に書く）

- タグ 6（Big）の Cell は、ヒープの頭の語を指す。頭の語には、大きさの語数 n（n ≥ 1）と符号を入れる。ビットの配置は ADR に書く。頭の語の下位 3 ビットは 0 にして、関数子の語と同じく「項ではない語」とする（私の判断）。
- 頭の語の後に、絶対値を 64 ビットの語 n 個で置く。下位の語が先で、最上位の語は 0 でない。符号は「符号と絶対値」で持つ（私の判断）。2 の補数より積と商が書きやすい。ビット演算は計算の間だけ 2 の補数に読み替える。
- 正規形の規則: int64 に収まる値は決して Big にしない（Int か IntBox にする）。Big の値は必ず int64 の外にある。こうしておけば、2 つの Big が等しいのは、語数と符号と全語が等しいときに限られる。比較も単一化も、語の比較で済む。ADR 0002 の「Big を int64 の箱にしない」はこの規則で守る。
- GC: `Marker::mark_value` は Big の頭の語を読んで n+1 語を Raw として印を付ける。`Rewriter::relocate` は Big を即値の扱いから外し、指す先を移す（今の `gc_mark.cpp:78` と `gc_compact.cpp:74` は Big を Atom・Int と同じ即値の扱いにしている。ここを直す）。移動では Raw の語をそのまま写す（今のまま）。
- 箱は葉である。ADR 0022 の共有項の候補にはならない（重み 1 の葉。今の Float と IntBox と同じ）。

### 2. 計算の値の型 `BigInt`（`src/core/`）

- ヒープの外の値の型。絶対値の語の並び（`std::vector`）と符号を持つ。語の幅（32 か 64 か）は実装者に任せる。ヒープの形（64 ビットの語）との変換はシフトで行い、`reinterpret_cast` は使わない。
- 演算: 加減、乗算、商と剰余（切り捨てと床の両方）、比較、シフト、2 の補数の意味でのビット演算、`gcd`、累乗、10 進と 2・8・16 進の字面との変換、double への正しい丸め（最近接偶数）、有限の double からの正確な変換。
- 乗算は、ある大きさ（実装者が測って決める）より上で Karatsuba にする（私の判断）。スイートの 4 件（`float_03`、`exp_01`、`log_01`、`sqrt_01`）が `7^7^7`（約 230 万ビット）を計算する。2 乗の掛け算では 1 件に数秒かかる見込みである。
- 除算は Knuth の Algorithm D（私の判断）。字面の変換は 10^19 か 10^9 ずつの 2 乗のアルゴリズムにする（非目標を参照）。
- このファイルはマシンにもヒープにも依存しない。`tests/cpp/bigint_test.cpp` で、境界の値（2^63±1、2^64、−2^63、0 をまたぐ減算、正規化）と、Karatsuba と 2 乗の掛け算の一致を確かめる。

### 3. 評価（`src/builtins/arith.cpp`）

- `Num` に Big の種類を足す。int64 と double の道は今のまま、確保なしで通す。Big を持つときだけヒープの外の値を持つ。正確な形（`std::variant` か、種類と欄の組か）は実装者に任せる。
- int64 の演算があふれたら、今の `int_overflow` の代わりに Big で計算し直す（`add_i`、`sub_i`、`mul_i` の失敗の道）。結果は正規形の規則で Int、IntBox、Big のどれかの Cell にする。
- 評価の値がヒープの語を直に指したまま確保をしないこと。確保は GC を起こし、ヒープを動かす。`number_of` で Big を読むときは、語を `BigInt` に写す（私の判断。写さずに済ませるなら、確保をまたがないことを実装者が示す）。
- 結果の大きさの事前の見積もり: `<<`、`^`、`*` は、計算の前に結果の語数を見積もる。ヒープに入らないなら、計算も確保もせずに、ヒープの上限と同じ扱い（`ResourceLimit`。今のヒープの上限と同じく問い合わせを止める）にする（私の判断）。`1 << (1 << 62)` や `2^(2^62)` は確保の前に止まる。`7^7^7`（約 36,000 語、約 290 KB）は既定の `--heap-max` の中に収まる。
- 浮動小数点数との混合: 整数を double に変える（問い 2 の既定 A）。Big の変換は正しい丸めで行う。double の範囲を越えたら `evaluation_error(float_overflow)` にする（スイートの `float_03` と `exp_01`）。`/` は今と同じく、整数どうしでも浮動小数点数を返す。`**` も今と同じく、いつも浮動小数点数である。
- 浮動小数点数から整数へ（`truncate`、`round`、`ceiling`、`floor`、`integer`）: 有限の値は、正確な整数（必要なら Big）になる。例として `round(1.0e32)` は 100000000000000005366162204393472 になる。無限大と NaN は、今と同じエラーのままにする（私の判断）。
- 比較 6 つ: 整数どうしは、Big を含めて正確に比べる。混合は上の規則に従う。
- `rt::arith_is` と `rt::arith_compare` はこのファイルの入口なので、`mosac` が訳したコードも同じ道を通る（生成コードに算術の展開は無い）。

### 4. 字句解析と reader

- 数字の並びが int64 に収まらなければ、トークンは Big の値を持つ。持ち方は実装者に任せる（`lexer.hpp` の `Token`）。`0b`、`0o`、`0x`、`_` の区切り、`0'c` の規則は変えない。
- `syntax_error(integer_overflow)` は無くなる。`messages_en.pl` と `messages_ja.pl` の鍵は、使われなくなったら消す。2^63 だけに付けていた `needs_minus` の印も無くなる。`- 9223372036854775808` は、Big の 2^63 の符号を反転して正規化し、IntBox の −2^63 になる。
- `number_codes/2`、`number_chars/2`、`atom_number/2` は今と同じく字句解析を使う。`strip_sign` の 2^63 の特例は無くなる。
- フェーズ9の枠の reader の形は変えない。

### 5. writer

- Big は 10 進で書く。負の値は `-` を先に付ける。`unsigned_number` と `leftmost_unsigned_number` の空白の判断（`- 1` と `-1`、`a- 1` など）は、int64 と同じ規則にする。
- `'$VAR'(N)` の `N` が Big のときも、字母と番号を Big の商と剰余で作る（`numbervar_text`）。

### 6. 複製と IR（ADR 0025 に書く）

- IR に多倍長の定数を足す。形（IR の新しいタグに、節ごとの定数表への添字を持たせるか、`IrTerm` に値を持たせるか）は ADR 0025 で決める。条件は次のとおり。
  - 節の指紋（`native.cpp` の `hash_term`）が値を含むこと。そうしないとインタプリタと `mosac` が食い違う。
  - `ir_budget` と `--heap-max` の勘定に絶対値の語を含めること。
  - 「呼び出せる項」の判定 4 箇所（`engine.cpp` の `body_callable`、`database.cpp` の `goal_form`、`codegen/check.cpp`、`load_clauses.cpp`）が Big を呼び出せない項とすること。今の判定は「Int でも Float でもない」なので、新しいタグを呼び出せる項と誤る。
  - `materialize` と `clause/2` と `ir.cpp` の写しが値を落とさないこと。
- `ExportedTerm`（`catch.cpp`）と `CopyNode`（`solutions.cpp`）にも Big の種類を足す。今の最後の `else` は、それぞれ Str と Float を仮定している。ここに Big を足す。ヒープの外の語は `footprint` と `tree_bytes` の勘定に入れる。
- 第 1 引数の索引: Big の鍵は値から作る（今の `KeyKind` に 1 つ足し、絶対値の語と符号の FNV にする。私の判断）。等しい値が同じ鍵になることを、正規形の規則が保証する。鍵が衝突しても、単一化で正しい答えに戻る。`mosac` の `key_test` も同じ鍵を使う。

### 7. 旗

- `bounded` は `false`。書き換えはできない（今のまま）。
- `max_integer` と `min_integer` は、問い 1 の決定に従う（既定 A: int64 の境界）。
- `max_tagged_integer`（2^60−1、評価可能関数子）は今のまま残す。

### 8. 整数の引数を取る組み込み述語

規則は 1 つにする（私の判断）。型の検査は Big を整数として通す。範囲の外の正の Big は、その述語が「大きすぎる int64」に対して今していることと同じ結果にする。負の Big は、負の int64 と同じ定義域エラーにする。黙って切り詰めることはしない。スイートの件はこの規則に合う。

- `arg/3`: 正の Big は失敗し（`arg_01`）、負の Big は `domain_error(not_less_than_zero, N)` になる（`arg_04`）。
- `functor/3`: Big の項数は `representation_error(max_arity)` になる（`functor_03`）。
- `char_code/2` などの文字コード: Big は `representation_error(character_code)` になる。
- `length/2`: Big の長さはヒープの上限で止まる。
- `sub_atom/5`、`atom_length/2`: 正の Big は失敗する。
- `set_stream_position/2`、`halt/1`、`op/3` の優先順位は、今の範囲の検査に Big を通す。

今「大きすぎる int64」を黙って `static_cast` しているところ（付録 B の 5）は、先に範囲を検査する。その範囲の外の扱いを規格が決めていない述語は、iso-notes に書き、所有者に問う（今の動きを既定にする）。

### 9. `mosac`

- 頭部と本体の Big の定数は、絶対値の語の配列（`static constexpr std::array<std::uint64_t, N>`）と、`runtime.hpp` の箱を作る関数で出す（私の判断）。名前と引数は ADR 0025 で決める。`build_words` は Big を n+1 語と数える（今の `default:` は 0 語と数え、確保が足りなくなる）。
- `emit.cpp` の `same_key` と `key_test`、`emit_head.cpp` の `match`、`place_leaf`、`build_words` の `default:` の枝に Big の場合を足す。
- 生成コードは算術を実行時ライブラリに任せているので、算術の生成の変更は無い。

### 10. 準拠スイートと完了条件

数は、どれもコードを読んで数えた見積もりである。何も実行していない。タスク 8 で `mosatest --suite` を走らせて確かめる。

- `unbounded/` は 111 件（`test/2` が 98 件、`test/3` が 13 件）。このうち 9 件は条件で別欄に行く（`succ`・`plus` の 5 件、`max_arity` の 4 件）。分母に入るのは約 102 件である（フェーズ9の設計の「111 件」は条件を数えていなかった）。
- `bounded` を `false` にすると、今通っている範囲内の 15 件が条件で別欄に移る（`is_2` の 8 件と、`div_2` などの 7 件。どれも `int_overflow` を期待する件）。`lgt_msb_1_07`・`08` の 2 件が分母に入る。
- 分母は、`unbounded/` を入れる前で約 2172 件、入れた後で約 2274 件になる。

完了条件（機械で確かめられるものだけ。SPEC 第9節）:

1. `tests/pl/bigint.pl`（新しいファイル。評価可能関数子ごと、組み込み述語ごと、往復、複製、例外の行）と、直したテストが `mosa` で通る。`mosatest --diff` の差分が 0（`japanese.pl` を含む）。
2. `mosatest --suite` が `unbounded/` を範囲に入れる。そこで動く件がすべて通るか、通らない件が iso-notes に解釈の違いとして書かれ、所有者が決めている。合格率は、実際の分母で 98.4% を下回らない。`docs/conformance.md` を作り直す。
3. `MOSA_GC_STRESS` の下で `tests/pl/bigint.pl` が通る。Big を作り続けるループでメモリが頭打ちになる。`1 << (1 << 62)` が確保の前に止まる。
4. ctest がすべて通る。CI の全ジョブ（今は 10）が通る。
5. 0.1.4 と同じ機械で、インタプリタの `nrev` と `queens` が 0.1.4 より 5% を越えて遅くならない。`mosac` の `nrev` はインタプリタの 3 倍以上の速さのまま。測った値を `bench.md` に書く。

SPEC 第9節の表に 10 行目を足す（タスク 1）。内容は「多倍長整数: Big の箱（ADR 0025）、上限の無い算術と読み書き、`bounded` が `false`」で、完了条件は上の 1〜3 と 5 である。第1節の非目標から多倍長整数を外し、「GMP は使わない」は残す（タスク 1）。第3節の 5 と 7 と処理系定義の表は、`bounded` を `false` にしたあと、タスク 13 で書き替える。

## 仕様の変更で期待値が変わるテスト

この文書の承認を根拠に、表のタスクで実装より先に直す。数は、コードを読んで数えた見積もりである。実装者は、このほかに期待値が変わるテストを見つけたら、直す前に所有者に問う。

| 何が変わるか | テスト | タスク |
| --- | --- | --- |
| `evaluation_error(int_overflow)` が値になる（14 件） | `tests/pl/arith.pl` の 13・74・75・77〜79・87・128・129・134・135・168 行、`lib_between.pl` の 29・43 行、`tests/cpp/machine_test.cpp` の 446 行 | 6・7 |
| `syntax_error(integer_overflow)` が読める（17 件） | `tests/pl/read.pl` の 135〜143・159・160・162・166・167・173・174 行（最後の 2 行は `syntax_error(max_arity)` になる見込み）、`atoms.pl` の 87 行、`tests/cpp/lexer_test.cpp` の 128〜145 行 | 5 |
| `bounded` が `false`、`max_integer` と `min_integer` が問い 1 に従う | `tests/pl/flags.pl` の 1・3・4 行、`read.pl` の `m1_min` と 145 行（問い 1 が B のとき）、`lib_format.pl` の 26・27 行 | 8 |
| `unbounded` が範囲に入る | `tests/cpp/conformance_report_test.cpp` の 164・165・351・374 行 | 8 |
| `max+1` が `int_overflow` にならない | マニュアルの `<!-- check: limits -->`（`docs/manual/en/06-arithmetic.md` の 182〜185 行、`ja` の 155〜158 行） | 13（実際に壊れるのはタスク 6。そこで直す） |
| `numbervars/3` の `representation_error(max_integer)` が無くなる | iso-notes の 460 行が指すテスト | 9 |

`read.pl` の 147・163〜165 行（「あふれのあとに読み直せる」テスト）は通り続けるが、何も確かめなくなる。タスク 5 で、Big を読み直すテストに置き替える。

## 実装者に任せること

- 頭の語のビットの配置と、`BigInt` の語の幅。どちらも ADR 0025 の範囲で決める。
- `Num` の正確な形。
- Karatsuba の閾値。
- IR の多倍長定数の形。どちらの形にするかを ADR 0025 で選ぶ。
- 字句解析のトークンが Big をどう持つか。
- タスク 9 の、述語ごとの範囲の外の扱い。規格の字面で決まらないものは iso-notes に書く。
- `tests/pl/bigint.pl` の行の選び方。各評価可能関数子について、境界（2^63−1、2^63、2^64、−2^63、−2^63−1）と、大きな値と、符号の組み合わせを入れる。

## リスク

- 新しいタグを黙って誤る場所の見落とし。守りは 3 つある。付録 A の 6 の一覧を各タスクで潰すこと。`-Wswitch` で `default:` の無い `switch` を見つけること。往復のテスト（読む → 書く → 読む、`assertz` → `clause/2`、`findall`、`copy_term`、`nb_setval`、`throw` と `catch`、`mosac`）である。
- GC の取りこぼし。Big を指す Cell が残っているのに箱を掃除すると、項が壊れる。フェーズ7と同じく、タスク 3 と 11 のレビュアーは opus で、`MOSA_GC_STRESS` の下で通すことをレビューの条件にする。
- 評価の値がヒープを直に指したまま、確保で GC が起きること（上の 3）。
- `7^7^7` の時間。Karatsuba で 1 件 1 秒未満を見込む。Debug の構成の CI で準拠スイートの時間が延びたら、所要時間を `bench.md` に書く。件を外すことはしない。
- int64 の道が遅くなること。`Num` が大きくなり、`std::vector<Num>` の写しの費用が増える。タスク 6 のレビューで `bench` を測る。
- 数は見積もりである（上の 10）。タスク 8 で実数に置き替え、この文書との違いを STATUS に書く。

## 付録 A: 項の表現の調べ（2026-11-12、読んだだけ）

1. 定義: `include/mosa/cell.hpp:10-19` の `Tag`（`Big=6` は 2 つの GC の行のほかで使われていない）。`Heap::integer`（`src/core/heap.cpp:127-135`）、`Heap::floating`（`heap.cpp:145-149`）は頭の語なしの 1 語。即値の範囲は `heap.cpp:15-16`。`integer_value`（`heap.cpp:137-143`）は Int 以外を先頭の 1 語として読む。2^60 の境界は `heap.cpp:15-16`、`runtime.hpp:73-75,109-112`、`codegen/emit.cpp:472`、`arith.cpp:518` の 4 箇所にある。
2. 単一化と比較:
   - 単一化は `src/core/unify.cpp:108-121`（`:121` で他のタグは偽）と、`native.cpp:238` の `rt::unify`。
   - 標準順序は `src/builtins/order.cpp:21-27` の `rank_of`（他のタグは複合項の順位 4）と、`:64-91` の `compare_scalar`。
   - `==` は `src/write/term_equal.cpp:80-94`。
   - スイートの判定は `lgt_shape.cpp:33-39,118-126` と `lgt_judge.cpp:222-232`。
   - 索引は `src/machine/index.cpp:36-48,175-184`（どちらも `default:`）、`clause_index.hpp:17` の `KeyKind`、`compile.cpp:640-649` の `set_key`。
   - 指紋は `native.cpp:40-53` の `hash_term`。
   - 複製は `internal.hpp:252-265` の `ExportedTerm::Node`、`catch.cpp:39-48,136-138`、`solutions_internal.hpp:14-23`、`solutions.cpp:384-397`、`solutions_answer.cpp:123-124`。
   - IR は `compile.cpp:198-225`（`:220` の既定の枝が `true`）、`:746-747`（頭部の検査）、`include/mosa/ir.hpp:10-41`、`src/core/ir.cpp:26-27,81-82,95-96`、`clause_term.cpp:116-117`、`materialize.cpp:66-87`。
   - 共有項（ADR 0022）は `compile.cpp:308,331-335`。
3. GC: `gc_mark.cpp:76-97`（`:78` が Big を即値として扱う、`:91-94` が箱を 1 語の Raw とする）、`gc_compact.cpp:72-78,103-127`、`gc.hpp:115-120` の `WordKind`。
4. `mosac`: `emit_head.cpp:41-53`（`build_words`）、`:61-65`、`:88-97`（`place_leaf`）、`:213-227`（`match`）、`:277`、`emit.cpp:39-43,158-182,425-439`、`runtime.hpp:45,73,109-116`。
5. その他:
   - 項の書き出しは `writer.cpp:400-406,416-428,472-481`（`:481` で他のタグは何も書かない）と `answer_text.cpp:66-87`。
   - 項の字面は `term_text.cpp:45,60`。
   - 大域変数は `global_vars.cpp:39,59`。
   - 勘定は `solutions.cpp:33,494`、`state_count.cpp:32-45`、`compile.cpp:677-681`。
6. 黙って誤る場所:
   - 最後の枝が別のタグを仮定するもの: `catch.cpp:48`、`solutions.cpp:392`、`order.cpp:26`、`terms.cpp:62`、`compile.cpp:220`、`heap.cpp:142`、`lgt_copy.cpp:30`、`suite.cpp:155`、`writer.cpp:481`、`unify.cpp:121`、`term_equal.cpp:94`。
   - `default:` のある `switch`: `gc_mark.cpp:95`、`index.cpp:46,182`、`emit.cpp:163,178`、`emit_head.cpp:50,95,226`。
   - 「呼び出せる項」の判定: `engine.cpp:380`、`database.cpp:197`、`codegen/check.cpp:49`、`load_clauses.cpp:60`。
   - `is_int`（Int か IntBox）の写し約 30 箇所（付録 B の 0）。

## 付録 B: 算術と字面の調べ（2026-11-12、読んだだけ）

0. `is_int` の写し: `types.cpp:20,24,182,190`、`terms.cpp:23,26`、`atoms_number.cpp:31,33`、`atoms_sub.cpp:26`、`atoms.cpp:36`、`unify.cpp:48`、`term_equal.cpp:16`、`qualify.cpp:24`、`format_float.cpp:25`、`operators.cpp:36`、`lists.cpp:32`、`bytes.cpp:55`、`stream_prop.cpp:51`、`char_width.cpp:11`、`writer.cpp:402,420`、`flags.cpp:122,165`、`roundtrip.cpp:97`、`read_pred.cpp:120`、`write_pred.cpp:129`、`streams.cpp:347,412`、`chars.cpp:176,214,236`、`database.cpp:561`、`database_scan.cpp:179`、`control.cpp:201`、`catch.cpp:280`、`load_directive.cpp:83,108`、`lgt_io.cpp:155`、`lgt_judge.cpp:45`、`lgt_shape.cpp:23`、`lgt_approx.cpp:14,22`、`suite.cpp:149`。`Tag::Int` だけを見るもの: `meta.cpp:34`、`heap.cpp:138`、`index.cpp:39-41`。
1. 評価:
   - 値の型は `arith.cpp:29-38`（`struct Num { bool flt; int64 ival; double fval; }`）。
   - 葉は `number_of`（`:104-118`）、結果は `num_cell`（`:664-667`）。
   - あふれの補助は `add_i`、`sub_i`、`mul_i`（`:63-96`）と `fail_eval`（`:44-46`）。
   - 関数子ごと:
     - 単項の `-`: `:478-487`。
     - `+ - *`: `eval_int_bin` の `:260-301`。
     - `/`: `:222-227`。
     - `// rem div mod`: `:264-298`。
     - `^`: `int_hat` の `:330-338`、`pow_int` の `:303-315`、`negative_hat` の `:317-328`。
     - `**`: `:228-245`、`integer_exponent` の `:160-171`、`negative_base_pow` の `:173-184`。
     - `abs`: `:396-404`。`sign`: `:405-415`。`min`・`max`: `:416-428`。
     - `gcd`: `:373-390`。`msb`: `:438-451`。`succ_or_zero`: `:452-461`。
     - ビット演算: `:462-471`。シフト: `shift_bits` の `:186-212`。
     - 浮動小数点数の丸め: `arith_float.cpp:34-47,56-71`。`float`: `:138-149`。
   - 比較は `cmp_nums` の `:669-685`、`run_cmp` の `:687-695`、`run_arith` の `:699-725`。
   - 生成コードの入口は `rt::arith_is` の `:752-758` と `rt::arith_compare` の `:760-772`。
   - `eval_int_expression` は `:727-735`（呼び手は `tab/2`、`chars.cpp:400-413`）。
2. 字句解析:
   - 数字の読み取りは `lexer_literal.cpp:28-34,230-255`。
   - あふれの検出は `:170,198`、`integer_token` は `:175-185`。
   - `scan_number` は `:132-143`、`scan_radix` は `:187-200`、`scan_char_code` は `:202-228`。
   - `Token` は `lexer.hpp:19,26-28,79-85`。
   - `needs_minus` の検査は `reader.cpp:105,198-202,228-237` と `reader_bracket.cpp:93`。
   - `number_codes` 系は `atoms_number.cpp:102-112,173-195,202-300`。
3. 書き出し:
   - `writer.cpp:472-476`（`std::to_string`）。
   - `library(format)`: `lib/format.pl:106-111`（桁の読み取り）、`:124-129`、`:159-165`（`~d`）、`:210-216`（`~e`、`~f`）、`:220-243`。
   - `'$format_float'`: `format_float.cpp:36-37,70-76`。
4. 旗: `flags.cpp:42-46,86-114,121-133,162-168,193-206`。
5. 整数の引数:
   - 黙った `static_cast` があるもの: `sub_atom` の `atoms_sub.cpp:85-88`、`length` の `lists.cpp:224-228`、`set_stream_position` の `stream_prop.cpp:323`、`halt` の `control.cpp:206` と `catch.cpp:284`、`op` の `operators.cpp:349`。
   - `arg`: `terms.cpp:154-165`。`functor`: `terms.cpp:123-135`。`numbervars`: `terms.cpp:331-358`。
   - 文字コード: `atoms.cpp:148,197,297-312`、`chars.cpp:212-245`、`term_text.cpp:45-48`、`streams.cpp:347-352`。
   - バイト: `bytes.cpp:112-128`。`sort/4` の鍵: `lists.cpp:86-93,121`。`op/3` の優先順位: `operators.cpp:84-86,143-154`。
   - ストリームの項: `chars.cpp:176-181`、`bytes.cpp:78-84`、`read_pred.cpp:120-122`、`write_pred.cpp:129-131`、`streams.cpp:412-414`、`stream_prop.cpp:73-79`。
   - 述語指示子: `database.cpp:561-572`、`qualify.cpp:140-143`、`database_scan.cpp:179-190`。
   - Prolog で書かれたもの: `lib/between.pl:5-61`、`lib/lists.pl:14-35,86-94`、`lib/japanese.pl:18-28`。

## 付録 C: テストとスイートの調べ（2026-11-12、読んだだけ。数は見積もり）

1. 範囲の表は `src/machine/lgt_suite.hpp` にある。
   - `kPredicateScope`（`:64-140`）、`kFunctionExcluded`（`:143-153`）、`excluded_tree`（`:155-160`。`unbounded` は `:156`）、`suite_in_scope`（`:169-185`）。
   - 範囲の外のディレクトリは動かさず、`test(` を含む行の数を別欄に出す（`iso_suite.cpp:39-66`）。
   - 件ごとの別欄は 2 箇所で決まる。`- test` と選ばれなかった `if` の枝（`lgt_file.cpp:283-299`）と、`condition(G)` が偽のとき（`:225-243`、`lgt_cond.cpp:182-199`）である。
   - 分母は合格と不合格の和（`iso_suite.cpp:229`）。
2. `unbounded/tests.lgt` は 517 行。
   - 補助の事実は `small(42)`、`big(1844674407370909797907654848955145546336677616)`、`real(1.0e+32)` である。
   - 比較の件は Big と小さい整数を比べる。浮動小数点数を混ぜるのは `13.42e+33` との 4 件と、`real/1` の丸め 8 件だけである。
3. `bounded` で条件が変わる範囲内の件:
   - 別欄へ移るもの: `is_2` の `iso_is_2_31〜35`、`lgt_is_2_44〜46`。`lgt_div_2_10`、`lgt_integer_division_2_12`、`lgt_integer_power_2_18`、`lgt_minus_1_07`、`lgt_minus_2_09`、`lgt_multiplication_2_13`、`lgt_plus_2_09`。
   - 分母に入るもの: `lgt_msb_1_07`・`08`。
   - 別の枝が動くもの: `iso_current_prolog_flag_2_02〜04`、`sics_read_term_3_23`・`24`。
4. 書き替える文書:
   - SPEC: `docs/SPEC.md:24,64,66,212,365` と `SPEC.ja.md:25,65,67,211,362`。
   - 元の指示書の 62 行。
   - マニュアル（en）: `06-arithmetic.md:178-185`、`09-errors.md:149`、`14-reference.md:135-137,157,543-544`。
   - マニュアル（ja）: `06-arithmetic.md:153-158`、`09-errors.md:121`、`14-reference.md:123-125,145,516`。
   - README の準拠率: `README.md:96,198`、`README.ja.md:98,198`。
   - iso-notes: `:20,460,606,660,684-694`。
   - CHANGELOG の 0.1.0 の既知の制限は、過去の記録なので直さない。新しい節を足す。
5. 規格の本文: リポジトリに 13211-1 の本文は無い。iso-notes にも `bounded`、`max_integer`、`min_integer` の節は無い（`numbervars` の 460 行だけ）。
