#pragma once

// mosac build と mosac run（SPEC 第6節「build and run pipeline」、計画 Task 9）。build.cpp が翻訳、コンパイル、リンクを
// 組み、cache.cpp が .mosac-cache/<hash>/ の鍵と置き場所を受け持つ。実行時ライブラリの場所は runtime_layout.cpp
// （計画 Task 10）。文面は lib/messages_*.pl の鍵（ADR 0007）。
// テスト: tests/mosac/build_test.cmake（CTest の mosac_build_hello など 6 件と、その _direct の 6 件）、
// tests/mosac/standalone_test.cmake、tests/mosac/installed_layout_test.cmake

#include "codegen.hpp"

#include "mosa/toolchain.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace mosa::codegen {

struct BuildRequest {
  std::string source;              // コマンド行に書いた .pl のパス
  std::optional<std::string> cxx;  // --cxx
  bool debug = false;              // -O0 -g と節の追跡
  bool verbose = false;            // 起こす命令と、失敗したコンパイラの生の出力を出す
  bool clean = false;              // 作る前に .mosac-cache を消す
  bool interactive = false;        // --interactive。読み込んだあと mosa と同じ REPL を起こす実行ファイルにする
  bool runtime_source = false;     // --runtime-source。同梱の源から libmosa を作って使う（ABI が合わないとき）
};

// code が 0 なら exe（.mosac-cache/<hash>/ の中）が使える。ほかは mosac の終了コードで、診断は出し終えている。
struct Built {
  int code = 1;
  std::filesystem::path exe;
};

[[nodiscard]] Built build_cached(const BuildRequest& request);

// 実行時ライブラリの場所（計画 Task 10）。root に libmosa（library）、include/、翻訳済みの lib/*.pl の単位 pl/（units_dir）
// がある。runtime_layout.cpp がビルド木の形（mosac の隣）、インストールの形（<prefix>/lib/mosa/<OS>-<arch>-<compiler>/）、
// --runtime-source が同梱の源から作った物（.mosac-cache/runtime/<hash>/prefix/）の順に見る。
struct RuntimeLayout {
  std::filesystem::path root;
  std::filesystem::path include_dir;
  std::filesystem::path library;
  std::filesystem::path units_dir;
};
// 置き場所の鍵 <OS>-<arch>-<compiler>。compiler は clang、gcc、msvc のどれか（版は入れない）。
[[nodiscard]] std::string runtime_abi(const Toolchain& toolchain);
// 見つからなければ診断を出して nullopt（環境の問題。SPEC 第6節のエラーの扱い）。
[[nodiscard]] std::optional<RuntimeLayout> runtime_layout(const BuildRequest& request, const Toolchain& toolchain);
// 翻訳済みのライブラリ name の静的ライブラリ pl/libmosa_pl_<name>.a（MSVC は mosa_pl_<name>.lib）。
[[nodiscard]] std::filesystem::path unit_archive(const Toolchain& toolchain, const RuntimeLayout& layout,
                                                 std::string_view name);

// コンパイラが cl（MSVC）か。旗の形とライブラリの名前が変わる。
[[nodiscard]] bool is_msvc(const std::filesystem::path& cxx);
// `cmake version 3.31.2` の版が 3.25 以上か。
[[nodiscard]] bool cmake_new_enough(std::string_view line);
// 実行ファイルの拡張子（Windows は .exe）と、-o の名前に付けるかどうか。
[[nodiscard]] std::string exe_suffix();
[[nodiscard]] std::filesystem::path with_exe_suffix(const std::filesystem::path& path);

[[nodiscard]] std::string mosac_message(std::string_view key, std::vector<std::string> args = {});
// 書けなかった理由で鍵を選ぶ。ディスクの空きと権限は環境の問題として手順を出す（SPEC 第6節のエラーの扱い）。
void report_write_error(const std::filesystem::path& path, std::error_code error);
// コンパイラが見つからないとき。名指しのものが無ければその旨、名指しが無ければ OS ごとの導入手順。
void report_missing_toolchain(const ToolchainQuery& query);

// 以下は cache.cpp。
// FNV-1a 64 の種。鍵を作る側はみなここから始める（cache.cpp と runtime_layout.cpp）。
inline constexpr std::uint64_t kFnvBasis = 0xcbf29ce484222325ULL;
[[nodiscard]] std::uint64_t fnv1a(std::string_view bytes, std::uint64_t seed);
[[nodiscard]] std::string hex16(std::uint64_t value);
// ファイルの中身の FNV-1a。読めなければ nullopt。
[[nodiscard]] std::optional<std::uint64_t> file_digest(const std::filesystem::path& path);
// ディレクトリの下の全ファイル（隠しファイルを除く）の相対名と中身の FNV-1a（並びは名前順）。読めなければ nullopt。
[[nodiscard]] std::optional<std::uint64_t> tree_digest(const std::filesystem::path& dir);
// 同梱の源の鍵。dir の CMakeLists.txt、src/、include/、lib/（install の source の部品）だけを見る。
[[nodiscard]] std::optional<std::uint64_t> source_digest(const std::filesystem::path& dir);
// ファイルの全文（読めなければ空）。鍵の比較と、失敗したログの表示に使う。
[[nodiscard]] std::string read_text(const std::filesystem::path& path);
// 記号リンクを辿らずに見た種類が type か。キャッシュの当たりの判定は、どの段も記号リンクを当たりにしない。
[[nodiscard]] bool plain(const std::filesystem::path& path, std::filesystem::file_type type);

struct CacheKeyInputs {
  std::span<const ProjectFile> files;
  const Toolchain* toolchain = nullptr;
  const RuntimeLayout* runtime = nullptr;
  std::vector<std::filesystem::path> units;  // 結ぶ翻訳済みのライブラリ（中身のバイトを鍵に入れる）
  std::string mode;                          // "direct" か "cmake <パス>"
  std::vector<std::string> flags;            // 置き場所に依らない旗だけ（-S や -o の先は鍵に入れない）
  std::string target;                        // 実行ファイルの名前
};

struct CacheKey {
  std::string text;  // .mosac-cache/<hash>/key.txt に書く全文。当たりの判定はこれの一致で見る
  std::string hash;  // text の FNV-1a 64 の 16 桁
};

// 実行時ライブラリか翻訳済みのライブラリが読めなければ nullopt。
[[nodiscard]] std::optional<CacheKey> cache_key(const CacheKeyInputs& inputs);
// 今の作業場所の .mosac-cache。
[[nodiscard]] std::filesystem::path cache_root();
// .mosac-cache が記号リンクなら診断を出して偽（中身の入れ替えを防ぐため使わない）。
[[nodiscard]] bool cache_root_usable();
// dir/key.txt が key.text と同じで、dir/exe_name があれば真。
[[nodiscard]] bool cache_hit(const std::filesystem::path& dir, const CacheKey& key, const std::filesystem::path& exe_name);
// 内容を path に書く。失敗したら理由を返す。
[[nodiscard]] std::optional<std::error_code> write_text(const std::filesystem::path& path, std::string_view text);
// mosac run の一時ディレクトリ。名前は推し量れず、既にあれば別の名前で作り直す（排他的に作る）。
[[nodiscard]] std::optional<std::filesystem::path> fresh_temp_dir(std::error_code& error);

}  // namespace mosa::codegen
