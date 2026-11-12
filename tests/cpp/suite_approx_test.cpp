// 判定器の =~=。式は固定コミット 4decb64 の lgtunit.lgt の (=~=)/2 と同じ。
// Big を通す判定（=~= の正しい丸めと範囲の外、エラーの包摂の -(Big) の符号の畳み）も見る（フェーズ10 タスク 9）。
// 一時ファイルは tests/tmp/suite-approx。

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

// approx_far と approx_length は不合格に数えられるのが正しい。
std::string approx_text() {
  return ":- uses(lgtunit, [op(700, xfx, =~=), (=~=)/2]).\n"
         "test(approx_pi, true(X =~= 3.1415927)) :-\n"
         "    {X is pi}.\n"
         "test(approx_zero, true(X =~= 0.0)) :-\n"
         "    {X is cos(pi / 2)}.\n"
         "test(approx_list, true([1.0, 2.0] =~= [1.0000001, 2.0])) :-\n"
         "    true.\n"
         "test(approx_far, true(1.0 =~= 1.1)) :-\n"
         "    true.\n"
         "test(approx_length, true([1.0] =~= [1.0, 2.0])) :-\n"
         "    true.\n";
}

void check_approx(const std::filesystem::path& root) {
  const auto suite = root / "approx";
  const auto prolog = suite / "logtalk" / "prolog";
  const auto report = root / "approx-report.md";
  mosa_test::check(write_text(prolog / "functions" / "pi_0" / "tests.lgt", approx_text()), "write approx");
  const int code = mosa::run_iso_suite(suite.string(), report.string());
  const std::string body = read_text(report);
  const std::string failed = between_headings(body, "不合格:", "別欄:");
  if (!has_exact_line(body, "合格数: 3")) show("approx report", body);
  mosa_test::check(code != 0, "approx reports failures");
  mosa_test::check(has_exact_line(body, "分母: 5"), "approx denominator");
  mosa_test::check(has_exact_line(body, "合格数: 3"), "approx passed");
  mosa_test::check(failed.find("approx_pi ") == std::string::npos, "approx_pi");
  mosa_test::check(failed.find("approx_zero ") == std::string::npos, "approx_zero");
  mosa_test::check(failed.find("approx_list ") == std::string::npos, "approx_list");
  mosa_test::check(failed.find("approx_far ") != std::string::npos, "approx_far fails");
  mosa_test::check(failed.find("approx_length ") != std::string::npos, "approx_length fails");
}

// big_over は double の範囲の外なので数として扱わず、big_other は別の Big なので、どちらも不合格が正しい。
// -(2^63) は正規形の IntBox の -2^63 に合う。int64 で符号を反転すると合わなかった。
// 優先順位が Big の op と、int に入らない 2^32+700 の op は定義しない（切り詰めると 700 になる）。small_op は対照。
// Big を含むバイトの列は set_binary_input を失敗させる。どちらも integer_value に Big を渡すと止まる道である。
std::string big_text() {
  return ":- uses(lgtunit, [op(700, xfx, =~=), (=~=)/2, op(700, xfx, <=>),\n"
         "    op(1180591620717411303424, xfx, ===>), op(4294967996, xfx, ==>>)]).\n"
         "test(small_op, true) :- {current_op(700, xfx, '<=>')}.\n"
         "test(big_op, false) :- {current_op(_, _, '===>')}.\n"
         "test(wide_op, false) :- {current_op(_, _, '==>>')}.\n"
         "test(big_byte, false) :- ^^set_binary_input([1, 1267650600228229401496703205376]).\n"
         "test(big_approx, true(X =~= 1.2676506002282294e30)) :- {X is 2^100}.\n"
         "test(big_approx_neg, true(X =~= -(1267650600228229401496703205376))) :- {X is -(2.0**100)}.\n"
         "test(big_over, true(X =~= 1.0e308)) :- {X is 10^400}.\n"
         "test(big_variant, true(variant(X, f(1267650600228229401496703205376)))) :-\n"
         "    {X = f(1267650600228229401496703205376)}.\n"
         "test(big_formal, error(domain_error(not_less_than_zero, -1844674407370909797907654848955145546336677616))) :-\n"
         "    {arg(-1844674407370909797907654848955145546336677616, t(a), _)}.\n"
         "test(big_folded, error(domain_error(not_less_than_zero, -(1844674407370909797907654848955145546336677616)))) :-\n"
         "    {arg(-1844674407370909797907654848955145546336677616, t(a), _)}.\n"
         "test(min_folded, error(domain_error(not_less_than_zero, -(9223372036854775808)))) :-\n"
         "    {arg(-9223372036854775808, t(a), _)}.\n"
         "test(big_other, error(domain_error(not_less_than_zero, -1844674407370909797907654848955145546336677617))) :-\n"
         "    {arg(-1844674407370909797907654848955145546336677616, t(a), _)}.\n";
}

void check_big(const std::filesystem::path& root) {
  const auto suite = root / "big";
  const auto report = root / "big-report.md";
  mosa_test::check(write_text(suite / "logtalk" / "prolog" / "unbounded" / "tests.lgt", big_text()), "write big");
  const int code = mosa::run_iso_suite(suite.string(), report.string());
  const std::string body = read_text(report);
  const std::string failed = between_headings(body, "不合格:", "別欄:");
  if (!has_exact_line(body, "合格数: 10")) show("big report", body);
  mosa_test::check(code != 0, "big reports failures");
  mosa_test::check(has_exact_line(body, "分母: 12"), "big denominator");
  mosa_test::check(has_exact_line(body, "合格数: 10"), "big passed");
  for (const std::string_view name : {"small_op ", "big_op ", "wide_op ", "big_byte ", "big_approx ", "big_approx_neg ",
                                      "big_variant ", "big_formal ", "big_folded ", "min_folded "}) {
    mosa_test::check(failed.find(name) == std::string::npos, std::string{name});
  }
  mosa_test::check(failed.find("big_over ") != std::string::npos, "big_over fails");
  mosa_test::check(failed.find("big_other ") != std::string::npos, "big_other fails");
}

// 判定器が自分で評価する表明（=~= を含む）の < =< > >= は、整数どうしなら正確に比べる。double を通すと 2^53+1 > 2^53 が
// 偽になり、2^100 >= 2^100+1 が真になる（偽の合格）。big_ge_false は不合格が正しい。big_float_gt は浮動小数点数との対照。
std::string order_text() {
  return ":- uses(lgtunit, [op(700, xfx, =~=), (=~=)/2]).\n"
         "test(big_gt, true((X =~= 1.2676506002282294e30, X > 1267650600228229401496703205375))) :- {X is 2^100}.\n"
         "test(int_gt, true((X =~= 9.007199254740992e15, X > 9007199254740992))) :- {X is 2^53 + 1}.\n"
         "test(big_float_gt, true((X =~= 1.2676506002282294e30, X > 1.0e30))) :- {X is 2^100}.\n"
         "test(big_ge_false, true((X =~= 1.2676506002282294e30, X >= 1267650600228229401496703205377))) :-\n"
         "    {X is 2^100}.\n";
}

void check_order(const std::filesystem::path& root) {
  const auto suite = root / "order";
  const auto report = root / "order-report.md";
  mosa_test::check(write_text(suite / "logtalk" / "prolog" / "unbounded" / "tests.lgt", order_text()), "write order");
  const int code = mosa::run_iso_suite(suite.string(), report.string());
  const std::string body = read_text(report);
  const std::string failed = between_headings(body, "不合格:", "別欄:");
  if (!has_exact_line(body, "合格数: 3")) show("order report", body);
  mosa_test::check(code != 0, "order reports failures");
  mosa_test::check(has_exact_line(body, "分母: 4"), "order denominator");
  mosa_test::check(has_exact_line(body, "合格数: 3"), "order passed");
  for (const std::string_view name : {"big_gt ", "int_gt ", "big_float_gt "}) {
    mosa_test::check(failed.find(name) == std::string::npos, std::string{name});
  }
  mosa_test::check(failed.find("big_ge_false ") != std::string::npos, "big_ge_false fails");
}

}  // namespace

int main() {
  const std::filesystem::path root = std::filesystem::path{MOSA_SOURCE_DIR} / "tests" / "tmp" / "suite-approx";
  {
    Fixture fixture(root);
    check_approx(root);
    check_big(root);
    check_order(root);
  }
  return mosa_test::failures();
}
