// ゴールの読み込み: consult/1（ISO に無い）と、フェーズ12の use_module/1,2・ensure_loaded/1（ADR 0026、SPEC 第5節）。
// use_module と ensure_loaded のゴールは 13211-1 に条項は無い。13211-2 での位置は未確認。
// モード: consult(+atom)、use_module(+Spec)、use_module(+Spec, +Imports)、ensure_loaded(+atom)。
//         Spec は library(Name) か原子。
// エラー: instantiation_error、type_error(atom, X)、type_error(list, L)、type_error(predicate_indicator, PI)。
//         ほかの失敗は診断を user_error に書いて失敗する（iso-notes の問い 1 の A）。
// テスト: tests/pl/modules.pl、tests/pl/directives.pl、tests/pl/japanese.pl、tests/pl/failed_load.pl、
//         tests/cpp/session_goals_test.cpp
#include "internal.hpp"

#include "mosa/writer.hpp"
#include "mosa/path_text.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <iostream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace mosa {

void prefix_consult_diagnostic(Impl& machine, std::string_view path) {
  // halt で止まった読み込みの診断は run_load が空にした。パスだけを付けると「F: 」になる。
  if (machine.halt_now) return;
  if (machine.syntax_diag) {
    machine.diagnostic = std::string{path} + ":" + std::to_string(machine.diag_line) + ":" +
                         std::to_string(machine.diag_column) + ": " + machine.diagnostic;
    return;
  }
  machine.diagnostic = std::string{path} + ": " + machine.diagnostic;
}

// 埋め込み（セッションが無い）では書き先が無いので std::cerr に書く。どちらも user_error の流れを通らないので、その
// 桁は進まない（Echo::warn と同じ）。覚えた字面は、問い合わせ全体が失敗したときにトップレベルが繰り返さないために使う。
void report_goal_load_failure(Impl& machine) {
  const std::string line = machine.diagnostic + "\n";
  if (machine.load_report) {
    machine.load_report(line);
  } else {
    std::cerr << line;
    std::cerr.flush();
  }
  machine.reported_diagnostic = machine.diagnostic;
}

namespace {

void note_failure(Impl& machine, std::string message) {
  machine.diagnostic = std::move(message);
  machine.syntax_diag = false;
}

// 相対の include は、開いたファイルのディレクトリから探す。
LoadStatus load_consulted(Impl& machine, std::string_view path, std::string_view source) {
  const std::filesystem::path file = path_from_utf8(path);
  return load_source(machine, source, utf8_from_path(file.parent_path()), utf8_from_path(file));
}

// 相対パスは、今読んでいるファイルのディレクトリから開く。文字だけの load は、渡されたパスのまま。
std::filesystem::path resolve_file(std::string_view directory, std::string_view atom) {
  const std::filesystem::path file = path_from_utf8(atom);
  if (file.is_absolute() || directory.empty()) return file;
  return path_from_utf8(directory) / file;
}

// 印は述語の名前（モジュール・名前・項数）で持つ。印は「親のテキストがこの名前に節を与えたか」で、子のテキストが
// 述語を消して作り直しても親の印は残る。添字で持つと、子の読み込みが abolish で述語を表から消したとき、後ろの述語に
// 隣の印が付く（M19 で見つけた不具合）。Predicate::serial で持つと、子が消して作り直した述語に親の印が付かず、親の
// 続きの節が子の節を置き換えて述語を静的にする（その直しの敵対的レビュー A1）。親のテキストで節が離れていれば、
// 子が作り直しても clauses_apart になる（同 B1）。子が消したままの述語は、親のテキストの abolish と同じく印を失う。
struct SeenMark {
  PredKey key;
  std::size_t serial = 0;
  bool seen = false;
  bool declared = false;
  bool meta = false;
};

bool key_less(const PredKey& a, const PredKey& b) {
  return std::tie(a.module, a.name, a.arity) < std::tie(b.module, b.name, b.arity);
}

// 子の読み込みの前の印と、モジュールが保留していた meta_predicate の名前（名前の順）。
struct SavedMarks {
  std::vector<SeenMark> marks;
  std::vector<PredKey> pending;
};

// 述語の表は serial の昇順（M18）なので、写しも serial の昇順になる。
SavedMarks mark_seen(const Impl& machine) {
  SavedMarks saved;
  saved.marks.reserve(machine.predicates.size());
  for (const Predicate& predicate : machine.predicates) {
    const PredKey key{predicate.module, predicate.name, predicate.arity};
    saved.marks.push_back(
        SeenMark{key, predicate.serial, predicate.seen, predicate.dynamic_declared, predicate.meta_declared});
  }
  for (const Module& module : machine.modules) {
    for (const PendingMeta& item : module.pending_meta) saved.pending.push_back(PredKey{module.name, item.name, item.arity});
  }
  std::sort(saved.pending.begin(), saved.pending.end(), key_less);
  return saved;
}

void put_mark(Predicate& predicate, const SeenMark* mark) {
  // 写しに無い名前は子の読み込みが初めて作った。親のテキストはまだ見ていないので、次の節は置き換える。
  predicate.seen = mark != nullptr && mark->seen;
  predicate.dynamic_declared = mark != nullptr && mark->declared;
  predicate.meta_declared = mark != nullptr && mark->meta;
}

// 表と写しを serial の順に並べて歩く。同じ serial は子の前からある述語。子が消した述語の印だけを名前で並べ、子が
// 作った述語はそこから同じ名前を探す。全部を名前で並べ替えると、consult のたびに述語の数 n について n log n かかる
// （敵対的レビュー B3）。
// 子が作った述語が親の保留した meta_predicate を引き取っていれば、それは親のテキストの宣言なので印を付ける。付けない
// と、親の次の節が子の節を置き換えるとき宣言も消す（M24 の敵対的レビュー A2。子のゴールの述語は、子が失敗しても残る）。
void restore_seen(Impl& machine, const SavedMarks& saved) {
  std::vector<const SeenMark*> gone;
  std::vector<Predicate*> created;
  auto mark = saved.marks.begin();
  for (Predicate& predicate : machine.predicates) {
    for (; mark != saved.marks.end() && mark->serial < predicate.serial; ++mark) gone.push_back(&*mark);
    if (mark != saved.marks.end() && mark->serial == predicate.serial) {
      put_mark(predicate, &*mark);
      ++mark;
    } else {
      created.push_back(&predicate);
    }
  }
  for (; mark != saved.marks.end(); ++mark) gone.push_back(&*mark);
  const auto before = [](const SeenMark* a, const SeenMark* b) { return key_less(a->key, b->key); };
  std::sort(gone.begin(), gone.end(), before);
  for (Predicate* predicate : created) {
    const SeenMark wanted{PredKey{predicate->module, predicate->name, predicate->arity}};
    const auto found = std::lower_bound(gone.begin(), gone.end(), &wanted, before);
    const bool kept = found != gone.end() && !key_less(wanted.key, (*found)->key);
    put_mark(*predicate, kept ? *found : nullptr);
    if (!kept && std::binary_search(saved.pending.begin(), saved.pending.end(), wanted.key, key_less)) {
      predicate->meta_declared = true;
    }
  }
}

bool unbound_item(const Impl& machine, const WalkedList& walked) {
  for (const Cell item : walked.items) {
    if (machine.heap.unbound(item)) return true;
  }
  return false;
}

std::string formal_message(Impl& machine, Cell formal) {
  return write_term(machine.heap, machine.operators, formal, {}, {});
}

// :- consult(File) は :- [File] と同じ。相対パスは、読み込み中のファイルのディレクトリから開く。
int collect_consulted(Impl& machine, Cell body, std::vector<Cell>& items) {
  const Cell file = machine.heap.deref(machine.heap.arg(body, 0));
  if (machine.heap.unbound(file)) {
    note_failure(machine, formal_message(machine, formal_instantiation(machine)));
    return -1;
  }
  if (file.tag() != Tag::Atom) {
    note_failure(machine, formal_message(machine, formal_type(machine, "atom", file)));
    return -1;
  }
  items = {file};
  return 1;
}

// 1 はファイルのリスト。0 はリストではない。-1 はこの指示のエラー。
int collect_files(Impl& machine, Cell term, std::vector<Cell>& items) {
  term = machine.heap.deref(term);
  if (!is_directive(machine.heap, term)) return 0;
  const Cell body = machine.heap.deref(machine.heap.arg(term, 0));
  if (body.tag() == Tag::Str && machine.heap.functor_arity(body) == 1 &&
      machine.atoms.name(machine.heap.functor_atom(body)) == "consult") {
    return collect_consulted(machine, body, items);
  }
  const WalkedList walked = walk_list(machine, body);
  if (walked.shape == ListShape::Variable) return 0;
  if (walked.shape == ListShape::Partial || unbound_item(machine, walked)) {
    note_failure(machine, formal_message(machine, formal_instantiation(machine)));
    return -1;
  }
  if (walked.shape != ListShape::Proper) {
    if (body.tag() != Tag::Lis) return 0;
    note_failure(machine, formal_message(machine, formal_type(machine, "list", body)));
    return -1;
  }
  for (const Cell item : walked.items) {
    if (item.tag() == Tag::Atom) continue;
    note_failure(machine, formal_message(machine, formal_type(machine, "atom", item)));
    return -1;
  }
  items = walked.items;
  return 1;
}

int consult_one(Impl& machine, const std::filesystem::path& file, std::vector<std::string>& opening) {
  const auto text = read_source(machine, file);
  if (!text) {
    note_failure(machine, message_text(machine, "cannot_open_file", {utf8_from_path(file)}));
    return -1;
  }
  const auto saved = mark_seen(machine);
  const LoadStatus status =
      load_source(machine, *text, utf8_from_path(file.parent_path()), utf8_from_path(file), &opening);
  restore_seen(machine, saved);
  if (status.ok) return 1;
  prefix_consult_diagnostic(machine, utf8_from_path(file));
  return -1;
}

// ゴールの読み込みの間だけ load_module を替え、抜けるとき戻す。C++ の例外（ヒープの上限）で抜けても戻す。前は通常の
// 道でだけ戻したので、例外の後の節と取り込みが、ゴールを呼んだ文脈のモジュールへ入った（フェーズ12 の Muse レビュー
// Major 1。ゴールの consult/1 では 0.2.2 から）。
class LoadModuleScope {
 public:
  LoadModuleScope(Impl& machine, std::uint32_t module) : machine_(machine), outer_(machine.load_module) {
    machine.load_module = module;
  }
  ~LoadModuleScope() { machine_.load_module = outer_; }
  LoadModuleScope(const LoadModuleScope&) = delete;
  LoadModuleScope& operator=(const LoadModuleScope&) = delete;

 private:
  Impl& machine_;
  std::uint32_t outer_;
};

// 公開述語は、ゴールを呼んだ文脈モジュールへ取り込む。読み込みの中の指示から呼ぶと、子の読み込みが親の印を落とす。
// [File] と同じく戻す（M19 で見つけた不具合）。戻さないと、親の次の節が自分の前の節を置き換え、dynamic の宣言も
// 忘れる。読み込みの外では印は使わない。C++ の例外で抜けるときも戻す。読み込みの中で例外を受けて読み続ける所が
// あれば、戻さない印で親の次の節が置き換えになる。restore_seen は確保するので、デストラクタでなく catch で戻す。
template <typename Load>
bool load_in_module(Impl& machine, std::uint32_t module, const Load& load) {
  const LoadModuleScope scope(machine, module);
  const auto saved = mark_seen(machine);
  bool loaded = false;
  try {
    loaded = load();
  } catch (...) {
    restore_seen(machine, saved);
    throw;
  }
  restore_seen(machine, saved);
  return loaded;
}

// ゴールの読み込み（consult/1、use_module/1,2、ensure_loaded/1）の共通の手順。片方だけを直して、二つのゴールの読み方が
// ずれないように 1 か所に置く。load は読み込み 1 回を動かし、失敗なら診断の字面を整えて偽を返す。extra は、読み込みの
// 間だけ GC の根にする項（無ければ null）。
template <typename Load>
bool load_from_goal(Impl& machine, const Goal& call, Cell* extra, const Load& load) {
  // 読み込みの指示は入れ子の run_loop で動き、GC が枠と項を動かす。読んだ後は登録した写し goal を使う。
  Goal goal = call;
  const RootScope roots(machine, [&goal, extra](GcVisitor& visitor) {
    visitor.frame(goal.at);
    visitor.frame(goal.cont);
    if (extra != nullptr) visitor.cell(*extra);
  });
  const bool loaded = load_in_module(machine, goal.module, load);
  if (!loaded) {
    // halt/0,1 で止まった読み込みは失敗ではなく終わりで、診断は run_load が空にした。書けば空の行になる。
    // 指令の読み込み（consult_files、load_outcome）と同じく halt_now を先に見る。
    // failed を立てても、run_loop は halt_now を先に見て止まる。
    if (!machine.halt_now) report_goal_load_failure(machine);
    machine.failed = true;
    return true;
  }
  machine.diagnostic.clear();
  machine.ip = goal.cont;
  return true;
}

}  // namespace

int apply_file_list(Impl& machine, Cell term, std::string_view directory, std::vector<std::string>& opening) {
  std::vector<Cell> items;
  const int collected = collect_files(machine, term, items);
  if (collected <= 0) return collected;
  // 失敗したファイルは自分のテキストを取り消す。この指令が失敗すると、それを読んでいた読み込みも失敗し、その巻き戻しが
  // 成功した前のファイルから渡された写しで、前のファイルのテキストも取り消す（ADR 0017）。前はここでリストの前の節を
  // 名前で戻し、ゴールが abolish した枠に生きた節を戻していた（M24 の敵対的レビュー A1）。
  for (const Cell item : items) {
    const auto text = machine.atoms.name(static_cast<std::uint32_t>(item.raw() >> 3));
    const auto file = resolve_file(directory, text);
    if (consult_one(machine, file, opening) < 0) return -1;
  }
  return 1;
}

bool Machine::consult(std::string_view path) {
  Impl& impl = engine_impl(*this);
  // 問い合わせと同じく、前の分の空きを生まない GC の数を持ち越さない（ADR 0010 の改訂 B）。
  reset_gc_judgement(impl);
  // 前の問い合わせが halt で止まった印は、読み込みの間だけ脇に置き、抜けるとき戻す（HaltSetAside）。
  const HaltSetAside set_aside(impl);
  const auto text = read_source(impl, path_from_utf8(path));
  if (!text) {
    impl.diagnostic = message_text(impl, "cannot_open_file", {path});
    impl.syntax_diag = false;
    return false;
  }
  const LoadStatus status = load_consulted(impl, path, *text);
  if (status.ok) {
    impl.diagnostic.clear();
    return true;
  }
  prefix_consult_diagnostic(impl, path);
  return false;
}

bool run_consult(Impl& machine, const Goal& call) {
  const Cell path = machine.heap.deref(call.arg(machine, 0));
  if (machine.heap.unbound(path)) {
    raise_formal(machine, formal_instantiation(machine), call.name, call.arity);
    return true;
  }
  if (path.tag() != Tag::Atom) {
    raise_formal(machine, formal_type(machine, "atom", path), call.name, call.arity);
    return true;
  }
  const auto text = std::string{machine.atoms.name(static_cast<std::uint32_t>(path.raw() >> 3))};
  const auto source = read_source(machine, path_from_utf8(text));
  if (!source) {
    machine.diagnostic = message_text(machine, "cannot_open_file", {text});
    report_goal_load_failure(machine);
    machine.failed = true;
    return true;
  }
  return load_from_goal(machine, call, nullptr, [&machine, &text, &source] {
    if (load_consulted(machine, text, *source).ok) return true;
    prefix_consult_diagnostic(machine, text);
    return false;
  });
}

namespace {

// 指令と同じ Formal を、ファイルを探す前に出す。Spec を先に、Imports を後に見る。ゴールでは引数の誤りは例外で、
// ライブラリが無いことは診断を書いて失敗するだけなので、引数を全部見てから探す。指令の apply_use は Spec の検査と
// Imports の検査の間でライブラリを探すので、順が違う。use_module(library(nope), foo) はゴールなら
// type_error(list, foo)、指令なら existence_error(source_sink, library(nope)) になる。
std::optional<Cell> load_goal_problem(Impl& machine, const Goal& call) {
  if (machine.atoms.name(call.name) == "ensure_loaded") {
    const Cell file = machine.heap.deref(call.arg(machine, 0));
    if (machine.heap.unbound(file)) return formal_instantiation(machine);
    if (file.tag() != Tag::Atom) return formal_type(machine, "atom", file);
    return std::nullopt;
  }
  if (const auto formal = check_use_spec(machine, call.arg(machine, 0))) return formal;
  if (call.arity != 2) return std::nullopt;
  ImportSpec spec;
  return parse_imports(machine, call.arg(machine, 1), spec);
}

// 検査したゴールを :-(G) にする。束縛済みの引数（use_module(library(N)) の N）は、そのまま項の中で deref される。
Cell goal_directive(Impl& machine, const Goal& call) {
  std::vector<Cell> args;
  args.reserve(call.arity);
  for (std::size_t index = 0; index < call.arity; ++index) args.push_back(call.arg(machine, index));
  const std::array<Cell, 1> body{machine.heap.structure(call.name, args)};
  return machine.heap.structure(machine.atoms.intern(":-"), body);
}

}  // namespace

bool run_load_goal(Impl& machine, const Goal& call) {
  if (const auto formal = load_goal_problem(machine, call)) {
    raise_formal(machine, *formal, call.name, call.arity);
    return true;
  }
  // 項は読み込みの間だけ GC の根にする。診断の字面は load_goal_term が付ける。
  Cell directive = goal_directive(machine, call);
  return load_from_goal(machine, call, &directive,
                        [&machine, &directive] { return load_goal_term(machine, directive).ok; });
}

}
