// 8.16.3 sub_atom(+atom, ?integer, ?integer, ?integer, ?atom)
// エラー: instantiation_error, type_error(atom), type_error(integer), domain_error(not_less_than_zero)
// 束縛された引数が許す候補だけを試す（レビュー M32）。Before、Length、After のうち 2 つが決まれば残りも決まり、
// 候補は 1 つ。1 つだけ決まれば残りの軸を動かす。Sub が決まれば Length も決まり、その字面が現れる位置だけを探す。
// 前は (Before, Length) の組をすべて先頭から数え直して試したので、長さ N の原子で N の 3 乗かかった。
// 解の順は 8.16.3 の通り Before の昇順、同じ Before では Length の昇順で、すべてを数え上げたときと同じ。
// テスト: tests/pl/atoms.pl（sub_atom_*、m32_*）、tests/pl/japanese.pl（sub_atom_go_sho、m32_ja_*）、
//   tests/cpp/sub_atom_scale_test.cpp

#include "../machine/internal.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mosa {
namespace {

// internal.hpp の Entry::gen_kind の 2。
constexpr std::uint8_t kSubGen = 2;

bool is_int_cell(Cell cell) { return cell.tag() == Tag::Int || cell.tag() == Tag::IntBox; }

std::uint32_t atom_id(Cell cell) { return static_cast<std::uint32_t>(cell.raw() >> 3); }

void type_at(Impl& machine, const Goal& goal, std::string_view type, Cell culprit) {
  raise_formal(machine, formal_type(machine, type, culprit), goal.name, goal.arity);
}

void domain_at(Impl& machine, const Goal& goal, Cell culprit) {
  raise_formal(machine, formal_domain(machine, "not_less_than_zero", culprit), goal.name, goal.arity);
}

bool reject_type(Impl& machine, const Goal& goal, Cell cell) {
  if (machine.heap.unbound(cell) || is_int_cell(cell)) return false;
  type_at(machine, goal, "integer", cell);
  return true;
}

bool reject_domain(Impl& machine, const Goal& goal, Cell cell) {
  if (!is_int_cell(cell) || machine.heap.integer_value(cell) >= 0) return false;
  domain_at(machine, goal, cell);
  return true;
}

// 候補の並べ方。One は候補が 1 つ。Before、Length、After はその引数だけが決まっている。Matches は Sub が決まり、
// Before も After も決まっていない。SameBeforeLength、SameBeforeAfter、SameLengthAfter は何も決まらず、名の 2 つの
// 引数が同じ変数。All は何も決まらず、同じ変数も無い。
enum class SubWalk { One, Before, Length, After, Matches, SameBeforeLength, SameBeforeAfter, SameLengthAfter, All };

struct SubPick {
  std::size_t before = 0;
  std::size_t length = 0;
};

struct SubQuery {
  CodeText text;
  SubWalk walk = SubWalk::All;
  // One では候補そのもの。Before、Length、Matches、After では決まった値を同じ名の欄に置く。
  std::size_t before = 0;
  std::size_t length = 0;
  std::size_t after = 0;
  bool sub_bound = false;
  // 原子表の名前は intern で動きうるので写す。
  std::string sub;
  // sub の KMP の失敗関数。素朴に照らすと、a…ab を a…a の中に探すとき 1 つの答えに長さの 2 乗かかる。
  std::vector<std::size_t> border;
};

// 決まった値と、同じ未束縛の変数を置いた引数の組。同じ変数を見ないと、sub_atom(A, X, _, X, P) が Sub の現れる
// 位置をすべて試して 1 つずつ外し、唯一の答えまで長さの 2 乗かかる（M32 の Codex 敵対的レビュー 1 回目）。
struct SubKnown {
  std::optional<std::uint64_t> before;
  std::optional<std::uint64_t> length;
  std::optional<std::uint64_t> after;
  bool before_length = false;
  bool before_after = false;
  bool length_after = false;
};

std::optional<std::uint64_t> bound_value(const Impl& machine, Cell cell) {
  if (machine.heap.unbound(cell)) return std::nullopt;
  return static_cast<std::uint64_t>(machine.heap.integer_value(cell));
}

// total から x と y を引いた残り。整数は 2^63 - 1 まであるので、足してから比べるとあふれる。
std::optional<std::size_t> rest(std::size_t total, std::uint64_t x, std::uint64_t y) {
  if (x > total || y > total - x) return std::nullopt;
  return total - static_cast<std::size_t>(x) - static_cast<std::size_t>(y);
}

bool set_one(SubQuery& query, std::uint64_t before, std::uint64_t length) {
  query.walk = SubWalk::One;
  query.before = static_cast<std::size_t>(before);
  query.length = static_cast<std::size_t>(length);
  return true;
}

bool set_axis(SubQuery& query, SubWalk walk, std::uint64_t value, std::size_t& slot) {
  if (value > query.text.size()) return false;
  query.walk = walk;
  slot = static_cast<std::size_t>(value);
  return true;
}

// 同じ変数の片方の値が決まれば、もう片方も同じ値になる。
bool share(std::optional<std::uint64_t>& left, std::optional<std::uint64_t>& right, bool same) {
  if (!same) return true;
  if (left && right) return *left == *right;
  if (left) right = left;
  else if (right) left = right;
  return true;
}

int known_count(const SubKnown& known) {
  return (known.before ? 1 : 0) + (known.length ? 1 : 0) + (known.after ? 1 : 0);
}

// 2 つ以上の値が決まっている。Before + Length + After は原子の長さなので、残りも決まる。
bool plan_fixed(SubQuery& query, const SubKnown& known) {
  const std::size_t total = query.text.size();
  if (known.before && known.length) {
    const auto after = rest(total, *known.before, *known.length);
    if (!after || (known.after && *known.after != *after)) return false;
    return set_one(query, *known.before, *known.length);
  }
  if (known.before && known.after) {
    const auto length = rest(total, *known.before, *known.after);
    return length && set_one(query, *known.before, *length);
  }
  const auto before = rest(total, *known.length, *known.after);
  return before && set_one(query, *before, *known.length);
}

// 1 つの値が決まり、残りの 2 つが同じ変数なら、どちらも残りの長さの半分。
bool plan_halved(SubQuery& query, const SubKnown& known) {
  const std::uint64_t value = known.before ? *known.before : known.length ? *known.length : *known.after;
  const auto room = rest(query.text.size(), value, 0);
  if (!room || *room % 2 != 0) return false;
  const std::size_t half = *room / 2;
  if (known.before) return set_one(query, value, half);
  if (known.length) return set_one(query, half, value);
  return set_one(query, half, half);
}

bool plan_single(SubQuery& query, const SubKnown& known) {
  if (known.before) return set_axis(query, SubWalk::Before, *known.before, query.before);
  if (known.length) {
    const SubWalk walk = query.sub_bound ? SubWalk::Matches : SubWalk::Length;
    return set_axis(query, walk, *known.length, query.length);
  }
  return set_axis(query, SubWalk::After, *known.after, query.after);
}

bool plan_free(SubQuery& query, const SubKnown& known) {
  const std::size_t total = query.text.size();
  if (known.before_length && known.before_after) return total % 3 == 0 && set_one(query, total / 3, total / 3);
  query.walk = known.before_length  ? SubWalk::SameBeforeLength
               : known.before_after ? SubWalk::SameBeforeAfter
               : known.length_after ? SubWalk::SameLengthAfter
                                    : SubWalk::All;
  return true;
}

bool plan_walk(SubQuery& query, SubKnown known) {
  if (!share(known.before, known.length, known.before_length) || !share(known.before, known.after, known.before_after) ||
      !share(known.length, known.after, known.length_after)) {
    return false;
  }
  const int count = known_count(known);
  if (count >= 2) return plan_fixed(query, known);
  if (count == 0) return plan_free(query, known);
  const bool others_same = (known.before && known.length_after) || (known.length && known.before_after) ||
                           (known.after && known.before_length);
  return others_same ? plan_halved(query, known) : plan_single(query, known);
}

std::vector<std::size_t> border_table(std::string_view pattern) {
  std::vector<std::size_t> border(pattern.size(), 0);
  std::size_t width = 0;
  for (std::size_t index = 1; index < pattern.size(); ++index) {
    while (width > 0 && pattern[index] != pattern[width]) width = border[width - 1];
    if (pattern[index] == pattern[width]) ++width;
    border[index] = width;
  }
  return border;
}

// バイトの位置が字の切れ目なら、その字の番号。
std::optional<std::size_t> code_at(const CodeText& text, std::size_t byte) {
  const auto found = std::lower_bound(text.cuts.begin(), text.cuts.end(), byte);
  if (found == text.cuts.end() || *found != byte) return std::nullopt;
  return static_cast<std::size_t>(found - text.cuts.begin());
}

// from 番目の字から後で、Sub と同じバイト列が字の切れ目から切れ目までに現れる最初の位置。原子の同一はバイト列の
// 同一なので、すべてを数え上げて intern し単一化したときと同じ位置だけが合う。
std::optional<std::size_t> find_match(const SubQuery& query, std::size_t from) {
  const std::string_view hay{query.text.bytes};
  const std::string_view pattern{query.sub};
  if (pattern.empty()) return from;
  std::size_t matched = 0;
  for (std::size_t at = query.text.cuts[from]; at < hay.size(); ++at) {
    while (matched > 0 && hay[at] != pattern[matched]) matched = query.border[matched - 1];
    if (hay[at] == pattern[matched]) ++matched;
    if (matched < pattern.size()) continue;
    // 切れ目でない位置で合うのは、壊れた UTF-8 の字のバイトを読み替えたときだけ。
    const auto start = code_at(query.text, at + 1 - pattern.size());
    const auto end = code_at(query.text, at + 1);
    if (start && end && *end - *start == query.length) return start;
    matched = query.border[matched - 1];
  }
  return std::nullopt;
}

bool matches_at(const SubQuery& query, std::size_t before) {
  return query.text.slice(before, query.length) == query.sub;
}

std::optional<SubPick> seek_one(const SubQuery& query, std::size_t before, std::size_t length) {
  if (before < query.before || (before == query.before && length <= query.length)) {
    return SubPick{query.before, query.length};
  }
  return std::nullopt;
}

std::optional<SubPick> seek_before(const SubQuery& query, std::size_t before, std::size_t length) {
  if (before < query.before) return SubPick{query.before, 0};
  if (before == query.before && length <= query.text.size() - query.before) return SubPick{before, length};
  return std::nullopt;
}

// Length が決まった並べ方で、(before, length) 以後の最初の Before。
std::optional<std::size_t> fixed_length_start(const SubQuery& query, std::size_t before, std::size_t length) {
  const std::size_t start = length <= query.length ? before : before + 1;
  if (start > query.text.size() - query.length) return std::nullopt;
  return start;
}

std::optional<SubPick> seek_after(const SubQuery& query, std::size_t before, std::size_t length) {
  const std::size_t room = query.text.size() - query.after;
  if (before <= room && length > room - before) ++before;
  if (before > room) return std::nullopt;
  return SubPick{before, room - before};
}

std::optional<SubPick> seek_same_before_length(const SubQuery& query, std::size_t before, std::size_t length) {
  const std::size_t start = length <= before ? before : before + 1;
  if (start > query.text.size() / 2) return std::nullopt;
  return SubPick{start, start};
}

std::optional<SubPick> seek_same_before_after(const SubQuery& query, std::size_t before, std::size_t length) {
  const std::size_t total = query.text.size();
  const std::size_t start = before <= total / 2 && length <= total - 2 * before ? before : before + 1;
  if (start > total / 2) return std::nullopt;
  return SubPick{start, total - 2 * start};
}

// Length と After が同じなら、Before は原子の長さと偶奇が同じ位置だけ。
std::optional<SubPick> seek_same_length_after(const SubQuery& query, std::size_t before, std::size_t length) {
  const std::size_t total = query.text.size();
  std::size_t start = before;
  if (start <= total && (total - start) % 2 == 0 && length > (total - start) / 2) ++start;
  if (start <= total && (total - start) % 2 != 0) ++start;
  if (start > total) return std::nullopt;
  return SubPick{start, (total - start) / 2};
}

std::optional<SubPick> seek_all(const SubQuery& query, std::size_t before, std::size_t length) {
  const std::size_t total = query.text.size();
  if (before <= total && length <= total - before) return SubPick{before, length};
  if (before < total) return SubPick{before + 1, 0};
  return std::nullopt;
}

// (before, length) かその後で最初の候補。並べ方の順は Before の昇順、同じ Before では Length の昇順。
std::optional<SubPick> seek(const SubQuery& query, std::size_t before, std::size_t length) {
  switch (query.walk) {
    case SubWalk::One:
      return seek_one(query, before, length);
    case SubWalk::Before:
      return seek_before(query, before, length);
    case SubWalk::Length:
      if (const auto start = fixed_length_start(query, before, length)) return SubPick{*start, query.length};
      return std::nullopt;
    case SubWalk::Matches:
      if (const auto start = fixed_length_start(query, before, length)) {
        if (const auto found = find_match(query, *start)) return SubPick{*found, query.length};
      }
      return std::nullopt;
    case SubWalk::After:
      return seek_after(query, before, length);
    case SubWalk::SameBeforeLength:
      return seek_same_before_length(query, before, length);
    case SubWalk::SameBeforeAfter:
      return seek_same_before_after(query, before, length);
    case SubWalk::SameLengthAfter:
      return seek_same_length_after(query, before, length);
    case SubWalk::All:
      return seek_all(query, before, length);
  }
  return std::nullopt;
}

bool same_var(const Impl& machine, Cell left, Cell right) {
  return machine.heap.unbound(left) && left.raw() == right.raw();
}

// 束縛は戻した状態で呼ぶ。初めの呼び出しでもやり直しでも、同じ引数から同じ並べ方ができる。
std::optional<SubQuery> make_query(Impl& machine, const Goal& goal) {
  SubQuery query;
  query.text = decode_codes(machine.atoms.name(atom_id(machine.heap.deref(goal.arg(machine, 0)))));
  const Cell before = machine.heap.deref(goal.arg(machine, 1));
  const Cell length = machine.heap.deref(goal.arg(machine, 2));
  const Cell after = machine.heap.deref(goal.arg(machine, 3));
  const Cell sub = machine.heap.deref(goal.arg(machine, 4));
  // Sub と整数の引数が同じ変数なら、原子と整数の両方にはなれない。
  if (same_var(machine, sub, before) || same_var(machine, sub, length) || same_var(machine, sub, after)) {
    return std::nullopt;
  }
  SubKnown known{bound_value(machine, before), bound_value(machine, length), bound_value(machine, after),
                 same_var(machine, before, length), same_var(machine, before, after), same_var(machine, length, after)};
  if (!machine.heap.unbound(sub)) {
    query.sub_bound = true;
    query.sub.assign(machine.atoms.name(atom_id(sub)));
    const std::uint64_t size = decode_codes(query.sub).size();
    if (known.length && *known.length != size) return std::nullopt;
    known.length = size;
  }
  if (!plan_walk(query, known)) return std::nullopt;
  if (query.walk == SubWalk::One && query.sub_bound && !matches_at(query, query.before)) return std::nullopt;
  if (query.walk == SubWalk::Matches) query.border = border_table(query.sub);
  return query;
}

bool bind_pick(Impl& machine, const Goal& goal, const SubQuery& query, SubPick pick) {
  const std::size_t after = query.text.size() - pick.before - pick.length;
  const Cell before_cell = machine.heap.integer(static_cast<std::int64_t>(pick.before));
  const Cell length_cell = machine.heap.integer(static_cast<std::int64_t>(pick.length));
  const Cell after_cell = machine.heap.integer(static_cast<std::int64_t>(after));
  if (!do_unify(machine, goal.arg(machine, 1), before_cell) || !do_unify(machine, goal.arg(machine, 2), length_cell) ||
      !do_unify(machine, goal.arg(machine, 3), after_cell)) {
    return false;
  }
  // 束縛された Sub は候補と同じバイト列なので同じ原子。部分原子は整数が合ってから作り、外れる候補で原子表を
  // 伸ばさない。
  if (query.sub_bound) return true;
  const Cell sub = machine.heap.atom(machine.atoms.intern(query.text.slice(pick.before, pick.length)));
  return do_unify(machine, goal.arg(machine, 4), sub);
}

void push_sub(Impl& machine, const Goal& goal, SubPick pick, std::size_t heap_mark, std::size_t trail_mark) {
  Entry entry;
  entry.pick = Pick::AtomGen;
  entry.gen_kind = kSubGen;
  entry.next = pick.before;
  entry.cursor = pick.length;
  entry.redo_goal = goal.at;
  entry.heap_mark = heap_mark;
  entry.trail_mark = trail_mark;
  push_frame(machine, std::move(entry));
}

bool prove_sub(Impl& machine, const Goal& goal, const SubQuery& query, std::optional<SubPick> pick) {
  while (pick) {
    const auto heap_mark = machine.heap.size();
    const auto trail_mark = machine.trail.slots.size();
    const auto extras_mark = machine.extras.size();
    // 次の候補を先に探し、あるときだけ選択点を積む。最後の答えは選択点を残さない。水位は束縛より前に取る。
    const std::optional<SubPick> following = seek(query, pick->before, pick->length + 1);
    if (following) push_sub(machine, goal, *following, heap_mark, trail_mark);
    if (bind_pick(machine, goal, query, *pick)) {
      machine.ip = goal.cont;
      return true;
    }
    if (following) machine.stack.pop_back();
    restore_state(machine, heap_mark, trail_mark, extras_mark);
    pick = following;
  }
  return false;
}

}  // namespace

void run_sub_atom(Impl& machine, const Goal& goal) {
  const Cell atom = machine.heap.deref(goal.arg(machine, 0));
  const Cell sub = machine.heap.deref(goal.arg(machine, 4));
  if (machine.heap.unbound(atom)) {
    raise_formal(machine, formal_instantiation(machine), goal.name, goal.arity);
    return;
  }
  if (atom.tag() != Tag::Atom) {
    type_at(machine, goal, "atom", atom);
    return;
  }
  // 8.16.3.3 は整数の型を、部分アトムの型より先に見る。
  const Cell before = machine.heap.deref(goal.arg(machine, 1));
  const Cell length = machine.heap.deref(goal.arg(machine, 2));
  const Cell after = machine.heap.deref(goal.arg(machine, 3));
  if (reject_type(machine, goal, before) || reject_type(machine, goal, length) || reject_type(machine, goal, after)) {
    return;
  }
  if (!machine.heap.unbound(sub) && sub.tag() != Tag::Atom) {
    type_at(machine, goal, "atom", sub);
    return;
  }
  if (reject_domain(machine, goal, before) || reject_domain(machine, goal, length) ||
      reject_domain(machine, goal, after)) {
    return;
  }
  const auto query = make_query(machine, goal);
  if (!query || !prove_sub(machine, goal, *query, seek(*query, 0, 0))) machine.failed = true;
}

bool redo_sub_atom(Impl& machine) {
  apply_catches(machine, machine.stack.back());
  const Entry top = machine.stack.back();
  const Goal goal = read_goal(machine, top.redo_goal);
  machine.stack.pop_back();
  const auto query = make_query(machine, goal);
  if (!query) return false;
  return prove_sub(machine, goal, *query, seek(*query, top.next, top.cursor));
}

}  // namespace mosa
