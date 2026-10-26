# フェーズ0 骨組み 設計

日付: 2026-10-24
状態: 承認済み（2026-10-24、方針 A）

## 意図

Mosa Prolog は、ISO/IEC 13211 に沿った Prolog 処理系を C++20 で作るプロジェクトである。フェーズ0の目的は、Prolog の処理を書く前に、次のセッションが会話の記録なしでフェーズ1へ進める骨組みを残すことである。読む人は、このリポジトリを継ぐエージェントと、ADR を承認する浩さんである。

成功は機械的に判定する。4 つの CI ジョブ（macOS、Linux GCC、Linux Clang、Windows MSVC）が緑である。`mosa --version`、`mosac --version`、`mosatest --version` が、どれも標準出力に `mosa 0.0.1 (phase 0)` の 1 行だけを出して終了コード 0 で終わる。

## 規模

新しいリポジトリの骨組みを、コードのコミット 5 つと、そのあとの公開確認で置く。Prolog の字句解析、項、実行は書かない。設計はこの文書 1 枚である。

## 目標

- `AGENTS.md` のディレクトリツリーを置く。空のディレクトリには、何を置くかを 1 行で書いた `README.md` を置く
- 1 つの CMake プロジェクトから静的ライブラリ `libmosa` と実行ファイル `mosa`、`mosac`、`mosatest` を出す
- `CMakePresets.json` に configure、build、test の 3 種を、`macos-clang`、`linux-gcc`、`linux-clang`、`windows-msvc` の 4 名前で揃える
- `tests/cpp/harness.hpp` と、`1 + 1 == 2` を確認するテスト 1 件を CTest に登録する
- 3 実行ファイルの `--version` を CTest で確認する
- GitHub Actions で 4 ジョブを回し、警告をエラーにする。Job Summary に準拠率の枠を出す
- `docs/adr/0001-cell-representation.md` に、SPEC 第6節のセル表現を写す
- 各コミットの終わりに `docs/STATUS.md` を 15 行以内で更新する
- 最後に GitHub へ push し、4 ジョブが緑になってからフェーズ0完了を STATUS に書く

## 非目標

- Prolog ソースの読み込み、変換、テスト実行、REPL
- `Machine::create`、`consult`、`query`、`halt` の引数の型と戻り値
- Cell、ヒープ、ユニフィケーション、Unicode 表の生成
- `mosac` の build、run、emit、dist、toolchain
- 準拠スイートの取得と、合格率の数値
- GC、多倍長整数、モジュール、マニュアル本文
- `AGENTS.md`、`CLAUDE.md`、`docs/SPEC.md`、元の指示書の改変

## 失敗になるもの

- 後のフェーズの機能が、骨組みのついでに入っている
- テストを通すために期待値を弱めたり、テストを飛ばしたりしている
- 準拠率が出ていないのに、README や Job Summary が割合を表示している
- ADR が SPEC のセル表現を変えたり、指示書に無い比較案を新しい決定として書いたりしている
- C++20 標準ライブラリと CMake 以外の依存が入っている
- 警告をエラーにしていない、または警告を消すために警告を無効化している

## 制約

- 言語は C++20。コンパイラ拡張は使わない。`CMAKE_CXX_EXTENSIONS` は OFF
- CMake 3.25 以上。手元の確認環境は CMake 4.4、Apple Clang 21、Ninja 1.13
- 警告は `-Wall -Wextra -Wpedantic -Werror`。MSVC は `/W4 /WX /utf-8 /permissive- /Zc:__cplusplus`
- 生の `new` と `delete`、`reinterpret_cast`、C 配列、マクロは使わない。`#pragma once` は使ってよい
- `#ifdef _WIN32` などの OS 依存は `src/platform/` 以外に書かない。フェーズ0では `src/platform/` にソースを置かない
- 識別子は英語。コメント、コミット、文書は日本語。コメントは理由だけを書く
- 1 関数は 60 行以内、1 ファイルは 800 行以内
- 文字コードは UTF-8、BOM なし
- セッションの終わりにビルドできる状態なら、`cmake --build build && ctest --test-dir build` を通す。CMake がまだ無いコミットの終わりでは、その旨を STATUS に書く
- コミットメッセージは `[phase0] ` で始まる日本語 1 行とし、何を確認したかを含める
- この作業ディレクトリは、設計時点では Git 管理下にない。フェーズ0のコミット 1 の前に `git init` し、既定ブランチは `main` にする。この設計文書の追加が、すでに最初のコミットになっているときは、`git init` をやり直さず、コミット 1 をその次に積む。Git のユーザー設定は変更しない

## アプローチ

ビルドの形は仕様で一つに決まっている。ルートの `CMakeLists.txt` 一つ、preset 4 名、外部テストフレームワークなし、である。選んだのは作業の切り方である。

**A. 指示書の番号どおりにコミットを分ける（採用）**

ツリー、CMake と `--version`、harness、`ci.yml`、ADR を別コミットにする。最後に GitHub へ公開し、4 ジョブが緑になってから STATUS を更新してフェーズ0を終える。途中でセッションが切れても STATUS から再開できる。ADR は浩さんが見る文書なので、ビルドのコミットと混ぜない。

**B. 手元の preset が通った時点で実装を止める（不採用）**

同じファイルを作り、`ci.yml` も書く。完了宣言は macOS の preset が通った時点にする。仕様の完了条件である 3 OS の緑を満たさない。

**C. 骨組みを 1 コミットにまとめて公開する（不採用）**

中身は A と同じで、履歴が 1 コミットになる。途中経過が STATUS に残らない。

採用の理由は、`AGENTS.md` がセッションごとに 1 タスク、論理単位ごとにコミット、STATUS だけで再開、と定めていることである。番号付きの 4 タスクがその粒度である。

## 決めたこと

ここより上の「目標」「非目標」と、次の挙動は、会話で合意したものである。

3 つの実行ファイルは、1 つの CMake プロジェクトから出る。どれも `--version` で `mosa 0.0.1 (phase 0)` を出して終わる。Prolog の読み込み、変換、テスト実行はしない。`libmosa` は、この 3 つがリンクする、中身のほとんど無い静的ライブラリである。

`include/mosa/mosa.hpp` には、`Machine::create`、`consult`、`query`、`halt` の 4 つを名前だけ書く。引数の型と戻り値は書かない。`Machine` の実体はフェーズ2まで無く、ここで型を決めると後から動かせなくなるためである。`runtime.hpp` は、生成コードが include する置き場として置き、宣言は空にする。

ADR 0001 は SPEC 第6節のセル表現を写すだけである。新しい表現は足さない。浩さんの承認はフェーズ1に入る前の関門であり、フェーズ0の CI 条件には入れない。

元の指示書 `docs/ISO Prolog 処理系（C++20変換）— Claude Code Grok Build 向け指示書.md` は残す。フェーズ0の成果物には数えない。移動も削除もしない。

## 実装者への決定

会話で埋まっていなかった箇所は、次のとおり決める。承認時にここだけを読み返せば足りる。

1. 版文字列の置き場は `include/mosa/version.hpp` と、非 inline の `mosa::version_line()` を定義する `src/version.cpp` である。3 つの main はこの関数だけを呼ぶ。静的ライブラリが空だと、ツールチェーンによってリンクに失敗するためである。`version.cpp` は `mosa.hpp` と `runtime.hpp` も include し、両ヘッダが翻訳単位としてコンパイルできることを CI で担保する。
2. CMake のライブラリターゲット名は `mosa_lib` とする。実行ファイルターゲット `mosa` と同じ名前にできないためである。成果物名は `OUTPUT_NAME` を `mosa` にし、Unix では `libmosa.a`、MSVC では `mosa.lib` にする。ターゲット名の既定では `libmosa_lib.a` と `mosa_lib.lib` になるため、既定のままにはしない。
3. `--version` は引数の完全一致だけを見る。どれか 1 個が `--version` なら、他の引数は無視し、標準出力に版の 1 行だけを出して終了コード 0 で終わる。標準エラーは空である。`-v` や `--version-foo` は版表示ではない。
4. それ以外の起動は、標準エラーに「このコマンドはフェーズ0では使えない」という意味の 1 行を出す。行頭は実行ファイル名（`mosa`、`mosac`、`mosatest`）である。標準出力は空、終了コードは 1 である。文言の細かな言い回しは実装者に任せる。この経路のテストはフェーズ0では作らない。
5. 版表示の改行は、Windows のテキストモードで付く `\r` を許す。比較する本文は `mosa 0.0.1 (phase 0)` の 1 行だけである。
6. preset のバイナリディレクトリは、4 つとも `${sourceDir}/build` とする。`AGENTS.md` の終了手順 `cmake --build build && ctest --test-dir build` と、preset の 3 コマンドを、同じディレクトリで通すためである。preset を切り替えるときは再 configure する。並べて複数構成を持つことはフェーズ0ではやらない。
7. macOS と Linux の configure preset は Ninja、単一構成、`CMAKE_BUILD_TYPE` は `Release` である。Windows の configure preset は Visual Studio 17 2022 のジェネレータ、x64、`Release` である。GitHub の Windows runner は、Ninja に VS の環境を渡さない。Visual Studio ジェネレータなら追加のアクションなしでビルドできる。build preset と test preset は `configuration` を `Release` にする。
8. CTest は 4 件である。`smoke` は `1 + 1 == 2`。残り 3 件は各実行ファイルの `--version` で、`PASS_REGULAR_EXPRESSION` は版の 1 行に一致させる。末尾の `\r` は許す。
9. `tests/cpp/harness.hpp` は 50 行以内である。検査は関数で書き、マクロにしない。失敗回数を返し、テストの `main` はその回数をプロセスの終了コードにする。名前空間は `mosa_test` とし、将来の `mosa` と分けておく。
10. `src/unicode_tables.inc` は、フェーズ1で生成し直すという日本語コメントだけを書く。フェーズ0のどの翻訳単位も include しない。`tools/gen_unicode_tables.py` は、実行すると「表の生成はフェーズ1」という意味の 1 行を出して終了コード 0 で終わる。ネットワークアクセスもしない。Unicode のバージョンはここでは決めない。
11. README の準拠率は「未計測（フェーズ3で `docs/conformance.md` を自動生成する）」と書く。割合のバッジは置かない。日本語の変数の規則は、SPEC 第4節の要約を 1 段落で書き、詳細は SPEC を指す。要約に必ず入れる事実は、`親(太郎, 一郎).` の `太郎` はアトムであることと、日本語名の変数は `_太郎` のように下線で始めることである。ビルド手順は `cmake --preset macos-clang`、`cmake --build --preset macos-clang`、`ctest --preset macos-clang` の 3 コマンドである。
12. `LICENSE` は MIT とする。著作権表示は `Copyright (c) 2026 Mosa Prolog contributors` とする。個人名は指示書に無いので書かない。2026-10-26 にライセンスは Apache-2.0 へ変更した。現行は `LICENSE` と `NOTICE` である。
13. CI の Job Summary には `conformance: n/a (phase 0)` とだけ書く。合格率の計算はしない。
14. `.gitignore` は `build/` と、`tests/tmp/` の中身（`README.md` を除く）を無視する。`.gitattributes` でテキストは LF に揃える。Windows 上で文書の改行が揺れないようにするためである。
15. `install()` ルールは置かない。共有ライブラリも作らない。フェーズ6まで利用者向けのインストール形は無い。
16. リポジトリの名前と公開範囲は、`gh repo create` を実行する直前に利用者へ確認する。提案は `SilentMalachite/mosa-prolog` である。確認が終わるまで作成も push もしない。ログイン済みアカウントは SilentMalachite で、同名のリポジトリは 2026-10-24 時点で存在しない。

## ファイルの中身

既にあるファイルはコミット 1 に含める。内容は変えない。

| パス | フェーズ0の中身 |
| --- | --- |
| `AGENTS.md`、`CLAUDE.md`、`docs/SPEC.md`、元の指示書 | 現状のまま |
| `README.md` | ビルド手順、日本語変数の規則、準拠率は未計測、LICENSE への参照 |
| `LICENSE` | MIT。著作権表示は上記 |
| `CMakeLists.txt`、`CMakePresets.json` | 上記のターゲットと preset |
| `.github/workflows/ci.yml` | 下記の 4 ジョブ |
| `.gitignore`、`.gitattributes` | 上記 |
| `docs/STATUS.md` | コミットごとに更新。最終文は下記 |
| `docs/adr/README.md` | ADR を連番で置く、という 1 行 |
| `docs/adr/0001-cell-representation.md` | 下記。コミット 5 で追加 |
| `docs/iso-notes.md` | 条番号付きで、A か B かの質問だけを書く、という見出し。項目は空 |
| `docs/conformance.md` | 自動生成であり手で編集しない、という見出し。フェーズ0では未生成 |
| `docs/bench.md` | 計測は後のフェーズ、という 1 行。数値は書かない |
| `docs/unicode-version.md` | バージョンはフェーズ1で表を生成するときに固定する、という 1 行 |
| `docs/manual/ja/README.md`、`docs/manual/en/README.md` | マニュアル本文はフェーズ7、という 1 行 |
| `include/mosa/mosa.hpp` | `#pragma once`、空の `namespace mosa`、4 関数の名前と「署名は Machine が出来るまで書かない」というコメント |
| `include/mosa/runtime.hpp` | `#pragma once`、空の `namespace mosa`、生成コードが include する唯一のヘッダであるというコメント |
| `include/mosa/version.hpp` | `mosa::version_line()` の宣言 |
| `src/version.cpp` | `version_line()` の定義。3 ヘッダを include する |
| `src/main_mosa.cpp`、`src/main_mosac.cpp`、`src/main_mosatest.cpp` | 上記の起動時の挙動だけ |
| `src/unicode_tables.inc` | 生成前であるというコメントだけ |
| `src/core/`、`read/`、`write/`、`machine/`、`builtins/`、`module/`、`compile/`、`codegen/`、`platform/` | 各 1 行の `README.md`。SPEC 第6節の対応表にある役割を書く |
| `tools/gen_unicode_tables.py` | フェーズ1まで生成しないスタブ |
| `lib/README.md` | Prolog の標準ライブラリを後のフェーズで置く、という 1 行 |
| `tests/cpp/harness.hpp`、`tests/cpp/smoke_test.cpp` | 上記の harness と `1 + 1 == 2` |
| `tests/pl/README.md` | 章ごとの Prolog テストを後のフェーズで置く、という 1 行 |
| `tests/conformance/README.md` | 外部スイートとライセンスファイルを後のフェーズで同梱する、という 1 行。フェーズ0ではダウンロードしない |
| `tests/tmp/README.md` | 一時ファイルはここだけに作る、という 1 行 |
| `bench/README.md` | `nrev` などの計測を後のフェーズで置く、という 1 行 |

## ADR 0001

見出しは「背景」「決定」「捨てた案」「影響」の 4 つである。各本文は 3 行以内を目安にする。決定の文が折り返しで 3 行を超えても、SPEC の文を削らない。

- 背景: インタプリタと `mosac` は同じランタイムを共有する。項の表現がその共有の単位である。フェーズ1で実装する前に、表現を文書で固定する。
- 決定: SPEC 第6節の表「セル表現」の文を、そのまま書く。タグの種類、61bit 即値、ボックス化、`max_integer` は `2^63−1` のまま、という点を変えない。
- 捨てた案: 「指示書に比較案の記載がない。この ADR では追加しない。」の 1 行だけにする。WAM や NaN-boxing などの案を、ここで新しく書かない。
- 影響: フェーズ1はこの表現で Cell を実装する。変えるときは新しい ADR を書き、浩さんの承認を得る。フェーズ0のコードは Cell を実装しない。

ファイルの先頭に、状態が「フェーズ1着手前の承認待ち」であると書く。

## CI

ジョブは 4 つ。どれも configure、build、test を、対応する preset 名で実行する。

| ジョブ | runner | preset | コンパイラ |
| --- | --- | --- | --- |
| macos | `macos-latest` | `macos-clang` | runner の Apple Clang。15 以上であること |
| linux-gcc | `ubuntu-24.04` | `linux-gcc` | 配布物の `g++`。13 以上であること |
| linux-clang | `ubuntu-24.04` | `linux-clang` | 配布物の `clang++`。17 以上であること |
| windows | `windows-latest` | `windows-msvc` | Visual Studio 2022。v17.8 以上であること |

Linux のジョブは、そのジョブが使うコンパイラと Ninja と CMake を、配布物のパッケージから入れる。コンパイラのパッチ版は固定しない。Windows では追加のセットアップアクションを使わない。

各ジョブの最後に、Job Summary へ `conformance: n/a (phase 0)` を 1 行追記する。

CI が失敗したままセッションを終えるときは、テストを弱めずに、STATUS へ失敗と再現手順を書く。

## コミットの順

各コミットが 1 セッションの終わりである。コミット 2 以降は、その時点の `build/` でビルドと、存在するテストが通る。

1. `git init`。ツリー、プレースホルダ、`README`、`LICENSE`、`STATUS`、無視設定。CMake はまだ無い。STATUS の次は「CMake と `--version`」。未実施である理由は、CMake がまだ無いこと。
2. `CMakeLists.txt`、preset、`version` のヘッダとソース、3 つの main、`mosa_lib`。確認は macOS で preset の configure と build、および 3 実行ファイルの `--version`。STATUS の次は「harness と `1 + 1 == 2`」。
3. `harness.hpp` と `smoke_test`、版表示の CTest 3 件。確認は `ctest --preset macos-clang`。STATUS の次は「`ci.yml`」。
4. `.github/workflows/ci.yml`。手元では workflow の構文が見える範囲で確認する。3 OS の実行結果は、まだ無い。STATUS の次は「ADR 0001」。未解決は「リモートがまだ無い」。
5. ADR 0001。コードは変えない。STATUS の次は「GitHub へ公開し、4 ジョブの緑を確認」。未解決は「ADR は未承認。リモートは未作成」。
6. 利用者にリポジトリ名と公開範囲を確認してから作成し、`main` を push する。4 ジョブが緑になったら STATUS を最終文に更新してコミットする。緑になる前に直すときは、直した論理単位ごとにコミットする。

コミット 1 の STATUS の初期値は、指示書の文面に合わせ、作業に入った時点で進行中を書く。終了時には次の 1 タスクだけが残る。15 行を超えたら古い行を削る。

フェーズ0完了時の STATUS は、次の意味を 15 行以内で持つ。

- フェーズ: 0 完了
- 進行中: なし
- 完了: 4 ジョブが緑。`--version` は `mosa 0.0.1 (phase 0)`
- 次: 浩さんが ADR 0001 を承認したら、フェーズ1（Cell とリーダ）
- 未解決: ADR 0001 は未承認

## 実装者に任せるもの

- 3 つの main の内部の関数分割。観察できる挙動は「実装者への決定」の 3 と 4 に従う
- 使えない起動の、日本語の言い回し
- CI のステップ名と、preset の `displayName`
- README の文の調子。入れる事実は「ファイルの中身」の表に従う
- 各ディレクトリ README の 1 行の言い回し。役割の割り当ては SPEC 第6節のソース対応表に従う

## 確認

フェーズ0を終える前に、次がすべて取れていること。

- 手元で `cmake --preset macos-clang`、`cmake --build --preset macos-clang`、`ctest --preset macos-clang` が通る
- 同じビルドに対して `cmake --build build && ctest --test-dir build` が通る
- 3 実行ファイルの `--version` が、決められた 1 行と終了コード 0 である
- GitHub Actions の 4 ジョブが緑である
- `docs/adr/0001-cell-representation.md` が SPEC のセル表現を変えずに含んでいる
- `AGENTS.md`、`docs/SPEC.md`、元の指示書の差分が空である
