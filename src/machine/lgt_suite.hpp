#pragma once

// 分母は logtalk/prolog より下だけ。区切りは /。先頭と末尾の / は無い。
// 一覧に無いパスは別欄。実行しない。

#include <algorithm>
#include <array>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace mosa {
namespace iso_scope {

inline bool listed(std::span<const std::string_view> names, std::string_view name) {
  return std::find(names.begin(), names.end(), name) != names.end();
}

inline bool under(std::string_view path, std::string_view root) {
  return path.size() > root.size() && path.starts_with(root) && path[root.size()] == '/';
}

inline bool path_is_or_under(std::string_view path, std::string_view root) {
  return path == root || under(path, root);
}

inline bool direct_child(std::string_view path, std::string_view parent) {
  if (!under(path, parent)) return false;
  return path.find('/', parent.size() + 1) == std::string_view::npos;
}

inline std::string_view child_name(std::string_view path, std::string_view parent) {
  return path.substr(parent.size() + 1);
}

// 段階 A〜E のディレクトリだけ真。not_1 は (\+)/1。一覧に無い子は別欄。
inline constexpr auto kPredicateScope = std::to_array<std::string_view>({
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

// 段階 B の表に無い 9 個。integer_power_2 は (^)/2 なので外さない。
inline constexpr auto kFunctionExcluded = std::to_array<std::string_view>({
    "acosh_1",
    "asinh_1",
    "atanh_1",
    "cosh_1",
    "sinh_1",
    "tanh_1",
    "log10_1",
    "lsb_1",
    "popcount_1",
});

inline bool excluded_tree(std::string_view path) {
  return path_is_or_under(path, "unbounded") || path_is_or_under(path, "unicode/encodings") ||
         path_is_or_under(path, "unicode/directives") ||
         path_is_or_under(path, "directives/conditional_compilation") ||
         path_is_or_under(path, "syntax/directives/meta_predicate_1");
}

inline bool unicode_leaf(std::string_view path) {
  return path == "unicode/builtins" || path == "unicode/case_variables" || path == "unicode/escape_sequences";
}

}  // namespace iso_scope

// 除外を先に見る。syntax/ と directives/ の例外は、親の許可より狭い。
inline bool suite_in_scope(std::string_view relative_posix) {
  if (iso_scope::excluded_tree(relative_posix)) return false;
  if (iso_scope::direct_child(relative_posix, "control")) return true;
  if (iso_scope::direct_child(relative_posix, "directives")) return true;
  if (relative_posix == "logical_update_semantics") return true;
  if (iso_scope::under(relative_posix, "syntax")) return true;
  if (iso_scope::unicode_leaf(relative_posix)) return true;
  if (iso_scope::direct_child(relative_posix, "functions")) {
    const auto name = iso_scope::child_name(relative_posix, "functions");
    return !iso_scope::listed(iso_scope::kFunctionExcluded, name);
  }
  if (iso_scope::direct_child(relative_posix, "predicates")) {
    const auto name = iso_scope::child_name(relative_posix, "predicates");
    return iso_scope::listed(iso_scope::kPredicateScope, name);
  }
  return false;
}

struct LgtCase {
  std::string name;
  std::string outcome;
  std::string body;
  bool outside = false;
};

struct LgtFile {
  std::string program;
  std::vector<LgtCase> cases;
  std::string error;
};

struct LgtRun {
  int passed = 0;
  int failed = 0;
  std::vector<std::string> failures;
  std::vector<std::string> outside;
};

[[nodiscard]] LgtFile read_lgt_file(std::string_view source);
[[nodiscard]] LgtRun execute_lgt(const LgtFile& file);

}  // namespace mosa
