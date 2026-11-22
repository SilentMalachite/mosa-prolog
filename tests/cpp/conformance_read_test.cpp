// conformance_io_test.cpp の続き（800 行の上限で分けた）。準拠スイートの入出力の場合を見る: nl/1 の誤り、空の
// バイナリを開くこと、read_term/3 の変数と残りと構文の誤り、読めない数、listing の誤り。場合は無名の名前空間に置き、
// 呼ばれない場合は -Werror の unused-function で落ちる。main は conformance_io_test.cpp にあり、
// conformance_read_checks をそこでの順のまま呼ぶ。一時木は tests/tmp だけ。

#include "conformance_io_support.hpp"
#include "harness.hpp"
#include "mosa/suite.hpp"

#include <filesystem>
#include <string>

namespace {

using conformance_io_support::between_headings;
using conformance_io_support::has_exact_line;
using conformance_io_support::read_text;
using conformance_io_support::show;
using conformance_io_support::write_text;

// ^^ のあと nl は別クエリ。無名変数は犯人を包摂し、束縛した Stream は同じ項になる。
std::string nl_error_text() {
  return "test(lgt_nl_1_04, error(permission_error(output, binary_stream, _))) :-\n"
         "    ^^set_binary_output(bin, []),\n"
         "    {nl(bin)}.\n"
         "test(lgt_nl_1_05, error(existence_error(stream, Stream))) :-\n"
         "    ^^closed_output_stream(Stream, []),\n"
         "    {nl(Stream)}.\n"
         "test(lgt_nl_1_06, error(existence_error(stream, Stream))) :-\n"
         "    ^^closed_input_stream(Stream, []),\n"
         "    {nl(Stream)}.\n"
         "test(lgt_nl_0_02, error(permission_error(output, binary_stream, Stream))) :-\n"
         "    ^^set_binary_output([]),\n"
         "    current_output(Stream),\n"
         "    {nl}.\n";
}

void check_nl_error(const std::filesystem::path& root) {
  const auto suite = root / "nl-error";
  const auto prolog = suite / "logtalk" / "prolog";
  const auto report = root / "nl-error-report.md";
  mosa_test::check(write_text(prolog / "predicates" / "nl_1" / "tests.lgt", nl_error_text()), "write nl error");
  const int code = mosa::run_iso_suite(suite.string(), report.string());
  const std::string body = read_text(report);
  const std::string failed = between_headings(body, "不合格:", "別欄:");
  if (code != 0 || failed.find("lgt_nl_") != std::string::npos) show("nl error report", body);
  mosa_test::check(code == 0, "nl error exits 0");
  mosa_test::check(has_exact_line(body, "分母: 4"), "nl error denominator");
  mosa_test::check(has_exact_line(body, "合格数: 4"), "nl error passed");
  mosa_test::check(failed.find("lgt_nl_1_04") == std::string::npos, "nl binary alias is subsumed");
  mosa_test::check(failed.find("lgt_nl_1_05") == std::string::npos, "nl closed output uses Stream");
  mosa_test::check(failed.find("lgt_nl_1_06") == std::string::npos, "nl closed input uses Stream");
  mosa_test::check(failed.find("lgt_nl_0_02") == std::string::npos, "nl current output uses Stream");
}

// 空のバイナリを読みで開くと、まだ読んでいなくても終端。
std::string open_empty_text() {
  return "test(iso_open_4_01, true) :-\n"
         "    ^^file_path('roger_data', Path),\n"
         "    ^^create_binary_file(Path, []),\n"
         "    {open(Path, read, D, [type(binary)]),\n"
         "     at_end_of_stream(D)}.\n";
}

void check_open_empty(const std::filesystem::path& root) {
  const auto suite = root / "open-empty";
  const auto prolog = suite / "logtalk" / "prolog";
  const auto report = root / "open-empty-report.md";
  mosa_test::check(write_text(prolog / "predicates" / "open_4" / "tests.lgt", open_empty_text()),
                   "write open empty");
  const int code = mosa::run_iso_suite(suite.string(), report.string());
  const std::string body = read_text(report);
  const std::string failed = between_headings(body, "不合格:", "別欄:");
  if (code != 0 || failed.find("iso_open_4_01") != std::string::npos) show("open empty report", body);
  mosa_test::check(code == 0, "open empty exits 0");
  mosa_test::check(has_exact_line(body, "分母: 1"), "open empty denominator");
  mosa_test::check(has_exact_line(body, "合格数: 1"), "open empty passed");
  mosa_test::check(failed.find("iso_open_4_01") == std::string::npos, "empty binary is at end");
}

// 8.14.1.4。束縛をまたぐ変数は、次の == でも同じセルである。
std::string read_vars_text() {
  return "test(iso_read_term_3_03, true) :-\n"
         "    ^^set_text_input(in, ['foo(A+Roger,A+_). ','term2. ...']),\n"
         "    {read_term(in, T, [variables(VL),variable_names(VN),singletons(VS)])},\n"
         "    ^^text_input_assertion(in, ' term2. ...', Assertion),\n"
         "    assertion(variant(T, foo(X1+X2,X1+X3))),\n"
         "    T = foo(X1+X2,X1+X3),\n"
         "    assertion(VL == [X1,X2,X3]),\n"
         "    assertion(VN == ['A'=X1,'Roger'=X2]),\n"
         "    assertion(VS == ['Roger'=X2]),\n"
         "    assertion(Assertion).\n";
}

void check_read_vars(const std::filesystem::path& root) {
  const auto suite = root / "read-vars";
  const auto prolog = suite / "logtalk" / "prolog";
  const auto report = root / "read-vars-report.md";
  mosa_test::check(write_text(prolog / "predicates" / "read_term_3" / "tests.lgt", read_vars_text()),
                   "write read vars");
  const int code = mosa::run_iso_suite(suite.string(), report.string());
  const std::string body = read_text(report);
  const std::string failed = between_headings(body, "不合格:", "別欄:");
  if (code != 0 || failed.find("iso_read_term_3_03") != std::string::npos) show("read vars report", body);
  mosa_test::check(code == 0, "read vars exits 0");
  mosa_test::check(has_exact_line(body, "分母: 1"), "read vars denominator");
  mosa_test::check(has_exact_line(body, "合格数: 1"), "read vars passed");
  mosa_test::check(failed.find("iso_read_term_3_03") == std::string::npos, "read term keeps variable names");
}

// 8.14.1.4 は term1 のあと、空白から term2 を残す。
std::string read_rest_text() {
  return "test(iso_read_term_3_01, true(Assertion)) :-\n"
         "    ^^set_text_input('term1. term2. ...'),\n"
         "    {read(T)},\n"
         "    T == term1,\n"
         "    ^^text_input_assertion(' term2. ...', Assertion).\n";
}

void check_read_rest(const std::filesystem::path& root) {
  const auto suite = root / "read-rest";
  const auto prolog = suite / "logtalk" / "prolog";
  const auto report = root / "read-rest-report.md";
  mosa_test::check(write_text(prolog / "predicates" / "read_term_3" / "tests.lgt", read_rest_text()),
                   "write read rest");
  const int code = mosa::run_iso_suite(suite.string(), report.string());
  const std::string body = read_text(report);
  const std::string failed = between_headings(body, "不合格:", "別欄:");
  if (code != 0 || failed.find("iso_read_term_3_01") != std::string::npos) show("read rest report", body);
  mosa_test::check(code == 0, "read rest exits 0");
  mosa_test::check(has_exact_line(body, "分母: 1"), "read rest denominator");
  mosa_test::check(has_exact_line(body, "合格数: 1"), "read rest passed");
  mosa_test::check(failed.find("iso_read_term_3_01") == std::string::npos, "read leaves the space");
}

// 8.14.1.4。構文エラーの項は終止符まで進み、確認の空白は次の項に残す。
std::string read_syntax_text() {
  return "test(iso_read_term_3_05, true(Assertion)) :-\n"
         "    ^^set_text_input('foo 123. term2. ...'),\n"
         "    catch({read(_T)}, error(syntax_error(_), _), true),\n"
         "    ^^text_input_assertion(' term2. ...', Assertion).\n";
}

void check_read_syntax(const std::filesystem::path& root) {
  const auto suite = root / "read-syntax";
  const auto prolog = suite / "logtalk" / "prolog";
  const auto report = root / "read-syntax-report.md";
  mosa_test::check(write_text(prolog / "predicates" / "read_term_3" / "tests.lgt", read_syntax_text()),
                   "write read syntax");
  const int code = mosa::run_iso_suite(suite.string(), report.string());
  const std::string body = read_text(report);
  const std::string failed = between_headings(body, "不合格:", "別欄:");
  if (code != 0 || failed.find("iso_read_term_3_05") != std::string::npos) show("read syntax report", body);
  mosa_test::check(code == 0, "read syntax exits 0");
  mosa_test::check(has_exact_line(body, "分母: 1"), "read syntax denominator");
  mosa_test::check(has_exact_line(body, "合格数: 1"), "read syntax passed");
  mosa_test::check(failed.find("iso_read_term_3_05") == std::string::npos, "read syntax leaves the space");
}

// 数の字句が失敗しても、その項の終止符まで進める。確認の空白は残す。
std::string read_bad_number_text() {
  return "test(read_bad_number, true(Assertion)) :-\n"
         "    ^^set_text_input('foo 1_. term2. ...'),\n"
         "    catch({read(_T)}, error(syntax_error(_), _), true),\n"
         "    ^^text_input_assertion(' term2. ...', Assertion).\n"
         "test(read_overflow_number, true(Assertion)) :-\n"
         "    ^^set_text_input('foo 999999999999999999999. term2. ...'),\n"
         "    catch({read(_T)}, error(syntax_error(_), _), true),\n"
         "    ^^text_input_assertion(' term2. ...', Assertion).\n"
         "test(read_bad_number_open, true) :-\n"
         "    ^^set_text_input('foo 1_'),\n"
         "    catch({read(_T)}, error(syntax_error(_), _), true).\n";
}

void check_read_bad_number(const std::filesystem::path& root) {
  const auto suite = root / "read-bad-number";
  const auto prolog = suite / "logtalk" / "prolog";
  const auto report = root / "read-bad-number-report.md";
  mosa_test::check(write_text(prolog / "predicates" / "read_term_3" / "tests.lgt", read_bad_number_text()),
                   "write read bad number");
  const int code = mosa::run_iso_suite(suite.string(), report.string());
  const std::string body = read_text(report);
  const std::string failed = between_headings(body, "不合格:", "別欄:");
  if (code != 0 || failed.find("read_bad_number") != std::string::npos) show("read bad number report", body);
  mosa_test::check(code == 0, "read bad number exits 0");
  mosa_test::check(has_exact_line(body, "分母: 3"), "read bad number denominator");
  mosa_test::check(has_exact_line(body, "合格数: 3"), "read bad number passed");
  mosa_test::check(failed.find("read_bad_number") == std::string::npos, "bad number leaves the space");
  mosa_test::check(failed.find("read_overflow_number") == std::string::npos, "overflow leaves the space");
}

// ^^ があると本体は混合実行になる。列を一つの Formal として見ると、同じ字面でも不合格になる。
std::string listed_error_text() {
  return "test(listed_formal, errors([domain_error(stream_or_alias, foo), existence_error(stream, foo)])) :-\n"
         "    ^^stream_position(Position),\n"
         "    set_stream_position(foo, Position).\n";
}

void check_listed_error(const std::filesystem::path& root) {
  const auto suite = root / "listed-error";
  const auto prolog = suite / "logtalk" / "prolog";
  const auto report = root / "listed-error-report.md";
  mosa_test::check(write_text(prolog / "predicates" / "set_stream_position_2" / "tests.lgt", listed_error_text()),
                   "write listed error");
  const int code = mosa::run_iso_suite(suite.string(), report.string());
  const std::string body = read_text(report);
  const std::string failed = between_headings(body, "不合格:", "別欄:");
  if (code != 0 || failed.find("listed_formal") != std::string::npos) show("listed error report", body);
  mosa_test::check(code == 0, "listed error exits 0");
  mosa_test::check(has_exact_line(body, "分母: 1"), "listed error denominator");
  mosa_test::check(has_exact_line(body, "合格数: 1"), "listed error passed");
  mosa_test::check(failed.find("listed_formal") == std::string::npos, "listed formal passes");
}

}  // namespace

void conformance_read_checks(const std::filesystem::path& root) {
  check_nl_error(root);
  check_open_empty(root);
  check_read_rest(root);
  check_read_vars(root);
  check_read_syntax(root);
  check_read_bad_number(root);
  check_listed_error(root);
}
