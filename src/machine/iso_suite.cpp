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
  SuiteColumn column = SuiteColumn::Iso;
};

// 欄ごとの集計。本欄とライブラリ欄は分母も不合格の一覧も別に持つ（フェーズ11の設計のアンチ目標）。
struct Tally {
  int passed = 0;
  int failed = 0;
  std::vector<std::string> failures;
  [[nodiscard]] int denominator() const { return passed + failed; }
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
  if (const std::optional<SuiteColumn> column = suite_column(posix)) {
    pending.push_back(Pending{posix + "/tests.lgt", tests, *column});
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
  // generic_string は Windows で ANSI コードページへ変換し、不合格行（UTF-8）に当たらなくなる（L7）。
  const auto add = [&bases](const std::filesystem::path& dir) {
    if (!dir.empty()) bases.push_back(utf8_from_path(dir) + "/");
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
  // 長い基準から消す。カレントがリポジトリの親だと、短い方が先に消えて途中までのパスが残る（L7）。
  std::sort(bases.begin(), bases.end(), [](const std::string& a, const std::string& b) { return a.size() > b.size(); });
  return bases;
}

// 本欄は見出しに前置きを付けない。ライブラリ欄は「ライブラリ欄の」を付け、行頭の「合格率:」が本欄だけになる
// （CI の Job Summary が grep -m 1 '^合格率:' で読む）。
void write_column(std::ostream& output, std::string_view prefix, const Tally& tally,
                  const std::vector<std::string>& bases) {
  const int denominator = tally.denominator();
  output << prefix << "分母: " << denominator << '\n';
  output << prefix << "合格数: " << tally.passed << '\n';
  if (denominator == 0) output << prefix << "合格率: 合格率なし\n";
  else output << prefix << "合格率: " << percentage(tally.passed, denominator) << '\n';
  output << prefix << "不合格:\n";
  for (const std::string& line : tally.failures) output << relativized(line, bases) << '\n';
}

// ライブラリ欄は本欄の節と別欄の一覧の後ろに置く。前からの節の並びと読み方を変えない。別欄は両方の欄の分をまとめる。
bool write_report(const std::filesystem::path& path, const Tally& iso, const Tally& library,
                  const std::vector<SideNote>& notes, const std::vector<std::string>& bases) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output) return false;
  output << "このファイルは自動生成である。手で編集しない。\n";
  write_column(output, "", iso, bases);
  output << "別欄:\n";
  for (const SideNote& note : notes) output << note.path << ' ' << note.detail << '\n';
  write_column(output, "ライブラリ欄の", library, bases);
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

bool write_badge(const std::filesystem::path& path, std::string_view label, int passed, int denominator) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output) return false;
  std::string message = "no tests";
  std::string color = "lightgrey";
  if (denominator > 0) {
    message = rate_text(passed, denominator);
    color = badge_color(passed, denominator);
  }
  output << R"({"schemaVersion":1,"label":")" << label << R"(","message":")" << message << R"(","color":")"
         << color << "\"}\n";
  output.close();
  return static_cast<bool>(output);
}

void say_unwritable(const std::filesystem::path& path) {
  std::cerr << render_message_fresh(default_message_language(), "bench_cannot_write", std::array{utf8_from_path(path)})
            << '\n';
}

// 欄ごとのバッジ JSON と後退の判定。本欄とライブラリ欄で同じ形にする（フェーズ11の設計）。
struct BadgeGate {
  std::filesystem::path path;
  std::string_view label;
  std::string_view regressed_key;
  std::optional<Baseline> baseline;
};

// 書き換える前の JSON が物差し。報告書の名前によっては同じファイルを指しうるので、報告書を書く前に読む。
BadgeGate open_gate(std::filesystem::path path, std::string_view label, std::string_view regressed_key) {
  const std::optional<Baseline> baseline = read_baseline(path);
  return BadgeGate{std::move(path), label, regressed_key, baseline};
}

// 後退した回は報告書（不合格の一覧）だけ書いて JSON を残すので、直すまで何度流しても失敗し、
// 後退した値のコミットで通り抜けられない（SPEC 第8節）。後退したら真を返す。
bool close_gate(const BadgeGate& gate, const Tally& tally, bool& written) {
  const int denominator = tally.denominator();
  if (gate.baseline && regressed(tally.passed, denominator, *gate.baseline)) {
    std::cerr << render_message_fresh(default_message_language(), gate.regressed_key,
                                      std::array{rate_text(gate.baseline->passed, gate.baseline->denominator),
                                                 rate_text(tally.passed, denominator)})
              << '\n';
    return true;
  }
  if (!write_badge(gate.path, gate.label, tally.passed, denominator)) {
    say_unwritable(gate.path);
    written = false;
  }
  return false;
}

// 報告書の隣の <名前>-library.json。同梱スイートなら docs/conformance-library.json。
// 文字列で組まず path のまま足す。generic_string は Windows で ANSI コードページへ変換する（L7）。
std::filesystem::path library_badge_path(const std::filesystem::path& report) {
  std::filesystem::path name = report.stem();
  name += "-library.json";
  return std::filesystem::path{report}.replace_filename(name);
}

std::string folder_of(std::string relative) {
  constexpr std::string_view suffix = "/tests.lgt";
  if (relative.size() >= suffix.size() && relative.ends_with(suffix)) {
    relative.resize(relative.size() - suffix.size());
  }
  return relative;
}

void run_file(const Pending& item, Tally& tally, std::vector<SideNote>& notes) {
  try {
    std::ifstream input(item.file, std::ios::binary);
    if (!input) {
      tally.failures.push_back(item.relative);
      ++tally.failed;
      return;
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    const LgtFile lgt = read_lgt_file(buffer.str());
    if (!lgt.error.empty()) {
      tally.failures.push_back(item.relative);
      ++tally.failed;
      return;
    }
    const LgtRun run = execute_lgt(lgt, utf8_from_path(item.file), folder_of(item.relative), {});
    tally.passed += run.passed;
    tally.failed += run.failed;
    for (const std::string& line : run.failures) tally.failures.push_back(item.relative + " " + line);
    const std::string folder = folder_of(item.relative);
    for (const std::string& name : run.outside) notes.push_back(SideNote{folder, name});
  } catch (...) {
    if (!is_heap_exhaustion()) throw;
    tally.failures.push_back(item.relative);
    ++tally.failed;
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
  Tally iso;
  Tally library;
  for (const Pending& item : pending) run_file(item, item.column == SuiteColumn::Library ? library : iso, notes);
  std::sort(notes.begin(), notes.end(), [](const SideNote& left, const SideNote& right) {
    if (left.path != right.path) return left.path < right.path;
    return left.detail < right.detail;
  });
  const std::filesystem::path report = path_from_utf8(report_path);
  const BadgeGate iso_gate =
      open_gate(std::filesystem::path{report}.replace_extension(".json"), "ISO conformance", "conformance_regressed");
  const BadgeGate library_gate =
      open_gate(library_badge_path(report), "library conformance", "library_conformance_regressed");
  // 書けなかった報告書は合格率がいくつでも失敗にする。黙って 0 で終えると、古い報告書とバッジが残る（Muse のレビューの中）。
  bool written = true;
  if (!write_report(report, iso, library, notes, bases)) {
    say_unwritable(report);
    written = false;
  }
  const bool iso_down = close_gate(iso_gate, iso, written);
  const bool library_down = close_gate(library_gate, library, written);
  // ライブラリ欄には 90% の下限を掛けない（フェーズ11の初めは 65% 前後）。掛けるのは後退の判定だけ。
  if (!written || iso_down || library_down) return 1;
  return suite_status(iso.passed, iso.denominator());
}

}  // namespace mosa
