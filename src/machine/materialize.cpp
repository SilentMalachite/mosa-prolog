// 節の IR から項を組む（materialize）。ADR 0011（深い項は手で積む）と ADR 0022（assert した節の共有の項は、1 つの頭か
// 1 つのゴールを組む間に 1 度だけ組む、レビュー M30）。
// テスト: tests/pl/database.pl の m30_*、tests/cpp/assert_shared_test.cpp、tests/cpp/machine_deep_test.cpp、
// tests/cpp/bigint_copy_test.cpp（Big）
#include "internal.hpp"

#include "mosa/bigint.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <type_traits>
#include <vector>

namespace mosa {
namespace {

// この深さまでは C++ の再帰で辿る。より深い部分木は手で積んで辿る（ADR 0011 と改訂 2）。浅い項の確保と
// 辿る順は前の再帰と同じになる。
constexpr int kShallow = 64;

// 写しの文脈。共有の項のない写し（PlainCopy）は値で渡す 2 語なのでレジスタで渡り、前と同じ速さで辿る。共有の項の
// ある写し（SharedCopy）は 1 語の指し手で、memo を平らにした SharedState を指す。前者に memo を足すと共有の項のない
// 節の写しが 2.5% 遅くなり、後者を前者と同じ形で渡すと共有の項を組む写しが 8% 遅くなったので、型で分ける。
struct PlainCopy {
  static constexpr bool kShared = false;
  Impl* machine_;
  const std::vector<Cell>* env_;

  [[nodiscard]] Impl& machine() const { return *machine_; }
  [[nodiscard]] const std::vector<Cell>& env() const { return *env_; }
};

// ADR 0022 の memo を平らにしたもの。鎖になった共有の項の 1 段ごとに辿る間接を減らす（M30 の Codex 敵対的レビュー
// 5 回目）。SharedMemo を作るのは写しの外だけなので、写しの間 entries の置き場は動かない。
struct SharedState {
  Impl* machine;
  const std::vector<Cell>* env;
  std::uint32_t base;
  const SharedTerm* terms;
  SharedEntry* entries;
  std::uint64_t stamp;
};

struct SharedCopy {
  static constexpr bool kShared = true;
  const SharedState* state;

  [[nodiscard]] Impl& machine() const { return *state->machine; }
  [[nodiscard]] const std::vector<Cell>& env() const { return *state->env; }
  [[nodiscard]] bool shared(const IrTerm& term) const { return term.tag == IrTag::Var && term.slot >= state->base; }
  [[nodiscard]] std::uint32_t index(const IrTerm& term) const { return term.slot - state->base; }
  [[nodiscard]] const IrTerm& body(std::uint32_t index) const { return state->terms[index].term; }
  [[nodiscard]] std::optional<Cell> find(std::uint32_t index) const {
    const SharedEntry& entry = state->entries[index];
    if (entry.stamp != state->stamp) return std::nullopt;
    return entry.cell;
  }
  void add(std::uint32_t index, Cell cell) const { state->entries[index] = SharedEntry{cell, state->stamp}; }
};

SharedState state_of(Impl& machine, const std::vector<Cell>& env, const SharedMemo& memo) {
  return SharedState{&machine, &env, memo.table.base, memo.table.terms.data(), memo.entries.data(), memo.stamp};
}

// 子を持たない項のセル。子を集めてから作る複合項（List・Fun・Call と制御の項）は nullopt。共有の項のスロットは
// 呼び手が先に見る。Big の子は項でなく絶対値の語なので、ここで箱にし終える（nullopt にすると構造体に組まれる）。
std::optional<Cell> materialize_leaf(Impl& machine, const IrTerm& term, const std::vector<Cell>& env) {
  switch (term.tag) {
    case IrTag::Var: return env.at(term.slot);
    case IrTag::Anon: return machine.heap.fresh_ref();
    case IrTag::Atom: return machine.heap.atom(term.atom);
    case IrTag::Int: return machine.heap.integer(term.integer);
    case IrTag::Float: return machine.heap.floating(term.floating);
    case IrTag::Big: return machine.heap.big(BigInt::from_words(big_words(term), term.integer != 0));
    case IrTag::True: return machine.heap.atom(machine.atoms.intern("true"));
    case IrTag::Fail: return machine.heap.atom(machine.atoms.intern("fail"));
    case IrTag::Cut: return machine.heap.atom(machine.atoms.intern("!"));
    case IrTag::List:
    case IrTag::And:
    case IrTag::Or:
    case IrTag::IfThen:
    case IrTag::SoftIf:
    case IrTag::Catch:
    case IrTag::Throw:
    case IrTag::Halt:
    case IrTag::Call:
    case IrTag::Fun: return std::nullopt;
  }
  return machine.heap.atom(machine.atoms.intern("[]"));
}

Cell close_compound(Impl& machine, const IrTerm& term, std::span<const Cell> parts) {
  if (term.tag == IrTag::List) return machine.heap.list(parts[0], parts[1]);
  if (term.tag == IrTag::Call || term.tag == IrTag::Fun) {
    if (parts.size() == 2 && machine.atoms.name(term.atom) == ".") return machine.heap.list(parts[0], parts[1]);
    return machine.heap.structure(term.atom, parts);
  }
  const char* name = "!";
  if (term.tag == IrTag::And) name = ",";
  else if (term.tag == IrTag::Or) name = ";";
  else if (term.tag == IrTag::IfThen) name = "->";
  else if (term.tag == IrTag::SoftIf) name = "*->";
  else if (term.tag == IrTag::Catch) name = "catch";
  else if (term.tag == IrTag::Throw) name = "throw";
  else if (term.tag == IrTag::Halt) name = "halt";
  return machine.heap.structure(machine.atoms.intern(name), parts);
}

// 組み立て途中の複合項。子のセルは parts に続けて置き、base は自分の分の始まり。record は組み終えたら memo に置く
// 共有の項の添字。共有の項のない写しの枠は record を持たない（2 語の枠を 3 語にすると深い写しが 2% 遅くなった）。
constexpr std::uint32_t kNoRecord = UINT32_MAX;
struct PlainFrame {
  const IrTerm* term;
  std::size_t base;
};
struct SharedFrame {
  const IrTerm* term;
  std::size_t base;
  std::uint32_t record = kNoRecord;
};

// 後順に組み立てる。子は左から作るので、ヒープの並びは再帰と同じになる。まだ組んでいない共有の項は同じ歩みの枠に
// するので、鎖になった共有の項も 1 段ごとに別の歩みを始めない（M30 の Codex 敵対的レビュー 5 回目）。
template <typename Copy>
Cell materialize_deep(Copy copy, const IrTerm& root) {
  using Frame = std::conditional_t<Copy::kShared, SharedFrame, PlainFrame>;
  std::vector<Frame> stack{Frame{&root, 0}};
  if constexpr (Copy::kShared) {
    if (copy.shared(root)) stack.back() = Frame{&copy.body(copy.index(root)), 0, copy.index(root)};
  }
  std::vector<Cell> parts;
  while (true) {
    const Frame top = stack.back();
    const std::size_t made = parts.size() - top.base;
    if (made < top.term->args.size()) {
      const IrTerm& next = top.term->args[made];
      if constexpr (Copy::kShared) {
        if (copy.shared(next)) {
          const std::uint32_t index = copy.index(next);
          if (const auto made_before = copy.find(index)) parts.push_back(*made_before);
          else stack.push_back(Frame{&copy.body(index), parts.size(), index});
          continue;
        }
      }
      if (const auto leaf = materialize_leaf(copy.machine(), next, copy.env())) parts.push_back(*leaf);
      else stack.push_back(Frame{&next, parts.size()});
      continue;
    }
    const Cell cell = close_compound(copy.machine(), *top.term, std::span<const Cell>(parts).subspan(top.base));
    if constexpr (Copy::kShared) {
      if (top.record != kNoRecord) copy.add(top.record, cell);
    }
    parts.resize(top.base);
    stack.pop_back();
    if (stack.empty()) return cell;
    parts.push_back(cell);
  }
}

// 共有の項は、その中身の根（複合項）を同じ深さで組む。Var で深さを足すと、共有しない写しより 1 段早く深い道（確保の
// ある materialize_deep）に入る。
template <typename Copy>
Cell materialize_at(Copy copy, const IrTerm& term, int depth) {
  const IrTerm* node = &term;
  [[maybe_unused]] std::uint32_t record = kNoRecord;
  if constexpr (Copy::kShared) {
    if (copy.shared(term)) {
      record = copy.index(term);
      if (const auto made_before = copy.find(record)) return *made_before;
      node = &copy.body(record);
    }
  }
  if (const auto leaf = materialize_leaf(copy.machine(), *node, copy.env())) return *leaf;
  if (depth >= kShallow) return materialize_deep(copy, term);
  Cell cell;
  if (node->tag == IrTag::List) {
    const Cell head = materialize_at(copy, node->args[0], depth + 1);
    const Cell tail = materialize_at(copy, node->args[1], depth + 1);
    cell = copy.machine().heap.list(head, tail);
  } else {
    std::vector<Cell> args;
    args.reserve(node->args.size());
    for (const IrTerm& arg : node->args) args.push_back(materialize_at(copy, arg, depth + 1));
    cell = close_compound(copy.machine(), *node, args);
  }
  if constexpr (Copy::kShared) {
    if (record != kNoRecord) copy.add(record, cell);
  }
  return cell;
}

}  // namespace

Cell materialize(Impl& machine, const IrTerm& root, const std::vector<Cell>& env) {
  return materialize_at(PlainCopy{&machine, &env}, root, 0);
}

// memo はこの呼び出しの間だけ持つ。
Cell materialize(Impl& machine, const IrTerm& root, const std::vector<Cell>& env, const SharedTable* shared) {
  if (shared == nullptr) return materialize(machine, root, env);
  const SharedMemo memo{machine, *shared};
  const SharedState state = state_of(machine, env, memo);
  return materialize_at(SharedCopy{&state}, root, 0);
}

Cell materialize(Impl& machine, const IrTerm& root, const std::vector<Cell>& env, SharedMemo& memo) {
  const SharedState state = state_of(machine, env, memo);
  return materialize_at(SharedCopy{&state}, root, 0);
}
}  // namespace mosa
