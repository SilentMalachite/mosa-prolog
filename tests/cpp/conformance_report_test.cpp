// 準拠スイートの選別。同梱の logtalk は読まない。一時木は tests/tmp だけ。
// 別欄を実行して std::exit すると、報告書の検査に届かない。

#include "harness.hpp"
#include "lgt_suite.hpp"
#include "mosa/suite.hpp"

#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>

namespace {

constexpr auto kPredicateTrue = std::to_array<std::string_view>({
    "abolish_1",
    "arg_3",
    "arithmetic_comparison",
    "asserta_1",
    "assertz_1",
    "at_end_of_stream_1",
    "atom_1",
    "atom_chars_2",
    "atom_codes_2",
    "atom_concat_3",
    "atom_length_2",
    "atomic_1",
    "bagof_3",
    "call_N",
    "callable_1",
    "char_code_2",
    "clause_2",
    "close_2",
    "compare_3",
    "compound_1",
    "copy_term_2",
    "current_input_1",
    "current_op_3",
    "current_output_1",
    "current_predicate_1",
    "current_prolog_flag_2",
    "false_0",
    "findall_3",
    "findall_4",
    "float_1",
    "flush_output_1",
    "functor_3",
    "get_byte_2",
    "get_char_2",
    "get_code_2",
    "halt_1",
    "integer_1",
    "is_2",
    "nl_1",
    "nonvar_1",
    "number_1",
    "number_chars_2",
    "number_codes_2",
    "once_1",
    "op_3",
    "open_4",
    "peek_byte_2",
    "peek_char_2",
    "peek_code_2",
    "print_1",
    "print_2",
    "put_byte_2",
    "put_char_2",
    "put_code_2",
    "read_term_3",
    "repeat_0",
    "retract_1",
    "retractall_1",
    "set_input_1",
    "set_output_1",
    "set_prolog_flag_2",
    "set_stream_position_2",
    "setof_3",
    "stream_property_2",
    "sub_atom_5",
    "term_comparison",
    "term_variables_2",
    "unify_2",
    "unify_with_occurs_check_2",
    "univ_2",
    "var_1",
    "write_term_3",
    "not_1",
});

constexpr auto kPredicateFalse = std::to_array<std::string_view>({
    "acyclic_term_1",
    "atomic_concat_3",
    "atomic_list_concat_2",
    "atomic_list_concat_3",
    "between_3",
    "call_nth_2",
    "clause_references",
    "evaluable_property_2",
    "forall_2",
    "format_2",
    "format_3",
    "ground_1",
    "keysort_2",
    "length_2",
    "msort_2",
    "not_unifiable_2",
    "numbervars_3",
    "plus_3",
    "portray_1",
    "predicate_property_2",
    "setup_call_cleanup_3",
    "sort_2",
    "subsumes_term_2",
    "succ_2",
});

constexpr auto kFunctionFalse = std::to_array<std::string_view>({
    "acosh_1", "asinh_1", "atanh_1", "cosh_1", "sinh_1", "tanh_1", "log10_1", "lsb_1", "popcount_1",
});

constexpr auto kFunctionTrue = std::to_array<std::string_view>({
    "integer_power_2",
    "abs_1",
    "log_1",
    "msb_1",
    "xor_2",
});

constexpr auto kRuleTrue = std::to_array<std::string_view>({
    "control/soft_cut_2_3",
    "control/true_0",
    "directives/op_3",
    "directives/include_1",
    "logical_update_semantics",
    "syntax/atoms",
    "syntax/directives",
    "syntax/directives/op_3",
    "unicode/builtins",
    "unicode/case_variables",
    "unicode/escape_sequences",
});

constexpr auto kRuleFalse = std::to_array<std::string_view>({
    "control",
    "control/soft_cut_2_3/extra",
    "directives",
    "directives/conditional_compilation",
    "directives/conditional_compilation/nested",
    "logical_update_semantics/child",
    "syntax",
    "syntax/directives/meta_predicate_1",
    "syntax/directives/meta_predicate_1/extra",
    "unicode",
    "unicode/builtins/extra",
    "unicode/encodings",
    "unicode/encodings/utf_8",
    "unicode/directives",
    "unicode/directives/encoding_1",
    "unbounded",
    "unbounded/child",
    "functions",
    "functions/log10_1/extra",
    "predicates",
    "predicates/no_such",
    "",
});

constexpr std::string_view kBanner = "このファイルは自動生成である。手で編集しない。";

void expect_scope(std::string_view path, bool in_scope) {
  const std::string label = std::string{in_scope ? "in " : "out "} + std::string{path};
  mosa_test::check(mosa::suite_in_scope(path) == in_scope, label);
}

template <std::size_t N>
void expect_all(const std::array<std::string_view, N>& paths, bool in_scope) {
  for (const std::string_view path : paths) expect_scope(path, in_scope);
}

template <std::size_t N>
void expect_children(std::string_view parent, const std::array<std::string_view, N>& names, bool in_scope) {
  for (const std::string_view name : names) {
    const std::string path = std::string{parent} + "/" + std::string{name};
    expect_scope(path, in_scope);
  }
}

void check_scope() {
  expect_scope("predicates/atom_length_2", true);
  expect_scope("predicates/format_2", false);
  expect_scope("predicates/ground_1", false);
  expect_scope("predicates/sort_2", false);
  expect_scope("predicates/length_2", false);
  expect_scope("predicates/keysort_2", false);
  expect_scope("functions/log10_1", false);
  expect_scope("control/soft_cut_2_3", true);
  expect_scope("predicates/not_1", true);
  expect_scope("predicates/print_1", true);
  expect_scope("predicates/print_2", true);
  expect_all(kRuleTrue, true);
  expect_all(kRuleFalse, false);
  expect_children("predicates", kPredicateTrue, true);
  expect_children("predicates", kPredicateFalse, false);
  expect_children("functions", kFunctionTrue, true);
  expect_children("functions", kFunctionFalse, false);
}

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

bool line_has_tokens(std::string_view line, std::string_view dir, std::string_view count) {
  bool saw_dir = false;
  bool saw_count = false;
  std::size_t pos = 0;
  while (pos < line.size()) {
    while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t')) ++pos;
    if (pos >= line.size()) break;
    const auto stop = line.find_first_of(" \t", pos);
    const auto token = line.substr(pos, stop == std::string_view::npos ? std::string_view::npos : stop - pos);
    if (token == dir) saw_dir = true;
    if (token == count) saw_count = true;
    if (stop == std::string_view::npos) break;
    pos = stop;
  }
  return saw_dir && saw_count;
}

bool report_has(std::string_view text, std::string_view dir, std::string_view count) {
  std::size_t start = 0;
  while (start <= text.size()) {
    if (line_has_tokens(line_at(text, start), dir, count)) return true;
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

int non_empty_lines(std::string_view text) {
  int count = 0;
  std::size_t start = 0;
  while (start < text.size()) {
    if (!line_at(text, start).empty()) ++count;
    const auto end = text.find('\n', start);
    if (end == std::string_view::npos) break;
    start = end + 1;
  }
  return count;
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

void check_absent(const std::filesystem::path& root) {
  const auto dir = root / "absent";
  const auto report = root / "absent-report.md";
  std::error_code error;
  std::filesystem::create_directories(dir, error);
  mosa_test::check(!error, "absent dir");
  const int code = mosa::run_iso_suite(dir.string(), report.string());
  mosa_test::check(code == 1, "absent returns 1");
  mosa_test::check(!path_exists(report), "absent writes no report");
}

void check_side_only(const std::filesystem::path& root) {
  const auto suite = root / "tree";
  const auto prolog = suite / "logtalk" / "prolog";
  const auto report = root / "report.md";
  mosa_test::check(write_text(prolog / "predicates" / "format_2" / "tests.lgt",
                              "% kept\ntest(bomb, true) :- {throw(1)}.\n"),
                   "write bomb");
  mosa_test::check(write_text(prolog / "predicates" / "atom_length_2" / "tests.lgt",
                              "test(live, true) :- {throw(1)}.\n"),
                   "write in scope");
  mosa_test::check(write_text(prolog / "control" / "soft_cut_2_3" / "tests.lgt", "test(cut, true) :- {true}.\n"),
                   "write soft cut");
  mosa_test::check(write_text(prolog / "predicates" / "sort_2" / "tests.lgt",
                              "note.\ntest(a, false).\ntest(b, true).\n"),
                   "write sort");
  mosa_test::check(write_text(prolog / "predicates" / "length_2" / "tests.lgt", ""), "write empty");
  std::error_code error;
  std::filesystem::create_directories(prolog / "predicates" / "ground_1", error);
  mosa_test::check(!error, "ground dir");
  mosa_test::check(write_text(prolog / "unbounded" / "skip.txt", "test(nope, true).\n"), "write unbounded");
  mosa_test::check(write_text(report, "OLD\n"), "seed report");
  const int code = mosa::run_iso_suite(suite.string(), report.string());
  mosa_test::check(code == 1, "side returns 1");
  const std::string body = read_text(report);
  const std::string failed = between_headings(body, "不合格:", "別欄:");
  const std::string side = after_heading(body, "別欄:");
  mosa_test::check(line_at(body, 0) == kBanner, "banner");
  // throw(1) は true に合わない。true は合格。1/2 は 90% 未満なので終了コードは 1。
  mosa_test::check(has_exact_line(body, "分母: 2"), "denominator 2");
  mosa_test::check(has_exact_line(body, "合格数: 1"), "passed 1");
  mosa_test::check(has_exact_line(body, "合格率: 50.0"), "rate 50.0");
  mosa_test::check(body.find('%') == std::string::npos, "no percent sign");
  mosa_test::check(has_exact_line(body, "不合格:"), "failure heading");
  mosa_test::check(has_exact_line(body, "別欄:"), "side heading");
  mosa_test::check(report_has(body, "predicates/format_2", "1"), "format_2 count");
  mosa_test::check(report_has(body, "predicates/sort_2", "2"), "sort_2 count");
  mosa_test::check(report_has(body, "predicates/length_2", "0"), "length_2 count");
  mosa_test::check(failed.find("atom_length_2") != std::string::npos, "in scope executed");
  mosa_test::check(side.find("atom_length_2") == std::string::npos, "in scope not side");
  mosa_test::check(failed.find("format_2") == std::string::npos, "format not executed");
  mosa_test::check(body.find("soft_cut") == std::string::npos, "soft cut not listed");
  mosa_test::check(body.find("ground_1") == std::string::npos, "missing tests.lgt");
  mosa_test::check(body.find("unbounded") == std::string::npos, "no tests.lgt name");
  mosa_test::check(body.find("bomb") == std::string::npos, "body not copied");
  mosa_test::check(body.find("OLD") == std::string::npos, "overwritten");
}

// 指示と {ゴール} を含む。halt のあと、setup、cleanup、演算子が残ることをこの 12 件で見る。
std::string true_0_text() {
  return R"lgt(p(1).
:- object(tests, extends(lgtunit)).
:- info([version is 1:0:0]).
:- set_logtalk_flag(report, warnings).
test(one, true) :- {p(1)}.
test(two, true(N == 1)) :- {p(N)}.
test(no, false) :- {p(2)}.
test(bad, error(instantiation_error)) :- {throw(error(instantiation_error, context(a/0, _)))}.
test(halted, true) :- {halt(1)}.
test(after, true) :- {p(1)}.
setup :- assertz(tick(1)).
test(tick_a, true) :- {retract(tick(1))}.
test(tick_b, true) :- {retract(tick(1))}.
cleanup :- assertz(marker(1)).
test(mark_a, true) :- {true}.
test(mark_b, true) :- {marker(1)}.
test(op_def, true) :- {op(100, xfx, qq)}.
test(op_see, true) :- {current_op(100, xfx, qq)}.
:- end_object.
)lgt";
}

// 閉じ括弧が無い。項として読めない実行対象は不合格 1 件で、プロセスは戻る。
std::string unclosed_object() { return ":- object(tests, extends(lgtunit)\n"; }

void check_executes(const std::filesystem::path& root) {
  const auto suite = root / "run";
  const auto prolog = suite / "logtalk" / "prolog";
  const auto report = root / "run-report.md";
  mosa_test::check(write_text(prolog / "control" / "true_0" / "tests.lgt", true_0_text()), "write true_0");
  mosa_test::check(write_text(prolog / "control" / "cut_0" / "tests.lgt", unclosed_object()), "write cut_0");
  const int code = mosa::run_iso_suite(suite.string(), report.string());
  const std::string body = read_text(report);
  const std::string failed = between_headings(body, "不合格:", "別欄:");
  // 12*10 >= 13*9。計画の終了コード 1 は、仕様の整数比較に負ける。
  mosa_test::check(code == 0, "twelve exits 0");
  mosa_test::check(has_exact_line(body, "分母: 13"), "denominator 13");
  mosa_test::check(has_exact_line(body, "合格数: 12"), "passed 12");
  mosa_test::check(has_exact_line(body, "合格率: 92.3"), "rate 92.3");
  mosa_test::check(non_empty_lines(failed) == 1, "one failure");
  mosa_test::check(has_exact_line(failed, "control/cut_0/tests.lgt"), "unreadable path");
}

std::string eight_one() {
  std::string text;
  for (int index = 0; index < 8; ++index) {
    text += "test(a" + std::to_string(index) + ", true) :- {true}.\n";
  }
  text += "test(nope, true) :- {fail}.\n";
  return text;
}

void check_rate(const std::filesystem::path& root) {
  const auto suite = root / "rate";
  const auto prolog = suite / "logtalk" / "prolog";
  const auto report = root / "rate-report.md";
  mosa_test::check(write_text(prolog / "control" / "fail_0" / "tests.lgt", eight_one()), "write eight");
  const int code = mosa::run_iso_suite(suite.string(), report.string());
  const std::string body = read_text(report);
  // 8*10 < 9*9。8/9 の表示は 88.9 であり、90 ではない。
  mosa_test::check(code == 1, "eight exits 1");
  mosa_test::check(has_exact_line(body, "分母: 9"), "denominator 9");
  mosa_test::check(has_exact_line(body, "合格数: 8"), "passed 8");
  mosa_test::check(has_exact_line(body, "合格率: 88.9"), "rate 88.9");
}

std::string outside_text() {
  return "test(keep, true) :- {true}.\n- test(skip, true) :- {throw(1)}.\n";
}

void check_zero_rate(const std::filesystem::path& root) {
  const auto suite = root / "zero";
  const auto prolog = suite / "logtalk" / "prolog";
  const auto report = root / "zero-report.md";
  mosa_test::check(write_text(prolog / "predicates" / "format_2" / "tests.lgt",
                              "test(bomb, true) :- {throw(1)}.\n"),
                   "write zero bomb");
  const int code = mosa::run_iso_suite(suite.string(), report.string());
  const std::string body = read_text(report);
  mosa_test::check(code == 1, "zero returns 1");
  mosa_test::check(has_exact_line(body, "分母: 0"), "zero denominator");
  mosa_test::check(has_exact_line(body, "合格数: 0"), "zero passed");
  mosa_test::check(has_exact_line(body, "合格率: 合格率なし"), "zero rate");
  mosa_test::check(body.find("bomb") == std::string::npos, "zero not read");
}

std::string shared_var_text() {
  return "test(diff, error(f(X,X))) :- {throw(error(f(a,b), c))}.\n"
         "test(same, error(f(X,X))) :- {throw(error(f(a,a), c))}.\n"
         "test(cells, error(f(X,X))) :- {throw(error(f(A,B), c))}.\n"
         "test(share, error(f(X,X))) :- {throw(error(f(A,A), c))}.\n"
         "test(nest, error(g(X,X))) :- {throw(error(g(f(A),f(B)), c))}.\n";
}

// 事実が 1 つならやり直しは失敗する。選択点が残っても、次が失敗すれば合格。
std::string det_one_text() {
  return "p(1).\n"
         "test(det_one, deterministic) :- {p(1)}.\n"
         "test(det_one_as, deterministic(N == 1)) :- {p(N)}.\n"
         "test(det_choice, deterministic) :- {(true; fail)}.\n";
}

// 同じ事実が 2 つあるとやり直しが成功する。最初の表明が通っても不合格。
std::string det_two_text() {
  return "p(1).\n"
         "p(1).\n"
         "test(det_two, deterministic) :- {p(1)}.\n"
         "test(det_two_as, deterministic(N == 1)) :- {p(N)}.\n";
}

std::string sol_text() {
  return "p(1).\n"
         "p(2).\n"
         "test(sol_exists, exists(N == 2)) :- {p(N)}.\n"
         "test(sol_all, all(N < 3)) :- {p(N)}.\n";
}

// 最初の解で表明が失敗したら、後の解は見ない。
std::string later_text() {
  return "q(1).\n"
         "q(2).\n"
         "test(sol_later, deterministic(N == 2)) :- {q(N)}.\n";
}

std::string shapes_text() {
  return R"lgt(:- uses(lgtunit, [
	op(700, xfx, =~=), (=~=)/2
]).
test(all_zero, all(N < 3)) :- {fail}.
test(err_list, errors([instantiation_error, type_error(atom,1)])) :-
	{throw(error(instantiation_error, x))}.
test(ball_oops, ball(oops)) :- {throw(oops)}.
test(var_copy, variant(S, [1+_])) :- {S = [1+A]}.
test(var_share, variant(S, [1+_,1+_])) :- {S = [1+A,1+A]}.
test(sub_wide, subsumes(f(_), X)) :- {X = f(a)}.
test(approx_ok, true(X =~= 1.0)) :- {X is 1.0}.
test(assert_ok, true) :- assertion(1 == 1).
test(assert_bad, true) :- assertion(1 == 0).
test(assert_or, true) :- (assertion(1 == 0); assertion(1 == 1)).
test(hide_var, false) :- {current_predicate(variant/2)}.
test(hide_assert, false) :- {current_predicate(assertion/1)}.
test(hide_approx, false) :- {current_predicate(('=~=')/2)}.
)lgt";
}

bool token_has(std::string_view text, std::string_view name) {
  std::size_t pos = 0;
  while (pos < text.size()) {
    const auto found = text.find(name, pos);
    if (found == std::string_view::npos) return false;
    const bool left = found == 0 || text[found - 1] == ' ' || text[found - 1] == '\n';
    const auto after = found + name.size();
    const bool right = after >= text.size() || text[after] == ' ' || text[after] == '\n';
    if (left && right) return true;
    pos = found + name.size();
  }
  return false;
}

void expect_token(std::string_view text, std::string_view name, bool present) {
  mosa_test::check(token_has(text, name) == present, name);
}

// 部分文字列 =~= で本体を query しないと assertz が走らず、次の tick/1 が残らない。
std::string machine_body_text() {
  return "p(f(_)).\n"
         "test(ran, true) :- {current_predicate(('=~=')/2), fail ; assertz(tick(1))}.\n"
         "test(ticked, true) :- {tick(1)}.\n"
         "test(atom_ok, true) :- {assertion(atom(a))}.\n"
         "test(mix, true) :- {p(Y)}, variant(Y, f(_)).\n";
}

void check_machine_body(const std::filesystem::path& root) {
  const auto suite = root / "body";
  const auto prolog = suite / "logtalk" / "prolog";
  const auto report = root / "body-report.md";
  mosa_test::check(write_text(prolog / "predicates" / "compound_1" / "tests.lgt", machine_body_text()),
                   "write machine body");
  const int code = mosa::run_iso_suite(suite.string(), report.string());
  const std::string body = read_text(report);
  const std::string failed = between_headings(body, "不合格:", "別欄:");
  mosa_test::check(code == 0, "body exits 0");
  mosa_test::check(has_exact_line(body, "分母: 4"), "body denominator");
  mosa_test::check(has_exact_line(body, "合格数: 4"), "body passed");
  mosa_test::check(!token_has(failed, "ran"), "ran queried");
  mosa_test::check(!token_has(failed, "ticked"), "tick asserted");
  mosa_test::check(!token_has(failed, "atom_ok"), "atom ran");
  mosa_test::check(!token_has(failed, "mix"), "variant after p");
}

// 食い違う節は別ファイル。解が 1 と 2 の all と、解が 0 の all は同じプログラムに置けない。
void check_judgments(const std::filesystem::path& root) {
  const auto suite = root / "judge";
  const auto prolog = suite / "logtalk" / "prolog";
  const auto report = root / "judge-report.md";
  mosa_test::check(write_text(prolog / "predicates" / "atom_length_2" / "tests.lgt", det_one_text()), "write det one");
  mosa_test::check(write_text(prolog / "predicates" / "atom_1" / "tests.lgt", det_two_text()), "write det two");
  mosa_test::check(write_text(prolog / "predicates" / "atomic_1" / "tests.lgt", sol_text()), "write sols");
  mosa_test::check(write_text(prolog / "predicates" / "atom_concat_3" / "tests.lgt", later_text()), "write later");
  mosa_test::check(write_text(prolog / "predicates" / "atom_codes_2" / "tests.lgt", shapes_text()), "write shapes");
  const int code = mosa::run_iso_suite(suite.string(), report.string());
  const std::string body = read_text(report);
  const std::string failed = between_headings(body, "不合格:", "別欄:");
  // 16/21 は 90% 未満。やり直しが成功するテストと、共有変数と、失敗する表明だけが不合格。
  mosa_test::check(code == 1, "judge exits 1");
  mosa_test::check(has_exact_line(body, "分母: 21"), "judge denominator");
  mosa_test::check(has_exact_line(body, "合格数: 16"), "judge passed");
  mosa_test::check(has_exact_line(body, "合格率: 76.2"), "judge rate");
  constexpr auto kPass = std::to_array<std::string_view>({
      "det_one",     "det_one_as", "det_choice", "sol_exists", "sol_all",    "all_zero",
      "err_list",    "ball_oops",  "var_copy",   "sub_wide",   "approx_ok",  "assert_ok",
      "assert_or",   "hide_var",   "hide_assert", "hide_approx",
  });
  constexpr auto kFail = std::to_array<std::string_view>({"det_two", "det_two_as", "sol_later", "var_share", "assert_bad"});
  for (const std::string_view name : kPass) expect_token(failed, name, false);
  for (const std::string_view name : kFail) expect_token(failed, name, true);
}

void check_shared_var(const std::filesystem::path& root) {
  const auto suite = root / "share";
  const auto prolog = suite / "logtalk" / "prolog";
  const auto report = root / "share-report.md";
  mosa_test::check(write_text(prolog / "control" / "throw_1" / "tests.lgt", shared_var_text()), "write share");
  const int code = mosa::run_iso_suite(suite.string(), report.string());
  const std::string body = read_text(report);
  const std::string failed = between_headings(body, "不合格:", "別欄:");
  // 同じ形式変数は同じ項。別の変数セル f(A) と f(B)、および A と B は不一致。
  mosa_test::check(code == 1, "share exits 1");
  mosa_test::check(has_exact_line(body, "分母: 5"), "share denominator");
  mosa_test::check(has_exact_line(body, "合格数: 2"), "share passed");
  mosa_test::check(failed.find("diff") != std::string::npos, "diff does not subsume");
  mosa_test::check(failed.find("same") == std::string::npos, "same subsumes");
  mosa_test::check(failed.find("cells") != std::string::npos, "distinct vars do not subsume");
  mosa_test::check(failed.find("share") == std::string::npos, "shared var cell subsumes");
  mosa_test::check(failed.find("nest") != std::string::npos, "distinct f(A) f(B) do not subsume");
}

int count_exact(std::string_view text, std::string_view expected) {
  int count = 0;
  std::size_t start = 0;
  while (start <= text.size()) {
    if (line_at(text, start) == expected) ++count;
    const auto end = text.find('\n', start);
    if (end == std::string_view::npos) break;
    start = end + 1;
  }
  return count;
}

// 各グループは endif で閉じる。閉じないと項の範囲が無く、ファイル全体が不合格 1 件。
std::string branch_text() {
  return R"lgt(:- if(catch(1^true, _, fail)).
test(if_yes, true) :- {fail}.
:- else.
test(if_no, true) :- {true}.
:- endif.
:- if(current_logtalk_flag(coinduction, supported)).
test(cyc, true) :- {true}.
:- else.
- test(cyc, true).
:- endif.
:- if(current_logtalk_flag(prolog_dialect, swi)).
:- else.
test(dialect, true) :- {true}.
:- endif.
test(depth, true, [condition(max_depth_option_supported)]) :- {true}.
max_depth_option_supported :- current_logtalk_flag(prolog_dialect, Dialect).
test(soft, true, [condition(predicate_property('*->'(_,_), built_in))]) :- {true}.
test(rest, true) :- {true}.
:- if(os::operating_system_type(windows)).
test(win, true) :- {fail}.
:- else.
test(win, true) :- {true}.
:- endif.
)lgt";
}

void check_branches(const std::filesystem::path& root) {
  const auto suite = root / "branch";
  const auto prolog = suite / "logtalk" / "prolog";
  const auto report = root / "branch-report.md";
  mosa_test::check(write_text(prolog / "predicates" / "bagof_3" / "tests.lgt", branch_text()), "write branches");
  const int code = mosa::run_iso_suite(suite.string(), report.string());
  const std::string body = read_text(report);
  const std::string failed = between_headings(body, "不合格:", "別欄:");
  const std::string side = after_heading(body, "別欄:");
  // 選ばなかった枝の fail が走ると不合格が増える。別欄なので分母は 5。
  mosa_test::check(code == 0, "branch exits 0");
  mosa_test::check(has_exact_line(body, "分母: 5"), "branch denominator");
  mosa_test::check(has_exact_line(body, "合格数: 5"), "branch passed");
  mosa_test::check(non_empty_lines(failed) == 0, "branch adds no failure");
  mosa_test::check(count_exact(side, "predicates/bagof_3 if_yes") == 1, "if_yes side");
  mosa_test::check(count_exact(side, "predicates/bagof_3 cyc") == 2, "cyc both side");
  mosa_test::check(count_exact(side, "predicates/bagof_3 depth") == 1, "depth side");
  mosa_test::check(count_exact(side, "predicates/bagof_3 win") == 1, "win unselected side");
  mosa_test::check(!token_has(side, "if_no"), "if_no ran");
  mosa_test::check(!token_has(side, "dialect"), "dialect ran");
  mosa_test::check(!token_has(side, "soft"), "soft in denominator");
  mosa_test::check(!token_has(side, "rest"), "rest ran");
  mosa_test::check(!token_has(failed, "if_yes"), "if_yes body did not run");
  mosa_test::check(!token_has(failed, "win"), "win fail did not run");
}

void check_open_if(const std::filesystem::path& root) {
  const auto suite = root / "open-if";
  const auto prolog = suite / "logtalk" / "prolog";
  const auto report = root / "open-if-report.md";
  mosa_test::check(write_text(prolog / "control" / "true_0" / "tests.lgt",
                              ":- if(true).\ntest(later, true) :- {true}.\n"),
                   "write open if");
  const int code = mosa::run_iso_suite(suite.string(), report.string());
  const std::string body = read_text(report);
  const std::string failed = between_headings(body, "不合格:", "別欄:");
  mosa_test::check(code == 1, "open if exits 1");
  mosa_test::check(has_exact_line(body, "分母: 1"), "open if denominator");
  mosa_test::check(non_empty_lines(failed) == 1, "open if one failure");
  mosa_test::check(has_exact_line(failed, "control/true_0/tests.lgt"), "open if path");
  mosa_test::check(failed.find("later") == std::string::npos, "open if did not run");
}

std::string qual_text() { return "user::aa(1).\ntest(see, true) :- {aa(1)}.\n"; }

std::string bind_text() {
  return "test(bound, true, [condition((X = 1, X == 1))]) :- {true}.\n"
         "test(cutelse, true, [condition((true -> fail ; true))]) :- {true}.\n";
}

void check_machine_goal(const std::filesystem::path& root) {
  const auto suite = root / "colon";
  const auto prolog = suite / "logtalk" / "prolog";
  const auto report = root / "colon-report.md";
  mosa_test::check(write_text(prolog / "predicates" / "unify_2" / "tests.lgt", qual_text()), "write qual");
  mosa_test::check(write_text(prolog / "predicates" / "callable_1" / "tests.lgt", bind_text()), "write bind");
  const int code = mosa::run_iso_suite(suite.string(), report.string());
  const std::string body = read_text(report);
  const std::string failed = between_headings(body, "不合格:", "別欄:");
  const std::string side = after_heading(body, "別欄:");
  // user:: を外すと aa/1 が載る。束縛は一つの query。true -> fail ; true は失敗なので別欄。
  mosa_test::check(code == 0, "colon exits 0");
  mosa_test::check(has_exact_line(body, "分母: 2"), "colon denominator");
  mosa_test::check(has_exact_line(body, "合格数: 2"), "colon passed");
  mosa_test::check(!token_has(failed, "see"), "see loaded");
  mosa_test::check(!token_has(failed, "bound"), "bound keeps X");
  mosa_test::check(!token_has(side, "bound"), "bound not side");
  mosa_test::check(token_has(side, "cutelse"), "cutelse side");
  mosa_test::check(!token_has(failed, "cutelse"), "cutelse not failed");
}

void check_outside(const std::filesystem::path& root) {
  const auto suite = root / "out";
  const auto prolog = suite / "logtalk" / "prolog";
  const auto report = root / "out-report.md";
  mosa_test::check(write_text(prolog / "control" / "call_1" / "tests.lgt", outside_text()), "write outside");
  const int code = mosa::run_iso_suite(suite.string(), report.string());
  const std::string body = read_text(report);
  const std::string failed = between_headings(body, "不合格:", "別欄:");
  // 実行すれば throw で不合格が増える。別欄なので合格数も不合格数も増えない。
  mosa_test::check(code == 0, "outside exits 0");
  mosa_test::check(has_exact_line(body, "分母: 1"), "outside denominator");
  mosa_test::check(has_exact_line(body, "合格数: 1"), "outside passed");
  mosa_test::check(non_empty_lines(failed) == 0, "outside adds no failure");
  mosa_test::check(report_has(body, "control/call_1", "skip"), "outside side");
}

}  // namespace

int main() {
  check_scope();
  const std::filesystem::path root =
      std::filesystem::path{MOSA_SOURCE_DIR} / "tests" / "tmp" / "suite-fixture";
  {
    Fixture fixture(root);
    check_absent(root);
    check_side_only(root);
    check_zero_rate(root);
    check_executes(root);
    check_rate(root);
    check_outside(root);
    check_shared_var(root);
    check_judgments(root);
    check_machine_body(root);
    check_branches(root);
    check_open_if(root);
    check_machine_goal(root);
  }
  mosa_test::check(!path_exists(root), "fixture removed");
  return mosa_test::failures();
}
