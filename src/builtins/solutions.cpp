// 8.10.1 findall/3 (+term, +callable_term, ?list)
// findall/4 (+term, +callable_term, ?list, ?list) は非 ISO の差分リスト
// 8.10.2 bagof/3 (+term, +callable_term, ?list)
// 8.10.3 setof/3 (+term, +callable_term, ?list)
//   第1〜3引数の誤りは findall/3 と同じ。instantiation_error、
//   type_error(callable, Goal)、type_error(list, List)
//   第4引数が部分リストでもリストでもなければ type_error(list, Tail)
// テスト: tests/pl/solutions.pl

#include "../machine/internal.hpp"
#include "solutions_internal.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

namespace mosa {
using sol::CopyKind;
using sol::CopyNode;
using sol::Finish;
using sol::Group;
using sol::Image;
using sol::Packet;

namespace {

enum class Which { None, Findall, Bagof, Setof };

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

// 呼び出し前に未束縛だった証人。解の途中で別のセルへ結び付いても、この番地が同一性である。
struct Home {
  std::size_t index = 0;
  std::uint64_t raw = 0;
  bool live = false;
};

// 呼び出し前の自由変数は解をまたいで同じ番号。存在量化と、ゴールが作った変数は解ごとに 0 から振り直す。
struct VarIds {
  std::size_t base = 0;
  int stable_next = 0;
  int local_next = 0;
  const std::vector<Cell>* hidden = nullptr;
  std::vector<std::pair<std::size_t, int>> stable;
  std::vector<std::pair<std::size_t, int>> local;
  std::vector<std::uint64_t> ext_raw;
  std::vector<Home> homes;
  // 証人でない未束縛セルを、最初にそこへ結び付いた証人の番号へ戻す。
  std::vector<std::pair<std::uint64_t, int>> alias;

  void reset_local() {
    local.clear();
    local_next = 0;
  }

  int outside(std::uint64_t raw) const {
    for (const auto& item : alias) {
      if (item.first == raw) return item.second;
    }
    return -1;
  }

  // 存在量化した変数を外部のまま残すと、自由変数の同じ具体化が解ごとに分かれる。
  int id_of(std::size_t index, std::uint64_t raw, bool as_local) {
    const bool stable_cell = !as_local && index < base;
    std::vector<std::pair<std::size_t, int>>& table = stable_cell ? stable : local;
    int& counter = stable_cell ? stable_next : local_next;
    for (const auto& item : table) {
      if (item.first == index) return item.second;
    }
    const int id = counter++;
    table.emplace_back(index, id);
    if (stable_cell) ext_raw.push_back(raw);
    return id;
  }
};

Which which_solution(std::string_view name, std::uint16_t arity) {
  // bagof/4 と setof/4 は差分リストではない。開いた末尾は findall だけ。
  if (name == "findall" && (arity == 3 || arity == 4)) return Which::Findall;
  if (arity != 3) return Which::None;
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

bool as_local(const VarIds& ids, std::size_t index, Cell cell) {
  if (index >= ids.base) return true;
  return ids.hidden != nullptr && listed(*ids.hidden, cell);
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

// 変数の末尾は差分リストの本体である。束縛してから拒むと、収集の副作用が残る。
bool bad_tail(Impl& machine, const Goal& goal) {
  if (goal.arity != 4) return false;
  const Cell cell = machine.heap.deref(goal.arg(machine, 3));
  if (walk_list(machine, cell).shape != ListShape::Bad) return false;
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

int kept_id(VarIds& ids, int home) {
  const Home& kept = ids.homes[static_cast<std::size_t>(home)];
  return ids.id_of(kept.index, kept.raw, false);
}

bool home_cell(const VarIds& ids, std::uint64_t raw) {
  for (const Home& home : ids.homes) {
    if (home.live && home.raw == raw) return true;
  }
  return false;
}

void note_homes(const Impl& machine, const std::vector<Cell>& witnesses, VarIds& ids) {
  ids.homes.clear();
  for (const Cell& var : witnesses) {
    Home home;
    const Cell value = machine.heap.deref(var);
    if (machine.heap.unbound(value)) {
      home.live = true;
      home.index = heap_index(machine, value);
      home.raw = value.raw();
    }
    ids.homes.push_back(home);
  }
}

// (=)/2 は対称である。自由変数がテンプレート側へ結び付いても、具体化 W は証人のセルで数える。
void note_alias(const Impl& machine, VarIds& ids) {
  ids.alias.clear();
  for (int index = 0; index < static_cast<int>(ids.homes.size()); ++index) {
    const Home& home = ids.homes[static_cast<std::size_t>(index)];
    if (!home.live) continue;
    const Cell value = machine.heap.deref(Cell{home.raw});
    if (!machine.heap.unbound(value) || value.raw() == home.raw || home_cell(ids, value.raw())) continue;
    if (ids.outside(value.raw()) >= 0) continue;
    ids.alias.emplace_back(value.raw(), index);
  }
}

Cell encode(Impl& machine, Cell cell, VarIds& ids, std::vector<std::pair<std::uint64_t, Cell>>& seen,
            std::uint32_t external, std::uint32_t renaming) {
  cell = machine.heap.deref(cell);
  if (machine.heap.unbound(cell)) {
    if (const int home = ids.outside(cell.raw()); home >= 0) {
      return marker_for(machine, external, kept_id(ids, home));
    }
    const auto index = heap_index(machine, cell);
    const bool local = as_local(ids, index, cell);
    const auto name = local ? renaming : external;
    return marker_for(machine, name, ids.id_of(index, cell.raw(), local));
  }
  if (cell.tag() != Tag::Lis && cell.tag() != Tag::Str) return cell;
  for (const auto& item : seen) {
    if (item.first == cell.raw()) return item.second;
  }
  const Cell hole = machine.heap.fresh_ref();
  seen.emplace_back(cell.raw(), hole);
  Cell built;
  if (cell.tag() == Tag::Lis) {
    const Cell head = encode(machine, machine.heap.list_head(cell), ids, seen, external, renaming);
    const Cell tail = encode(machine, machine.heap.list_tail(cell), ids, seen, external, renaming);
    built = machine.heap.list(head, tail);
  } else {
    std::vector<Cell> args;
    const auto arity = machine.heap.functor_arity(cell);
    args.reserve(arity);
    for (std::uint16_t index = 0; index < arity; ++index) {
      args.push_back(encode(machine, machine.heap.arg(cell, index), ids, seen, external, renaming));
    }
    built = machine.heap.structure(machine.heap.functor_atom(cell), args);
  }
  machine.heap.bind(hole, built);
  return hole;
}

Cell coded_key(Impl& machine, const std::vector<Cell>& witnesses, VarIds& ids) {
  std::vector<Cell> key_items;
  std::vector<std::pair<std::uint64_t, Cell>> seen;
  const auto external = machine.atoms.intern("$witness");
  const auto renaming = machine.atoms.intern("$rename");
  for (const Cell& var : witnesses) {
    key_items.push_back(encode(machine, var, ids, seen, external, renaming));
  }
  return list_from_cells(machine, key_items);
}

int push_node(Image& image, CopyNode node) {
  const int at = static_cast<int>(image.nodes.size());
  image.nodes.push_back(std::move(node));
  return at;
}

int image_list(Image& image, std::uint32_t nil, const std::vector<int>& kids) {
  CopyNode empty;
  empty.kind = CopyKind::Atom;
  empty.atom = nil;
  int tail = push_node(image, std::move(empty));
  for (std::size_t index = kids.size(); index > 0; --index) {
    CopyNode node;
    node.kind = CopyKind::List;
    node.kids.push_back(kids[index - 1]);
    node.kids.push_back(tail);
    tail = push_node(image, std::move(node));
  }
  return tail;
}

int found_node(const std::vector<std::pair<std::uint64_t, int>>& seen, std::uint64_t raw) {
  for (const auto& item : seen) {
    if (item.first == raw) return item.second;
  }
  return -1;
}

int copy_scalar(Impl& machine, Cell cell, Image& image) {
  CopyNode node;
  if (cell.tag() == Tag::Atom) {
    node.kind = CopyKind::Atom;
    node.atom = static_cast<std::uint32_t>(cell.raw() >> 3);
  } else if (cell.tag() == Tag::Int || cell.tag() == Tag::IntBox) {
    node.kind = CopyKind::Int;
    node.integer = machine.heap.integer_value(cell);
  } else {
    node.kind = CopyKind::Float;
    node.floating = machine.heap.float_value(cell);
  }
  return push_node(image, std::move(node));
}

int copy_cell(Impl& machine, Cell cell, Image& image, std::vector<std::pair<std::uint64_t, int>>& seen, VarIds& ids) {
  cell = machine.heap.deref(cell);
  if (const int prior = found_node(seen, cell.raw()); prior >= 0 &&
                                                        (machine.heap.unbound(cell) || cell.tag() == Tag::Lis ||
                                                         cell.tag() == Tag::Str)) {
    return prior;
  }
  if (machine.heap.unbound(cell)) {
    if (const int home = ids.outside(cell.raw()); home >= 0) {
      CopyNode node;
      node.kind = CopyKind::Ext;
      node.id = kept_id(ids, home);
      const int at = push_node(image, std::move(node));
      seen.emplace_back(cell.raw(), at);
      return at;
    }
    CopyNode node;
    const auto index = heap_index(machine, cell);
    const bool local = as_local(ids, index, cell);
    node.kind = local ? CopyKind::Loc : CopyKind::Ext;
    node.id = ids.id_of(index, cell.raw(), local);
    const int at = push_node(image, std::move(node));
    seen.emplace_back(cell.raw(), at);
    return at;
  }
  if (cell.tag() != Tag::Lis && cell.tag() != Tag::Str) return copy_scalar(machine, cell, image);
  const int at = push_node(image, CopyNode{});
  seen.emplace_back(cell.raw(), at);
  CopyNode node;
  if (cell.tag() == Tag::Lis) {
    node.kind = CopyKind::List;
    node.kids.push_back(copy_cell(machine, machine.heap.list_head(cell), image, seen, ids));
    node.kids.push_back(copy_cell(machine, machine.heap.list_tail(cell), image, seen, ids));
  } else {
    node.kind = CopyKind::Str;
    node.atom = machine.heap.functor_atom(cell);
    node.arity = machine.heap.functor_arity(cell);
    for (std::uint16_t index = 0; index < node.arity; ++index) {
      node.kids.push_back(copy_cell(machine, machine.heap.arg(cell, index), image, seen, ids));
    }
  }
  image.nodes[static_cast<std::size_t>(at)] = std::move(node);
  return at;
}

Image snapshot(Impl& machine, const std::vector<Cell>& witnesses, Cell templ, VarIds& ids) {
  Image image;
  std::vector<std::pair<std::uint64_t, int>> seen;
  std::vector<int> kids;
  kids.reserve(witnesses.size());
  for (const Cell& var : witnesses) kids.push_back(copy_cell(machine, var, image, seen, ids));
  image.witness = image_list(image, machine.atoms.intern("[]"), kids);
  image.templ = copy_cell(machine, templ, image, seen, ids);
  return image;
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
              std::vector<Group>& groups, bool share) {
  ids.reset_local();
  note_alias(machine, ids);
  const ExportedTerm key_copy = export_term(machine, coded_key(machine, witnesses, ids));
  ExportedTerm templ_copy;
  Image image;
  if (share) image = snapshot(machine, witnesses, templ, ids);
  else templ_copy = export_term(machine, templ);
  for (Group& group : groups) {
    if (!same_term(machine, group.key, key_copy)) continue;
    if (share) group.shots.push_back(std::move(image));
    else group.templs.push_back(templ_copy);
    return;
  }
  Group created;
  created.key = key_copy;
  if (share) created.shots.push_back(std::move(image));
  else created.templs.push_back(std::move(templ_copy));
  groups.push_back(std::move(created));
}

void disarm_below(Impl& machine, std::size_t floor) {
  for (std::size_t index = 0; index < floor; ++index) {
    if (machine.stack[index].pick == Pick::Catch) machine.stack[index].armed = false;
  }
}

void rearm_below(Impl& machine, const Saved& saved, std::size_t floor) {
  std::size_t limit = floor;
  if (limit > machine.stack.size()) limit = machine.stack.size();
  if (limit > saved.stack.size()) limit = saved.stack.size();
  for (std::size_t index = 0; index < limit; ++index) {
    if (saved.stack[index].pick == Pick::Catch && saved.stack[index].armed) {
      machine.stack[index].armed = true;
    }
  }
}

std::size_t park_callers(Impl& machine, const Saved& saved) {
  const auto floor = machine.stack.size();
  disarm_below(machine, floor);
  Entry barrier;
  barrier.pick = Pick::Barrier;
  barrier.heap_mark = saved.heap_mark;
  barrier.trail_mark = saved.trail_mark;
  push_frame(machine, std::move(barrier));
  return floor;
}

void release_callers(Impl& machine, const Saved& saved, std::size_t floor) {
  restore_state(machine, saved.heap_mark, saved.trail_mark, saved.extras_mark);
  if (!machine.stack.empty() && machine.stack.back().pick == Pick::Barrier) machine.stack.pop_back();
  rearm_below(machine, saved, floor);
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

// 床より下はスナップショットで覆さない。asserta が直した節の添字を残すため。
void unwind_throw(Impl& machine, const Saved& saved, std::size_t floor) {
  if (machine.stack.size() > floor) machine.stack.resize(floor);
  restore_state(machine, saved.heap_mark, saved.trail_mark, saved.extras_mark);
  machine.ip = saved.ip;
  machine.failed = saved.failed;
  machine.exception = saved.exception;
  machine.eval_abort = saved.eval_abort;
  machine.ball = saved.ball;
  rearm_below(machine, saved, floor);
}

// 外側の節走査はスタックに残す。asserta と retract が添字を更新できるようにするため。
// 床より下の catch は外し、収集中に外側の回復ゴールへ落ちないようにする。
Packet collect(Impl& machine, const Compiled& compiled, Cell templ, const std::vector<Cell>& witnesses,
               const std::vector<Cell>& hidden, bool share) {
  const Saved saved = save_exec(machine);
  const auto floor = park_callers(machine, saved);
  machine.failed = false;
  machine.exception = false;
  machine.eval_abort = false;
  VarIds ids;
  ids.base = saved.heap_mark;
  ids.hidden = &hidden;
  note_homes(machine, witnesses, ids);
  launch(machine, compiled.term, kNoFrame, machine.stack.size(), compiled.env);
  Packet packet;
  while (true) {
    const Stop stop = run_loop(machine);
    if (stop.kind == StopKind::Exception) {
      packet.end = Finish::Threw;
      packet.ball = export_term(machine, machine.ball);
      unwind_throw(machine, saved, floor);
      return packet;
    }
    if (stop.kind == StopKind::Halt) {
      packet.end = Finish::Halted;
      packet.halt_code = machine.halt_code;
      restore_exec(machine, saved);
      return packet;
    }
    if (stop.kind != StopKind::Success) break;
    remember(machine, templ, witnesses, ids, packet.groups, share);
    if (!backtrack(machine)) break;
  }
  release_callers(machine, saved, floor);
  packet.ext_raw = std::move(ids.ext_raw);
  return packet;
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
  if (bad_tail(machine, goal)) return true;
  std::vector<Cell> witnesses;
  if (which != Which::Findall) witnesses = free_witnesses(machine, runnable, goal.arg(machine, 0), hidden);
  const bool share = which != Which::Findall;
  const Packet packet = collect(machine, compiled, goal.arg(machine, 0), witnesses, hidden, share);
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
  if (which == Which::Findall) return sol::bind_found(machine, goal, packet);
  const bool sorting = which == Which::Setof;
  if (packet.groups.empty()) {
    machine.failed = true;
    return true;
  }
  sol::bind_answers(machine, goal, witnesses,
                    sol::answers_from(machine, packet.groups, sorting, packet.ext_raw));
  return true;
}

}  // namespace mosa
