#pragma once

// 機械の非公開状態。ゴール枠の語 0 が 0 のときは制御枠で、述語枠ではない。

#include "mosa/ir.hpp"
#include "mosa/machine.hpp"
#include "mosa/reader.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace mosa {

inline constexpr std::uint64_t kNoFrame = UINT64_MAX;
inline constexpr std::size_t kFrameHead = 4;

enum class KeyKind { Any, None, Var, Atom, Int, Float, List, Fun };

struct Clause {
  IrTerm head;
  IrTerm body;
  std::uint16_t vars = 0;
  KeyKind key = KeyKind::Any;
  std::uint32_t key_atom = 0;
  std::uint16_t key_arity = 0;
  std::int64_t key_int = 0;
  double key_float = 0;
};

struct Predicate {
  std::uint32_t name = 0;
  std::uint16_t arity = 0;
  std::vector<Clause> clauses;
  bool seen = false;
  // :- dynamic が付いた述語だけ、assert と retract の対象になる。
  bool dynamic = false;
};

enum class Pick { Clauses, ElseGoal, Negation, Once, Repeat, Catch, Cleanup, AtomGen };

struct Entry {
  Pick pick = Pick::Clauses;
  std::size_t heap_mark = 0;
  std::size_t trail_mark = 0;
  std::uint64_t redo_goal = kNoFrame;
  std::vector<std::size_t> candidates;
  std::size_t next = 0;
  std::uint32_t pred_name = 0;
  std::uint16_t pred_arity = 0;
  std::size_t cut = 0;
  IrTerm else_goal;
  std::vector<Cell> env;
  std::uint64_t cont = kNoFrame;
  int soft_id = 0;
  bool cond_ok = false;
  bool armed = false;
  std::size_t extras_mark = 0;
  // この選択点をやり直すとき、有効に戻す Catch のスタック位置。抜けたあとの選択点は空である。
  std::vector<std::uint32_t> live_catches;
  Cell pattern{};
  IrTerm recovery;
  // 節ではない組込のやり直し。1 は atom_concat、2 は sub_atom。next が次の候補。
  std::uint8_t gen_kind = 0;
};

enum class Ctrl { Interpret, Commit, SoftCommit, NegDone, OnceDone, CatchExit };

struct Extra {
  Ctrl ctrl = Ctrl::Interpret;
  IrTerm goal;
  IrTerm then;
  std::vector<Cell> env;
  std::uint64_t cont = kNoFrame;
  std::size_t cut = 0;
  std::size_t trim_to = 0;
  int soft_id = 0;
};

struct ExportedTerm {
  enum class Kind { Var, Atom, Int, Float, List, Str, Share };
  struct Node {
    Kind kind = Kind::Atom;
    std::uint32_t atom = 0;
    std::uint16_t arity = 0;
    std::int64_t integer = 0;
    double floating = 0;
    int share = -1;
    std::vector<int> kids;
  };
  std::vector<Node> nodes;
  int root = -1;
};

struct Machine::Impl {
  AtomTable atoms;
  Heap heap;
  OperatorTable operators;
  Trail trail;
  std::vector<Predicate> predicates;
  std::vector<Entry> stack;
  std::vector<Extra> extras;
  std::uint64_t ip = kNoFrame;
  bool failed = false;
  bool exception = false;
  // catch が評価の途中で例外を消しても、呼び出し元がヒープを読み続けないようにする。
  bool eval_abort = false;
  Cell ball{};
  bool halt_now = false;
  bool halt_pending = false;
  int halt_code = 0;
  std::string diagnostic;
  bool syntax_diag = false;
  std::size_t diag_line = 1;
  std::size_t diag_column = 1;
  std::vector<Binding> bindings;
  int next_soft = 1;
  DoubleQuotes quotes = DoubleQuotes::Codes;
  // unknown=fail のときだけ真。warning は未決 10 で受け付けない。
  bool unknown_fail = false;
  bool float_overflow_error = true;
  bool float_zero_div_error = true;
  bool float_undefined_error = true;

  explicit Impl(std::size_t limit) : heap(atoms, limit), operators(OperatorTable::iso_core()) {}
};

using Impl = Machine::Impl;

struct Goal {
  std::uint64_t at = 0;
  std::uint32_t name = 0;
  std::uint16_t arity = 0;
  std::size_t cut = 0;
  std::uint64_t cont = kNoFrame;

  [[nodiscard]] Cell arg(const Impl& machine, std::size_t index) const {
    return Cell{machine.heap.word_at(at + kFrameHead + index)};
  }
};

struct Compiled {
  IrTerm term;
  std::vector<Cell> env;
};

[[nodiscard]] Impl& engine_impl(Machine& machine);
[[nodiscard]] const Impl& engine_impl(const Machine& machine);

[[nodiscard]] Compiled compile_goal(const Heap& heap, Cell cell);
[[nodiscard]] bool compile_clause(const Heap& heap, Cell cell, Clause& clause);
[[nodiscard]] bool is_directive(const Heap& heap, Cell cell);

void launch(Impl& machine, const IrTerm& goal, std::uint64_t cont, std::size_t cut,
            const std::vector<Cell>& env);
void launch_or(Impl& machine, const IrTerm& goal, std::uint64_t cont, std::size_t cut,
               const std::vector<Cell>& env);
void launch_if_then(Impl& machine, const IrTerm& goal, std::uint64_t cont, std::size_t cut,
                    const std::vector<Cell>& env);
void launch_soft(Impl& machine, const IrTerm& goal, std::uint64_t cont, std::size_t cut,
                 const std::vector<Cell>& env);
void trim_stack(Impl& machine, std::size_t boundary);
[[nodiscard]] Cell materialize(Impl& machine, const IrTerm& term, const std::vector<Cell>& env);
[[nodiscard]] std::uint64_t embody(Impl& machine, const IrTerm& goal, std::uint64_t cont, std::size_t cut,
                                   const std::vector<Cell>& env);
[[nodiscard]] Goal read_goal(const Impl& machine, std::uint64_t at);
[[nodiscard]] bool do_unify(Impl& machine, Cell left, Cell right);
void enter_clause(Impl& machine, const Clause& clause, std::uint64_t goal, std::size_t cut);
void restore_state(Impl& machine, std::size_t heap_mark, std::size_t trail_mark, std::size_t extras_mark);
void note_catches(const Impl& machine, Entry& entry);
void apply_catches(Impl& machine, const Entry& entry);
void push_frame(Impl& machine, Entry entry);
[[nodiscard]] bool body_callable(const IrTerm& term);
[[nodiscard]] bool backtrack(Impl& machine);
[[nodiscard]] bool run_builtin(Impl& machine, const Goal& goal);
void enter_user(Impl& machine, const Goal& goal);
[[nodiscard]] ExportedTerm export_term(Impl& machine, Cell cell);
[[nodiscard]] Cell import_term(Impl& machine, const ExportedTerm& term);
void propagate_throw(Impl& machine, Cell ball);
void raise_formal(Impl& machine, Cell formal, std::uint32_t name, std::uint16_t arity);
[[nodiscard]] Cell cell_functor(Impl& machine, std::uint32_t name, std::uint16_t arity);
[[nodiscard]] std::vector<std::size_t> select_clauses(const Impl& machine, const Predicate& predicate,
                                                      bool has_arg, Cell first);

[[nodiscard]] Stop run_loop(Impl& machine);
void arm_catch(Impl& machine, const IrTerm& goal, std::uint64_t cont, std::size_t cut,
               const std::vector<Cell>& env);
void arm_throw(Impl& machine, const IrTerm& goal, const std::vector<Cell>& env);
void arm_halt(Impl& machine, const IrTerm& goal, const std::vector<Cell>& env);
[[nodiscard]] Cell formal_instantiation(Impl& machine);
[[nodiscard]] Cell formal_type(Impl& machine, std::string_view type, Cell culprit);
[[nodiscard]] Cell formal_domain(Impl& machine, std::string_view domain, Cell culprit);
[[nodiscard]] Cell formal_existence(Impl& machine, std::string_view kind, Cell culprit);
[[nodiscard]] Cell formal_evaluation(Impl& machine, std::string_view which);
[[nodiscard]] Cell formal_representation(Impl& machine, std::string_view which);
[[nodiscard]] Cell formal_syntax(Impl& machine, Cell detail);
[[nodiscard]] bool run_types(Impl& machine, const Goal& goal);
[[nodiscard]] bool run_order(Impl& machine, const Goal& goal);
[[nodiscard]] bool run_terms(Impl& machine, const Goal& goal);
[[nodiscard]] bool run_control(Impl& machine, const Goal& goal);
[[nodiscard]] bool run_arith(Impl& machine, const Goal& goal);
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
[[nodiscard]] bool run_database(Impl& machine, const Goal& goal);
[[nodiscard]] bool redo_retract(Impl& machine);
[[nodiscard]] int apply_dynamic_directive(Impl& machine, Cell directive);
[[nodiscard]] bool redo_flag_gen(Impl& machine);
[[nodiscard]] DoubleQuotes current_double_quotes(const Impl& machine);
[[nodiscard]] int apply_flag_directive(Impl& machine, Cell directive);
[[nodiscard]] Cell formal_permission(Impl& machine, std::string_view action, std::string_view type,
                                     Cell culprit);
[[nodiscard]] LoadStatus load_source(Impl& machine, std::string_view source);
[[nodiscard]] bool run_consult(Impl& machine, const Goal& goal);

}
