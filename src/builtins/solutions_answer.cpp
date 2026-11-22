// 8.10 の袋を呼び出し元へ束縛する。収集は solutions.cpp。
// 800 行を超えないよう、解の復元と選択点だけをこちらに分けた。

#include "solutions_internal.hpp"

#include "mosa/bigint.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace mosa {
namespace sol {
namespace {

// 安定な併合ソート。compare_cells が 0 を返す要素は元の順を保つ（前の挿入ソートと同じ並び）。比較は N log N 回で、
// 前の挿入ソートは 2 乗だった（レビュー M-27）。std::sort は比較が厳密な弱順序でないと範囲の外を読みうるが、併合は
// 比較の結果によらず添字を範囲の中で進める。
template <typename T, typename Key>
void merge_sort(Impl& machine, std::vector<T>& items, Key key) {
  std::vector<T> buffer(items.size());
  for (std::size_t width = 1; width < items.size(); width *= 2) {
    for (std::size_t left = 0; left < items.size(); left += 2 * width) {
      const std::size_t mid = std::min(left + width, items.size());
      const std::size_t right = std::min(left + 2 * width, items.size());
      std::size_t first = left;
      std::size_t second = mid;
      std::size_t out = left;
      while (first < mid && second < right) {
        const bool take_second = compare_cells(machine, key(items[second]), key(items[first])) < 0;
        buffer[out++] = std::move(take_second ? items[second++] : items[first++]);
      }
      while (first < mid) buffer[out++] = std::move(items[first++]);
      while (second < right) buffer[out++] = std::move(items[second++]);
    }
    std::swap(items, buffer);
  }
}

void sort_cells(Impl& machine, std::vector<Cell>& items) {
  merge_sort(machine, items, [](const Cell& item) { return item; });
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
  merge_sort(machine, answers, [](const Answer& answer) { return answer.witness; });
}

struct Porter {
  Impl* machine = nullptr;
  const Image* image = nullptr;
  const std::vector<std::uint64_t>* ext_raw = nullptr;
  std::vector<Cell> loc;
  std::vector<Cell>* shared = nullptr;
  std::vector<char>* shared_mark = nullptr;
  bool marking = false;
  std::vector<Cell> made;
  std::vector<char> started;

  Cell slot(std::vector<Cell>& table, int id) {
    const auto index = static_cast<std::size_t>(id);
    if (table.size() <= index) table.resize(index + 1);
    if (table[index].raw() == 0) table[index] = machine->heap.fresh_ref();
    return table[index];
  }

  bool marked(int id) const {
    const auto index = static_cast<std::size_t>(id);
    return shared_mark != nullptr && index < shared_mark->size() && (*shared_mark)[index] != 0;
  }

  void mark(int id) {
    if (shared_mark == nullptr) return;
    const auto index = static_cast<std::size_t>(id);
    if (shared_mark->size() <= index) shared_mark->resize(index + 1);
    (*shared_mark)[index] = 1;
  }

  // 証人に出た局所変数だけ、袋の中で同じセルにする。存在量化だけの変数は解ごとに新しい。
  Cell local_cell(int id) {
    if (marking) mark(id);
    return slot(marked(id) ? *shared : loc, id);
  }

  // 取り込み中の複合項 1 つ分。穴を先に作って made に置き、子をすべて作ってから本体を作って穴に束縛する。
  struct Frame {
    int index;
    Cell hole;
    std::size_t next;
    std::vector<Cell> args;
  };
  std::vector<Frame> frames;

  // 前の再帰と同じ順にヒープを割り当てる。変数の標準順序は番地で決まるので、順を変えると答えが変わる。
  // 深い像でも C++ のスタックを使わないよう、祖先を frames に手で積む（ADR 0011）。
  Cell at(int index) {
    const Cell result = start(index);
    while (!frames.empty()) {
      Frame& top = frames.back();
      const CopyNode& node = image->nodes[static_cast<std::size_t>(top.index)];
      if (top.next == node.kids.size()) {
        const Cell real = node.kind == CopyKind::List ? machine->heap.list(top.args[0], top.args[1])
                                                       : machine->heap.structure(node.atom, top.args);
        machine->heap.bind(top.hole, real);
        frames.pop_back();
        continue;
      }
      const int kid = node.kids[top.next++];
      // start が frames を伸ばすと top は無効になるので、子の値は伸ばす前の位置へ添字で書く。
      const std::size_t slot_at = frames.size() - 1;
      const Cell value = start(kid);
      frames[slot_at].args.push_back(value);
    }
    return result;
  }

  Cell start(int index) {
    const auto slot_at = static_cast<std::size_t>(index);
    if (started[slot_at]) return made[slot_at];
    started[slot_at] = 1;
    const CopyNode& node = image->nodes[slot_at];
    Cell cell;
    if (node.kind == CopyKind::Ext) cell = Cell{ext_raw->at(static_cast<std::size_t>(node.id))};
    else if (node.kind == CopyKind::Loc) cell = local_cell(node.id);
    else if (node.kind == CopyKind::Atom) cell = machine->heap.atom(node.atom);
    else if (node.kind == CopyKind::Int) cell = machine->heap.integer(node.integer);
    else if (node.kind == CopyKind::Float) cell = machine->heap.floating(node.floating);
    else if (node.kind == CopyKind::Big) cell = big(node);
    else {
      cell = machine->heap.fresh_ref();
      std::vector<Cell> args;
      args.reserve(node.kids.size());
      frames.push_back(Frame{index, cell, 0, std::move(args)});
    }
    made[slot_at] = cell;
    return cell;
  }

  // 子の Int の節点は語として読むだけで、セルにはしない（catch.cpp の取り込みと同じ）。
  Cell big(const CopyNode& node) {
    std::vector<std::uint64_t> words;
    words.reserve(node.kids.size());
    for (const int kid : node.kids) {
      words.push_back(std::bit_cast<std::uint64_t>(image->nodes[static_cast<std::size_t>(kid)].integer));
    }
    return machine->heap.big(BigInt::from_words(words, node.integer != 0));
  }
};

Porter porter_for(Impl& machine, const Image& image, const std::vector<std::uint64_t>& ext_raw) {
  Porter porter;
  porter.machine = &machine;
  porter.image = &image;
  porter.ext_raw = &ext_raw;
  porter.made.assign(image.nodes.size(), Cell{});
  porter.started.assign(image.nodes.size(), 0);
  return porter;
}

void finish_list(Impl& machine, Answer& answer, std::vector<Cell>& templs, bool sorting) {
  if (sorting) {
    sort_cells(machine, templs);
    unique_cells(machine, templs);
  }
  answer.list = list_from_cells(machine, templs);
}

Answer make_shared(Impl& machine, const Group& group, bool sorting, const std::vector<std::uint64_t>& ext_raw) {
  Answer answer;
  std::vector<Cell> templs;
  std::vector<Cell> shared;
  std::vector<char> shared_mark;
  bool first = true;
  for (const Image& image : group.shots) {
    Porter porter = porter_for(machine, image, ext_raw);
    porter.shared = &shared;
    porter.shared_mark = &shared_mark;
    porter.marking = true;
    const Cell witness = porter.at(image.witness);
    porter.marking = false;
    if (first) {
      answer.witness = witness;
      answer.values = walk_list(machine, answer.witness).items;
      first = false;
    }
    templs.push_back(porter.at(image.templ));
  }
  finish_list(machine, answer, templs, sorting);
  return answer;
}

// list_from_cells は常に [] で閉じる。他の呼び出しがその閉じに依存するので、開いた末尾はここで付ける。
Cell list_ending(Impl& machine, const std::vector<Cell>& items, Cell tail) {
  Cell end = tail;
  for (auto it = items.rbegin(); it != items.rend(); ++it) end = machine.heap.list(*it, end);
  return end;
}

Answer make_copied(Impl& machine, const Group& group, bool sorting, Cell tail) {
  Answer answer;
  std::vector<Cell> templs;
  templs.reserve(group.templs.size());
  for (const ExportedTerm& term : group.templs) templs.push_back(import_term(machine, term));
  if (sorting) {
    sort_cells(machine, templs);
    unique_cells(machine, templs);
  }
  answer.list = list_ending(machine, templs, tail);
  return answer;
}

Answer make_answer(Impl& machine, const Group& group, bool sorting, const std::vector<std::uint64_t>& ext_raw) {
  if (!group.shots.empty()) return make_shared(machine, group, sorting, ext_raw);
  return make_copied(machine, group, sorting, machine.heap.atom(machine.atoms.intern("[]")));
}

Answer blank_answer(Impl& machine) {
  Answer answer;
  answer.list = list_from_cells(machine, {});
  answer.witness = answer.list;
  return answer;
}

IrTerm slot_var(std::uint32_t slot) {
  IrTerm term;
  term.tag = IrTag::Var;
  term.slot = slot;
  return term;
}

IrTerm eq_term(std::uint32_t name, std::uint32_t left, std::uint32_t right) {
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

}  // namespace

std::vector<Answer> answers_from(Impl& machine, const std::vector<Group>& groups, bool sorting,
                                 const std::vector<std::uint64_t>& ext_raw) {
  std::vector<Answer> answers;
  answers.reserve(groups.size());
  for (const Group& group : groups) answers.push_back(make_answer(machine, group, sorting, ext_raw));
  if (sorting) sort_answers(machine, answers);
  return answers;
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
    const auto list_slot = slot_number(env.size());
    env.push_back(answer.list);
    for (std::size_t index = 0; index < witnesses.size(); ++index) {
      const auto slot = slot_number(env.size());
      env.push_back(answer.values[index]);
      parts.push_back(eq_term(eq, slot_number(index + 1), slot));
    }
    parts.push_back(eq_term(eq, 0, list_slot));
    branches.push_back(fold(IrTag::And, parts));
  }
  // 束縛は収集が例外なく終わったあと。残りの証人は選択点で返す。単一化は = だけで、= は文脈モジュールを見ないので、
  // system の文脈で動かす。デバッガはそこから出る呼び出しに箱を作らない（組込の中には入らない。ADR 0028）。
  launch(machine, make_run_goal(machine.heap, fold(IrTag::Or, branches)), goal.cont, machine.stack.size(),
         make_env(machine.heap, std::move(env)), machine.system_module);
}

// 空の findall/3 は []。空の findall/4 は List と Tail を同じにし、Tail を [] にしない。
void bind_found(Impl& machine, const Goal& goal, const Packet& packet) {
  if (packet.groups.empty()) {
    if (goal.arity == 3) {
      bind_answers(machine, goal, {}, {blank_answer(machine)});
      return;
    }
    Answer answer;
    answer.list = goal.arg(machine, 3);
    bind_answers(machine, goal, {}, {answer});
    return;
  }
  const Cell tail = goal.arity == 4 ? goal.arg(machine, 3) : machine.heap.atom(machine.atoms.intern("[]"));
  std::vector<Answer> answers;
  answers.reserve(packet.groups.size());
  for (const Group& group : packet.groups) answers.push_back(make_copied(machine, group, false, tail));
  bind_answers(machine, goal, {}, answers);
}

}  // namespace sol
}  // namespace mosa
