#include "mosa/suite.hpp"

#include "lgt_suite.hpp"

#include "mosa/heap.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace mosa {
namespace {

struct SideNote {
  std::string path;
  std::string detail;
};

struct Pending {
  std::string relative;
  std::filesystem::path file;
};

// 別欄は項にしない。test( を含む行だけ数える。読めなければ件数は無い。
int count_test_lines(const std::filesystem::path& file) {
  std::ifstream input(file, std::ios::binary);
  if (!input) return -1;
  int count = 0;
  std::string line;
  while (std::getline(input, line)) {
    if (line.find("test(") != std::string::npos) ++count;
  }
  return count;
}

void consider_directory(const std::filesystem::path& dir, const std::filesystem::path& root,
                        std::vector<SideNote>& notes, std::vector<Pending>& pending) {
  std::error_code error;
  const auto relative = std::filesystem::relative(dir, root, error);
  if (error) return;
  const std::string posix = relative.generic_string();
  if (posix.empty() || posix == ".") return;
  const auto tests = dir / "tests.lgt";
  if (!std::filesystem::is_regular_file(tests, error) || error) return;
  if (suite_in_scope(posix)) {
    pending.push_back(Pending{posix + "/tests.lgt", tests});
    return;
  }
  const int count = count_test_lines(tests);
  if (count < 0) return;
  notes.push_back(SideNote{posix, std::to_string(count)});
}

void walk_suite(const std::filesystem::path& root, std::vector<SideNote>& notes, std::vector<Pending>& pending) {
  std::error_code error;
  const auto options = std::filesystem::directory_options::skip_permission_denied;
  std::filesystem::recursive_directory_iterator cursor(root, options, error);
  if (error) return;
  const std::filesystem::recursive_directory_iterator end;
  while (cursor != end) {
    std::error_code entry_error;
    if (cursor->is_directory(entry_error) && !entry_error) {
      consider_directory(cursor->path(), root, notes, pending);
    }
    cursor.increment(error);
    if (error) return;
  }
}

std::string percentage(int passed, int denominator) {
  const auto den = static_cast<long long>(denominator);
  // 小数第 2 位が 5 のときは切り上げる。終了コードはこの表示を見ない。
  const auto tenths = (static_cast<long long>(passed) * 2000 + den) / (den * 2);
  return std::to_string(tenths / 10) + "." + std::to_string(tenths % 10);
}

int suite_status(int passed, int denominator) {
  if (denominator <= 0) return 1;
  const auto left = static_cast<long long>(passed) * 10;
  const auto right = static_cast<long long>(denominator) * 9;
  return left >= right ? 0 : 1;
}

void write_report(const std::filesystem::path& path, int passed, int denominator,
                  const std::vector<std::string>& failures, const std::vector<SideNote>& notes) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output) return;
  output << "このファイルは自動生成である。手で編集しない。\n";
  output << "分母: " << denominator << '\n';
  output << "合格数: " << passed << '\n';
  if (denominator == 0) output << "合格率: 合格率なし\n";
  else output << "合格率: " << percentage(passed, denominator) << '\n';
  output << "不合格:\n";
  for (const std::string& line : failures) output << line << '\n';
  output << "別欄:\n";
  for (const SideNote& note : notes) output << note.path << ' ' << note.detail << '\n';
}

std::string folder_of(std::string relative) {
  constexpr std::string_view suffix = "/tests.lgt";
  if (relative.size() >= suffix.size() && relative.ends_with(suffix)) {
    relative.resize(relative.size() - suffix.size());
  }
  return relative;
}

void run_file(const Pending& item, int& passed, int& failed, std::vector<std::string>& failures,
              std::vector<SideNote>& notes) {
  try {
    std::ifstream input(item.file, std::ios::binary);
    if (!input) {
      failures.push_back(item.relative);
      ++failed;
      return;
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    const LgtFile lgt = read_lgt_file(buffer.str());
    if (!lgt.error.empty()) {
      failures.push_back(item.relative);
      ++failed;
      return;
    }
    const LgtRun run = execute_lgt(lgt, item.file.string(), folder_of(item.relative));
    passed += run.passed;
    failed += run.failed;
    for (const std::string& line : run.failures) failures.push_back(item.relative + " " + line);
    const std::string folder = folder_of(item.relative);
    for (const std::string& name : run.outside) notes.push_back(SideNote{folder, name});
  } catch (const ResourceLimit&) {
    failures.push_back(item.relative);
    ++failed;
  }
}

}  // namespace

int run_iso_suite(std::string_view suite_dir, std::string_view report_path) {
  const std::filesystem::path prolog = std::filesystem::path{std::string{suite_dir}} / "logtalk" / "prolog";
  std::error_code error;
  if (!std::filesystem::is_directory(prolog, error) || error) return 1;
  std::vector<SideNote> notes;
  std::vector<Pending> pending;
  walk_suite(prolog, notes, pending);
  std::sort(pending.begin(), pending.end(), [](const Pending& left, const Pending& right) {
    return left.relative < right.relative;
  });
  int passed = 0;
  int failed = 0;
  std::vector<std::string> failures;
  for (const Pending& item : pending) run_file(item, passed, failed, failures, notes);
  std::sort(notes.begin(), notes.end(), [](const SideNote& left, const SideNote& right) {
    if (left.path != right.path) return left.path < right.path;
    return left.detail < right.detail;
  });
  const int denominator = passed + failed;
  write_report(std::filesystem::path{std::string{report_path}}, passed, denominator, failures, notes);
  return suite_status(passed, denominator);
}

}  // namespace mosa
