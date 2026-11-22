// レビュー M-44。mosac build と --runtime-source は <hash>/w-*/ で作り、publish_dir で <hash>/current にその名を置く。
// 同時に流す確かめ（tests/mosac/build_parallel_test.cmake）は空のキャッシュから始まり、初めて置く道しか通らないので、
// current が既にある道と同時に置く道をここで確かめる: 当たりを指す current は替えずに自分の work を消す、当たりを
// 指さない current（.. や区切り、記号リンク、ディレクトリ、消された物）は外れで、自分の work のほかは消さない、
// 同時に置くと 1 つだけが置き、ほかは負けて消える、前のビルドの残りは失敗の印か古さで消え、作りかけは残る。
#include "harness.hpp"

#include "codegen/build.hpp"

#include <filesystem>
#include <fstream>
#include <chrono>
#include <iterator>
#include <optional>
#include <string>
#include <thread>
#include <vector>
#include <system_error>

namespace {

namespace fs = std::filesystem;

void write_file(const fs::path& path, const std::string& text) {
  std::ofstream out{path, std::ios::binary};
  out << text;
}

std::string read_file(const fs::path& path) {
  std::ifstream in{path, std::ios::binary};
  return std::string{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
}

// 当たりは done があるディレクトリ（build_finished の実行ファイルと key.txt の代わり）。記号リンクは辿らない。
bool finished(const fs::path& dir) {
  std::error_code error;
  return fs::symlink_status(dir / "done", error).type() == fs::file_type::regular;
}

// entry の中に作業用のディレクトリを作り、mark を書く。done は当たりのときだけ置く。
fs::path make_work(const fs::path& entry, const std::string& mark, bool done) {
  std::error_code error;
  const auto work = mosa::codegen::private_dir(entry, error);
  mosa_test::check(work.has_value() && !error, "private_dir makes a directory");
  if (!work) return {};
  const std::string name = work->filename().string();
  mosa_test::check(work->parent_path() == entry && name.size() == 10 && name.starts_with("w-"),
                   "private_dir names it <entry>/w-<8 hex digits>");
  write_file(*work / "mark", mark);
  if (done) write_file(*work / "done", "");
  return *work;
}

std::string current_mark(const fs::path& entry) {
  const auto dir = mosa::codegen::current_dir(entry);
  return dir ? read_file(*dir / "mark") : std::string{"<none>"};
}

// entry に残るのは current と、名を挙げた作業用のディレクトリだけ（current.tmp-* は残らない）。
void expect_entries(const fs::path& entry, std::size_t dirs, const std::string& label) {
  std::error_code error;
  std::size_t found = 0;
  bool stray = false;
  for (const fs::directory_entry& item : fs::directory_iterator{entry, error}) {
    const std::string name = item.path().filename().string();
    if (name.starts_with("w-")) {
      ++found;
    } else if (name != "current") {
      stray = true;
    }
  }
  mosa_test::check(!error && found == dirs && !stray, label + ": the entry holds current and the expected w-* only");
}

std::optional<fs::path> publish(const fs::path& entry, const fs::path& work) {
  std::error_code error;
  return mosa::codegen::publish_dir(entry, work, finished, error);
}

void first_and_later(const fs::path& entry) {
  const fs::path first = make_work(entry, "first", true);
  mosa_test::check(publish(entry, first) == first, "first: a finished work is published");
  mosa_test::check(current_mark(entry) == "first", "first: current names it");
  const fs::path second = make_work(entry, "second", true);
  mosa_test::check(publish(entry, second) == first, "later: the earlier finished entry is returned");
  mosa_test::check(!fs::exists(second), "later: the losing work is removed");
  mosa_test::check(current_mark(entry) == "first", "later: current is not replaced");
  expect_entries(entry, 1, "later");
}

// private_dir は、current が指さない w-* のうち失敗の印がある物と 1 日より古い物を消し、current の指す物と
// 新しい作りかけ（同時に走っているビルド）は残す。
void sweep(const fs::path& entry) {
  const fs::path good = make_work(entry, "good", true);
  static_cast<void>(publish(entry, good));
  const fs::path failed = make_work(entry, "failed", false);
  mosa::codegen::mark_failed(failed);
  const fs::path old = make_work(entry, "old", false);
  fs::last_write_time(old, fs::file_time_type::clock::now() - std::chrono::hours{48});
  const fs::path live = make_work(entry, "live", false);
  const fs::path fresh = make_work(entry, "fresh", false);
  mosa_test::check(!fs::exists(failed), "sweep: a failed build's directory is removed");
  mosa_test::check(!fs::exists(old), "sweep: a directory older than a day is removed");
  mosa_test::check(fs::exists(live / "mark") && fs::exists(fresh / "mark"), "sweep: a build in progress is kept");
  mosa_test::check(current_mark(entry) == "good" && fs::exists(good / "mark"), "sweep: the published entry is kept");
}

// 同じ entry へ 4 つのビルドが同時に置く。どれも同じ 1 つを返し、current はそれを指し、負けた 3 つは消える。
void concurrent(const fs::path& root) {
  for (int round = 0; round < 50; ++round) {
    const fs::path entry = root / ("round" + std::to_string(round));
    std::vector<fs::path> works;
    for (int index = 0; index < 4; ++index) works.push_back(make_work(entry, "w" + std::to_string(index), true));
    std::vector<std::optional<fs::path>> results(works.size());
    std::vector<std::thread> threads;
    for (std::size_t index = 0; index < works.size(); ++index) {
      threads.emplace_back([&, index] { results[index] = publish(entry, works[index]); });
    }
    for (std::thread& thread : threads) thread.join();
    const auto winner = mosa::codegen::current_dir(entry);
    bool same = winner.has_value();
    for (const auto& result : results) same = same && result == winner;
    mosa_test::check(same, "concurrent: every build gets the one published entry");
    expect_entries(entry, 1, "concurrent");
  }
}

// 当たりを指さない current は外れ。置き換えて自分の work を指し、指していた物も置き場所の外も消さない。
void invalid_current(const fs::path& root, const std::string& text, const std::string& label) {
  const fs::path entry = root / label;
  const fs::path outside = root / (label + "-outside");
  fs::create_directories(outside);
  write_file(outside / "mark", "outside");
  fs::create_directories(entry);
  write_file(outside / "done", "");
  write_file(entry / "current", text);
  mosa_test::check(!mosa::codegen::current_dir(entry), label + ": an invalid current is a miss");
  const fs::path work = make_work(entry, "ours", true);
  mosa_test::check(publish(entry, work) == work, label + ": an invalid current is replaced");
  mosa_test::check(current_mark(entry) == "ours", label + ": current names the new work");
  mosa_test::check(read_file(outside / "mark") == "outside", label + ": nothing outside the entry is removed");
}

// current がディレクトリなら外れ。名前替えでは置けないので消してから置く。
void invalid_dir_current(const fs::path& root) {
  const fs::path entry = root / "dir_current";
  fs::create_directories(entry / "current");
  write_file(entry / "current" / "junk", "");
  const fs::path work = make_work(entry, "ours", true);
  mosa_test::check(publish(entry, work) == work, "dir current: a directory current is replaced");
  mosa_test::check(current_mark(entry) == "ours", "dir current: current names the new work");
}

// 置き場所が記号リンクかファイルなら、private_dir はそれだけを消して作り直し、リンクの先は残す。
void invalid_entry(const fs::path& root) {
  const fs::path file_entry = root / "file_entry";
  write_file(file_entry, "not a directory");
  static_cast<void>(make_work(file_entry, "ours", true));
  mosa_test::check(fs::is_directory(fs::symlink_status(file_entry)), "file entry: replaced by a directory");
  const fs::path target = root / "entry_target";
  fs::create_directories(target);
  write_file(target / "mark", "target");
  const fs::path link_entry = root / "link_entry";
  std::error_code error;
  fs::create_directory_symlink(target, link_entry, error);
  if (error) return;
  static_cast<void>(make_work(link_entry, "ours", true));
  mosa_test::check(!fs::is_symlink(fs::symlink_status(link_entry)), "link entry: the link is replaced by a directory");
  mosa_test::check(read_file(target / "mark") == "target" && std::distance(fs::directory_iterator{target}, {}) == 1,
                   "link entry: nothing is written into the link's target");
}

// current が記号リンクなら外れ。名前替えはリンクそのものを替え、リンクの先は残す。リンクを作れない所では飛ばす。
void symlink_current(const fs::path& root) {
  const fs::path entry = root / "link";
  fs::create_directories(entry);
  const fs::path decoy = make_work(entry, "decoy", true);
  write_file(root / "pointer", decoy.filename().string());
  std::error_code error;
  fs::create_symlink(root / "pointer", entry / "current", error);
  if (error) return;
  mosa_test::check(!mosa::codegen::current_dir(entry), "symlink: a symlinked current is a miss");
  const fs::path work = make_work(entry, "ours", true);
  mosa_test::check(publish(entry, work) == work, "symlink: the link is replaced");
  mosa_test::check(!fs::is_symlink(fs::symlink_status(entry / "current")) && current_mark(entry) == "ours",
                   "symlink: current is a plain file naming the new work");
  mosa_test::check(read_file(root / "pointer") == decoy.filename().string(), "symlink: the link's target is kept");
}

}  // namespace

int main() {
  const fs::path root = fs::path{MOSA_SOURCE_DIR} / "tests" / "tmp" / "cache_place";
  fs::remove_all(root);
  fs::create_directories(root);
  first_and_later(root / "later");
  sweep(root / "sweep");
  concurrent(root / "concurrent");
  invalid_dir_current(root);
  invalid_entry(root);
  invalid_current(root, "", "empty");
  invalid_current(root, "../empty-outside", "parent");
  invalid_current(root, "w-0000000/", "separator");
  invalid_current(root, "w-zzzzzzzz", "not_hex");
  invalid_current(root, "w-0000aaaa", "missing");
  symlink_current(root);
  fs::remove_all(root);
  return mosa_test::failures();
}
