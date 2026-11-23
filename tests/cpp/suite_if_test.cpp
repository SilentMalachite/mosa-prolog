// ランナーの (->)/2。variant が条件ならランナーが判定する。
// ランナー目標の無い (fail -> fail ; true) は機械の if-then のまま。
// 一時ファイルは tests/tmp/suite-if。

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

// 直接の setof は [[A,_]-A, [_]-1] を先に返す。-> を機械へ渡すと variant/2 が無く、この件は落ちる。
std::string if_text() {
  return "test(setof_if_order, true) :-\n"
         "    findall(S-Y, {setof(X,(X=Y;X=_Z;Y=1),S)}, LL),\n"
         "    (variant(LL, [[A,_]-A, [_]-1]) -> true ; variant(LL, [[_,A]-A, [_]-1])).\n"
         "test(setof_if_second, true) :-\n"
         "    findall(S-Y, {setof(X,(X=Y;X=_Z;Y=1),S)}, LL),\n"
         "    (variant(LL, [[_,A]-A, [_]-1]) -> true ; variant(LL, [[A,_]-A, [_]-1])).\n"
         "test(plain_if, true) :-\n"
         "    (fail -> fail ; true).\n"
         "test(if_skips_else, true) :-\n"
         "    (variant(a, a) -> true ; fail).\n"
         "test(if_cuts_else, fail) :-\n"
         "    (variant([1],[1]) -> fail ; true).\n";
}

void check_if(const std::filesystem::path& root) {
  const auto suite = root / "if";
  const auto prolog = suite / "logtalk" / "prolog";
  const auto report = root / "if-report.md";
  mosa_test::check(write_text(prolog / "predicates" / "setof_3" / "tests.lgt", if_text()), "write if");
  const int code = mosa::run_iso_suite(suite.string(), report.string());
  const std::string body = read_text(report);
  const std::string failed = between_headings(body, "不合格:", "別欄:");
  if (code != 0 || failed.find("setof_if_order") != std::string::npos ||
      failed.find("setof_if_second") != std::string::npos || failed.find("plain_if") != std::string::npos ||
      failed.find("if_skips_else") != std::string::npos || failed.find("if_cuts_else") != std::string::npos) {
    show("if report", body);
  }
  mosa_test::check(code == 0, "if exits 0");
  mosa_test::check(has_exact_line(body, "分母: 5"), "if denominator");
  mosa_test::check(has_exact_line(body, "合格数: 5"), "if passed");
  mosa_test::check(failed.find("setof_if_order") == std::string::npos, "setof if takes the first variant");
  mosa_test::check(failed.find("setof_if_second") == std::string::npos, "setof if falls through to the second");
  mosa_test::check(failed.find("plain_if") == std::string::npos, "plain if stays on the machine");
  mosa_test::check(failed.find("if_skips_else") == std::string::npos, "successful if does not run else");
  mosa_test::check(failed.find("if_cuts_else") == std::string::npos, "successful if cuts else");
}

// else にだけランナー目標がある if-then-else（レビュー R-H01）。条件が成功したら then が失敗しても else を動かさない。
// 表明の中の (C -> T ; E) も同じ。前は (C -> T) を 1 つのゴールとして試し、失敗すると E を判定した。
std::string else_runner_text() {
  return "test(ite_true_fp, true) :- (true -> fail ; assertion(true)).\n"
         "test(ite_false_fn, false) :- (true -> fail ; assertion(true)).\n"
         "test(ite_check_fp, true((true -> fail ; variant(X, 1)))) :- X = 1.\n"
         "test(ite_check_then, true((X == 1 -> variant(X, 1) ; fail))) :- X = 1.\n"
         "test(ite_else_taken, true) :- (fail -> true ; assertion(true)).\n"
         "test(ite_then_taken, true) :- (true -> true ; assertion(fail)).\n"
         "test(ite_check_else, true((fail -> fail ; variant(X, 1)))) :- X = 1.\n";
}

void check_else_runner(const std::filesystem::path& root) {
  const auto suite = root / "else";
  const auto prolog = suite / "logtalk" / "prolog";
  const auto report = root / "else-report.md";
  mosa_test::check(write_text(prolog / "predicates" / "atom_1" / "tests.lgt", else_runner_text()), "write else");
  static_cast<void>(mosa::run_iso_suite(suite.string(), report.string()));
  const std::string body = read_text(report);
  const std::string failed = between_headings(body, "不合格:", "別欄:");
  const bool fp = failed.find("ite_true_fp") != std::string::npos;
  const bool check_fp = failed.find("ite_check_fp") != std::string::npos;
  if (!fp || !check_fp || !has_exact_line(body, "合格数: 5")) show("else report", body);
  mosa_test::check(has_exact_line(body, "分母: 7"), "else denominator");
  mosa_test::check(has_exact_line(body, "合格数: 5"), "else passed");
  mosa_test::check(fp, "then that fails does not fall to an else assertion");
  mosa_test::check(failed.find("ite_false_fn") == std::string::npos, "false test passes when then fails");
  mosa_test::check(check_fp, "check (C -> T ; E) does not judge E after C");
  mosa_test::check(failed.find("ite_check_then") == std::string::npos, "check judges T after C");
  mosa_test::check(failed.find("ite_else_taken") == std::string::npos, "failed condition runs else");
  mosa_test::check(failed.find("ite_then_taken") == std::string::npos, "successful then skips else");
  mosa_test::check(failed.find("ite_check_else") == std::string::npos, "check judges E after a failed C");
}

}  // namespace

int main() {
  const std::filesystem::path root = std::filesystem::path{MOSA_SOURCE_DIR} / "tests" / "tmp" / "suite-if";
  {
    Fixture fixture(root);
    check_if(root);
  }
  {
    Fixture fixture(root);
    check_else_runner(root);
  }
  return mosa_test::failures();
}
