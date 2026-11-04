#pragma once

// 機械の非公開状態。ゴール枠の語 0 が 0 のときは制御枠で、述語枠ではない。

#include "mosa/ir.hpp"
#include "mosa/machine.hpp"
#include "mosa/messages.hpp"
#include "mosa/reader.hpp"
#include "mosa/runtime.hpp"

#include "../module/module.hpp"
#include "clause_index.hpp"
#include "clause_list.hpp"
#include "gc.hpp"
#include "pred_table.hpp"
#include "state_alloc.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <initializer_list>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace mosa {

class LoadSnapshots;

inline constexpr std::uint64_t kNoFrame = UINT64_MAX;
inline constexpr std::size_t kFrameHead = 4;
// 生成コードは rt::kFrameHead でゴール枠の引数を読む。2 つは同じ枠の同じ頭である。
static_assert(rt::kFrameHead == kFrameHead);

// 節や call/1 のゴールの変数の値。継続と選択点が共有し、GC は gc_pass で 1 回の回収に 1 度だけ訪ねる。
// 書き換えるのは GC だけである。作った機械の Heap::state に自分の語を数え、放すときに引く（ADR 0010 の改訂 3）。
// Heap は Impl の 2 番目の要素なので、Impl の中の env はどれも Heap より先に壊れる。
struct EnvCells {
  EnvCells(Heap& heap, std::vector<Cell> values);
  ~EnvCells();
  EnvCells(const EnvCells&) = delete;
  EnvCells& operator=(const EnvCells&) = delete;

  std::vector<Cell> cells;
  std::uint64_t gc_pass = 0;
  Heap* owner;
};
using EnvRef = std::shared_ptr<EnvCells>;

// 節の格納の木（節の本体）。上限には数えない。
[[nodiscard]] GoalRef make_goal(IrTerm term);
// 実行時に訳したゴールの木。木の語を heap の Heap::state に数える（ADR 0010 の改訂 3）。
[[nodiscard]] GoalRef make_run_goal(Heap& heap, IrTerm term);
// goal の args[index] を、goal と同じ持ち主で指す。
[[nodiscard]] GoalRef sub_goal(const GoalRef& goal, std::size_t index);
[[nodiscard]] EnvRef make_env(Heap& heap, std::vector<Cell> cells);

// Entry::cursor の「もう試す節が無い」。番号は 2^62 の前後から振るので、この値は節の番号にならない。
inline constexpr std::size_t kNoClause = SIZE_MAX;

// multifile と discontiguous の宣言。述語が無くても残す。宣言だけで節の無い手続きになる（iso-notes 7.8.3 の決定 B）。
// shared は multifile。複数のテキストに節を足してよい印でもある。
struct SharedPred {
  std::uint32_t module = 0;
  std::uint32_t name = 0;
  std::uint16_t arity = 0;
  bool shared = true;
};

enum class Pick {
  Clauses,
  ElseGoal,
  Negation,
  Once,
  Repeat,
  Catch,
  Cleanup,
  AtomGen,
  Retract,
  Barrier,
  // 節の本体は実行しない。Clauses に載せるとやり直しが節を呼ぶ。
  ClauseScan,
  PredGen,
  // ストリームの性質は節ではない。やり直しを節の番号に載せない。
  StreamGen,
  // 生成関数の節の候補。candidates は節の番号ではなく状態なので、pred_* を入れず節の走査に数えさせない。
  Native,
  // 動いている findall・bagof・setof（ADR 0013）。ここへ戻ればゴールの解は尽きたので、答えを束ねる。
  Collect,
  // setup_call_cleanup/3 の後始末（ADR 0016）。pattern が Cleanup、module がその文脈。解ではない。
  CallCleanup
};

// 動いている収集の状態（ADR 0013）。Pick::Collect の Entry が持つ。中身は solutions.cpp だけが知る。
// Entry の写し（退避したスタック）も同じものを指すので、GC は gc_pass で 1 回の訪問に 1 度だけ辿る。
struct Collector {
  std::uint64_t gc_pass = 0;
  Collector() = default;
  Collector(const Collector&) = delete;
  Collector& operator=(const Collector&) = delete;
  virtual ~Collector() = default;
  virtual void visit(GcVisitor& visitor) = 0;
};
using CollectRef = std::shared_ptr<Collector>;

// 抜けた Catch のスタック位置と、その Entry の effect_id。入れ子の実行の後では同じ位置に別の Catch が
// ありうるので、位置だけでは有効に戻す相手を決めない。
struct CatchRecord {
  std::size_t index = 0;
  std::uint64_t id = 0;
};

struct Entry {
  Pick pick = Pick::Clauses;
  std::size_t heap_mark = 0;
  std::size_t trail_mark = 0;
  std::uint64_t redo_goal = kNoFrame;
  // PredGen は述語の番号（Predicate::serial）、StreamGen と Native は状態。節の走査は candidates を使わない（ADR 0015
  // の改訂）。上限に数える（ADR 0010 の改訂 3）。作るときは id_list で、この機械の Heap に数える列にする。
  IdList candidates;
  // candidates の次の添字。節の走査では、最後に返した節の鍵の組の添字（ClauseWalk::hint、無ければ kNoHint）で、
  // やり直しが組を二分探索せずに続ける。番号を照らしてから使うので、組が変わっていても答えは変わらない。
  std::size_t next = 0;
  // Clauses・Retract・ClauseScan の走査を始めたときの Impl::db_generation。消した節がまだこの走査に届くかを決める。
  std::uint64_t generation = 0;
  // Clauses・Retract・ClauseScan は候補の列を作らず、次に試す節の番号（尽きたら kNoClause）と選ぶ鍵だけを持つ。
  // 列を作ると、最初の答えで切っても呼ぶたびに候補の数だけかかる（レビュー M3 と M20）。番号は消しても asserta
  // しても付け直さない（ADR 0015）。Retract と ClauseScan のカーソルは、次に合う節（先に試して合った節）を指す。
  std::size_t cursor = kNoClause;
  IndexKey key;
  std::uint32_t pred_module = 0;
  std::uint32_t pred_name = 0;
  std::uint16_t pred_arity = 0;
  // else_goal と recovery を動かす文脈モジュール。
  std::uint32_t module = 0;
  std::size_t cut = 0;
  GoalRef else_goal;
  // 選択点の種類によっては、組込が状態のセルを置く（operators.cpp）。null は空と同じ。
  EnvRef env;
  std::uint64_t cont = kNoFrame;
  // 0 は *-> の else ではない印。64 ビットなので、決定的に外れる *-> を何回まわしてもあふれない（ADR 0012）。
  std::uint64_t soft_id = 0;
  bool cond_ok = false;
  bool armed = false;
  std::size_t extras_mark = 0;
  // 0 は束縛の水位だけで、選択点ではない。catch_log と scan_log はこの番号で同じ枠かを確かめる。
  std::uint64_t effect_id = 0;
  // 積んだときの Impl::catch_log の長さ。やり直すとき、これより後に無効にした Catch を有効に戻す。
  std::size_t catch_mark = 0;
  Cell pattern{};
  GoalRef recovery;
  // 節ではない組込のやり直し。1 は atom_concat、2 は sub_atom、3 は旗、4 は current_op、
  // 5 は current_char_conversion、6 は length。
  std::uint8_t gen_kind = 0;
  // Pick::Native のやり直しで呼ぶ生成関数。
  rt::NativeProc native = nullptr;
  // Pick::Collect の収集の状態。ほかの選択点では null。
  CollectRef collect;
};

// Native は extras に載せない。語 1 の第 63 ビットが立った制御枠で、ヒープに関数と節の変数を持つ（ADR 0009）。
enum class Ctrl { Interpret, Commit, SoftCommit, NegDone, OnceDone, CatchExit, Native, Collect, CleanupExit };

struct Extra {
  Ctrl ctrl = Ctrl::Interpret;
  GoalRef goal;
  GoalRef then;
  EnvRef env;
  std::uint64_t cont = kNoFrame;
  std::size_t cut = 0;
  // Commit と CatchExit では切り詰める高さ。SoftCommit では else の Entry の位置で、条件が動く間は変わらない
  // （条件のカットは else より上だけを切る）。スタックをなめて soft_id を探さずに済む（ADR 0011 の改訂 3）。
  std::size_t trim_to = 0;
  std::uint64_t soft_id = 0;
  std::uint32_t module = 0;
};

}  // namespace mosa

namespace mosa::rt {

// 呼び先の番号ごとの表。proc は呼び先がそのモジュールで付いているときだけ入る。
struct NativeCall {
  std::uint32_t module = 0;
  std::uint32_t name = 0;
  std::uint16_t arity = 0;
  NativeProc proc = nullptr;
};

// Machine ごとの単位の表。番号は単位の中だけで振るので、tag の番地で単位を引く。
struct UnitState {
  const UnitTag* tag = nullptr;
  std::vector<std::uint32_t> atoms;
  std::vector<NativeCall> calls;
  std::vector<PredKey> entries;
};

}  // namespace mosa::rt

namespace mosa {

// mosac の翻訳の読み込み（計画 Task 5）。Impl::translation が在る間、読み込み器は宣言だけを実行し
// （一般の指示のゴールと initialization/1 は呼ばない）、開いたファイルの原文と節の位置を書き留める。
struct TranslationLog {
  struct File {
    std::string key;  // 正規化したパス。実行ファイルの埋め込みの鍵になる
    std::string text;
    std::size_t line = 0;  // 最初の項の位置。module/2 の診断に使う
    std::size_t column = 0;
    bool library = false;  // library(_) で開いたファイルと、そこから開いたファイル（同梱の単位として結ぶ。Task 10）
    bool skipped_goal = false;  // 飛ばした指示のゴールがある。そのゴールの op/3 が後の項の読みを変えうる
    bool opaque = false;        // 飛ばしたゴールの後で構文エラーがあった。このファイルの述語は IR のまま動かす
    // ライブラリなら、その根（library(Name) を見つけた $MOSA_LIB か lib/）の正規化したパス。埋め込みの鍵は
    // この根からの相対を擬似ディレクトリ library/ の下に置いた物（library/sub/util.pl）。プログラムのファイルは空。
    std::string library_root{};
  };
  // library(Name) で開いたファイルと、その探索の根。FileNote が開いたファイルを library と印を付けるのに使う。
  struct LibraryOpen {
    std::string key;
    std::string root;
  };
  struct Site {
    PredKey pred;
    std::size_t clause = 0;
    std::size_t file = 0;
    std::size_t line = 0;
  };
  std::vector<File> files;
  std::vector<Site> sites;
  std::vector<LibraryOpen> library_opens;
  std::size_t current = static_cast<std::size_t>(-1);  // 読んでいるファイルの files の添字
};

// 実行ファイルに埋め込んだ原文。include/1 などはファイルシステムより先にこれを正規化したパスで引く。
struct EmbeddedSource {
  std::string key;
  std::string text;
};

struct ExportedTerm {
  enum class Kind { Var, Atom, Int, Float, List, Str, Share };
  struct Node {
    Kind kind = Kind::Atom;
    std::uint32_t atom = 0;
    std::uint16_t arity = 0;
    std::int64_t integer = 0;
    double floating = 0;
    int share = -1;
    std::vector<int> kids;
  };
  std::vector<Node> nodes;
  int root = -1;
};

// カットで外した setup_call_cleanup/3 の後始末。run_loop が次の歩みの前に動かす（ADR 0016）。
struct PendingCleanup {
  Cell goal{};
  std::uint32_t module = 0;
};

// nb_setval/2 の値（レビュー M11）。words は Heap::held に足した語数で、置き換えるときに返す。
struct GlobalValue {
  ExportedTerm value;
  std::size_t words = 0;
};

// 入出力は値のベクタ。抽象クラスにはしない（ADR 0005）。添字が '$stream'(N) の N。
struct Stream {
  enum class Kind { File, Standard, Memory };
  enum class Data { Text, Binary };
  enum class Mode { Read, Write, Append };
  enum class EofAction { Error, EofCode, Reset };
  Kind kind = Kind::Memory;
  Data data = Data::Text;
  Mode mode = Mode::Read;
  EofAction eof_action = EofAction::EofCode;
  std::string bytes;
  std::size_t cursor = 0;
  std::size_t char_no = 0;
  std::size_t line = 1;
  std::size_t line_pos = 1;
  bool closed = false;
  // 終端の印を一度返したあと。開いた直後の空は cursor が末尾で、こちらは false。
  bool past_end = false;
  bool reposition = true;
  // 書いてからまだファイルへ保存していない。halt と終わりはこれが立つ流れだけを書き出す。開いただけの流れや
  // 保存の後に書いていない流れが、後で書かれたファイルを古い写しで潰さないため（レビュー M5 の A1〜A3）。
  bool unsaved = false;
  // bytes の先頭のうち、ファイルと同じだと分かっている長さ。保存はその続きだけを書き、append の元の中身を
  // 切り詰めない（Grok のレビューの Medium）。0 ならファイル全体を書き直す。
  std::size_t on_disk = 0;
  std::string alias;
  std::string path;
};

// 書き込みの前、cursor を末尾までに収めた後に呼ぶ。cursor から後ろはファイルと違ってくる。
inline void note_edit(Stream& stream) {
  stream.unsaved = true;
  if (stream.cursor < stream.on_disk) stream.on_disk = stream.cursor;
}

struct LgtIo;

// cleanup の ^^ はクエリの中で走る。Skip のときは利用者手続きのままにする。
enum class SuiteSuper { Skip, Ok, No };

// Impl::stack・extras・scan_log が今の確保（伸びたときと容量を返したとき）の後に持った要素の数の最高と、それを
// 数えた語（ADR 0010 の改訂 3）。容量のうち触っていない分のページは OS が割り当てないので、倍に伸びた直後に
// 容量で数えると、使っていない半分で上限に着く（敵対的レビュー A2）。縮んでも触ったページは残るので下げない。
struct CountedVectors {
  std::size_t stack = 0;
  std::size_t extras = 0;
  std::size_t scan_log = 0;
  std::size_t words = 0;
};

// run_parked が脇に置いた stack と scan_log（レビュー M19）。指す先は run_parked の SavedExec で、戻すまで
// 伸び縮みしない。
struct ParkedScans {
  std::vector<Entry>* stack = nullptr;
  std::vector<ScanRecord>* scan_log = nullptr;
};

struct Machine::Impl {
  AtomTable atoms;
  Heap heap;
  OperatorTable operators;
  std::uint64_t effect_clock = 0;
  // 0 が stdin、1 が stdout、2 が stderr。読み書きそのものは get と put が行う。
  std::vector<Stream> streams;
  std::size_t current_in = 0;
  std::size_t current_out = 1;
  Trail trail;
  PredicateTable predicates;
  // 最後に振った Predicate::serial。
  std::size_t pred_serial = 0;
  // 節を足すたびと消すたびに 1 つ進める（ADR 0015）。走査はこの値を世代として持ち、それより後の変更を見ない。
  // 64 ビットなので、変更をいくつ重ねてもあふれず、kAlive にも届かない。
  std::uint64_t db_generation = 0;
  std::vector<Module> modules;
  std::uint32_t user_module = 0;
  std::uint32_t system_module = 0;
  // 読んでいるテキストの節が入るモジュール。module/2 が切り替え、そのファイルを読み終えると戻す。
  std::uint32_t load_module = 0;
  // 読み込みが失敗したときは戻す。成功した宣言は次のテキストへ残す。
  std::vector<SharedPred> declared;
  // 正規化したパス。ensure_loaded/1 は、ここにあるファイルを読み直さない。
  // include と consult は読み直す。節を戻す失敗では、この一覧も戻す。
  std::vector<std::string> prepared;
  // 初期化の例外で節を残す。入れ子の consult がこれで失敗したとき、親は節を戻さない。
  bool load_kept = false;
  // 動いている一番内側の読み込みの写し（ADR 0017）。入れ子の読み込みは、成功したら自分の写しを親へ渡す。
  LoadSnapshots* load_snapshots = nullptr;
  std::vector<Entry> stack;
  std::vector<Extra> extras;
  // 上に選択点を残したまま抜けた（無効にした）Catch。選択点ごとに有効な Catch を写すと、同時に生きている
  // 選択点の数の 2 乗になる（ADR 0011 の改訂 3）。
  std::vector<CatchRecord> catch_log;
  // 消した節にまだ届く走査を探すとき、スタック全体でなく開いた節の走査だけを見る（レビュー M3）。retract の枠は
  // 消した節を試さないので載せない。載せると、選択点を残す retract の N 回が N の 2 乗になる。
  std::vector<ScanRecord> scan_log;
  // 上の 3 つのベクタを Heap::state に数えたときの値（ADR 0010 の改訂 3）。
  CountedVectors counted_vectors;
  // consult の指示と初期化のゴールが脇に置いた外のスタック。古い順。外の走査もまだ開いているので、消した節に
  // 届く走査と、読み込みが置き換える述語の選択点を探すときに数える（レビュー M19）。
  std::vector<ParkedScans> parked;
  std::uint64_t ip = kNoFrame;
  bool failed = false;
  bool exception = false;
  // catch が評価の途中で例外を消しても、呼び出し元がヒープを読み続けないようにする。
  bool eval_abort = false;
  // launch_var の中から launch_var に入った段数（レビュー M13）。深くなれば call/1 の枠にして run_loop に回す。
  int var_depth = 0;
  Cell ball{};
  bool halt_now = false;
  bool halt_pending = false;
  int halt_code = 0;
  std::string diagnostic;
  bool syntax_diag = false;
  std::size_t diag_line = 1;
  std::size_t diag_column = 1;
  std::vector<Binding> bindings;
  // nb_setval/2 の値を鍵のアトムで引く。ヒープは後戻りで巻き戻るので、findall の答えと同じく外に写して持つ。
  std::unordered_map<std::uint32_t, GlobalValue> globals;
  // GC の根。歩みの途中のカットが積み、run_loop の次の回の初めに空にするので、歩みの境ではいつも空である。
  std::vector<PendingCleanup> pending_cleanups;
  std::uint64_t next_soft = 1;
  DoubleQuotes quotes = DoubleQuotes::Codes;
  // unknown=fail のときだけ真。warning は未決 10 で受け付けない。
  bool unknown_fail = false;
  // invalid_utf8=replace のときだけ真。既定の error は壊れたバイトを進めない。
  bool utf8_replace = false;
  bool float_overflow_error = true;
  bool float_zero_div_error = true;
  bool float_undefined_error = true;
  // 旗 debug は値だけ持つ。on でも追跡しない。既定は off。
  bool debug_on = false;
  // 旗 japanese_punctuation。Reader::set_fullwidth で字句解析に渡す。既定は false。
  bool japanese_punct = false;
  // 旗 message_language。文面のカタログを選ぶ（ADR 0007）。
  MessageLanguage message_lang = default_message_language();
  // 旗 argv。プログラムに渡す引数（mosa では -- より後ろ）。読み取り専用で、起動前に一度だけ決まる。
  std::vector<std::string> program_argv;
  // initialization(G, main) で決まったゴールの文面。読み込み後にヒープを切るので、項ではなく文で持つ。
  std::optional<std::string> main_goal;
  // 文面を作るためだけのマシン。自分の診断を文面にしに行くと、また新しいマシンを作り続ける。
  bool render_only = false;
  CharConv char_conv;
  // 前のテストのバイナリ出力を閉じる。空のままだと現在の出力がテキストに戻らない。
  SuiteSuper (*suite_super)(Machine& machine, const LgtIo& io, Cell goal) = nullptr;
  const LgtIo* suite_io = nullptr;
  Machine* suite_self = nullptr;
  // ADR 0009。単位の表、次の step が run_builtin より先に呼ぶ生成関数、呼び出しとやり直しのカット境界。
  std::vector<rt::UnitState> native_units;
  rt::NativeProc native_next = nullptr;
  std::size_t native_cut = 0;
  TranslationLog* translation = nullptr;
  // mosatest の test/3 の横取り（suite.cpp）。読み込み器が項を節にする前に呼び、真を返した項は節にしない。
  // 指示はこの読み込みの中で順に 1 回ずつ動くので、:- true, op(...) の op も後の項の読みに効き、include した
  // ファイルのゴールも 1 回だけ、前の節がそろった所で動く。origin は読んでいるファイルで、include の先は別の値。
  std::function<bool(const ReadTerm& read, std::string_view origin)> term_hook;
  std::vector<EmbeddedSource> embedded;
  // 実行ファイルの起動の読み込みがヒープの上限で止まった。rt::finish は mosa の consult と同じ 2 で終わる。
  bool load_heap_limit = false;
  // ADR 0010 の GC。gc_capacity は柔らかい容量 C、gc_threshold は安全点で GC を始める heap.size()。
  // 普段は閾値 = C で、上限の近くだけ C より上に置く（init_gc と collect_garbage の方針）。
  GcStats gc_stats;
  std::vector<RootVisit> gc_roots;
  std::size_t gc_capacity = 0;
  // GcVisitor の訪問ごとに 1 つ進める。EnvCells::gc_pass と比べ、共有の env を 2 度訪ねない。
  std::uint64_t gc_pass = 0;
  std::size_t gc_threshold = 0;
  // 安全点の速い道の比較に使う値。通常は gc_threshold と同じで、stress 中は 0（毎回 gc_safe_point に入る）。
  std::size_t gc_trigger = 0;
  // 前の GC の後に生きていた語（バックトラックで縮めば、決め直しのときに今のヒープの大きさまで下げる）と、
  // 閾値を決めたときの Heap::held()（ADR 0010 の改訂 2）。持つ語が変われば安全点が閾値を決め直す。
  std::size_t gc_last_live = 0;
  std::size_t gc_tuned_held = 0;
  // C が天井にあるとき、閾値で始めた GC のうち空きを生まなかったものの連続回数（ADR 0010 の改訂 B）。
  // 書くのは gc.cpp の安全点（judge_collection）と reset_gc_judgement（問い合わせ・consult・load_program の始まり）だけ。
  std::size_t gc_unproductive = 0;
  // 前に判定した GC の時点の gc_stats.freed_words と gc_state_freed の和。判定はその後に空いた語（ストレスや
  // garbage_collect/0 の GC の分も含む）で行うので、ストレスの GC が先に回収しても止まるかは変わらない。
  // 書く場所は gc_unproductive と同じ。
  std::uint64_t gc_judged_freed = 0;
  // GC が空けた状態の語の累計（ADR 0010 の改訂 3）。書くのは collect_garbage だけ。
  std::uint64_t gc_state_freed = 0;
  // MOSA_GC_STRESS=N。N 回の安全点ごとに GC する。0 は無効。
  std::size_t gc_stress = 0;
  std::size_t gc_ticks = 0;
  bool trace_gc = false;
  // trace_gc の行の型と、それを作ったときの言語（gc.cpp の trace_form）。旗で言語が変われば作り直す。
  std::optional<std::string> gc_trace_form;
  MessageLanguage gc_trace_form_lang = MessageLanguage::En;
  // statistics(runtime/walltime, [合計, 前回からの差]) の「前回」。マシンごとに持つ。
  std::chrono::steady_clock::time_point started_at = std::chrono::steady_clock::now();
  std::int64_t last_runtime_ms = 0;
  std::int64_t last_walltime_ms = 0;

  explicit Impl(std::size_t limit) : heap(atoms, limit), operators(OperatorTable::iso_core()) {
    install_modules(*this);
    init_gc(*this);
  }
};

using Impl = Machine::Impl;

// ADR 0010 の改訂 3。stack・extras・scan_log の要素の数の最高で Heap::state を数え直す。count_vectors は最高を
// 超えたときだけ数え直すので、積んだ後に呼ぶ。
void recount_vectors(Impl& machine);
inline void count_vectors(Impl& machine) {
  const CountedVectors& counted = machine.counted_vectors;
  if (machine.stack.size() > counted.stack || machine.extras.size() > counted.extras ||
      machine.scan_log.size() > counted.scan_log) {
    recount_vectors(machine);
  }
}
// stack と scan_log を別のベクタと入れ替えた後（consult の指示の脇置きと戻し）。最高をその値にして数え直す。
void mark_parked_vectors(Impl& machine, std::size_t stack, std::size_t scan_log);
// GC の後。大きさの 4 倍を超えた容量を返し、返したベクタの最高を今の大きさにする。
void give_back_vectors(Impl& machine);
// extras に積み、それを指す 4 語の制御枠をヒープに作って返す。
[[nodiscard]] std::uint64_t push_extra(Impl& machine, Extra extra);

struct Goal {
  std::uint64_t at = 0;
  std::uint32_t name = 0;
  std::uint16_t arity = 0;
  std::uint32_t module = 0;
  std::size_t cut = 0;
  std::uint64_t cont = kNoFrame;

  [[nodiscard]] Cell arg(const Impl& machine, std::size_t index) const {
    return Cell{machine.heap.word_at(at + kFrameHead + index)};
  }
};

struct Compiled {
  GoalRef term;
  EnvRef env;
};

// 読み込み中の診断の文面（ADR 0007）。読み込んでいるマシンでは問い合わせを動かせないので、新しいマシンで作る。
[[nodiscard]] std::string message_text(const Impl& machine, std::string_view key,
                                       std::initializer_list<std::string_view> args = {});

// ヒープが上限に達したときの文面。上限は MiB で添える（ADR 0010）。
[[nodiscard]] std::string heap_limit_text(const Impl& machine);

[[nodiscard]] Impl& engine_impl(Machine& machine);
[[nodiscard]] const Impl& engine_impl(const Machine& machine);

[[nodiscard]] Compiled compile_goal(Heap& heap, Cell cell);
[[nodiscard]] bool compile_clause(const Heap& heap, Cell cell, Clause& clause, std::span<const VarName> names = {});
[[nodiscard]] bool is_directive(const Heap& heap, Cell cell);

void launch(Impl& machine, GoalRef goal, std::uint64_t cont, std::size_t cut, const EnvRef& env,
            std::uint32_t module);
// launch の繰り返しの 1 歩。制御構造の枠を積み、次に動かす最初の下位ゴールとその継続とカットの高さを返す。
// launch を呼ばないので、入れ子が深くても C++ の呼び出しは積まない（ADR 0011 の改訂 3）。
struct LaunchNext {
  GoalRef goal;
  std::uint64_t cont = kNoFrame;
  std::size_t cut = 0;
};
[[nodiscard]] LaunchNext open_or(Impl& machine, const GoalRef& goal, std::uint64_t cont, std::size_t cut,
                                 const EnvRef& env, std::uint32_t module);
[[nodiscard]] LaunchNext open_if_then(Impl& machine, const GoalRef& goal, std::uint64_t cont, std::size_t cut,
                                      const EnvRef& env, std::uint32_t module);
[[nodiscard]] LaunchNext open_soft(Impl& machine, const GoalRef& goal, std::uint64_t cont, std::size_t cut,
                                   const EnvRef& env, std::uint32_t module);
void trim_stack(Impl& machine, std::size_t boundary);
// 定義は clause_store.cpp。Predicate::clauses を書くのはここだけで、番号・first_id・next_id・live をここで保つ
// （ADR 0015）。append と prepend は新しい番号を振り、born を節の世代にする。replace は渡された番号と世代を残し、
// 範囲をそれが入るまで広げる。
void append_clause(Predicate& predicate, Clause clause, std::uint64_t born);
void prepend_clause(Predicate& predicate, Clause clause, std::uint64_t born);
void replace_clauses(Predicate& predicate, ClauseList clauses);
void erase_clause_id(Predicate& predicate, std::size_t id);
// held[位置] が偽の死んだ節を一度に消し、残した死んだ節の数を dead_since と held_dead に、それに届いた一番下の枠
// holder を held_by に置く。消した節があれば索引を捨てる。held は節の数だけ要る。
void sweep_clauses(Predicate& predicate, const std::vector<bool>& held, ScanRecord holder);
// 番号の節の位置。無い番号は nullopt。
[[nodiscard]] std::optional<std::size_t> clause_position(const Predicate& predicate, std::size_t id);
// 番号が id 以上の最初の節の位置。無ければ clauses.size()。
[[nodiscard]] std::size_t clause_from(const Predicate& predicate, std::size_t id);
[[nodiscard]] std::size_t live_count(const Predicate& predicate);
void reset_at_end(Stream& stream);
// 開いた走査が届かない死んだ節を一度に消す（定義は database.cpp、ADR 0015）。述語は消さない。
void sweep_dead(Impl& machine, Predicate& predicate);
// 生きた節を全部消す（abolish と、読み込みが述語を置き換えるとき）。開いた走査がまだ試す節は死んだ節で残す。
void retire_clauses(Impl& machine, Predicate& predicate);
// 静的な述語に残った死んだ節のうち、開いた走査がもう届かないものを消す（enter_user が呼ぶ）。
void purge_static(Impl& machine, Predicate& predicate);
[[nodiscard]] bool declared_only(const Impl& machine, std::uint32_t module, std::uint32_t name, std::uint16_t arity);
[[nodiscard]] Cell materialize(Impl& machine, const IrTerm& term, const std::vector<Cell>& env);
// 述語を呼ぶゴール（制御構造でも変数でもない）。env を残さないので、共有の env を作らずに枠にできる。
[[nodiscard]] bool is_call_leaf(IrTag tag);
[[nodiscard]] std::uint64_t embody_leaf(Impl& machine, const IrTerm& goal, std::uint64_t cont, std::size_t cut,
                                        const std::vector<Cell>& cells, std::uint32_t module);
[[nodiscard]] std::uint64_t embody(Impl& machine, const GoalRef& goal, std::uint64_t cont, std::size_t cut,
                                   const EnvRef& env, std::uint32_t module);
[[nodiscard]] Goal read_goal(const Impl& machine, std::uint64_t at);
[[nodiscard]] bool do_unify(Impl& machine, Cell left, Cell right);
// floor より前の REF を候補ごとに戻す。節の匿名は外側の選択点より新しい。
[[nodiscard]] bool unify_visible(Impl& machine, std::size_t floor, Cell left, Cell right);
void enter_clause(Impl& machine, const Predicate& predicate, std::size_t index, std::uint64_t goal, std::size_t cut);
void restore_state(Impl& machine, std::size_t heap_mark, std::size_t trail_mark, std::size_t extras_mark);
void shrink_extras(Impl& machine, std::size_t mark);

struct OpName {
  Cell cell;
  std::string text;
};

// op/3 と公開リストの op(P, T, N) の引数。read_op_request が 8.14.3 の順で確かめる。
struct OpRequest {
  int priority = 0;
  Specifier specifier = Specifier::Xfx;
  std::vector<OpName> names;
};

[[nodiscard]] std::optional<Cell> read_op_request(Impl& machine, Cell priority, Cell specifier, Cell names,
                                                  OpRequest& request);
// module の層へ全部の名前を入れる。拒否は permission_error の Formal で、表は変えない。
[[nodiscard]] std::optional<Cell> commit_op_request(Impl& machine, std::uint32_t module, const OpRequest& request);
void note_catches(const Impl& machine, Entry& entry);
void apply_catches(Impl& machine, const Entry& entry);
// スタックが縮んだ後、外した Catch を指す末尾の記録を捨てる。カットで縮み続ける決定的な再帰で記録が溜まらない。
void prune_catch_log(Impl& machine);
// 外した枠を指す scan_log の記録を捨てる。残った記録はどれも今のスタックの開いた節の走査を指す。
void prune_scan_log(Impl& machine);
void push_frame(Impl& machine, Entry entry);
// ゴールの位置にある、制御構造でない項（葉）の型を見る。
using GoalLeafOk = bool (*)(IrTag tag);
// 葉がどれも ok を満たすか。深い制御構造でも C++ の呼び出しを積まない（ADR 0011 の改訂 2）。
[[nodiscard]] bool goal_leaves_all(const IrTerm& root, GoalLeafOk ok);
[[nodiscard]] bool body_callable(const IrTerm& term);
[[nodiscard]] bool backtrack(Impl& machine);
[[nodiscard]] bool run_builtin(Impl& machine, const Goal& goal);
void enter_user(Impl& machine, const Goal& goal);
[[nodiscard]] ExportedTerm export_term(Impl& machine, Cell cell);
[[nodiscard]] Cell import_term(Impl& machine, const ExportedTerm& term);
void propagate_throw(Impl& machine, Cell ball);
void raise_formal(Impl& machine, Cell formal, std::uint32_t name, std::uint16_t arity);
void raise_formal_extra(Impl& machine, Cell formal, std::uint32_t name, std::uint16_t arity, Cell extra);
[[nodiscard]] Cell cell_functor(Impl& machine, std::uint32_t name, std::uint16_t arity);
// 束縛した第1引数 first（deref 済み）の走査の鍵。鍵の型でない値なら kind が None で、鍵の無い節だけを選ぶ。未束縛の
// 第1引数は呼び手が Any の鍵（IndexKey{}）にする（定義は index.cpp）。
[[nodiscard]] IndexKey first_key(const Impl& machine, Cell first);
// この機械の Heap::state に数える空の候補の列（ADR 0010 の改訂 3）。
[[nodiscard]] inline IdList id_list(Impl& machine) { return IdList(StateAllocator<std::size_t>(machine.heap)); }

[[nodiscard]] Stop run_loop(Impl& machine);
[[nodiscard]] LaunchNext open_catch(Impl& machine, const GoalRef& goal, std::uint64_t cont, std::size_t cut,
                                    const EnvRef& env, std::uint32_t module);
void arm_throw(Impl& machine, const IrTerm& goal, const std::vector<Cell>& env);
// throw/1 の本体。ball が変数なら instantiation_error、他は捕まえる catch まで巻き戻す。生成コードも通る。
void throw_ball(Impl& machine, Cell ball);
// ゴールを抜けた catch を無効にする。選択点が残る間は枠を残し、戻ったときだけ apply_catches が有効に戻す。
// 上に選択点が無ければ、戻ってくる選択点も無いので記録しない。
void disarm_catch(Impl& machine, std::size_t index);
void arm_halt(Impl& machine, const IrTerm& goal, const std::vector<Cell>& env);
// slots[n] はスロット n の最初の出現で作る。匿名は slots を使わない。
[[nodiscard]] Cell ir_to_term(Impl& machine, const IrTerm& term, std::vector<Cell>& slots, bool as_goal);
// clause/2 が見せる形が格納した IR と違うか（変数ゴール、Call、halt/0、catch の隠し引数）。
[[nodiscard]] bool shown_differs(const IrTerm& term, bool as_goal);
// run_builtin が引き受ける手続きだけ。述語は実行しない。
[[nodiscard]] bool is_builtin_procedure(const Impl& machine, std::uint32_t name, std::uint16_t arity);
[[nodiscard]] DoubleQuotes current_double_quotes(const Impl& machine);
[[nodiscard]] int apply_flag_directive(Impl& machine, Cell directive);
[[nodiscard]] Cell formal_permission(Impl& machine, std::string_view action, std::string_view type,
                                     Cell culprit);
// directory が空でなければ、相対の include はそのディレクトリから開く。
// origin は循環の検出に使う。空なら、このテキスト自体はファイルではない。
// shared_opening は、入れ子の consult が親と同じ循環を見るため。
[[nodiscard]] LoadStatus load_source(Impl& machine, std::string_view source, std::string_view directory = {},
                                    std::string_view origin = {},
                                    std::vector<std::string>* shared_opening = nullptr);
// 0 はファイルリストではない。1 は読んだ。-1 は失敗。
[[nodiscard]] int apply_file_list(Impl& machine, Cell term, std::string_view directory,
                                  std::vector<std::string>& opening);
[[nodiscard]] bool run_consult(Impl& machine, const Goal& goal);
// 定義は load_path.cpp。読み込みのファイルの鍵（Module::file、prepared、埋め込み）にする正規化したパス。
[[nodiscard]] std::filesystem::path canonical_key(const std::filesystem::path& file);
// 埋め込みの原文を先に引き、無ければファイルを読む。開けなければ nullopt。
[[nodiscard]] std::optional<std::string> read_source(const Impl& machine, const std::filesystem::path& file);
// 拡張子の無い名前に .pl を足すかを、埋め込みの原文も見て決める。
[[nodiscard]] std::filesystem::path source_path(const Impl& machine, const std::filesystem::path& file);
// 定義は consult.cpp。読み込みの診断にパスと、構文エラーなら行と列を付ける。
void prefix_consult_diagnostic(Impl& machine, std::string_view path);
// 定義は native.cpp。単位の name が埋め込んだパスならそのファイルとして、ほかは文字だけで source を読む。
// 差分テスト（計画 Task 11）のインタプリタ側は、表の無い単位でこれを呼んで翻訳側と同じ読み方をする。
[[nodiscard]] bool load_unit(Impl& machine, const rt::NativeUnit& unit);
// 定義は session.cpp。initialization(G, main) を Task 2 の規則で動かし、終了コードを返す。文面は warning に足す。
[[nodiscard]] int run_main_goal(Machine& machine, const std::string& goal, std::string& warning);
// 定義は native.cpp。翻訳器（Task 4）も同じ関数で指紋を出す。
[[nodiscard]] std::uint64_t clause_fingerprint(const AtomTable& atoms, const Predicate& predicate);
// install_unit が生成関数を付けてよい述語か（指紋は見ない）。翻訳器も同じ関数で出す述語を選ぶ。
[[nodiscard]] bool native_eligible(const Impl& machine, const Predicate& predicate);
// 付けたあとの節か meta への書き込みのたびに呼ぶ。述語と全単位の同じ鍵の calls を一度に空にする。
void detach_native(Impl& machine, Predicate& predicate);
// 単位の表を作って（番号は単位の中だけ）その slot を返す。付けるのは原文を読み終えた後（ADR 0009）。
[[nodiscard]] std::size_t prepare_native_unit(Impl& machine, const rt::NativeUnit& unit);
void attach_native_unit(Impl& machine, std::size_t slot, const rt::NativeUnit& unit);
// 定義は native_library.cpp。同梱ライブラリの擬似パス library/<Name>.pl。mosac（project.cpp）も同じ関数で鍵を作る。
[[nodiscard]] std::filesystem::path bundled_library_path(std::string_view name);
// use_module(library(Name)) が SPEC 第5節の探索より先に引く。同梱にあれば原文を擬似パスで埋め込み、そのパスを返す。
[[nodiscard]] std::optional<std::filesystem::path> locate_bundled_library(Impl& machine, std::string_view name);
// 同梱ライブラリの原文を読み終えた後に、その翻訳済みの単位を付ける（原文だけの同梱なら何もしない）。
void attach_bundled_library(Impl& machine, std::string_view name);
// 同梱ライブラリの中の素のパスの use_module(util) が擬似パス library/util.pl に解決したら、原文を埋め込み（まだなら）、
// file を .pl 付きに直して名前を返す。ほかのパスなら nullopt。
[[nodiscard]] std::optional<std::string> locate_bundled_plain(Impl& machine, std::filesystem::path& file);
// 定義は native_redo.cpp。
void call_native(Impl& machine, rt::NativeProc proc, std::uint64_t frame);
void resume_native(Impl& machine);
[[nodiscard]] bool redo_native(Impl& machine);

}

// 組込述語の入口と、組込が共に使う誤りの項・リスト・字面の手助け。internal.hpp の 800 行の上限のために分けた。
#include "builtin_api.hpp"
