#pragma once

// 機械の非公開状態。ゴール枠の語 0 が 0 のときは制御枠で、述語枠ではない。

#include "mosa/ir.hpp"
#include "mosa/machine.hpp"

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
};

enum class Pick { Clauses, ElseGoal, Negation, Once, Repeat, Catch, Cleanup };

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
  Cell pattern{};
  IrTerm recovery;
};

enum class Ctrl { Interpret, Commit, SoftCommit, NegDone, OnceDone };

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
void restore_state(Impl& machine, std::size_t heap_mark, std::size_t trail_mark);
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
[[nodiscard]] bool run_types(Impl& machine, const Goal& goal);
[[nodiscard]] bool run_order(Impl& machine, const Goal& goal);
[[nodiscard]] bool run_terms(Impl& machine, const Goal& goal);
[[nodiscard]] bool run_control(Impl& machine, const Goal& goal);
[[nodiscard]] bool run_arith(Impl& machine, const Goal& goal);
[[nodiscard]] LoadStatus load_source(Impl& machine, std::string_view source);
[[nodiscard]] bool run_consult(Impl& machine, const Goal& goal);

}
