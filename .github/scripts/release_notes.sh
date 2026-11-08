#!/usr/bin/env bash
# CHANGELOG.md の「## <版>」の節の本文を標準出力に書く（リリース 0.1.0 Task 10）。本文は見出しの次の行から次の「## 」の
# 見出しの前までで、見出しの行は Release の題名と重なるので入れない。release.yml の release ジョブが下書きの Release の
# 本文にし、linux ジョブがタグの実行の最初に同じ取り出しを試す。1 つのファイルにするのは、早い確かめと本文の取り出しが
# 同じ規則で節を探すようにするため（3 つのビルドの後で初めて節が無いと分かることがないように）。
# 節が無いか、空白しか無ければ 1 で終わる。本文の無い Release を作らないため。
# 節の中の相対のリンク（docs/manual/… など）は、そのタグの木の GitHub の URL に書き換える（リリース 0.1.0 Task 11 修正 1）。
# Release のページでは相対のリンクが開けないため。CHANGELOG.md そのものは、パッケージの中でリンクが通るよう相対のままにする。
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

# 「](先)」の先が http://、https://、#、mailto: のどれでもなければ、タグの木の URL を前に付ける。] ( ) を角括弧で
# 書くのは、\] や \( の意味が awk の実装で違いうるため（ランナーは mawk、手元の macOS は BSD awk）。
base="https://github.com/SilentMalachite/mosa-prolog/blob/v${version}/"
section=$(printf '%s\n' "$section" | awk -v base="$base" '
  {
    rest = $0
    out = ""
    while (match(rest, /[]][(][^)]*[)]/)) {
      target = substr(rest, RSTART + 2, RLENGTH - 3)
      if (target !~ /^(http:\/\/|https:\/\/|#|mailto:)/) target = base target
      out = out substr(rest, 1, RSTART - 1) "](" target ")"
      rest = substr(rest, RSTART + RLENGTH)
    }
    print out rest
  }
')
printf '%s\n' "$section"
