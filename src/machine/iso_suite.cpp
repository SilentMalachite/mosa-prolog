#include "mosa/suite.hpp"

#include "lgt_suite.hpp"
#include "percent.hpp"

#include "mosa/heap.hpp"
#include "mosa/path_text.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
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

// 小数第 2 位が 5 のときは切り上げる（percent.hpp）。終了コードはこの表示を見ない。
std::string percentage(int passed, int denominator) {
  return percent_text(static_cast<std::size_t>(passed), static_cast<std::size_t>(denominator));
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

// README のバッジは shields.io の endpoint 形式でこの JSON を読む。色の閾値 90 は SPEC 第9節の
// フェーズ3完了条件に合わせ、それ未満を黄と赤に分ける。比較は percentage と同じく整数で行う。
std::string badge_color(int passed, int denominator) {
  const auto left = static_cast<long long>(passed) * 100;
  const auto right = static_cast<long long>(denominator);
  if (left >= right * 90) return "brightgreen";
  if (left >= right * 75) return "yellow";
  return "red";
}

void write_badge(const std::filesystem::path& path, int passed, int denominator) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output) return;
  std::string message = "no tests";
  std::string color = "lightgrey";
  if (denominator > 0) {
    message = percentage(passed, denominator) + "% (" + std::to_string(passed) + "/" + std::to_string(denominator) +
              ")";
    color = badge_color(passed, denominator);
  }
  output << R"({"schemaVersion":1,"label":"ISO conformance","message":")" << message << R"(","color":")" << color
         << "\"}\n";
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
    const LgtRun run = execute_lgt(lgt, utf8_from_path(item.file), folder_of(item.relative), {});
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

std::vector<Pending> sorted_pending(const std::filesystem::path& prolog, std::vector<SideNote>& notes) {
  std::vector<Pending> pending;
  walk_suite(prolog, notes, pending);
  std::sort(pending.begin(), pending.end(), [](const Pending& left, const Pending& right) {
    return left.relative < right.relative;
  });
  return pending;
}

}  // namespace

std::vector<std::filesystem::path> lgt_suite_files(const std::filesystem::path& prolog_root) {
  std::vector<SideNote> notes;
  std::vector<std::filesystem::path> files;
  for (const Pending& item : sorted_pending(prolog_root, notes)) files.push_back(item.file);
  return files;
}

SuiteArgs parse_suite_args(std::span<const std::string_view> args) {
  SuiteArgs parsed;
  if (args.empty() || args.front().empty()) return parsed;
  parsed.dir = std::string{args.front()};
  if (args.size() == 3 && args[1] == "--report" && !args[2].empty()) {
    parsed.report = std::string{args[2]};
    parsed.ok = true;
    return parsed;
  }
  if (args.size() != 1) return parsed;
  // 報告先を省けるのは、リポジトリ直下から同梱スイートを渡したときだけ。CI と README の手順がこの形で、
  // 別のスイートを試して docs の報告書とバッジの JSON を上書きする事故を防ぐ。
  std::error_code error;
  const bool bundled = std::filesystem::equivalent(path_from_utf8(parsed.dir), "tests/conformance", error);
  if (error || !bundled) return parsed;
  parsed.report = "docs/conformance.md";
  parsed.ok = true;
  return parsed;
}

int run_iso_suite(std::string_view suite_dir, std::string_view report_path) {
  const std::filesystem::path prolog = path_from_utf8(suite_dir) / "logtalk" / "prolog";
  std::error_code error;
  if (!std::filesystem::is_directory(prolog, error) || error) return 1;
  std::vector<SideNote> notes;
  const std::vector<Pending> pending = sorted_pending(prolog, notes);
  int passed = 0;
  int failed = 0;
  std::vector<std::string> failures;
  for (const Pending& item : pending) run_file(item, passed, failed, failures, notes);
  std::sort(notes.begin(), notes.end(), [](const SideNote& left, const SideNote& right) {
    if (left.path != right.path) return left.path < right.path;
    return left.detail < right.detail;
  });
  const int denominator = passed + failed;
  const std::filesystem::path report = path_from_utf8(report_path);
  write_report(report, passed, denominator, failures, notes);
  write_badge(std::filesystem::path{report}.replace_extension(".json"), passed, denominator);
  return suite_status(passed, denominator);
}

}  // namespace mosa
