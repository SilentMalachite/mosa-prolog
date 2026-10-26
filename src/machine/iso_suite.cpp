#include "mosa/suite.hpp"

#include "lgt_suite.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

namespace mosa {
namespace {

struct SideNote {
  std::string path;
  int count = 0;
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

// 範囲内は実行しないので、分母にも別欄にも足さない。tests.lgt が無い別欄も行を足さない。
void note_if_out_of_scope(const std::filesystem::path& dir, const std::filesystem::path& root,
                          std::vector<SideNote>& notes) {
  std::error_code error;
  const auto relative = std::filesystem::relative(dir, root, error);
  if (error) return;
  const std::string posix = relative.generic_string();
  if (posix.empty() || posix == ".") return;
  if (suite_in_scope(posix)) return;
  const auto tests = dir / "tests.lgt";
  if (!std::filesystem::is_regular_file(tests, error) || error) return;
  const int count = count_test_lines(tests);
  if (count < 0) return;
  notes.push_back(SideNote{posix, count});
}

void walk_out_of_scope(const std::filesystem::path& root, std::vector<SideNote>& notes) {
  std::error_code error;
  const auto options = std::filesystem::directory_options::skip_permission_denied;
  std::filesystem::recursive_directory_iterator cursor(root, options, error);
  if (error) return;
  const std::filesystem::recursive_directory_iterator end;
  while (cursor != end) {
    std::error_code entry_error;
    if (cursor->is_directory(entry_error) && !entry_error) note_if_out_of_scope(cursor->path(), root, notes);
    cursor.increment(error);
    if (error) return;
  }
}

// 分母 0 に百分率は無い。0% とも 0.0 とも書かない。
void write_report(const std::filesystem::path& path, const std::vector<SideNote>& notes) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output) return;
  constexpr int denominator = 0;
  constexpr int passed = 0;
  output << "このファイルは自動生成である。手で編集しない。\n";
  output << "分母: " << denominator << '\n';
  output << "合格数: " << passed << '\n';
  output << "合格率: 合格率なし\n";
  output << "不合格:\n";
  output << "別欄:\n";
  for (const SideNote& note : notes) output << note.path << ' ' << note.count << '\n';
}

}  // namespace

int run_iso_suite(std::string_view suite_dir, std::string_view report_path) {
  const std::filesystem::path prolog = std::filesystem::path{std::string{suite_dir}} / "logtalk" / "prolog";
  std::error_code error;
  if (!std::filesystem::is_directory(prolog, error) || error) return 1;
  std::vector<SideNote> notes;
  walk_out_of_scope(prolog, notes);
  std::sort(notes.begin(), notes.end(), [](const SideNote& left, const SideNote& right) {
    return left.path < right.path;
  });
  write_report(std::filesystem::path{std::string{report_path}}, notes);
  // このタスクはゴールを実行しない。分母が 0 のあいだ終了コードは 1。プロセスは終わらせない。
  return 1;
}

}  // namespace mosa
