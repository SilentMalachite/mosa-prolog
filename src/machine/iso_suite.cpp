#include "mosa/suite.hpp"
#include "mosa/heap_option.hpp"

#include "lgt_suite.hpp"
#include "percent.hpp"

#include "mosa/heap.hpp"
#include "mosa/messages.hpp"
#include "mosa/path_text.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
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

std::string rate_text(int passed, int denominator) {
  return percentage(passed, denominator) + "% (" + std::to_string(passed) + "/" + std::to_string(denominator) + ")";
}

struct Baseline {
  int passed = 0;
  int denominator = 0;
};

std::optional<int> count_in(std::string_view text) {
  int value = 0;
  const char* const last = text.data() + text.size();
  const auto parsed = std::from_chars(text.data(), last, value);
  if (parsed.ec != std::errc{} || parsed.ptr != last) return std::nullopt;
  return value;
}

// 前回のバッジ JSON（write_badge が書く 1 行）から合格数と分母を読む。無い・読めない・形が違うときは
// 物差しなしで、90% の下限だけが残る。
std::optional<Baseline> read_baseline(const std::filesystem::path& badge) {
  std::ifstream input(badge, std::ios::binary);
  std::string line;
  if (!input || !std::getline(input, line)) return std::nullopt;
  const std::string_view text = line;
  const auto open = text.find('(');
  const auto slash = open == std::string_view::npos ? open : text.find('/', open);
  const auto close = slash == std::string_view::npos ? slash : text.find(')', slash);
  if (close == std::string_view::npos) return std::nullopt;
  const auto passed = count_in(text.substr(open + 1, slash - open - 1));
  const auto denominator = count_in(text.substr(slash + 1, close - slash - 1));
  if (!passed || !denominator || *passed < 0 || *denominator <= 0 || *passed > *denominator) return std::nullopt;
  return Baseline{*passed, *denominator};
}

// 合格率が物差しより下か。表示の丸めで比べると 1 件の後退を見逃すので、分数のまま掛け比べて比べる。
bool regressed(int passed, int denominator, const Baseline& baseline) {
  if (denominator <= 0) return false;
  const auto left = static_cast<long long>(passed) * baseline.denominator;
  const auto right = static_cast<long long>(baseline.passed) * denominator;
  return left < right;
}

// 報告書はコミットする生成物。不合格行の絶対パス（^^file_path のスクラッチ等）を相対へ
// 直さないと機械ごとに差分が出る（全体レビュー推奨3）。
std::string relativized(std::string line, const std::vector<std::string>& bases) {
  for (const std::string& base : bases) {
    if (base.empty()) continue;
    std::size_t at = 0;
    while ((at = line.find(base, at)) != std::string::npos) line.erase(at, base.size());
  }
  return line;
}

std::vector<std::string> report_bases(const std::filesystem::path& prolog) {
  std::vector<std::string> bases;
  const auto add = [&bases](const std::filesystem::path& dir) {
    if (!dir.empty()) bases.push_back(dir.generic_string() + "/");
  };
  std::error_code error;
  add(std::filesystem::current_path(error));
  // スクラッチは tests の下（lgt_io_context）。tests の親を落とせば tests/tmp/... の相対になる。
  auto cursor = std::filesystem::absolute(prolog, error);
  while (!error && !cursor.empty()) {
    if (cursor.filename() == "tests") {
      add(cursor.parent_path());
      break;
    }
    const auto parent = cursor.parent_path();
    if (parent == cursor) break;
    cursor = parent;
  }
  return bases;
}

bool write_report(const std::filesystem::path& path, int passed, int denominator,
                  const std::vector<std::string>& failures, const std::vector<SideNote>& notes,
                  const std::vector<std::string>& bases) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output) return false;
  output << "このファイルは自動生成である。手で編集しない。\n";
  output << "分母: " << denominator << '\n';
  output << "合格数: " << passed << '\n';
  if (denominator == 0) output << "合格率: 合格率なし\n";
  else output << "合格率: " << percentage(passed, denominator) << '\n';
  output << "不合格:\n";
  for (const std::string& line : failures) output << relativized(line, bases) << '\n';
  output << "別欄:\n";
  for (const SideNote& note : notes) output << note.path << ' ' << note.detail << '\n';
  output.close();
  return static_cast<bool>(output);
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

bool write_badge(const std::filesystem::path& path, int passed, int denominator) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output) return false;
  std::string message = "no tests";
  std::string color = "lightgrey";
  if (denominator > 0) {
    message = rate_text(passed, denominator);
    color = badge_color(passed, denominator);
  }
  output << R"({"schemaVersion":1,"label":"ISO conformance","message":")" << message << R"(","color":")" << color
         << "\"}\n";
  output.close();
  return static_cast<bool>(output);
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
  } catch (...) {
    if (!is_heap_exhaustion()) throw;
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
  // 実行中はカレントがスクラッチへ移るので、相対化の基準は走らせる前に取る。
  const std::vector<std::string> bases = report_bases(prolog);
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
  const std::filesystem::path badge = std::filesystem::path{report}.replace_extension(".json");
  // 書き換える前の JSON が物差し。後退した回は報告書（不合格の一覧）だけ書いて JSON を残すので、
  // 直すまで何度流しても失敗し、後退した値のコミットで通り抜けられない（SPEC 第8節）。
  const std::optional<Baseline> baseline = read_baseline(badge);
  const bool down = baseline && regressed(passed, denominator, *baseline);
  // 書けなかった報告書は合格率がいくつでも失敗にする。黙って 0 で終えると、古い報告書とバッジが残る（Muse のレビューの中）。
  bool written = true;
  if (!write_report(report, passed, denominator, failures, notes, bases)) {
    std::cerr << render_message_fresh(default_message_language(), "bench_cannot_write",
                                      std::array{utf8_from_path(report)})
              << '\n';
    written = false;
  }
  if (down) {
    std::cerr << render_message_fresh(default_message_language(), "conformance_regressed",
                                      std::array{rate_text(baseline->passed, baseline->denominator),
                                                 rate_text(passed, denominator)})
              << '\n';
  } else if (!write_badge(badge, passed, denominator)) {
    std::cerr << render_message_fresh(default_message_language(), "bench_cannot_write",
                                      std::array{utf8_from_path(badge)})
              << '\n';
    written = false;
  }
  return written && !down ? suite_status(passed, denominator) : 1;
}

}  // namespace mosa
