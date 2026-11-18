// setup_call_cleanup(:Setup, :Goal, :Cleanup)。非 ISO（SPEC 第7節の段階 G、レビュー M11、ADR 0016）で、SWI-Prolog の
// boot/init.pl と pl-wam.c に合わせる。
//   once(Setup) が成功すれば Goal を呼び、Goal が選択点を残さずに抜けたとき、失敗したとき、例外を投げたとき、選択点を
//   切られたときに、Cleanup を once/1 として 1 回動かす。Cleanup の成否は見ない。抜けたときと切られたときは Goal の
//   束縛が見え、Cleanup の束縛も残る。失敗と例外のときは Setup の直後まで戻した状態で動く。例外のときは Cleanup の
//   例外を捨てて Goal の例外を投げ直し、ほかのときは Cleanup の例外がそのまま出る。halt とヒープの上限での停止は
//   Cleanup を動かさない。デバッガの a は止まる前に、例外のときと同じ束縛で動かす（ADR 0028 の改訂）。
//   Setup の誤りは once/1 が出す。Cleanup の instantiation_error と type_error(callable, Cleanup)（M: を外して見る）は
//   Setup の後で仕掛ける前、Goal の誤りは仕掛けた後に出るので、そのときは Cleanup が動く。Goal や Cleanup の M: の層、
//   Goal の制御構造が巡回すれば resource_error(cyclic_term)（iso-notes 7.3.3、2026-11-17 の持ち主の決定 A）。
// テスト: tests/pl/utilities.pl、tests/pl/cleanup_dir.pl、tests/cpp/debugger_abort_test.cpp（デバッガの a が動かす後始末）、
//   tests/cpp/data/debugger（transcript_debug_abort_heap_limit）
#include "../machine/internal.hpp"

#include <array>
#include <cstdint>
#include <exception>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace mosa {
namespace {

Cell structure(Impl& machine, std::string_view name, std::span<const Cell> args) {
  return machine.heap.structure(machine.atoms.intern(name), args);
}

Cell atom(Impl& machine, std::string_view name) { return machine.heap.atom(machine.atoms.intern(name)); }

// (call(C) -> true ; true)。once/1 と同じく Cleanup の選択点を切り、失敗しても先へ進む。中のカットは call/1 が閉じる。
// error があれば (catch(call(C), Error, true) -> true ; true) にし、Cleanup の例外を Error に受けて先へ進む。
Cell guarded(Impl& machine, Cell cleanup, std::optional<Cell> error) {
  Cell call = structure(machine, "call", std::array<Cell, 1>{cleanup});
  if (error) call = structure(machine, "catch", std::array<Cell, 3>{call, *error, atom(machine, "true")});
  const Cell commit = structure(machine, "->", std::array<Cell, 2>{call, atom(machine, "true")});
  return structure(machine, ";", std::array<Cell, 2>{commit, atom(machine, "true")});
}

// (nonvar(E1) -> throw(E1) ; nonvar(E2) -> throw(E2) ; … ; true)。受けた例外のうち最初のものを投げ直す。
// 投げられた項は変数ではない（throw/1 が instantiation_error にする）ので、nonvar で受けたかが分かる。
Cell rethrow_first(Impl& machine, std::span<const Cell> errors) {
  Cell rest = atom(machine, "true");
  for (auto it = errors.rbegin(); it != errors.rend(); ++it) {
    const Cell test = structure(machine, "nonvar", std::array<Cell, 1>{*it});
    const Cell again = structure(machine, "throw", std::array<Cell, 1>{*it});
    const Cell branch = structure(machine, "->", std::array<Cell, 2>{test, again});
    rest = structure(machine, ";", std::array<Cell, 2>{branch, rest});
  }
  return rest;
}

// term を module で動かす枠。カットは term の中に閉じる。
std::uint64_t frame_of(Impl& machine, Cell term, std::uint64_t cont, std::uint32_t module) {
  const Compiled compiled = compile_goal(machine.heap, term);
  return embody(machine, compiled.term, cont, machine.stack.size(), compiled.env, module);
}

void raise(Impl& machine, Cell formal) {
  raise_formal(machine, formal, machine.atoms.intern("setup_call_cleanup"), 3);
}

bool callable_cell(Cell cell) {
  return cell.tag() == Tag::Atom || cell.tag() == Tag::Str || cell.tag() == Tag::Lis;
}

// M:G の G。SWI-Prolog の mustBeCallable と同じく、修飾を外した項で呼べるかを見る。
// M: の層が巡回すれば nullopt（iso-notes 7.3.3、2026-11-17 の持ち主の決定 A）。前は回り続けた。
std::optional<Cell> unqualified(const Impl& machine, Cell cell) {
  cell = machine.heap.deref(cell);
  LayerWatch watch(cell);
  while (cell.tag() == Tag::Str && machine.heap.functor_arity(cell) == 2 &&
         machine.atoms.name(machine.heap.functor_atom(cell)) == ":") {
    cell = machine.heap.deref(machine.heap.arg(cell, 1));
    if (watch.returned_to(cell)) return std::nullopt;
  }
  return cell;
}

// 1 回のカットが外した後始末を、cont の前に順につなぐ。pending は外した順（上の Entry が先）に並び、後ろから継続を
// つなぐと先頭が最初に動く。2 つ以上なら、先のものが投げても残りを動かし（SWI-Prolog の discardChoicesAfter と
// 同じ。外側の Entry はもうスタックに無いので、例外の伝わりでは拾えない）、全部の後で最初の例外を投げ直す
// （敵対的レビュー A2）。1 つなら受けずにそのまま伝える。結果は同じで、カットのたびの catch/3 を省ける。
std::uint64_t chain_cleanups(Impl& machine, std::span<const PendingCleanup> pending, std::uint64_t cont) {
  if (pending.size() == 1) {
    return frame_of(machine, guarded(machine, pending[0].goal, std::nullopt), cont, pending[0].module);
  }
  std::vector<Cell> errors;
  errors.reserve(pending.size());
  for (std::size_t index = 0; index < pending.size(); ++index) errors.push_back(machine.heap.fresh_ref());
  // 投げ直しの試しは利用者のゴールではない。system の文脈で動かし、デバッガの箱を作らせない（ADR 0028）。
  cont = frame_of(machine, rethrow_first(machine, errors), cont, machine.system_module);
  for (std::size_t index = pending.size(); index-- > 0;) {
    cont = frame_of(machine, guarded(machine, pending[index].goal, errors[index]), cont, pending[index].module);
  }
  return cont;
}

// once(Setup) の後に '$call_cleanup'(Goal, Cleanup) を動かす。Setup の誤りと失敗は once/1 のものになる。once は
// '$once'/1（once/1 と同じ働きと誤り）で積み、デバッガに組込の中のゴールとして箱を作らせない（ADR 0028）。
void run_setup(Impl& machine, const Goal& goal) {
  const Cell once = structure(machine, "$once", std::array<Cell, 1>{goal.arg(machine, 0)});
  const Cell arm = structure(machine, "$call_cleanup", std::array<Cell, 2>{goal.arg(machine, 1), goal.arg(machine, 2)});
  const Compiled compiled = compile_goal(machine.heap, structure(machine, ",", std::array<Cell, 2>{once, arm}));
  if (raise_cyclic_goal(machine, compiled, goal.name, goal.arity)) return;
  launch(machine, compiled.term, goal.cont, machine.stack.size(), compiled.env, goal.module);
}

// Cleanup を調べてから Entry を積み、Ctrl::CleanupExit の枠を継続にして Goal を呼ぶ。Goal の誤りは積んだ後に投げるので、
// propagate_throw が Entry に着いて Cleanup を動かす。
void run_arm(Impl& machine, const Goal& goal) {
  const Cell cleanup = goal.arg(machine, 1);
  const auto stripped = unqualified(machine, cleanup);
  if (!stripped) return raise(machine, formal_resource(machine, "cyclic_term"));
  const Cell bare = *stripped;
  if (machine.heap.unbound(bare)) return raise(machine, formal_instantiation(machine));
  if (!callable_cell(bare)) return raise(machine, formal_type(machine, "callable", bare));
  Entry entry;
  entry.pick = Pick::CallCleanup;
  entry.pattern = cleanup;
  entry.module = goal.module;
  entry.heap_mark = machine.heap.size();
  entry.trail_mark = machine.trail.slots.size();
  push_frame(machine, std::move(entry));
  Extra exit;
  exit.ctrl = Ctrl::CleanupExit;
  exit.trim_to = machine.stack.size() - 1;
  exit.cont = goal.cont;
  const auto at = push_extra(machine, std::move(exit));
  const Cell inner = machine.heap.deref(goal.arg(machine, 0));
  if (machine.heap.unbound(inner)) return raise(machine, formal_instantiation(machine));
  if (!callable_cell(inner)) return raise(machine, formal_type(machine, "callable", inner));
  const Compiled compiled = compile_goal(machine.heap, inner);
  if (!body_callable(*compiled.term)) return raise(machine, formal_type(machine, "callable", inner));
  if (raise_cyclic_goal(machine, compiled, machine.atoms.intern("setup_call_cleanup"), 3)) return;
  launch(machine, compiled.term, at, machine.stack.size(), compiled.env, goal.module);
}

// abort の後始末を、それぞれ自分の catch/3 の中で順に動かす。下にまだ Entry が残るので、受けずに投げると利用者の
// catch/3 が後始末の誤りを捕まえ、止めたはずのプログラムが先へ進んでしまう。失敗しても次へ進む。
void run_aborted(Impl& machine, std::span<const PendingCleanup> cleanups) {
  std::uint64_t cont = kNoFrame;
  for (std::size_t index = cleanups.size(); index-- > 0;) {
    const Cell goal = guarded(machine, cleanups[index].goal, machine.heap.fresh_ref());
    cont = frame_of(machine, goal, cont, cleanups[index].module);
  }
  machine.ip = cont;
  // 例外のポートの a は巻き戻しの途中で戻る。印を残すと settle_cleanups が古い ball を投げ直す。
  machine.native_next = nullptr;
  machine.failed = false;
  machine.exception = false;
  // 後始末の中で trace を入れて打った a は、入れ子の run_abort_cleanups が残りの Entry を動かす。
  machine.abort_now = false;
  static_cast<void>(run_loop(machine));
  machine.failed = false;
  machine.exception = false;
}

// 床より上の Entry の後始末を上から 1 つずつ、その Entry の印まで戻してから動かす。throw_through_cleanup と同じく
// Setup の束縛は見え、Goal と内側の後始末の束縛は見えない（2026-11-18 の持ち主の決定 B）。印を先に集めず Entry を
// スタックに残したまま外すのは、後始末の中の GC が Entry の印と項は直しても、集めた写しは直さないからである。
void unwind_aborted(Impl& machine, std::size_t floor) {
  while (!machine.halt_now) {
    std::size_t index = machine.stack.size();
    while (index > floor && machine.stack[index - 1].pick != Pick::CallCleanup) --index;
    if (index == floor) return;
    // 上に CallCleanup は無いので、この切り詰めは後始末を積まない。
    trim_stack(machine, index);
    const Entry entry = std::move(machine.stack.back());
    machine.stack.pop_back();
    prune_catch_log(machine);
    restore_state(machine, entry.heap_mark, entry.trail_mark, entry.extras_mark);
    const std::array<PendingCleanup, 1> one{PendingCleanup{entry.pattern, entry.module}};
    run_aborted(machine, one);
  }
}

}  // namespace

bool run_cleanup(Impl& machine, const Goal& goal) {
  const std::string_view name = machine.atoms.name(goal.name);
  if (name == "setup_call_cleanup" && goal.arity == 3) {
    run_setup(machine, goal);
    return true;
  }
  if (name == "$call_cleanup" && goal.arity == 2) {
    run_arm(machine, goal);
    return true;
  }
  return false;
}

void exit_cleanup(Impl& machine, std::size_t index, std::uint64_t cont) {
  // 上に選択点が無ければ Goal には戻れない。切れば trim_stack が Cleanup を積む。残れば、後の失敗かカットか例外が動かす。
  if (index + 1 == machine.stack.size() && machine.stack[index].pick == Pick::CallCleanup) trim_stack(machine, index);
  machine.ip = cont;
}

void settle_cleanups(Impl& machine) {
  std::vector<PendingCleanup> pending;
  pending.swap(machine.pending_cleanups);
  // 前に置く枠は、call_goal が次の歩みに渡した生成関数の呼び先とは違う。呼び先は普通の引き当てに戻す。
  machine.native_next = nullptr;
  std::uint64_t cont = machine.ip;
  if (machine.exception) {
    cont = frame_of(machine, structure(machine, "throw", std::array<Cell, 1>{machine.ball}), kNoFrame,
                    machine.user_module);
  } else if (machine.failed) {
    cont = frame_of(machine, atom(machine, "fail"), kNoFrame, machine.user_module);
  }
  machine.exception = false;
  machine.failed = false;
  machine.ip = chain_cleanups(machine, pending, cont);
}

void throw_through_cleanup(Impl& machine, const Entry& entry, const ExportedTerm& ball) {
  restore_state(machine, entry.heap_mark, entry.trail_mark, entry.extras_mark);
  const Cell again = structure(machine, "throw", std::array<Cell, 1>{import_term(machine, ball)});
  const std::uint64_t rethrow = frame_of(machine, again, kNoFrame, entry.module);
  machine.ip = frame_of(machine, guarded(machine, entry.pattern, machine.heap.fresh_ref()), rethrow, entry.module);
  machine.failed = false;
  machine.exception = false;
}

bool has_cleanup_entries(const Impl& machine, std::size_t floor) {
  for (std::size_t index = floor; index < machine.stack.size(); ++index) {
    if (machine.stack[index].pick == Pick::CallCleanup) return true;
  }
  return false;
}

Stop close_cleanups(Impl& machine, std::size_t floor) {
  if (!has_cleanup_entries(machine, floor)) return Stop{StopKind::Success, 0, {}, {}, false};
  trim_stack(machine, floor);
  machine.ip = kNoFrame;
  machine.failed = false;
  machine.exception = false;
  return run_loop(machine);
}

// abort が後始末を動かすのは 2026-11-18 の持ち主の決定 A、例外のときと同じ束縛で動かすのは同じ日の決定 B、ほかは
// 既定 A（open-items）。床は入れ子の run_loop（portray、with_output_to/2）の Barrier の上で、その下は外の run_loop が
// 自分で動かすので内側が先になる。後始末はポートを出さない。打った a の後でまた止まらないためである。その失敗と誤りは
// 捨て、abort を続ける。後始末の中の halt はプロセスを終えるので、残りを動かさずに halt の道へ返す。
void run_abort_cleanups(Impl& machine) {
  std::size_t floor = machine.stack.size();
  while (floor > 0 && machine.stack[floor - 1].pick != Pick::Barrier) --floor;
  if (machine.pending_cleanups.empty() && !has_cleanup_entries(machine, floor)) return;
  // abort の後もデバッガは打ったときの入り切りと進み方のまま（マニュアルの a の行）。後始末の nodebug や trace はそれを
  // 変えない。leash とスパイポイントは、ほかのゴールが変えたときと同じく残る。入れ子の run_loop はヒープの上限で C++ の
  // 例外を投げうるので、抜け方によらず戻す。
  class Restore {
   public:
    explicit Restore(Impl& machine)
        : machine_(machine),
          debug_on_(machine.debug_on),
          mode_(machine.debug.mode),
          unwinding_(std::uncaught_exceptions()) {}
    Restore(const Restore&) = delete;
    Restore& operator=(const Restore&) = delete;
    ~Restore() {
      machine_.debug_on = debug_on_;
      machine_.debug.mode = mode_;
      // ヒープの上限の C++ の例外で抜けるときは、その止まり方が abort に代わるので印を下ろす。残すと、drop_run を
      // 通らない読み込みでは後の指令の失敗が abort と書かれた（PR #4 の 2 度目の敵対的レビュー）。後始末の項を作る
      // 確保は、印を下ろす run_aborted より前に抜けうるので、立て直さないだけでは印が残る（その修正への Muse のレビュー）。
      machine_.abort_now = std::uncaught_exceptions() == unwinding_;
    }

   private:
    Impl& machine_;
    bool debug_on_;
    DebugMode mode_;
    int unwinding_;
  };
  const Restore restore{machine};
  machine.debug_on = false;
  // カットがもう積んだ後始末は、propagate_throw と同じく巻き戻す前に、カットのときの束縛で動かす。
  if (!machine.pending_cleanups.empty()) {
    std::vector<PendingCleanup> pending;
    pending.swap(machine.pending_cleanups);
    run_aborted(machine, pending);
  }
  unwind_aborted(machine, floor);
  if (machine.halt_now) return;
  trim_stack(machine, floor);
  machine.ip = kNoFrame;
}

Stop close_query(Impl& machine) {
  try {
    return close_cleanups(machine, 0);
  } catch (...) {
    // ヒープの上限で抜けたら Machine::query と同じく選択点を残さない。残すと次の問い合わせが古い Entry を閉じに行く。
    drop_run(machine);
    throw;
  }
}

}  // namespace mosa
