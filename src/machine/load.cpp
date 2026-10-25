#include "internal.hpp"

#include "mosa/reader.hpp"
#include "mosa/syntax_error.hpp"
#include "mosa/writer.hpp"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace mosa {
namespace {

struct Snapshot {
  std::uint32_t name = 0;
  std::uint16_t arity = 0;
  std::vector<Clause> clauses;
  bool created = false;
  bool dynamic = false;
};

// この読み込みの中だけで隙間を許す。述語ベクタには入れない。宣言は手続きを作らない。
struct Apart {
  std::uint32_t name = 0;
  std::uint16_t arity = 0;
};

std::string reason_text(SyntaxReason reason) {
  switch (reason) {
    case SyntaxReason::InvalidUtf8: return "不正な UTF-8";
    case SyntaxReason::IllegalCharacter: return "使えない文字";
    case SyntaxReason::UnterminatedQuote: return "引用が閉じていない";
    case SyntaxReason::UnterminatedComment: return "コメントが閉じていない";
    case SyntaxReason::BadEscape: return "不正なエスケープ";
    case SyntaxReason::IntegerOverflow: return "整数が範囲を超えている";
    case SyntaxReason::UnexpectedToken: return "予期しないトークン";
    case SyntaxReason::MaxArity: return "引数が多すぎる";
    case SyntaxReason::IncompleteTerm: return "項が閉じていません";
  }
  return "予期しないトークン";
}

void fail_load(Impl& machine, std::string message) {
  machine.diagnostic = std::move(message);
  machine.syntax_diag = false;
}

bool same_pred(const Predicate& predicate, std::uint32_t name, std::uint16_t arity) {
  return predicate.name == name && predicate.arity == arity;
}

Predicate* find_pred(Impl& machine, std::uint32_t name, std::uint16_t arity) {
  for (Predicate& predicate : machine.predicates) {
    if (same_pred(predicate, name, arity)) return &predicate;
  }
  return nullptr;
}

std::uint32_t head_name(const Clause& clause) { return clause.head.atom; }

std::uint16_t head_arity(const Clause& clause) {
  if (clause.head.tag == IrTag::Atom) return 0;
  return clause.head.arity;
}

bool is_rule_arrow(const Heap& heap, Cell cell) {
  cell = heap.deref(cell);
  return cell.tag() == Tag::Str && heap.functor_arity(cell) == 2 && heap.atoms().name(heap.functor_atom(cell)) == "-->";
}

std::optional<Specifier> specifier_of(std::string_view text) {
  if (text == "xf") return Specifier::Xf;
  if (text == "yf") return Specifier::Yf;
  if (text == "xfx") return Specifier::Xfx;
  if (text == "xfy") return Specifier::Xfy;
  if (text == "yfx") return Specifier::Yfx;
  if (text == "fx") return Specifier::Fx;
  if (text == "fy") return Specifier::Fy;
  return std::nullopt;
}

std::string op_failure(OpResult result) {
  switch (result) {
    case OpResult::BadPriority: return "優先度が範囲外です";
    case OpResult::BadSpecifier: return "演算子の指定子が不正です";
    case OpResult::ModifyComma: return "コンマの演算子は変更できません";
    case OpResult::CreateEmpty: return "空リストや空の中括弧は演算子にできません";
    case OpResult::InfixAndPostfix: return "中置と後置は同時に宣言できません";
    case OpResult::CreateBar: return "この縦棒は演算子にできません";
    case OpResult::Ok: return {};
  }
  return "演算子を宣言できません";
}

bool define_name(Impl& machine, int priority, Specifier specifier, std::string name) {
  const OpResult result = machine.operators.define(priority, specifier, std::move(name));
  if (result == OpResult::Ok) return true;
  fail_load(machine, op_failure(result));
  return false;
}

bool define_op_names(Impl& machine, int priority, Specifier specifier, Cell names) {
  names = machine.heap.deref(names);
  if (names.tag() == Tag::Atom) {
    return define_name(machine, priority, specifier, std::string(machine.atoms.name(static_cast<std::uint32_t>(names.raw() >> 3))));
  }
  const auto nil = machine.atoms.intern("[]");
  while (names.tag() == Tag::Lis) {
    const Cell head = machine.heap.deref(machine.heap.list_head(names));
    if (head.tag() != Tag::Atom) {
      fail_load(machine, "演算子名がアトムではありません");
      return false;
    }
    if (!define_name(machine, priority, specifier,
                     std::string(machine.atoms.name(static_cast<std::uint32_t>(head.raw() >> 3))))) {
      return false;
    }
    names = machine.heap.deref(machine.heap.list_tail(names));
  }
  if (!(names.tag() == Tag::Atom && static_cast<std::uint32_t>(names.raw() >> 3) == nil)) {
    fail_load(machine, "演算子名がアトムではありません");
    return false;
  }
  return true;
}

bool apply_op(Impl& machine, Cell directive) {
  const Cell body = machine.heap.deref(machine.heap.arg(directive, 0));
  if (!(body.tag() == Tag::Str && machine.heap.functor_arity(body) == 3 &&
        machine.atoms.name(machine.heap.functor_atom(body)) == "op")) {
    return false;
  }
  const Cell priority_cell = machine.heap.deref(machine.heap.arg(body, 0));
  const Cell spec_cell = machine.heap.deref(machine.heap.arg(body, 1));
  if (priority_cell.tag() != Tag::Int && priority_cell.tag() != Tag::IntBox) return false;
  if (spec_cell.tag() != Tag::Atom) return false;
  const auto specifier = specifier_of(machine.atoms.name(static_cast<std::uint32_t>(spec_cell.raw() >> 3)));
  if (!specifier) {
    fail_load(machine, op_failure(OpResult::BadSpecifier));
    return false;
  }
  const auto priority64 = machine.heap.integer_value(priority_cell);
  if (priority64 < 0 || priority64 > 1200) {
    fail_load(machine, op_failure(OpResult::BadPriority));
    return false;
  }
  return define_op_names(machine, static_cast<int>(priority64), *specifier, machine.heap.arg(body, 2));
}

bool allows_gap(const std::vector<Apart>& apart, std::uint32_t name, std::uint16_t arity) {
  for (const Apart& item : apart) {
    if (item.name == name && item.arity == arity) return true;
  }
  return false;
}

bool add_clause(Impl& machine, Clause clause, std::size_t& last, const std::vector<Apart>& apart) {
  const auto name = head_name(clause);
  const auto arity = head_arity(clause);
  std::size_t index = machine.predicates.size();
  for (std::size_t cursor = 0; cursor < machine.predicates.size(); ++cursor) {
    if (same_pred(machine.predicates[cursor], name, arity)) index = cursor;
  }
  if (index == machine.predicates.size()) {
    machine.predicates.push_back(Predicate{});
    machine.predicates.back().name = name;
    machine.predicates.back().arity = arity;
  }
  Predicate& predicate = machine.predicates[index];
  // 宣言の無い述語は、これまでと同じ診断のまま失敗する。
  if (predicate.seen && last != index && !allows_gap(apart, name, arity)) {
    fail_load(machine, "述語 " + std::string(machine.atoms.name(name)) + "/" + std::to_string(arity) +
                            " の節が離れている");
    return false;
  }
  if (!predicate.seen && !predicate.clauses.empty()) {
    bool busy = false;
    for (const Entry& entry : machine.stack) {
      if (entry.pick != Pick::Cleanup && entry.pick != Pick::Catch) busy = true;
    }
    if (busy) {
      fail_load(machine, "実行中の述語は置き換えられない");
      return false;
    }
    predicate.clauses.clear();
    // 宣言が無い読み込みは、以前動的でも置き換えた述語を静的に戻す。
    if (!predicate.dynamic_declared) predicate.dynamic = false;
  }
  predicate.seen = true;
  last = index;
  predicate.clauses.push_back(std::move(clause));
  return true;
}

void remember_pred(Impl& machine, std::vector<Snapshot>& snapshots, std::uint32_t name, std::uint16_t arity) {
  const bool known = std::any_of(snapshots.begin(), snapshots.end(), [&](const Snapshot& shot) {
    return shot.name == name && shot.arity == arity;
  });
  if (known) return;
  Predicate* predicate = find_pred(machine, name, arity);
  Snapshot shot;
  shot.name = name;
  shot.arity = arity;
  shot.created = predicate == nullptr;
  if (predicate != nullptr) {
    shot.clauses = predicate->clauses;
    shot.dynamic = predicate->dynamic;
  }
  snapshots.push_back(std::move(shot));
}

void remember(Impl& machine, std::vector<Snapshot>& snapshots, const Clause& clause) {
  remember_pred(machine, snapshots, head_name(clause), head_arity(clause));
}

void rollback(Impl& machine, const std::vector<Snapshot>& snapshots, std::size_t previous_size) {
  for (const Snapshot& shot : snapshots) {
    if (shot.created) continue;
    Predicate* predicate = find_pred(machine, shot.name, shot.arity);
    if (predicate != nullptr) {
      predicate->clauses = shot.clauses;
      predicate->dynamic = shot.dynamic;
    }
  }
  machine.predicates.resize(previous_size);
}

struct SavedFlags {
  DoubleQuotes quotes = DoubleQuotes::Codes;
  bool unknown_fail = false;
  bool float_overflow_error = true;
  bool float_zero_div_error = true;
  bool float_undefined_error = true;
  bool char_conv_on = false;
  std::vector<std::pair<char32_t, char32_t>> char_map;
};

SavedFlags save_flags(const Impl& machine) {
  return SavedFlags{machine.quotes,
                    machine.unknown_fail,
                    machine.float_overflow_error,
                    machine.float_zero_div_error,
                    machine.float_undefined_error,
                    machine.char_conv.enabled,
                    machine.char_conv.map};
}

void restore_flags(Impl& machine, const SavedFlags& saved) {
  machine.quotes = saved.quotes;
  machine.unknown_fail = saved.unknown_fail;
  machine.float_overflow_error = saved.float_overflow_error;
  machine.float_zero_div_error = saved.float_zero_div_error;
  machine.float_undefined_error = saved.float_undefined_error;
  machine.char_conv.enabled = saved.char_conv_on;
  machine.char_conv.map = saved.char_map;
}

LoadStatus reject_load(Impl& machine, const OperatorTable& saved_ops, const SavedFlags& saved_flags,
                       const std::vector<Snapshot>& snapshots, std::size_t previous_size,
                       std::size_t heap_base) {
  machine.operators = saved_ops;
  restore_flags(machine, saved_flags);
  rollback(machine, snapshots, previous_size);
  machine.heap.truncate(heap_base);
  return LoadStatus{false, machine.diagnostic};
}

bool one_indicator(const Impl& machine, Cell cell, std::uint32_t& name, std::uint16_t& arity) {
  cell = machine.heap.deref(cell);
  if (cell.tag() != Tag::Str || machine.heap.functor_arity(cell) != 2) return false;
  if (machine.atoms.name(machine.heap.functor_atom(cell)) != "/") return false;
  const Cell left = machine.heap.deref(machine.heap.arg(cell, 0));
  const Cell right = machine.heap.deref(machine.heap.arg(cell, 1));
  if (left.tag() != Tag::Atom) return false;
  if (right.tag() != Tag::Int && right.tag() != Tag::IntBox) return false;
  const auto value = machine.heap.integer_value(right);
  if (value < 0 || value > 1024) return false;
  name = static_cast<std::uint32_t>(left.raw() >> 3);
  arity = static_cast<std::uint16_t>(value);
  return true;
}

bool collect_indicators(Impl& machine, Cell cell, std::vector<std::pair<std::uint32_t, std::uint16_t>>& out) {
  cell = machine.heap.deref(cell);
  std::uint32_t name = 0;
  std::uint16_t arity = 0;
  if (one_indicator(machine, cell, name, arity)) {
    out.emplace_back(name, arity);
    return true;
  }
  const auto nil = machine.atoms.intern("[]");
  if (cell.tag() == Tag::Atom && static_cast<std::uint32_t>(cell.raw() >> 3) == nil) return true;
  while (cell.tag() == Tag::Lis) {
    if (!one_indicator(machine, machine.heap.list_head(cell), name, arity)) return false;
    out.emplace_back(name, arity);
    cell = machine.heap.deref(machine.heap.list_tail(cell));
  }
  return cell.tag() == Tag::Atom && static_cast<std::uint32_t>(cell.raw() >> 3) == nil;
}

bool mark_dynamic(Impl& machine, std::vector<Snapshot>& snapshots, std::uint32_t name, std::uint16_t arity) {
  if (is_builtin_procedure(machine, name, arity)) {
    const Cell formal = formal_permission(machine, "modify", "static_procedure", cell_functor(machine, name, arity));
    fail_load(machine, write_term(machine.heap, machine.operators, formal, {}, {}));
    return false;
  }
  Predicate* predicate = find_pred(machine, name, arity);
  if (predicate != nullptr && !predicate->dynamic && !predicate->clauses.empty()) {
    fail_load(machine, "静的な述語は動的にできない");
    return false;
  }
  // 失敗した読み込みは、宣言の前の動的フラグへ戻す。
  remember_pred(machine, snapshots, name, arity);
  if (predicate == nullptr) {
    Predicate created;
    created.name = name;
    created.arity = arity;
    created.dynamic = true;
    created.dynamic_declared = true;
    machine.predicates.push_back(std::move(created));
    return true;
  }
  predicate->dynamic = true;
  predicate->dynamic_declared = true;
  return true;
}

bool dynamic_argument(Impl& machine, Cell directive, Cell& arg) {
  directive = machine.heap.deref(directive);
  if (directive.tag() != Tag::Str || machine.heap.functor_arity(directive) != 1) return false;
  const Cell body = machine.heap.deref(machine.heap.arg(directive, 0));
  if (body.tag() != Tag::Str || machine.heap.functor_arity(body) != 1) return false;
  if (machine.atoms.name(machine.heap.functor_atom(body)) != "dynamic") return false;
  arg = machine.heap.arg(body, 0);
  return true;
}

bool discontiguous_argument(Impl& machine, Cell directive, Cell& arg) {
  directive = machine.heap.deref(directive);
  if (directive.tag() != Tag::Str || machine.heap.functor_arity(directive) != 1) return false;
  const Cell body = machine.heap.deref(machine.heap.arg(directive, 0));
  if (body.tag() != Tag::Str || machine.heap.functor_arity(body) != 1) return false;
  if (machine.atoms.name(machine.heap.functor_atom(body)) != "discontiguous") return false;
  arg = machine.heap.arg(body, 0);
  return true;
}

int apply_discontiguous_directive(Impl& machine, Cell directive, std::vector<Apart>& apart) {
  Cell arg;
  if (!discontiguous_argument(machine, directive, arg)) return 0;
  std::vector<std::pair<std::uint32_t, std::uint16_t>> indicators;
  if (!collect_indicators(machine, arg, indicators)) {
    fail_load(machine, "述語指示子ではありません");
    return -1;
  }
  for (const auto& indicator : indicators) {
    if (allows_gap(apart, indicator.first, indicator.second)) continue;
    apart.push_back(Apart{indicator.first, indicator.second});
  }
  return 1;
}

int apply_dynamic_directive(Impl& machine, Cell directive, std::vector<Snapshot>& snapshots) {
  Cell arg;
  if (!dynamic_argument(machine, directive, arg)) return 0;
  std::vector<std::pair<std::uint32_t, std::uint16_t>> indicators;
  if (!collect_indicators(machine, arg, indicators)) {
    fail_load(machine, "述語指示子ではありません");
    return -1;
  }
  for (const auto& indicator : indicators) {
    if (!mark_dynamic(machine, snapshots, indicator.first, indicator.second)) return -1;
  }
  return 1;
}

int consume_directive(Impl& machine, Reader& reader, Cell term, std::vector<Snapshot>& snapshots,
                      std::vector<Apart>& apart) {
  if (!is_directive(machine.heap, term)) return 0;
  const int flag_directive = apply_flag_directive(machine, term);
  if (flag_directive < 0) return -1;
  if (flag_directive > 0) {
    reader.set_quotes(current_double_quotes(machine));
    return 1;
  }
  const int dynamic_directive = apply_dynamic_directive(machine, term, snapshots);
  if (dynamic_directive < 0) return -1;
  if (dynamic_directive > 0) return 1;
  const int apart_directive = apply_discontiguous_directive(machine, term, apart);
  if (apart_directive < 0) return -1;
  if (apart_directive > 0) return 1;
  const int conversion = apply_char_conv_directive(machine, term);
  if (conversion < 0) return -1;
  if (conversion > 0) return 1;
  if (!apply_op(machine, machine.heap.deref(term))) {
    if (machine.diagnostic.empty()) {
      const auto text = write_term(machine.heap, machine.operators, term, {}, {});
      fail_load(machine, "この指示はフェーズ2では使えない: " + text);
    }
    return -1;
  }
  return 1;
}

}  // namespace

LoadStatus load_source(Impl& machine, std::string_view source) {
  // 実行中の consult は、クエリの変数と継続枠より上だけを捨てる。
  const auto heap_base = machine.heap.size();
  machine.diagnostic.clear();
  machine.syntax_diag = false;
  for (Predicate& predicate : machine.predicates) {
    predicate.seen = false;
    predicate.dynamic_declared = false;
  }
  const auto saved_ops = machine.operators;
  const SavedFlags saved_flags = save_flags(machine);
  std::vector<Snapshot> snapshots;
  // 次の読み込みには渡さない。宣言の無い述語は、これまでどおり置き換える。
  std::vector<Apart> apart;
  const auto previous_size = machine.predicates.size();
  std::size_t last = static_cast<std::size_t>(-1);
  Reader reader(source, machine.heap, machine.operators, current_double_quotes(machine),
                &machine.char_conv);
  while (true) {
    const ReadOutcome outcome = reader.next();
    if (outcome.failure) {
      machine.syntax_diag = true;
      machine.diag_line = outcome.failure->line;
      machine.diag_column = outcome.failure->column;
      machine.diagnostic = reason_text(outcome.failure->reason);
      return reject_load(machine, saved_ops, saved_flags, snapshots, previous_size, heap_base);
    }
    if (!outcome.term) break;
    const Cell term = outcome.term->term;
    if (is_rule_arrow(machine.heap, term)) {
      const auto text = write_term(machine.heap, machine.operators, term, {}, {});
      fail_load(machine, "この指示はフェーズ2では使えない: " + text);
      return reject_load(machine, saved_ops, saved_flags, snapshots, previous_size, heap_base);
    }
    const int directed = consume_directive(machine, reader, term, snapshots, apart);
    if (directed < 0) return reject_load(machine, saved_ops, saved_flags, snapshots, previous_size, heap_base);
    if (directed > 0) continue;
    Clause clause;
    if (!compile_clause(machine.heap, term, clause)) {
      fail_load(machine, "この指示はフェーズ2では使えない: " +
                             write_term(machine.heap, machine.operators, term, {}, {}));
      return reject_load(machine, saved_ops, saved_flags, snapshots, previous_size, heap_base);
    }
    remember(machine, snapshots, clause);
    if (!add_clause(machine, std::move(clause), last, apart)) {
      return reject_load(machine, saved_ops, saved_flags, snapshots, previous_size, heap_base);
    }
  }
  machine.heap.truncate(heap_base);
  machine.diagnostic.clear();
  return LoadStatus{true, {}};
}

LoadStatus Machine::load_program(std::string_view source) { return load_source(*impl_, source); }

}
