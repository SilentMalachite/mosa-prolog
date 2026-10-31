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
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace mosa {
using sol::CopyKind;
using sol::CopyNode;
using sol::Group;
using sol::Image;
using sol::Packet;

namespace {

enum class Which { None, Findall, Bagof, Setof };

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

// 1 つの項を見る。変数なら vars に足し、まだ見ていない複合項なら子を右から pending に積む。右から積むので、
// 取り出す順は前の再帰と同じ左からの前順になり、vars の並びも変わらない。
void free_one(const Impl& machine, Cell term, std::vector<Cell>& vars, std::unordered_set<std::uint64_t>& seen,
              std::vector<Cell>& pending) {
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
  if (!seen.insert(term.raw()).second) return;
  if (term.tag() == Tag::Lis) {
    pending.push_back(machine.heap.list_tail(term));
    pending.push_back(machine.heap.list_head(term));
    return;
  }
  for (std::uint16_t index = machine.heap.functor_arity(term); index > 0; --index) {
    pending.push_back(machine.heap.arg(term, static_cast<std::uint16_t>(index - 1)));
  }
}

// 深い項でも C++ のスタックを使わないよう、まだ見ていない項を手で積む（ADR 0011）。
void collect_free(const Impl& machine, Cell term, std::vector<Cell>& vars, std::unordered_set<std::uint64_t>& seen) {
  std::vector<Cell> pending{term};
  while (!pending.empty()) {
    const Cell next = pending.back();
    pending.pop_back();
    free_one(machine, next, vars, seen, pending);
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
    std::unordered_set<std::uint64_t> seen;
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
  std::unordered_set<std::uint64_t> seen_goal;
  std::unordered_set<std::uint64_t> seen_templ;
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
  if (body_callable(*compiled.term)) return true;
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

// 鍵に写す途中の複合項 1 つ分。穴を先に作って seen に置き、子をすべて写してから本体を作って穴に束縛する。
struct EncodeFrame {
  Cell hole;
  Cell cell;
  std::uint16_t next;
  std::uint16_t arity;
  std::vector<Cell> args;
};

struct Encoder {
  Impl& machine;
  VarIds& ids;
  std::unordered_map<std::uint64_t, Cell>& seen;
  std::uint32_t external;
  std::uint32_t renaming;
  std::vector<EncodeFrame> frames;

  // 葉はその場で写し、まだ見ていない複合項は穴を返して frames に積む。
  Cell start(Cell cell) {
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
    if (const auto found = seen.find(cell.raw()); found != seen.end()) return found->second;
    const Cell hole = machine.heap.fresh_ref();
    seen.emplace(cell.raw(), hole);
    const std::uint16_t arity = cell.tag() == Tag::Lis ? 2 : machine.heap.functor_arity(cell);
    std::vector<Cell> args;
    args.reserve(arity);
    frames.push_back(EncodeFrame{hole, cell, 0, arity, std::move(args)});
    return hole;
  }

  Cell child(const EncodeFrame& frame) const {
    if (frame.cell.tag() == Tag::Lis) {
      return frame.next == 0 ? machine.heap.list_head(frame.cell) : machine.heap.list_tail(frame.cell);
    }
    return machine.heap.arg(frame.cell, frame.next);
  }

  // 前の再帰と同じ順にヒープを割り当てる。子を 1 つ写し終えるまで次の兄弟へ進まない（ADR 0011）。
  Cell run(Cell cell) {
    const Cell result = start(cell);
    while (!frames.empty()) {
      EncodeFrame& top = frames.back();
      if (top.next == top.arity) {
        const Cell built = top.cell.tag() == Tag::Lis
                               ? machine.heap.list(top.args[0], top.args[1])
                               : machine.heap.structure(machine.heap.functor_atom(top.cell), top.args);
        machine.heap.bind(top.hole, built);
        frames.pop_back();
        continue;
      }
      const Cell next = child(top);
      ++top.next;
      // start が frames を伸ばすと top は無効になるので、子の値は伸ばす前の位置へ添字で書く。
      const std::size_t at = frames.size() - 1;
      const Cell value = start(next);
      frames[at].args.push_back(value);
    }
    return result;
  }
};

Cell coded_key(Impl& machine, const std::vector<Cell>& witnesses, VarIds& ids) {
  std::vector<Cell> key_items;
  std::unordered_map<std::uint64_t, Cell> seen;
  Encoder encoder{machine, ids, seen, machine.atoms.intern("$witness"), machine.atoms.intern("$rename"), {}};
  for (const Cell& var : witnesses) key_items.push_back(encoder.run(var));
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

using NodeSeen = std::unordered_map<std::uint64_t, int>;

// 像に写す途中の複合項 1 つ分。kids は image.nodes[id] に直に足す。nodes は伸びるので添字で持つ。
struct CopyFrame {
  int id;
  Cell cell;
  std::uint16_t next;
  std::uint16_t arity;
};

int copy_var(Impl& machine, Cell cell, Image& image, NodeSeen& seen, VarIds& ids) {
  CopyNode node;
  if (const int home = ids.outside(cell.raw()); home >= 0) {
    node.kind = CopyKind::Ext;
    node.id = kept_id(ids, home);
  } else {
    const auto index = heap_index(machine, cell);
    const bool local = as_local(ids, index, cell);
    node.kind = local ? CopyKind::Loc : CopyKind::Ext;
    node.id = ids.id_of(index, cell.raw(), local);
  }
  const int at = push_node(image, std::move(node));
  seen.emplace(cell.raw(), at);
  return at;
}

// 1 つの項に番号を振る。複合項は番号だけ先に取って frames に積む。前の再帰と同じ前順の番号になる。
int copy_one(Impl& machine, Cell cell, Image& image, NodeSeen& seen, VarIds& ids, std::vector<CopyFrame>& frames) {
  cell = machine.heap.deref(cell);
  const bool compound = cell.tag() == Tag::Lis || cell.tag() == Tag::Str;
  if (machine.heap.unbound(cell) || compound) {
    if (const auto found = seen.find(cell.raw()); found != seen.end()) return found->second;
  }
  if (machine.heap.unbound(cell)) return copy_var(machine, cell, image, seen, ids);
  if (!compound) return copy_scalar(machine, cell, image);
  CopyNode node;
  std::uint16_t arity = 2;
  if (cell.tag() == Tag::Lis) {
    node.kind = CopyKind::List;
  } else {
    node.kind = CopyKind::Str;
    node.atom = machine.heap.functor_atom(cell);
    node.arity = machine.heap.functor_arity(cell);
    arity = node.arity;
  }
  node.kids.reserve(arity);
  const int at = push_node(image, std::move(node));
  seen.emplace(cell.raw(), at);
  frames.push_back(CopyFrame{at, cell, 0, arity});
  return at;
}

// 深い項でも C++ のスタックを使わないよう、祖先を frames に手で積む（ADR 0011）。
int copy_cell(Impl& machine, Cell cell, Image& image, NodeSeen& seen, VarIds& ids) {
  std::vector<CopyFrame> frames;
  const int root = copy_one(machine, cell, image, seen, ids, frames);
  while (!frames.empty()) {
    CopyFrame& top = frames.back();
    if (top.next == top.arity) {
      frames.pop_back();
      continue;
    }
    const int parent = top.id;
    const Cell child = top.cell.tag() == Tag::Lis
                           ? (top.next == 0 ? machine.heap.list_head(top.cell) : machine.heap.list_tail(top.cell))
                           : machine.heap.arg(top.cell, top.next);
    ++top.next;
    const int kid = copy_one(machine, child, image, seen, ids, frames);
    image.nodes[static_cast<std::size_t>(parent)].kids.push_back(kid);
  }
  return root;
}

Image snapshot(Impl& machine, const std::vector<Cell>& witnesses, Cell templ, VarIds& ids) {
  Image image;
  NodeSeen seen;
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

// base は「呼ぶ前からあったセル」の境の水位なので、語の生死ではなく heap_mark として写す。
// stable・local・homes・alias は見つけたセルの同一性を覚えるだけなので弱い参照にする。死んだセルは二度と
// 現れないので、SIZE_MAX と 0 はどれとも一致しないまま残ってよい。ext_raw は collect の後で答えの外部変数に
// 戻す（solutions_answer.cpp）ので、強い参照にして生かしておく。
void visit_ids(GcVisitor& visitor, VarIds& ids) {
  visitor.heap_mark(ids.base);
  for (auto& item : ids.stable) visitor.weak_index(item.first);
  for (auto& item : ids.local) visitor.weak_index(item.first);
  for (std::uint64_t& raw : ids.ext_raw) {
    Cell cell{raw};
    visitor.cell(cell);
    raw = cell.raw();
  }
  for (Home& home : ids.homes) {
    visitor.weak_index(home.index);
    visitor.weak_address(home.raw);
    // 番地 0 の Ref を deref しないよう、死んだ証人の家は無いものにする。
    if (home.raw == 0) home.live = false;
  }
  for (auto& item : ids.alias) visitor.weak_address(item.first);
}

// 動いている収集の状態（ADR 0013）。Pick::Collect の Entry が持ち、GC は Entry を通して辿る。
// goal は findall などの呼び出しの枠で、終わりに引数と継続を読む。templ・witnesses・hidden は呼ぶ前からあった項。
struct Collecting final : Collector {
  Which which = Which::None;
  Goal goal;
  Cell templ{};
  std::vector<Cell> witnesses;
  std::vector<Cell> hidden;
  VarIds ids;
  Packet packet;

  void visit(GcVisitor& visitor) override {
    visitor.frame(goal.at);
    visitor.frame(goal.cont);
    visitor.cell(templ);
    for (Cell& cell : witnesses) visitor.cell(cell);
    for (Cell& cell : hidden) visitor.cell(cell);
    visit_ids(visitor, ids);
  }
};

// 収集の床と Ctrl::Collect の枠を積み、ゴールを呼び手の run_loop で動かす。\+ と同じ形で、C++ では再帰しない。
// 外側の節走査と catch はスタックにそのまま残るので、assert が直す添字も、ゴールからの例外の行き先も普通の呼び出しと同じ。
void start_collect(Impl& machine, const Compiled& compiled, std::shared_ptr<Collecting> state) {
  Entry entry;
  entry.pick = Pick::Collect;
  entry.heap_mark = machine.heap.size();
  entry.trail_mark = machine.trail.slots.size();
  state->ids.base = entry.heap_mark;
  state->ids.hidden = &state->hidden;
  note_homes(machine, state->witnesses, state->ids);
  const std::uint32_t module = state->goal.module;
  entry.collect = std::move(state);
  push_frame(machine, std::move(entry));
  const auto boundary = machine.stack.size();
  Extra done;
  done.ctrl = Ctrl::Collect;
  done.trim_to = boundary - 1;
  machine.extras.push_back(std::move(done));
  const auto at = machine.heap.allocate_words(kFrameHead);
  machine.heap.set_word(at, 0);
  machine.heap.set_word(at + 1, machine.extras.size() - 1);
  machine.heap.set_word(at + 2, 0);
  machine.heap.set_word(at + 3, kNoFrame);
  launch(machine, compiled.term, at, boundary, compiled.env, module);
}

Collecting& collecting(Entry& entry) { return static_cast<Collecting&>(*entry.collect); }

}  // namespace

void collect_step(Impl& machine, std::size_t entry) {
  Collecting& state = collecting(machine.stack[entry]);
  remember(machine, state.templ, state.witnesses, state.ids, state.packet.groups, state.which != Which::Findall);
  machine.failed = true;
}

void finish_collect(Impl& machine) {
  const CollectRef held = machine.stack.back().collect;
  apply_catches(machine, machine.stack.back());
  machine.stack.pop_back();
  Collecting& state = static_cast<Collecting&>(*held);
  state.packet.ext_raw = std::move(state.ids.ext_raw);
  const Goal& goal = state.goal;
  if (state.which == Which::Findall) {
    sol::bind_found(machine, goal, state.packet);
    return;
  }
  if (state.packet.groups.empty()) {
    machine.failed = true;
    return;
  }
  const bool sorting = state.which == Which::Setof;
  sol::bind_answers(machine, goal, state.witnesses,
                    sol::answers_from(machine, state.packet.groups, sorting, state.packet.ext_raw));
}

bool run_solutions(Impl& machine, const Goal& call) {
  const Which which = which_solution(machine.atoms.name(call.name), call.arity);
  if (which == Which::None) return false;
  const Cell given = machine.heap.deref(call.arg(machine, 1));
  if (which != Which::Findall && !goal_shape_ok(machine, call, given)) return true;
  std::vector<Cell> hidden;
  Cell runnable = given;
  if (which != Which::Findall) runnable = strip_caret(machine, given, hidden);
  Compiled compiled;
  if (!goal_body_ok(machine, call, runnable, compiled)) return true;
  if (bad_list(machine, call)) return true;
  if (bad_tail(machine, call)) return true;
  auto state = std::make_shared<Collecting>();
  state->which = which;
  state->goal = call;
  state->templ = call.arg(machine, 0);
  if (which != Which::Findall) state->witnesses = free_witnesses(machine, runnable, state->templ, hidden);
  state->hidden = std::move(hidden);
  start_collect(machine, compiled, std::move(state));
  return true;
}

}  // namespace mosa
