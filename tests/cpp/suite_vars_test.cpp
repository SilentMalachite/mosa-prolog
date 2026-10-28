// term_variables のあとの == は、項の中の変数と同じセルを見る。
// 束縛を一つずつ字面で読み戻すと、Vars の B と項の B が別になる。
// 一時ファイルは tests/tmp/suite-vars。

#include "harness.hpp"
#include "mosa/suite.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>

namespace {

bool write_text(const std::filesystem::path& path, std::string_view text) {
  std::error_code error;
  std::filesystem::create_directories(path.parent_path(), error);
  if (error) return false;
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output.write(text.data(), static_cast<std::streamsize>(text.size()));
  return static_cast<bool>(output);
}

std::string read_text(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

std::string_view line_at(std::string_view text, std::size_t start) {
  const auto end = text.find('\n', start);
  auto line = text.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);
  if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
  return line;
}

bool has_exact_line(std::string_view text, std::string_view expected) {
  std::size_t start = 0;
  while (start <= text.size()) {
    if (line_at(text, start) == expected) return true;
    const auto end = text.find('\n', start);
    if (end == std::string_view::npos) break;
    start = end + 1;
  }
  return false;
}

std::string after_heading(std::string_view text, std::string_view heading) {
  const auto begin = text.find(heading);
  if (begin == std::string_view::npos) return {};
  auto start = begin + heading.size();
  if (start < text.size() && text[start] == '\n') ++start;
  return std::string{text.substr(start)};
}

std::string between_headings(std::string_view text, std::string_view from, std::string_view to) {
  const std::string chunk = after_heading(text, from);
  const auto stop = chunk.find(to);
  if (stop == std::string::npos) return chunk;
  return chunk.substr(0, stop);
}

struct Fixture {
  std::filesystem::path path;
  explicit Fixture(std::filesystem::path root) : path(std::move(root)) {
    std::error_code error;
    std::filesystem::remove_all(path, error);
  }
  Fixture(const Fixture&) = delete;
  Fixture& operator=(const Fixture&) = delete;
  ~Fixture() {
    std::error_code error;
    std::filesystem::remove_all(path, error);
  }
};

void show(std::string_view label, std::string_view text) { std::cerr << label << '\n' << text << '\n'; }

// 06 と同じ三つ。一つずつに分けると、落ちた表明が報告に残る。
std::string vars_text() {
  return "test(iso_term_variables_2_06, true) :-\n"
         "    {S=B+T, T=A*B, term_variables(S, Vars)},\n"
         "    ^^assertion(Vars == [B, A]),\n"
         "    ^^assertion(T == A*B),\n"
         "    ^^assertion(S == B+A*B).\n"
         "test(vars_list, true) :-\n"
         "    {S=B+T, T=A*B, term_variables(S, Vars)},\n"
         "    ^^assertion(Vars == [B, A]).\n"
         "test(vars_prod, true) :-\n"
         "    {S=B+T, T=A*B, term_variables(S, Vars)},\n"
         "    ^^assertion(T == A*B).\n"
         "test(vars_sum, true) :-\n"
         "    {S=B+T, T=A*B, term_variables(S, Vars)},\n"
         "    ^^assertion(S == B+A*B).\n"
         "test(iso_term_variables_2_07, true) :-\n"
         "    {T=A*B, S=B+T, term_variables(S, Vars)},\n"
         "    ^^assertion(Vars == [B, A]),\n"
         "    ^^assertion(T == A*B),\n"
         "    ^^assertion(S == B+A*B).\n";
}

void check_vars(const std::filesystem::path& root) {
  const auto suite = root / "vars";
  const auto prolog = suite / "logtalk" / "prolog";
  const auto report = root / "vars-report.md";
  mosa_test::check(write_text(prolog / "predicates" / "term_variables_2" / "tests.lgt", vars_text()), "write vars");
  const int code = mosa::run_iso_suite(suite.string(), report.string());
  const std::string body = read_text(report);
  const std::string failed = between_headings(body, "不合格:", "別欄:");
  if (code != 0 || failed.find("iso_term_variables_2_06") != std::string::npos ||
      failed.find("vars_list") != std::string::npos || failed.find("vars_prod") != std::string::npos ||
      failed.find("vars_sum") != std::string::npos || failed.find("iso_term_variables_2_07") != std::string::npos) {
    show("vars report", body);
  }
  mosa_test::check(code == 0, "vars exits 0");
  mosa_test::check(has_exact_line(body, "分母: 5"), "vars denominator");
  mosa_test::check(has_exact_line(body, "合格数: 5"), "vars passed");
  mosa_test::check(failed.find("iso_term_variables_2_06") == std::string::npos, "shared variables pass together");
  mosa_test::check(failed.find("vars_list") == std::string::npos, "Vars stays [B, A]");
  mosa_test::check(failed.find("vars_prod") == std::string::npos, "T stays A*B");
  mosa_test::check(failed.find("vars_sum") == std::string::npos, "S stays B+A*B");
  mosa_test::check(failed.find("iso_term_variables_2_07") == std::string::npos, "reverse order stays shared");
}

// 書き手は書くたびに _G1 から番号を振る（iso-notes 7.10.5 a の決定 B）。実行器が束縛と表明を別々に書いて
// 貼り合わせると、別の変数が同じ名前で一つになる。L の変数と表明の中の新しい変数は別でなければならない。
std::string fresh_text() {
  return "test(fresh_apart, true(L \\== f(_, _))) :-\n"
         "    {L = f(_, _)}.\n"
         "test(fresh_shared_kept, true(X == Y)) :-\n"
         "    {L = f(A, A)}, L = f(X, Y).\n";
}

void check_fresh(const std::filesystem::path& root) {
  const auto suite = root / "fresh";
  const auto prolog = suite / "logtalk" / "prolog";
  const auto report = root / "fresh-report.md";
  mosa_test::check(write_text(prolog / "predicates" / "term_variables_2" / "tests.lgt", fresh_text()), "write fresh");
  const int code = mosa::run_iso_suite(suite.string(), report.string());
  const std::string body = read_text(report);
  if (!has_exact_line(body, "合格数: 2")) show("fresh report", body);
  mosa_test::check(code == 0, "fresh exits 0");
  mosa_test::check(has_exact_line(body, "合格数: 2"), "fresh variables stay apart");
}

}  // namespace

int main() {
  const std::filesystem::path root = std::filesystem::path{MOSA_SOURCE_DIR} / "tests" / "tmp" / "suite-vars";
  {
    Fixture fixture(root);
    check_vars(root);
    check_fresh(root);
  }
  mosa_test::check(!std::filesystem::exists(root), "fixture removed");
  return mosa_test::failures();
}
