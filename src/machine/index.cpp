// 第1引数で節を選ぶ（ADR 0015 とその改訂）。線形に照らすときも索引を引くときも、clause_key と cell_key の鍵の
// 等しさだけで決める。8 節より少ない述語と多い述語で答えと順が変わらないよう、等しさの定めを一つにする。候補は列に
// せず、走査が一歩ごとに ClauseWalk で次の候補を探す（レビュー M20）。

#include "internal.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>

namespace mosa {
namespace {

// 指示付きの初期化子は、省いた欄に -Wmissing-field-initializers を出すコンパイラがあるので使わない。
IndexKey key_of(KeyKind kind, std::uint64_t value = 0, std::uint32_t arity = 0) {
  IndexKey key;
  key.kind = kind;
  key.value = value;
  key.arity = arity;
  return key;
}

// 0.0 と -0.0 は等しく、NaN どうしも等しい（mosac の生成コードの same_float と同じ）。ビット列を揃えて同じ鍵にする。
std::uint64_t float_bits(double value) {
  if (std::isnan(value)) return std::bit_cast<std::uint64_t>(std::numeric_limits<double>::quiet_NaN());
  if (value == 0.0) return 0;
  return std::bit_cast<std::uint64_t>(value);
}

// 呼び出しの束縛した第1引数の鍵。箱の整数も即値と同じ値の鍵になる。鍵の型でないセルは nullopt で、鍵の無い節
// だけに合う。
std::optional<IndexKey> cell_key(const Impl& machine, Cell first) {
  switch (first.tag()) {
    case Tag::Atom: return key_of(KeyKind::Atom, static_cast<std::uint32_t>(first.raw() >> 3));
    case Tag::Int:
    case Tag::IntBox:
      return key_of(KeyKind::Int, std::bit_cast<std::uint64_t>(machine.heap.integer_value(first)));
    case Tag::Float: return key_of(KeyKind::Float, float_bits(machine.heap.float_value(first)));
    case Tag::Lis: return key_of(KeyKind::List);
    case Tag::Str:
      return key_of(KeyKind::Fun, machine.heap.functor_atom(first), machine.heap.functor_arity(first));
    default: return std::nullopt;
  }
}

void build_index(const Predicate& predicate) {
  FirstArgIndex& index = predicate.index;
  index.buckets.clear();
  index.keyless.clear();
  for (const Clause& clause : predicate.clauses) {
    if (const auto key = clause_key(clause)) index.buckets[*key].push_back(clause.id);
    else index.keyless.push_back(clause.id);
  }
  index.built = true;
}

// 幅は番号の差。32 ビットを超える差は最大値に留める。範囲が短くなるだけで、その中の節が死んでいることは変わらない。
std::uint32_t span_to(std::size_t id, std::size_t target) {
  return static_cast<std::uint32_t>(std::min<std::size_t>(target - id, std::numeric_limits<std::uint32_t>::max()));
}

std::size_t hop_all(const Predicate& predicate, const Clause& clause) {
  return clause_from(predicate, clause.id + clause.skip_all);
}

// 位置 at の死んだ節から skip_all に沿って跳び、最初の生きた節の位置（無ければ clauses.size()）を返す。跳んだ元の
// 節の幅は跳んだ先までに延ばす（経路圧縮）。どの元からも跳んだ先までの節は全部死んでいるので、延ばしても正しい。
// 末尾まで死んでいれば先は next_id で、後で assertz する節の番号はそこから始まる。
std::size_t past_dead(const Predicate& predicate, std::size_t at) {
  const ClauseList& clauses = predicate.clauses;
  std::size_t end = at;
  while (end < clauses.size() && clauses[end].dead()) end = hop_all(predicate, clauses[end]);
  const std::size_t target = end < clauses.size() ? clauses[end].id : predicate.next_id;
  for (std::size_t walk = at; walk < end;) {
    const Clause& clause = clauses[walk];
    walk = hop_all(predicate, clause);
    clause.skip_all = span_to(clause.id, target);
  }
  return end;
}

// 組の at 番目の節。死んでいなければ null。索引は節の列と同じ番号を持つが、無い番号も死んでいない節として止める。
const Clause* dead_entry(const Predicate& predicate, const IdBucket& ids, std::size_t at) {
  if (at >= ids.size()) return nullptr;
  const auto position = clause_position(predicate, ids[at]);
  if (!position || !predicate.clauses[*position].dead()) return nullptr;
  return &predicate.clauses[*position];
}

// past_dead の組の版。skip_key に沿って同じ鍵の組（か鍵の無い節の列）の中を跳び、跳んだ元の幅を延ばす。返すのは
// 組の添字。ほかの鍵の生きた節が間に挟まっても、同じ鍵の死んだ節を一度に跳べる。
std::size_t past_dead_in(const Predicate& predicate, const IdBucket& ids, std::size_t at) {
  std::size_t end = at;
  for (const Clause* clause = dead_entry(predicate, ids, end); clause != nullptr;
       clause = dead_entry(predicate, ids, end)) {
    end = ids.lower_bound(clause->id + clause->skip_key);
  }
  const std::size_t target = end < ids.size() ? ids[end] : predicate.next_id;
  for (std::size_t walk = at; walk < end;) {
    const Clause* clause = dead_entry(predicate, ids, walk);
    if (clause == nullptr) break;
    walk = ids.lower_bound(clause->id + clause->skip_key);
    clause->skip_key = span_to(clause->id, target);
  }
  return end;
}

}  // namespace

bool IdBucket::pop_front_id(std::size_t id) {
  if (!prepended_.empty()) {
    if (prepended_.back() != id) return false;
    prepended_.pop_back();
    return true;
  }
  if (start_ == appended_.size() || appended_[start_] != id) return false;
  ++start_;
  // 外した分が半分を超えたら詰める。詰めて動かす番号は外した数より少ないので、ならせば 1 回 O(1)。
  if (start_ * 2 > appended_.size()) {
    appended_.erase(appended_.begin(), appended_.begin() + static_cast<std::ptrdiff_t>(start_));
    start_ = 0;
  }
  return true;
}

// appended が尽きたら最大の番号は prepended の最初の要素で、vector の先頭を消すと残りが全部ずれる。prepended を
// 昇順で appended へ移してから末尾を外す。番号は prepended から appended へ一度しか移らないので、ならせば O(1)。
bool IdBucket::pop_back_id(std::size_t id) {
  if (start_ == appended_.size() && !prepended_.empty()) {
    appended_.assign(prepended_.rbegin(), prepended_.rend());
    prepended_.clear();
    start_ = 0;
  }
  if (start_ == appended_.size() || appended_.back() != id) return false;
  appended_.pop_back();
  if (start_ == appended_.size()) {
    appended_.clear();
    start_ = 0;
  }
  return true;
}

void IdBucket::clear() {
  prepended_.clear();
  appended_.clear();
  start_ = 0;
}

std::size_t IndexKeyHash::operator()(const IndexKey& key) const noexcept {
  // 整数の鍵は連番になりやすい。種類とアリティを混ぜてから splitmix64 の仕上げで散らす。
  const std::uint64_t shape = (static_cast<std::uint64_t>(key.kind) << 32) | key.arity;
  std::uint64_t mixed = key.value ^ (shape * 0x9E3779B97F4A7C15ULL);
  mixed = (mixed ^ (mixed >> 30)) * 0xBF58476D1CE4E5B9ULL;
  mixed = (mixed ^ (mixed >> 27)) * 0x94D049BB133111EBULL;
  return static_cast<std::size_t>(mixed ^ (mixed >> 31));
}

std::size_t IdBucket::lower_bound(std::size_t id) const {
  // 新しい走査は番号 0 から始める。よく通る道なので二分探索を省く。
  if (empty() || id <= (*this)[0]) return 0;
  std::size_t low = 1;
  std::size_t high = size();
  while (low < high) {
    const std::size_t middle = low + (high - low) / 2;
    if ((*this)[middle] < id) low = middle + 1;
    else high = middle;
  }
  return low;
}

std::optional<IndexKey> clause_key(const Clause& clause) {
  switch (clause.key) {
    case KeyKind::Atom: return key_of(KeyKind::Atom, clause.key_atom);
    case KeyKind::Int: return key_of(KeyKind::Int, std::bit_cast<std::uint64_t>(clause.key_int));
    case KeyKind::Float: return key_of(KeyKind::Float, float_bits(clause.key_float));
    case KeyKind::List: return key_of(KeyKind::List);
    case KeyKind::Fun: return key_of(KeyKind::Fun, clause.key_atom, clause.key_arity);
    default: return std::nullopt;
  }
}

IndexKey first_key(const Impl& machine, Cell first) {
  return cell_key(machine, first).value_or(key_of(KeyKind::None));
}

bool scan_usable(const Clause& clause, ScanView view) {
  return view.retract ? clause.born <= view.generation && !clause.dead() : visible(clause, view.generation);
}

bool key_selects(const Clause& clause, const IndexKey& key) {
  if (key.kind == KeyKind::Any) return true;
  const auto own = clause_key(clause);
  return !own || *own == key;
}

// 8 節より少ない述語は索引を持たないので、鍵があっても節の列を歩く。どちらで歩いても選ぶ節と順は同じ。
ClauseWalk::ClauseWalk(const Predicate& predicate, const IndexKey& key, std::size_t from, ScanView view,
                       std::size_t hint)
    : predicate_(&predicate), key_(key), view_(view) {
  // retract はどの死んだ節も試さない。呼び出しと clause/2 は、その述語の最後の削除より後に始めた走査ならどの死んだ節も
  // 見えない。古い走査が跳ぶと、跳ぶ範囲にある、走査より後に消した節（まだ見える節）を欠く。
  skip_ = view.retract || view.generation >= predicate.last_death;
  by_index_ = key.kind != KeyKind::Any && predicate.clauses.size() >= kIndexMinClauses;
  if (!by_index_) {
    at_ = clause_from(predicate, from);
    return;
  }
  if (!predicate.index.built) build_index(predicate);
  const FirstArgIndex& index = predicate.index;
  if (const auto found = index.buckets.find(key); found != index.buckets.end()) bucket_ = &found->second;
  if (bucket_ != nullptr) {
    at_ = hint < bucket_->size() && (*bucket_)[hint] == from ? hint : bucket_->lower_bound(from);
  }
  keyless_ = index.keyless.lower_bound(from);
}

std::size_t ClauseWalk::next() { return by_index_ ? next_in_index() : next_in_list(); }

// 走査より後に生まれた節に当たったら、そこで尽きたことにする（敵対的レビュー A1）。歩く先の番号はどれも走査が始めに
// 見た節の番号以上で、asserta した節はその時の最小の番号より小さいので、その節は走査より後に assertz した節。
// assertz の番号と生まれた世代は一緒に増えるので、その先の節もみな走査より後に生まれ、どれも見えない。戻した写しも
// 写した時の番号と世代のままなので、この順は崩れない。歩き続けると、尽きるたびに後で足した節を 1 つずつなめる。
std::size_t ClauseWalk::next_in_list() {
  const ClauseList& clauses = predicate_->clauses;
  while (at_ < clauses.size()) {
    const std::size_t position = at_;
    const Clause& clause = clauses[position];
    if (clause.born > view_.generation) break;
    if (scan_usable(clause, view_) && key_selects(clause, key_)) {
      at_ = position + 1;
      return position;
    }
    at_ = skip_ && clause.dead() ? past_dead(*predicate_, position) : position + 1;
  }
  at_ = clauses.size();
  return clauses.size();
}

// 組も keyless も番号の昇順なので、小さい方から取れば節の順になる。索引に無い番号（節の列と食い違う索引）は飛ばす。
// 取った番号は両方の残りで最小なので、その節が走査より後に生まれたなら両方の残りもみなそう。
std::size_t ClauseWalk::next_in_index() {
  const Predicate& predicate = *predicate_;
  const IdBucket& keyless = predicate.index.keyless;
  const std::size_t keyed = bucket_ == nullptr ? 0 : bucket_->size();
  while (at_ < keyed || keyless_ < keyless.size()) {
    const bool from_bucket = keyless_ >= keyless.size() || (at_ < keyed && (*bucket_)[at_] < keyless[keyless_]);
    const IdBucket& ids = from_bucket ? *bucket_ : keyless;
    std::size_t& at = from_bucket ? at_ : keyless_;
    const auto position = clause_position(predicate, ids[at]);
    if (position && predicate.clauses[*position].born > view_.generation) break;
    if (position && scan_usable(predicate.clauses[*position], view_)) {
      hint_ = from_bucket ? at : kNoHint;
      ++at;
      return *position;
    }
    at = position && skip_ && predicate.clauses[*position].dead() ? past_dead_in(predicate, ids, at) : at + 1;
  }
  at_ = keyed;
  keyless_ = keyless.size();
  return predicate.clauses.size();
}

}  // namespace mosa
