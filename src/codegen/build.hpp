#pragma once

// mosac build と mosac run（SPEC 第6節「build and run pipeline」、計画 Task 9）。build.cpp が翻訳、コンパイル、リンクを
// 組み、cache.cpp が .mosac-cache/<hash>/ の鍵と置き場所を受け持つ。文面は lib/messages_*.pl の鍵（ADR 0007）。
// テスト: tests/mosac/build_test.cmake（CTest の mosac_build_hello など 6 件と、その _direct の 6 件）

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
};

// code が 0 なら exe（.mosac-cache/<hash>/ の中）が使える。ほかは mosac の終了コードで、診断は出し終えている。
struct Built {
  int code = 1;
  std::filesystem::path exe;
};

[[nodiscard]] Built build_cached(const BuildRequest& request);

// 実行時ライブラリと include/ の場所。今は Toolchain::runtime_dir（mosac の隣のビルド木）から引く。
// 計画 Task 10 がインストールの形に替えるので、引き方はこの 1 つの関数にだけ書く。
struct RuntimeLayout {
  std::filesystem::path include_dir;
  std::filesystem::path library;
};
[[nodiscard]] std::optional<RuntimeLayout> runtime_layout(const Toolchain& toolchain);

// 実行ファイルの拡張子（Windows は .exe）と、-o の名前に付けるかどうか。
[[nodiscard]] std::string exe_suffix();
[[nodiscard]] std::filesystem::path with_exe_suffix(const std::filesystem::path& path);

[[nodiscard]] std::string mosac_message(std::string_view key, std::vector<std::string> args = {});
// 書けなかった理由で鍵を選ぶ。ディスクの空きと権限は環境の問題として手順を出す（SPEC 第6節のエラーの扱い）。
void report_write_error(const std::filesystem::path& path, std::error_code error);
// コンパイラが見つからないとき。名指しのものが無ければその旨、名指しが無ければ OS ごとの導入手順。
void report_missing_toolchain(const ToolchainQuery& query);

// 以下は cache.cpp。
[[nodiscard]] std::uint64_t fnv1a(std::string_view bytes, std::uint64_t seed);
[[nodiscard]] std::string hex16(std::uint64_t value);

struct CacheKeyInputs {
  std::span<const ProjectFile> files;
  const Toolchain* toolchain = nullptr;
  const RuntimeLayout* runtime = nullptr;
  std::string mode;                // "direct" か "cmake <パス>"
  std::vector<std::string> flags;  // 置き場所に依らない旗だけ（-S や -o の先は鍵に入れない）
  std::string target;              // 実行ファイルの名前
};

struct CacheKey {
  std::string text;  // .mosac-cache/<hash>/key.txt に書く全文。当たりの判定はこれの一致で見る
  std::string hash;  // text の FNV-1a 64 の 16 桁
};

// 実行時ライブラリが読めなければ nullopt。
[[nodiscard]] std::optional<CacheKey> cache_key(const CacheKeyInputs& inputs);
// 今の作業場所の .mosac-cache。
[[nodiscard]] std::filesystem::path cache_root();
// dir/key.txt が key.text と同じで、dir/exe_name があれば真。
[[nodiscard]] bool cache_hit(const std::filesystem::path& dir, const CacheKey& key, const std::filesystem::path& exe_name);
// 内容を path に書く。失敗したら理由を返す。
[[nodiscard]] std::optional<std::error_code> write_text(const std::filesystem::path& path, std::string_view text);
// mosac run の一時ディレクトリ。名前は推し量れず、既にあれば別の名前で作り直す（排他的に作る）。
[[nodiscard]] std::optional<std::filesystem::path> fresh_temp_dir(std::error_code& error);

}  // namespace mosa::codegen
