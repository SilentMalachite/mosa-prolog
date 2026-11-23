// 8.4.1 ==/2、\==/2、@</2、@=</2、@>/2、@>=/2（@term, @term）。エラー無し。
// 8.4.2 compare/3（?atom, @term, @term）
//   type_error(atom), domain_error(order)
// 巡回する項の順序は ADR 0029（7.2 は有限の項だけを定め、7.3.3 は巡回する項を定めない）。
// テスト: tests/pl/order.pl、tests/cpp/bigint_heap_test.cpp、tests/cpp/pair_limit_test.cpp

#include "../core/pair_memo.hpp"
#include "../machine/internal.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace mosa {
namespace {

// 7.2: 変数、浮動小数点数、整数、アトム、複合項の順。Big も整数の順位で、int64 の整数と値で並ぶ。
int rank_of(const Impl& machine, Cell cell) {
  if (machine.heap.unbound(cell)) return 0;
  if (cell.tag() == Tag::Float) return 1;
  if (is_integer_tag(cell.tag())) return 2;
  if (cell.tag() == Tag::Atom) return 3;
  return 4;
}

// 深さ優先の歩みが、比べている途中の組にまた会ったときの印（ADR 0029）。2 つの項の深さ優先の節点の並びは、そこから
// いつまでも等しく続く。比べた答え（-1、0、1）とは別の値で、compare_terms の外には出さない。
constexpr int kEndless = 2;
// 比べ始めた同じ関数子の組を全部覚える。小さい比較は線形の表で済ませ、この数を超えたら探すのが定数時間の表へ移す。
// 深い項でも比較が 2 乗の時間にならない。
constexpr std::size_t kFewPairs = 256;
// この深さまでは C++ の再帰で子を比べる。より深い部分木は、その場で手で積んで比べ終えてから戻る（ADR 0011）。
constexpr int kShallow = 64;

// 入った組が、新しいか、比べている途中か、比べ終えて等しかったか。
enum class PairVisit { New, Open, Done };

// 組ごとに入った順の番号を覚え、今の道（比べている途中の組）の番号を昇順に積む。比べ終えた組にまた会えば等しい（共有
// した部分項）。途中の組にまた会えば、深さ優先の比べは終わらない（ADR 0029）。途中かは道の番号を二分探索して見る。
// 比べ終えるときは道から外すだけで、表を引き直さない。比べ終えた印を表に書くと、大きな写しの == が 3 割遅くなった。
class PairSeen {
 public:
  // collect があれば、飛ばした共有の部分項をそこに集める。
  explicit PairSeen(const Heap& heap, std::vector<Cell>* collect = nullptr) : heap_(heap), collect_(collect) {}

  PairVisit enter(std::uint64_t left, std::uint64_t right) {
    const std::optional<std::uint64_t> known = find_or_add(left, right);
    if (!known) {
      path_.push_back(next_++);
      return PairVisit::New;
    }
    return std::binary_search(path_.begin(), path_.end(), *known) ? PairVisit::Open : PairVisit::Done;
  }

  // 入った組の子を全部比べて等しかった。深さ優先なので、それは道のいちばん上の組である。
  void finish() { path_.pop_back(); }

  // 2 つの項が共有する（同じセルの）複合項を、等しいとして飛ばした。違いで決めたときに、巡回しないかを確かめる。
  // 同じセルは組の子の欄ごとに来る（f(S, …, S, p(1)) と f(S, …, S, p(2)) の 1 組で S がアリティ − 1 回）。続けて来た
  // 同じセルは足さず、長さが倍になるたびに並べて重なりを捨てる。違う共有のセルはヒープのセルより多くないので、集めた
  // 列はヒープの語の数の程度に収まる（レビュー RV-m12）。any_cyclic は列の順に依らない。
  void share(Cell cell) {
    shared_ = true;
    if (collect_ == nullptr) return;
    if (!collect_->empty() && collect_->back().raw() == cell.raw()) return;
    collect_->push_back(cell);
    if (collect_->size() < compact_at_) return;
    std::sort(collect_->begin(), collect_->end(), [](Cell left, Cell right) { return left.raw() < right.raw(); });
    const auto last = std::unique(collect_->begin(), collect_->end(),
                                  [](Cell left, Cell right) { return left.raw() == right.raw(); });
    collect_->erase(last, collect_->end());
    compact_at_ = std::max(compact_at_, collect_->size() * 2);
  }
  [[nodiscard]] bool shared() const { return shared_; }
  // 違いで決めた節点の深さ（根が 0）。
  void decide(std::size_t depth) { decided_ = depth; }
  [[nodiscard]] std::size_t decided() const { return decided_; }

 private:
  struct Entry {
    std::uint64_t left;
    std::uint64_t right;
    std::uint64_t number;
  };

  // 前に入った組ならその番号。新しい組は今の番号で覚えて空を返す。
  std::optional<std::uint64_t> find_or_add(std::uint64_t left, std::uint64_t right) {
    if (!many_.active()) {
      for (std::size_t index = 0; index < few_count_; ++index) {
        if (few_[index].left == left && few_[index].right == right) return few_[index].number;
      }
      if (few_count_ < few_.size()) {
        few_[few_count_++] = {left, right, next_};
        return std::nullopt;
      }
      for (const Entry& item : few_) many_.enter(heap_, item.left, item.right, item.number);
    }
    return many_.enter(heap_, left, right, next_);
  }

  const Heap& heap_;
  std::vector<Cell>* collect_;
  // collect_ をこの長さで詰める。
  std::size_t compact_at_ = 1024;
  // 小さい比較はヒープの外に確保しない。
  std::array<Entry, kFewPairs> few_;
  std::size_t few_count_ = 0;
  std::vector<std::uint64_t> path_;
  std::uint64_t next_ = 0;
  bool shared_ = false;
  std::size_t decided_ = 0;
  // 使っているヒープの語の数を超えた組は上限に数える（PairNumbers、ADR 0010 の改訂 4）。
  PairNumbers many_;
};

// 正規形の絶対値の語（下位が先、最上位の語は 0 でない）。語の数が違えば多い方が大きいので、同じ数のときだけ上位から比べる。
int compare_magnitudes(std::span<const std::uint64_t> a, std::span<const std::uint64_t> b) {
  if (a.size() != b.size()) return a.size() < b.size() ? -1 : 1;
  for (std::size_t index = a.size(); index > 0; --index) {
    if (a[index - 1] != b[index - 1]) return a[index - 1] < b[index - 1] ? -1 : 1;
  }
  return 0;
}

// 整数どうしの標準順序。Big は int64 に入らない（ADR 0025 の正規形）ので、片方だけが Big なら Big の符号で決まる。
// 両方が Big なら符号、語の数、語の順に箱の中で比べ、BigInt を作らない。msort/2 や keysort/2 が比べるたびに確保しない
// ためである（最終レビューの Minor 4）。負どうしは絶対値の大きい方が小さい。
int compare_integers(const Impl& machine, Cell left, Cell right) {
  const bool left_big = left.tag() == Tag::Big;
  const bool right_big = right.tag() == Tag::Big;
  if (!left_big && !right_big) {
    const auto a = machine.heap.integer_value(left);
    const auto b = machine.heap.integer_value(right);
    return a < b ? -1 : (a > b ? 1 : 0);
  }
  if (!right_big) return machine.heap.big_negative(left) ? -1 : 1;
  if (!left_big) return machine.heap.big_negative(right) ? 1 : -1;
  const bool negative = machine.heap.big_negative(left);
  if (negative != machine.heap.big_negative(right)) return negative ? -1 : 1;
  const int magnitude = compare_magnitudes(machine.heap.big_magnitude(left), machine.heap.big_magnitude(right));
  return negative ? -magnitude : magnitude;
}

int compare_scalar(const Impl& machine, Cell left, Cell right, int rank) {
  if (rank == 0) return left.raw() < right.raw() ? -1 : (left.raw() > right.raw() ? 1 : 0);
  if (rank == 1) {
    const double a = machine.heap.float_value(left);
    const double b = machine.heap.float_value(right);
    const bool a_nan = std::isnan(a);
    const bool b_nan = std::isnan(b);
    if (a_nan || b_nan) return a_nan == b_nan ? 0 : (a_nan ? 1 : -1);
    if (a < b) return -1;
    if (a > b) return 1;
    // -0.0 と 0.0 は値が等しいが別の項なので、== と同じく区別し、-0.0 を先にする（iso-notes 7.2.2 の決定 A）。
    if (std::signbit(a) != std::signbit(b)) return std::signbit(a) ? -1 : 1;
    return 0;
  }
  if (rank == 2) return compare_integers(machine, left, right);
  const auto a = machine.atoms.name(static_cast<std::uint32_t>(left.raw() >> 3));
  const auto b = machine.atoms.name(static_cast<std::uint32_t>(right.raw() >> 3));
  if (a < b) return -1;
  if (a > b) return 1;
  return 0;
}

// 組の頭だけを比べる。同じ関数子の複合項で子を比べる必要があるときだけその数を返し、それ以外は order に答えを置いて
// 0 を返す。同じセルは等しい。
std::uint16_t compare_head(const Impl& machine, Cell& left, Cell& right, int& order) {
  order = 0;
  left = machine.heap.deref(left);
  right = machine.heap.deref(right);
  if (left.raw() == right.raw() && left.tag() != Tag::Float) return 0;
  const int left_rank = rank_of(machine, left);
  const int right_rank = rank_of(machine, right);
  if (left_rank != right_rank) {
    order = left_rank < right_rank ? -1 : 1;
    return 0;
  }
  if (left_rank < 4) {
    order = compare_scalar(machine, left, right, left_rank);
    return 0;
  }
  const bool left_list = left.tag() == Tag::Lis;
  const bool right_list = right.tag() == Tag::Lis;
  const auto left_arity = left_list ? std::uint16_t{2} : machine.heap.functor_arity(left);
  const auto right_arity = right_list ? std::uint16_t{2} : machine.heap.functor_arity(right);
  if (left_arity != right_arity) {
    order = left_arity < right_arity ? -1 : 1;
    return 0;
  }
  // 同じアトムなら名前も同じなので、文字列を比べない。
  if (!left_list && !right_list && machine.heap.functor_atom(left) == machine.heap.functor_atom(right)) {
    return left_arity;
  }
  if (left_list && right_list) return left_arity;
  const std::string_view left_text =
      left_list ? std::string_view{"."} : machine.atoms.name(machine.heap.functor_atom(left));
  const std::string_view right_text =
      right_list ? std::string_view{"."} : machine.atoms.name(machine.heap.functor_atom(right));
  if (left_text != right_text) {
    order = left_text < right_text ? -1 : 1;
    return 0;
  }
  return left_arity;
}

Cell child_of(const Impl& machine, Cell cell, std::uint16_t index) {
  if (cell.tag() == Tag::Lis) return index == 0 ? machine.heap.list_head(cell) : machine.heap.list_tail(cell);
  return machine.heap.arg(cell, index);
}

bool is_compound(Cell cell) { return cell.tag() == Tag::Str || cell.tag() == Tag::Lis; }

std::uint16_t arity_of(const Impl& machine, Cell cell) {
  return cell.tag() == Tag::Lis ? std::uint16_t{2} : machine.heap.functor_arity(cell);
}

// 深さ優先の歩みが組に入る。新しい組なら子の数を返す。比べ終えて等しかった組は 0（等しい）、比べている途中の組なら
// order に kEndless を置いて 0 を返す。同じセルの複合項は等しいとして飛ばし、覚えておく。
std::uint16_t enter_pair(const Impl& machine, Cell& left, Cell& right, PairSeen& seen, std::size_t depth, int& order) {
  const std::uint16_t arity = compare_head(machine, left, right, order);
  if (arity == 0) {
    if (order == 0 && left.raw() == right.raw() && is_compound(left)) seen.share(left);
    if (order != 0) seen.decide(depth);
    return 0;
  }
  const PairVisit visit = seen.enter(left.raw(), right.raw());
  if (visit == PairVisit::New) return arity;
  if (visit == PairVisit::Open) order = kEndless;
  return 0;
}

// 部分木を手で積んで比べる。枠は組と次に比べる子を持ち、子を全部比べ終えた組を道から外す。比べる順は前の再帰と
// 同じになる。
int compare_deep(const Impl& machine, Cell left, Cell right, PairSeen& seen, std::size_t depth) {
  struct Frame {
    Cell left;
    Cell right;
    std::uint16_t next;
    std::uint16_t arity;
  };
  int order = 0;
  const std::uint16_t arity = enter_pair(machine, left, right, seen, depth, order);
  if (arity == 0) return order;
  std::vector<Frame> frames{{left, right, 0, arity}};
  while (!frames.empty()) {
    Frame& top = frames.back();
    if (top.next == top.arity) {
      seen.finish();
      frames.pop_back();
      continue;
    }
    Cell a = child_of(machine, top.left, top.next);
    Cell b = child_of(machine, top.right, top.next);
    ++top.next;
    const std::uint16_t kids = enter_pair(machine, a, b, seen, depth + frames.size(), order);
    if (order != 0) return order;
    if (kids != 0) frames.push_back({a, b, 0, kids});
  }
  return 0;
}

// 1 つ目の鍵（ADR 0029）。展開した木を深さ優先に、左から右に比べる。
int compare_seen(const Impl& machine, Cell left, Cell right, PairSeen& seen, int depth) {
  if (depth >= kShallow) return compare_deep(machine, left, right, seen, static_cast<std::size_t>(depth));
  int order = 0;
  const std::uint16_t arity = enter_pair(machine, left, right, seen, static_cast<std::size_t>(depth), order);
  for (std::uint16_t index = 0; index < arity && order == 0; ++index) {
    order = compare_seen(machine, child_of(machine, left, index), child_of(machine, right, index), seen, depth + 1);
  }
  if (arity != 0 && order == 0) seen.finish();
  return order;
}

// 幅優先の歩みが積んだ組。PairSeen と同じく、小さい比較は線形の表で済ませる。大きい表は待ち行列の 1 組の分も数える。
class PairOnce {
 public:
  explicit PairOnce(const Heap& heap) : heap_(heap) {}

  // 新しい組なら覚えて true。
  bool insert(std::uint64_t left, std::uint64_t right) {
    if (!many_.active()) {
      for (const auto& item : few_) {
        if (item.first == left && item.second == right) return false;
      }
      if (few_.size() < kFewPairs) {
        few_.emplace_back(left, right);
        return true;
      }
      for (const auto& item : few_) many_.insert(heap_, item.first, item.second);
    }
    return many_.insert(heap_, left, right);
  }

 private:
  const Heap& heap_;
  std::vector<std::pair<std::uint64_t, std::uint64_t>> few_;
  PairMemo many_{sizeof(std::pair<Cell, Cell>)};
};

// 2 つ目の鍵（ADR 0029）。与えられた 2 つの項を上から段ごとに、左から右に比べ、最初の違いで決める。待ち行列は前から
// 読むだけなので、段の順、段の中では左からの順に並ぶ。同じ組は 2 度積まない。共有の部分項（同じセル）の中には入らない。
// 深さ limit より深い節点は比べない（違いが無ければ 0）。
constexpr std::size_t kAnyDepth = std::numeric_limits<std::size_t>::max();

int compare_wide(const Impl& machine, Cell left, Cell right, std::size_t limit = kAnyDepth) {
  PairOnce queued{machine.heap};
  left = machine.heap.deref(left);
  right = machine.heap.deref(right);
  queued.insert(left.raw(), right.raw());
  std::vector<std::pair<Cell, Cell>> queue{{left, right}};
  std::size_t depth = 0;
  std::size_t level_end = 1;
  for (std::size_t at = 0; at < queue.size(); ++at) {
    if (at == level_end) {
      if (++depth > limit) return 0;
      level_end = queue.size();
    }
    auto [a, b] = queue[at];
    int order = 0;
    const std::uint16_t arity = compare_head(machine, a, b, order);
    if (order != 0) return order;
    // 深さ limit の段の子は比べないので積まない。前は積んでから段の境で捨て、その段の子の数だけ待ち行列と組の表が
    // 伸びた（レビュー R-L03）。
    if (depth == limit) continue;
    for (std::uint16_t index = 0; index < arity; ++index) {
      const Cell x = machine.heap.deref(child_of(machine, a, index));
      const Cell y = machine.heap.deref(child_of(machine, b, index));
      if (queued.insert(x.raw(), y.raw())) queue.emplace_back(x, y);
    }
  }
  return 0;
}

// roots のどれかから閉路に着くか。根ごとに深さ優先で辿り、辿っている途中の複合項にまた会えば閉路である。辿り終えた
// 複合項は共有されていても辿り直さない。表は PairSeen に (セル, 0) の組として覚え、上限に数える。
bool any_cyclic(const Impl& machine, const std::vector<Cell>& roots) {
  struct Frame {
    Cell term;
    std::uint16_t next;
    std::uint16_t arity;
  };
  PairSeen marks{machine.heap};
  std::vector<Frame> frames;
  const auto enter = [&](Cell cell) {
    cell = machine.heap.deref(cell);
    if (!is_compound(cell)) return false;
    const PairVisit visit = marks.enter(cell.raw(), 0);
    if (visit == PairVisit::New) frames.push_back({cell, 0, arity_of(machine, cell)});
    return visit == PairVisit::Open;
  };
  for (const Cell root : roots) {
    if (enter(root)) return true;
    while (!frames.empty()) {
      Frame& top = frames.back();
      if (top.next == top.arity) {
        marks.finish();
        frames.pop_back();
        continue;
      }
      const Cell child = child_of(machine, top.term, top.next);
      ++top.next;
      if (enter(child)) return true;
    }
  }
  return false;
}

// 共有の部分項を飛ばした後に、深さ depth の違いで order と決めた。飛ばした部分項が巡回すれば、1 つ目の鍵はそこで
// 先へ進まず、答えは 2 つ目の鍵のものになる（ADR 0029）。深さ優先の歩みは違いより左をもう等しいと見たので、2 つ目の鍵が
// 違う答えになるのは、depth より浅い所に違いがあるときだけである。そこを比べて同じ答えか違いが無ければ、巡回しても
// しなくても答えは同じなので、共有の部分項を辿らない。毎回辿ると、大きな部分項を共有する項の並べ替えが 300 倍遅く
// なった（p(C, M) を 1 万個、C は 1,000 要素のリスト）。食い違ったときだけ、歩み直して飛ばした部分項を集める。
int settle_shared(const Impl& machine, Cell left, Cell right, int order, std::size_t depth) {
  if (depth <= 1) return order;
  const int wide = compare_wide(machine, left, right, depth - 1);
  if (wide == 0 || wide == order) return order;
  std::vector<Cell> passed;
  {
    PairSeen seen{machine.heap, &passed};
    compare_seen(machine, left, right, seen, 0);
  }
  return any_cyclic(machine, passed) ? wide : order;
}

// 1 つ目の鍵がいつまでも等しく続くときだけ、2 つ目の鍵で比べる。深さ優先の組の表は、幅優先に移る前に返す。
// 等しく終わった比較は、共有の部分項を確かめない（X == X は辿らない）。
int compare_terms(const Impl& machine, Cell left, Cell right) {
  int order = 0;
  bool shared = false;
  std::size_t depth = 0;
  {
    PairSeen seen{machine.heap};
    order = compare_seen(machine, left, right, seen, 0);
    shared = seen.shared();
    depth = seen.decided();
  }
  if (order == kEndless) return compare_wide(machine, left, right);
  if (order == 0 || !shared) return order;
  return settle_shared(machine, left, right, order, depth);
}


void succeed(Impl& machine, const Goal& goal) { machine.ip = goal.cont; }

bool ordered(Impl& machine, const Goal& goal, int mode) {
  const int order = compare_terms(machine, goal.arg(machine, 0), goal.arg(machine, 1));
  const bool yes = (mode == -2 && order < 0) || (mode == -1 && order <= 0) || (mode == 0 && order == 0) ||
                   (mode == 1 && order >= 0) || (mode == 2 && order > 0);
  if (yes) succeed(machine, goal);
  else machine.failed = true;
  return true;
}

void run_compare(Impl& machine, const Goal& goal) {
  const Cell order_cell = machine.heap.deref(goal.arg(machine, 0));
  if (!machine.heap.unbound(order_cell) && order_cell.tag() != Tag::Atom) {
    raise_formal(machine, formal_type(machine, "atom", order_cell), goal.name, goal.arity);
    return;
  }
  if (order_cell.tag() == Tag::Atom) {
    const auto text = machine.atoms.name(static_cast<std::uint32_t>(order_cell.raw() >> 3));
    if (text != "<" && text != "=" && text != ">") {
      raise_formal(machine, formal_domain(machine, "order", order_cell), goal.name, goal.arity);
      return;
    }
  }
  const int order = compare_terms(machine, goal.arg(machine, 1), goal.arg(machine, 2));
  const char* text = order < 0 ? "<" : (order > 0 ? ">" : "=");
  const Cell symbol = machine.heap.atom(machine.atoms.intern(text));
  if (!do_unify(machine, goal.arg(machine, 0), symbol)) machine.failed = true;
  else succeed(machine, goal);
}

}  // namespace

int compare_cells(const Impl& machine, Cell left, Cell right) { return compare_terms(machine, left, right); }

bool run_order(Impl& machine, const Goal& goal) {
  const auto name = machine.atoms.name(goal.name);
  if (name == "@<" && goal.arity == 2) return ordered(machine, goal, -2);
  if (name == "@=<" && goal.arity == 2) return ordered(machine, goal, -1);
  if (name == "==" && goal.arity == 2) return ordered(machine, goal, 0);
  if (name == "@>=" && goal.arity == 2) return ordered(machine, goal, 1);
  if (name == "@>" && goal.arity == 2) return ordered(machine, goal, 2);
  if (name == "\\==" && goal.arity == 2) {
    const int order = compare_terms(machine, goal.arg(machine, 0), goal.arg(machine, 1));
    if (order != 0) succeed(machine, goal);
    else machine.failed = true;
    return true;
  }
  if (name == "compare" && goal.arity == 3) {
    run_compare(machine, goal);
    return true;
  }
  return false;
}

}
