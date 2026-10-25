// 8.10.1 findall/3 (+term, +callable_term, ?list)
// 8.10.2 bagof/3 (+term, +callable_term, ?list)
// 8.10.3 setof/3 (+term, +callable_term, ?list)
//   instantiation_error、type_error(callable, Goal)、type_error(list, List)
//   findall/4 は未決 3 のため登録しない
// テスト: tests/pl/solutions.pl

#include "../machine/internal.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

namespace mosa {
namespace {

enum class Which { None, Findall, Bagof, Setof };

enum class Finish { Ready, Threw, Halted };

struct Group {
  ExportedTerm key;
  ExportedTerm witness;
  std::vector<ExportedTerm> templs;
};

struct Packet {
  Finish end = Finish::Ready;
  std::vector<Group> groups;
  ExportedTerm ball;
  int halt_code = 0;
};

struct Answer {
  Cell witness{};
  std::vector<Cell> values;
  Cell list{};
};

struct Saved {
  std::vector<Entry> stack;
  std::uint64_t ip = kNoFrame;
  bool failed = false;
  bool exception = false;
  bool eval_abort = false;
  Cell ball{};
  std::size_t heap_mark = 0;
  std::size_t trail_mark = 0;
  std::size_t extras_mark = 0;
};

// 収集開始より前の自由変数だけ解をまたいで同じ鍵にする。
// ゴールが後から作った未束縛変数は、後戻りで同じ番地が別物になる。
struct VarIds {
  std::size_t base = 0;
  int next = 0;
  std::vector<std::pair<std::size_t, int>> stable;
  std::vector<std::pair<std::size_t, int>> local;

  void reset_local() { local.clear(); }

  int id_of(std::size_t index) {
    std::vector<std::pair<std::size_t, int>>& table = index < base ? stable : local;
    for (const auto& item : table) {
      if (item.first == index) return item.second;
    }
    const int id = next++;
    table.emplace_back(index, id);
    return id;
  }
};

Which which_solution(std::string_view name, std::uint16_t arity) {
  if (arity != 3) return Which::None;
  if (name == "findall") return Which::Findall;
  if (name == "bagof") return Which::Bagof;
  if (name == "setof") return Which::Setof;
  return Which::None;
}

bool listed(const std::vector<Cell>& vars, Cell cell) {
  for (const Cell& var : vars) {
    if (var.raw() == cell.raw()) return true;
  }
  return false;
}

void collect_free(const Impl& machine, Cell term, std::vector<Cell>& vars, std::vector<std::uint64_t>& seen) {
  if (term.tag() == Tag::Ref) {
    const Cell value = machine.heap.deref(term);
    if (machine.heap.unbound(value)) {
      if (!listed(vars, value)) vars.push_back(value);
      return;
    }
    term = value;
  } else {
    term = machine.heap.deref(term);
  }
  if (machine.heap.unbound(term) || (term.tag() != Tag::Lis && term.tag() != Tag::Str)) return;
  for (const auto raw : seen) {
    if (raw == term.raw()) return;
  }
  seen.push_back(term.raw());
  if (term.tag() == Tag::Lis) {
    collect_free(machine, machine.heap.list_head(term), vars, seen);
    collect_free(machine, machine.heap.list_tail(term), vars, seen);
    return;
  }
  for (std::uint16_t index = 0; index < machine.heap.functor_arity(term); ++index) {
    collect_free(machine, machine.heap.arg(term, index), vars, seen);
  }
}

bool is_caret(const Impl& machine, Cell cell) {
  cell = machine.heap.deref(cell);
  if (cell.tag() != Tag::Str || machine.heap.functor_arity(cell) != 2) return false;
  return machine.atoms.name(machine.heap.functor_atom(cell)) == "^";
}

// 先頭に並んだ ^/2 だけを外す。中のゴールに埋まった ^ は量化ではない。
Cell strip_caret(Impl& machine, Cell goal, std::vector<Cell>& hidden) {
  goal = machine.heap.deref(goal);
  while (is_caret(machine, goal)) {
    std::vector<std::uint64_t> seen;
    collect_free(machine, machine.heap.arg(goal, 0), hidden, seen);
    goal = machine.heap.deref(machine.heap.arg(goal, 1));
  }
  return goal;
}

std::vector<Cell> witness_vars(const std::vector<Cell>& goal_vars, const std::vector<Cell>& templ,
                               const std::vector<Cell>& hidden) {
  std::vector<Cell> out;
  for (const Cell& var : goal_vars) {
    if (listed(templ, var) || listed(hidden, var)) continue;
    out.push_back(var);
  }
  return out;
}

std::vector<Cell> free_witnesses(Impl& machine, Cell goal, Cell templ, const std::vector<Cell>& hidden) {
  std::vector<Cell> goal_vars;
  std::vector<Cell> templ_vars;
  std::vector<std::uint64_t> seen_goal;
  std::vector<std::uint64_t> seen_templ;
  collect_free(machine, goal, goal_vars, seen_goal);
  collect_free(machine, templ, templ_vars, seen_templ);
  return witness_vars(goal_vars, templ_vars, hidden);
}

bool goal_shape_ok(Impl& machine, const Goal& goal, Cell cell) {
  cell = machine.heap.deref(cell);
  if (machine.heap.unbound(cell)) {
    raise_formal(machine, formal_instantiation(machine), goal.name, goal.arity);
    return false;
  }
  if (cell.tag() == Tag::Atom || cell.tag() == Tag::Str || cell.tag() == Tag::Lis) return true;
  raise_formal(machine, formal_type(machine, "callable", cell), goal.name, goal.arity);
  return false;
}

bool goal_body_ok(Impl& machine, const Goal& goal, Cell cell, Compiled& compiled) {
  if (!goal_shape_ok(machine, goal, cell)) return false;
  compiled = compile_goal(machine.heap, cell);
  if (body_callable(compiled.term)) return true;
  raise_formal(machine, formal_type(machine, "callable", cell), goal.name, goal.arity);
  return false;
}

// 部分リストは 8.10 の type_error(list) に含まれない。
bool bad_list(Impl& machine, const Goal& goal) {
  const Cell cell = machine.heap.deref(goal.arg(machine, 2));
  const WalkedList walked = walk_list(machine, cell);
  if (walked.shape != ListShape::Bad) return false;
  raise_formal(machine, formal_type(machine, "list", cell), goal.name, goal.arity);
  return true;
}

std::size_t heap_index(const Impl& machine, Cell cell) {
  return static_cast<std::size_t>(machine.heap.word(cell) - machine.heap.data());
}

Cell marker_for(Impl& machine, std::uint32_t name, int id) {
  const std::array<Cell, 1> args{machine.heap.integer(id)};
  return machine.heap.structure(name, args);
}

Cell encode(Impl& machine, Cell cell, VarIds& ids, std::vector<std::pair<std::uint64_t, Cell>>& seen,
            std::uint32_t marker) {
  cell = machine.heap.deref(cell);
  if (machine.heap.unbound(cell)) return marker_for(machine, marker, ids.id_of(heap_index(machine, cell)));
  if (cell.tag() != Tag::Lis && cell.tag() != Tag::Str) return cell;
  for (const auto& item : seen) {
    if (item.first == cell.raw()) return item.second;
  }
  const Cell hole = machine.heap.fresh_ref();
  seen.emplace_back(cell.raw(), hole);
  Cell built;
  if (cell.tag() == Tag::Lis) {
    const Cell head = encode(machine, machine.heap.list_head(cell), ids, seen, marker);
    const Cell tail = encode(machine, machine.heap.list_tail(cell), ids, seen, marker);
    built = machine.heap.list(head, tail);
  } else {
    std::vector<Cell> args;
    const auto arity = machine.heap.functor_arity(cell);
    args.reserve(arity);
    for (std::uint16_t index = 0; index < arity; ++index) {
      args.push_back(encode(machine, machine.heap.arg(cell, index), ids, seen, marker));
    }
    built = machine.heap.structure(machine.heap.functor_atom(cell), args);
  }
  machine.heap.bind(hole, built);
  return hole;
}

struct Shot {
  Cell raw{};
  Cell key{};
};

Shot shoot(Impl& machine, const std::vector<Cell>& witnesses, VarIds& ids) {
  ids.reset_local();
  std::vector<Cell> raw_items;
  std::vector<Cell> key_items;
  std::vector<std::pair<std::uint64_t, Cell>> seen;
  const auto marker = machine.atoms.intern("$witness");
  for (const Cell& var : witnesses) {
    const Cell value = machine.heap.deref(var);
    raw_items.push_back(value);
    key_items.push_back(encode(machine, value, ids, seen, marker));
  }
  Shot shot;
  shot.raw = list_from_cells(machine, raw_items);
  shot.key = list_from_cells(machine, key_items);
  return shot;
}

bool same_term(Impl& machine, const ExportedTerm& left, const ExportedTerm& right) {
  const auto heap_mark = machine.heap.size();
  const auto trail_mark = machine.trail.slots.size();
  const auto extras_mark = machine.extras.size();
  const Cell a = import_term(machine, left);
  const Cell b = import_term(machine, right);
  const int order = compare_cells(machine, a, b);
  restore_state(machine, heap_mark, trail_mark, extras_mark);
  return order == 0;
}

void remember(Impl& machine, Cell templ, const std::vector<Cell>& witnesses, VarIds& ids,
              std::vector<Group>& groups) {
  const Shot shot = shoot(machine, witnesses, ids);
  const ExportedTerm raw_copy = export_term(machine, shot.raw);
  const ExportedTerm key_copy = export_term(machine, shot.key);
  const ExportedTerm templ_copy = export_term(machine, templ);
  for (Group& group : groups) {
    if (!same_term(machine, group.key, key_copy)) continue;
    group.templs.push_back(templ_copy);
    return;
  }
  Group created;
  created.key = key_copy;
  created.witness = raw_copy;
  created.templs.push_back(templ_copy);
  groups.push_back(std::move(created));
}

Saved save_exec(const Impl& machine) {
  Saved saved;
  saved.stack = machine.stack;
  saved.ip = machine.ip;
  saved.failed = machine.failed;
  saved.exception = machine.exception;
  saved.eval_abort = machine.eval_abort;
  saved.ball = machine.ball;
  saved.heap_mark = machine.heap.size();
  saved.trail_mark = machine.trail.slots.size();
  saved.extras_mark = machine.extras.size();
  return saved;
}

void restore_exec(Impl& machine, const Saved& saved) {
  restore_state(machine, saved.heap_mark, saved.trail_mark, saved.extras_mark);
  machine.stack = saved.stack;
  machine.ip = saved.ip;
  machine.failed = saved.failed;
  machine.exception = saved.exception;
  machine.eval_abort = saved.eval_abort;
  machine.ball = saved.ball;
}

// 外側の選択点を外す。ゴールの後戻りやカットが、呼び出し元の選択点を消費しないため。
// 空のスタックは水位がヒープ先頭になり、古い変数への束縛がトレイルに残らない。
// Once はマークまで戻して捨てるだけなので、最後の解の束縛を消す床に使う。
Packet collect(Impl& machine, const Compiled& compiled, Cell templ, const std::vector<Cell>& witnesses) {
  const Saved saved = save_exec(machine);
  machine.stack.clear();
  machine.failed = false;
  machine.exception = false;
  machine.eval_abort = false;
  Entry floor;
  floor.pick = Pick::Once;
  floor.heap_mark = saved.heap_mark;
  floor.trail_mark = saved.trail_mark;
  push_frame(machine, std::move(floor));
  VarIds ids;
  ids.base = saved.heap_mark;
  launch(machine, compiled.term, kNoFrame, machine.stack.size(), compiled.env);
  Packet packet;
  while (true) {
    const Stop stop = run_loop(machine);
    if (stop.kind == StopKind::Exception) {
      packet.end = Finish::Threw;
      packet.ball = export_term(machine, machine.ball);
      break;
    }
    if (stop.kind == StopKind::Halt) {
      packet.end = Finish::Halted;
      packet.halt_code = machine.halt_code;
      break;
    }
    if (stop.kind != StopKind::Success) break;
    // 解の時点で複製する。あとから変数を束縛しても、集めた項は変わらない。
    remember(machine, templ, witnesses, ids, packet.groups);
    if (!backtrack(machine)) break;
  }
  restore_exec(machine, saved);
  return packet;
}

void sort_cells(Impl& machine, std::vector<Cell>& items) {
  for (std::size_t index = 1; index < items.size(); ++index) {
    const Cell current = items[index];
    std::size_t hole = index;
    while (hole > 0 && compare_cells(machine, items[hole - 1], current) > 0) {
      items[hole] = items[hole - 1];
      --hole;
    }
    items[hole] = current;
  }
}

void unique_cells(Impl& machine, std::vector<Cell>& items) {
  std::vector<Cell> kept;
  for (const Cell& item : items) {
    if (!kept.empty() && compare_cells(machine, kept.back(), item) == 0) continue;
    kept.push_back(item);
  }
  items = std::move(kept);
}

void sort_answers(Impl& machine, std::vector<Answer>& answers) {
  for (std::size_t index = 1; index < answers.size(); ++index) {
    Answer current = std::move(answers[index]);
    std::size_t hole = index;
    while (hole > 0 && compare_cells(machine, answers[hole - 1].witness, current.witness) > 0) {
      answers[hole] = std::move(answers[hole - 1]);
      --hole;
    }
    answers[hole] = std::move(current);
  }
}

Answer make_answer(Impl& machine, const Group& group, bool sorting) {
  Answer answer;
  answer.witness = import_term(machine, group.witness);
  answer.values = walk_list(machine, answer.witness).items;
  std::vector<Cell> templs;
  templs.reserve(group.templs.size());
  for (const ExportedTerm& term : group.templs) templs.push_back(import_term(machine, term));
  if (sorting) {
    sort_cells(machine, templs);
    unique_cells(machine, templs);
  }
  answer.list = list_from_cells(machine, templs);
  return answer;
}

std::vector<Answer> answers_from(Impl& machine, const std::vector<Group>& groups, bool sorting) {
  std::vector<Answer> answers;
  answers.reserve(groups.size());
  for (const Group& group : groups) answers.push_back(make_answer(machine, group, sorting));
  if (sorting) sort_answers(machine, answers);
  return answers;
}

Answer blank_answer(Impl& machine) {
  Answer answer;
  answer.list = list_from_cells(machine, {});
  answer.witness = answer.list;
  return answer;
}

IrTerm slot_var(std::uint16_t slot) {
  IrTerm term;
  term.tag = IrTag::Var;
  term.slot = slot;
  return term;
}

IrTerm eq_term(std::uint32_t name, std::uint16_t left, std::uint16_t right) {
  IrTerm term;
  term.tag = IrTag::Fun;
  term.atom = name;
  term.arity = 2;
  term.args.push_back(slot_var(left));
  term.args.push_back(slot_var(right));
  return term;
}

IrTerm fold(IrTag tag, const std::vector<IrTerm>& parts) {
  IrTerm tree = parts.back();
  for (std::size_t index = parts.size(); index > 1; --index) {
    IrTerm parent;
    parent.tag = tag;
    parent.args.push_back(parts[index - 2]);
    parent.args.push_back(std::move(tree));
    tree = std::move(parent);
  }
  return tree;
}

void bind_answers(Impl& machine, const Goal& goal, const std::vector<Cell>& witnesses,
                  const std::vector<Answer>& answers) {
  const auto eq = machine.atoms.intern("=");
  std::vector<Cell> env;
  env.push_back(goal.arg(machine, 2));
  for (const Cell& var : witnesses) env.push_back(var);
  std::vector<IrTerm> branches;
  branches.reserve(answers.size());
  for (const Answer& answer : answers) {
    std::vector<IrTerm> parts;
    const auto list_slot = static_cast<std::uint16_t>(env.size());
    env.push_back(answer.list);
    for (std::size_t index = 0; index < witnesses.size(); ++index) {
      const auto slot = static_cast<std::uint16_t>(env.size());
      env.push_back(answer.values[index]);
      parts.push_back(eq_term(eq, static_cast<std::uint16_t>(index + 1), slot));
    }
    parts.push_back(eq_term(eq, 0, list_slot));
    branches.push_back(fold(IrTag::And, parts));
  }
  // 束縛は収集が例外なく終わったあと。残りの証人は選択点で返す。
  launch(machine, fold(IrTag::Or, branches), goal.cont, machine.stack.size(), env);
}

}  // namespace

bool run_solutions(Impl& machine, const Goal& goal) {
  const Which which = which_solution(machine.atoms.name(goal.name), goal.arity);
  if (which == Which::None) return false;
  const Cell given = machine.heap.deref(goal.arg(machine, 1));
  if (which != Which::Findall && !goal_shape_ok(machine, goal, given)) return true;
  std::vector<Cell> hidden;
  Cell runnable = given;
  if (which != Which::Findall) runnable = strip_caret(machine, given, hidden);
  Compiled compiled;
  if (!goal_body_ok(machine, goal, runnable, compiled)) return true;
  if (bad_list(machine, goal)) return true;
  std::vector<Cell> witnesses;
  if (which != Which::Findall) witnesses = free_witnesses(machine, runnable, goal.arg(machine, 0), hidden);
  const Packet packet = collect(machine, compiled, goal.arg(machine, 0), witnesses);
  if (packet.end == Finish::Threw) {
    propagate_throw(machine, import_term(machine, packet.ball));
    return true;
  }
  if (packet.end == Finish::Halted) {
    machine.halt_now = true;
    machine.halt_code = packet.halt_code;
    machine.ip = kNoFrame;
    return true;
  }
  const bool sorting = which == Which::Setof;
  if (packet.groups.empty()) {
    if (which != Which::Findall) {
      machine.failed = true;
      return true;
    }
    bind_answers(machine, goal, witnesses, {blank_answer(machine)});
    return true;
  }
  bind_answers(machine, goal, witnesses, answers_from(machine, packet.groups, sorting));
  return true;
}

}  // namespace mosa
