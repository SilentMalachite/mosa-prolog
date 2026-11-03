// ADR 0010 の改訂 3（レビュー M17）。ヒープの外の実行の状態を Heap::state に語で数える。選択点のスタック、extras、
// scan_log は持った要素の数の最大を、env と実行時に訳したゴールは 1 つずつを数え、歩みごとの安全点（gc.cpp）が
// 上限に数える。ゴールの木と env を作る関数と、extras に枠を積む関数もここに置き、数え忘れる道を作らない。

#include "internal.hpp"

#include <algorithm>
#include <array>
#include <iterator>
#include <utility>

namespace mosa {
namespace {

// make_shared の塊が持つ参照の数 2 つ。管理部の残り（仮想関数表など）はライブラリごとに違うので数えない。
constexpr std::size_t kRefCountBytes = 2 * sizeof(std::size_t);

// これより短いベクタの容量は返さない。返しても数語しか空かず、積むたびにまた伸びる。
constexpr std::size_t kKeptItems = 64;

constexpr std::size_t words_of(std::size_t bytes) { return (bytes + kHeapWordBytes - 1) / kHeapWordBytes; }

std::size_t env_words(const std::vector<Cell>& cells) {
  return words_of(sizeof(EnvCells) + kRefCountBytes + cells.capacity() * sizeof(Cell));
}

// 待っている子の数がこれを超えるまでは、作業の列をスタックの上に置く。call/1 に渡すふつうのゴールは収まる。
constexpr std::size_t kInlinePending = 32;

// 子は親の args の要素なので、args の容量で数える。長い連言の木は深いので、C++ の再帰にしない（ADR 0011）。
// 引数の無い子は積まない。小さい木で作業の列を確保すると、call/1 を回すループが 5% 遅くなった（敵対的レビュー A4）。
std::size_t tree_bytes(const IrTerm& root) {
  std::size_t bytes = 0;
  std::array<const IrTerm*, kInlinePending> inline_pending{};
  std::size_t inline_count = 0;
  std::vector<const IrTerm*> overflow;
  const IrTerm* node = &root;
  while (true) {
    bytes += node->args.capacity() * sizeof(IrTerm);
    for (const IrTerm& arg : node->args) {
      if (arg.args.empty()) continue;
      if (inline_count < kInlinePending) {
        inline_pending[inline_count++] = &arg;
      } else {
        overflow.push_back(&arg);
      }
    }
    if (!overflow.empty()) {
      node = overflow.back();
      overflow.pop_back();
    } else if (inline_count > 0) {
      node = inline_pending[--inline_count];
    } else {
      return bytes;
    }
  }
}

// 実行時に訳したゴールの木。節の本体は節の格納の物なので数えず、こちらだけが作った機械に語を数える。
struct RunGoal {
  RunGoal(Heap& heap, IrTerm tree)
      : term(std::move(tree)), owner(&heap), words(words_of(sizeof(RunGoal) + kRefCountBytes + tree_bytes(term))) {
    owner->add_state(words);
  }
  ~RunGoal() { owner->drop_state(words); }
  RunGoal(const RunGoal&) = delete;
  RunGoal& operator=(const RunGoal&) = delete;

  IrTerm term;
  Heap* owner;
  std::size_t words;
};

// 大きさの 4 倍を超えた容量を、大きさの 2 倍まで返す。積む側の倍々の伸びと同じ幅を残し、すぐには伸ばさない。
template <class T>
bool give_back(std::vector<T>& items) {
  if (items.capacity() <= kKeptItems || items.capacity() / 4 <= items.size()) return false;
  std::vector<T> kept;
  kept.reserve(std::max(items.size() * 2, kKeptItems));
  std::move(items.begin(), items.end(), std::back_inserter(kept));
  items.swap(kept);
  return true;
}

}  // namespace

// cells は作った後に長さを変えない（GC が中のセルを書き換えるだけ）ので、引く語は足した語と同じになる。
EnvCells::EnvCells(Heap& heap, std::vector<Cell> values) : cells(std::move(values)), owner(&heap) {
  owner->add_state(env_words(cells));
}

EnvCells::~EnvCells() { owner->drop_state(env_words(cells)); }

GoalRef make_goal(IrTerm term) { return std::make_shared<const IrTerm>(std::move(term)); }

GoalRef make_run_goal(Heap& heap, IrTerm term) {
  auto holder = std::make_shared<RunGoal>(heap, std::move(term));
  return GoalRef{holder, &holder->term};
}

GoalRef sub_goal(const GoalRef& goal, std::size_t index) { return GoalRef{goal, &goal->args[index]}; }

EnvRef make_env(Heap& heap, std::vector<Cell> cells) { return std::make_shared<EnvCells>(heap, std::move(cells)); }

void recount_vectors(Impl& machine) {
  CountedVectors& counted = machine.counted_vectors;
  counted.stack = std::max(counted.stack, machine.stack.size());
  counted.extras = std::max(counted.extras, machine.extras.size());
  counted.scan_log = std::max(counted.scan_log, machine.scan_log.size());
  const std::size_t words = words_of(counted.stack * sizeof(Entry) + counted.extras * sizeof(Extra) +
                                     counted.scan_log * sizeof(ScanRecord));
  machine.heap.drop_state(counted.words);
  machine.heap.add_state(words);
  counted.words = words;
}

void mark_parked_vectors(Impl& machine, std::size_t stack, std::size_t scan_log) {
  machine.counted_vectors.stack = stack;
  machine.counted_vectors.scan_log = scan_log;
  recount_vectors(machine);
}

// 中を指す参照を持ち続ける所は無く（積めば伸びて動くので、持てない）、入れ子の実行の中でも返してよい。返さないと
// 入れ子の間ずっと、前に切った選択点の分を数え続ける（敵対的レビュー A3）。
void give_back_vectors(Impl& machine) {
  CountedVectors& counted = machine.counted_vectors;
  if (give_back(machine.stack)) counted.stack = machine.stack.size();
  if (give_back(machine.extras)) counted.extras = machine.extras.size();
  if (give_back(machine.scan_log)) counted.scan_log = machine.scan_log.size();
  recount_vectors(machine);
}

std::uint64_t push_extra(Impl& machine, Extra extra) {
  machine.extras.push_back(std::move(extra));
  count_vectors(machine);
  const auto at = machine.heap.allocate_words(kFrameHead);
  machine.heap.set_word(at, 0);
  machine.heap.set_word(at + 1, machine.extras.size() - 1);
  machine.heap.set_word(at + 2, 0);
  machine.heap.set_word(at + 3, kNoFrame);
  return at;
}

}  // namespace mosa
