// print/1,2 の portray/1 の道（規格 8.14 の外の print/1。write_pred.cpp の頭の注釈）と、入れ子の実行 run_goal。
// portray/1 は user から呼び、その束縛は字面を取った後に戻す（レビュー H-24、H-25）。halt と例外の扱いは
// write_pred.cpp の頭の注釈のとおり。引数の検査と誤り（existence_error(stream) を含む）は write_pred.cpp の
// run_out と emit_written が受け持ち、ここは誤りを投げない。run_goal は with_output_to/2（write_capture.cpp）も使う。
// write_pred.cpp から分けた（800 行の上限のため）。移しただけで、振る舞いは変えない。
// テスト: tests/pl/print.pl, tests/pl/print_module.pl, tests/cpp/portray_halt_test.cpp, tests/pl/write.pl

#include "write_internal.hpp"

#include "../machine/internal.hpp"

#include "mosa/writer.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace mosa {
using namespace write_internal;
namespace {

struct Snap {
  std::vector<Entry> stack;
  bool failed = false;
  bool exception = false;
  bool eval_abort = false;
  Cell ball{};
  std::size_t heap_mark = 0;
  std::size_t trail_mark = 0;
  std::size_t extras_mark = 0;
};

struct StreamSpot {
  std::size_t bytes = 0;
  std::size_t cursor = 0;
  std::size_t char_no = 0;
  std::size_t line = 1;
  std::size_t line_pos = 1;
  bool past_end = false;
};

StreamSpot spot_of(const Stream& stream) {
  return StreamSpot{stream.bytes.size(), stream.cursor, stream.char_no, stream.line, stream.line_pos,
                    stream.past_end};
}

void rewind_spot(Stream& stream, const StreamSpot& spot) {
  stream.bytes.resize(spot.bytes);
  stream.cursor = spot.cursor;
  stream.char_no = spot.char_no;
  stream.line = spot.line;
  stream.line_pos = spot.line_pos;
  stream.past_end = spot.past_end;
}

IrTerm portray_call(std::uint32_t name) {
  // 引数は変数スロットのまま渡す。data に通すと単項のマイナスが数に畳まれる。
  IrTerm arg;
  arg.tag = IrTag::Var;
  arg.slot = 0;
  IrTerm goal;
  goal.tag = IrTag::Fun;
  goal.atom = name;
  goal.arity = 1;
  goal.args.push_back(arg);
  return goal;
}

// writer の訪問を GC の訪問へつなぐ。名前の無い変数の表は弱い参照の番地で持つ。
class HeldRoots final : public WriteRoots {
 public:
  explicit HeldRoots(GcVisitor& visitor) : visitor_(visitor) {}
  void cell(Cell& slot) override { visitor_.cell(slot); }
  void weak(std::uint64_t& raw) override { visitor_.weak_address(raw); }

 private:
  GcVisitor& visitor_;
};

Snap take_snap(const Impl& machine) {
  Snap saved;
  saved.stack = machine.stack;
  saved.failed = machine.failed;
  saved.exception = machine.exception;
  saved.eval_abort = machine.eval_abort;
  saved.ball = machine.ball;
  saved.heap_mark = machine.heap.size();
  saved.trail_mark = machine.trail.slots.size();
  saved.extras_mark = machine.extras.size();
  return saved;
}

void disarm(Impl& machine, std::size_t floor) {
  for (std::size_t index = 0; index < floor; ++index) {
    if (machine.stack[index].pick == Pick::Catch) machine.stack[index].armed = false;
  }
}

void trim_extras(Impl& machine, std::size_t mark) { shrink_extras(machine, mark); }

// 入れ子の run_loop の間に GC が動かす値（gc.hpp）。saved.stack は呼ぶ前の機械のスタックの写しである。
void visit_snap(GcVisitor& visitor, Snap& saved) {
  for (Entry& entry : saved.stack) visitor.entry(entry);
  // exception が偽のときの ball は古い番地でありうる（gc.hpp の Impl::ball と同じ）。辿らずに消す。
  if (saved.exception) visitor.cell(saved.ball);
  else saved.ball = Cell{};
  visitor.heap_mark(saved.heap_mark);
  visitor.trail_mark(saved.trail_mark);
  visitor.extras_mark(saved.extras_mark);
}

}  // namespace

namespace write_internal {

// apply_portray は portray/1 を user から呼ぶので、同じ引き当てで user から見える portray/1（user の定義か取り込み）
// だけを見る。前はどのモジュールの portray/1 も見たので、user から見えない portray/1 があると user から呼んで
// existence_error になった（レビュー 2026-11-20 の H-24）。
bool portray_ready(Impl& machine, std::uint32_t& name) {
  const std::uint32_t portray = machine.atoms.intern("portray");
  const Predicate* predicate = resolve_pred(machine, machine.user_module, portray, 1);
  if (predicate == nullptr || predicate->abolished || live_count(*predicate) == 0) return false;
  name = portray;
  return true;
}

WriteCut apply_portray(void* raw, Cell term, std::string& out, WriteHeld& held) {
  auto* hook = static_cast<PortrayHook*>(raw);
  if (hook->stop) return WriteCut::Stop;
  Impl& machine = *hook->machine;
  // 変数には portray を呼ばない。呼ぶと、どの節の頭にも合って束縛を作り、変数の字面の代わりにその書き出しが出た
  // （レビュー 2026-11-20 の H-25）。writer の掛け金は変数でも呼ぶ（lgt_exec の paste_saved が変数を差し替える）ので、
  // ここで外す。
  if (machine.heap.unbound(machine.heap.deref(term))) return WriteCut::Keep;
  if (!hook->have_scratch) {
    hook->scratch = push_memory(machine);
    hook->have_scratch = true;
  }
  const StreamSpot spot = spot_of(machine.streams[hook->scratch]);
  // write/1 は現在出力へ書く。親の字面へ挟むため、成功と halt の分だけメモリから戻す。scratch は hook が閉じる。
  OutputScope scope(machine, hook->scratch, false);
  // portray/1 は user の述語を探して呼ぶ（portray_ready）。入れ子の GC は書きかけの項を動かすので、writer が
  // 積んだ仕事と表を見せる（ADR 0011）。
  const RootScope roots(machine, [&held](GcVisitor& visitor) {
    HeldRoots adapter(visitor);
    held.visit(adapter);
  });
  // print/1 は何も束縛しない（@term）ので、portray/1 の束縛は字面を取った後に戻す（レビュー 2026-11-20 の H-25）。
  // 残すと、portray/1 に合った束縛済みの項の中の変数まで呼び手の側で束縛された。
  const Ran ran = run_goal(machine, make_run_goal(machine.heap, portray_call(hook->name)),
                           make_env(machine.heap, {term}), machine.user_module, Bindings::Undo);
  scope.restore();
  // 下書きを閉じたら、print を止める。続けると次の portray の write/1 が閉じた流れを名指し、閉じた後の書き出しが
  // print の字面より先に出た（レビュー M-31）。閉じた流れの中身は読まない。例外と halt は今までどおり止める。
  if (machine.streams[hook->scratch].closed) {
    hook->stop = true;
    hook->halted = ran == Ran::Halted;
    hook->scratch_closed = ran == Ran::Ok || ran == Ran::Fail;
    if (hook->scratch_closed) machine.failed = false;
    return hook->halted ? WriteCut::Last : WriteCut::Stop;
  }
  // portray の push_back が vector を伸ばすと、呼ぶ前の参照は無効になる。
  Stream& stream = machine.streams[hook->scratch];
  // halt は write/1 の後の halt と同じく、それまでに書いた分を行き先に残す（レビュー L14）。例外は catch で続きが
  // 書かれうるので、書きかけを残さない。
  if (ran == Ran::Ok || ran == Ran::Halted) out = stream.bytes.substr(spot.bytes);
  // ゴール全体が失敗した書き出しを残すと、続く write の字面と重なる。
  rewind_spot(stream, spot);
  if (ran == Ran::Ok) return WriteCut::Swap;
  if (ran == Ran::Fail) {
    machine.failed = false;
    return WriteCut::Keep;
  }
  hook->stop = true;
  hook->halted = ran == Ran::Halted;
  return hook->halted ? WriteCut::Last : WriteCut::Stop;
}


Ran run_goal(Impl& machine, const GoalRef& goal, const EnvRef& env, std::uint32_t module, Bindings bindings) {
  Snap saved = take_snap(machine);
  const RootScope roots(machine, [&saved](GcVisitor& visitor) { visit_snap(visitor, saved); });
  disarm(machine, machine.stack.size());
  Entry barrier;
  barrier.pick = Pick::Barrier;
  barrier.heap_mark = saved.heap_mark;
  barrier.trail_mark = saved.trail_mark;
  barrier.extras_mark = saved.extras_mark;
  push_frame(machine, barrier);
  machine.failed = false;
  machine.exception = false;
  machine.eval_abort = false;
  launch(machine, goal, kNoFrame, machine.stack.size(), env, module);
  Stop stop = run_loop(machine);
  // portray は 1 回だけ解く。床の上に残った選択点を捨てる前に切り、setup_call_cleanup/3 の後始末を動かす（ADR 0016）。
  if (stop.kind == StopKind::Success) {
    Stop closed = close_cleanups(machine, saved.stack.size() + 1);
    if (closed.kind != StopKind::Success) stop = std::move(closed);
  }
  machine.stack = saved.stack;
  count_vectors(machine);
  trim_extras(machine, saved.extras_mark);
  if (stop.kind == StopKind::Exception) {
    const ExportedTerm ball = export_term(machine, machine.ball);
    machine.failed = saved.failed;
    machine.exception = false;
    machine.eval_abort = saved.eval_abort;
    propagate_throw(machine, import_term(machine, ball));
    return Ran::Threw;
  }
  // portray/1 の中のデバッガの a も、halt と同じく書くのを止める。abort_now は立ったまま外の run_loop に届く。
  if (stop.kind == StopKind::Halt || stop.kind == StopKind::Abort) {
    machine.failed = false;
    machine.exception = false;
    if (stop.kind == StopKind::Halt) machine.halt_now = true;
    return Ran::Halted;
  }
  if (stop.kind != StopKind::Success) {
    machine.failed = true;
    machine.exception = saved.exception;
    machine.eval_abort = saved.eval_abort;
    machine.ball = saved.ball;
    return Ran::Fail;
  }
  // 床まで失敗で戻ったときと同じ戻し方をする。印は入れ子の GC が saved ごと直してある（visit_snap）。
  if (bindings == Bindings::Undo) restore_state(machine, saved.heap_mark, saved.trail_mark, saved.extras_mark);
  machine.failed = false;
  machine.exception = false;
  machine.eval_abort = false;
  return Ran::Ok;
}

std::size_t push_memory(Impl& machine) {
  Stream made;
  made.kind = Stream::Kind::Memory;
  made.data = Stream::Data::Text;
  made.mode = Stream::Mode::Write;
  made.reposition = true;
  machine.streams.push_back(std::move(made));
  return machine.streams.size() - 1;
}

}  // namespace write_internal

}  // namespace mosa
