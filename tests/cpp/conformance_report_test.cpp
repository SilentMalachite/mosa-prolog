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
  mosa_test::check(line_at(body, 0) == kBanner, "banner");
  mosa_test::check(has_exact_line(body, "分母: 0"), "denominator 0");
  mosa_test::check(has_exact_line(body, "合格数: 0"), "passed 0");
  mosa_test::check(has_exact_line(body, "合格率: 合格率なし"), "no rate");
  mosa_test::check(body.find('%') == std::string::npos, "no percentage");
  mosa_test::check(has_exact_line(body, "不合格:"), "failure heading");
  mosa_test::check(has_exact_line(body, "別欄:"), "side heading");
  mosa_test::check(report_has(body, "predicates/format_2", "1"), "format_2 count");
  mosa_test::check(report_has(body, "predicates/sort_2", "2"), "sort_2 count");
  mosa_test::check(report_has(body, "predicates/length_2", "0"), "length_2 count");
  mosa_test::check(body.find("atom_length_2") == std::string::npos, "in scope not listed");
  mosa_test::check(body.find("soft_cut") == std::string::npos, "soft cut not listed");
  mosa_test::check(body.find("ground_1") == std::string::npos, "missing tests.lgt");
  mosa_test::check(body.find("unbounded") == std::string::npos, "no tests.lgt name");
  mosa_test::check(body.find("bomb") == std::string::npos, "body not copied");
  mosa_test::check(body.find("OLD") == std::string::npos, "overwritten");
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
  }
  mosa_test::check(!path_exists(root), "fixture removed");
  return mosa_test::failures();
}
