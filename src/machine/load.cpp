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

// 読み込みの前の状態。失敗した読み込みは、ここへ戻す（ADR 0017）。
struct SavedLoad {
  SavedOps ops;
  SavedFlags flags;
  std::vector<SharedPred> declared;
  std::vector<std::string> prepared;
  std::vector<Module> modules;
  std::size_t serial = 0;
};

SavedLoad save_load(const Impl& machine) {
  return SavedLoad{SavedOps{machine.operators, machine.set_aside_ops}, save_flags(machine), machine.declared,
                   machine.prepared, machine.modules, machine.pred_serial};
}

LoadStatus reject_load(Impl& machine, const SavedLoad& saved, const LoadSnapshots& snapshots, std::size_t heap_base) {
  machine.operators = saved.ops.table;
  machine.set_aside_ops = saved.ops.aside;
  // 途中で作ったモジュールと取り込みを残さない。半分だけ登録されたモジュールが見えると解決が変わる。
  machine.modules = saved.modules;
  restore_flags(machine, saved.flags);
  machine.declared = saved.declared;
  machine.prepared = saved.prepared;
  rollback_preds(machine, snapshots, saved.serial);
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
  // open_prolog_file が開けたファイルの数。ゴールの読み込みの診断が、開けなかったのが根のファイルかを見分ける。
  std::size_t opened = 0;
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
    // 翻訳の読み込みでは、分岐の中の指令の失敗を任せて読み続けることがある（CondFrames::tolerate、I2）。そのときも
    // 途中までのファイルだけを読み直させる。任せない失敗は、外の reject_load が prepared を読み込みの前へ戻す。
    if (machine == nullptr || finished || (!machine->load_kept && machine->translation == nullptr)) return;
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
  ++scan.opened;
  // 同じ走査に足す。別の load にすると、節の置き換えと初期化の区切りが変わる。
  if (!read_terms(machine, *text, utf8_from_path(file.parent_path()), utf8_from_path(file), scan, role)) return -1;
  return 1;
}

int fail_formal(Impl& machine, Cell formal) {
  fail_load(machine, write_term(machine.heap, machine.operators, formal, {}, {}));
  return -1;
}

// 指令の use_module/1,2 と reexport/1,2 の本体。reexport は指令だけで、ゴールには振り分けない（ADR 0026）。
std::optional<Cell> use_body(Impl& machine, Cell term) {
  if (!is_directive(machine.heap, term)) return std::nullopt;
  const Cell body = machine.heap.deref(machine.heap.arg(machine.heap.deref(term), 0));
  if (body.tag() != Tag::Str) return std::nullopt;
  const std::string_view name = machine.atoms.name(machine.heap.functor_atom(body));
  if (name != "use_module" && name != "reexport") return std::nullopt;
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
  if (const auto formal = check_use_spec(machine, arg)) return formal;
  if (names_library(machine, arg)) {
    const Cell name = machine.heap.deref(machine.heap.arg(arg, 0));
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
  file = source_path(machine, resolve_include(directory, machine.atoms.name(static_cast<std::uint32_t>(arg.raw() >> 3))));
  bundled = locate_bundled_plain(machine, file);
  return std::nullopt;
}

// locate_use で見つけたファイルを、準備済みでなければ読み、spec のとおり取り込む。指令は読み込み中のテキストの入れ子
// として、ゴールは根のテキストとして読む（nested）。arg は呼び手が GC の根に登録した変数で、読んだ後も使う。
int use_file(Impl& machine, const Cell& arg, const std::filesystem::path& file,
             const std::optional<std::string>& bundled, const ImportSpec& spec, LoadScan& scan, bool nested) {
  if (!file_ready(machine, scan, file)) {
    // library(...) で見つけたファイルは、利用者の表でなく ISO の核と自分のモジュールの演算子で読む（ADR 0006 の
    // 改訂 1）。利用者が -> や :- を変えた後でも、ライブラリの節を別の項に読まない。
    std::optional<CoreOperatorScope> core;
    if (names_library(machine, arg)) core.emplace(machine);
    if (open_prolog_file(machine, file, scan, TextRole{nested, false, true}) < 0) return -1;
    if (core) core->finish();
    // 同梱ライブラリの翻訳済みの単位は、原文を読み終えてから付ける（ADR 0009）。
    if (bundled) attach_bundled_library(machine, *bundled);
  }
  const Module* module = module_of_file(machine, file);
  if (module == nullptr) return fail_formal(machine, formal_permission(machine, "load", "source_sink", arg));
  // デバッガがライブラリの中の呼び出しを見せないための印（ADR 0028）。
  if (names_library(machine, arg) || bundled) find_module(machine, module->name)->library = true;
  if (const auto formal = apply_imports(machine, module->name, machine.load_module, spec)) {
    return fail_formal(machine, *formal);
  }
  return 1;
}

// use_module/1,2 と reexport/1,2。ensure_loaded と同じく読むのは 1 回だけで、取り込みは呼ぶたびに足す。
int apply_use(Impl& machine, Cell term, std::string_view directory, LoadScan& scan) {
  const auto body = use_body(machine, term);
  if (!body) return 0;
  // 根に登録するのは arg だけなので、読む前に見分けておく。読んだ後は body を見ない。
  const bool reexport = machine.atoms.name(machine.heap.functor_atom(*body)) == "reexport";
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
  const int used = use_file(machine, arg, file, bundled, spec, scan, true);
  if (used < 0 || !reexport) return used;
  // 取り込めたので、同じ項目を読み込み中のモジュールの公開に足す。use_file が成功したならモジュールのファイルである。
  const std::uint32_t from = module_of_file(machine, file)->name;
  if (const auto formal = add_reexports(machine, from, machine.load_module, spec)) return fail_formal(machine, *formal);
  return 1;
}

// ensure_loaded のファイル。準備済みなら節も初期化も足さず、公開述語だけを取り込み直す。
int ensure_file(Impl& machine, const std::filesystem::path& file, LoadScan& scan, bool nested) {
  if (file_ready(machine, scan, file)) return import_ready(machine, file);
  return open_prolog_file(machine, file, scan, TextRole{nested, true, true});
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
  if (once) return ensure_file(machine, file, scan, true);
  return open_prolog_file(machine, file, scan, TextRole{true, true, false});
}

// ゴールの読み込み 1 回（ADR 0026）。body は呼び手が GC の根に登録した use_module(...) か ensure_loaded(...)。file は
// 開いたパス。prefix_file は、失敗の診断に「file: 」を付けること（計画の「設計との違い」6）。
struct GoalLoad {
  Cell& body;
  std::filesystem::path file;
  bool prefix_file = true;
};

// 根のファイルが開けなかったときだけ、run_consult と同じく cannot_open_file の文面にパスを重ねない。準備済みの
// ファイルは開かずに取り込みで失敗したので、パスを付ける。
bool goal_failed(GoalLoad& load, const LoadScan& scan, bool ready) {
  load.prefix_file = ready || scan.opened > 0;
  return false;
}

// ゴールの use_module/1,2。相対パスは作業ディレクトリから開き（directory は空）、ファイルは根のテキストとして読む。
// 引数は run_load_goal が検査済みなので、locate_use の誤りはライブラリが見つからないことだけである。
bool goal_use(Impl& machine, GoalLoad& load, LoadScan& scan) {
  Cell arg = machine.heap.deref(machine.heap.arg(load.body, 0));
  const RootScope roots(machine, [&arg](GcVisitor& visitor) { visitor.cell(arg); });
  std::optional<std::string> bundled;
  if (const auto formal = locate_use(machine, arg, {}, load.file, bundled)) {
    // ファイルを探す前の誤りには、開いたパスが無いので Spec を書いた字面を付ける。
    const std::string spec_text = write_term(machine.heap, machine.operators, arg, {}, {});
    fail_formal(machine, *formal);
    machine.diagnostic = spec_text + ": " + machine.diagnostic;
    load.prefix_file = false;
    return false;
  }
  ImportSpec spec;
  if (machine.heap.functor_arity(load.body) == 2) {
    if (const auto formal = parse_imports(machine, machine.heap.arg(load.body, 1), spec)) {
      load.prefix_file = false;
      return fail_formal(machine, *formal) > 0;
    }
  }
  const bool ready = file_ready(machine, scan, load.file);
  if (use_file(machine, arg, load.file, bundled, spec, scan, false) > 0) return true;
  return goal_failed(load, scan, ready);
}

// ゴールの ensure_loaded/1。パスの基準と読み方は goal_use と同じ。引数は検査済みの原子である。
bool goal_ensure(Impl& machine, GoalLoad& load, LoadScan& scan) {
  const Cell arg = machine.heap.deref(machine.heap.arg(load.body, 0));
  load.file = source_path(machine, resolve_include({}, machine.atoms.name(static_cast<std::uint32_t>(arg.raw() >> 3))));
  const bool ready = file_ready(machine, scan, load.file);
  if (ensure_file(machine, load.file, scan, false) > 0) return true;
  return goal_failed(load, scan, ready);
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

bool take_term(Impl& machine, Reader& reader, const ReadTerm& read, std::string_view directory,
               std::string_view origin, bool first, LoadScan& scan) {
  // mosatest の test/3 は節にせず横取りする（suite.cpp）。指示より先に見るので、横取りした項は何も変えない。
  if (machine.term_hook && machine.term_hook(read, origin)) return true;
  if (machine.translation != nullptr) machine.translation->clause_module.reset();
  Cell term = read.term;
  // 文法規則は訳した節として、以下の節と同じ道を通す（ADR 0024）。
  if (is_rule_arrow(machine.heap, term)) {
    const DcgOut translated = dcg_rule(machine, term);
    if (translated.formal) return fail_formal(machine, *translated.formal) > 0;
    term = translated.term;
  }
  note_goal_libraries(machine, term);
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
  if (machine.translation != nullptr) machine.translation->clause_module = module;
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
  // 分岐が残した物で足せない節は、起動時には足せるかもしれない（I2 の直し 4 回目）。
  if (!add_clause(machine, module, std::move(clause), scan.snapshots, scan.last, scan.apart)) {
    return tolerate_after_block(machine);
  }
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
  FileNote note{machine, origin, source};
  ModuleScope scope{machine};
  // if と endif の対はテキストごとに閉じる（ADR 0027）。include したテキストも自分の積みを持つ。
  CondFrames cond{machine};
  Reader reader(source, machine.heap, machine.operators, current_double_quotes(machine), &machine.char_conv);
  reader.set_fullwidth(machine.japanese_punct);
  bool first = true;
  while (true) {
    // module/2 と op/3 が見え方を替える。項ごとに、今のモジュールの見え方で読む。
    reader.set_operators(op_view(machine, machine.load_module));
    const std::size_t before = reader.consumed();
    const ReadOutcome outcome = reader.next();
    // 読み飛ばし中の構文の誤りは、診断を出さずに下の読み飛ばしの道へ進む（問い 3 の A）。tolerate_syntax より先に
    // 見て、翻訳の印（opaque）も付けない。起動時には取らないかもしれない分岐の中なら、節の誤りと同じく任せる（I2）。
    if (outcome.failure && !cond.skipping() && !note.tolerate_syntax() && !cond.tolerate(cond.tolerable())) {
      return note_syntax(machine, origin, role.nested, *outcome.failure);
    }
    // 読み飛ばしが進まなければ、そこで終わりにする。同じ誤りを読み続けない。
    if (outcome.failure && reader.consumed() > before) continue;
    if (!outcome.term) {
      if (!cond.close()) return false;
      guard.finished = true;
      note.own_module = machine.load_module != scope.outer;
      if (!scope.close(role.import_all)) return false;
      // 読み終えたファイルだけが、前に定義して今は定義しない述語を失う。include したテキストは読んだファイルの分。
      if (role.owns && file != 0) scan.snapshots.notes().files.push_back(file);
      return true;
    }
    note.first_term(*outcome.term);
    // 条件の指令と読み飛ばす項は take_term へ渡さない。mosatest の横取り（term_hook）と文法規則の変換より先に見る。
    // どちらの後も、module/2 はもうファイルの最初の項ではない。
    const int stepped = cond.step(outcome.term->term);
    if (stepped < 0) return false;
    if (stepped == 0) {
      // 翻訳の読みで、起動時には取らないかもしれない分岐の指令か節が失敗したら、起動時の読み込みに任せる（I2）。
      // 開いたファイルの数は、その項の指令がファイルを開く前に取る。
      const auto tolerable = cond.tolerable();
      const bool clause = !is_directive(machine.heap, outcome.term->term);
      if (!take_term(machine, reader, *outcome.term, directory, origin, first, scan) &&
          !cond.tolerate(tolerable, clause)) {
        return false;
      }
    }
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

// C++ の例外（ヒープの上限）でテキストの読みを抜けるときも、失敗した読み込みと同じく戻してから投げ直す。前は戻さず、
// 途中まで読んだモジュールと節と準備済みの印が残り、次の use_module がその半分の読み込みを読み直さずに使った
// （フェーズ12 の Muse レビュー Major 1、0.2.2 から）。入れ子なら、外の読み込みも同じ例外を受けて自分の分を戻す。
template <typename Read>
bool read_or_reject(Impl& machine, const SavedLoad& saved, const LoadSnapshots& snapshots,
                    const std::size_t& heap_base, const Read& read, LoadScan& scan) {
  try {
    return read(scan);
  } catch (...) {
    (void)reject_load(machine, saved, snapshots, heap_base);
    throw;
  }
}

// 初期化のゴールを呼び、その後で写しを渡す。そのゴールが読んだ孫の写しも、この写しへ渡されている（2 度目の Codex
// レビュー P2）。初期化のゴールが失敗しても節は戻さないが、親が失敗すれば親が戻す。C++ の例外（ヒープの上限）で
// 抜けるときも、Prolog の例外と同じくヒープを水位まで戻し、節を残して写しを渡す。渡さないと、同じ例外で抜ける親の
// 巻き戻しが、この読み込みの置き換えた節を戻せない。load_kept は立てない。親は同じ例外を受けて自分の分ごと戻すので、
// 親に節を残させる印は要らない。
LoadStatus finish_and_hand_over(Impl& machine, const std::size_t& heap_base, const std::vector<Cell>& inits,
                                LoadSnapshots* parent, LoadSnapshots& snapshots, std::uint64_t started_at) {
  const auto hand_over = [&] {
    if (parent == nullptr) apply_file_notes(machine, snapshots.notes(), started_at);
    else hand_up_snapshots(*parent, snapshots);
  };
  LoadStatus status;
  try {
    status = finish_load(machine, heap_base, inits);
  } catch (...) {
    machine.heap.truncate(heap_base);
    hand_over();
    throw;
  }
  hand_over();
  return status;
}

// 読み込み 1 回の退避と巻き戻し（ADR 0017）。read がこの読み込みのテキストを scan へ読み、偽なら読み込みは失敗する。
// file_text は、読んだテキストがファイルであること。ファイルの字句の旗だけを、読み終えたら戻す（SPEC 第5節）。
template <typename Read>
LoadStatus load_once(Impl& machine, bool file_text, std::vector<std::string>* shared_opening, const Read& read) {
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
  const SavedLoad saved = save_load(machine);
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
  const auto started_at = machine.db_generation;
  std::optional<PredKey> last;
  std::vector<std::string> local_opening;
  std::vector<std::string>& opening = shared_opening == nullptr ? local_opening : *shared_opening;
  LoadScan scan{snapshots, apart, inits, last, opening, 0};
  if (!read_or_reject(machine, saved, snapshots, heap_base, read, scan)) {
    if (machine.load_kept) {
      if (file_text) restore_file_flags(machine, saved.flags);
      machine.heap.truncate(heap_base);
      // 初期化の例外で残した節も、成功した読み込みの節と同じく持ち主を付け、古い述語を消す（L11 の敵対的レビュー A3）。
      keep_notes(machine, shot_scope.parent(), snapshots, started_at);
      return LoadStatus{false, machine.diagnostic};
    }
    return reject_load(machine, saved, snapshots, heap_base);
  }
  // initialization のゴールより先に戻す（iso-notes 7.4.2.8 の決定 B）。
  if (file_text) restore_file_flags(machine, saved.flags);
  // 古い述語を消すのは一番外の読み込みだけで、初期化のゴールより先（ADR 0020）。入れ子の読み込みが消すと、後で外の
  // 読み込みが失敗したとき戻せない。初期化のゴールが consult したファイルの分は、ゴールの後でもう一度使う。
  if (shot_scope.parent() == nullptr) apply_file_notes(machine, snapshots.notes(), started_at);
  return finish_and_hand_over(machine, heap_base, inits, shot_scope.parent(), snapshots, started_at);
}

// halt/0,1 で止まった読み込みは失敗ではなく終わりで、診断は空か、指令が吸った前の失敗の字面が残っているだけである。
// どの入口（Machine::consult、load_program、ゴールの読み込み）にも残さないよう、読み込みの出口 1 か所で空にする
// （台帳 L74。前は「F: 」や、吸われた字面に外側のパスを付けた物が diagnostic() に残った）。C++ の例外（ヒープの上限）
// で抜ける読み込みも同じく空にする。理由は例外が持ち、残っているのは指示が吸った失敗の字面だけである（2 度目の Muse
// レビュー）。
template <typename Read>
LoadStatus run_load(Impl& machine, bool file_text, std::vector<std::string>* shared_opening, const Read& read) {
  LoadStatus status;
  try {
    status = load_once(machine, file_text, shared_opening, read);
  } catch (...) {
    machine.diagnostic.clear();
    machine.syntax_diag = false;
    throw;
  }
  if (status.ok || !machine.halt_now) return status;
  machine.diagnostic.clear();
  machine.syntax_diag = false;
  status.message.clear();
  return status;
}

}  // namespace

LoadStatus load_source(Impl& machine, std::string_view source, std::string_view directory,
                       std::string_view origin, std::vector<std::string>* shared_opening) {
  // 文字だけの読み込み（mosatest の指令の断片など）はファイルではないので、旗を戻さない。
  return run_load(machine, !origin.empty(), shared_opening, [&](LoadScan& scan) {
    return read_terms(machine, source, directory, origin, scan, TextRole{false, true, true});
  });
}

std::optional<Cell> check_use_spec(Impl& machine, Cell spec) {
  spec = machine.heap.deref(spec);
  if (machine.heap.unbound(spec)) return formal_instantiation(machine);
  if (names_library(machine, spec)) {
    const Cell name = machine.heap.deref(machine.heap.arg(spec, 0));
    if (machine.heap.unbound(name)) return formal_instantiation(machine);
    if (name.tag() != Tag::Atom) return formal_type(machine, "atom", name);
    return std::nullopt;
  }
  if (spec.tag() != Tag::Atom) return formal_type(machine, "atom", spec);
  return std::nullopt;
}

LoadStatus load_goal_term(Impl& machine, Cell directive) {
  Cell body = machine.heap.deref(machine.heap.arg(machine.heap.deref(directive), 0));
  // ファイルの指示と初期化のゴールが GC を起こしうる。読んだ後の取り込みで body の引数を使うので登録する。
  const RootScope roots(machine, [&body](GcVisitor& visitor) { visitor.cell(body); });
  const bool ensure = machine.atoms.name(machine.heap.functor_atom(body)) == "ensure_loaded";
  GoalLoad load{body, {}, true};
  // 読んだファイルの字句の旗は、ファイルの consult と同じく戻す（file_text）。
  const LoadStatus status = run_load(machine, true, nullptr, [&machine, &load, ensure](LoadScan& scan) {
    return ensure ? goal_ensure(machine, load, scan) : goal_use(machine, load, scan);
  });
  if (status.ok || !load.prefix_file) return status;
  // 開いたファイルの中の誤りと読んだ後の誤りは、mosa F と同じく開いたパスを付ける（構文の誤りなら行と桁も）。
  prefix_consult_diagnostic(machine, utf8_from_path(load.file));
  return LoadStatus{false, machine.diagnostic};
}

LoadStatus Machine::load_program(std::string_view source) {
  // 問い合わせと同じく、前の分の空きを生まない GC の数を持ち越さない（ADR 0010 の改訂 B）。
  reset_gc_judgement(*impl_);
  // 前の問い合わせが halt で止まった印は、読み込みの間だけ脇に置き、抜けるとき戻す（HaltSetAside）。
  const HaltSetAside set_aside(*impl_);
  return load_source(*impl_, source);
}

}
