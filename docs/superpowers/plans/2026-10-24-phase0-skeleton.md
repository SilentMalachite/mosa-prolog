# フェーズ0 骨組み Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Prolog の処理を書かずに、次のセッションが会話なしでフェーズ1へ進める骨組みを、コミット 5 つと公開確認で置く。

**Architecture:** 1 つの CMake プロジェクトから静的ライブラリ `mosa_lib`（成果物名 `mosa`）と実行ファイル `mosa`、`mosac`、`mosatest` を出す。3 つの main は `mosa::version_line()` だけを呼び、`--version` のときだけ版の 1 行を出して終了する。preset 4 名はどれもバイナリディレクトリを `build/` にし、CTest は `1 + 1 == 2` と版表示 3 件だけである。ADR 0001 は SPEC 第6節のセル表現の転写で、コードは Cell を実装しない。

**Tech Stack:** C++20、CMake 3.25 以上、CTest、Ninja（macOS / Linux）、Visual Studio 17 2022（Windows）。依存ライブラリは追加しない。

**Spec:** `docs/superpowers/specs/2026-10-24-phase0-skeleton-design.md`

各タスクは 1 コミットで終わり、そのコミットがセッションの区切りである。次のタスクは、前のタスクの確認コマンドが通ってから始める。実装中にこの計画と設計文書を書き換えない。ずれに気づいたら `docs/STATUS.md` の未解決に 1 行書いて止まり、利用者に聞く。

## Global Constraints

- 言語は C++20。`CMAKE_CXX_EXTENSIONS` は OFF。コンパイラ拡張、`__int128`、`#pragma once` 以外の独自プラグマは使わない
- CMake 3.25 以上。警告は `-Wall -Wextra -Wpedantic -Werror`。MSVC は `/W4 /WX /utf-8 /permissive- /Zc:__cplusplus`
- 生の `new` と `delete`、`reinterpret_cast`、C 配列、マクロは使わない。`#pragma once` は使ってよい
- `#ifdef _WIN32` などの OS 依存は書かない。`src/platform/` にソースを置かない
- 識別子は英語。コメント、コミット、文書は日本語。コメントは理由だけを書く
- 1 関数は 60 行以内、1 ファイルは 800 行以内。文字コードは UTF-8、BOM なし
- コミットメッセージは `[phase0] ` で始まる日本語 1 行とし、何を確認したかを含める
- Git は既に `main` の `65ab1ad` がある。`git init` をやり直さない。Git のユーザー設定は変更しない
- `AGENTS.md`、`CLAUDE.md`、`docs/SPEC.md`、`docs/ISO Prolog 処理系（C++20変換）— Claude Code Grok Build 向け指示書.md` は内容を変えない
- `install()` は置かない。共有ライブラリは作らない。新しい依存は追加しない
- CTest は 4 件だけにする。版表示以外の起動経路を CTest に足さない。テストを弱めたり飛ばしたりしない
- 準拠率の割合は README にも Job Summary にも書かない
- この作業ディレクトリは既に Git 管理下にある。コミット 1 は `65ab1ad` の次に積む

## Review Focus

設計が CTest を 4 件に固定しているため、次の 5 つは CTest にしない。Task 2 の手確認コマンドが、期待する出力と終了コードをそのまま比較する。

- 引数なしで起動した人は、標準エラーに `<実行ファイル名>: このコマンドはフェーズ0では使えません` の 1 行だけを見て、標準出力は空、終了コードは 1 である
- `-v` は版表示ではない。同じエラー行、空の標準出力、終了コード 1 である
- `--version-foo` は版表示ではない。同じエラー行、空の標準出力、終了コード 1 である
- `foo --version bar` のように、どれか 1 個が `--version` なら他の引数は無視し、標準出力は版の 1 行だけ、標準エラーは空、終了コードは 0 である
- `./build/mosa` のようにパス付きで起動しても、使えない起動の行頭はパスではなく `mosa`、`mosac`、`mosatest` である

## ファイルの責任

タスク 1 で、ビルドしないファイルを全部置く。タスク 2 以降は、そのタスクの列だけを追加または更新する。

| パス | 責任 | タスク |
| --- | --- | --- |
| `README.md` | ビルド 3 コマンド、日本語変数の 2 事実、準拠率は未計測、LICENSE への参照 | 1 |
| `LICENSE` | MIT。著作権表示は `Copyright (c) 2026 Mosa Prolog contributors` | 1 |
| `.gitignore`、`.gitattributes` | `build/` と `tests/tmp/` の中身を無視。テキストは LF | 1 |
| `docs/STATUS.md` | 15 行以内の現在地。各タスクの終わりに上書きする | 1 以降 |
| `docs/iso-notes.md`、`conformance.md`、`bench.md`、`unicode-version.md` | 空の型。数値や解釈の項目は書かない | 1 |
| `docs/adr/README.md`、`docs/manual/*/README.md`、各ディレクトリの `README.md` | 何を置くかを 1 行で書く | 1 |
| `include/mosa/mosa.hpp`、`runtime.hpp` | 名前だけ、または空の名前空間。署名は書かない | 1 |
| `src/unicode_tables.inc`、`tools/gen_unicode_tables.py` | 生成はフェーズ1、というスタブ。ネットワークは使わない | 1 |
| `CMakeLists.txt`、`CMakePresets.json` | ターゲット、警告、preset 4 名 | 2、テスト登録は 3 |
| `include/mosa/version.hpp`、`src/version.cpp` | `mosa::version_line()` | 2 |
| `src/main_mosa.cpp`、`main_mosac.cpp`、`main_mosatest.cpp` | 起動時の挙動だけ | 2 |
| `tests/cpp/harness.hpp`、`smoke_test.cpp` | `1 + 1 == 2` と失敗回数 | 3 |
| `.github/workflows/ci.yml` | 4 ジョブ。Job Summary は `conformance: n/a (phase 0)` | 4 |
| `docs/adr/0001-cell-representation.md` | SPEC 第6節のセル表現の転写 | 5 |

既にある `AGENTS.md`、`CLAUDE.md`、`docs/SPEC.md`、元の指示書はタスク 1 で、内容を変えずにコミットする。

## タスクをまたいで使う契約

`include/mosa/version.hpp` が後のタスクに渡す宣言は次だけである。

```cpp
namespace mosa {
[[nodiscard]] std::string version_line();
}
```

戻り値は改行を含まない `mosa 0.0.1 (phase 0)` である。CMake の `project()` の版も `0.0.1` にする。文字列は生成しない。

使えない起動の 1 行は、計画側で言い回しを固定する。`mosa` のときは `mosa: このコマンドはフェーズ0では使えません`。`mosac` と `mosatest` は行頭の名前だけが変わる。末尾の改行は 1 つ。

`tests/cpp/harness.hpp` が渡す宣言は次だけである。

```cpp
namespace mosa_test {
void check(bool condition, std::string_view label);
int failures();
}
```

`check` は条件が偽のときだけ失敗回数を 1 増やし、標準エラーに `FAIL: ` と label と改行を出す。真のときは何も出さない。`failures()` はその回数を返す。マクロにも `assert` にもしない。

---

### Task 1: ツリーとプレースホルダ

**Files:**
- Create: 下の一覧。CMake、`.cpp`、`ci.yml`、ADR 0001 は作らない
- Modify: なし
- Test: `tools/gen_unicode_tables.py` の実行

**Interfaces:**
- Consumes: なし
- Produces: タスク 2 が include する `include/mosa/mosa.hpp` と `include/mosa/runtime.hpp`。中身は下のコード

- [ ] **Step 1: 既存の Git を確認し、初期化しない**

Run: `git status --short && git log --oneline -1`

Expected: ブランチは `main`。最新コミットは `65ab1ad`。未追跡は `AGENTS.md`、`CLAUDE.md`、`docs/SPEC.md`、元の指示書だけである。`git init` は実行しない。

- [ ] **Step 2: ディレクトリの 1 行 README を置く**

各ファイルは 1 行だけにする。

| パス | 行 |
| --- | --- |
| `src/core/README.md` | Cell、ヒープ、トレイル、アトム、ユニフィケーションを置く。 |
| `src/read/README.md` | lexer、reader、演算子テーブル、Unicode 表を置く。 |
| `src/write/README.md` | `write_term/3`、`writeq`、浮動小数点書式、引用判定を置く。 |
| `src/machine/README.md` | 実行ループ、選択点、catch/throw、IR 定義を置く。 |
| `src/builtins/README.md` | 組込述語を置く。ISO の章ごとに 1 ファイルとする。 |
| `src/module/README.md` | モジュールテーブル、インポート解決、meta_predicate を置く。 |
| `src/compile/README.md` | 節コンパイラを置く。IR の生成とインデクシングを担当する。 |
| `src/codegen/README.md` | IR から C++20 を生成するコードを置く。 |
| `src/platform/README.md` | OS 依存コードを置く。ここ以外に OS 分岐を書かない。フェーズ0ではソースを置かない。 |
| `lib/README.md` | Prolog で書いた標準ライブラリを後のフェーズで置く。 |
| `tests/cpp/README.md` | C++ の部品テストと `harness.hpp` を置く。 |
| `tests/pl/README.md` | 章ごとの Prolog テストを後のフェーズで置く。 |
| `tests/conformance/README.md` | 外部スイートとライセンスファイルを後のフェーズで同梱する。フェーズ0ではダウンロードしない。 |
| `tests/tmp/README.md` | 一時ファイルはここだけに作る。 |
| `bench/README.md` | `nrev` などの計測を後のフェーズで置く。 |
| `docs/adr/README.md` | ADR を連番で置く。 |
| `docs/manual/ja/README.md` | 日本語マニュアルの本文はフェーズ7で置く。 |
| `docs/manual/en/README.md` | 英語マニュアルの本文はフェーズ7で置く。 |

- [ ] **Step 3: 文書の型、ヘッダ、生成スタブを置く**

`docs/iso-notes.md`

```markdown
# ISO 解釈メモ

規格と仕様が食い違うときは、条番号を付けて「A か B か」の質問を 1 つ書く。確認前に実装しない。

項目はまだ無い。
```

`docs/conformance.md`

```markdown
# 準拠率

このファイルは自動生成である。手で編集しない。フェーズ0では未生成。
```

`docs/bench.md`

```markdown
計測は後のフェーズで行う。
```

`docs/unicode-version.md`

```markdown
Unicode のバージョンは、フェーズ1で表を生成するときに固定する。
```

`include/mosa/mosa.hpp`

```cpp
#pragma once

// 署名は Machine が出来るまで書かない。フェーズ0で型を決めると、フェーズ2まで動かせなくなる。
namespace mosa {
// Machine::create
// consult
// query
// halt
}
```

`include/mosa/runtime.hpp`

```cpp
#pragma once

// 生成コードが include する唯一のヘッダ。宣言はフェーズ6まで空にする。
namespace mosa {}
```

`src/unicode_tables.inc`

```text
// フェーズ1で tools/gen_unicode_tables.py がこのファイルを生成し直す。フェーズ0では include しない。
```

`tools/gen_unicode_tables.py`

```python
print("表の生成はフェーズ1で行う")
```

import は書かない。ネットワークも開かない。

- [ ] **Step 4: README、LICENSE、無視設定、STATUS を置く**

`README.md` には次の 4 つを入れる。文の長さは実装者が整えてよい。事実は変えない。

- ビルド手順は、この 3 コマンドだけを書く。`cmake --preset macos-clang`、`cmake --build --preset macos-clang`、`ctest --preset macos-clang`
- 日本語の変数: `親(太郎, 一郎).` の `太郎` はアトムである。日本語名の変数は `_太郎` のように下線で始める。詳細は `docs/SPEC.md` 第4節
- 準拠率の文言は「未計測（フェーズ3で `docs/conformance.md` を自動生成する）」。割合もバッジも置かない
- ライセンスは MIT で、本文は `LICENSE` を指す

`LICENSE` は MIT の全文とする。著作権表示の行は `Copyright (c) 2026 Mosa Prolog contributors` だけにし、個人名は書かない。

`.gitignore`

```gitignore
/build/
/tests/tmp/*
!/tests/tmp/README.md
```

`.gitattributes`

```gitattributes
* text=auto eol=lf
```

`docs/STATUS.md` は次で上書きする。

```markdown
# STATUS

フェーズ: 0
進行中: なし
完了: ディレクトリ、README、LICENSE、無視設定を置いた
次: CMake と --version
未解決: なし
確認: ctest は未実施。CMake がまだ無い
```

- [ ] **Step 5: スタブを実行し、既存文書が変わっていないことを確認する**

Run: `python3 tools/gen_unicode_tables.py`

Expected: 標準出力は `表の生成はフェーズ1で行う` の 1 行。終了コード 0。

Run: `git diff -- AGENTS.md CLAUDE.md docs/SPEC.md "docs/ISO Prolog 処理系（C++20変換）— Claude Code Grok Build 向け指示書.md"`

Expected: 差分は空である。この 4 ファイルは編集しない。

- [ ] **Step 6: Commit**

```bash
git add AGENTS.md CLAUDE.md README.md LICENSE .gitignore .gitattributes \
  docs/SPEC.md docs/STATUS.md docs/iso-notes.md docs/conformance.md \
  docs/bench.md docs/unicode-version.md \
  "docs/ISO Prolog 処理系（C++20変換）— Claude Code Grok Build 向け指示書.md" \
  docs/adr/README.md docs/manual include src lib tests bench tools
git commit -m "[phase0] ディレクトリと STATUS を置く（CMake が無いので ctest は未実施）"
```

`git status --short` が空であること。設計文書とこの計画は、タスク 1 より前のコミットに残っているので、このコミットに混ぜない。

---

### Task 2: CMake と --version

**Files:**
- Create: `CMakeLists.txt`、`CMakePresets.json`、`include/mosa/version.hpp`、`src/version.cpp`、`src/main_mosa.cpp`、`src/main_mosac.cpp`、`src/main_mosatest.cpp`
- Modify: `docs/STATUS.md`
- Test: 3 実行ファイルの起動。CTest はまだ登録しない

**Interfaces:**
- Consumes: `include/mosa/mosa.hpp`、`include/mosa/runtime.hpp`
- Produces: `std::string mosa::version_line()`。3 実行ファイルは同じ起動規則

- [ ] **Step 1: 版のヘッダと定義を書く**

`include/mosa/version.hpp` は、上の契約の宣言と `<string>` の include だけにする。

`src/version.cpp` は次にする。`mosa.hpp` と `runtime.hpp` をこの翻訳単位に入れ、空の静的ライブラリがリンクできず、ヘッダがコンパイルできない状態を避けるためである。

```cpp
#include "mosa/version.hpp"

#include "mosa/mosa.hpp"
#include "mosa/runtime.hpp"

namespace mosa {
std::string version_line() {
  return "mosa 0.0.1 (phase 0)";
}
}
```

戻り値に改行を付けない。

- [ ] **Step 2: 3 つの main を書く**

`src/main_mosa.cpp` の観測できる挙動は次である。`argc` の 1 以降を見る。どれかが `std::string_view` で `"--version"` と完全一致したら、`mosa::version_line()` と `'\n'` だけを標準出力へ出して 0 で終わる。標準エラーには何も書かない。一致が無ければ、標準エラーへ `mosa: このコマンドはフェーズ0では使えません\n` だけを出して 1 で終わる。標準出力には何も書かない。`argv[0]` は行頭に使わない。

`main_mosac.cpp` と `main_mosatest.cpp` は、エラー行の行頭だけを `mosac` と `mosatest` に変える。呼ぶライブラリ関数は `version_line()` だけである。

- [ ] **Step 3: CMakeLists.txt を書く**

構成は次に固定する。テスト登録と `enable_testing()` はタスク 3 まで書かない。

- `cmake_minimum_required(VERSION 3.25)`
- `project(mosa VERSION 0.0.1 LANGUAGES CXX)`
- `CMAKE_CXX_STANDARD` は 20、`CMAKE_CXX_STANDARD_REQUIRED` は ON、`CMAKE_CXX_EXTENSIONS` は OFF
- INTERFACE ライブラリ `mosa_warnings` のコンパイルオプションは、ジェネレータ式で分ける。コンパイラ ID が MSVC のとき `/W4 /WX /utf-8 /permissive- /Zc:__cplusplus`、それ以外は `-Wall -Wextra -Wpedantic -Werror`
- `add_library(mosa_lib STATIC src/version.cpp)`。`target_include_directories` は `PUBLIC include`。`OUTPUT_NAME` は `mosa`。`mosa_warnings` を PUBLIC でリンクする
- `mosa`、`mosac`、`mosatest` はそれぞれ `src/main_mosa.cpp`、`src/main_mosac.cpp`、`src/main_mosatest.cpp`。`mosa_lib` を PRIVATE でリンクする
- `install()` は書かない。`OUTPUT_NAME` を実行ファイル側には書かない

- [ ] **Step 4: CMakePresets.json を書く**

`version` は 6。configure、build、test の 3 種を、`macos-clang`、`linux-gcc`、`linux-clang`、`windows-msvc` の 4 名で置く。`binaryDir` は 4 つとも `${sourceDir}/build`。`displayName` の文言は実装者に任せる。

macOS と Linux の configure は Ninja、`CMAKE_BUILD_TYPE` は `Release`。コンパイラは `macos-clang` が `clang++`、`linux-gcc` が `g++`、`linux-clang` が `clang++`。`condition` は `hostSystemName` がそれぞれ `Darwin`、`Linux`、`Linux` のときだけにする。

Windows の configure はジェネレータ `Visual Studio 17 2022`、`architecture` は `{ "value": "x64", "strategy": "set" }`、`condition` は `Windows`。`CMAKE_BUILD_TYPE` は置かない。

build preset と test preset は、4 名とも対応する configure preset を指し、`configuration` を `Release` にする。test preset は `outputOnFailure` を true にする。手元の CMake 4.4 では、Ninja の単一構成でもこの `configuration` で `ctest --preset` がテストを見つける。

別の preset へ切り替えるときは、`build/` を消してから configure する。同じディレクトリに別ジェネレータのキャッシュを残さない。

- [ ] **Step 5: macOS で configure と build を通す**

Run:

```bash
cmake --preset macos-clang
cmake --build --preset macos-clang
```

Expected: どちらも終了コード 0。`build/libmosa.a`、`build/mosa`、`build/mosac`、`build/mosatest` がある。

Run: `cmake --build build`

Expected: 終了コード 0。

- [ ] **Step 6: 起動の観測を、CTest にせず確認する**

リポジトリのルートで次を実行する。失敗したらソースを直し、このスクリプトは変えない。起動はパス付きなので、エラー行の行頭が `build/` になっていたら失敗である。この確認は CTest に登録しない。

```python
import subprocess
import sys

bins = [
    ("build/mosa", "mosa"),
    ("build/mosac", "mosac"),
    ("build/mosatest", "mosatest"),
]
version = b"mosa 0.0.1 (phase 0)\n"
failures = 0

def check(label, proc, exp_code, exp_out, exp_err):
    global failures
    if (proc.returncode, proc.stdout, proc.stderr) != (exp_code, exp_out, exp_err):
        failures += 1
        print(label, proc.returncode, proc.stdout, proc.stderr)

for path, name in bins:
    denied = f"{name}: このコマンドはフェーズ0では使えません\n".encode()
    cases = [
        (["--version"], 0, version, b""),
        ([], 1, b"", denied),
        (["-v"], 1, b"", denied),
        (["--version-foo"], 1, b"", denied),
        (["foo", "--version", "bar"], 0, version, b""),
    ]
    for args, exp_code, exp_out, exp_err in cases:
        proc = subprocess.run([path, *args], capture_output=True)
        check(f"{path} {args}", proc, exp_code, exp_out, exp_err)

if failures:
    sys.exit(1)
print("ok")
```

Expected: `ok`。終了コード 0。

その後に `ctest --test-dir build` を実行する。テスト未登録で終了コード 0 なら、STATUS に「CTest は未登録」と書く。非 0 なら、空のテストを足さずに、失敗と再現を STATUS に書いて止まる。

- [ ] **Step 7: STATUS を更新して Commit**

`docs/STATUS.md` を次で上書きする。Step 6 の ctest が非 0 だった場合は、その行を未解決に残す。

```markdown
# STATUS

フェーズ: 0
進行中: なし
完了: CMake。macOS で 3 実行ファイルの --version が `mosa 0.0.1 (phase 0)`
次: harness と 1 + 1 == 2
未解決: なし
確認: cmake --preset macos-clang と cmake --build --preset macos-clang が通った。CTest は未登録
```

```bash
git add CMakeLists.txt CMakePresets.json include/mosa/version.hpp \
  src/version.cpp src/main_mosa.cpp src/main_mosac.cpp src/main_mosatest.cpp \
  docs/STATUS.md
git commit -m "[phase0] CMake と --version を置く（macos-clang の configure と build が通る）"
```

---

### Task 3: harness と CTest

**Files:**
- Create: `tests/cpp/harness.hpp`、`tests/cpp/smoke_test.cpp`
- Modify: `CMakeLists.txt`、`docs/STATUS.md`
- Test: `ctest --preset macos-clang`

**Interfaces:**
- Consumes: タスク 2 の 3 実行ファイルと `version_line()` の出力
- Produces: CTest 名 `smoke`、`mosa_version`、`mosac_version`、`mosatest_version`

- [ ] **Step 1: 失敗するテストを先に登録する**

`tests/cpp/smoke_test.cpp` を次にする。

```cpp
#include "harness.hpp"

int main() {
  mosa_test::check(1 + 1 == 2, "1 + 1 == 2");
  return mosa_test::failures();
}
```

`CMakeLists.txt` に `enable_testing()` を足す。`smoke_test` は `tests/cpp/smoke_test.cpp` から作り、include ディレクトリは `tests/cpp`、`mosa_warnings` を PRIVATE でリンクする。`mosa_lib` にはリンクしない。

```cmake
add_test(NAME smoke COMMAND $<TARGET_FILE:smoke_test>)
```

版表示 3 件は、実行ファイルターゲットの `$<TARGET_FILE:...>` に引数 `--version` を渡す。プロパティ `PASS_REGULAR_EXPRESSION` は 3 件とも次の 1 つにする。

```text
^mosa 0\.0\.1 \(phase 0\)\r?\n?$
```

この正規表現は、手元の CMake 4.4 で、版の 1 行だけを通し、2 行目があると落とすことを確認済みである。テスト名は `mosa_version`、`mosac_version`、`mosatest_version`。4 件より増やさない。

- [ ] **Step 2: harness が無い状態でビルドが失敗することを確認する**

Run: `cmake --build --preset macos-clang`

Expected: `harness.hpp` が無いため失敗する。期待側の比較文字列は変えない。

- [ ] **Step 3: harness.hpp を 50 行以内で書く**

宣言は「タスクをまたいで使う契約」のとおり。失敗回数は、ヘッダ内の `inline` 関数の関数ローカル `static int` に置く。翻訳単位が分かれても回数を 1 つにするためである。偽のときだけ回数を増やし、`FAIL: ` を標準エラーへ出す。ファイルは 50 行以内。

- [ ] **Step 4: CTest を通す**

Run:

```bash
cmake --build --preset macos-clang
ctest --preset macos-clang --output-on-failure
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: 4 件とも成功。`smoke` の終了コードは 0。版表示 3 件は正規表現に一致する。

`wc -l tests/cpp/harness.hpp` が 50 以下であることも確認する。

- [ ] **Step 5: STATUS を更新して Commit**

```markdown
# STATUS

フェーズ: 0
進行中: なし
完了: harness と CTest 4 件（smoke と --version 3 件）
次: ci.yml
未解決: なし
確認: ctest --preset macos-clang が通った
```

```bash
git add tests/cpp/harness.hpp tests/cpp/smoke_test.cpp CMakeLists.txt docs/STATUS.md
git commit -m "[phase0] harness と 1+1==2 を CTest に載せる（ctest --preset macos-clang が通る）"
```

---

### Task 4: CI

**Files:**
- Create: `.github/workflows/ci.yml`
- Modify: `docs/STATUS.md`
- Test: workflow ファイルが YAML として読めること。4 ジョブの実行はタスク 6 まで無い

**Interfaces:**
- Consumes: タスク 2 の preset 名と、タスク 3 の CTest 4 件
- Produces: GitHub Actions のジョブ `macos`、`linux-gcc`、`linux-clang`、`windows`

- [ ] **Step 1: ci.yml を書く**

トリガーは `push` と `pull_request`。ジョブは 4 つ。各ジョブは checkout のあと、対応する preset で configure、build、test を行う。追加のセットアップアクションは Windows では使わない。

| ジョブ | runs-on | パッケージ | preset |
| --- | --- | --- | --- |
| `macos` | `macos-latest` | 入れない | `macos-clang` |
| `linux-gcc` | `ubuntu-24.04` | `g++` `ninja-build` `cmake` | `linux-gcc` |
| `linux-clang` | `ubuntu-24.04` | `clang` `ninja-build` `cmake` | `linux-clang` |
| `windows` | `windows-latest` | 入れない | `windows-msvc` |

Linux は `sudo apt-get update` のあと、そのジョブのパッケージだけを `apt-get install -y` する。コンパイラのパッチ版は固定しない。checkout は `actions/checkout@v4`。

各ジョブの最後に、`if: always()` かつ `shell: bash` で、次の 1 行を `GITHUB_STEP_SUMMARY` へ追記する。

```text
conformance: n/a (phase 0)
```

合格数の計算は書かない。テストが失敗したジョブを成功に変える設定は書かない。

- [ ] **Step 2: YAML として読めることを確認する**

Run: `ruby -ryaml -e 'YAML.load_file(".github/workflows/ci.yml"); puts "ok"'`

Expected: `ok`。4 ジョブはこの時点では走らせない。リモートがまだ無い。

- [ ] **Step 3: 手元の CTest がまだ通ることを確認する**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`

Expected: 4 件成功。CI の追加でビルド対象は増えない。

- [ ] **Step 4: STATUS を更新して Commit**

```markdown
# STATUS

フェーズ: 0
進行中: なし
完了: .github/workflows/ci.yml に 4 ジョブを書いた
次: ADR 0001
未解決: リモートがまだ無い
確認: workflow の YAML を読み込めた。3 OS の実行結果はまだ無い
```

```bash
git add .github/workflows/ci.yml docs/STATUS.md
git commit -m "[phase0] 4 ジョブの ci.yml を置く（手元は workflow の構文確認）"
```

---

### Task 5: ADR 0001

**Files:**
- Create: `docs/adr/0001-cell-representation.md`
- Modify: `docs/STATUS.md`
- Test: コード差分が空であること。セル表現の文が SPEC と一致すること

**Interfaces:**
- Consumes: `docs/SPEC.md` の第6節、表「ランタイム（`libmosa`）の核」の行「セル表現」
- Produces: フェーズ1が実装する前に浩さんが見る ADR。フェーズ0のコードは Cell を作らない

- [ ] **Step 1: ADR を転写する**

ファイル先頭に、状態が「フェーズ1着手前の承認待ち」であると書く。見出しは「背景」「決定」「捨てた案」「影響」の 4 つだけにする。

背景は 3 行以内で、次の意味にする。インタプリタと `mosac` は同じランタイムを共有する。項の表現がその共有の単位である。フェーズ1で実装する前に、表現を文書で固定する。

決定には、SPEC のセル表現の文を、句読点も記号も変えずに置く。`2^63−1` のマイナスは U+2212 である。ASCII のハイフンに置き換えない。文は次である。

```text
64bit タグ付き整数 `Cell`。下位 3bit がタグ: REF、ATOM、INT（61bit 即値）、STR（ヒープ内の関手セルへのポインタ）、LIS（リスト）、FLT（ボックス化 double）、BIG（将来予約）。61bit に収まらない整数は FLT と同様にボックス化し、`max_integer` は 2^63−1 のままにする
```

捨てた案は次の 1 行だけにする。WAM や NaN-boxing は書かない。

```text
指示書に比較案の記載がない。この ADR では追加しない。
```

影響は 3 行以内で、次の意味にする。フェーズ1はこの表現で Cell を実装する。変えるときは新しい ADR を書き、浩さんの承認を得る。フェーズ0のコードは Cell を実装しない。

- [ ] **Step 2: 転写と、コードが変わっていないことを確認する**

Run:

```bash
python3 - << 'PY'
from pathlib import Path
spec = Path("docs/SPEC.md").read_text()
adr = Path("docs/adr/0001-cell-representation.md").read_text()
start = spec.index("64bit タグ付き整数")
end = spec.index("のままにする", start) + len("のままにする")
sentence = spec[start:end]
if sentence not in adr:
    raise SystemExit("cell sentence differs")
if "\u2212" not in sentence:
    raise SystemExit("spec minus is not U+2212")
print("ok")
PY
git diff -- src include CMakeLists.txt CMakePresets.json tests .github
```

Expected: Python は `ok`。`git diff` は空である。`AGENTS.md`、`docs/SPEC.md`、元の指示書にも差分が無い。

Run: `cmake --build build && ctest --test-dir build --output-on-failure`

Expected: 4 件成功。

- [ ] **Step 3: STATUS を更新して Commit**

```markdown
# STATUS

フェーズ: 0
進行中: なし
完了: ADR 0001 に SPEC 第6節のセル表現を写した
次: GitHub へ公開し、4 ジョブの緑を確認
未解決: ADR 0001 は未承認。リモートは未作成
確認: コードは変えていない。ctest --test-dir build が通った
```

```bash
git add docs/adr/0001-cell-representation.md docs/STATUS.md
git commit -m "[phase0] ADR 0001 にセル表現を写す（コードは変えない）"
```

---

### Task 6: 公開と、4 ジョブが緑になったあとの STATUS

**Files:**
- Modify: `docs/STATUS.md`（4 ジョブが緑になったあとだけ）
- Test: GitHub Actions の 4 ジョブ

**Interfaces:**
- Consumes: タスク 4 の workflow と、タスク 5 までの `main`
- Produces: リモートの `main` と、フェーズ0完了の STATUS

このタスクは、利用者の答えが来るまで作成も push もしない。

- [ ] **Step 1: リポジトリ名と公開範囲を聞いて、待つ**

質問は 1 つにまとめる。提案は `SilentMalachite/mosa-prolog`、公開範囲は public か private か、である。2026-10-24 時点では、ログインアカウント SilentMalachite に同名のリポジトリは無かった。実行直前に `gh repo view` で、まだ無いかをもう一度見る。答えが来るまで Step 2 へ進まない。

- [ ] **Step 2: 作成して main を push する**

答えのとおりに `gh repo create` する。`--push` で `main` を送ってよい。force push はしない。名前を変える指示が無い限り、提案の名前を使う。

- [ ] **Step 3: 4 ジョブの完了を待つ**

`macos`、`linux-gcc`、`linux-clang`、`windows` の 4 つが緑になるまで、フェーズ0完了を STATUS に書かない。失敗したらテストの期待値を変えず、警告を無効化せず、論理単位ごとに直してコミットし、再度 push する。セッションを失敗のまま終えるときは、STATUS に失敗と再現手順を書く。

- [ ] **Step 4: 完了の STATUS を Commit**

4 ジョブが緑だったときだけ、次で上書きしてコミットする。

```markdown
# STATUS

フェーズ: 0 完了
進行中: なし
完了: 4 ジョブが緑。--version は `mosa 0.0.1 (phase 0)`
次: 浩さんが ADR 0001 を承認したら、フェーズ1（Cell とリーダ）
未解決: ADR 0001 は未承認
```

```bash
git add docs/STATUS.md
git commit -m "[phase0] 4 ジョブが緑なのでフェーズ0完了を STATUS に書く"
git push
```

- [ ] **Step 5: フェーズ0の出口を照合する**

次が全部取れていること。1 つでも欠けていたら、完了の STATUS を戻して原因を STATUS の未解決に書く。

- 手元で `cmake --preset macos-clang`、`cmake --build --preset macos-clang`、`ctest --preset macos-clang` が通る
- `cmake --build build && ctest --test-dir build` が通る
- 3 実行ファイルの `--version` が `mosa 0.0.1 (phase 0)` の 1 行と終了コード 0 である
- GitHub Actions の 4 ジョブが緑である
- ADR の決定が、SPEC のセル表現の文を U+2212 のまま含んでいる
- `AGENTS.md`、`docs/SPEC.md`、元の指示書の差分が、タスク 1 で入れた内容から空である
