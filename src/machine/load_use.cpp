// ファイルを開く指示とゴール: 指示の include/1、ensure_loaded/1、use_module/1,2、reexport/1,2 と、ゴールの
// use_module/1,2、ensure_loaded/1（ADR 0026）。load.cpp が 800 行の上限に近いので、読み込み 1 回の退避と巻き戻し
// （load_once）とテキストの走査（read_terms）から分けた。振る舞いは変えない。

#include "load_scan.hpp"

#include "mosa/library_path.hpp"
#include "mosa/path_text.hpp"
#include "mosa/writer.hpp"

#include <algorithm>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace mosa {

// 定義は load_path.cpp。include と ensure_loaded は同じ経路で開く。
std::filesystem::path resolve_include(std::string_view directory, std::string_view atom);
std::filesystem::path canonical_key(const std::filesystem::path& file);

namespace {

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

// 指示で読むファイルを読む間だけ、この読み込みの深さを 1 つ上げる。その中のゴールの読み込みは、その上に数える。
class NestedDepth {
 public:
  explicit NestedDepth(LoadSnapshots& snapshots) : snapshots_(snapshots), outer_(snapshots.depth()) {
    snapshots.set_depth(outer_ + 1);
  }
  ~NestedDepth() { snapshots_.set_depth(outer_); }
  NestedDepth(const NestedDepth&) = delete;
  NestedDepth& operator=(const NestedDepth&) = delete;

 private:
  LoadSnapshots& snapshots_;
  std::size_t outer_;
};

// 指示の include/1、ensure_loaded/1、use_module/1,2 が読むファイルも、同時に開いている読み込みに数える。今の読み込みの
// 中で読み、load_once の入れ子にならないので、前は数えず、その鎖は 1 MiB のスタックで 437 段で落ちた（レビュー C-01 の
// 2 つ目の持ち主の決定 B）。ゴールが読む根のテキスト（nested が偽）は、そのゴールの load_once が数えている。
int open_prolog_file(Impl& machine, const std::filesystem::path& file, LoadScan& scan, TextRole role) {
  std::optional<NestedDepth> depth;
  if (role.nested) {
    if (too_deep(machine, scan.snapshots.depth() + 1)) return -1;
    depth.emplace(scan.snapshots);
  }
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
  const auto name = machine.atoms.name(static_cast<std::uint32_t>(arg.raw() >> 3));
  file = source_path(machine, resolve_include(directory, name));
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
    if (open_prolog_file(machine, file, scan, TextRole{nested, false, true}) < 0) {
      // 節を残した失敗（入れ子の初期化の例外、load_kept）は、退けた user の表の op/3 も残す。前は範囲が抜けるとき
      // 戻し、モジュールと節は残るのに user の演算子だけが消えた（レビュー L-22）。断られた読み込みは戻す。
      if (core && machine.load_kept) core->finish();
      return -1;
    }
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

// ensure_loaded のファイル。準備済みなら節も初期化も足さず、公開述語だけを取り込み直す。
int ensure_file(Impl& machine, const std::filesystem::path& file, LoadScan& scan, bool nested) {
  if (file_ready(machine, scan, file)) return import_ready(machine, file);
  return open_prolog_file(machine, file, scan, TextRole{nested, true, true});
}

// 根のファイルが開けなかったときだけ、run_consult と同じく cannot_open_file の文面にパスを重ねない。準備済みの
// ファイルは開かずに取り込みで失敗したので、パスを付ける。
bool goal_failed(GoalLoad& load, const LoadScan& scan, bool ready) {
  load.prefix_file = ready || scan.opened > 0;
  return false;
}

}  // namespace

int fail_formal(Impl& machine, Cell formal) {
  fail_load(machine, write_term(machine.heap, machine.operators, formal, {}, {}));
  return -1;
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

}  // namespace mosa
