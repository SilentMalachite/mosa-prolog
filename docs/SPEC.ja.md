[English](SPEC.md) | 日本語

# Mosa Prolog 仕様書（SPEC）
この文書は [英語版](SPEC.md) の日本語訳である。食い違うときは英語版に従う。

指示書「ISO Prolog 処理系（C++20変換）— Claude Code Grok Build 向け指示書」の第 1〜9 節を写したもの。第 10・11 節（エージェントの作業ルール、リポジトリ構成）は `AGENTS.ja.md` にある。

## 1. 目的と非目標

この文書は、ISO/IEC 13211 準拠の Prolog 処理系（名称 Mosa Prolog、リポジトリ名 `mosa-prolog`）を Claude Code と Grok Build で実装するための共通仕様である。CLAUDE.md と AGENTS.md の両方からこの文書を参照すること（同じ内容を二重管理しない）。

名前の体系: コマンドは `mosa`（インタプリタ）、`mosac`（変換器）、`mosatest`（テストランナー）。ランタイムは `libmosa`、ヘッダは `include/mosa/`、C++ の名前空間は `mosa`、環境変数の接頭辞は `MOSA_`。表記は小文字の `mosa` で統一し、文章中の正式名称だけ Mosa Prolog と書く。

**目的（優先順）**

1. ISO/IEC 13211-1（コア）に準拠し、公開されている準拠テストスイートで高い合格率を出す。
2. 日本語をアトム・文字列・識別子・入出力・ソースファイルのすべてで自然に扱える（Unicode コードポイント単位）。
3. モジュールシステムを持つ（13211-2 の構文を基本に、SWI-Prolog 互換の実用的な意味論）。
4. Prolog プログラムをポータブルな C++20 ソースに変換し、ランタイムとリンクして単体実行ファイルを生成できる。変換・コンパイル・リンク・実行は mosac の 1 コマンドで完結し、利用者は C++ ツールチェーンを直接操作しない。
5. 対話的インタプリタ（REPL、`consult/1`、`assert` 系）も同じランタイム上で動く。変換結果とインタプリタの結果は常に一致する。

**非目標（今回はやらない）**

- 制約論理プログラミング（CLP(FD) など）、テーブリング、並列実行
- 多倍長整数（Phase 6 以降に任意機能として検討。それまでは 64bit 整数とオーバーフロー時の `evaluation_error`）
- GUI、IDE 連携、デバッガ以外のツール
- C++20 標準ライブラリと CMake 以外への依存（ICU、Boost、GMP は使わない。GMP は将来のオプション）
- Windows 専用／macOS 専用の API への依存。OS 依存コードは `src/platform/` に隔離し、1 ファイル以内に収める

## 2. 成果物と動作環境

成果物は 3 つの実行物と 1 つのライブラリ。すべて 1 つの CMake プロジェクトから生成する。

| 成果物 | 内容 | 形式 |
| --- | --- | --- |
| `libmosa` | 項表現・ユニフィケーション・ソルバ・組込述語・ストリーム・モジュール管理を含むランタイム | 静的ライブラリ（共有ライブラリは任意） |
| `mosa` | 対話的インタプリタ。REPL、`consult/1`、`-g Goal -t halt` 等のコマンドライン、スクリプト実行 | 実行ファイル |
| `mosac` | Prolog → C++20 変換器。Prolog ファイルから単体実行ファイルを作るまでを一括で行う（build / run / emit / dist / toolchain の各サブコマンド）。C++ ソースの出力は中間段階 | 実行ファイル |
| `mosatest` | テストランナー。Prolog で書かれたテストと準拠スイートを実行し、JUnit 形式で結果を出す | 実行ファイル（Prolog スクリプト + 薄いラッパ） |

**ビルド環境（3 OS すべてで CI を通すこと）**

| OS | コンパイラ | 備考 |
| --- | --- | --- |
| macOS（Apple Silicon） | Apple Clang 15 以上 | 開発の主環境 |
| Linux（Ubuntu 24.04） | GCC 13 以上、Clang 17 以上 | CI の基準環境 |
| Windows 11 | MSVC 2022（v17.8 以上） | `/utf-8` `/permissive-` `/Zc:__cplusplus` を必須とする |

- 言語規格は `-std=c++20` / `/std:c++20` 固定。コンパイラ拡張（GNU 拡張、`__int128`、`#pragma once` 以外の独自プラグマ）は使わない。
- 必須ツールは CMake 3.25 以上と上記コンパイラのみ。Ninja 推奨。
- 生成される C++ コードも同じ制約に従う。生成コードは `libmosa` のヘッダ（`mosa/runtime.hpp`）だけを include する。
- 警告は `-Wall -Wextra -Wpedantic`（MSVC は `/W4`）で、CI では警告をエラー扱いにする。
- 文字コードはソース・テスト・ドキュメントのすべてで UTF-8（BOM なし）。

## 3. 言語仕様の範囲（ISO/IEC 13211-1, 13211-2）

13211-1:1995 に Cor.1（2007）、Cor.2（2012）、Cor.3（2017）を加えたものを「コア」とし、矛盾がある場合は Corrigenda を優先する。規格と実装の判断が分かれる箇所は `docs/iso-notes.md` に条番号付きで記録する。

**コアで必ず実装するもの（13211-1 の章立て順）**

1. 構文（第 6 章）: トークン化、演算子優先順位、`op/3`、引用アトム、エスケープ列（`\x...\`、`\uXXXX`、`\UXXXXXXXX` を含む）、`0'c` 文字コード記法、`{}`/`[]` の扱い、`end_of_file`
2. 項の表現・ユニフィケーション（7.3）。`unify_with_occurs_check/2` を含む。デフォルトのユニフィケーションでは出現検査を行わない
3. 実行モデル（7.7）: 探索木、カット、`catch/3`・`throw/1`、`call/1..8` のカットの透過性（`call/N` 内のカットはローカル）
4. エラー体系（7.12）: `error(Formal, Context)` の形式とすべての Formal 項（`instantiation_error`、`uninstantiation_error/1`、`type_error/2`、`domain_error/2`、`existence_error/2`、`permission_error/3`、`representation_error/1`、`evaluation_error/1`、`resource_error/1`、`syntax_error/1`、`system_error`）
5. 算術（第 9 章）: 整数は 64bit 符号付き、浮動小数点は IEEE 754 倍精度。`max_integer`/`min_integer` フラグを提供し、オーバーフローは `evaluation_error(int_overflow)`。整数除算・剰余・ビット演算・三角関数・`copysign`、`truncate` 等 Cor.2 で追加された関数を含む
6. 組込述語（第 8 章）すべて。一覧と各段階は第 7 節に示す
7. フラグ（7.11）: `bounded`、`max_integer`、`min_integer`、`integer_rounding_function`、`char_conversion`、`debug`、`max_arity`、`unknown`、`double_quotes`。加えて実装定義フラグ `encoding`（常に `utf8`）、変更できない `version`（整数 `Major*10000 + Minor*100 + Patch`）、`japanese_punctuation`（第 4 節）、変更できない `dialect`（`mosa`）と `version_data`（`mosa(Major, Minor, Patch, [])`）
8. ディレクティブ（7.4.2）: `dynamic/1`、`discontiguous/1`、`multifile/1`、`op/3`、`char_conversion/2`、`initialization/1`、`set_prolog_flag/2`、`include/1`、`ensure_loaded/1`

**実装定義事項の決定**

| 事項 | 決定 |
| --- | --- |
| `max_arity` | 1024 |
| `double_quotes` の既定値 | `codes`（ISO 既定）。モジュール単位で変更可 |
| `unknown` の既定値 | `error` |
| 文字の範囲 | Unicode スカラー値 U+0000〜U+10FFFF（サロゲートは不可） |
| 文字列型 | 持たない。`"..."` は `double_quotes` フラグに従う（SWI の string 型は非目標） |
| 浮動小数点の書式 | 最短往復表現（`std::to_chars`）。必ず `.` を含み、指数は `e` |
| 整数の構文 | `0b`、`0o`、`0x`、`0'c`、`_` 区切り（Cor.3 では未規定だが許可） |

**ISO からの意図的な逸脱（すべてフラグまたは既定オフで、`iso` フラグ true のとき無効化される）**

- モジュール（第 5 節）。13211-2 は実装例が少なく矛盾も指摘されているため、構文のみ採り、意味論は SWI-Prolog を参照する
- 全角記号の受理（第 4 節、既定オフ）
- `format/2,3`、`between/3`、`succ/2`、`msort/2`、`forall/2`、`nb_getval/2` 等の広く使われる非 ISO 述語を `library(lists)`、`library(apply)` 等のモジュールに置く。`user` モジュールの既定では見えず、`use_module` で取り込む

## 4. 日本語・Unicode 対応の仕様

「文字」は常に Unicode スカラー値 1 個であり、バイトでも UTF-16 単位でもない。この原則をすべての組込述語と変換器に貫く。

**内部表現**

- アトム名は UTF-8（`std::string`）で保持し、アトムテーブルはバイト列で一意化する。正規化（NFC/NFD）は行わないので、`親` と合成列で書いた同形文字は別アトムになる。この点はマニュアルに明記する
- `atom_length/2`、`sub_atom/5`、`atom_chars/2`、`char_code/2`、`atom_codes/2` はコードポイント単位で動く。`sub_atom/5` の計算量を抑えるため、アトムに「ASCII のみ」フラグを持たせ、ASCII ならバイト位置をそのまま使う
- 1 文字のアトムは `char32_t` から直接生成できる高速パスを用意する

**字句解析（13211-1 6.5 の文字分類を Unicode に拡張）**

| ISO の分類 | Unicode での定義 | 例 |
| --- | --- | --- |
| 小文字（alpha の先頭になれる） | `Ll`、`Lo`、`Lm`、`Nl` および `Mn`/`Mc`（結合文字、先頭不可） | `親`、`たろう`、`カタカナ`、`ß` |
| 大文字（変数の先頭） | `Lu`、`Lt` | `X`、`Ä`、`Σ` |
| 数字 | ASCII の `0-9` のみ（全角数字は不可） | `123` |
| シンボル文字 | ISO の 12 文字 + `Sm`、`So`、`Pd`、`Po` のうち ASCII 外のもの | `+`、`→`、`≠`、`∧` |
| 空白 | `Zs`、`Zl`、`Zp` + 制御文字の空白 | 全角空白 U+3000 を含む |

- 日本語には大文字小文字がないので、`親(太郎, 一郎).` の `太郎` は**アトム**になる。日本語名の変数は `_太郎` のように下線で始める。この規則を README と REPL のヘルプの両方に書く
- 写像（`char_conversion/2`）の既定は恒等写像。`char_conversion` で全角→半角などを行わない
- 漢字 1 文字のアトムは引用符なしで書ける: `write(東)` は `東` を出力する。`writeq/1` は文字分類に従って引用の要否を判定する。つまり `writeq(東京)` は `東京`、`writeq('東 京')` は `'東 京'`
- Unicode の分類表は ICU に頼らず、`tools/gen_unicode_tables.py` が `UnicodeData.txt`（バージョンを `docs/unicode-version.md` に固定）から `src/unicode_tables.inc` を生成する。生成物はリポジトリにコミットし、ビルド時に Python を要求しない

**全角記号モード（フラグ `japanese_punctuation`、既定 false）**

真のとき、字句解析器は次の全角記号を半角と同一視する。ソースの可読性のためであり、`writeq/1` は常に半角で出力する。

| 全角 | 半角 |
| --- | --- |
| `（` `）` | `(` `)` |
| `［` `］` | `[` `]` |
| `，` `、` | `,` |
| `。` | `.`（節の終端のみ。小数点には使わない） |
| `：` | `:` |
| `「` `」` | `'` `'`（引用アトムの開始・終了。入れ子不可） |

**入出力**

- すべてのストリームは `encoding` オプションを持ち、既定は `utf8`。`text` モードのストリームは不正な UTF-8 列に対して `representation_error(character)` を投げるか、フラグ `invalid_utf8` が `replace` なら U+FFFD に置換する
- 標準入出力は Windows でも UTF-8 で動かす。`src/platform/windows.cpp` で `SetConsoleCP(65001)`、`SetConsoleOutputCP(65001)` を呼び、コンソール入力は `ReadConsoleW` で UTF-16 を受けて UTF-8 に変換する。ファイルは `std::filesystem::path` 経由で開き、日本語のパス名を扱えること
- `read_term/2,3` の `syntax_error` メッセージは、行番号・列番号（コードポイント単位）・問題のトークンを含む。列は全角文字でも 1 文字 1 列で数える
- `format/2,3` の `~t~|` 桁揃えは東アジアの幅（`East_Asian_Width` の W/F を幅 2）を考慮する。これも生成テーブルに含める

**日本語テストの最低セット（`tests/pl/japanese.pl`）**

- 日本語アトム・演算子（`:- op(700, xfx, は).` のあと `太郎 は 学生.` が読める）の読み書き往復
- `sub_atom/5` で `日本語処理系` から `語処` を切り出す
- `atom_length('𠮷野家', 3)`。𠮷は U+20BB7 の 1 コードポイントで、UTF-16 のサロゲートペアとしては数えない
- 日本語ファイル名の `consult/1`、日本語を含む `syntax_error` のメッセージ表示
- 全角記号モードの on/off でのパース結果の一致

## 5. モジュールシステムの仕様

構文は ISO/IEC 13211-2 と SWI-Prolog の共通部分を採り、意味論は SWI-Prolog の「述語ベースのモジュール」に従う。ファイル 1 つがモジュール 1 つに対応する。

**宣言と参照**

```prolog
:- module(家系, [親/2, 祖父/2, (は)/2]).      % 第 1 節として置く。エクスポートは PI のリスト
:- use_module(library(lists)).                 % 全エクスポートを取り込む
:- use_module(人名, [姓名分解/3 as 分解]).     % 一部だけ取り込み、別名を付ける
:- use_module(人名, except([読み/2])).
:- meta_predicate 全員(1, ?).                  % 引数 1 はゴール（モジュール限定が付く）
:- export(新述語/1).                           % module/2 の後からの追加エクスポート

祖父(X, Z) :- 親(X, Y), 親(Y, Z).
?- 家系:祖父(太郎, Z).                         % 明示限定呼び出し
```

**解決規則（この順で検索し、最初に見つかった定義を使う）**

1. 呼び出し元モジュールの自前の定義（ローカル述語とインポート済み述語は同じ名前空間。衝突は読み込み時の `permission_error(import_into, ...)`）
2. そのモジュールの既定インポート元（通常は `user`、`user` の既定は `system`）
3. `system` モジュール（すべての組込述語と制御構造。再定義は `permission_error(modify, static_procedure, PI)`）

- `Module:Goal` は `Goal` を `Module` の文脈で実行する。`Module` が未知なら `existence_error(module, Module)`
- `dynamic/1`、`assertz/1` 等が作る述語は、宣言・追加したモジュールに属する。`assertz(家系:親(a, b))` のように限定もできる
- `meta_predicate/1` で `:`、`0`〜`9`、`^`、`//` と指定された引数は、呼び出し時に `呼び出し元モジュール:引数` に包まれる。これにより `maplist/2,3`、`findall/3`、`catch/3`、`forall/2` などがモジュールをまたいで正しく動く
- `multifile/1` は `モジュール:PI` 形式で、他モジュールから節を追加できる
- `user` モジュールはトップレベルの既定。`library(...)` は `$MOSA_LIB` と実行ファイル相対の `lib/` から探す。`mosac` が作った実行ファイルは、まず自分に同梱したライブラリの一覧（第6節）を引き、同梱していないライブラリだけをこの 2 か所から探す
- `op/3` は宣言したモジュールにローカル。エクスポートリストに `op(P, T, N)` を書くと取り込み先にも効く。`user` で宣言した演算子は全モジュールに見える
- フラグ `double_quotes` と `japanese_punctuation` はモジュール単位（正確には読み込み中のファイル単位）に保存・復元する

**変換器（`mosac`）での扱い**

- 各モジュールは C++ の名前空間 `mosa_gen::m<マングルしたモジュール名>`（例: `mosa_gen::muser`）に対応する。モジュール名は UTF-8 のまま C++ 識別子には使えないので、`m` + 名前から作る。ASCII の英数字はそのまま、`_` は `_5f`、ほかのバイトは `_xx`（小文字 16 進 2 桁）にする。C++ は `__` を含む識別子を予約している（[lex.name]）が、この方式は `__` を出さない（持ち主の決定 B、2026-10-29）。元の名前は文字列定数として保持し、実行時のモジュールテーブルに登録する
- 生成コードはほかの述語の C++ 関数を直接は呼ばない。制御構文（`,`、`;`、`->`、`*->`、`\+`、`!`、`true`、`fail`、`catch/3`、`throw/1`）は生成関数の中で `rt::` の制御の入口を通してその場で動かし、`is/2` と算術比較（8.6.1、8.7.1）は `rt::arith_is` と `rt::arith_compare` を直に呼ぶ。それ以外の呼び出し（利用者、ライブラリ、組み込みの述語）はどれも `rt::call_goal` でゴール枠を書いてトランポリンへ戻り、呼び先は上の規則で実行時に解決する。呼び先が C++ 関数で動くのは、その述語に翻訳単位が付いているときだけである（ADR 0009）
- 変換単位は「モジュールの集合」であり、`use_module` の推移閉包を自動で含める。`library(...)` は事前変換済みのオブジェクトをリンクする

**最初に用意する標準ライブラリ（すべて Prolog で書く。C++ 実装が必要なものだけ `system` に置く）**

| モジュール | 主な述語 |
| --- | --- |
| `library(lists)` | `append/3`、`member/2`、`memberchk/2`、`length/2`（ISO コアなので system に置き re-export）、`nth0/3`、`nth1/3`、`last/2`、`reverse/2`、`msort/2`、`sum_list/2`、`max_list/2`、`list_to_set/2` |
| `library(apply)` | `maplist/2..5`、`foldl/4..6`、`include/3`、`exclude/3` |
| `library(between)` | `between/3`、`succ/2`、`plus/3` |
| `library(format)` | `format/1,2,3`（`~w`、`~q`、`~a`、`~d`、`~f`、`~e`、`~s`、`~c`、`~n`、`~t`、`~\|`） |
| `library(strings)` | `atomic_list_concat/2,3`、`split_atom/4`、`upcase_atom/2`（Unicode 大文字化は simple case mapping のみ） |
| `library(japanese)` | `ひらがな_カタカナ/2`、`全角_半角/2`、`読み幅/2`（表示幅の計算）。生成テーブルで実装 |

## 6. アーキテクチャ（ランタイム・インタプリタ・C++20変換）

インタプリタと変換器は同じランタイム（項・ヒープ・トレイル・組込述語）を共有し、違いは「節をどう実行するか」だけにする。これにより両者の結果の一致を差分テストで保証できる。

**処理の流れ**

1. 字句解析（`lexer`）: UTF-8 → トークン列。第 4 節の文字分類を使う
2. 構文解析（`reader`）: 演算子優先順位パーサ（Pratt 法）。`read_term/3` のオプション（`variable_names`、`singletons`、`quoted` など）を完全サポート
3. 読み込み（`loader`）: ディレクティブ処理、モジュール解決、項展開（`term_expansion/2`、`goal_expansion/2` は `user` と読み込み中モジュールで探す）
4. 節コンパイル（`clause_compiler`）: 節を中間表現（IR）に落とす。IR はインタプリタと `mosac` の共通入力
5. 実行: インタプリタは IR を直接解釈。`mosac` は IR から C++20 を生成

**ランタイム（`libmosa`）の核**

| 部品 | 設計 |
| --- | --- |
| セル表現 | 64bit タグ付き整数 `Cell`。下位 3bit がタグ: REF、ATOM、INT（61bit 即値）、STR（ヒープ内の関手セルへのポインタ）、LIS（リスト）、FLT（ボックス化 double）、BIG（将来予約）。61bit に収まらない整数は FLT と同様にボックス化し、`max_integer` は 2^63−1 のままにする |
| ヒープ | 単一の連続配列（起動時に上限までのアドレス空間を予約し、必要に応じて下から確保する。確保済みのセルは動かない。ADR 0010）。バックトラック時にトップを戻すだけで回収する |
| トレイル | 束縛した REF のアドレスを積む。条件付きトレイル（選択点より古いセルのみ記録） |
| 選択点スタック | 継続・トレイル位置・ヒープ位置・代替節へのポインタ・カット境界を保持 |
| GC | Phase 5 まで持たない（バックトラックによる回収のみ）。Phase 7 で、ステップの間の安全点でだけ走る sliding のマーク＆コンパクト型を追加（ADR 0010）。上限の近くで、空けた語が `limit / 32` より少ない GC が 3 回続くと、上限のメッセージで問い合わせを止める。`garbage_collect/0` を提供 |
| アトムテーブル | UTF-8 バイト列 → 32bit アトム番号。スレッド非対応で良い（非目標） |
| 例外 | Prolog の `throw/1` は C++ 例外を使わず、選択点スタックに `catch` フレームを積んで巻き戻す。C++ 例外はランタイム内部の致命的エラー（`resource_error`）だけに使う |
| ストリーム | `Stream` 抽象クラス + ファイル／標準入出力／メモリ（`open_string` 相当）の実装。バッファリングとピーク（`peek_char/1`）を内蔵 |

**実行モデル: 二値化 + 明示的継続**

節 `h :- b1, b2, b3.` を、継続を最後の引数に持つ形 `h(K) :- b1(b2(b3(K)))` に変換（二値化）する。これにより WAM の環境スタックが不要になり、C++ への変換が単純な関数とループになる。

- 各述語は `void p_X_N(Machine::Impl& machine, std::uint64_t frame, std::uint32_t state)` 形式の C++ 関数（`rt::NativeProc`、ADR 0009）に対応する。`frame` はヒープ上のゴール枠で、引数と継続を持つ。`state` 0 が呼び出しで、ほかの状態は節のやり直しか本体のゴールのあとの再開である。失敗は戻り値ではなく機械に記す
- 実行ループは `while (goal = m.pop()) { dispatch(goal); }` のトランポリン。C++ の再帰は使わないので深い再帰でもスタックオーバーフローしない
- 選択点は述語本体内の節の切り替えで作る。最初の引数のインデクシング（型タグ＋関手）を節コンパイラで行い、単一候補なら選択点を作らない
- カット境界は選択点スタックの高さである。実行時系が生成関数の呼び出しとやり直しのたびに設定し、再開する本体は継続枠の語 2 に持つ（ADR 0009）。`call/N` は呼び出し時の高さを新しい境界にするのでローカルになる

**`mosac` が生成する C++**

- モジュールごとに 1 つの `.cpp` と、そのモジュールの単位の関数と述語の関数を宣言するヘッダ。述語名は `p` + マングルした名前 + `_` + アリティ（例: `app/3` は `papp_3`）
- 生成コードは `mosa/runtime.hpp` のみを include し、C++20 標準ライブラリ以外を使わない
- 節は 1 つの関数で、本体の再開点（呼び出しのあとの続きと、制御構文の枝、then、else、done、回復、抜ける点）はそれぞれ別の関数になる。述語ごとの関数が `state` を `state_of(clause, point)` の `switch` で振り分け、再開点は翻訳器が作った順に番号を振る（ADR 0009）。`goto` もマクロも使わず、`inline` 関数と `constexpr` で書く（生成コードの可読性を保ち、デバッガで追えるようにするため）
- 静的述語だけを変換する。`dynamic` 述語と `assert` で追加される節はインタプリタ経路（IR 解釈）で動き、同じ `Machine` 上で共存する
- 生成物には `main.cpp`（`initialization(main)` 相当の呼び出し）と `CMakeLists.txt` を含め、`mosac emit app.pl -o out/ && cmake -S out -B out/build && cmake --build out/build` で実行ファイルができる
- 生成コードは人間が読んで元の Prolog を追えるように、各述語の先頭に元の節をコメントで入れる（日本語のままで良い）

**`mosac` のビルド・実行パイプライン（C++ を吐いて終わりにしない）**

`mosac` の既定動作は「Prolog ファイルを受け取り、単体で動く実行ファイルを作る」ことである。C++ ソースは中間生成物にすぎず、利用者は CMake もコンパイラも直接触らない。

| サブコマンド | 動作 | 出力 |
| --- | --- | --- |
| `mosac build app.pl -o app` | 変換 → コンパイル → リンクを一括。既定 | 実行ファイル `app`（Windows は `app.exe`） |
| `mosac run app.pl -- 引数...` | build のあと即実行。`build` と同じく `./.mosac-cache` で作り、実行ファイルの写しを一時ディレクトリから動かして（`--keep` で残す）、終了コードをそのまま返す | 実行結果 |
| `mosac emit app.pl -o out/` | C++ ソースと `CMakeLists.txt` だけを出す（手で改変したい人向け、デバッグ用） | `out/` |
| `mosac dist app.pl -o dist/` | build + 配布用パッケージ（実行ファイル、LICENSE、README） | zip / tar.gz |
| `mosac toolchain` | 検出したコンパイラ・CMake・同梱ランタイムの場所と版を表示。問題があれば対処法を日本語で出す | 診断表示 |

**ツールチェーンの扱い**

- C++ コンパイラは次の順で探す: `--cxx` → 環境変数 `MOSA_CXX` → `CXX` → OS 既定（macOS は `clang++`、Linux は `g++` 次いで `clang++`、Windows は `vswhere` で MSVC を見つけ `vcvarsall` 相当の環境変数を `mosac` 自身が設定する）。見つからなければ `mosac toolchain` と同じ診断を出して止まり、各 OS のインストール手順（Xcode Command Line Tools、`apt install g++`、Visual Studio Build Tools）を表示する。`--cxx`、`MOSA_CXX`、`CXX` で名指ししたコンパイラは `vcvarsall` を通さず今の環境で動かすので、名指しの `cl` には開発者用のコマンドプロンプトが要る（持ち主の決定、2026-10-30）
- CMake は任意。あれば使い、なければコンパイラを直接呼ぶ（生成コードはファイルごとにコンパイルして `mosac` 自身がリンクコマンドを組む）。したがって利用者側の最小要件は「C++20 コンパイラが 1 つ入っていること」
- ランタイム `libmosa` は、ビルド済みの静的ライブラリとヘッダを `mosac` のインストール先（`<prefix>/lib/mosa/<OS>-<arch>-<compiler>/`。`<OS>` は `macos`、`linux`、`windows`、`<arch>` は `arm64`、`x86_64`、`<compiler>` は族 `clang`、`gcc`、`msvc`）に同梱する。`mosac` はそのディレクトリを自分の場所から見つける。`<prefix>/bin/mosac` は `<prefix>/lib/mosa/<鍵>/` を見、ビルド木では `mosac` 自身のディレクトリに `libmosa`、`include/`、`pl/` がある。インストールするのは Release だけなので、`mosac build --debug` も Release のランタイムを結ぶ。利用者のマシンでランタイムを毎回ビルドしない。ABI が合わないときの逃げ道として `--runtime-source` で同梱ソース `<prefix>/lib/mosa/source/` から CMake でビルドする（結果は `.mosac-cache/runtime/<hash>/` にキャッシュする）
- `lib/*.pl` の標準ライブラリは事前変換済みのオブジェクトとして、ライブラリごとに 1 つ（`libmosa` の隣の `pl/libmosa_pl_<名前>.a`、Windows は `pl/mosa_pl_<名前>.lib`）同梱し、`use_module` で推移的に読まれたものと、未捕捉エラーの文面のための `messages_ja`・`messages_en` だけをリンクする。リンクしたライブラリの原文も実行ファイルに埋め込むので、ライブラリは実行ファイルから consult され、単位はそれに付く。ファイルからは読まない。インタプリタ用の写しは `<prefix>/bin/lib/*.pl` に置く
- 既定は静的リンク・`-O2`。Windows は `/MT`。できあがった実行ファイルは配布先に C++ ランタイム DLL も `mosac` も Prolog ファイルも要らない。`--debug` で `-O0 -g` と実行時の節トレースを有効にする。同梱ライブラリの単位はトレース無しで作ってあるので、`--debug` がトレースするのはプログラム自身の単位だけである
- 中間ファイルは今のディレクトリの `.mosac-cache/<hash>/` に置き、Prolog ソースとランタイムのハッシュで再利用する。生成 C++ はいつもそこに残る。`--clean` はビルドの前に `.mosac-cache/` を消し、`mosac run --keep` は実行ファイルの写しを置いた一時ディレクトリを残す
- クロスコンパイルは非目標。実行ファイルは `mosac` を動かした OS・アーキテクチャ向けだけを作る

**エラーの扱い（誰の責任かで出し分ける）**

- 利用者の Prolog の問題（構文エラー、エクスポート不整合、未定義述語の静的警告）→ 変換段階で止め、Prolog の行番号付きで報告する。コンパイラは起動しない
- 生成した C++ がコンパイルできない → `mosac` のバグとして扱う。「内部エラー: 生成コードがコンパイルできません。`.mosac-cache/<hash>/` を添えて報告してください」とだけ出し、コンパイラの生エラーは `--verbose` のときだけ表示する。CI では「生成 C++ は必ずコンパイルできる」を不変条件とし、差分テストの全ケースで確認する
- コンパイラ未検出、ディスク不足、権限 → 環境の問題として、対処法付きで報告する

**できあがった実行ファイルの振る舞い**

- `:- initialization(main, main).` があればそれがエントリポイント。`main/0` が成功すれば終了コード 0、失敗なら 1、未捕捉例外ならメッセージを出して 2。`halt/1` の引数はそのまま終了コードになる
- コマンドライン引数はフラグ `argv`（アトムのリスト）で受け取る
- `mosac build --interactive` で作ると、変換済みコードを読み込んだ状態で REPL が立ち上がる実行ファイルになる（変換コードとインタプリタの共存は上記の通り）。`--interactive` は `build` だけが受ける（`-o` は通常どおり使える。`mosac run` は受けない）。`:- initialization(main, main).` は実行せず、REPL を立ち上げる（オーナー決定、2026-10-30）
- 実行ファイルは起動時に外部ファイルを一切要求しない。必要な Prolog ライブラリはすべてリンク済みで、`use_module(library(X))` は第5節の探索より先に同梱の一覧で解決し、`lib/` を探しに行かない

**ソースの対応関係**

| ディレクトリ | 内容 |
| --- | --- |
| `src/core/` | Cell、ヒープ、トレイル、アトム、ユニフィケーション |
| `src/read/` | lexer、reader、演算子テーブル、Unicode 表 |
| `src/write/` | `write_term/3`、`writeq`、浮動小数点書式、引用判定 |
| `src/machine/` | 実行ループ、選択点、catch/throw、IR 定義 |
| `src/builtins/` | 組込述語（ISO の章ごとに 1 ファイル。`control.cpp`、`arith.cpp`、`atoms.cpp`、`streams.cpp` …） |
| `src/module/` | モジュールテーブル、インポート解決、meta\_predicate |
| `src/compile/` | 節コンパイラ（IR 生成、インデクシング） |
| `src/codegen/` | IR → C++20 生成 |
| `src/platform/` | OS 依存コード（Windows コンソール、パス変換）。ここ以外で `#ifdef _WIN32` を書かない |
| `lib/` | Prolog で書いた標準ライブラリ |

## 7. 組込述語とエラー体系

13211-1 第 8 章の組込述語は全部で約 100 個。実装順は依存関係に従い、下の表の上から進める。各述語は「規格の条番号・引数のモード・投げうるエラー・テストファイル名」を `src/builtins/` のコメント先頭に書く。

| 段階 | 規格の節 | 述語 |
| --- | --- | --- |
| A（最初） | 8.2 項のユニフィケーション、8.3 型検査、8.4 項比較 | `=/2`、`\=/2`、`unify_with_occurs_check/2`、`var/1`、`atom/1`、`integer/1`、`float/1`、`atomic/1`、`compound/1`、`nonvar/1`、`number/1`、`callable/1`、`is_list/1`（非 ISO、system）、`@</2` 等 6 個、`compare/3`、`==/2`、`\==/2` |
| A | 8.5 項の生成と分解 | `functor/3`、`arg/3`、`=../2`、`copy_term/2`、`term_variables/2` |
| A | 7.8 制御構造、8.15 論理と制御 | `true/0`、`fail/0`、`false/0`、`!/0`、`,/2`、`;/2`、`->/2`、`*->/2`（非 ISO）、`if/3`（非 ISO、切断を内に閉じた `(C *-> T ; E)`）、`catch/3`、`throw/1`、`call/1..8`、`\+/1`、`once/1`、`repeat/0`、`halt/0,1` |
| B | 9 算術、8.6・8.7 | `is/2`、`=:=/2` 等 6 個、全評価関数（`+ - * / // mod rem div abs sign min max gcd ** ^ sqrt sin cos tan asin acos atan atan2 exp log/1 log/2 log2 float integer float_integer_part float_fractional_part truncate round ceiling floor >> << /\ \/ xor \ pi e inf nan epsilon max_tagged_integer msb copysign nexttoward succ_or_zero`） |
| B | 8.16 アトム処理 | `atom_length/2`、`atom_concat/3`、`sub_atom/5`、`atom_chars/2`、`atom_codes/2`、`char_code/2`、`number_chars/2`、`number_codes/2`、`atom_number/2`（非 ISO） |
| C | 8.8・8.9 節の検索と生成・破壊 | `clause/2`、`current_predicate/1`、`asserta/1`、`assertz/1`、`retract/1`、`abolish/1`、`retractall/1`（Cor.2） |
| C | 8.10 すべての解 | `findall/3`、`findall/4`、`bagof/3`、`setof/3`（`^/2` の扱い、変数の標準順序ソート） |
| D | 8.11 ストリームの選択と制御 | `current_input/1`、`current_output/1`、`set_input/1`、`set_output/1`、`open/3,4`、`close/1,2`、`flush_output/0,1`、`stream_property/2`、`at_end_of_stream/0,1`、`set_stream_position/2` |
| D | 8.12 文字入出力、8.13 バイト入出力 | `get_char/1,2`、`get_code/1,2`、`peek_char/1,2`、`peek_code/1,2`、`put_char/1,2`、`put_code/1,2`、`nl/0,1`、`get_byte/1,2`、`peek_byte/1,2`、`put_byte/1,2` |
| D | 8.14 項入出力 | `read_term/2,3`、`read/1,2`、`write_term/2,3`、`write/1,2`、`writeq/1,2`、`print/1,2`、`write_canonical/1,2`、`op/3`、`current_op/3`、`char_conversion/2`、`current_char_conversion/2` |
| E | 8.17 実装定義フック | `set_prolog_flag/2`、`current_prolog_flag/2`、`halt/0,1`（A で先行） |
| E | 7.4.2 ディレクティブ、読み込み | `consult/1`、`[File]`、`ensure_loaded/1`、`include/1`、`initialization/1`、`dynamic/1`、`discontiguous/1`、`multifile/1` |
| F | 第 5 節 | モジュール関連（`module/2`、`use_module/1,2`、`current_module/1`、`predicate_property/2`、`meta_predicate/1`、`export/1`） |
| G | 非 ISO ユーティリティ（`library(*)`、第 5 節の表） | `format/1,2,3`、`between/3`、`succ/2`、`msort/2`、`sort/4`、`sort/2` と `keysort/2`（ISO コアなので system）、`forall/2`、`nb_setval/2`、`nb_getval/2`、`tab/1,2`、`term_to_atom/2`、`number_vars/3`、`setup_call_cleanup/3`、`garbage_collect/0`、`statistics/2` |

**エラー体系の規則**

- すべてのエラーは `throw(error(Formal, Context))` の形で投げる。`Context` は `context(PI, Extra)` とし、`PI` は述語指示子、`Extra` は追加情報（`syntax_error` なら `位置(Line, Col)` ではなく `syntax(stream(S, Line, LinePos, CharNo), Token)` の形。`Token` は問題のトークンの原子。ADR 0007）。`Context` に実装定義の項を置けるのは ISO が許している
- 引数検査の順序は ISO の各述語の定義（8.x.y.3 Errors の列挙順）に厳密に従う。たとえば `atom_length(X, foo)` は `type_error(integer, foo)` ではなく、先に `X` を検査して `instantiation_error` を投げる。順序違反は準拠スイートで検出される
- C++ 側では `throw_error(m, Formal, PI)` の 1 系統のヘルパだけを使い、`Formal` の各種は `make_type_error(m, Type, Culprit)` のような関数で組む。文字列の直書きは禁止（アトム名のタイポを防ぐ）
- 算術エラー: ゼロ除算は `evaluation_error(zero_divisor)`、整数オーバーフローは `evaluation_error(int_overflow)`、浮動小数点のオーバーフローは `evaluation_error(float_overflow)`、`sqrt(-1)` 等は `evaluation_error(undefined)`。`inf`/`nan` は `float_overflow`/`undefined` フラグが `error` のとき投げ、それ以外は IEEE 754 の値を返す（SWI 互換の `float_overflow`、`float_zero_div`、`float_undefined` フラグ）
- トップレベルは捕捉されなかった例外を `警告: ゴールは例外を送出しました: error(...)` の形で日本語と英語（フラグ `message_language`、既定は環境変数 `LANG` から判定）で表示する。メッセージ文字列は `lib/messages_ja.pl`、`lib/messages_en.pl` の Prolog 事実として置き、C++ には埋め込まない

## 8. テスト戦略

テストは 4 層。「準拠スイートの合格率」と「インタプリタと変換器の一致率」の 2 つの数値を README のバッジとして毎コミットで更新する。

| 層 | 中身 | 実行方法 |
| --- | --- | --- |
| 1. C++ 単体テスト | Cell・アトム・lexer・ユニフィケーション・UTF-8 変換など、Prolog を起動する前の部品 | 外部フレームワークを入れず、`tests/cpp/` に `assert` ベースの最小ランナー（`tests/cpp/harness.hpp`、50 行以内）。CTest に登録 |
| 2. Prolog 自前テスト | `tests/pl/<章>.pl`。`test(名前, ゴール, 期待)` の事実として書き、`mosatest` が読み込んで実行する。期待は `true`、`fail`、`error(Formal)`、`solutions([X=1, X=2])` の 4 種 | `mosatest tests/pl/` |
| 3. ISO 準拠スイート | Logtalk の Prolog 準拠テストを `tests/conformance/logtalk/` に改変せず置く。リポジトリと同じ Apache-2.0 で、著作権表示はそこの `NOTICE.txt` に残す。INRIA Suite（`inriasuite.pl`）は再配布の許諾が無いので同梱しない。Ulrich Neumerkel の表はコピーしない。`mosatest --suite` で実行し、合格数／総数と不合格一覧を `docs/conformance.md` に書き出す | CI で毎回。合格率が下がる変更はマージしない |
| 4. 差分テスト | 層 2・3 の全ゴールを `mosac` run で変換・ビルド・実行し、インタプリタの出力（解・エラー項・標準出力）とバイト単位で比較する | `mosatest --diff`。Phase 6 以降は CI 必須 |

**テストの書き方の規則**

- 新しい組込述語を実装するときは、規格の Examples（8.x.y.4）をそのまま `tests/pl/` に写してから実装する。Examples の各行が 1 テスト
- エラーのテストは `Formal` 項だけを比較し、`Context` は比較しない（実装定義のため）
- 日本語テスト（第 4 節）は層 2 に `tests/pl/japanese.pl` として置き、層 4 の差分テストにも必ず含める
- 入出力のテストはメモリストリーム（`open_string`、`with_output_to/2` 相当の `system` 述語）で行い、一時ファイルは `tests/tmp/` 配下にだけ作る
- Windows の CI では標準出力を UTF-8 で受けて比較する（`chcp 65001` をワークフローに入れる）

**ベンチマーク（合格率より後回しだが、退行の検知には使う）**

- `bench/`: `nrev`（naive reverse、LIPS 計測）、`queens`、`zebra`、`tak`、`deriv`、`crypt`。インタプリタと `mosac` 変換物の両方で計測し、`docs/bench.md` に表で記録する
- 目標値は置かない。前回比 20% 以上の悪化を CI で警告する

## 9. 開発フェーズとマイルストーン

各フェーズは「完了条件」を満たしたときだけ次へ進む。完了条件は機械的に確認できるものに限る（テストが通る、コマンドが動く）。1 セッションでは 1 フェーズの 1 タスクだけを扱う。

| Phase | 内容 | 完了条件 |
| --- | --- | --- |
| 0 | 骨組み: CMake、ディレクトリ、CI（3 OS）、`harness.hpp`、`docs/STATUS.md`、`docs/adr/0001-cell-representation.md` | 3 OS で空のテストが通る。`mosa --version` が動く |
| 1 | 項とリーダ・ライタ: Cell、ヒープ、アトム、ユニフィケーション、Unicode 表の生成、lexer、reader（`op/3` 含む）、`write_term/3` | `tests/pl/syntax.pl`（規格 6 章の例）が読み書き往復で通る。日本語アトムの往復が通る |
| 2 | 実行の核: IR、節コンパイラ（第 1 引数インデクシング）、トランポリン実行ループ、選択点、カット、`catch/throw`、段階 A の組込述語、最小 REPL、`consult/1` | `nrev` と `queens` が動く。段階 A の規格 Examples が全部通る |
| 3 | 組込述語の拡充: 段階 B〜E、ストリーム全般、フラグ、ディレクティブ、エラー体系の完成 | 同梱した Logtalk 準拠スイートの合格率 90% 以上。`docs/conformance.md` が自動生成される |
| 4 | モジュール: 第 5 節の全部、`library(*)` の最初の 6 モジュール、`meta_predicate` | `tests/pl/modules.pl` が通る。`library(lists)` を使った既存テストが壊れない |
| 5 | 準拠率の追い込みと日本語の磨き込み: 不合格項目の潰し込み、全角記号モード、Windows コンソール、メッセージの日本語化 | 同梱した Logtalk 準拠スイートの合格率を上げる。`tests/pl/japanese.pl` 全通過 |
| 6 | `mosac`: IR → C++20 生成、ビルドパイプライン（build / run / emit / toolchain、ツールチェーン検出、ランタイム同梱）、差分テスト | 層 2・3 の全テストで差分ゼロ。3 OS で mosac build hello.pl が作った実行ファイルが、コンパイラも CMake も mosac も入っていないクリーンな環境で動く。`nrev` の変換物がインタプリタの 3 倍以上速い |
| 7 | 仕上げ: GC、`garbage_collect/0`、`statistics/2`、マニュアル（日本語・英語）、mosac dist、リリース用パッケージ（各 OS のバイナリ、同梱ランタイム） | 長時間ループでメモリが頭打ちになる。README の手順だけで初見の人がビルドできる |

**フェーズをまたぐ禁止事項**

- Phase 1 で決めた Cell 表現と Phase 2 で決めた IR は、ADR（`docs/adr/`）を書いて承認を得るまで変えない
- 先のフェーズの機能を「ついでに」入れない。GC や多倍長整数の下準備も Phase 7 まで書かない
- 準拠スイートの不合格を「仕様の解釈違い」として `docs/iso-notes.md` に記録せずに放置しない
