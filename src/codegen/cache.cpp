// mosac build の中間物の置き場所 .mosac-cache/<hash>/（SPEC 第6節、計画 Task 9）。
// 鍵は翻訳した目録（原文を埋め込んだ main.cpp を含む）、実行時ライブラリ、コンパイラ、旗から作る。
// 当たりは key.txt の全文の一致と、できあがった実行ファイルがあることで見る。ハッシュの衝突でも古い物を出さない。
// テスト: tests/mosac/build_test.cmake（mosac_cache_reuse）

#include "build.hpp"

#include "mosa/path_text.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>
#include <random>

namespace mosa::codegen {
namespace {

// 実行時ライブラリは大きさと更新時刻ではなく中身のバイトで鍵にする。同じ秒の中の作り直しや、時刻を保って
// 写したライブラリを取り違えないためである。数 MB を読む手間はコンパイル 1 回に比べて小さい。
std::optional<std::uint64_t> file_digest_from(const std::filesystem::path& path, std::uint64_t seed) {
  std::ifstream in{path, std::ios::binary};
  if (!in) return std::nullopt;
  std::array<char, 1 << 16> buffer{};
  std::uint64_t hash = seed;
  while (in) {
    in.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    hash = fnv1a(std::string_view{buffer.data(), static_cast<std::size_t>(in.gcount())}, hash);
  }
  if (in.bad()) return std::nullopt;
  return hash;
}

// 生成コードが include する見出しも鍵に入れる。inline の関数だけを変えた版はライブラリのバイトが同じでありうる。
// 隠しファイル（.DS_Store、編集器の退避ファイル）は入れない。install は .DS_Store を除くので、同梱の源の鍵が
// ビルド木（リポジトリの根）とインストールの lib/mosa/source/ で同じになる。
std::optional<std::uint64_t> tree_digest_from(const std::filesystem::path& dir, std::uint64_t seed) {
  std::error_code error;
  std::vector<std::filesystem::path> files;
  for (std::filesystem::recursive_directory_iterator it{dir, error}, end; !error && it != end; it.increment(error)) {
    if (it->is_regular_file(error) && !utf8_from_path(it->path().filename()).starts_with('.')) files.push_back(it->path());
  }
  if (error) return std::nullopt;
  // 並びは辿る順に依らせない。
  std::sort(files.begin(), files.end());
  std::uint64_t hash = seed;
  for (const std::filesystem::path& file : files) {
    hash = fnv1a(utf8_from_path(file.lexically_relative(dir)), hash);
    const auto digest = file_digest_from(file, hash);
    if (!digest) return std::nullopt;
    hash = *digest;
  }
  return hash;
}

}  // namespace

bool plain(const std::filesystem::path& path, std::filesystem::file_type type) {
  std::error_code error;
  const std::filesystem::file_status status = std::filesystem::symlink_status(path, error);
  return !error && status.type() == type;
}

std::string read_text(const std::filesystem::path& path) {
  std::ifstream in{path, std::ios::binary};
  return std::string{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
}

std::string hex16(std::uint64_t value) {
  constexpr std::string_view digits = "0123456789abcdef";
  std::string out;
  for (int shift = 60; shift >= 0; shift -= 4) out += digits[(value >> shift) & 0x0FU];
  return out;
}

std::optional<std::uint64_t> file_digest(const std::filesystem::path& path) { return file_digest_from(path, kFnvBasis); }

std::optional<std::uint64_t> tree_digest(const std::filesystem::path& dir) { return tree_digest_from(dir, kFnvBasis); }

// --runtime-source の鍵（計画 Task 10）。源の 4 か所だけを見る。ビルド木の形では dir がリポジトリの根なので、木ごと
// 読むと .git、build/、tests/tmp/ と .mosac-cache 自身（作るたびに変わる）が入り、いつまでも当たらない。
std::optional<std::uint64_t> source_digest(const std::filesystem::path& dir) {
  std::uint64_t hash = kFnvBasis;
  const auto top = file_digest_from(dir / "CMakeLists.txt", hash);
  if (!top) return std::nullopt;
  hash = *top;
  for (const char* part : {"src", "include", "lib"}) {
    const auto digest = tree_digest_from(dir / part, fnv1a(part, hash));
    if (!digest) return std::nullopt;
    hash = *digest;
  }
  return hash;
}

// 目録の全文を鍵にする。main.cpp が読んだファイルの原文を埋め込むので原文の変化を拾い、翻訳器が変わって
// 出すコードが変わったときも拾う。--debug（旗と kTrace）と --cxx（パスと版）も鍵に入り、別の物を使い回さない。
// 結ぶ翻訳済みのライブラリも中身で鍵にする（計画 Task 10）。
std::optional<CacheKey> cache_key(const CacheKeyInputs& inputs) {
  const auto library = file_digest(inputs.runtime->library);
  const auto headers = tree_digest(inputs.runtime->include_dir);
  if (!library || !headers) return std::nullopt;
  std::string text = "mosac-cache 2\nmode " + inputs.mode + "\n";
  text += "cxx " + utf8_from_path(inputs.toolchain->cxx) + "\nversion " + inputs.toolchain->cxx_version + "\n";
  for (const std::string& flag : inputs.flags) text += "flag " + flag + "\n";
  text += "runtime " + utf8_from_path(inputs.runtime->library) + " " + hex16(*library) + "\n";
  text += "include " + utf8_from_path(inputs.runtime->include_dir) + " " + hex16(*headers) + "\n";
  for (const std::filesystem::path& unit : inputs.units) {
    const auto digest = file_digest(unit);
    if (!digest) return std::nullopt;
    text += "unit " + utf8_from_path(unit) + " " + hex16(*digest) + "\n";
  }
  text += "target " + inputs.target + "\n";
  for (const ProjectFile& file : inputs.files) text += "file " + file.name + " " + hex16(fnv1a(file.text, kFnvBasis)) + "\n";
  return CacheKey{text, hex16(fnv1a(text, kFnvBasis))};
}

// 作業場所の隣に置く（SPEC 第6節）。読めなければ相対の名前のまま使う。.mosac-cache はほかの利用者と共有する
// ディレクトリ（/tmp など）に置いてはならない。中身の入れ替えを防げるのは、そこへ書けるのが自分だけのときだけである。
std::filesystem::path cache_root() {
  std::error_code error;
  const std::filesystem::path cwd = std::filesystem::current_path(error);
  return error ? std::filesystem::path{".mosac-cache"} : cwd / ".mosac-cache";
}

bool cache_root_usable() {
  std::error_code error;
  if (!std::filesystem::is_symlink(std::filesystem::symlink_status(cache_root(), error))) return true;
  std::cerr << mosac_message("mosac_cache_symlink", {utf8_from_path(cache_root())}) << '\n';
  return false;
}

// どれかが記号リンクなら外れにする。外れの後の stage がハッシュのディレクトリを作り直す。
bool cache_hit(const std::filesystem::path& dir, const CacheKey& key, const std::filesystem::path& exe_name) {
  using Type = std::filesystem::file_type;
  if (!plain(dir.parent_path(), Type::directory) || !plain(dir, Type::directory)) return false;
  if (!plain(dir / exe_name, Type::regular) || !plain(dir / "key.txt", Type::regular)) return false;
  return read_text(dir / "key.txt") == key.text;
}

// ofstream は理由を返さない。開くか書くかに失敗した直後の errno を理由にする（POSIX も MSVC の CRT も設定する）。
std::optional<std::error_code> write_text(const std::filesystem::path& path, std::string_view text) {
  // 記号リンクの先へは書かない。
  std::error_code status_error;
  if (std::filesystem::is_symlink(std::filesystem::symlink_status(path, status_error))) {
    return std::make_error_code(std::errc::file_exists);
  }
  errno = 0;
  std::ofstream out{path, std::ios::binary};
  if (out) out.write(text.data(), static_cast<std::streamsize>(text.size()));
  if (out) out.close();
  if (!out.fail()) return std::nullopt;
  const int code = errno;
  return std::error_code{code != 0 ? code : EIO, std::generic_category()};
}

// 共有の一時の場所に決まった名前を使わない（bench.cpp の件）。時刻と乱数と回数を混ぜた名前を create_directory で
// 作り、既にあれば（ほかの利用者が先に置いた名前でも）使わずに別の名前を試す。作れたら自分だけが読めるようにする。
std::optional<std::filesystem::path> fresh_temp_dir(std::error_code& error) {
  static std::atomic<std::uint32_t> counter{0};
  const std::filesystem::path base = std::filesystem::temp_directory_path(error);
  if (error) return std::nullopt;
  for (int attempt = 0; attempt < 16; ++attempt) {
    const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::string name = "mosac-run-" + std::to_string(ticks) + "-" + std::to_string(std::random_device{}()) +
                             "-" + std::to_string(counter++);
    const std::filesystem::path dir = base / name;
    if (std::filesystem::create_directory(dir, error)) {
      // 権限を絞れない（Windows の ACL など）のは作れたことを覆さない。名前の推し量れなさが主の守りである。
      std::error_code ignored;
      std::filesystem::permissions(dir, std::filesystem::perms::owner_all, std::filesystem::perm_options::replace,
                                   ignored);
      return dir;
    }
    if (error) return std::nullopt;
  }
  error = std::make_error_code(std::errc::file_exists);
  return std::nullopt;
}

}  // namespace mosa::codegen
