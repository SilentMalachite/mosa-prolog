#include "mosa/operator_table.hpp"

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mosa {
namespace {

struct CoreOp {
  int priority;
  Specifier specifier;
  std::string_view name;
};

// 13211-1 の表7。`div` は Cor.2 の追加として残す。`|` `xor` は入れない。
// `:` は SPEC 第5節の限定ゴール、`as` は use_module/2 の別名に要る。表7からの逸脱である
// （iso-notes「Section 5 Operator :」と「Section 5 Operators meta_predicate and as」）。
// `*->` は組込の制御が既にある。書き出しに `->` と同じ中置が要る。
constexpr std::array<CoreOp, 45> k_iso_core{{
    {1200, Specifier::Xfx, ":-"},  {1200, Specifier::Xfx, "-->"}, {1200, Specifier::Fx, ":-"},
    {1200, Specifier::Fx, "?-"},   {1100, Specifier::Xfy, ";"},   {1050, Specifier::Xfy, "->"},
    {1050, Specifier::Xfy, "*->"}, {1150, Specifier::Fx, "meta_predicate"},
    {1000, Specifier::Xfy, ","},   {600, Specifier::Xfy, ":"},   {900, Specifier::Fy, "\\+"},   {700, Specifier::Xfx, "="},    {700, Specifier::Xfx, "as"},
    {700, Specifier::Xfx, "\\="},  {700, Specifier::Xfx, "=="},   {700, Specifier::Xfx, "\\=="},
    {700, Specifier::Xfx, "@<"},   {700, Specifier::Xfx, "@=<"},  {700, Specifier::Xfx, "@>"},
    {700, Specifier::Xfx, "@>="},  {700, Specifier::Xfx, "=.."},  {700, Specifier::Xfx, "is"},
    {700, Specifier::Xfx, "=:="},  {700, Specifier::Xfx, "=\\="}, {700, Specifier::Xfx, "<"},
    {700, Specifier::Xfx, "=<"},   {700, Specifier::Xfx, ">"},    {700, Specifier::Xfx, ">="},
    {500, Specifier::Yfx, "+"},    {500, Specifier::Yfx, "-"},    {500, Specifier::Yfx, "/\\"},
    {500, Specifier::Yfx, "\\/"},  {400, Specifier::Yfx, "*"},    {400, Specifier::Yfx, "/"},
    {400, Specifier::Yfx, "//"},   {400, Specifier::Yfx, "rem"},  {400, Specifier::Yfx, "mod"},
    {400, Specifier::Yfx, "<<"},   {400, Specifier::Yfx, ">>"},   {400, Specifier::Yfx, "div"},
    {200, Specifier::Xfx, "**"},   {200, Specifier::Xfy, "^"},    {200, Specifier::Fy, "+"},
    {200, Specifier::Fy, "-"},     {200, Specifier::Fy, "\\"},
}};

// 列挙子は Xf から Fy まで隙間なく並ぶ。この範囲外は BadSpecifier にする。
bool known_specifier(Specifier specifier) {
  const auto rank = static_cast<int>(specifier);
  return rank >= static_cast<int>(Specifier::Xf) && rank <= static_cast<int>(Specifier::Fy);
}

// 列挙の値が宣言順である。同優先度の lookup はこの順に依存する。
int specifier_rank(Specifier specifier) { return static_cast<int>(specifier); }

bool is_postfix(Specifier specifier) {
  return specifier == Specifier::Xf || specifier == Specifier::Yf;
}

bool is_infix(Specifier specifier) {
  return specifier == Specifier::Xfx || specifier == Specifier::Xfy || specifier == Specifier::Yfx;
}

bool is_prefix(Specifier specifier) {
  return specifier == Specifier::Fx || specifier == Specifier::Fy;
}

bool named_kind(const std::vector<OpDef>& ops, std::string_view name, bool (*kind)(Specifier)) {
  for (const OpDef& op : ops) {
    if (op.name == name && kind(op.specifier)) return true;
  }
  return false;
}

// 変更より先に拒否を決める。優先度 0 でもコンマと [] と {} は消さない。
// | の優先度 0 は削除なので拒否しない。中置の 1..1000 と前置と後置だけ拒否する。
// 中置と後置の併存は足すときだけ見る。優先度 0 は無い組を消しても Ok。
std::optional<OpResult> rejection(const std::vector<OpDef>& ops, int priority, Specifier specifier,
                                  std::string_view name) {
  if (priority < 0 || priority > 1200) return OpResult::BadPriority;
  if (!known_specifier(specifier)) return OpResult::BadSpecifier;
  if (name == ",") return OpResult::ModifyComma;
  if (name == "[]" || name == "{}") return OpResult::CreateEmpty;
  if (name == "|" && priority != 0 &&
      (is_prefix(specifier) || is_postfix(specifier) || priority <= 1000)) {
    return OpResult::CreateBar;
  }
  if (priority != 0 &&
      ((is_infix(specifier) && named_kind(ops, name, is_postfix)) ||
       (is_postfix(specifier) && named_kind(ops, name, is_infix)))) {
    return OpResult::InfixAndPostfix;
  }
  return std::nullopt;
}

void erase_pair(std::vector<OpDef>& ops, std::string_view name, Specifier specifier) {
  std::erase_if(ops, [&](const OpDef& op) { return op.name == name && op.specifier == specifier; });
}

bool same_class(Specifier left, Specifier right) {
  if (is_prefix(left)) return is_prefix(right);
  if (is_infix(left)) return is_infix(right);
  return is_postfix(left) && is_postfix(right);
}

// 一つの名前は前置、中置、後置をそれぞれ一つだけ持つ。
void erase_class(std::vector<OpDef>& ops, std::string_view name, Specifier specifier) {
  std::erase_if(ops, [&](const OpDef& op) {
    return op.name == name && same_class(op.specifier, specifier);
  });
}

void upsert(std::vector<OpDef>& ops, int priority, Specifier specifier, std::string name) {
  for (OpDef& op : ops) {
    if (op.name == name && op.specifier == specifier) {
      op.priority = priority;
      return;
    }
  }
  ops.push_back(OpDef{priority, specifier, std::move(name)});
}

}

OperatorTable OperatorTable::iso_core() {
  OperatorTable table;
  table.ops_.reserve(k_iso_core.size());
  for (const CoreOp& op : k_iso_core) {
    table.ops_.push_back(OpDef{op.priority, op.specifier, std::string(op.name)});
  }
  return table;
}

OpResult OperatorTable::define(int priority, Specifier specifier, std::string name) {
  if (const std::optional<OpResult> rejected = rejection(ops_, priority, specifier, name)) {
    return *rejected;
  }
  if (priority == 0) {
    erase_pair(ops_, name, specifier);
    return OpResult::Ok;
  }
  erase_class(ops_, name, specifier);
  upsert(ops_, priority, specifier, std::move(name));
  return OpResult::Ok;
}

std::vector<OpDef> OperatorTable::entries() const { return ops_; }

std::optional<OpDef> OperatorTable::stored(std::string_view name, Specifier specifier) const {
  for (const OpDef& op : ops_) {
    if (op.name == name && op.specifier == specifier) return op;
  }
  return std::nullopt;
}

std::optional<OpDef> OperatorTable::stored_class(std::string_view name, Specifier specifier) const {
  for (const OpDef& op : ops_) {
    if (op.name == name && same_class(op.specifier, specifier)) return op;
  }
  return std::nullopt;
}

void OperatorTable::erase_stored(std::string_view name, Specifier specifier) { erase_pair(ops_, name, specifier); }

void OperatorTable::put_stored(int priority, Specifier specifier, std::string name) {
  upsert(ops_, priority, specifier, std::move(name));
}

std::vector<OpDef> OperatorTable::lookup(std::string_view name) const {
  std::vector<OpDef> found;
  for (const OpDef& op : ops_) {
    if (op.name == name) found.push_back(op);
  }
  std::sort(found.begin(), found.end(), [](const OpDef& left, const OpDef& right) {
    if (left.priority != right.priority) return left.priority < right.priority;
    return specifier_rank(left.specifier) < specifier_rank(right.specifier);
  });
  return found;
}

}
