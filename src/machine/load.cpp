#include "load_internal.hpp"

#include "dcg.hpp"

#include "mosa/library_path.hpp"
#include "mosa/path_text.hpp"
#include "mosa/reader.hpp"
#include "mosa/syntax_error.hpp"
#include "mosa/writer.hpp"

#include <algorithm>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace mosa {

// 定義は load_path.cpp。include と ensure_loaded は同じ経路で開く。
std::filesystem::path resolve_include(std::string_view directory, std::string_view atom);
std::filesystem::path canonical_key(const std::filesystem::path& file);
std::filesystem::path prolog_source_path(const std::filesystem::path& file);

namespace {

bool is_rule_arrow(const Heap& heap, Cell cell) {
  cell = heap.deref(cell);
  return cell.tag() == Tag::Str && heap.functor_arity(cell) == 2 && heap.atoms().name(heap.functor_atom(cell)) == "-->";
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
  // 旗ではないが同じく、失敗した読み込みの initialization(G, main) を残さない。残すと取り消したファイルの G が動く。
  std::optional<ExportedTerm> main_goal;
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
                    machine.char_conv.map,
                    machine.main_goal};
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
  machine.main_goal = saved.main_goal;
}

// SPEC 第5節。この 2 つは字句の旗なので、ファイルの書き手の選択を読み手のファイルへ持ち出さない。
// 他の旗は実行の旗なので、成功した読み込みの後も残す。
void restore_file_flags(Impl& machine, const SavedFlags& saved) {
  machine.quotes = saved.quotes;
  machine.japanese_punct = saved.japanese_punct;
}

// 読み込みの前の演算子。library(...) を読む間に退けた利用者の表も、その読み込みの user の op/3 を受けるので戻す
// （ADR 0006 の改訂 1）。
struct SavedOps {
  OperatorTable table;
  std::vector<OperatorTable> aside;
};

LoadStatus reject_load(Impl& machine, const SavedOps& saved_ops, const SavedFlags& saved_flags,
                       const LoadSnapshots& snapshots, std::size_t previous_serial,
                       std::size_t heap_base, const std::vector<SharedPred>& saved_declared,
                       const std::vector<std::string>& saved_prepared, const std::vector<Module>& saved_modules) {
  machine.operators = saved_ops.table;
  machine.set_aside_ops = saved_ops.aside;
  // 途中で作ったモジュールと取り込みを残さない。半分だけ登録されたモジュールが見えると解決が変わる。
  machine.modules = saved_modules;
  restore_flags(machine, saved_flags);
  machine.declared = saved_declared;
  machine.prepared = saved_prepared;
  rollback_preds(machine, snapshots, previous_serial);
  machine.heap.truncate(heap_base);
  return LoadStatus{false, machine.diagnostic};
}

// heap_base は load_source が RootScope に登録した変数を参照で受ける。初期化のゴールの GC が値を書き換える。
LoadStatus finish_load(Impl& machine, const std::size_t& heap_base, const std::vector<Cell>& inits) {
  // 翻訳のときは initialization/1 のゴールを書き留めるだけで呼ばない。起動時の読み込みが呼ぶ。
  if (machine.translation == nullptr && !run_inits(machine, inits)) {
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
  LoadSnapshots& snapshots;
  std::vector<Apart>& apart;
  std::vector<Cell>& inits;
  std::optional<PredKey>& last;
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
  const auto reason = message_text(machine, syntax_key(failure.reason));
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

// テキストの読み方。import_all は、モジュールのファイルを読み終えたとき公開述語を全部取り込むこと（use_module は
// 自分で取り込む）。owns は、テキストの述語をこのファイルのものにすること（ADR 0020）。include したテキストは、
// それを読んだファイルの述語を定義する。
struct TextRole {
  bool nested = true;
  bool import_all = true;
  bool owns = true;
};

bool read_terms(Impl& machine, std::string_view source, std::string_view directory, std::string_view origin,
                LoadScan& scan, TextRole role);

int open_prolog_file(Impl& machine, const std::filesystem::path& file, LoadScan& scan, TextRole role) {
  const auto text = read_source(machine, file);
  if (!text) {
    fail_load(machine, message_text(machine, "cannot_open_file", {utf8_from_path(file)}));
    return -1;
  }
  // 同じ走査に足す。別の load にすると、節の置き換えと初期化の区切りが変わる。
  if (!read_terms(machine, *text, utf8_from_path(file.parent_path()), utf8_from_path(file), scan, role)) return -1;
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

// library(Name) で見つけたファイルから Name の分だけ親に戻ると、探索したディレクトリ（$MOSA_LIB か lib/）。
// 翻訳はそこからの相対を埋め込みの鍵にする（library/sub/util.pl）。Name は sub/util や util.pl の形でもよい。
std::filesystem::path library_root(std::filesystem::path found, std::string_view name) {
  for ([[maybe_unused]] const auto& part : path_from_utf8(name).lexically_normal()) found = found.parent_path();
  return canonical_key(found);
}

bool names_library(const Impl& machine, Cell arg) {
  return arg.tag() == Tag::Str && machine.heap.functor_arity(arg) == 1 &&
         machine.atoms.name(machine.heap.functor_atom(arg)) == "library";
}

// use_module の第 1 引数。原子は読み込み中のファイルからの相対、library(Name) は、実行ファイルに同梱した一覧
// （bundled に名前が入る。計画 Task 10）を先に引き、無ければ library_path の探索（SPEC 第5節）。同梱ライブラリの中の
// 素のパスは擬似ディレクトリ library/ に解決するので、それも同じ一覧で引く。
std::optional<Cell> locate_use(Impl& machine, Cell arg, std::string_view directory, std::filesystem::path& file,
                               std::optional<std::string>& bundled) {
  if (machine.heap.unbound(arg)) return formal_instantiation(machine);
  if (names_library(machine, arg)) {
    const Cell name = machine.heap.deref(machine.heap.arg(arg, 0));
    if (machine.heap.unbound(name)) return formal_instantiation(machine);
    if (name.tag() != Tag::Atom) return formal_type(machine, "atom", name);
    const std::string_view text = machine.atoms.name(static_cast<std::uint32_t>(name.raw() >> 3));
    if (const auto embedded = locate_bundled_library(machine, text)) {
      file = *embedded;
      bundled = std::string{text};
      return std::nullopt;
    }
    const auto found = find_library(text);
    if (!found) return formal_existence(machine, "source_sink", arg);
    file = *found;
    if (machine.translation != nullptr) {
      machine.translation->library_opens.push_back(
          TranslationLog::LibraryOpen{utf8_from_path(canonical_key(file)), utf8_from_path(library_root(file, text))});
    }
    return std::nullopt;
  }
  if (arg.tag() != Tag::Atom) return formal_type(machine, "atom", arg);
  file = source_path(machine, resolve_include(directory, machine.atoms.name(static_cast<std::uint32_t>(arg.raw() >> 3))));
  bundled = locate_bundled_plain(machine, file);
  return std::nullopt;
}

// use_module/1,2。ensure_loaded と同じく読むのは 1 回だけで、取り込みは呼ぶたびに足す。
int apply_use(Impl& machine, Cell term, std::string_view directory, LoadScan& scan) {
  const auto body = use_body(machine, term);
  if (!body) return 0;
  Cell arg = machine.heap.deref(machine.heap.arg(*body, 0));
  // 読み込むファイルの指示が GC を起こしうる。読んだ後の permission_error で arg を使うので登録する。
  const RootScope roots(machine, [&arg](GcVisitor& visitor) { visitor.cell(arg); });
  std::filesystem::path file;
  std::optional<std::string> bundled;
  if (const auto formal = locate_use(machine, arg, directory, file, bundled)) return fail_formal(machine, *formal);
  ImportSpec spec;
  if (machine.heap.functor_arity(*body) == 2) {
    if (const auto formal = parse_imports(machine, machine.heap.arg(*body, 1), spec)) {
      return fail_formal(machine, *formal);
    }
  }
  if (!file_ready(machine, scan, file)) {
    // library(...) で見つけたファイルは、利用者の表でなく ISO の核と自分のモジュールの演算子で読む（ADR 0006 の
    // 改訂 1）。利用者が -> や :- を変えた後でも、ライブラリの節を別の項に読まない。
    std::optional<CoreOperatorScope> core;
    if (names_library(machine, arg)) core.emplace(machine);
    if (open_prolog_file(machine, file, scan, TextRole{true, false, true}) < 0) return -1;
    if (core) core->finish();
    // 同梱ライブラリの翻訳済みの単位は、原文を読み終えてから付ける（ADR 0009）。
    if (bundled) attach_bundled_library(machine, *bundled);
  }
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
  const auto file = source_path(machine, resolve_include(directory, name));
  // include は毎回足す。ensure_loaded は、準備済みのパスを節も初期化も足さない。
  if (once && file_ready(machine, scan, file)) return import_ready(machine, file);
  return open_prolog_file(machine, file, scan, TextRole{true, true, once});
}

// 開き済みのファイルをもう一度開くと、読み込みが終わらない。
bool enter_file(Impl& machine, std::string_view origin, LoadScan& scan, CycleGuard& guard) {
  if (origin.empty()) return true;
  const auto key = utf8_from_path(canonical_key(path_from_utf8(origin)));
  if (std::find(scan.opening.begin(), scan.opening.end(), key) != scan.opening.end()) {
    fail_load(machine, message_text(machine, "file_cycle", {origin}));
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

// 翻訳のときだけ、足した節の位置を書き留める。どこにも定義の無い述語を呼ぶ警告がこの行を出す。
// key は add_clause が節を足した述語。
void note_site(Impl& machine, PredKey key, std::size_t line) {
  TranslationLog* log = machine.translation;
  if (log == nullptr || log->current >= log->files.size()) return;
  const Predicate* found = find_pred(machine, key);
  if (found == nullptr) return;
  const Predicate& predicate = *found;
  log->sites.push_back(TranslationLog::Site{PredKey{predicate.module, predicate.name, predicate.arity},
                                            predicate.clauses.size() - 1, log->current, line});
}

// 飛ばしたゴールの op/3 は後の項の読みを変えうるので、翻訳の読みで誤りになった項も起動時には読めるかもしれない
// （計画 Task 5）。誤りにせず、このファイルの述語を IR のまま動かす印を付ける。飛ばしたゴールが無ければ誤りのまま。
bool mark_opaque(TranslationLog* log) {
  if (log == nullptr || log->current >= log->files.size() || !log->files[log->current].skipped_goal) return false;
  log->files[log->current].opaque = true;
  return true;
}

// 翻訳のときに開いたファイルの原文を書き留め、読み終えるまで節の行き先にする。include を 2 回読んでも 1 つ。
// library(_) で開いたファイルと、そこから開いたファイルはライブラリとして印を付ける（翻訳は Task 10）。
struct FileNote {
  TranslationLog* log = nullptr;
  std::size_t outer = 0;
  FileNote(Impl& machine, std::string_view origin, std::string_view source) : log(machine.translation) {
    if (log == nullptr) return;
    outer = log->current;
    if (origin.empty()) return;
    const auto key = utf8_from_path(canonical_key(path_from_utf8(origin)));
    for (std::size_t index = 0; index < log->files.size(); ++index) {
      if (log->files[index].key == key) log->current = index;
    }
    if (log->current != outer) return;
    // library(Name) で開いた物はその探索の根、ライブラリから素のパスや include で開いた物は親と同じ根。
    std::string root;
    for (const TranslationLog::LibraryOpen& open : log->library_opens) {
      if (open.key == key) root = open.root;
    }
    if (root.empty() && outer < log->files.size()) root = log->files[outer].library_root;
    TranslationLog::File file{key, std::string{source}, 0, 0, !root.empty()};
    file.library_root = std::move(root);
    log->files.push_back(std::move(file));
    log->current = log->files.size() - 1;
  }
  FileNote(const FileNote&) = delete;
  FileNote& operator=(const FileNote&) = delete;
  // include した先で飛ばしたゴールの op/3 は、読み込んだ側のテキストにも効く。
  ~FileNote() {
    if (log == nullptr) return;
    if (log->current < log->files.size() && outer < log->files.size() && log->files[log->current].skipped_goal) {
      log->files[outer].skipped_goal = true;
    }
    log->current = outer;
  }
  [[nodiscard]] bool tolerate_syntax() const { return mark_opaque(log); }
  void first_term(const ReadTerm& read) const {
    if (log == nullptr || log->current >= log->files.size() || log->files[log->current].line != 0) return;
    log->files[log->current].line = read.line;
    log->files[log->current].column = read.column;
  }
};

bool take_term(Impl& machine, Reader& reader, const ReadTerm& read, std::string_view directory,
               std::string_view origin, bool first, LoadScan& scan) {
  // mosatest の test/3 は節にせず横取りする（suite.cpp）。指示より先に見るので、横取りした項は何も変えない。
  if (machine.term_hook && machine.term_hook(read, origin)) return true;
  Cell term = read.term;
  // 文法規則は訳した節として、以下の節と同じ道を通す（ADR 0024）。
  if (is_rule_arrow(machine.heap, term)) {
    const DcgOut translated = dcg_rule(machine, term);
    if (translated.formal) return fail_formal(machine, *translated.formal) > 0;
    term = translated.term;
  }
  const int included = apply_file(machine, term, directory, scan, "include", false);
  if (included != 0) return included > 0;
  const int ensured = apply_file(machine, term, directory, scan, "ensure_loaded", true);
  if (ensured != 0) return ensured > 0;
  const int used = apply_use(machine, term, directory, scan);
  if (used != 0) return used > 0;
  const int listed = apply_file_list(machine, term, directory, scan.opening);
  if (listed != 0) return listed > 0;
  const int directed =
      consume_directive(machine, reader, term, scan.snapshots, scan.apart, scan.inits, first, origin);
  if (directed != 0) return directed > 0;
  std::uint32_t module = machine.load_module;
  if (const auto formal = strip_clause(machine, term, module)) return fail_formal(machine, *formal) > 0;
  // M:(:- G) と M:(H --> B) は節ではない。外す前と同じく受けない。
  if (is_rule_arrow(machine.heap, term) || is_directive(machine.heap, term)) {
    fail_load(machine, message_text(machine, "directive_unsupported",
                                    {write_term(machine.heap, machine.operators, term, {}, {})}));
    return false;
  }
  Clause clause;
  if (!compile_clause(machine.heap, term, clause, read.names)) {
    fail_load(machine, message_text(machine, "directive_unsupported",
                                    {write_term(machine.heap, machine.operators, term, {}, {})}));
    return false;
  }
  // SPEC 第5節。組込と制御構造は system モジュールにあるので、どのモジュールへ足す節でも断る。
  // 翻訳の読みで断った節は、構文エラーと同じく足さずに読み進め、起動時の読み込みに判定を任せる（M10 の Codex レビュー）。
  if (const auto formal = refused_clause(machine, term, clause)) {
    if (mark_opaque(machine.translation)) return true;
    return fail_formal(machine, *formal) > 0;
  }
  if (!add_clause(machine, module, std::move(clause), scan.snapshots, scan.last, scan.apart)) return false;
  if (scan.last) note_site(machine, *scan.last, read.line);
  return true;
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

// 述語の持ち主になるファイルの番号（Predicate::source_file と同じ数え方）。無ければ足す。
std::uint32_t file_number(Impl& machine, const std::string& key) {
  const auto found = std::find(machine.source_files.begin(), machine.source_files.end(), key);
  if (found == machine.source_files.end()) {
    machine.source_files.push_back(key);
    return static_cast<std::uint32_t>(machine.source_files.size());
  }
  return static_cast<std::uint32_t>(found - machine.source_files.begin()) + 1;
}

// テキストを読み終えるまで、定義した述語をこのファイルのものとして書き留める（ADR 0020）。
struct OwnerScope {
  Impl& machine;
  std::uint32_t outer;
  OwnerScope(Impl& owner, std::uint32_t file) : machine(owner), outer(owner.load_owner) { owner.load_owner = file; }
  OwnerScope(const OwnerScope&) = delete;
  OwnerScope& operator=(const OwnerScope&) = delete;
  ~OwnerScope() { machine.load_owner = outer; }
};

bool read_terms(Impl& machine, std::string_view source, std::string_view directory, std::string_view origin,
                LoadScan& scan, TextRole role) {
  CycleGuard guard;
  if (!enter_file(machine, origin, scan, guard)) return false;
  const std::uint32_t file = !role.owns ? machine.load_owner : origin.empty() ? 0 : file_number(machine, guard.key);
  const OwnerScope owner{machine, file};
  const FileNote note{machine, origin, source};
  ModuleScope scope{machine};
  Reader reader(source, machine.heap, machine.operators, current_double_quotes(machine), &machine.char_conv);
  reader.set_fullwidth(machine.japanese_punct);
  bool first = true;
  while (true) {
    // module/2 と op/3 が見え方を替える。項ごとに、今のモジュールの見え方で読む。
    reader.set_operators(op_view(machine, machine.load_module));
    const std::size_t before = reader.consumed();
    const ReadOutcome outcome = reader.next();
    if (outcome.failure && !note.tolerate_syntax()) {
      return note_syntax(machine, origin, role.nested, *outcome.failure);
    }
    // 読み飛ばしが進まなければ、そこで終わりにする。同じ誤りを読み続けない。
    if (outcome.failure && reader.consumed() > before) continue;
    if (!outcome.term) {
      guard.finished = true;
      if (!scope.close(role.import_all)) return false;
      // 読み終えたファイルだけが、前に定義して今は定義しない述語を失う。include したテキストは読んだファイルの分。
      if (role.owns && file != 0) scan.snapshots.notes().files.push_back(file);
      return true;
    }
    note.first_term(*outcome.term);
    if (!take_term(machine, reader, *outcome.term, directory, origin, first, scan)) return false;
    first = false;
  }
}

// 読み込みの間だけ Impl::load_snapshots をこの読み込みの写しにし、抜けるときに親のものへ戻す。[File] と
// consult(File) の入れ子の読み込みは既存の節を置き換えるので、子の写しを親へ渡さないと親の失敗で戻せない。前は
// 指令の前に全部の述語を写していたので、その後のゴールが既にある述語へした変更まで戻した（ADR 0017）。
class SnapshotScope {
 public:
  SnapshotScope(Impl& machine, LoadSnapshots& own) : machine_(machine), parent_(machine.load_snapshots) {
    machine.load_snapshots = &own;
  }
  ~SnapshotScope() { machine_.load_snapshots = parent_; }
  SnapshotScope(const SnapshotScope&) = delete;
  SnapshotScope& operator=(const SnapshotScope&) = delete;
  [[nodiscard]] LoadSnapshots* parent() const { return parent_; }

 private:
  Impl& machine_;
  LoadSnapshots* parent_;
};

// 一番外の読み込みなら書き留めを使い、入れ子なら親へ渡す。
void keep_notes(Impl& machine, LoadSnapshots* parent, LoadSnapshots& own, std::uint64_t started_at) {
  if (parent == nullptr) apply_file_notes(machine, own.notes(), started_at);
  else hand_up_notes(*parent, own);
}

}  // namespace

LoadStatus load_source(Impl& machine, std::string_view source, std::string_view directory,
                       std::string_view origin, std::vector<std::string>* shared_opening) {
  // 実行中の consult は、クエリの変数と継続枠より上だけを捨てる。
  std::size_t heap_base = machine.heap.size();
  machine.diagnostic.clear();
  machine.syntax_diag = false;
  machine.load_kept = false;
  for (Predicate& predicate : machine.predicates) {
    predicate.seen = false;
    predicate.dynamic_declared = false;
    predicate.meta_declared = false;
  }
  const SavedOps saved_ops{machine.operators, machine.set_aside_ops};
  const SavedFlags saved_flags = save_flags(machine);
  const auto saved_declared = machine.declared;
  const auto saved_prepared = machine.prepared;
  const auto saved_modules = machine.modules;
  LoadSnapshots snapshots;
  const SnapshotScope shot_scope(machine, snapshots);
  // 次の読み込みには渡さない。宣言の無い述語は、これまでどおり置き換える。
  std::vector<Apart> apart;
  // テキストを準備し終えてから、ソースの順に実行する。
  std::vector<Cell> inits;
  // 指示と初期化のゴールは入れ子の run_loop で動き、GC がヒープを詰める。水位と後で呼ぶゴールを写させる。
  const RootScope roots(machine, [&heap_base, &inits](GcVisitor& visitor) {
    visitor.heap_mark(heap_base);
    for (Cell& goal : inits) visitor.cell(goal);
  });
  const auto previous_serial = machine.pred_serial;
  const auto started_at = machine.db_generation;
  std::optional<PredKey> last;
  std::vector<std::string> local_opening;
  std::vector<std::string>& opening = shared_opening == nullptr ? local_opening : *shared_opening;
  LoadScan scan{snapshots, apart, inits, last, opening};
  if (!read_terms(machine, source, directory, origin, scan, TextRole{false, true, true})) {
    if (machine.load_kept) {
      if (!origin.empty()) restore_file_flags(machine, saved_flags);
      machine.heap.truncate(heap_base);
      // 初期化の例外で残した節も、成功した読み込みの節と同じく持ち主を付け、古い述語を消す（L11 の敵対的レビュー A3）。
      keep_notes(machine, shot_scope.parent(), snapshots, started_at);
      return LoadStatus{false, machine.diagnostic};
    }
    return reject_load(machine, saved_ops, saved_flags, snapshots, previous_serial, heap_base, saved_declared,
                       saved_prepared, saved_modules);
  }
  // 文字だけの読み込み（mosatest の指令の断片など）はファイルではないので、旗を戻さない。
  // initialization のゴールより先に戻す（iso-notes 7.4.2.8 の決定 B）。
  if (!origin.empty()) restore_file_flags(machine, saved_flags);
  // 古い述語を消すのは一番外の読み込みだけで、初期化のゴールより先（ADR 0020）。入れ子の読み込みが消すと、後で外の
  // 読み込みが失敗したとき戻せない。初期化のゴールが consult したファイルの分は、ゴールの後でもう一度使う。
  const bool outermost = shot_scope.parent() == nullptr;
  if (outermost) apply_file_notes(machine, snapshots.notes(), started_at);
  const LoadStatus status = finish_load(machine, heap_base, inits);
  // 初期化のゴールの後で渡す。そのゴールが読んだ孫の写しも、この写しへ渡されている（2 度目の Codex レビュー P2）。
  // 初期化のゴールが失敗しても節は戻さないが、親が失敗すれば親が戻す。
  if (outermost) apply_file_notes(machine, snapshots.notes(), started_at);
  else hand_up_snapshots(*shot_scope.parent(), snapshots);
  return status;
}

LoadStatus Machine::load_program(std::string_view source) {
  // 問い合わせと同じく、前の分の空きを生まない GC の数を持ち越さない（ADR 0010 の改訂 B）。
  reset_gc_judgement(*impl_);
  return load_source(*impl_, source);
}

}
