#pragma once

// 組込述語の入口（run_builtin が順に呼ぶ run_* と、やり直しの redo_*）と、組込が共に使う誤りの項・リスト・
// 字面の手助け。internal.hpp の末尾から読む（Impl と Goal の定義が要る）。

#include "internal.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mosa {

[[nodiscard]] Cell formal_instantiation(Impl& machine);
[[nodiscard]] Cell formal_uninstantiation(Impl& machine, Cell culprit);
[[nodiscard]] Cell formal_type(Impl& machine, std::string_view type, Cell culprit);
[[nodiscard]] Cell formal_domain(Impl& machine, std::string_view domain, Cell culprit);
[[nodiscard]] Cell formal_existence(Impl& machine, std::string_view kind, Cell culprit);
// existence_error(Kind, Where, Culprit)。SWI-Prolog の sort/4 が引数の無い要素に使う 3 引数の形。
[[nodiscard]] Cell formal_existence_at(Impl& machine, std::string_view kind, Cell where, Cell culprit);
[[nodiscard]] Cell formal_evaluation(Impl& machine, std::string_view which);
[[nodiscard]] Cell formal_representation(Impl& machine, std::string_view which);
[[nodiscard]] Cell formal_syntax(Impl& machine, Cell detail);
[[nodiscard]] Cell formal_system(Impl& machine);
[[nodiscard]] bool run_types(Impl& machine, const Goal& goal);
[[nodiscard]] bool run_order(Impl& machine, const Goal& goal);
// setof/3 は @</2 と同じ標準順序を使う。アルゴリズムは order.cpp に一つ。
[[nodiscard]] int compare_cells(const Impl& machine, Cell left, Cell right);
[[nodiscard]] bool run_terms(Impl& machine, const Goal& goal);
[[nodiscard]] bool run_control(Impl& machine, const Goal& goal);
[[nodiscard]] bool run_qualify(Impl& machine, const Goal& goal);
[[nodiscard]] bool run_arith(Impl& machine, const Goal& goal);
// 式を is/2 と同じく評価する。値が整数でなければ SWI-Prolog の PL_eval_expression_to_int64_ex と同じく
// type_error(integer_expression, 元の式)。誤りを投げたら nullopt（tab/1,2）。
[[nodiscard]] std::optional<std::int64_t> eval_int_expression(Impl& machine, const Goal& goal, Cell cell);
struct CodeText {
  std::string bytes;
  std::vector<char32_t> codes;
  std::vector<std::size_t> cuts;
  [[nodiscard]] std::size_t size() const { return codes.size(); }
  [[nodiscard]] std::string_view slice(std::size_t from, std::size_t count) const {
    return std::string_view{bytes}.substr(cuts[from], cuts[from + count] - cuts[from]);
  }
};

enum class ListShape { Variable, Proper, Partial, Bad };

struct WalkedList {
  ListShape shape = ListShape::Bad;
  std::vector<Cell> items;
  // Partial のときだけ、末尾の未束縛の変数。
  Cell tail{};
};

[[nodiscard]] CodeText decode_codes(std::string_view text);
void append_utf8_code(std::string& text, char32_t code);
[[nodiscard]] WalkedList walk_list(const Impl& machine, Cell list);
[[nodiscard]] Cell list_from_cells(Impl& machine, const std::vector<Cell>& items);
[[nodiscard]] bool one_char_atom(const Impl& machine, Cell cell, char32_t& code);
[[nodiscard]] bool scalar_code(std::int64_t code);
[[nodiscard]] bool run_atoms(Impl& machine, const Goal& goal);
[[nodiscard]] bool redo_atoms(Impl& machine);
[[nodiscard]] bool run_flags(Impl& machine, const Goal& goal);
[[nodiscard]] bool run_statistics(Impl& machine, const Goal& goal);
[[nodiscard]] bool redo_flag_gen(Impl& machine);
[[nodiscard]] bool run_database(Impl& machine, const Goal& goal);
[[nodiscard]] bool run_module_prop(Impl& machine, const Goal& goal);
[[nodiscard]] bool run_solutions(Impl& machine, const Goal& goal);
// ヒープの外に写した項の C++ での大きさ（バイト）。findall の答えと nb_setval/2 の値が Heap::held に数える。
[[nodiscard]] std::size_t footprint(const ExportedTerm& term);
[[nodiscard]] bool run_global_vars(Impl& machine, const Goal& goal);
// Ctrl::Collect の枠に着いた解を、stack[entry] の収集に記録して失敗する（ADR 0013）。
void collect_step(Impl& machine, std::size_t entry);
// backtrack が一番上の Pick::Collect に戻ったとき。状態は戻してある。Entry を外して答えを束ねる。
void finish_collect(Impl& machine);
void init_streams(Impl& machine);
// 開いている書き込みのファイルを、閉じずに今の中身で書き出す。halt と Machine の終わりで呼ぶ（レビュー M5）。
void flush_open_files(Impl& machine);
[[nodiscard]] bool run_streams(Impl& machine, const Goal& goal);
void run_current_input(Impl& machine, const Goal& goal);
void run_current_output(Impl& machine, const Goal& goal);
void run_set_input(Impl& machine, const Goal& goal);
void run_set_output(Impl& machine, const Goal& goal);
void run_set_position(Impl& machine, const Goal& goal);
void run_at_end(Impl& machine, const Goal& goal);
void run_stream_property(Impl& machine, const Goal& goal);
[[nodiscard]] bool redo_stream_prop(Impl& machine);
[[nodiscard]] bool run_char_io(Impl& machine, const Goal& goal);
[[nodiscard]] bool run_byte_io(Impl& machine, const Goal& goal);
[[nodiscard]] bool run_read(Impl& machine, const Goal& goal);
[[nodiscard]] bool run_write(Impl& machine, const Goal& goal);
[[nodiscard]] bool run_term_text(Impl& machine, const Goal& goal);
[[nodiscard]] bool run_operators(Impl& machine, const Goal& goal);
[[nodiscard]] bool redo_current_op(Impl& machine);
[[nodiscard]] bool redo_current_char_conversion(Impl& machine);
[[nodiscard]] bool run_lists(Impl& machine, const Goal& goal);
[[nodiscard]] bool redo_length_gen(Impl& machine);
[[nodiscard]] bool run_format_float(Impl& machine, const Goal& goal);
[[nodiscard]] bool run_case(Impl& machine, const Goal& goal);
[[nodiscard]] bool run_char_width(Impl& machine, const Goal& goal);
int apply_char_conv_directive(Impl& machine, Cell directive);
[[nodiscard]] bool redo_retract(Impl& machine);
[[nodiscard]] bool redo_clause_scan(Impl& machine);
[[nodiscard]] bool redo_pred_gen(Impl& machine);

}  // namespace mosa
