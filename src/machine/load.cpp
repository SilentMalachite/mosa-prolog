#include "load_internal.hpp"

#include "mosa/reader.hpp"
#include "mosa/syntax_error.hpp"
#include "mosa/writer.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace mosa {

// 定義は load_path.cpp。include と ensure_loaded は同じ経路で開く。
std::filesystem::path resolve_include(std::string_view directory, std::string_view atom);
std::filesystem::path canonical_key(const std::filesystem::path& file);
std::filesystem::path prolog_source_path(const std::filesystem::path& file);

namespace {

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

// 読み込みの節はまだ user にだけ入る。モジュールのファイルはフェーズ4 Task 4 で受ける。
bool same_pred(const Impl& machine, const Predicate& predicate, std::uint32_t name, std::uint16_t arity) {
  return predicate.module == machine.user_module && predicate.name == name && predicate.arity == arity;
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

bool keeps_clauses(const Impl& machine, std::uint32_t name, std::uint16_t arity) {
  for (const SharedPred& item : machine.multifile) {
    if (item.name == name && item.arity == arity) return true;
  }
  return false;
}

}  // namespace

void fail_load(Impl& machine, std::string message) {
  machine.diagnostic = std::move(message);
  machine.syntax_diag = false;
}

bool allows_gap(const std::vector<Apart>& apart, std::uint32_t name, std::uint16_t arity) {
  for (const Apart& item : apart) {
    if (item.name == name && item.arity == arity) return true;
  }
  return false;
}

void note_multifile(Impl& machine, std::uint32_t name, std::uint16_t arity) {
  if (keeps_clauses(machine, name, arity)) return;
  machine.multifile.push_back(SharedPred{name, arity});
}

void remember_pred(Impl& machine, std::vector<Snapshot>& snapshots, std::uint32_t name, std::uint16_t arity) {
  const bool known = std::any_of(snapshots.begin(), snapshots.end(), [&](const Snapshot& shot) {
    return shot.name == name && shot.arity == arity;
  });
  if (known) return;
  Predicate* predicate = find_pred(machine, PredKey{machine.user_module, name, arity});
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

namespace {

bool add_clause(Impl& machine, Clause clause, std::size_t& last, const std::vector<Apart>& apart) {
  const auto name = head_name(clause);
  const auto arity = head_arity(clause);
  std::size_t index = machine.predicates.size();
  for (std::size_t cursor = 0; cursor < machine.predicates.size(); ++cursor) {
    if (same_pred(machine, machine.predicates[cursor], name, arity)) index = cursor;
  }
  if (index == machine.predicates.size()) {
    machine.predicates.push_back(Predicate{});
    machine.predicates.back().module = machine.user_module;
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
    // 宣言が残っている述語は足す。無い述語は、これまでどおり置き換える。
    if (!keeps_clauses(machine, name, arity)) {
      predicate.clauses.clear();
      // 宣言が無い読み込みは、以前動的でも置き換えた述語を静的に戻す。
      if (!predicate.dynamic_declared) predicate.dynamic = false;
    }
  }
  predicate.seen = true;
  last = index;
  predicate.clauses.push_back(std::move(clause));
  return true;
}

void remember(Impl& machine, std::vector<Snapshot>& snapshots, const Clause& clause) {
  remember_pred(machine, snapshots, head_name(clause), head_arity(clause));
}

void rollback(Impl& machine, const std::vector<Snapshot>& snapshots, std::size_t previous_size) {
  for (const Snapshot& shot : snapshots) {
    if (shot.created) continue;
    Predicate* predicate = find_pred(machine, PredKey{machine.user_module, shot.name, shot.arity});
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
  bool utf8_replace = false;  // 失敗した読み込みは、途中の set_prolog_flag を残さない。
  bool debug_on = false;
  std::vector<std::pair<char32_t, char32_t>> char_map;
};

SavedFlags save_flags(const Impl& machine) {
  return SavedFlags{machine.quotes,
                    machine.unknown_fail,
                    machine.float_overflow_error,
                    machine.float_zero_div_error,
                    machine.float_undefined_error,
                    machine.char_conv.enabled,
                    machine.utf8_replace,
                    machine.debug_on,
                    machine.char_conv.map};
}

void restore_flags(Impl& machine, const SavedFlags& saved) {
  machine.quotes = saved.quotes;
  machine.unknown_fail = saved.unknown_fail;
  machine.float_overflow_error = saved.float_overflow_error;
  machine.float_zero_div_error = saved.float_zero_div_error;
  machine.float_undefined_error = saved.float_undefined_error;
  machine.char_conv.enabled = saved.char_conv_on;
  machine.utf8_replace = saved.utf8_replace;
  machine.debug_on = saved.debug_on;
  machine.char_conv.map = saved.char_map;
}

LoadStatus reject_load(Impl& machine, const OperatorTable& saved_ops, const SavedFlags& saved_flags,
                       const std::vector<Snapshot>& snapshots, std::size_t previous_size,
                       std::size_t heap_base, const std::vector<SharedPred>& saved_multi,
                       const std::vector<std::string>& saved_prepared) {
  machine.operators = saved_ops;
  restore_flags(machine, saved_flags);
  machine.multifile = saved_multi;
  machine.prepared = saved_prepared;
  rollback(machine, snapshots, previous_size);
  machine.heap.truncate(heap_base);
  return LoadStatus{false, machine.diagnostic};
}

LoadStatus finish_load(Impl& machine, std::size_t heap_base, const std::vector<Cell>& inits) {
  if (!run_inits(machine, inits)) {
    machine.heap.truncate(heap_base);
    // 節は残す。親の reject が、この例外で読んだ節まで消さないようにする。
    machine.load_kept = true;
    return LoadStatus{false, machine.diagnostic};
  }
  machine.heap.truncate(heap_base);
  machine.diagnostic.clear();
  // 成功した読み込みの印を残すと、親の構文エラーが節を戻さなくなる。
  machine.load_kept = false;
  return LoadStatus{true, {}};
}

struct LoadScan {
  std::vector<Snapshot>& snapshots;
  std::vector<Apart>& apart;
  std::vector<Cell>& inits;
  std::size_t& last;
  std::vector<std::string>& opening;
};

struct CycleGuard {
  std::vector<std::string>* opening = nullptr;
  ~CycleGuard() {
    if (opening != nullptr && !opening->empty()) opening->pop_back();
  }
};

bool file_argument(Impl& machine, Cell directive, std::string_view functor, Cell& arg) {
  // 事実 p(include(f)) は指示ではない。関手が :- か ?- のときだけ開く。
  if (!is_directive(machine.heap, directive)) return false;
  directive = machine.heap.deref(directive);
  if (directive.tag() != Tag::Str || machine.heap.functor_arity(directive) != 1) return false;
  const Cell body = machine.heap.deref(machine.heap.arg(directive, 0));
  if (body.tag() != Tag::Str || machine.heap.functor_arity(body) != 1) return false;
  if (machine.atoms.name(machine.heap.functor_atom(body)) != functor) return false;
  arg = machine.heap.arg(body, 0);
  return true;
}

bool file_ready(const Impl& machine, const LoadScan& scan, const std::filesystem::path& file) {
  const auto key = canonical_key(file).generic_string();
  if (std::find(scan.opening.begin(), scan.opening.end(), key) != scan.opening.end()) return true;
  return std::find(machine.prepared.begin(), machine.prepared.end(), key) != machine.prepared.end();
}

bool note_syntax(Impl& machine, std::string_view origin, bool nested, const SyntaxFailure& failure) {
  const auto reason = reason_text(failure.reason);
  if (!nested) {
    machine.syntax_diag = true;
    machine.diag_line = failure.line;
    machine.diag_column = failure.column;
    machine.diagnostic = reason;
    return false;
  }
  // 親の consult は自分のパスを付ける。入れ子の位置は、ここにファイル名を残す。
  machine.syntax_diag = false;
  machine.diagnostic = std::string{origin} + ":" + std::to_string(failure.line) + ":" +
                       std::to_string(failure.column) + ": " + reason;
  return false;
}

bool read_terms(Impl& machine, std::string_view source, std::string_view directory, std::string_view origin,
                bool nested, LoadScan& scan);

int open_prolog_file(Impl& machine, const std::filesystem::path& file, LoadScan& scan) {
  std::ifstream input{file, std::ios::binary};
  if (!input) {
    fail_load(machine, file.string() + ": ファイルを開けません");
    return -1;
  }
  std::ostringstream buffer;
  buffer << input.rdbuf();
  // 同じ走査に足す。別の load にすると、節の置き換えと初期化の区切りが変わる。
  if (!read_terms(machine, buffer.str(), file.parent_path().string(), file.string(), true, scan)) return -1;
  return 1;
}

int apply_file(Impl& machine, Cell term, std::string_view directory, LoadScan& scan, std::string_view functor,
               bool once) {
  Cell arg;
  if (!file_argument(machine, term, functor, arg)) return 0;
  arg = machine.heap.deref(arg);
  if (machine.heap.unbound(arg)) {
    fail_load(machine, write_term(machine.heap, machine.operators, formal_instantiation(machine), {}, {}));
    return -1;
  }
  if (arg.tag() != Tag::Atom) {
    const Cell formal = formal_type(machine, "atom", arg);
    fail_load(machine, write_term(machine.heap, machine.operators, formal, {}, {}));
    return -1;
  }
  const auto name = machine.atoms.name(static_cast<std::uint32_t>(arg.raw() >> 3));
  const auto file = prolog_source_path(resolve_include(directory, name));
  // include は毎回足す。ensure_loaded は、準備済みのパスを節も初期化も足さない。
  if (once && file_ready(machine, scan, file)) return 1;
  return open_prolog_file(machine, file, scan);
}

bool read_terms(Impl& machine, std::string_view source, std::string_view directory, std::string_view origin,
                bool nested, LoadScan& scan) {
  CycleGuard guard;
  if (!origin.empty()) {
    // 開き済みのファイルをもう一度開くと、読み込みが終わらない。
    const auto key = canonical_key(std::filesystem::path{std::string{origin}}).generic_string();
    if (std::find(scan.opening.begin(), scan.opening.end(), key) != scan.opening.end()) {
      fail_load(machine, std::string{origin} + ": ファイルが循環しています");
      return false;
    }
    scan.opening.push_back(key);
    guard.opening = &scan.opening;
    // 開き始めたパスは準備済みにする。失敗した読み込みは reject_load が戻す。
    if (std::find(machine.prepared.begin(), machine.prepared.end(), key) == machine.prepared.end()) {
      machine.prepared.push_back(key);
    }
  }
  Reader reader(source, machine.heap, machine.operators, current_double_quotes(machine), &machine.char_conv);
  while (true) {
    const ReadOutcome outcome = reader.next();
    if (outcome.failure) return note_syntax(machine, origin, nested, *outcome.failure);
    if (!outcome.term) return true;
    const Cell term = outcome.term->term;
    if (is_rule_arrow(machine.heap, term)) {
      fail_load(machine, "この指示はフェーズ2では使えない: " +
                             write_term(machine.heap, machine.operators, term, {}, {}));
      return false;
    }
    const int included = apply_file(machine, term, directory, scan, "include", false);
    if (included < 0) return false;
    if (included > 0) continue;
    const int ensured = apply_file(machine, term, directory, scan, "ensure_loaded", true);
    if (ensured < 0) return false;
    if (ensured > 0) continue;
    // [File] は別の読み込みで既存の節を置き換える。親の写しに無いと、親の失敗で戻せない。
    if (is_directive(machine.heap, term)) {
      const Cell body = machine.heap.deref(machine.heap.arg(term, 0));
      if (body.tag() == Tag::Lis || (body.tag() == Tag::Atom &&
                                     machine.atoms.name(static_cast<std::uint32_t>(body.raw() >> 3)) == "[]")) {
        for (const Predicate& predicate : machine.predicates) {
          remember_pred(machine, scan.snapshots, predicate.name, predicate.arity);
        }
      }
    }
    const int listed = apply_file_list(machine, term, directory, scan.opening);
    if (listed < 0) return false;
    if (listed > 0) continue;
    const int directed = consume_directive(machine, reader, term, scan.snapshots, scan.apart, scan.inits);
    if (directed < 0) return false;
    if (directed > 0) continue;
    Clause clause;
    if (!compile_clause(machine.heap, term, clause)) {
      fail_load(machine, "この指示はフェーズ2では使えない: " +
                             write_term(machine.heap, machine.operators, term, {}, {}));
      return false;
    }
    remember(machine, scan.snapshots, clause);
    if (!add_clause(machine, std::move(clause), scan.last, scan.apart)) return false;
  }
}

}  // namespace

LoadStatus load_source(Impl& machine, std::string_view source, std::string_view directory,
                       std::string_view origin, std::vector<std::string>* shared_opening) {
  // 実行中の consult は、クエリの変数と継続枠より上だけを捨てる。
  const auto heap_base = machine.heap.size();
  machine.diagnostic.clear();
  machine.syntax_diag = false;
  machine.load_kept = false;
  for (Predicate& predicate : machine.predicates) {
    predicate.seen = false;
    predicate.dynamic_declared = false;
  }
  const auto saved_ops = machine.operators;
  const SavedFlags saved_flags = save_flags(machine);
  const auto saved_multi = machine.multifile;
  const auto saved_prepared = machine.prepared;
  std::vector<Snapshot> snapshots;
  // 次の読み込みには渡さない。宣言の無い述語は、これまでどおり置き換える。
  std::vector<Apart> apart;
  // テキストを準備し終えてから、ソースの順に実行する。
  std::vector<Cell> inits;
  const auto previous_size = machine.predicates.size();
  std::size_t last = static_cast<std::size_t>(-1);
  std::vector<std::string> local_opening;
  std::vector<std::string>& opening = shared_opening == nullptr ? local_opening : *shared_opening;
  LoadScan scan{snapshots, apart, inits, last, opening};
  if (!read_terms(machine, source, directory, origin, false, scan)) {
    if (machine.load_kept) {
      machine.heap.truncate(heap_base);
      return LoadStatus{false, machine.diagnostic};
    }
    return reject_load(machine, saved_ops, saved_flags, snapshots, previous_size, heap_base, saved_multi,
                       saved_prepared);
  }
  return finish_load(machine, heap_base, inits);
}

LoadStatus Machine::load_program(std::string_view source) { return load_source(*impl_, source); }

}
