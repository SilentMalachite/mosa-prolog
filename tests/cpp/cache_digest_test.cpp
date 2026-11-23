// レビュー L-36。実行時ライブラリの見出しの木の指紋（tree_digest、cache_key の include の行と --runtime-source の鍵）を
// 確かめる。隠しファイルと同じく隠しディレクトリの中も入れない。パスと中身の間とファイルの間に境目を入れるので、
// include/z の中身 zQ と include/zz の中身 Q、a と b の 2 つと中身 XbY の a だけ、を取り違えない。前は隠しディレクトリの
// 中のファイルで指紋が変わり、境目が無いので取り違えたキャッシュが当たった。
#include "harness.hpp"

#include "codegen/build.hpp"

#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

namespace {

namespace fs = std::filesystem;

using Files = std::initializer_list<std::pair<const char*, const char*>>;

void write_file(const fs::path& path, const std::string& text) {
  fs::create_directories(path.parent_path());
  std::ofstream out{path, std::ios::binary};
  out << text;
}

std::optional<std::uint64_t> digest_of(const fs::path& dir, Files files) {
  fs::create_directories(dir);
  for (const auto& [name, text] : files) write_file(dir / name, text);
  return mosa::codegen::tree_digest(dir);
}

void hidden_entries(const fs::path& root) {
  const fs::path dir = root / "hidden";
  const auto before = digest_of(dir, {{"mosa/a.h", "x"}});
  mosa_test::check(before.has_value(), "tree_digest reads a plain tree");
  write_file(dir / ".hid" / "x.h", "y");
  mosa_test::check(mosa::codegen::tree_digest(dir) == before, "a file under a hidden directory is left out");
  write_file(dir / "mosa" / ".cache" / "deep" / "y.h", "z");
  mosa_test::check(mosa::codegen::tree_digest(dir) == before, "a nested hidden directory is left out");
  write_file(dir / "mosa" / ".DS_Store", "w");
  mosa_test::check(mosa::codegen::tree_digest(dir) == before, "a hidden file is still left out");
  write_file(dir / "mosa" / "sub" / "b.h", "v");
  mosa_test::check(mosa::codegen::tree_digest(dir) != before, "a file under a plain directory is counted");
}

void boundaries(const fs::path& root) {
  const auto short_name = digest_of(root / "path_short", {{"z", "zQ"}});
  const auto long_name = digest_of(root / "path_long", {{"zz", "Q"}});
  mosa_test::check(short_name && long_name && short_name != long_name,
                   "the path and the content are kept apart (z holding zQ, zz holding Q)");
  const auto two = digest_of(root / "files_two", {{"a", "X"}, {"b", "Y"}});
  const auto one = digest_of(root / "files_one", {{"a", "XbY"}});
  mosa_test::check(two && one && two != one, "two files are kept apart (a and b, a holding XbY)");
  const auto same = digest_of(root / "files_same", {{"a", "X"}, {"b", "Y"}});
  mosa_test::check(same == two, "the same names and contents give the same digest in another directory");
  const auto changed = digest_of(root / "files_changed", {{"a", "X"}, {"b", "Z"}});
  mosa_test::check(changed && changed != two, "a changed content changes the digest");
}

// レビュー R-L13。種類を調べられない項目（行き先の無いシンボリックリンク）があれば、指紋を作らない。前は
// is_regular_file の誤りを次の increment が消し、その項目を黙って飛ばした指紋を返した。
void unreadable_entry(const fs::path& root) {
  const fs::path dir = root / "dangling";
  write_file(dir / "a.hpp", "A");
  std::error_code error;
  fs::create_symlink(dir / "missing.hpp", dir / "b.hpp", error);
  if (error) return;  // シンボリックリンクを作れない環境（権限の無い Windows）では見ない。
  mosa_test::check(!mosa::codegen::tree_digest(dir).has_value(), "an entry whose type cannot be read stops the digest");
  // 読めなかった項目を呼び手へ返す（レビュー RV-m11）。前は mosac build がある libmosa.a を「見つからない」と言った。
  fs::path failed;
  mosa_test::check(!mosa::codegen::tree_digest(dir, &failed).has_value() && failed == dir / "b.hpp",
                   "the digest names the entry it could not read: " + failed.string());
  // 隠しの項目は入れないので、行き先の無い隠しのリンク（Emacs のロックの .#a.hpp）では止めない（L-36 と同じ）。
  const fs::path hidden = root / "dangling_hidden";
  write_file(hidden / "a.hpp", "A");
  const auto plain = mosa::codegen::tree_digest(hidden);
  fs::create_symlink(hidden / "gone", hidden / ".#a.hpp", error);
  mosa_test::check(plain.has_value() && mosa::codegen::tree_digest(hidden) == plain,
                   "a dangling hidden link is still left out");
}

}  // namespace

int main() {
  const fs::path root = fs::path{MOSA_SOURCE_DIR} / "tests" / "tmp" / "cache_digest";
  fs::remove_all(root);
  fs::create_directories(root);
  hidden_entries(root);
  boundaries(root);
  unreadable_entry(root);
  fs::remove_all(root);
  return mosa_test::failures();
}
