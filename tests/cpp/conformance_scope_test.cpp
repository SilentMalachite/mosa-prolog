// 準拠スイートの範囲の規則（mosa::suite_in_scope、mosa::suite_column）の検査。どのディレクトリをどの欄の分母に入れるかを固定する。
// ファイルは作らない。スクラッチも使わないので資源のロックは要らない。

#include "harness.hpp"
#include "lgt_suite.hpp"
#include "mosa/suite.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace {

constexpr auto kPredicateTrue = std::to_array<std::string_view>({
    "abolish_1",
    "acyclic_term_1",
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
    "subsumes_term_2",
    "term_comparison",
    "term_variables_2",
    "unify_2",
    "unify_with_occurs_check_2",
    "univ_2",
    "var_1",
    "write_term_3",
    "not_1",
});

// 本欄にもライブラリ欄にも入らない。mosa に無い述語と、非目標の portray_1（フェーズ11の範囲の規則）。
constexpr auto kPredicateFalse = std::to_array<std::string_view>({
    "atomic_concat_3",
    "call_nth_2",
    "clause_references",
    "evaluable_property_2",
    "portray_1",
});

// mosa が system か同梱のライブラリで提供する 13 個。ライブラリ欄で数える（フェーズ11の範囲の規則）。
constexpr auto kLibrary = std::to_array<std::string_view>({
    "atomic_list_concat_2",
    "atomic_list_concat_3",
    "between_3",
    "format_2",
    "format_3",
    "forall_2",
    "length_2",
    "msort_2",
    "numbervars_3",
    "plus_3",
    "predicate_property_2",
    "setup_call_cleanup_3",
    "succ_2",
});

// 13211-1 の節番号を持つのに段階表の外にあった 4 つ。フェーズ11で本欄に入る。
constexpr auto kIsoExtra = std::to_array<std::string_view>({
    "sort_2",
    "keysort_2",
    "ground_1",
    "not_unifiable_2",
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
    "unbounded",
    "unbounded/child",
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
    "functions",
    "functions/log10_1/extra",
    "predicates",
    "predicates/no_such",
    "",
});

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
  // 差分テストは両方の欄を流すので、ライブラリ欄も suite_in_scope では真（フェーズ11）。
  expect_scope("predicates/format_2", true);
  expect_scope("predicates/ground_1", true);
  expect_scope("predicates/sort_2", true);
  expect_scope("predicates/length_2", true);
  expect_scope("predicates/keysort_2", true);
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
  expect_children("predicates", kIsoExtra, true);
  expect_children("predicates", kLibrary, true);
}

std::string column_text(std::optional<mosa::SuiteColumn> column) {
  if (!column) return "none";
  return *column == mosa::SuiteColumn::Iso ? "iso" : "library";
}

void expect_column(std::string_view path, std::optional<mosa::SuiteColumn> expected) {
  const auto actual = mosa::suite_column(path);
  mosa_test::check(actual == expected,
                   std::string{path} + " wants " + column_text(expected) + ", got " + column_text(actual));
}

template <std::size_t N>
void expect_column_children(const std::array<std::string_view, N>& names, std::optional<mosa::SuiteColumn> expected) {
  for (const std::string_view name : names) expect_column("predicates/" + std::string{name}, expected);
}

// 本欄とライブラリ欄を混ぜない（フェーズ11の設計のアンチ目標）。どの欄に入るかを固定する。
void check_columns() {
  using mosa::SuiteColumn;
  expect_column("predicates/sort_2", SuiteColumn::Iso);
  expect_column("predicates/keysort_2", SuiteColumn::Iso);
  expect_column("predicates/ground_1", SuiteColumn::Iso);
  expect_column("predicates/not_unifiable_2", SuiteColumn::Iso);
  expect_column("predicates/atom_length_2", SuiteColumn::Iso);
  expect_column("predicates/format_2", SuiteColumn::Library);
  expect_column("predicates/length_2", SuiteColumn::Library);
  expect_column("predicates/between_3", SuiteColumn::Library);
  expect_column("predicates/call_nth_2", std::nullopt);
  expect_column("predicates/clause_references", std::nullopt);
  expect_column("predicates/evaluable_property_2", std::nullopt);
  expect_column("predicates/atomic_concat_3", std::nullopt);
  expect_column("predicates/portray_1", std::nullopt);
  expect_column("directives/conditional_compilation", std::nullopt);
  expect_column("functions/log10_1", std::nullopt);
  expect_column("control/true_0", SuiteColumn::Iso);
  expect_column_children(kLibrary, SuiteColumn::Library);
  expect_column_children(mosa::iso_scope::kLibraryScope, SuiteColumn::Library);
  expect_column_children(kIsoExtra, SuiteColumn::Iso);
  expect_column_children(kPredicateTrue, SuiteColumn::Iso);
  expect_column_children(kPredicateFalse, std::nullopt);
  mosa_test::check(mosa::iso_scope::kLibraryScope.size() == 13, "library scope lists 13 directories");
}

}  // namespace

int main() {
  check_scope();
  check_columns();
  return mosa_test::failures();
}
