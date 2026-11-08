#!/usr/bin/env bash
# タグの版と mosa --version の版が同じことを確かめる（リリース 0.1.0 Task 10）。release.yml の 3 つのジョブが、
# ビルドの後、書庫を作る前に呼ぶ。書庫の名前の版は CMakeCache.txt の CMAKE_PROJECT_VERSION から来るので、
# CMakeLists.txt の版を上げ忘れてタグを打つと v0.1.1 の Release に mosa-0.1.0-… が付く。それを書庫を作る前に止める。
# YAML に埋めずにファイルにするのは、タグを打たずに手元で同じ確かめを流せるようにするため。tools/ に置かないのは、
# AGENTS.md の配置で tools/ が開発者だけが流す物の場所で、これは CI が流すため。
# 使い方: GITHUB_REF_NAME=v<版> bash .github/scripts/check_tag_version.sh <mosa の実行ファイル>
set -euo pipefail

if [ "$#" -ne 1 ]; then
  echo "usage: GITHUB_REF_NAME=v<version> $0 <path to mosa>" >&2
  exit 2
fi
mosa=$1
tag=${GITHUB_REF_NAME:-}
case "$tag" in
  v?*) ;;
  *)
    echo "check_tag_version: GITHUB_REF_NAME must be a tag such as v0.1.0, not '$tag'" >&2
    exit 2
    ;;
esac
expected=${tag#v}

# Windows で行末が CR LF になっても、版の比べ方を 3 OS で同じにするため CR を外す。
if ! line=$("$mosa" --version | tr -d '\r'); then
  echo "check_tag_version: '$mosa --version' failed" >&2
  exit 1
fi
actual=${line#mosa }
if [ "$actual" = "$line" ]; then
  echo "check_tag_version: '$mosa --version' printed '$line', not 'mosa <version>'" >&2
  exit 1
fi
if [ "$actual" != "$expected" ]; then
  echo "check_tag_version: the tag $tag is version $expected, but $mosa --version says $actual" >&2
  exit 1
fi
echo "check_tag_version: the tag $tag and $mosa --version agree on $actual"
