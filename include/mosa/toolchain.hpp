#pragma once

// C++ コンパイラと CMake の検出（SPEC 第6節「Toolchain handling」、計画 Task 8）。定義は src/platform/toolchain.cpp。
// テスト: tests/cpp/toolchain_test.cpp
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mosa {

enum class HostOs { macos, linux, windows };

struct Toolchain {
  std::filesystem::path cxx;
  std::string cxx_version;
  std::optional<std::filesystem::path> cmake;
  // `cmake --version` の 1 行目（find_toolchain が埋める）。起こせなければ空で、mosac build は CMake を使わない。
  std::string cmake_version;
  std::filesystem::path runtime_dir;
  // 空なら親の環境を継ぐ。Windows で vswhere から MSVC を見つけたときは、vcvarsall を通した後の環境全体を入れる。
  std::vector<std::pair<std::string, std::string>> env;
};

struct ToolchainQuery {
  // mosac の --cxx。
  std::optional<std::string> cxx;
};

// 探索の入力をすべて引数で受ける版（find_library_in と同じ形）。子プロセスを起こさず、ファイルの有無だけを見る。
struct ToolchainSearch {
  std::optional<std::string> cxx_flag;
  std::optional<std::string> mosa_cxx;
  std::optional<std::string> cxx_env;
  // PATH を区切りで分けたもの。区切り（: か ;）の判断は呼び手が持つ。
  std::vector<std::filesystem::path> path;
  HostOs os = HostOs::linux;
  std::filesystem::path runtime_dir;
};

// 名前を与えられたコンパイラと、どこで与えられたか（"--cxx"、"MOSA_CXX"、"CXX" のどれか）。
struct NamedCompiler {
  std::string program;
  std::string_view source;
};

// 動いている OS。
[[nodiscard]] HostOs host_os();
// mosac 自身の CPU の名前（arm64、x86_64）。実行時ライブラリの置き場所 <OS>-<arch>-<compiler> の arch（計画 Task 10）。
[[nodiscard]] std::string_view host_arch();
// mosac の在るディレクトリ。ビルド木ではここに libmosa と include/ と pl/ があり、インストールの形では
// ../lib/mosa/<OS>-<arch>-<compiler>/ にある（codegen/runtime_layout.cpp が両方を見る。計画 Task 10）。
[[nodiscard]] std::filesystem::path default_runtime_dir();

// cxx_flag → mosa_cxx → cxx_env のうち最初に与えられたもの。どれも無ければ nullopt（OS の既定で探す）。
[[nodiscard]] std::optional<NamedCompiler> named_compiler(const ToolchainSearch& search);
// --cxx と環境変数 MOSA_CXX、CXX、PATH を読んで探索の入力を作る。find_toolchain もこれを使う。
[[nodiscard]] ToolchainSearch host_toolchain_search(const ToolchainQuery& query);

// 順は cxx_flag → mosa_cxx → cxx_env → OS の既定。名前の付いたものが見つからなければ既定へは落ちず nullopt。
// cxx_version は空のまま返す（版は起動して取るので find_toolchain が埋める）。
[[nodiscard]] std::optional<Toolchain> find_toolchain_in(const ToolchainSearch& search);
// 環境変数と PATH を読み、版を `<cxx> --version`（MSVC は cl の見出し）から取る。Windows の既定は vswhere。
[[nodiscard]] std::optional<Toolchain> find_toolchain(const ToolchainQuery& query);

// 以下は Windows の道の文字列処理。macOS でも単体で試せるよう、起動から切り離して出す。
[[nodiscard]] std::vector<std::filesystem::path> split_path_list(std::string_view text, HostOs os);
// vswhere -property installationPath の出力の最初の空でない行。
[[nodiscard]] std::optional<std::filesystem::path> parse_vswhere_output(std::string_view text);
// cmd の set の出力（NAME=VALUE の行）。= で始まる行（=C: など）は飛ばす。
[[nodiscard]] std::vector<std::pair<std::string, std::string>> parse_set_dump(std::string_view text);
// cmd に渡すパスは OS の区切りのまま出す（cmd は / を切り替えの印と読み、C:/a/b.bat を命令 C: と引数に分ける）。
// utf8_from_path が / にそろえるのと対になる。
[[nodiscard]] std::string native_path_text(const std::filesystem::path& path);
// cl（MSVC）か。パスの語幹を大文字小文字を区別せずに見る（cl、CL.EXE、C:\VS\cl.exe）。
[[nodiscard]] bool is_msvc(const std::filesystem::path& cxx);
// cl を vcvarsall の環境無しで（名指しの cl は vcvarsall を通さない）、INCLUDE も無いまま動かそうとしているか。
// そのとき mosac build はコンパイルの前に開発者用のプロンプトを案内して止まる。
[[nodiscard]] bool needs_developer_prompt(const std::filesystem::path& cxx, bool has_vcvars_env, bool include_set);
// .bat の call の行で "..." に入れて書けるパスか（% と " を含まない）。書けない vcvarsall は使わず PATH を見る。
[[nodiscard]] bool cmd_quotable_path(const std::filesystem::path& path);
// vcvarsall.bat を x64 で呼んでから set を出す .bat の中身（行末は \r\n）。
[[nodiscard]] std::string vcvars_script(const std::filesystem::path& vcvarsall);
// cmd /u が書く UTF-16LE を UTF-8 にする。対になっていないサロゲートは U+FFFD、奇数の最後の 1 バイトは捨てる。
[[nodiscard]] std::string utf8_from_utf16le(std::string_view bytes);
// cl を引数なしで起こした出力の見出し「Microsoft (R) C/C++ Optimizing Compiler Version …」の行。ファイルに向けると
// 使い方の行が先に出るので、Microsoft で始まる最初の行を取り、無ければ最初の空でない行。
[[nodiscard]] std::string msvc_banner_line(std::string_view text);

}
