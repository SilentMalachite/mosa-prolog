// true(Goal) は、テスト機械にある利用者の節を見てから実行する。
// 組込だけを足した機械では seen/1 が見えない。無い述語は失敗のまま。
// {} の中で付いた束縛は、外の ^^assertion と ^^variant に届く（check_outside_bindings）。
// 一時ファイルは tests/tmp/suite-assert。

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

// 本体が true だけだと表明は同じ機械の query になる。
// variant があると表明は scratch に落ち、補助節が今は見えない。
std::string assert_text() {
  return "seen(1).\n"
         "test(aux_seen, true(seen(1))) :-\n"
         "    variant(a, a).\n"
         "test(aux_missing, true(missing(1))) :-\n"
         "    variant(a, a).\n"
         "test(aux_atom, true(atom(a))) :-\n"
         "    variant(a, a).\n";
}

void check_assert(const std::filesystem::path& root) {
  const auto suite = root / "assert";
  const auto prolog = suite / "logtalk" / "prolog";
  const auto report = root / "assert-report.md";
  mosa_test::check(write_text(prolog / "predicates" / "atom_1" / "tests.lgt", assert_text()), "write assert");
  const int code = mosa::run_iso_suite(suite.string(), report.string());
  const std::string body = read_text(report);
  const std::string failed = between_headings(body, "不合格:", "別欄:");
  if (code != 1 || failed.find("aux_seen") != std::string::npos || failed.find("aux_atom") != std::string::npos ||
      failed.find("aux_missing") == std::string::npos) {
    show("assert report", body);
  }
  // 無い述語が残るので合格率は 0.9 未満。終了コードは 1。
  mosa_test::check(code == 1, "assert exits 1");
  mosa_test::check(has_exact_line(body, "分母: 3"), "assert denominator");
  mosa_test::check(has_exact_line(body, "合格数: 2"), "assert passed");
  mosa_test::check(failed.find("aux_seen") == std::string::npos, "auxiliary clause is visible");
  mosa_test::check(failed.find("aux_missing") != std::string::npos, "missing predicate still fails");
  mosa_test::check(failed.find("aux_atom") == std::string::npos, "builtin atom/1 still passes");
}

// スイートの 3 件をそのまま写す（numbervars_3、length_2、format_2）。{} の中のゴールが付けた束縛は、
// lgtunit と同じく外の ^^assertion、^^variant、出力の表明へ届く。
std::string outside_text() {
  return ":- object(tests, extends(lgtunit)).\n"
         "\ttest(commons_numbervars_3_08, true) :-\n"
         "\t\tT = a(_X,_Y,_Z),\n"
         "\t\t{numbervars(T, 0, N)},\n"
         "\t\t^^assertion(ground(T)),\n"
         "\t\t^^assertion(N == 3).\n"
         "\ttest(commons_length_2_06, deterministic) :-\n"
         "\t\t{length(List, 3)},\n"
         "\t\t^^variant(List, [_, _, _]).\n"
         "\ttest(lgt_format_2_write_numbervars, true(Assertion)) :-\n"
         "\t\t^^set_text_output(''),\n"
         "\t\tTerm = a(_),\n"
         "\t\t{numbervars(Term, 0, _)},\n"
         "\t\t{format(\"~w\", [Term])},\n"
         "\t\t^^text_output_assertion('a(A)', Assertion).\n"
         ":- end_object.\n";
}

void check_outside_bindings(const std::filesystem::path& root) {
  const auto suite = root / "outside";
  const auto prolog = suite / "logtalk" / "prolog";
  const auto report = root / "outside-report.md";
  mosa_test::check(write_text(prolog / "predicates" / "atom_1" / "tests.lgt", outside_text()), "write outside");
  const int code = mosa::run_iso_suite(suite.string(), report.string());
  const std::string body = read_text(report);
  if (code != 0 || !has_exact_line(body, "合格数: 3")) show("outside report", body);
  mosa_test::check(has_exact_line(body, "分母: 3"), "outside denominator");
  mosa_test::check(has_exact_line(body, "合格数: 3"), "bindings inside {} reach the goals outside");
  mosa_test::check(code == 0, "outside exits 0");
}

}  // namespace

int main() {
  const std::filesystem::path root = std::filesystem::path{MOSA_SOURCE_DIR} / "tests" / "tmp" / "suite-assert";
  {
    Fixture fixture(root);
    check_assert(root);
    check_outside_bindings(root);
  }
  mosa_test::check(!std::filesystem::exists(root), "fixture removed");
  return mosa_test::failures();
}
