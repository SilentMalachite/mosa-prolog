// ISO/IEC 13211-1 Cor.2 8.4.3 sort/2 (@list, -list)
//   instantiation_error, type_error(list, List), type_error(list, Sorted)
// ISO/IEC 13211-1 Cor.2 8.4.4 keysort/2 (@list, -list)
//   instantiation_error, type_error(list, Pairs), type_error(pair, E), type_error(list, Sorted)
// length/2 (?list, ?integer)。13211-1 に条項は無く、SPEC 第5節の表で system に置き library(lists) が公開し直す。
//   type_error(integer, N), type_error(list, List), domain_error(not_less_than_zero, N)
//   N が Big（フェーズ10、設計の 8）なら、負は domain_error、正は閉じたリストで失敗し、開いたリストではリストを
//   作る前にヒープの上限で止まる（ResourceLimit）。
// sort(+integer, +atom, +list, -list)。非 ISO（SPEC 第7節の段階 G、レビュー M11）で、SWI-Prolog の pl-list.c に合わせる。
//   Key が 0 なら要素全体、正の整数ならその番目の引数を鍵にする。Order は @< @=< @> @>= と同じ意味の < =< > >=。
//   @< と @> は鍵の等しい要素のうち入力で最初のものだけを残し、@=< と @>= は等しい鍵の順を保つ。
//   検査は Key、Order、List の形、要素の鍵の順。Sorted は検査せず単一化するだけ。
//   instantiation_error, domain_error(not_less_than_one, Key), type_error(sort_key, Key), type_error(atom, Order),
//   domain_error(order, Order), type_error(list, List), type_error(compound, E), existence_error(argument, Key, E)。
//   SWI-Prolog は原子の Key（辞書の鍵）とリストの Key（鍵の道）も受けるが、Mosa に辞書は無く、どちらも
//   type_error(sort_key, Key) にする。Key の Big（フェーズ10）は、負なら domain_error(not_less_than_one)、正なら
//   どの項数より大きいので、大きすぎる int64 と同じく最初の要素で existence_error(argument, Key, E)。
// テスト: tests/pl/lib_lists.pl、tests/pl/utilities.pl、tests/pl/bigint.pl、tests/cpp/bigint_limit_test.cpp
#include "../machine/internal.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace mosa {
namespace {

constexpr std::uint8_t kLengthGen = 6;

void succeed(Impl& machine, const Goal& goal) { machine.ip = goal.cont; }

bool is_int(Cell cell) { return cell.tag() == Tag::Int || cell.tag() == Tag::IntBox; }

bool is_pair(const Impl& machine, Cell cell) {
  return cell.tag() == Tag::Str && machine.heap.functor_arity(cell) == 2 &&
         machine.atoms.name(machine.heap.functor_atom(cell)) == "-";
}

// 8.4.4.3 の順。Pairs の形、その要素、Sorted の形、その要素。sort/2 は要素を見ない。
std::optional<Cell> sort_problem(Impl& machine, Cell list, Cell sorted, bool keyed, WalkedList& input) {
  input = walk_list(machine, list);
  if (input.shape == ListShape::Variable || input.shape == ListShape::Partial) return formal_instantiation(machine);
  if (input.shape == ListShape::Bad) return formal_type(machine, "list", machine.heap.deref(list));
  if (keyed) {
    for (const Cell item : input.items) {
      if (machine.heap.unbound(item)) return formal_instantiation(machine);
      if (!is_pair(machine, item)) return formal_type(machine, "pair", item);
    }
  }
  const WalkedList output = walk_list(machine, sorted);
  if (output.shape == ListShape::Bad) return formal_type(machine, "list", machine.heap.deref(sorted));
  if (keyed) {
    for (const Cell item : output.items) {
      if (!machine.heap.unbound(item) && !is_pair(machine, item)) return formal_type(machine, "pair", item);
    }
  }
  return std::nullopt;
}

void run_sort(Impl& machine, const Goal& goal, bool keyed) {
  WalkedList input;
  if (const auto problem = sort_problem(machine, goal.arg(machine, 0), goal.arg(machine, 1), keyed, input)) {
    raise_formal(machine, *problem, goal.name, goal.arity);
    return;
  }
  std::vector<Cell> items = std::move(input.items);
  // keysort/2 は同じキーの順を保つ（8.4.4.1）。sort/2 は重複を落とすので安定でなくてよいが、同じ手で済ませる。
  std::stable_sort(items.begin(), items.end(), [&machine, keyed](Cell left, Cell right) {
    if (keyed) return compare_cells(machine, machine.heap.arg(left, 0), machine.heap.arg(right, 0)) < 0;
    return compare_cells(machine, left, right) < 0;
  });
  if (!keyed) {
    const auto same = [&machine](Cell left, Cell right) { return compare_cells(machine, left, right) == 0; };
    items.erase(std::unique(items.begin(), items.end(), same), items.end());
  }
  if (do_unify(machine, goal.arg(machine, 1), list_from_cells(machine, items))) succeed(machine, goal);
  else machine.failed = true;
}

struct SortBy {
  std::int64_t key = 0;
  // 正の Big の Key。key は読まず、どの要素にもその番目の引数は無い。誤りの項には元の Key を出す。
  bool big_key = false;
  Cell key_cell{};
  bool descending = false;
  bool unique = false;
};

std::optional<Cell> sort_key_problem(Impl& machine, Cell key, SortBy& by) {
  key = machine.heap.deref(key);
  if (machine.heap.unbound(key)) return formal_instantiation(machine);
  if (!is_integer_tag(key.tag())) return formal_type(machine, "sort_key", key);
  by.key_cell = key;
  if (integer_negative(machine, key)) return formal_domain(machine, "not_less_than_one", key);
  if (key.tag() == Tag::Big) {
    by.big_key = true;
    return std::nullopt;
  }
  by.key = machine.heap.integer_value(key);
  return std::nullopt;
}

std::optional<Cell> sort_order_problem(Impl& machine, Cell order, SortBy& by) {
  order = machine.heap.deref(order);
  if (machine.heap.unbound(order)) return formal_instantiation(machine);
  if (order.tag() != Tag::Atom) return formal_type(machine, "atom", order);
  const auto name = machine.atoms.name(static_cast<std::uint32_t>(order.raw() >> 3));
  const bool known = name == "@<" || name == "<" || name == "@=<" || name == "=<" || name == "@>" || name == ">" ||
                     name == "@>=" || name == ">=";
  if (!known) return formal_domain(machine, "order", order);
  by.descending = name.find('>') != std::string_view::npos;
  by.unique = name.find('=') == std::string_view::npos;
  return std::nullopt;
}

// 鍵が無い要素は、比べる前に入力の順で断る。keys[i] は items[i] の鍵。
std::optional<Cell> sort_keys_problem(Impl& machine, const SortBy& by, const std::vector<Cell>& items,
                                      std::vector<Cell>& keys) {
  keys.reserve(items.size());
  for (const Cell item : items) {
    if (by.key == 0 && !by.big_key) {
      keys.push_back(item);
      continue;
    }
    const Cell term = machine.heap.deref(item);
    if (machine.heap.unbound(term)) return formal_instantiation(machine);
    if (term.tag() != Tag::Str && term.tag() != Tag::Lis) return formal_type(machine, "compound", term);
    const std::int64_t arity = term.tag() == Tag::Lis ? 2 : machine.heap.functor_arity(term);
    if (by.big_key || by.key > arity) return formal_existence_at(machine, "argument", by.key_cell, term);
    if (term.tag() == Tag::Lis) keys.push_back(by.key == 1 ? machine.heap.list_head(term) : machine.heap.list_tail(term));
    else keys.push_back(machine.heap.arg(term, static_cast<std::uint16_t>(by.key - 1)));
  }
  return std::nullopt;
}

std::optional<Cell> sort4_problem(Impl& machine, const Goal& goal, SortBy& by, std::vector<Cell>& items,
                                  std::vector<Cell>& keys) {
  if (auto problem = sort_key_problem(machine, goal.arg(machine, 0), by)) return problem;
  if (auto problem = sort_order_problem(machine, goal.arg(machine, 1), by)) return problem;
  WalkedList input = walk_list(machine, goal.arg(machine, 2));
  if (input.shape == ListShape::Variable || input.shape == ListShape::Partial) return formal_instantiation(machine);
  if (input.shape == ListShape::Bad) return formal_type(machine, "list", machine.heap.deref(goal.arg(machine, 2)));
  items = std::move(input.items);
  return sort_keys_problem(machine, by, items, keys);
}

void run_sort4(Impl& machine, const Goal& goal) {
  SortBy by;
  std::vector<Cell> items;
  std::vector<Cell> keys;
  if (const auto problem = sort4_problem(machine, goal, by, items, keys)) {
    raise_formal(machine, *problem, goal.name, goal.arity);
    return;
  }
  // 入力の位置で並べる。安定なので、等しい鍵は降順でも入力の順のまま並び、重複を落とすと最初の要素が残る。
  std::vector<std::size_t> order(items.size());
  for (std::size_t index = 0; index < order.size(); ++index) order[index] = index;
  std::stable_sort(order.begin(), order.end(), [&machine, &keys, &by](std::size_t left, std::size_t right) {
    const int compared = compare_cells(machine, keys[left], keys[right]);
    return by.descending ? compared > 0 : compared < 0;
  });
  if (by.unique) {
    const auto same = [&machine, &keys](std::size_t left, std::size_t right) {
      return compare_cells(machine, keys[left], keys[right]) == 0;
    };
    order.erase(std::unique(order.begin(), order.end(), same), order.end());
  }
  std::vector<Cell> sorted;
  sorted.reserve(order.size());
  for (const std::size_t index : order) sorted.push_back(items[index]);
  if (do_unify(machine, goal.arg(machine, 3), list_from_cells(machine, sorted))) succeed(machine, goal);
  else machine.failed = true;
}

struct LengthShape {
  std::size_t prefix = 0;
  // 未束縛なら、リストの末尾の変数（リスト全体が変数ならそれ自身）。
  std::optional<Cell> tail;
};

std::optional<Cell> length_problem(Impl& machine, const Goal& goal, LengthShape& shape) {
  const Cell list = machine.heap.deref(goal.arg(machine, 0));
  const Cell count = machine.heap.deref(goal.arg(machine, 1));
  if (!machine.heap.unbound(count) && !is_integer_tag(count.tag())) return formal_type(machine, "integer", count);
  const WalkedList walked = walk_list(machine, list);
  if (walked.shape == ListShape::Bad) return formal_type(machine, "list", list);
  if (!machine.heap.unbound(count) && integer_negative(machine, count)) {
    return formal_domain(machine, "not_less_than_zero", count);
  }
  shape.prefix = walked.items.size();
  if (walked.shape == ListShape::Variable) shape.tail = list;
  if (walked.shape == ListShape::Partial) shape.tail = walked.tail;
  return std::nullopt;
}

// 末尾を size - prefix 個の新しい変数で閉じ、個数を size にする。size は利用者の整数なので先に確保しない。
// ヒープの上限で fresh_ref が ResourceLimit を投げる方が、vector の length_error より先に来る。
bool bind_length(Impl& machine, const Goal& goal, const LengthShape& shape, std::size_t size) {
  std::vector<Cell> fresh;
  for (std::size_t index = shape.prefix; index < size; ++index) fresh.push_back(machine.heap.fresh_ref());
  return do_unify(machine, *shape.tail, list_from_cells(machine, fresh)) &&
         do_unify(machine, goal.arg(machine, 1), machine.heap.integer(static_cast<std::int64_t>(size)));
}

// 個数が未束縛のときは size から一つずつ長くする。選択点は束縛の前に積み、やり直しで束縛を戻す。
void prove_length_from(Impl& machine, const Goal& goal, const LengthShape& shape, std::size_t size) {
  Entry entry;
  entry.pick = Pick::AtomGen;
  entry.gen_kind = kLengthGen;
  entry.next = size + 1;
  entry.redo_goal = goal.at;
  entry.heap_mark = machine.heap.size();
  entry.trail_mark = machine.trail.slots.size();
  push_frame(machine, std::move(entry));
  if (bind_length(machine, goal, shape, size)) succeed(machine, goal);
  else machine.failed = true;
}

void run_length(Impl& machine, const Goal& goal) {
  LengthShape shape;
  if (const auto problem = length_problem(machine, goal, shape)) {
    raise_formal(machine, *problem, goal.name, goal.arity);
    return;
  }
  const Cell count = machine.heap.deref(goal.arg(machine, 1));
  if (!shape.tail) {
    const Cell size = machine.heap.integer(static_cast<std::int64_t>(shape.prefix));
    if (do_unify(machine, count, size)) succeed(machine, goal);
    else machine.failed = true;
    return;
  }
  // 正の Big の長さのリストは、どのヒープの上限にも入らない。大きすぎる int64 は変数を作り続けて上限で止まるが、
  // 同じ止まり方を、作り始める前に出す（ヒープを伸ばさない。bigint_limit_test の length_big）。
  if (count.tag() == Tag::Big) throw ResourceLimit("heap_limit");
  if (is_int(count)) {
    const auto wanted = static_cast<std::size_t>(machine.heap.integer_value(count));
    if (wanted >= shape.prefix && bind_length(machine, goal, shape, wanted)) succeed(machine, goal);
    else machine.failed = true;
    return;
  }
  // length(L, L) の L はリストにも整数にもなれない。生成すると止まらない。
  if (count.raw() == shape.tail->raw()) {
    machine.failed = true;
    return;
  }
  prove_length_from(machine, goal, shape, shape.prefix);
}

}  // namespace

bool redo_length_gen(Impl& machine) {
  if (machine.stack.empty() || machine.stack.back().pick != Pick::AtomGen) return false;
  apply_catches(machine, machine.stack.back());
  const Entry top = machine.stack.back();
  const Goal goal = read_goal(machine, top.redo_goal);
  machine.stack.pop_back();
  // 束縛は戻っている。初めの呼び出しと同じ形を辿り直す。
  LengthShape shape;
  if (length_problem(machine, goal, shape) || !shape.tail) return false;
  // 束縛に失敗しても次の長さの選択点は積んである。failed を立てて run_loop に戻す。
  prove_length_from(machine, goal, shape, top.next);
  return true;
}

bool run_lists(Impl& machine, const Goal& goal) {
  const auto name = machine.atoms.name(goal.name);
  if (name == "length" && goal.arity == 2) {
    run_length(machine, goal);
    return true;
  }
  if ((name == "sort" || name == "keysort") && goal.arity == 2) {
    run_sort(machine, goal, name == "keysort");
    return true;
  }
  if (name == "sort" && goal.arity == 4) {
    run_sort4(machine, goal);
    return true;
  }
  return false;
}

}  // namespace mosa
