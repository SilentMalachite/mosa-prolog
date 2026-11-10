#!/usr/bin/env bash
# README.md の「Build from source」の手順を、書いてあるとおりに 1 行 1 コマンドで標準出力に出す（SPEC 第 9 節、
# フェーズ 7 の完了条件「README の手順だけで初見の人がビルドできる」。持ち主の決定 A、2026-11-10）。
# ci.yml の readme-linux と readme-windows がこれを流す。手で打ち直さずに README から取るのは、README を直したときに
# CI が古い手順を確かめ続けないようにするため。形が変わって取れなくなったら、黙って何もしないのではなく落とす。
# 使い方:
#   bash .github/scripts/readme_steps.sh apt [README]                    Ubuntu 24.04 の apt の行
#   bash .github/scripts/readme_steps.sh build <preset> [<commit>] [README] Build and test の sh のブロック
# build は README の例の preset（macos-clang）を <preset> に替える。README は macOS の例を見せ、ほかの OS では
# preset を選ぶよう書いているため。<commit> を渡すと、cd の行の後でその版に移る。clone は既定の枝の先を取るので、
# プルリクエストや後の push のときに、確かめている版とずれないようにする。
set -euo pipefail

usage() {
  echo "usage: $0 apt [README] | $0 build <preset> [<commit>] [README]" >&2
  exit 2
}

[ "$#" -ge 1 ] || usage
mode=$1
shift

apt_line() {
  local readme=$1 line
  line=$(sed -n 's/.*On Ubuntu 24\.04, `\([^`]*\)`.*/\1/p' "$readme")
  if [ -z "$line" ] || [ "$(printf '%s\n' "$line" | wc -l | tr -d ' ')" -ne 1 ]; then
    echo "readme_steps: $readme has no single 'On Ubuntu 24.04, \`...\`' line" >&2
    exit 1
  fi
  case "$line" in
    "sudo apt "*) ;;
    *) echo "readme_steps: the Ubuntu line of $readme does not start with 'sudo apt': $line" >&2; exit 1 ;;
  esac
  printf '%s\n' "$line"
}

# 見出し「### Build and test」から次の「## 」の見出しまでの、```sh のブロックの空でない行。
build_lines() {
  local readme=$1
  awk '/^### Build and test/ { on = 1; next }
       on && /^## / { on = 0 }
       on && /^```sh/ { block = 1; next }
       on && /^```/ { block = 0; next }
       on && block && NF { print }' "$readme"
}

case "$mode" in
  apt)
    [ "$#" -le 1 ] || usage
    apt_line "${1:-README.md}"
    ;;
  build)
    [ "$#" -ge 1 ] && [ "$#" -le 3 ] || usage
    preset=$1
    commit=${2:-}
    readme=${3:-README.md}
    lines=$(build_lines "$readme")
    # README の手順の形: clone、cd、configure、build、test の 5 行。どれかが欠けたら手順が変わったので落とす。
    for want in '^git clone https://' '^cd [^ ]+$' '^cmake --preset macos-clang$' \
                '^cmake --build --preset macos-clang$' '^ctest --preset macos-clang$'; do
      if ! printf '%s\n' "$lines" | grep -E -q "$want"; then
        echo "readme_steps: the Build and test steps of $readme have no line matching '$want':" >&2
        printf '%s\n' "$lines" >&2
        exit 1
      fi
    done
    if [ "$(printf '%s\n' "$lines" | wc -l | tr -d ' ')" -ne 5 ]; then
      echo "readme_steps: expected 5 commands in the Build and test steps of $readme, got:" >&2
      printf '%s\n' "$lines" >&2
      exit 1
    fi
    printf '%s\n' "$lines" | while IFS= read -r line; do
      printf '%s\n' "${line//--preset macos-clang/--preset $preset}"
      case "$line" in
        "cd "*)
          if [ -n "$commit" ]; then
            printf '%s\n' "git fetch --quiet origin $commit" "git checkout --quiet --detach $commit"
          fi
          ;;
      esac
    done
    ;;
  *)
    usage
    ;;
esac
