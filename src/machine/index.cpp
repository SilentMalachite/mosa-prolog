// 第1引数で節を選ぶ（ADR 0015）。線形に照らすときも索引を引くときも、clause_key と cell_key の鍵の等しさだけで
// 決める。8 節より少ない述語と多い述語で答えと順が変わらないよう、等しさの定めを一つにする。

#include "internal.hpp"

#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

namespace mosa {
namespace {

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
    case Tag::Atom: return IndexKey{KeyKind::Atom, static_cast<std::uint32_t>(first.raw() >> 3), 0};
    case Tag::Int:
    case Tag::IntBox:
      return IndexKey{KeyKind::Int, std::bit_cast<std::uint64_t>(machine.heap.integer_value(first)), 0};
    case Tag::Float: return IndexKey{KeyKind::Float, float_bits(machine.heap.float_value(first)), 0};
    case Tag::Lis: return IndexKey{KeyKind::List, 0, 0};
    case Tag::Str:
      return IndexKey{KeyKind::Fun, machine.heap.functor_atom(first), machine.heap.functor_arity(first)};
    default: return std::nullopt;
  }
}

bool selects(const Clause& clause, const std::optional<IndexKey>& wanted) {
  const auto key = clause_key(clause);
  return !key || (wanted && *key == *wanted);
}

// every なら鍵を見ずに見える節を全部選ぶ。
void scan_candidates(const Predicate& predicate, const std::optional<IndexKey>& wanted, std::uint64_t generation,
                     bool every, IdList& chosen) {
  for (const Clause& clause : predicate.clauses) {
    if (visible(clause, generation) && (every || selects(clause, wanted))) chosen.push_back(clause.id);
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

// 番号は繰り返さないので、節の列に無い番号が別の節を指すことは無い。無い番号と見えない節は飛ばす。
void take_visible(const Predicate& predicate, std::size_t id, std::uint64_t generation, IdList& chosen) {
  const auto position = clause_position(predicate, id);
  if (position && visible(predicate.clauses[*position], generation)) chosen.push_back(id);
}

// 組も keyless も番号の昇順なので、併せれば節の順になる。開いた走査の候補は昇順でなければならない（scan_reaches）。
void index_candidates(const Predicate& predicate, const std::optional<IndexKey>& wanted, std::uint64_t generation,
                      IdList& chosen) {
  if (!predicate.index.built) build_index(predicate);
  const FirstArgIndex& index = predicate.index;
  const IdBucket* bucket = nullptr;
  if (wanted) {
    const auto found = index.buckets.find(*wanted);
    if (found != index.buckets.end()) bucket = &found->second;
  }
  const std::size_t keyed = bucket == nullptr ? 0 : bucket->size();
  chosen.reserve(keyed + index.keyless.size());
  const IdBucket& keyless = index.keyless;
  std::size_t at = 0;
  std::size_t other = 0;
  while (at < keyed || other < keyless.size()) {
    const bool from_bucket = other >= keyless.size() || (at < keyed && (*bucket)[at] < keyless[other]);
    take_visible(predicate, from_bucket ? (*bucket)[at++] : keyless[other++], generation, chosen);
  }
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

std::optional<IndexKey> clause_key(const Clause& clause) {
  switch (clause.key) {
    case KeyKind::Atom: return IndexKey{KeyKind::Atom, clause.key_atom, 0};
    case KeyKind::Int: return IndexKey{KeyKind::Int, std::bit_cast<std::uint64_t>(clause.key_int), 0};
    case KeyKind::Float: return IndexKey{KeyKind::Float, float_bits(clause.key_float), 0};
    case KeyKind::List: return IndexKey{KeyKind::List, 0, 0};
    case KeyKind::Fun: return IndexKey{KeyKind::Fun, clause.key_atom, clause.key_arity};
    default: return std::nullopt;
  }
}

IdList key_candidates(Impl& machine, const Predicate& predicate, Cell first, std::uint64_t generation) {
  IdList chosen = id_list(machine);
  const Cell cell = machine.heap.deref(first);
  // 未束縛の第1引数はどの節の頭とも合う。cell_key は鍵の無いセルとして nullopt を返し、鍵の無い節だけを選ぶので、
  // そのまま引くと答えが欠ける。
  if (machine.heap.unbound(cell)) {
    scan_candidates(predicate, std::nullopt, generation, true, chosen);
  } else if (const auto wanted = cell_key(machine, cell); predicate.clauses.size() < kIndexMinClauses) {
    scan_candidates(predicate, wanted, generation, false, chosen);
  } else {
    index_candidates(predicate, wanted, generation, chosen);
  }
  return chosen;
}

}  // namespace mosa
