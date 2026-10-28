#include "load_internal.hpp"

#include "mosa/library_path.hpp"
#include "mosa/path_text.hpp"
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

bool same_pred(const Predicate& predicate, std::uint32_t module, std::uint32_t name, std::uint16_t arity) {
  return predicate.module == module && predicate.name == name && predicate.arity == arity;
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

bool keeps_clauses(const Impl& machine, std::uint32_t module, std::uint32_t name, std::uint16_t arity) {
  for (const SharedPred& item : machine.declared) {
    if (item.shared && item.module == module && item.name == name && item.arity == arity) return true;
  }
  return false;
}

}  // namespace

void fail_load(Impl& machine, std::string message) {
  machine.diagnostic = std::move(message);
  machine.syntax_diag = false;
}

bool allows_gap(const std::vector<Apart>& apart, PredKey key) {
  for (const Apart& item : apart) {
    if (item.module == key.module && item.name == key.name && item.arity == key.arity) return true;
  }
  return false;
}

void note_declared(Impl& machine, PredKey key, bool shared) {
  if (shared ? keeps_clauses(machine, key.module, key.name, key.arity)
             : declared_only(machine, key.module, key.name, key.arity)) {
    return;
  }
  machine.declared.push_back(SharedPred{key.module, key.name, key.arity, shared});
}

bool declared_only(const Impl& machine, std::uint32_t module, std::uint32_t name, std::uint16_t arity) {
  for (const SharedPred& item : machine.declared) {
    if (item.module == module && item.name == name && item.arity == arity) return true;
  }
  return false;
}

void remember_pred(Impl& machine, std::vector<Snapshot>& snapshots, PredKey key) {
  const bool known = std::any_of(snapshots.begin(), snapshots.end(), [&](const Snapshot& shot) {
    return shot.module == key.module && shot.name == key.name && shot.arity == key.arity;
  });
  if (known) return;
  Predicate* predicate = find_pred(machine, key);
  Snapshot shot;
  shot.module = key.module;
  shot.name = key.name;
  shot.arity = key.arity;
  shot.created = predicate == nullptr;
  if (predicate != nullptr) {
    shot.clauses = predicate->clauses;
    shot.dynamic = predicate->dynamic;
    shot.meta = predicate->meta;
  }
  snapshots.push_back(std::move(shot));
}

namespace {

// 節は module に入る。頭部に M: が無ければ読み込み中のモジュールで、module/2 の無いテキストでは user。
bool add_clause(Impl& machine, std::uint32_t module, Clause clause, std::size_t& last,
                const std::vector<Apart>& apart) {
  const auto name = head_name(clause);
  const auto arity = head_arity(clause);
  std::size_t index = machine.predicates.size();
  for (std::size_t cursor = 0; cursor < machine.predicates.size(); ++cursor) {
    if (same_pred(machine.predicates[cursor], module, name, arity)) index = cursor;
  }
  if (index == machine.predicates.size()) {
    // 取り込んだ名前と同じ局所の述語は作らない（iso-notes 第5節の取り込みの衝突）。
    if (const auto formal = check_local(machine, module, name, arity)) {
      fail_load(machine, write_term(machine.heap, machine.operators, *formal, {}, {}));
      return false;
    }
    machine.predicates.push_back(Predicate{});
    machine.predicates.back().module = module;
    machine.predicates.back().name = name;
    machine.predicates.back().arity = arity;
    adopt_meta(machine, machine.predicates.back());
  }
  Predicate& predicate = machine.predicates[index];
  // 宣言の無い述語は、これまでと同じ診断のまま失敗する。
  if (predicate.seen && last != index && !allows_gap(apart, PredKey{module, name, arity})) {
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
    if (!keeps_clauses(machine, module, name, arity)) {
      predicate.clauses.clear();
      // 宣言が無い読み込みは、以前動的でも置き換えた述語を静的に戻す。
      if (!predicate.dynamic_declared) predicate.dynamic = false;
      // meta_predicate も同じ。読み直したテキストが宣言を消したら、限定もやめる。
      if (!predicate.meta_declared) predicate.meta.clear();
    }
  }
  predicate.seen = true;
  last = index;
  predicate.clauses.push_back(std::move(clause));
  return true;
}

void remember(Impl& machine, std::vector<Snapshot>& snapshots, std::uint32_t module, const Clause& clause) {
  remember_pred(machine, snapshots, PredKey{module, head_name(clause), head_arity(clause)});
}

void rollback(Impl& machine, const std::vector<Snapshot>& snapshots, std::size_t previous_size) {
  for (const Snapshot& shot : snapshots) {
    if (shot.created) continue;
    Predicate* predicate = find_pred(machine, PredKey{shot.module, shot.name, shot.arity});
    if (predicate != nullptr) {
      predicate->clauses = shot.clauses;
      predicate->dynamic = shot.dynamic;
      predicate->meta = shot.meta;
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
  bool japanese_punct = false;
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
                    machine.japanese_punct,
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
  machine.japanese_punct = saved.japanese_punct;
  machine.char_conv.map = saved.char_map;
}

// SPEC 第5節。この 2 つは字句の旗なので、ファイルの書き手の選択を読み手のファイルへ持ち出さない。
// 他の旗は実行の旗なので、成功した読み込みの後も残す。
void restore_file_flags(Impl& machine, const SavedFlags& saved) {
  machine.quotes = saved.quotes;
  machine.japanese_punct = saved.japanese_punct;
}

LoadStatus reject_load(Impl& machine, const OperatorTable& saved_ops, const SavedFlags& saved_flags,
                       const std::vector<Snapshot>& snapshots, std::size_t previous_size,
                       std::size_t heap_base, const std::vector<SharedPred>& saved_declared,
                       const std::vector<std::string>& saved_prepared, const std::vector<Module>& saved_modules) {
  machine.operators = saved_ops;
  // 途中で作ったモジュールと取り込みを残さない。半分だけ登録されたモジュールが見えると解決が変わる。
  machine.modules = saved_modules;
  restore_flags(machine, saved_flags);
  machine.declared = saved_declared;
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
  Impl* machine = nullptr;
  std::string key;
  bool finished = false;
  CycleGuard() = default;
  CycleGuard(const CycleGuard&) = delete;
  CycleGuard& operator=(const CycleGuard&) = delete;
  ~CycleGuard() {
    if (opening != nullptr && !opening->empty()) opening->pop_back();
    // 入れ子の初期化の例外では節を戻さないので、reject_load も prepared を戻さない。
    // 途中までしか読んでいないファイルの印だけを外し、次の use_module と ensure_loaded で読み直す。
    if (machine == nullptr || finished || !machine->load_kept) return;
    auto& prepared = machine->prepared;
    prepared.erase(std::remove(prepared.begin(), prepared.end(), key), prepared.end());
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
  const auto key = utf8_from_path(canonical_key(file));
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

// import_all は、モジュールのファイルを読み終えたとき公開述語を全部取り込むこと。use_module は自分で取り込む。
bool read_terms(Impl& machine, std::string_view source, std::string_view directory, std::string_view origin,
                bool nested, LoadScan& scan, bool import_all = true);

int open_prolog_file(Impl& machine, const std::filesystem::path& file, LoadScan& scan, bool import_all = true) {
  std::ifstream input{file, std::ios::binary};
  if (!input) {
    fail_load(machine, utf8_from_path(file) + ": ファイルを開けません");
    return -1;
  }
  std::ostringstream buffer;
  buffer << input.rdbuf();
  // 同じ走査に足す。別の load にすると、節の置き換えと初期化の区切りが変わる。
  if (!read_terms(machine, buffer.str(), utf8_from_path(file.parent_path()), utf8_from_path(file), true, scan,
                  import_all)) {
    return -1;
  }
  return 1;
}

int fail_formal(Impl& machine, Cell formal) {
  fail_load(machine, write_term(machine.heap, machine.operators, formal, {}, {}));
  return -1;
}

std::optional<Cell> use_body(Impl& machine, Cell term) {
  if (!is_directive(machine.heap, term)) return std::nullopt;
  const Cell body = machine.heap.deref(machine.heap.arg(machine.heap.deref(term), 0));
  if (body.tag() != Tag::Str || machine.atoms.name(machine.heap.functor_atom(body)) != "use_module") {
    return std::nullopt;
  }
  const auto arity = machine.heap.functor_arity(body);
  if (arity != 1 && arity != 2) return std::nullopt;
  return body;
}

// Module::file は正規化したパスである（declare_module に渡す前に正規化する）。
const Module* module_of_file(const Impl& machine, const std::filesystem::path& file) {
  const auto key = utf8_from_path(canonical_key(file));
  for (const Module& module : machine.modules) {
    if (module.file == key) return &module;
  }
  return nullptr;
}

// 読み込み済みのモジュールのファイルを ensure_loaded したとき。読み直しで消えた取り込みを足し直す。
int import_ready(Impl& machine, const std::filesystem::path& file) {
  const Module* module = module_of_file(machine, file);
  if (module == nullptr) return 1;
  if (const auto formal = import_exports(machine, module->name, machine.load_module)) {
    return fail_formal(machine, *formal);
  }
  return 1;
}

// use_module の第 1 引数。原子は読み込み中のファイルからの相対、library(Name) は library_path の探索。
std::optional<Cell> locate_use(Impl& machine, Cell arg, std::string_view directory, std::filesystem::path& file) {
  if (machine.heap.unbound(arg)) return formal_instantiation(machine);
  if (arg.tag() == Tag::Str && machine.heap.functor_arity(arg) == 1 &&
      machine.atoms.name(machine.heap.functor_atom(arg)) == "library") {
    const Cell name = machine.heap.deref(machine.heap.arg(arg, 0));
    if (machine.heap.unbound(name)) return formal_instantiation(machine);
    if (name.tag() != Tag::Atom) return formal_type(machine, "atom", name);
    const auto found = find_library(machine.atoms.name(static_cast<std::uint32_t>(name.raw() >> 3)));
    if (!found) return formal_existence(machine, "source_sink", arg);
    file = *found;
    return std::nullopt;
  }
  if (arg.tag() != Tag::Atom) return formal_type(machine, "atom", arg);
  file = prolog_source_path(resolve_include(directory, machine.atoms.name(static_cast<std::uint32_t>(arg.raw() >> 3))));
  return std::nullopt;
}

// use_module/1,2。ensure_loaded と同じく読むのは 1 回だけで、取り込みは呼ぶたびに足す。
int apply_use(Impl& machine, Cell term, std::string_view directory, LoadScan& scan) {
  const auto body = use_body(machine, term);
  if (!body) return 0;
  const Cell arg = machine.heap.deref(machine.heap.arg(*body, 0));
  std::filesystem::path file;
  if (const auto formal = locate_use(machine, arg, directory, file)) return fail_formal(machine, *formal);
  ImportSpec spec;
  if (machine.heap.functor_arity(*body) == 2) {
    if (const auto formal = parse_imports(machine, machine.heap.arg(*body, 1), spec)) {
      return fail_formal(machine, *formal);
    }
  }
  if (!file_ready(machine, scan, file) && open_prolog_file(machine, file, scan, false) < 0) return -1;
  const Module* module = module_of_file(machine, file);
  if (module == nullptr) return fail_formal(machine, formal_permission(machine, "load", "source_sink", arg));
  if (const auto formal = apply_imports(machine, module->name, machine.load_module, spec)) {
    return fail_formal(machine, *formal);
  }
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
  if (once && file_ready(machine, scan, file)) return import_ready(machine, file);
  return open_prolog_file(machine, file, scan);
}

// 開き済みのファイルをもう一度開くと、読み込みが終わらない。
bool enter_file(Impl& machine, std::string_view origin, LoadScan& scan, CycleGuard& guard) {
  if (origin.empty()) return true;
  const auto key = utf8_from_path(canonical_key(path_from_utf8(origin)));
  if (std::find(scan.opening.begin(), scan.opening.end(), key) != scan.opening.end()) {
    fail_load(machine, std::string{origin} + ": ファイルが循環しています");
    return false;
  }
  scan.opening.push_back(key);
  guard.opening = &scan.opening;
  guard.machine = &machine;
  guard.key = key;
  // 開き始めたパスは準備済みにする。失敗した読み込みは reject_load が戻す。
  if (std::find(machine.prepared.begin(), machine.prepared.end(), key) == machine.prepared.end()) {
    machine.prepared.push_back(key);
  }
  return true;
}

// [File] と consult(File) は別の読み込みで既存の節を置き換える。親の写しに無いと、親の失敗で戻せない。
bool replaces_files(Impl& machine, Cell term) {
  if (!is_directive(machine.heap, term)) return false;
  const Cell body = machine.heap.deref(machine.heap.arg(term, 0));
  if (body.tag() == Tag::Lis) return true;
  if (body.tag() == Tag::Atom) return machine.atoms.name(static_cast<std::uint32_t>(body.raw() >> 3)) == "[]";
  return body.tag() == Tag::Str && machine.heap.functor_arity(body) == 1 &&
         machine.atoms.name(machine.heap.functor_atom(body)) == "consult";
}

bool take_term(Impl& machine, Reader& reader, Cell term, std::string_view directory, std::string_view origin,
               bool first, LoadScan& scan) {
  if (is_rule_arrow(machine.heap, term)) {
    fail_load(machine, "この指示はフェーズ2では使えない: " +
                           write_term(machine.heap, machine.operators, term, {}, {}));
    return false;
  }
  const int included = apply_file(machine, term, directory, scan, "include", false);
  if (included != 0) return included > 0;
  const int ensured = apply_file(machine, term, directory, scan, "ensure_loaded", true);
  if (ensured != 0) return ensured > 0;
  const int used = apply_use(machine, term, directory, scan);
  if (used != 0) return used > 0;
  if (replaces_files(machine, term)) {
    for (const Predicate& predicate : machine.predicates) {
      remember_pred(machine, scan.snapshots, PredKey{predicate.module, predicate.name, predicate.arity});
    }
  }
  const int listed = apply_file_list(machine, term, directory, scan.opening);
  if (listed != 0) return listed > 0;
  const int directed =
      consume_directive(machine, reader, term, scan.snapshots, scan.apart, scan.inits, first, origin);
  if (directed != 0) return directed > 0;
  std::uint32_t module = machine.load_module;
  if (const auto formal = strip_clause(machine, term, module)) return fail_formal(machine, *formal) > 0;
  // M:(:- G) と M:(H --> B) は節ではない。外す前と同じく受けない。
  if (is_rule_arrow(machine.heap, term) || is_directive(machine.heap, term)) {
    fail_load(machine, "この指示はフェーズ2では使えない: " +
                           write_term(machine.heap, machine.operators, term, {}, {}));
    return false;
  }
  Clause clause;
  if (!compile_clause(machine.heap, term, clause)) {
    fail_load(machine, "この指示はフェーズ2では使えない: " +
                           write_term(machine.heap, machine.operators, term, {}, {}));
    return false;
  }
  remember(machine, scan.snapshots, module, clause);
  return add_clause(machine, module, std::move(clause), scan.last, scan.apart);
}

// module/2 はこのテキストの中だけ節の行き先を変える。読み終えたら、読み込んだ側へ戻す。
struct ModuleScope {
  Impl& machine;
  std::uint32_t outer;
  explicit ModuleScope(Impl& owner) : machine(owner), outer(owner.load_module) {}
  ModuleScope(const ModuleScope&) = delete;
  ModuleScope& operator=(const ModuleScope&) = delete;
  ~ModuleScope() { machine.load_module = outer; }
  // 公開は読み終えてから見せる。途中の export/1 で足した述語も取り込む。
  bool close(bool import_all) {
    if (!import_all || machine.load_module == outer) return true;
    const auto formal = import_exports(machine, machine.load_module, outer);
    if (!formal) return true;
    fail_load(machine, write_term(machine.heap, machine.operators, *formal, {}, {}));
    return false;
  }
};

bool read_terms(Impl& machine, std::string_view source, std::string_view directory, std::string_view origin,
                bool nested, LoadScan& scan, bool import_all) {
  CycleGuard guard;
  if (!enter_file(machine, origin, scan, guard)) return false;
  ModuleScope scope{machine};
  Reader reader(source, machine.heap, machine.operators, current_double_quotes(machine), &machine.char_conv);
  bool first = true;
  while (true) {
    // module/2 と op/3 が見え方を替える。項ごとに、今のモジュールの見え方で読む。
    reader.set_operators(op_view(machine, machine.load_module));
    const ReadOutcome outcome = reader.next();
    if (outcome.failure) return note_syntax(machine, origin, nested, *outcome.failure);
    if (!outcome.term) {
      guard.finished = true;
      return scope.close(import_all);
    }
    if (!take_term(machine, reader, outcome.term->term, directory, origin, first, scan)) return false;
    first = false;
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
    predicate.meta_declared = false;
  }
  const auto saved_ops = machine.operators;
  const SavedFlags saved_flags = save_flags(machine);
  const auto saved_declared = machine.declared;
  const auto saved_prepared = machine.prepared;
  const auto saved_modules = machine.modules;
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
      if (!origin.empty()) restore_file_flags(machine, saved_flags);
      machine.heap.truncate(heap_base);
      return LoadStatus{false, machine.diagnostic};
    }
    return reject_load(machine, saved_ops, saved_flags, snapshots, previous_size, heap_base, saved_declared,
                       saved_prepared, saved_modules);
  }
  // 文字だけの読み込み（mosatest の指令の断片など）はファイルではないので、旗を戻さない。
  // initialization のゴールより先に戻す（iso-notes 7.4.2.8 の決定 B）。
  if (!origin.empty()) restore_file_flags(machine, saved_flags);
  return finish_load(machine, heap_base, inits);
}

LoadStatus Machine::load_program(std::string_view source) { return load_source(*impl_, source); }

}
