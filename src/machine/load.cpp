#include "load_scan.hpp"

#include "dcg.hpp"

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

// 定義は load_path.cpp。
std::filesystem::path canonical_key(const std::filesystem::path& file);
std::filesystem::path prolog_source_path(const std::filesystem::path& file);

namespace {

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

bool take_term(Impl& machine, const ReadTerm& read, std::string_view directory,
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
      consume_directive(machine, term, scan.snapshots, scan.apart, scan.inits, first, origin);
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

}  // namespace

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
  bool first = true;
  while (true) {
    // module/2 と op/3 が見え方を替える。項ごとに、今のモジュールの見え方で読む。
    reader.set_operators(op_view(machine, machine.load_module));
    // 旗も項ごとに今の値で読む。set_prolog_flag の指令だけでなく、ゴールの指令と include したテキストも旗を変え、
    // 次の項から効く（レビュー M-10、7.11.2.5）。前は旗の指令の後にしか読み手へ渡さなかった。
    reader.set_quotes(current_double_quotes(machine));
    reader.set_fullwidth(machine.japanese_punct);
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
      if (!take_term(machine, *outcome.term, directory, origin, first, scan) &&
          !cond.tolerate(tolerable, clause)) {
        return false;
      }
    }
    first = false;
  }
}

// 同時に開いている読み込みの数の上限（iso-notes 7.1）。入れ子の読み込みは、指示と初期化のゴールごとに run_loop を
// 入れ子にして C++ のスタックを使う。1 MiB のスタックで、ゴールの consult/1 の鎖は 226 段まで、[F] は 318 段まで、
// initialization/1 は 393 段まで、指示の include/1 などは 425 段まで通り、その先で落ちた（Release、macOS、2026-11-20）。
// 一番浅いものに 3 倍余りの余裕を置く。数えるのは load_once の入れ子と、指示で読むファイル（open_prolog_file）。
constexpr std::size_t kLoadDepthMax = 64;

// 上限を超える読み込みは何もせずに失敗する。自分の初期化のゴールで自分を consult するファイルは、テキストを閉じた後に
// 読むので循環にならず、前は落ちた（レビュー C-01、持ち主の決定 B）。
bool too_deep(Impl& machine, std::size_t depth) {
  if (depth < kLoadDepthMax) return false;
  // ほかの読み込みの誤りと同じく、前の構文の誤りの行と桁を残さない。指示の道は load_once のように先に消さない。
  fail_load(machine, message_text(machine, "load_too_deep", {std::to_string(kLoadDepthMax)}));
  return true;
}

namespace {

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
  const std::size_t depth = machine.load_snapshots == nullptr ? 0 : machine.load_snapshots->depth() + 1;
  if (too_deep(machine, depth)) return LoadStatus{false, machine.diagnostic};
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
  // 入れ子の読み込みは、積みを渡されなくても親の積みを継ぐ。ゴールの consult/1 が空の積みから始めると、外が開いて
  // いるファイルを見ずに自分を読み続け、C++ のスタックが尽きて落ちた（レビュー C-01）。一番外だけが自分の積みを持つ。
  std::vector<std::string> local_opening;
  std::vector<std::string>* inherited = shared_opening;
  if (inherited == nullptr && shot_scope.parent() != nullptr) inherited = shot_scope.parent()->opening();
  std::vector<std::string>& opening = inherited == nullptr ? local_opening : *inherited;
  snapshots.set_opening(opening);
  snapshots.set_depth(depth);
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
  // デバッガの a で止まった読み込みも同じく空にする。知らせはセッションが 1 度だけ書く（ADR 0028）。
  if (status.ok || !(machine.halt_now || machine.abort_now)) return status;
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
  // 入れ子の深さの上限は、ファイルを探す前に読み込みを止める。パスが無いので付けない（前は「: 」が残った）。
  if (status.ok || !load.prefix_file || load.file.empty()) return status;
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
