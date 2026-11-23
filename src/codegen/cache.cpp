// mosac build の中間物の置き場所 .mosac-cache/<hash>/（SPEC 第6節、計画 Task 9）。
// 鍵は翻訳した目録（原文を埋め込んだ main.cpp を含む）、実行時ライブラリ、コンパイラ、旗から作る。
// 各ビルドは <hash>/w-<16 進 8 桁>/ で作って動かさず、できたら <hash>/current がその名を指す（レビュー M-44）。当たりは
// current の指す物の key.txt の全文の一致と、できあがった実行ファイルがあることで見る。ハッシュの衝突でも古い物を出さない。
// テスト: tests/mosac/build_test.cmake（mosac_cache_reuse）、tests/mosac/build_parallel_test.cmake、tests/cpp/cache_place_test.cpp

#include "build.hpp"

#include "mosa/path_text.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <random>
#include <thread>

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

// 8 バイトの固定幅で混ぜる。長さの決まらない欄の境目にする。
std::uint64_t mix_word(std::uint64_t word, std::uint64_t hash) {
  std::array<char, 8> bytes{};
  for (std::size_t at = 0; at < bytes.size(); ++at) bytes[at] = static_cast<char>((word >> (8 * at)) & 0xFFU);
  return fnv1a(std::string_view{bytes.data(), bytes.size()}, hash);
}

// 生成コードが include する見出しも鍵に入れる。inline の関数だけを変えた版はライブラリのバイトが同じでありうる。
// 隠しファイル（.DS_Store、編集器の退避ファイル）と隠しディレクトリの中は入れない。install は .DS_Store を除くので、
// 同梱の源の鍵がビルド木（リポジトリの根）とインストールの lib/mosa/source/ で同じになる。1 つのファイルは
// パスの長さ、パス、中身だけの指紋の順に混ぜる。境目が無いと z の中身 zQ と zz の中身 Q が同じ指紋になる（レビュー L-36）。
std::optional<std::uint64_t> tree_digest_from(const std::filesystem::path& dir, std::uint64_t seed) {
  std::error_code error;
  std::vector<std::filesystem::path> files;
  // 種類を調べた誤りは、次の increment が消す前に見る。前は消され、行き先の無いシンボリックリンクを黙って飛ばした
  // 指紋を返した（レビュー R-L13）。
  for (std::filesystem::recursive_directory_iterator it{dir, error}, end; !error && it != end; it.increment(error)) {
    if (!utf8_from_path(it->path().filename()).starts_with('.')) {
      if (it->is_regular_file(error)) files.push_back(it->path());
    } else if (it->is_directory(error)) {
      it.disable_recursion_pending();
    }
    if (error) return std::nullopt;
  }
  if (error) return std::nullopt;
  // 並びは辿る順に依らせない。
  std::sort(files.begin(), files.end());
  std::uint64_t hash = seed;
  for (const std::filesystem::path& file : files) {
    const std::string name = utf8_from_path(file.lexically_relative(dir));
    hash = fnv1a(name, mix_word(name.size(), hash));
    const auto digest = file_digest_from(file, kFnvBasis);
    if (!digest) return std::nullopt;
    hash = mix_word(*digest, hash);
  }
  return hash;
}

// current を置く回数。名前替えが一時に通らないとき（Windows でほかの mosac が current を読んでいるとき）は back_off で
// 待つ。合わせて 2.75 秒で、それでも通らなければ書けないとして止める。
constexpr int kPublishAttempts = 10;

void back_off(int attempt) { std::this_thread::sleep_for(std::chrono::milliseconds{50 * (attempt + 1)}); }

bool present(const std::filesystem::path& path) {
  std::error_code error;
  return std::filesystem::exists(std::filesystem::symlink_status(path, error));
}

// current の中身として受ける名前は w-<16 進 8 桁> だけ。ファイルから読む名前なので、.. や区切りで置き場所の外を
// 指させない。
bool work_name(std::string_view name) {
  const auto hex = [](char unit) { return (unit >= '0' && unit <= '9') || (unit >= 'a' && unit <= 'f'); };
  return name.size() == 10 && name.starts_with("w-") && std::ranges::all_of(name.substr(2), hex);
}

bool older_than_a_day(const std::filesystem::path& path) {
  std::error_code error;
  const auto written = std::filesystem::last_write_time(path, error);
  return !error && std::filesystem::file_time_type::clock::now() - written > std::chrono::hours{24};
}

}  // namespace

// 16 進 8 桁。時刻と乱数と回数を混ぜる。CMake の深いオブジェクトのパスに足されるので短くし（Windows のパスの長さ）、
// 重なりは排他的に作ることで避ける。
std::string unique_suffix() {
  static std::atomic<std::uint32_t> counter{0};
  const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
  const std::string seed = std::to_string(ticks) + "-" + std::to_string(std::random_device{}()) + "-" +
                           std::to_string(counter++);
  return hex16(fnv1a(seed, kFnvBasis)).substr(8);
}

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
  std::string text = "mosac-cache 3\nmode " + inputs.mode + "\n";
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

std::optional<std::filesystem::path> current_dir(const std::filesystem::path& entry) {
  using Type = std::filesystem::file_type;
  if (!plain(entry, Type::directory) || !plain(entry / "current", Type::regular)) return std::nullopt;
  const std::string name = read_text(entry / "current");
  if (!work_name(name) || !plain(entry / name, Type::directory)) return std::nullopt;
  return entry / name;
}

bool build_finished(const std::filesystem::path& dir, const CacheKey& key, const std::filesystem::path& exe_name) {
  using Type = std::filesystem::file_type;
  if (!plain(dir / exe_name, Type::regular) || !plain(dir / "key.txt", Type::regular)) return false;
  return read_text(dir / "key.txt") == key.text;
}

// どれかが記号リンクなら外れにする。外れの後は新しい作業用のディレクトリで作り、publish_dir が current を置く。
std::optional<std::filesystem::path> cache_hit(const std::filesystem::path& entry, const CacheKey& key,
                                               const std::filesystem::path& exe_name) {
  if (!plain(entry.parent_path(), std::filesystem::file_type::directory)) return std::nullopt;
  const auto dir = current_dir(entry);
  if (!dir || !build_finished(*dir, key, exe_name)) return std::nullopt;
  return dir;
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

// 前のビルドの残りを消す（レビュー M-44）。前は外れのたびに置き場所ごと消したので、残りは鍵ごとに 1 組だった。
// 消すのは current が指さない w-* のうち、失敗の印 failed がある物（終わった物で、ログは次のビルドまで残る）と、
// 1 日より古い物（止められたビルド）、それに 1 日より古い current.tmp-*（止められた publish）だけ。同時に走っている
// 作りかけは新しいので残る。外れのときだけでなく当たりのときも呼ぶので、当たりが続いても残りは 1 日で消える。
void sweep_stale(const std::filesystem::path& entry) {
  const auto keep = current_dir(entry);
  std::error_code error;
  std::vector<std::filesystem::path> stale;
  for (std::filesystem::directory_iterator it{entry, error}, end; !error && it != end; it.increment(error)) {
    const std::filesystem::path path = it->path();
    const std::string name = utf8_from_path(path.filename());
    if (name.starts_with("current.tmp-") && plain(path, std::filesystem::file_type::regular)) {
      if (older_than_a_day(path)) stale.push_back(path);
      continue;
    }
    if (!work_name(name) || (keep && *keep == path) || !plain(path, std::filesystem::file_type::directory)) continue;
    if (older_than_a_day(path) || plain(path / "failed", std::filesystem::file_type::regular)) stale.push_back(path);
  }
  std::error_code ignored;
  for (const std::filesystem::path& path : stale) std::filesystem::remove_all(path, ignored);
}

void mark_failed(const std::filesystem::path& dir) {
  // 印を書けなくても失敗の知らせは変えない。残った物は 1 日たてば sweep_stale が消す。
  static_cast<void>(write_text(dir / "failed", ""));
}

// レビュー M-44。同じ鍵のビルドが同時に走っても互いの作りかけを消さないよう、各自が置き場所の中に自分の名で作る。
// 置き場所が記号リンクかファイル（置かれた物）なら、それだけを消す。辿った先には書かない。
std::optional<std::filesystem::path> private_dir(const std::filesystem::path& entry, std::error_code& error) {
  if (present(entry) && !plain(entry, std::filesystem::file_type::directory)) std::filesystem::remove(entry, error);
  if (!error) std::filesystem::create_directories(entry, error);
  if (error) return std::nullopt;
  sweep_stale(entry);
  for (int attempt = 0; attempt < 16; ++attempt) {
    const std::filesystem::path dir = entry / ("w-" + unique_suffix());
    if (std::filesystem::create_directory(dir, error)) return dir;
    if (error) return std::nullopt;
  }
  error = std::make_error_code(std::errc::file_exists);
  return std::nullopt;
}

// 守ること: 当たりを指す current は替えず、ディレクトリは動かさない（呼び手が中の実行ファイルを写し、--debug の
// デバッグ情報が作ったときのパスを指す）。消すのは、負けた自分の work だけ。初めて置く current は硬いリンクで
// 排他的に作り、同時に終えたビルドのうち 1 つだけが置く。置き換えるのは、あると分かってから読んで当たりを指さない
// current（壊れた物、消された物を指す物）だけで、指していた物は消さない（名前はファイルから読んだもので、当てにしない）。
std::optional<std::filesystem::path> publish_dir(const std::filesystem::path& entry, const std::filesystem::path& work,
                                                 const std::function<bool(const std::filesystem::path&)>& finished,
                                                 std::error_code& error) {
  const std::filesystem::path pointer = entry / "current";
  const std::filesystem::path temp = entry / ("current.tmp-" + unique_suffix());
  if (const auto failed = write_text(temp, utf8_from_path(work.filename()))) {
    // 書きかけの一時のファイルも消す。前は残し、同じ鍵の次のビルドまで残った（レビュー R-N05）。
    std::error_code removed;
    std::filesystem::remove(temp, removed);
    error = *failed;
    return std::nullopt;
  }
  std::error_code ignored;
  bool doubted = false;
  for (int attempt = 0; attempt < kPublishAttempts; ++attempt) {
    // あるかを先に見てから読む。current は硬いリンクか名前替えでしか置かないので、あると見た後に読んだ中身は
    // 書きかけではない。逆の順では、読んだ後に置かれた当たりを、無いと見て置き換えてしまう。
    const bool existed = present(pointer);
    if (const auto theirs = current_dir(entry); theirs && finished(*theirs)) {
      std::filesystem::remove(temp, ignored);
      if (*theirs != work) {
        // 消し切れない（Windows でほかのプロセスが中のファイルを開いている）ときは印を付け、次のビルドに消させる。
        std::error_code removed;
        std::filesystem::remove_all(work, removed);
        if (removed) mark_failed(work);
      }
      return theirs;
    }
    if (!existed) {
      std::filesystem::create_hard_link(temp, pointer, error);
      if (!error) {
        std::filesystem::remove(temp, ignored);
        return work;
      }
      if (present(pointer)) continue;  // 先を越された。次の回でその current を読む
      // 硬いリンクを作れないファイルシステム（exFAT など）は名前替えで置く。2 つが同時に置くと、先の物の work は
      // 誰も指さずに残る（呼び手が写しているので消さない）。
    } else if (!plain(pointer, std::filesystem::file_type::regular)) {
      // ディレクトリの current には名前替えで置けない。mosac が置いた物ではないので消す（記号リンクはリンクだけ）。
      std::filesystem::remove_all(pointer, error);
      if (error) back_off(attempt);
      continue;
    } else if (!doubted) {
      // 当たりを指さない current は、もう 1 度読んでから置き換える。Windows では、勝ったビルドが一時のファイル（current と
      // 同じファイルの硬いリンク）を消す間、current を開けずに読みが空になることがある。
      doubted = true;
      back_off(attempt);
      continue;
    }
    std::filesystem::rename(temp, pointer, error);
    if (!error) return work;
    back_off(attempt);
  }
  std::filesystem::remove(temp, ignored);
  return std::nullopt;
}

}  // namespace mosa::codegen
