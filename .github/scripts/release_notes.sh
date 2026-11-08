#!/usr/bin/env bash
# CHANGELOG.md の「## <版>」の節の本文を標準出力に書く（リリース 0.1.0 Task 10）。本文は見出しの次の行から次の「## 」の
# 見出しの前までで、見出しの行は Release の題名と重なるので入れない。release.yml の release ジョブが下書きの Release の
# 本文にし、linux ジョブがタグの実行の最初に同じ取り出しを試す。1 つのファイルにするのは、早い確かめと本文の取り出しが
# 同じ規則で節を探すようにするため（3 つのビルドの後で初めて節が無いと分かることがないように）。
# 節が無いか、空白しか無ければ 1 で終わる。本文の無い Release を作らないため。
# 使い方: bash .github/scripts/release_notes.sh <版> [CHANGELOG のパス（既定は CHANGELOG.md）]
set -euo pipefail

if [ "$#" -lt 1 ] || [ "$#" -gt 2 ] || [ -z "$1" ]; then
  echo "usage: $0 <version> [CHANGELOG.md]" >&2
  exit 2
fi
version=$1
changelog=${2:-CHANGELOG.md}
if [ ! -f "$changelog" ]; then
  echo "release_notes: $changelog does not exist" >&2
  exit 1
fi

# 見出しは「## 0.1.0 (2026-11-10)」の形なので、欄で比べる（正規表現にすると版の . が任意の 1 字になる）。
if ! section=$(awk -v version="$version" '
    $1 == "##" && $2 == version { found = 1; next }
    found && /^## / { exit }
    found { print }
    END { if (!found) exit 1 }
  ' "$changelog"); then
  echo "release_notes: $changelog has no '## $version' section" >&2
  exit 1
fi
if [[ ! $section =~ [^[:space:]] ]]; then
  echo "release_notes: the '## $version' section of $changelog is empty" >&2
  exit 1
fi
printf '%s\n' "$section"
