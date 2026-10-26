// 準拠テストの一時出力と隣の Prolog。同梱の logtalk は読まない。一時木は tests/tmp だけ。

#include "harness.hpp"
#include "mosa/suite.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

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

bool path_exists(const std::filesystem::path& path) {
  std::error_code error;
  return std::filesystem::exists(path, error) && !error;
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

std::vector<std::string> regular_files(const std::filesystem::path& root) {
  std::vector<std::string> names;
  std::error_code error;
  if (!std::filesystem::is_directory(root, error) || error) return names;
  const auto options = std::filesystem::directory_options::skip_permission_denied;
  std::filesystem::recursive_directory_iterator cursor(root, options, error);
  if (error) return names;
  const std::filesystem::recursive_directory_iterator end;
  while (cursor != end) {
    std::error_code entry_error;
    if (cursor->is_regular_file(entry_error) && !entry_error) {
      const auto relative = std::filesystem::relative(cursor->path(), root, entry_error);
      if (!entry_error) names.push_back(relative.generic_string());
    }
    cursor.increment(error);
    if (error) break;
  }
  std::sort(names.begin(), names.end());
  return names;
}

bool directory_empty(const std::filesystem::path& path) {
  std::error_code error;
  if (!std::filesystem::exists(path, error) || error) return !error;
  return std::filesystem::is_empty(path, error) && !error;
}

void show(std::string_view label, std::string_view text) { std::cerr << label << '\n' << text << '\n'; }

std::string tester_text() {
  return ":- initialization((\n"
         "\tlogtalk_load_context(directory, Directory),\n"
         "\tatom_concat(Directory, 'main.pl', File),\n"
         "\t'$lgt_load_prolog_file'(File),\n"
         "\tlogtalk_load(missing)\n"
         ")).\n";
}

std::string loaded_text() { return "test(loaded, true(L == [1])) :- findall(X, {a(X)}, L).\n"; }

std::string nl_text() {
  return "test(nl_out, true(Assertion)) :-\n"
         "    ^^set_text_output(''),\n"
         "    {nl},\n"
         "    ^^text_output_assertion('\\n', Assertion).\n";
}

std::string missing_text() { return "test(missing, true) :- ^^not_in_list(a).\n"; }

std::string colon_tester() {
  return ":- initialization((\n"
         "\tlogtalk_load_context(directory, Directory),\n"
         "\tatom_concat(Directory, 'main.pl', File),\n"
         "\t'$lgt_load_prolog_file'(File),\n"
         "\tlogtalk_load(missing),\n"
         "\ttests::run\n"
         ")).\n";
}

std::string helper_text() {
  return "helper :- ^^clean_text_output.\n"
         "test(ok, true) :- {true}.\n";
}

std::string reopen_text() {
  return "test(reopen, true(Assertion)) :-\n"
         "    ^^set_text_output(st_o, ''),\n"
         "    {close(st_o)},\n"
         "    ^^set_text_output(st_o, ''),\n"
         "    {put_char(st_o, a)},\n"
         "    ^^text_output_assertion(st_o, 'a', Assertion).\n";
}

// 直接の is/2 は -125.0。スイートは表明を書き戻すので、前置の負数のままだと落ちる。
std::string power_text() {
  return ":- uses(lgtunit, [op(700, xfx, =~=), (=~=)/2]).\n"
         "test(iso_power_2_02, true(X =~= -125.0)) :-\n"
         "    {X is '**'(-5.0, 3)}.\n";
}

void check_pair(const std::filesystem::path& root) {
  const auto suite = root / "io";
  const auto prolog = suite / "logtalk" / "prolog";
  const auto report = root / "io-report.md";
  const auto include = prolog / "directives" / "include_1";
  const auto nl = prolog / "predicates" / "nl_1";
  mosa_test::check(write_text(include / "tester.lgt", tester_text()), "write tester");
  mosa_test::check(write_text(include / "main.pl", "a(1).\n"), "write main");
  mosa_test::check(write_text(include / "tests.lgt", loaded_text()), "write loaded");
  mosa_test::check(write_text(nl / "tests.lgt", nl_text()), "write nl");
  const auto conformance = std::filesystem::path{MOSA_SOURCE_DIR} / "tests" / "conformance";
  const auto before_fixture = regular_files(suite);
  const auto before_conformance = regular_files(conformance);
  const int code = mosa::run_iso_suite(suite.string(), report.string());
  const std::string body = read_text(report);
  const std::string failed = between_headings(body, "不合格:", "別欄:");
  if (code != 0 || !has_exact_line(body, "合格数: 2")) show("io report", body);
  // 隣の Prolog と nl の一時出力。2 件とも合格しないと分母は 2 にならない。
  mosa_test::check(code == 0, "io exits 0");
  mosa_test::check(has_exact_line(body, "分母: 2"), "io denominator");
  mosa_test::check(has_exact_line(body, "合格数: 2"), "io passed");
  mosa_test::check(failed.find_first_not_of(" \t\r\n") == std::string::npos, "io adds no failure");
  mosa_test::check(regular_files(suite) == before_fixture, "fixture gains no file");
  mosa_test::check(regular_files(conformance) == before_conformance, "conformance gains no file");
}

void check_colon_run(const std::filesystem::path& root) {
  const auto suite = root / "colon-run";
  const auto prolog = suite / "logtalk" / "prolog";
  const auto report = root / "colon-run-report.md";
  const auto include = prolog / "directives" / "include_1";
  mosa_test::check(write_text(include / "tester.lgt", colon_tester()), "write colon tester");
  mosa_test::check(write_text(include / "main.pl", "a(1).\n"), "write colon main");
  mosa_test::check(write_text(include / "tests.lgt", loaded_text()), "write colon tests");
  const int code = mosa::run_iso_suite(suite.string(), report.string());
  const std::string body = read_text(report);
  const std::string failed = between_headings(body, "不合格:", "別欄:");
  if (code != 0 || failed.find("a/1") != std::string::npos) show("colon report", body);
  // tests::run は実行しない。:: を読めて main.pl を consult できれば a/1 がある。
  mosa_test::check(code == 0, "colon exits 0");
  mosa_test::check(has_exact_line(body, "分母: 1"), "colon denominator");
  mosa_test::check(has_exact_line(body, "合格数: 1"), "colon passed");
  mosa_test::check(failed.find("a/1") == std::string::npos, "colon consulted main");
  mosa_test::check(failed.find("run") == std::string::npos, "colon did not run tests");
}

void check_canonical(const std::filesystem::path& root) {
  const auto suite = root / "canon";
  const auto prolog = suite / "logtalk" / "prolog";
  const auto report = root / "canon-report.md";
  mosa_test::check(write_text(prolog / "predicates" / "atom_1" / "tests.lgt", helper_text()), "write helper");
  const int code = mosa::run_iso_suite(suite.string(), report.string());
  const std::string body = read_text(report);
  const std::string failed = between_headings(body, "不合格:", "別欄:");
  if (failed.find("load") != std::string::npos || code != 0) show("canon report", body);
  // helper は呼ばない。節の ^^ が正準形なら load は落ちず、ok だけ合格する。
  mosa_test::check(code == 0, "canon exits 0");
  mosa_test::check(has_exact_line(body, "分母: 1"), "canon denominator");
  mosa_test::check(has_exact_line(body, "合格数: 1"), "canon passed");
  mosa_test::check(failed.find("load") == std::string::npos, "canon is not a load failure");
}

void check_unknown(const std::filesystem::path& root) {
  const auto suite = root / "unknown";
  const auto prolog = suite / "logtalk" / "prolog";
  const auto report = root / "unknown-report.md";
  mosa_test::check(write_text(prolog / "predicates" / "put_byte_2" / "tests.lgt", missing_text()), "write missing");
  const int code = mosa::run_iso_suite(suite.string(), report.string());
  const std::string body = read_text(report);
  const std::string failed = between_headings(body, "不合格:", "別欄:");
  if (failed.find("not_in_list/1") == std::string::npos) show("unknown report", body);
  // 未実装の ^^ は利用者述語にしない。存在エラーではなく、名前とアリティを書く。
  mosa_test::check(code == 1, "unknown exits 1");
  mosa_test::check(has_exact_line(body, "分母: 1"), "unknown denominator");
  mosa_test::check(has_exact_line(body, "合格数: 0"), "unknown passed");
  mosa_test::check(failed.find("not_in_list/1") != std::string::npos, "unknown names predicate");
  mosa_test::check(failed.find("existence_error") == std::string::npos, "unknown is not a user predicate");
}

void check_power(const std::filesystem::path& root) {
  const auto suite = root / "power";
  const auto prolog = suite / "logtalk" / "prolog";
  const auto report = root / "power-report.md";
  mosa_test::check(write_text(prolog / "functions" / "power_2" / "tests.lgt", power_text()), "write power");
  const int code = mosa::run_iso_suite(suite.string(), report.string());
  const std::string body = read_text(report);
  const std::string failed = between_headings(body, "不合格:", "別欄:");
  if (code != 0 || failed.find("iso_power_2_02") != std::string::npos) show("power report", body);
  // 負の底のべきは -125.0。表明の前置マイナスを数として見ないと不合格のまま残る。
  mosa_test::check(code == 0, "power exits 0");
  mosa_test::check(has_exact_line(body, "分母: 1"), "power denominator");
  mosa_test::check(has_exact_line(body, "合格数: 1"), "power passed");
  mosa_test::check(failed.find("iso_power_2_02") == std::string::npos, "power negative base");
}

// 束縛は ^^ の前にできる。表明を入出力に渡すと、値が合っていても assertion/1 で落ちる。
std::string sign_text() {
  return "test(sign_1_integer_01, true) :-\n"
         "    {Sign is sign(-17)},\n"
         "    ^^assertion(Sign == -1).\n"
         "test(sign_1_float_01, true) :-\n"
         "    {Sign is sign(-1.7)},\n"
         "    ^^assertion(float(Sign)),\n"
         "    ^^assertion(Sign == -1.0).\n"
         "test(sign_bad, true) :-\n"
         "    {Sign is sign(-17)},\n"
         "    ^^assertion(Sign == 0).\n"
         "test(sign_why, true) :-\n"
         "    {Sign is sign(-17)},\n"
         "    ^^assertion(kept, Sign == 0).\n";
}

void check_sign(const std::filesystem::path& root) {
  const auto suite = root / "sign";
  const auto prolog = suite / "logtalk" / "prolog";
  const auto report = root / "sign-report.md";
  mosa_test::check(write_text(prolog / "functions" / "sign_1" / "tests.lgt", sign_text()), "write sign");
  const int code = mosa::run_iso_suite(suite.string(), report.string());
  const std::string body = read_text(report);
  const std::string failed = between_headings(body, "不合格:", "別欄:");
  if (failed.find("assertion/1") != std::string::npos || failed.find("sign_1_integer_01") != std::string::npos) {
    show("sign report", body);
  }
  // 失敗した表明は false にしない。説明があるときは、その説明が不合格の中身になる。
  mosa_test::check(code == 1, "sign exits 1");
  mosa_test::check(has_exact_line(body, "分母: 4"), "sign denominator");
  mosa_test::check(has_exact_line(body, "合格数: 2"), "sign passed");
  mosa_test::check(failed.find("sign_1_integer_01") == std::string::npos, "sign integer bound");
  mosa_test::check(failed.find("sign_1_float_01") == std::string::npos, "sign float bound");
  mosa_test::check(failed.find("assertion/1") == std::string::npos, "sign is not unknown assertion");
  mosa_test::check(has_exact_line(failed, "functions/sign_1/tests.lgt sign_bad true assertion"),
                   "sign failure is not false");
  mosa_test::check(has_exact_line(failed, "functions/sign_1/tests.lgt sign_why true kept"), "sign explanation");
}

// 期待の -1 は前置の複合項。投げた整数と同じ字面でも、包摂が違う形のままなら不合格。
std::string abolish_text() {
  return "test(eddbali_abolish_1_10, error(domain_error(not_less_than_zero,-1))) :-\n"
         "    {abolish(foo/(-1))}.\n";
}

void check_abolish(const std::filesystem::path& root) {
  const auto suite = root / "abolish";
  const auto prolog = suite / "logtalk" / "prolog";
  const auto report = root / "abolish-report.md";
  mosa_test::check(write_text(prolog / "predicates" / "abolish_1" / "tests.lgt", abolish_text()), "write abolish");
  const int code = mosa::run_iso_suite(suite.string(), report.string());
  const std::string body = read_text(report);
  const std::string failed = between_headings(body, "不合格:", "別欄:");
  if (code != 0 || failed.find("eddbali_abolish_1_10") != std::string::npos) show("abolish report", body);
  mosa_test::check(code == 0, "abolish exits 0");
  mosa_test::check(has_exact_line(body, "分母: 1"), "abolish denominator");
  mosa_test::check(has_exact_line(body, "合格数: 1"), "abolish passed");
  mosa_test::check(failed.find("eddbali_abolish_1_10") == std::string::npos, "abolish negative arity");
}

// {1,2,3} は引数の ISO 項。呼び出しの {} まで外すと Arg は 1 になり、この 1 件は不合格。
std::string arg_text() {
  return "test(lgt_arg_3_20, true(Arg == (1,2,3))) :-\n"
         "    {arg(1, {1,2,3}, Arg)}.\n";
}

void check_arg(const std::filesystem::path& root) {
  const auto suite = root / "arg";
  const auto prolog = suite / "logtalk" / "prolog";
  const auto report = root / "arg-report.md";
  mosa_test::check(write_text(prolog / "predicates" / "arg_3" / "tests.lgt", arg_text()), "write arg");
  const int code = mosa::run_iso_suite(suite.string(), report.string());
  const std::string body = read_text(report);
  const std::string failed = between_headings(body, "不合格:", "別欄:");
  if (code != 0 || failed.find("lgt_arg_3_20") != std::string::npos) show("arg report", body);
  mosa_test::check(code == 0, "arg exits 0");
  mosa_test::check(has_exact_line(body, "分母: 1"), "arg denominator");
  mosa_test::check(has_exact_line(body, "合格数: 1"), "arg passed");
  mosa_test::check(failed.find("lgt_arg_3_20") == std::string::npos, "arg keeps curly term");
}

// [] はアトム。文字リストは [ と ]。裸の [[, ]] は読み戻すと別の項になる。
std::string atom_chars_text() {
  return "test(iso_atom_chars_2_02, true(L == ['[',']'])) :-\n"
         "    {atom_chars([], L)}.\n";
}

void check_atom_chars(const std::filesystem::path& root) {
  const auto suite = root / "atom-chars";
  const auto prolog = suite / "logtalk" / "prolog";
  const auto report = root / "atom-chars-report.md";
  mosa_test::check(write_text(prolog / "predicates" / "atom_chars_2" / "tests.lgt", atom_chars_text()),
                   "write atom chars");
  const int code = mosa::run_iso_suite(suite.string(), report.string());
  const std::string body = read_text(report);
  const std::string failed = between_headings(body, "不合格:", "別欄:");
  if (code != 0 || failed.find("iso_atom_chars_2_02") != std::string::npos) show("atom chars report", body);
  mosa_test::check(code == 0, "atom chars exits 0");
  mosa_test::check(has_exact_line(body, "分母: 1"), "atom chars denominator");
  mosa_test::check(has_exact_line(body, "合格数: 1"), "atom chars passed");
  mosa_test::check(failed.find("iso_atom_chars_2_02") == std::string::npos, "atom chars brackets round-trip");
}

// 失敗した query はヒープを捨てる。束縛を項ごとに読むと、X と Y の穴は別の変数になる。
std::string share_text() {
  return "share_pair(f(A), f(A)).\n"
         "test(share_cells, true) :-\n"
         "    {share_pair(X, Y)},\n"
         "    (fail ; variant(pair(X, Y), pair(f(A), f(A)))).\n";
}

void check_share(const std::filesystem::path& root) {
  const auto suite = root / "share";
  const auto prolog = suite / "logtalk" / "prolog";
  const auto report = root / "share-report.md";
  mosa_test::check(write_text(prolog / "predicates" / "bagof_3" / "tests.lgt", share_text()), "write share");
  const int code = mosa::run_iso_suite(suite.string(), report.string());
  const std::string body = read_text(report);
  const std::string failed = between_headings(body, "不合格:", "別欄:");
  if (code != 0 || failed.find("share_cells") != std::string::npos) show("share report", body);
  mosa_test::check(code == 0, "share exits 0");
  mosa_test::check(has_exact_line(body, "分母: 1"), "share denominator");
  mosa_test::check(has_exact_line(body, "合格数: 1"), "share passed");
  mosa_test::check(failed.find("share_cells") == std::string::npos, "share keeps one variable");
}

// 実行する機械の表に : は無い。中置の user:elk/1 は読めない。正準形ならこの 1 件は成功する。
std::string colon_user_text() {
  return "test(colon_user, true) :-\n"
         "    {X = ':'(user, elk/1)}.\n";
}

void check_colon_user(const std::filesystem::path& root) {
  const auto suite = root / "colon-user";
  const auto prolog = suite / "logtalk" / "prolog";
  const auto report = root / "colon-user-report.md";
  mosa_test::check(write_text(prolog / "predicates" / "clause_2" / "tests.lgt", colon_user_text()),
                   "write colon user");
  const int code = mosa::run_iso_suite(suite.string(), report.string());
  const std::string body = read_text(report);
  const std::string failed = between_headings(body, "不合格:", "別欄:");
  if (code != 0 || failed.find("colon_user") != std::string::npos) show("colon user report", body);
  mosa_test::check(code == 0, "colon user exits 0");
  mosa_test::check(has_exact_line(body, "分母: 1"), "colon user denominator");
  mosa_test::check(has_exact_line(body, "合格数: 1"), "colon user passed");
  mosa_test::check(failed.find("colon_user") == std::string::npos, "colon user reads canonical");
}

// 直接の bagof は X と Y を変数のまま残す。forall/2 は組込ではないので、ランナーが拾わないとこの 1 件は落ちる。
std::string bagof_free_text() {
  return "c(2, b, 'B').\n"
         "c(1, a, 'A').\n"
         "c(3, c, 'C').\n"
         "test(iso_bagof_3_17, true) :-\n"
         "    forall(\n"
         "        {bagof(X, Y^c(X, Y, _), _)},\n"
         "        (var(X), var(Y))\n"
         "    ).\n";
}

void check_bagof_free(const std::filesystem::path& root) {
  const auto suite = root / "bagof-free";
  const auto prolog = suite / "logtalk" / "prolog";
  const auto report = root / "bagof-free-report.md";
  mosa_test::check(write_text(prolog / "predicates" / "bagof_3" / "tests.lgt", bagof_free_text()),
                   "write bagof free");
  const int code = mosa::run_iso_suite(suite.string(), report.string());
  const std::string body = read_text(report);
  const std::string failed = between_headings(body, "不合格:", "別欄:");
  if (code != 0 || failed.find("iso_bagof_3_17") != std::string::npos) show("bagof free report", body);
  mosa_test::check(code == 0, "bagof free exits 0");
  mosa_test::check(has_exact_line(body, "分母: 1"), "bagof free denominator");
  mosa_test::check(has_exact_line(body, "合格数: 1"), "bagof free passed");
  mosa_test::check(failed.find("iso_bagof_3_17") == std::string::npos, "bagof free keeps template variables");
}

// uses(list, [msort/2]) は Logtalk の輸入。プログラムへ渡すと未知の指示でファイル全体が load になる。
std::string list_uses_text() {
  return ":- uses(list, [msort/2]).\n"
         "test(list_uses_ok, true) :- {true}.\n";
}

void check_list_uses(const std::filesystem::path& root) {
  const auto suite = root / "list-uses";
  const auto prolog = suite / "logtalk" / "prolog";
  const auto report = root / "list-uses-report.md";
  mosa_test::check(write_text(prolog / "predicates" / "bagof_3" / "tests.lgt", list_uses_text()), "write list uses");
  const int code = mosa::run_iso_suite(suite.string(), report.string());
  const std::string body = read_text(report);
  const std::string failed = between_headings(body, "不合格:", "別欄:");
  if (code != 0 || failed.find("load") != std::string::npos) show("list uses report", body);
  mosa_test::check(code == 0, "list uses exits 0");
  mosa_test::check(has_exact_line(body, "分母: 1"), "list uses denominator");
  mosa_test::check(has_exact_line(body, "合格数: 1"), "list uses passed");
  mosa_test::check(failed.find("load") == std::string::npos, "list uses is not a load failure");
}

void check_reopen(const std::filesystem::path& root) {
  const auto suite = root / "reopen";
  const auto prolog = suite / "logtalk" / "prolog";
  const auto report = root / "reopen-report.md";
  mosa_test::check(write_text(prolog / "predicates" / "put_char_2" / "tests.lgt", reopen_text()), "write reopen");
  const int code = mosa::run_iso_suite(suite.string(), report.string());
  const std::string body = read_text(report);
  const std::string failed = between_headings(body, "不合格:", "別欄:");
  if (code != 0 || failed.find("existence_error") != std::string::npos) show("reopen report", body);
  // 閉じた別名が残ると、次の put_char は新しい流れではなく存在エラーになる。
  mosa_test::check(code == 0, "reopen exits 0");
  mosa_test::check(has_exact_line(body, "分母: 1"), "reopen denominator");
  mosa_test::check(has_exact_line(body, "合格数: 1"), "reopen passed");
  mosa_test::check(failed.find("existence_error") == std::string::npos, "reopen is not a closed alias");
}

// min_integer は -2^63。字面へ戻すと正のトークンが溢れて、表明の Value は変数のままになる。
std::string flag_value_text() {
  return "test(lgt_current_prolog_flag_2_20, true) :-\n"
         "    forall(\n"
         "        {current_prolog_flag(Flag, Value)},\n"
         "        assertion(flag(Flag), nonvar(Value))\n"
         "    ).\n";
}

void check_flag_value(const std::filesystem::path& root) {
  const auto suite = root / "flag-value";
  const auto prolog = suite / "logtalk" / "prolog";
  const auto report = root / "flag-value-report.md";
  mosa_test::check(write_text(prolog / "predicates" / "current_prolog_flag_2" / "tests.lgt", flag_value_text()),
                   "write flag value");
  const int code = mosa::run_iso_suite(suite.string(), report.string());
  const std::string body = read_text(report);
  const std::string failed = between_headings(body, "不合格:", "別欄:");
  if (code != 0 || failed.find("lgt_current_prolog_flag_2_20") != std::string::npos) show("flag value report", body);
  mosa_test::check(code == 0, "flag value exits 0");
  mosa_test::check(has_exact_line(body, "分母: 1"), "flag value denominator");
  mosa_test::check(has_exact_line(body, "合格数: 1"), "flag value passed");
  mosa_test::check(failed.find("flag(Flag)") == std::string::npos, "flag value reaches the assertion");
}

// 結果の {} は Logtalk のゴール。述語 {}/1 として呼ぶと存在エラーになる。
std::string retractall_curly_text() {
  return ":- dynamic(foo/1).\n"
         "foo(1).\n"
         "foo(2).\n"
         "foo(3).\n"
         "test(iso_retractall_1_01, true({current_predicate(foo/1)})) :-\n"
         "    {retractall(foo(_))}.\n";
}

void check_retractall_curly(const std::filesystem::path& root) {
  const auto suite = root / "retractall-curly";
  const auto prolog = suite / "logtalk" / "prolog";
  const auto report = root / "retractall-curly-report.md";
  mosa_test::check(write_text(prolog / "predicates" / "retractall_1" / "tests.lgt", retractall_curly_text()),
                   "write retractall curly");
  const int code = mosa::run_iso_suite(suite.string(), report.string());
  const std::string body = read_text(report);
  const std::string failed = between_headings(body, "不合格:", "別欄:");
  if (code != 0 || failed.find("iso_retractall_1_01") != std::string::npos) show("retractall curly report", body);
  mosa_test::check(code == 0, "retractall curly exits 0");
  mosa_test::check(has_exact_line(body, "分母: 1"), "retractall curly denominator");
  mosa_test::check(has_exact_line(body, "合格数: 1"), "retractall curly passed");
  mosa_test::check(failed.find("{}/1") == std::string::npos, "retractall curly is not curly one");
  mosa_test::check(failed.find("iso_retractall_1_01") == std::string::npos, "retractall curly keeps the predicate");
}

// ^ の右の {} はゴール。残すと bagof が {}/1 を呼ぶ。左は存在量化した変数なので辿らない。
std::string caret_curly_text() {
  return "test(caret_curly, true(L == [t])) :-\n"
         "    bagof(t, A^B^C^{true}, L).\n";
}

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

void check_caret_curly(const std::filesystem::path& root) {
  const auto suite = root / "caret-curly";
  const auto prolog = suite / "logtalk" / "prolog";
  const auto report = root / "caret-curly-report.md";
  mosa_test::check(write_text(prolog / "predicates" / "bagof_3" / "tests.lgt", caret_curly_text()),
                   "write caret curly");
  const int code = mosa::run_iso_suite(suite.string(), report.string());
  const std::string body = read_text(report);
  const std::string failed = between_headings(body, "不合格:", "別欄:");
  if (code != 0 || failed.find("caret_curly") != std::string::npos) show("caret curly report", body);
  mosa_test::check(code == 0, "caret curly exits 0");
  mosa_test::check(has_exact_line(body, "分母: 1"), "caret curly denominator");
  mosa_test::check(has_exact_line(body, "合格数: 1"), "caret curly passed");
  mosa_test::check(failed.find("{}/1") == std::string::npos, "caret curly is not curly one");
  mosa_test::check(failed.find("caret_curly") == std::string::npos, "caret curly unwraps the right");
}

}  // namespace

int main() {
  const std::filesystem::path root = std::filesystem::path{MOSA_SOURCE_DIR} / "tests" / "tmp" / "io-fixture";
  const std::filesystem::path scratch = std::filesystem::path{MOSA_SOURCE_DIR} / "tests" / "tmp" / "suite";
  {
    Fixture fixture(root);
    std::error_code error;
    std::filesystem::remove_all(scratch, error);
    check_pair(root);
    check_colon_run(root);
    check_canonical(root);
    check_unknown(root);
    check_reopen(root);
    check_power(root);
    check_sign(root);
    check_abolish(root);
    check_arg(root);
    check_atom_chars(root);
    check_list_uses(root);
    check_bagof_free(root);
    check_retractall_curly(root);
    check_caret_curly(root);
    check_share(root);
    check_colon_user(root);
    check_flag_value(root);
    check_nl_error(root);
    check_open_empty(root);
    check_read_rest(root);
    check_read_vars(root);
    check_read_syntax(root);
    check_read_bad_number(root);
  }
  mosa_test::check(!path_exists(root), "fixture removed");
  mosa_test::check(directory_empty(scratch), "suite scratch empty");
  return mosa_test::failures();
}
