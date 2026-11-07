#pragma once

// 子プロセスの起動と待ち合わせ（計画 Task 6、mosatest --bench）。定義は src/platform/process.cpp だけに置く。
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace mosa {

// argv[0] のプログラムを PATH から探して起こし、終わるまで待つ。引数は UTF-8。起こせなければ nullopt、
// 起きれば終了コード。log が空でなければ標準出力と標準エラーをそのファイルに上書きで書き、空なら親のものを継ぐ。
// env が空なら親の環境を継ぎ、空でなければ子の環境をその組だけにする（Toolchain::env、計画 Task 9）。
// PATH から探すのはどちらの場合も親の PATH なので、env の PATH で探させたいプログラムは絶対パスで渡す。
// input が空でなければそのファイルを子の標準入力にし、空なら親の標準入力を継ぐ（mosatest --manual、リリース 0.1.0）。
[[nodiscard]] std::optional<int> run_process(std::span<const std::string> argv, const std::filesystem::path& log,
                                             std::span<const std::pair<std::string, std::string>> env = {},
                                             const std::filesystem::path& input = {});

// run_process の終了コードが、子が自分で終わったのでなく落ちたことを示すか。POSIX はシグナル（run_process は
// シェルと同じく 128 + 番号を返すので、128 より大きい値。exit(129) 以上と区別できないのはシェルと同じ）、
// Windows は NTSTATUS のエラー（0xC0000000 以上。アクセス違反など）。
[[nodiscard]] bool exited_abnormally(int code);

// 親の環境の写し。set の名前は値を差し替え（無ければ後ろに足し）、unset の名前は除く。名前と値は UTF-8。
// Windows の名前は大文字と小文字を区別しないので、そこでは ASCII の大小を同じに見る。
[[nodiscard]] std::vector<std::pair<std::string, std::string>> environment_with(
    std::span<const std::pair<std::string, std::string>> set, std::span<const std::string> unset);

}
