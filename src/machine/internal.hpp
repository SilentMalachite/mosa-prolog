#pragma once

// 機械の非公開状態。ゴール枠の語 0 が 0 のときは制御枠で、述語枠ではない。

#include "mosa/ir.hpp"
#include "mosa/machine.hpp"
#include "mosa/messages.hpp"
#include "mosa/reader.hpp"
#include "mosa/runtime.hpp"

#include "../module/module.hpp"

#include <cstdint>
#include <filesystem>
#include <initializer_list>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace mosa {

inline constexpr std::uint64_t kNoFrame = UINT64_MAX;
inline constexpr std::size_t kFrameHead = 4;
// 生成コードは rt::kFrameHead でゴール枠の引数を読む。2 つは同じ枠の同じ頭である。
static_assert(rt::kFrameHead == kFrameHead);

enum class KeyKind { Any, None, Var, Atom, Int, Float, List, Fun };

struct Clause {
  IrTerm head;
  IrTerm body;
  std::uint16_t vars = 0;
  KeyKind key = KeyKind::Any;
  std::uint32_t key_atom = 0;
  std::uint16_t key_arity = 0;
  std::int64_t key_int = 0;
  double key_float = 0;
  // ADR 0008。開いた走査がまだ試していない節を消すと、消さずにこの印を付ける。新しい呼び出しは飛ばす。
  bool dead = false;
  // スロットごとの原文の変数名（名前の無い変数は空）。読み込んだ節だけが持ち、mosac のコメントだけが読む。
  // 意味は持たないので指紋には数えない。
  std::vector<std::string> names;
};

// multifile と discontiguous の宣言。述語が無くても残す。宣言だけで節の無い手続きになる（iso-notes 7.8.3 の決定 B）。
// shared は multifile。複数のテキストに節を足してよい印でもある。
struct SharedPred {
  std::uint32_t module = 0;
  std::uint32_t name = 0;
  std::uint16_t arity = 0;
  bool shared = true;
};

struct Predicate {
  // 既定値の 0 は user ではない。述語を作る所で必ず入れる。
  std::uint32_t module = 0;
  std::uint32_t name = 0;
  std::uint16_t arity = 0;
  std::vector<Clause> clauses;
  bool seen = false;
  bool dynamic = false;
  // 読み込みの開始で落とす。この読み込みが dynamic を見たら真。見ていなければ置き換え後は静的。
  bool dynamic_declared = false;
  // meta_predicate/1 の宣言。空なら限定する引数は無い。meta_declared は dynamic_declared と同じく読み込みごと。
  std::vector<MetaArg> meta;
  bool meta_declared = false;
  // ADR 0008。開いた走査があるときの abolish。走査のやり直しだけが引き、新しい引き当ては無いものとして扱う。
  bool abolished = false;
  // ADR 0009。指紋が合った静的述語の生成関数。節か meta を書けば detach_native が外し、付け直さない。
  rt::NativeProc native = nullptr;
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
  // ストリームの性質は節ではない。やり直しを節の添字に載せない。
  StreamGen,
  // 生成関数の節の候補。candidates は節の添字ではなく状態なので、pred_* を入れず節の走査に数えさせない。
  Native
};

struct Entry {
  Pick pick = Pick::Clauses;
  std::size_t heap_mark = 0;
  std::size_t trail_mark = 0;
  std::uint64_t redo_goal = kNoFrame;
  std::vector<std::size_t> candidates;
  std::size_t next = 0;
  std::uint32_t pred_module = 0;
  std::uint32_t pred_name = 0;
  std::uint16_t pred_arity = 0;
  // else_goal と recovery を動かす文脈モジュール。
  std::uint32_t module = 0;
  std::size_t cut = 0;
  IrTerm else_goal;
  std::vector<Cell> env;
  std::uint64_t cont = kNoFrame;
  int soft_id = 0;
  bool cond_ok = false;
  bool armed = false;
  std::size_t extras_mark = 0;
  // 0 は束縛の水位だけで、選択点ではない。op/3 を戻す対象にしない。
  std::uint64_t effect_id = 0;
  // この選択点をやり直すとき、有効に戻す Catch のスタック位置。抜けたあとの選択点は空である。
  std::vector<std::uint32_t> live_catches;
  Cell pattern{};
  IrTerm recovery;
  // 節ではない組込のやり直し。1 は atom_concat、2 は sub_atom、3 は旗、4 は current_op、
  // 5 は current_char_conversion、6 は length。
  std::uint8_t gen_kind = 0;
  // Pick::Native のやり直しで呼ぶ生成関数。
  rt::NativeProc native = nullptr;
};

// Native は extras に載せない。語 1 の第 63 ビットが立った制御枠で、ヒープに関数と節の変数を持つ（ADR 0009）。
enum class Ctrl { Interpret, Commit, SoftCommit, NegDone, OnceDone, CatchExit, Native };

struct Extra {
  Ctrl ctrl = Ctrl::Interpret;
  IrTerm goal;
  IrTerm then;
  std::vector<Cell> env;
  std::uint64_t cont = kNoFrame;
  std::size_t cut = 0;
  std::size_t trim_to = 0;
  int soft_id = 0;
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
    bool library = false;  // library(_) で開いたファイルと、そこから開いたファイル（Task 10 まで翻訳しない）
    bool skipped_goal = false;  // 飛ばした指示のゴールがある。そのゴールの op/3 が後の項の読みを変えうる
    bool opaque = false;        // 飛ばしたゴールの後で構文エラーがあった。このファイルの述語は IR のまま動かす
  };
  struct Site {
    PredKey pred;
    std::size_t clause = 0;
    std::size_t file = 0;
    std::size_t line = 0;
  };
  std::vector<File> files;
  std::vector<Site> sites;
  std::vector<std::string> library_keys;
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
  std::string alias;
  std::string path;
};

// この op/3 が変えた名前だけ。表全体を写すと、後の :- op まで消える。
struct OpTouch {
  std::string name;
  int priority = 0;
  Specifier specifier = Specifier::Xfx;
  bool had = false;
  int old_priority = 0;
  Specifier old_specifier = Specifier::Xfx;
};

// 選択点より後ろの op/3 だけを失敗で戻す。assert と retract はここへ載せない。
struct OpUndo {
  std::uint64_t effect_id = 0;
  // どの層の変更か。user なら全体の表、ほかはそのモジュールの固有層。
  std::uint32_t module = 0;
  std::vector<OpTouch> touches;
};

struct LgtIo;

// cleanup の ^^ はクエリの中で走る。Skip のときは利用者手続きのままにする。
enum class SuiteSuper { Skip, Ok, No };

struct Machine::Impl {
  AtomTable atoms;
  Heap heap;
  OperatorTable operators;
  std::vector<OpUndo> op_undos;
  std::uint64_t effect_clock = 0;
  // 0 が stdin、1 が stdout、2 が stderr。読み書きそのものは get と put が行う。
  std::vector<Stream> streams;
  std::size_t current_in = 0;
  std::size_t current_out = 1;
  Trail trail;
  std::vector<Predicate> predicates;
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
  std::vector<Entry> stack;
  std::vector<Extra> extras;
  std::uint64_t ip = kNoFrame;
  bool failed = false;
  bool exception = false;
  // catch が評価の途中で例外を消しても、呼び出し元がヒープを読み続けないようにする。
  bool eval_abort = false;
  Cell ball{};
  bool halt_now = false;
  bool halt_pending = false;
  int halt_code = 0;
  std::string diagnostic;
  bool syntax_diag = false;
  std::size_t diag_line = 1;
  std::size_t diag_column = 1;
  std::vector<Binding> bindings;
  int next_soft = 1;
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
  // mosatest の断片の読み込み（suite.cpp）。断片のテキスト自身の指示のゴールは、最後の load だけが呼ぶ。
  bool fragment_pass = false;
  std::vector<EmbeddedSource> embedded;

  explicit Impl(std::size_t limit) : heap(atoms, limit), operators(OperatorTable::iso_core()) { install_modules(*this); }
};

using Impl = Machine::Impl;

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
  IrTerm term;
  std::vector<Cell> env;
};

// 読み込み中の診断の文面（ADR 0007）。読み込んでいるマシンでは問い合わせを動かせないので、新しいマシンで作る。
[[nodiscard]] std::string message_text(const Impl& machine, std::string_view key,
                                       std::initializer_list<std::string_view> args = {});

[[nodiscard]] Impl& engine_impl(Machine& machine);
[[nodiscard]] const Impl& engine_impl(const Machine& machine);

[[nodiscard]] Compiled compile_goal(const Heap& heap, Cell cell);
[[nodiscard]] bool compile_clause(const Heap& heap, Cell cell, Clause& clause, std::span<const VarName> names = {});
[[nodiscard]] bool is_directive(const Heap& heap, Cell cell);

void launch(Impl& machine, const IrTerm& goal, std::uint64_t cont, std::size_t cut,
            const std::vector<Cell>& env, std::uint32_t module);
void launch_or(Impl& machine, const IrTerm& goal, std::uint64_t cont, std::size_t cut,
               const std::vector<Cell>& env, std::uint32_t module);
void launch_if_then(Impl& machine, const IrTerm& goal, std::uint64_t cont, std::size_t cut,
                    const std::vector<Cell>& env, std::uint32_t module);
void launch_soft(Impl& machine, const IrTerm& goal, std::uint64_t cont, std::size_t cut,
                 const std::vector<Cell>& env, std::uint32_t module);
void trim_stack(Impl& machine, std::size_t boundary);
[[nodiscard]] std::size_t live_count(const Predicate& predicate);
void reset_at_end(Stream& stream);
Predicate* purge_dead_clauses(Impl& machine, Predicate* predicate);
[[nodiscard]] bool declared_only(const Impl& machine, std::uint32_t module, std::uint32_t name, std::uint16_t arity);
[[nodiscard]] Cell materialize(Impl& machine, const IrTerm& term, const std::vector<Cell>& env);
[[nodiscard]] std::uint64_t embody(Impl& machine, const IrTerm& goal, std::uint64_t cont, std::size_t cut,
                                   const std::vector<Cell>& env, std::uint32_t module);
[[nodiscard]] Goal read_goal(const Impl& machine, std::uint64_t at);
[[nodiscard]] bool do_unify(Impl& machine, Cell left, Cell right);
// floor より前の REF を候補ごとに戻す。節の匿名は外側の選択点より新しい。
[[nodiscard]] bool unify_visible(Impl& machine, std::size_t floor, Cell left, Cell right);
void enter_clause(Impl& machine, const Predicate& predicate, std::size_t index, std::uint64_t goal, std::size_t cut);
void restore_state(Impl& machine, std::size_t heap_mark, std::size_t trail_mark, std::size_t extras_mark);
void trail_operators(Impl& machine, std::uint32_t module, int priority, Specifier specifier,
                     const std::vector<std::string>& names);

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
void restore_operators(Impl& machine, std::uint64_t effect_id);
void note_catches(const Impl& machine, Entry& entry);
void apply_catches(Impl& machine, const Entry& entry);
void push_frame(Impl& machine, Entry entry);
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
[[nodiscard]] std::vector<std::size_t> select_clauses(const Impl& machine, const Predicate& predicate,
                                                      bool has_arg, Cell first);

[[nodiscard]] Stop run_loop(Impl& machine);
void arm_catch(Impl& machine, const IrTerm& goal, std::uint64_t cont, std::size_t cut,
               const std::vector<Cell>& env, std::uint32_t module);
void arm_throw(Impl& machine, const IrTerm& goal, const std::vector<Cell>& env);
// throw/1 の本体。ball が変数なら instantiation_error、他は捕まえる catch まで巻き戻す。生成コードも通る。
void throw_ball(Impl& machine, Cell ball);
// ゴールを抜けた catch を無効にする。選択点が残る間は枠を残し、戻ったときだけ apply_catches が有効に戻す。
void disarm_catch(Impl& machine, std::size_t index);
void arm_halt(Impl& machine, const IrTerm& goal, const std::vector<Cell>& env);
[[nodiscard]] Cell formal_instantiation(Impl& machine);
[[nodiscard]] Cell formal_uninstantiation(Impl& machine, Cell culprit);
[[nodiscard]] Cell formal_type(Impl& machine, std::string_view type, Cell culprit);
[[nodiscard]] Cell formal_domain(Impl& machine, std::string_view domain, Cell culprit);
[[nodiscard]] Cell formal_existence(Impl& machine, std::string_view kind, Cell culprit);
[[nodiscard]] Cell formal_evaluation(Impl& machine, std::string_view which);
[[nodiscard]] Cell formal_representation(Impl& machine, std::string_view which);
[[nodiscard]] Cell formal_syntax(Impl& machine, Cell detail);
[[nodiscard]] Cell formal_system(Impl& machine);
[[nodiscard]] bool run_types(Impl& machine, const Goal& goal);
[[nodiscard]] bool run_order(Impl& machine, const Goal& goal);
// setof/3 は @</2 と同じ標準順序を使う。アルゴリズムは order.cpp に一つ。
[[nodiscard]] int compare_cells(const Impl& machine, Cell left, Cell right);
[[nodiscard]] bool run_terms(Impl& machine, const Goal& goal);
[[nodiscard]] bool run_control(Impl& machine, const Goal& goal);
[[nodiscard]] bool run_qualify(Impl& machine, const Goal& goal);
[[nodiscard]] bool run_arith(Impl& machine, const Goal& goal);
struct CodeText {
  std::string bytes;
  std::vector<char32_t> codes;
  std::vector<std::size_t> cuts;
  [[nodiscard]] std::size_t size() const { return codes.size(); }
  [[nodiscard]] std::string_view slice(std::size_t from, std::size_t count) const {
    return std::string_view{bytes}.substr(cuts[from], cuts[from + count] - cuts[from]);
  }
};

enum class ListShape { Variable, Proper, Partial, Bad };

struct WalkedList {
  ListShape shape = ListShape::Bad;
  std::vector<Cell> items;
  // Partial のときだけ、末尾の未束縛の変数。
  Cell tail{};
};

[[nodiscard]] CodeText decode_codes(std::string_view text);
void append_utf8_code(std::string& text, char32_t code);
[[nodiscard]] WalkedList walk_list(const Impl& machine, Cell list);
[[nodiscard]] Cell list_from_cells(Impl& machine, const std::vector<Cell>& items);
[[nodiscard]] bool one_char_atom(const Impl& machine, Cell cell, char32_t& code);
[[nodiscard]] bool scalar_code(std::int64_t code);
[[nodiscard]] bool run_atoms(Impl& machine, const Goal& goal);
[[nodiscard]] bool redo_atoms(Impl& machine);
[[nodiscard]] bool run_flags(Impl& machine, const Goal& goal);
[[nodiscard]] bool redo_flag_gen(Impl& machine);
[[nodiscard]] bool run_database(Impl& machine, const Goal& goal);
[[nodiscard]] bool run_module_prop(Impl& machine, const Goal& goal);
[[nodiscard]] bool run_solutions(Impl& machine, const Goal& goal);
void init_streams(Impl& machine);
[[nodiscard]] bool run_streams(Impl& machine, const Goal& goal);
void run_current_input(Impl& machine, const Goal& goal);
void run_current_output(Impl& machine, const Goal& goal);
void run_set_input(Impl& machine, const Goal& goal);
void run_set_output(Impl& machine, const Goal& goal);
void run_set_position(Impl& machine, const Goal& goal);
void run_at_end(Impl& machine, const Goal& goal);
void run_stream_property(Impl& machine, const Goal& goal);
[[nodiscard]] bool redo_stream_prop(Impl& machine);
[[nodiscard]] bool run_char_io(Impl& machine, const Goal& goal);
[[nodiscard]] bool run_byte_io(Impl& machine, const Goal& goal);
[[nodiscard]] bool run_read(Impl& machine, const Goal& goal);
[[nodiscard]] bool run_write(Impl& machine, const Goal& goal);
[[nodiscard]] bool run_operators(Impl& machine, const Goal& goal);
[[nodiscard]] bool redo_current_op(Impl& machine);
[[nodiscard]] bool redo_current_char_conversion(Impl& machine);
[[nodiscard]] bool run_lists(Impl& machine, const Goal& goal);
[[nodiscard]] bool redo_length_gen(Impl& machine);
[[nodiscard]] bool run_format_float(Impl& machine, const Goal& goal);
[[nodiscard]] bool run_case(Impl& machine, const Goal& goal);
[[nodiscard]] bool run_char_width(Impl& machine, const Goal& goal);
int apply_char_conv_directive(Impl& machine, Cell directive);
[[nodiscard]] bool redo_retract(Impl& machine);
[[nodiscard]] bool redo_clause_scan(Impl& machine);
[[nodiscard]] bool redo_pred_gen(Impl& machine);
// slots[n] はスロット n の最初の出現で作る。匿名は slots を使わない。
[[nodiscard]] Cell ir_to_term(Impl& machine, const IrTerm& term, std::vector<Cell>& slots, bool as_goal);
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
// 定義は session.cpp。initialization(G, main) を Task 2 の規則で動かし、終了コードを返す。文面は warning に足す。
[[nodiscard]] int run_main_goal(Machine& machine, const std::string& goal, std::string& warning);
// 定義は native.cpp。翻訳器（Task 4）も同じ関数で指紋を出す。
[[nodiscard]] std::uint64_t clause_fingerprint(const AtomTable& atoms, const Predicate& predicate);
// install_unit が生成関数を付けてよい述語か（指紋は見ない）。翻訳器も同じ関数で出す述語を選ぶ。
[[nodiscard]] bool native_eligible(const Impl& machine, const Predicate& predicate);
// 付けたあとの節か meta への書き込みのたびに呼ぶ。述語と全単位の同じ鍵の calls を一度に空にする。
void detach_native(Impl& machine, Predicate& predicate);
// 定義は native_redo.cpp。
void call_native(Impl& machine, rt::NativeProc proc, std::uint64_t frame);
void resume_native(Impl& machine);
[[nodiscard]] bool redo_native(Impl& machine);

}
