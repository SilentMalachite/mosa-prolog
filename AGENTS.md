# AGENTS.md — Mosa Prolog

このファイルは Claude Code と Grok Build が共通で読むエージェント向け指示の本体である。Claude Code は `CLAUDE.md`（中身は `@AGENTS.md` の 1 行）経由で、Grok Build はこのファイルを直接読む。エージェントごとの差分は設けない。

## プロジェクト概要

- ISO/IEC 13211 準拠の Prolog 処理系 **Mosa Prolog**（リポジトリ `mosa-prolog`）を C++20 で実装する
- コマンド: `mosa`（インタプリタ）、`mosac`（Prolog → C++20 変換・ビルド）、`mosatest`（テストランナー）。ランタイムは `libmosa`、ヘッダは `include/mosa/`、名前空間は `mosa`、環境変数の接頭辞は `MOSA_`
- 依存は C++20 標準ライブラリと CMake 3.25 以上のみ。macOS（Apple Clang 15+）、Linux（GCC 13+ / Clang 17+）、Windows（MSVC 2022 v17.8+）の 3 OS で CI を通す
- 文字コードはすべて UTF-8（BOM なし）。「文字」は常に Unicode スカラー値 1 個

## 仕様の参照先

仕様の本文は `docs/SPEC.md` にある（このファイルと二重管理しない）。作業前に該当節を読むこと。

| 知りたいこと | `docs/SPEC.md` の節 |
| --- | --- |
| 目的・非目標 | 第 1 節 |
| 成果物・ビルド環境・コンパイラ制約 | 第 2 節 |
| ISO の実装範囲・実装定義事項・意図的な逸脱 | 第 3 節 |
| 日本語・Unicode（字句分類、全角記号モード、入出力） | 第 4 節 |
| モジュールシステム | 第 5 節 |
| アーキテクチャ（Cell、実行モデル、`mosac`、ソース配置） | 第 6 節 |
| 組込述語の実装順・エラー体系 | 第 7 節 |
| テスト戦略 | 第 8 節 |
| フェーズと完了条件 | 第 9 節 |

## エージェントの作業ルール

### セッションの始め方（毎回必ず）

1. `docs/STATUS.md` を読む。「今のフェーズ」「進行中のタスク」「前回の未解決事項」が 15 行以内で書いてある
2. `git log --oneline -10` と `ctest` の結果を確認し、壊れたままのテストがあれば新しい作業より先に直す
3. 今回扱うタスクを 1 つ選び、`STATUS.md` の「進行中」に書いてから着手する。タスクの大きさは「1 つの述語」「1 つのファイル」「1 つのバグ」のいずれか

### セッションの終わり方（毎回必ず）

1. `cmake --build build && ctest --test-dir build` を通す。通らない状態で終えるときは `STATUS.md` にその旨と再現手順を書く
2. `STATUS.md` を更新する: 完了したこと、次にやること 1 つ、迷った点。15 行を超えたら古い記述を削る
3. 変更を 1 つの論理単位ごとにコミットする。メッセージは `[phase2] functor/3 を実装（8.5.1 の Examples 全通過）` のように、フェーズ・内容・確認方法を日本語で 1 行

### 設計判断の扱い

- Cell 表現、IR、モジュール解決、エラーの `Context` 形式など、後から変えにくい決定は `docs/adr/NNNN-題名.md` に「背景・決定・捨てた案・影響」を各 3 行以内で書く。ADR を書く前に実装しない
- 仕様（`docs/SPEC.md` 第 3〜7 節）と規格が食い違うと気づいたら、その場で直さず `docs/iso-notes.md` に条番号付きで書き、浩さんに確認を求める。確認は「A か B か」の形の質問 1 つにする
- 判断に迷う箇所で「とりあえず動かす」実装をしたときは、コードに `// TODO(iso): 8.16.3.3 の順序未確認` のように規格の条番号を付ける。条番号のない TODO は書かない

### コードの書き方

- 識別子は英語、コメント・コミット・ドキュメントは日本語。コメントには「何をしているか」ではなく「なぜこの順序・この方法か」を書く
- 1 関数は 60 行以内、1 ファイルは 800 行以内。超えるなら分割を先にやる
- 生の `new`/`delete`、`reinterpret_cast`、C 配列、マクロは使わない。`std::span`、`std::string_view`、`std::expected` 相当（`std::optional` + エラー型）を使う。例外は `docs/SPEC.md` 第 6 節の規則に従う（Prolog の `throw/1` に C++ 例外を使わない）
- 組込述語の C++ 関数は、引数検査 → 本処理 → 束縛、の順に書き、途中で束縛してから失敗する書き方をしない
- 組込述語のファイル先頭コメントには「規格の条番号・引数のモード・投げうるエラー・テストファイル名」を書く
- エラーは `throw_error(m, Formal, PI)` と `make_type_error(m, Type, Culprit)` 系のヘルパだけで投げる。アトム名の文字列直書きは禁止
- `#ifdef _WIN32` などの OS 依存コードは `src/platform/` 以外に書かない
- コンパイラ拡張（GNU 拡張、`__int128`、`#pragma once` 以外の独自プラグマ）は使わない。警告は `-Wall -Wextra -Wpedantic`（MSVC は `/W4`）でエラー扱い
- 新しい依存ライブラリは追加しない。必要だと思ったら `STATUS.md` に理由を書いて止まる

### テストの書き方

- 新しい組込述語は、規格の Examples（8.x.y.4）をそのまま `tests/pl/` に写してから実装する。Examples の各行が 1 テスト
- エラーのテストは `Formal` 項だけを比較し、`Context` は比較しない
- 入出力のテストはメモリストリームで行い、一時ファイルは `tests/tmp/` 配下にだけ作る
- 日本語テストは `tests/pl/japanese.pl` に置き、差分テスト（`mosatest --diff`）にも必ず含める

### フェーズをまたぐ禁止事項

- Phase 1 で決めた Cell 表現と Phase 2 で決めた IR は、ADR を書いて承認を得るまで変えない
- 先のフェーズの機能を「ついでに」入れない。GC や多倍長整数の下準備も Phase 7 まで書かない
- 準拠スイートの不合格を「仕様の解釈違い」として `docs/iso-notes.md` に記録せずに放置しない

### やってはいけないこと

- テストを通すためにテストを弱める、スキップする、期待値を実装に合わせて書き換える
- 「ISO ではこうだが SWI では違う」という理由で SWI に寄せる。`iso` フラグが true のときの挙動は常に ISO
- 1 セッションで複数フェーズに手を出す。フェーズの完了条件を満たさずに次へ進む
- `STATUS.md` を更新せずに終える。次のセッションの自分（または別のエージェント）が、会話ログなしで続きを始められることが基準
- 浩さんへの報告で長文の要約を書く。報告は「できたこと 1 行・次 1 行・質問 1 行（あれば）」で足りる

## リポジトリ構成とファイル配置

Phase 0 で以下をそのまま作る。空のディレクトリには `.gitkeep` ではなく、そこに何を置くかを 1 行書いた `README.md` を置く。

```text
mosa-prolog/
├── AGENTS.md                 # このファイル。Grok Build と Claude Code が読む本体
├── CLAUDE.md                 # 1 行: @AGENTS.md
├── README.md                 # ビルド手順、日本語変数の規則、準拠率バッジ
├── LICENSE                   # MIT
├── CMakeLists.txt
├── CMakePresets.json         # macos-clang / linux-gcc / linux-clang / windows-msvc
├── .github/workflows/ci.yml  # 3 OS マトリクス、警告をエラー扱い、準拠率を Job Summary に出す
├── docs/
│   ├── SPEC.md               # 仕様（指示書の第 1〜9 節）
│   ├── STATUS.md             # 現在地。15 行以内
│   ├── adr/                  # 0001-cell-representation.md から連番
│   ├── iso-notes.md          # 規格の解釈メモ（条番号付き）
│   ├── conformance.md        # 自動生成。手で編集しない
│   ├── bench.md
│   ├── unicode-version.md
│   └── manual/               # ja/ と en/。Phase 7
├── include/mosa/
│   ├── runtime.hpp           # 生成コードが include する唯一のヘッダ
│   └── mosa.hpp              # 組込み用 C++ API（Machine の生成、consult、query）
├── src/
│   ├── core/  read/  write/  machine/  builtins/  module/  compile/  codegen/  platform/
│   ├── unicode_tables.inc    # 生成物。コミットする
│   ├── main_mosa.cpp
│   ├── main_mosac.cpp
│   └── main_mosatest.cpp
├── lib/                      # Prolog 製の標準ライブラリ（lists.pl, apply.pl, format.pl, japanese.pl, messages_ja.pl, messages_en.pl …）
├── tests/
│   ├── cpp/                  # harness.hpp と部品テスト
│   ├── pl/                   # 章ごとの自前テスト。japanese.pl、modules.pl を含む
│   ├── conformance/          # 外部スイート（ライセンスファイルを同梱）
│   └── tmp/                  # .gitignore
├── bench/
└── tools/
    └── gen_unicode_tables.py # 開発者だけが実行。ビルドには不要
```

### 各ファイルの最初の中身

- `docs/STATUS.md` の初期内容: `フェーズ: 0 / 進行中: なし / 次: CMake と harness.hpp / 未解決: なし`
- `docs/adr/0001-cell-representation.md`: `docs/SPEC.md` 第 6 節の Cell 設計を ADR 形式に写す。これが最初の ADR で、Phase 1 着手前に浩さんが承認する
- `CMakePresets.json` には `configurePresets` と `buildPresets` と `testPresets` を揃え、`cmake --preset macos-clang && cmake --build --preset macos-clang && ctest --preset macos-clang` の 3 コマンドで全部が動くようにする
- `include/mosa/mosa.hpp` の API は最小に保つ: `Machine::create()`、`consult(path)`、`query(text, callback)`、`halt()`。これ以外は Phase 7 まで追加しない

### Phase 0 の最初のタスク

1. 上のツリーを作り、`AGENTS.md`・`CLAUDE.md`・`docs/SPEC.md`・`docs/STATUS.md` を置く
2. `CMakeLists.txt` と `CMakePresets.json` を書き、`mosa --version` が `mosa 0.0.1 (phase 0)` と出力するだけの実行ファイルを作る
3. `tests/cpp/harness.hpp` と、`1 + 1 == 2` を確認するだけのテストを 1 つ登録し、`ctest` で通す
4. `.github/workflows/ci.yml` を書き、3 OS で緑になるのを確認してから `STATUS.md` を更新して終了する
