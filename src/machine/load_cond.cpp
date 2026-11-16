// 条件付きコンパイル :- if/1、elif/1、else/0、endif/0（13211-1 に無い。SPEC 第3節、ADR 0027、iso-notes
// "Conditional compilation (not in 13211-1)"）。read_terms が項ごとに CondFrames::step を呼び、条件は run_condition が
// 読み込み中のモジュールで 1 回解く。指令の形は :- の 4 つだけで、ほかの項数（:- if(C, T, E) など）は一般の指令のまま。
// 誤り: if の無い elif・else・endif は cond_unmatched、else の後ろの elif と 2 つ目の else は cond_after_else、閉じない
// if は cond_unclosed（どれも読み込みの失敗で、読み飛ばし中の入れ子の節でも失敗させる。計画の「設計との違い」1）。
// 条件の例外はその Formal、halt/0,1 は読み込みをその場で止める。
// 翻訳の読み込み（mosac、Impl::translation がある）も条件を解く。そこでは例外と halt は「その分岐を取らない」とだけ
// 扱い、読み飛ばした項からライブラリの名前と印を書き留める（問い 1 の A、SPEC 第6節）。条件より前に飛ばしたゴールが
// ある分岐の中で指令が失敗しても、翻訳を止めずに起動時の読み込みに任せる（I2、持ち主の選択 A）。
// テスト: tests/pl/cond_compile.pl、tests/pl/japanese.pl、tests/cpp/cond_compile_test.cpp、翻訳の読み込みは
//         tests/cpp/codegen_test.cpp（check_cond_translation、check_cond_build_failure、check_cond_log）と
//         tests/mosac/standalone_test.cmake（cond_*）
#include "load_internal.hpp"

#include "mosa/writer.hpp"

#include <algorithm>
#include <string_view>

namespace mosa {

namespace {

enum class CondKind { None, If, Elif, Else, Endif };

// term が条件の指令ならその種類。if と elif は条件を goal に入れる。
CondKind cond_kind(const Impl& machine, Cell term, Cell& goal) {
  term = machine.heap.deref(term);
  if (term.tag() != Tag::Str || machine.heap.functor_arity(term) != 1) return CondKind::None;
  if (machine.atoms.name(machine.heap.functor_atom(term)) != ":-") return CondKind::None;
  const Cell body = machine.heap.deref(machine.heap.arg(term, 0));
  if (body.tag() == Tag::Atom) {
    const std::string_view name = machine.atoms.name(static_cast<std::uint32_t>(body.raw() >> 3));
    if (name == "else") return CondKind::Else;
    return name == "endif" ? CondKind::Endif : CondKind::None;
  }
  if (body.tag() != Tag::Str || machine.heap.functor_arity(body) != 1) return CondKind::None;
  const std::string_view name = machine.atoms.name(machine.heap.functor_atom(body));
  if (name != "if" && name != "elif") return CondKind::None;
  goal = machine.heap.arg(body, 0);
  return name == "if" ? CondKind::If : CondKind::Elif;
}

// 対の誤り。指令を書いた字面（:-endif など）を添える。
int fail_cond(Impl& machine, std::string_view key, Cell term) {
  fail_load(machine, message_text(machine, key, {write_term(machine.heap, machine.operators, term, {}, {})}));
  return -1;
}

// 翻訳の読み込みで if の条件を解いている間も、その if を閉じていない if に数える。条件のゴールが開いたファイル
// （:- if((C, use_module(m))).）は、起動時に C が違えば読まれないので、分岐の中で開いた物と同じに扱う（ADR 0027）。
// 節はまだ積んでいないので、ここで足して引く。C++ の例外（ヒープの上限）で抜けても数を残さない。
class Evaluating {
 public:
  explicit Evaluating(TranslationLog* log) : log_(log) {
    if (log_ != nullptr) ++log_->open_conditions;
  }
  ~Evaluating() {
    if (log_ != nullptr) --log_->open_conditions;
  }
  Evaluating(const Evaluating&) = delete;
  Evaluating& operator=(const Evaluating&) = delete;

 private:
  TranslationLog* log_;
};

// 翻訳を止めずに読み続ける失敗の跡を消す。run_aside と読み込みの指令が書いた診断と halt の求めを残すと、翻訳の後の段
// （translate_program）がプログラムの失敗や終了と取り違える。条件の中の consult が残した印（load_kept）も持ち越さない。
void forget_failure(Impl& machine) {
  machine.diagnostic.clear();
  machine.syntax_diag = false;
  machine.halt_now = false;
  machine.halt_code = 0;
  machine.load_kept = false;
}

// 翻訳の読み込みで、飛ばしたゴールの印があるファイルの数。前のファイルの assertz/1 も後ろのファイルの条件を変えうる
// ので、今のファイル（と読み終えた子）だけを見る mark_opaque より広く、記録の全部のファイルを数える。
std::size_t skipped_files(const TranslationLog* log) {
  if (log == nullptr) return 0;
  return static_cast<std::size_t>(std::count_if(log->files.begin(), log->files.end(),
                                                [](const TranslationLog::File& file) { return file.skipped_goal; }));
}

}  // namespace

int run_condition(Impl& machine, Cell goal) {
  const StopKind kind = run_parked(machine, in_load_module(machine, goal));
  // 翻訳の読み込みでは、例外も halt も「その分岐を取らない」とだけ扱い、翻訳を止めない（ADR 0027）。本当の扱いは
  // 実行ファイルの起動時の読み込みが決める。
  if (machine.translation != nullptr && (kind == StopKind::Exception || kind == StopKind::Halt)) {
    forget_failure(machine);
    return 0;
  }
  // 例外の Formal は run_aside が診断に書いた。SWI-Prolog は偽として読み続けるが、Mosa は一般の指令と同じく読み込みを
  // 失敗させる（iso-notes "Conditional compilation (not in 13211-1)"）。
  if (kind == StopKind::Exception) return -1;
  // halt/0,1 は run_directive_goal と同じく読み込みをその場で止め、halt_now を残す。読んだ節は戻さない。
  if (kind == StopKind::Halt) {
    machine.load_kept = true;
    return -1;
  }
  // 成功も失敗も読み込みを続ける。条件の中の consult が初期化の例外で残した印を持ち越すと、この読み込みの後の
  // 構文の誤りで、取り消すはずの節が残る（run_directive_goal の成功と同じ理由）。
  machine.load_kept = false;
  return kind == StopKind::Success ? 1 : 0;
}

CondFrames::CondFrames(Impl& machine) : machine_(machine), log_(machine.translation) {}

CondFrames::~CondFrames() {
  if (log_ != nullptr) log_->open_conditions -= frames_.size();
}

// 節を積む所は 1 つにして、翻訳の読み込みの数（開いたファイルが分岐の中かを FileNote が見る）と食い違わせない。
void CondFrames::push(Frame frame) {
  frames_.push_back(frame);
  if (log_ != nullptr) ++log_->open_conditions;
}

// 起動時の読み込みはこの項を読むかもしれない。字面のライブラリは同梱の閉包に入れ、取らなかった分岐があることを
// ファイルに書き留める（公開の未定義を警告にする）。指令は op/3 かもしれず、後の項の読みを変えうるので、飛ばした
// 指令のゴールと同じ印にする。原文の無い読み込み（load_program の文字）には書き留めるファイルが無い。
void CondFrames::note_skipped(Cell term) {
  if (log_ == nullptr) return;
  note_goal_libraries(machine_, term);
  if (log_->current >= log_->files.size()) return;
  TranslationLog::File& file = log_->files[log_->current];
  file.untaken_branch = true;
  if (is_directive(machine_.heap, term)) file.skipped_goal = true;
}

bool CondFrames::skipping() const { return !frames_.empty() && !frames_.back().reading; }

int CondFrames::step(Cell term) {
  Cell goal{};
  const CondKind kind = cond_kind(machine_, term, goal);
  // 条件のゴールに字面で書いたライブラリも同梱の閉包に入れる（フェーズ12 の問い 2 の A）。条件の指令は take_term へ
  // 行かないので、ここで拾う。ビルド時の評価がそこへ届かなくても、解かない elif でも、起動時には届きうる。
  if (log_ != nullptr && (kind == CondKind::If || kind == CondKind::Elif)) note_goal_libraries(machine_, term);
  switch (kind) {
    case CondKind::If:
      // 読み飛ばし中の if も読み飛ばした項である。条件は呼ばれないので、書き留めてから節を積む。
      if (skipping()) note_skipped(term);
      return open(goal);
    case CondKind::Elif: return next_branch(term, goal);
    case CondKind::Else: return otherwise(term);
    case CondKind::Endif: return finish(term);
    case CondKind::None: break;
  }
  if (!skipping()) return 0;
  note_skipped(term);
  return 1;
}

// 読み飛ばし中に開いた if は条件を呼ばず、endif と対にするためだけに積む。中の else と elif の誤りも見る。
int CondFrames::open(Cell goal) {
  if (skipping()) {
    push(Frame{true, false, false, false});
    return 1;
  }
  int holds = 0;
  {
    const Evaluating evaluating{log_};
    holds = run_condition(machine_, goal);
  }
  if (holds < 0) return -1;
  // 飛ばしたゴールは条件を解いた後で見る。条件のゴールが読んだファイル（:- if(consult(f)).）の中で飛ばしたゴールも、
  // 起動時にはその条件の答えを変えうる。
  push(Frame{false, holds == 1, false, holds == 1, skipped_files(log_) > 0});
  return 1;
}

// 対の誤りは条件を呼ぶ前に見る。外が読み飛ばし中か前の分岐を取っていれば、条件を呼ばずに endif まで読み飛ばす。
int CondFrames::next_branch(Cell term, Cell goal) {
  if (frames_.empty()) return fail_cond(machine_, "cond_unmatched", term);
  if (frames_.back().seen_else) return fail_cond(machine_, "cond_after_else", term);
  if (frames_.back().outer_skipping || frames_.back().taken) {
    frames_.back().reading = false;
    return 1;
  }
  // 同じ if の前の分岐で読み飛ばした指令（note_skipped の印）は、この条件を変えない。起動時にその分岐を取れば、この
  // elif は読まない。だから if の時の値に、この条件のゴールが読んだファイルで新しく付いた印だけを足す。
  const std::size_t before = skipped_files(log_);
  const int holds = run_condition(machine_, goal);
  if (holds < 0) return -1;
  frames_.back().taken = holds == 1;
  frames_.back().reading = holds == 1;
  frames_.back().doubtful = frames_.back().doubtful || skipped_files(log_) > before;
  return 1;
}

int CondFrames::otherwise(Cell term) {
  if (frames_.empty()) return fail_cond(machine_, "cond_unmatched", term);
  Frame& frame = frames_.back();
  if (frame.seen_else) return fail_cond(machine_, "cond_after_else", term);
  frame.seen_else = true;
  frame.reading = !frame.outer_skipping && !frame.taken;
  frame.taken = true;
  return 1;
}

int CondFrames::finish(Cell term) {
  if (frames_.empty()) return fail_cond(machine_, "cond_unmatched", term);
  frames_.pop_back();
  if (log_ != nullptr) --log_->open_conditions;
  return 1;
}

// 閉じない if の診断は、テキストを読み終えた所でだけ書く。例外や halt で抜けた読み込みの診断を上書きしない。
bool CondFrames::close() {
  if (frames_.empty()) return true;
  fail_load(machine_, message_text(machine_, "cond_unclosed"));
  return false;
}

// 一番内の節だけを見る。外の節の条件はそれより前に解いたので、そこまでに飛ばしたゴールは内の節の doubtful にも入る。
// else の分岐は、前の条件が全部偽だったから読むので、if と解いた elif の値を持ち越す。分岐の外と、条件を解いた後で
// 飛ばしたゴールしか無い分岐の失敗は、起動時にも同じく起きるので任せない。
std::optional<std::size_t> CondFrames::tolerable(Cell term) const {
  if (log_ == nullptr || frames_.empty() || !frames_.back().doubtful || !is_directive(machine_.heap, term)) {
    return std::nullopt;
  }
  return log_->files.size();
}

// ビルド時に取った分岐を起動時には取らないかもしれないので、失敗した指令の後ろも読み続け、判定を起動時の読み込みに
// 任せる。指令が開いたファイル（分岐の中の use_module(m) の m と、m が読んだファイル）も同じ印にする。半分だけ読んだ
// 節から生成関数を出さず、定義の無いままの公開で翻訳を止めない。準備済みの印はここでは触らない。途中までしか読んで
// いないファイルの印は、翻訳の読み込みでは CycleGuard が外すので、後ろの分岐の外の use_module が読み直し、起動時にも
// 起きるその失敗で止まる。読み終えたファイルの印は残し、同じ読み込みで 2 度読んで節が離れていると誤らない。
// 原文の無いテキスト（load_program）は印を付ける所が無いので任せない。
bool CondFrames::tolerate(std::optional<std::size_t> known) {
  if (!known || log_->current >= log_->files.size()) return false;
  log_->files[log_->current].opaque = true;
  for (std::size_t index = *known; index < log_->files.size(); ++index) log_->files[index].opaque = true;
  forget_failure(machine_);
  return true;
}

}  // namespace mosa
