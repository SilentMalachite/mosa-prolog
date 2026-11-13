// 問い合わせの入口。字面を読むもの（Machine::query、query_written）と、写しを戻すもの（query_exported）がある。
// engine.cpp から分けた（AGENTS.md の 800 行の上限、レビュー L23）。

#include "internal.hpp"

#include "mosa/reader.hpp"

#include <span>
#include <string_view>
#include <utility>

namespace mosa {
namespace {

// C++ の例外（ヒープの上限、OS のメモリ不足）で抜ける問い合わせは、選択点・トレイル・束縛を残さない。残すと次の
// load_program の指示（run_parked）が古いスタックを退避し、GC が空けたヒープの番地を根として辿る。
template <class Run>
Stop drop_on_throw(Impl& machine, Run run) {
  try {
    return run();
  } catch (...) {
    drop_run(machine);
    throw;
  }
}

Stop start_goal(Impl& machine, Cell goal) {
  // 失敗したクエリの束縛を、選択点が無くても戻すための水位。解の候補には数えない。
  Entry cleanup;
  cleanup.pick = Pick::Cleanup;
  cleanup.heap_mark = machine.heap.size();
  cleanup.trail_mark = machine.trail.slots.size();
  cleanup.extras_mark = machine.extras.size();
  machine.stack.push_back(std::move(cleanup));
  count_vectors(machine);
  const Compiled compiled = compile_goal(machine.heap, goal);
  launch(machine, compiled.term, kNoFrame, 0, compiled.env, machine.user_module);
  return run_loop(machine);
}

Stop start_query(Impl& machine, std::string_view goal_text, const CharConv* conv) {
  Reader reader(goal_text, machine.heap, machine.operators, current_double_quotes(machine), conv);
  reader.set_fullwidth(machine.japanese_punct);
  const ReadOutcome outcome = reader.next();
  if (!outcome.term) return Stop{StopKind::Failure, 0, {}, {}, false};
  for (const VarName& name : outcome.term->names) {
    machine.bindings.push_back(Binding{name.name, name.cell});
  }
  return start_goal(machine, outcome.term->term);
}

// start は前の問い合わせを片付けた後で呼ぶ。片付けはヒープを切り詰めるので、ゴールの項はその後で作る。
template <class Start>
Stop open_query(Impl& machine, Start start) {
  if (machine.halt_pending) {
    machine.halt_pending = false;
    return Stop{StopKind::Halt, machine.halt_code, {}, {}, false};
  }
  // halt で止まった問い合わせの後始末は、次の問い合わせが閉じるときも動かさない（ADR 0016、敵対的レビュー A5）。
  const bool halted = machine.halt_now;
  machine.halt_now = false;
  // 差し替えはどれも抜けるとき戻す（レビュー H6）。それでも閉じた流れを指して来たら、標準の流れから始める。
  // 開いた流れは set_output/1 が問い合わせをまたいで持ち越したものなので戻さない。load_program と next は見ない。
  const auto usable = [&machine](std::size_t index) {
    return index < machine.streams.size() && !machine.streams[index].closed;
  };
  if (!usable(machine.current_in)) machine.current_in = 0;
  if (!usable(machine.current_out)) machine.current_out = 1;
  // 前の問い合わせが選択点を残していれば、捨てる前に setup_call_cleanup/3 の後始末を動かす。その例外は返す先が無い。
  if (!halted && has_cleanup_entries(machine, 0)) {
    const Stop closed = close_query(machine);
    if (closed.kind == StopKind::Halt) {
      drop_run(machine);
      return closed;
    }
  }
  drop_run(machine);
  // 空きを生まない GC の連続は 1 つの問い合わせの中だけで数える（ADR 0010 の改訂 B）。
  reset_gc_judgement(machine);
  machine.diagnostic.clear();
  return drop_on_throw(machine, start);
}

}  // namespace

// conv は字面を読むときの char_conversion。利用者の字面は今の変換で、Mosa が書いた字面は変換なしで読む。
Stop Machine::query(std::string_view goal_text) {
  Impl& machine = *impl_;
  return open_query(machine, [&machine, goal_text] { return start_query(machine, goal_text, &machine.char_conv); });
}

// 書き手は char_conversion を知らずに書く。読み直しで変換すると、写される字を含むゴールが別の項になる（レビュー L17
// の Codex レビュー 8 回目。a を b、b を c に写すと X = a が X = c になっていた）。
Stop query_written(Machine& machine, std::string_view goal_text) {
  Impl& impl = engine_impl(machine);
  return open_query(impl, [&impl, goal_text] { return start_query(impl, goal_text, nullptr); });
}

// 字面を経ないので、演算子の表、旗、char_conversion が後で変わってもゴールは写したときの項のまま（レビュー L23）。
Stop query_exported(Machine& machine, const ExportedTerm& goal) {
  Impl& impl = engine_impl(machine);
  return open_query(impl, [&impl, &goal] { return start_goal(impl, import_term(impl, goal)); });
}

// 束縛は取り込んだ直後、走り出す前に根（machine.bindings）へ入れる。GC が途中で動いても、名前の変数は新しい番地に
// 付いていく。片付け（drop_run）は束縛を消すので、open_query の中で足す。
Stop query_exported(Machine& machine, const ExportedTerm& goal, std::span<const ExportedName> names) {
  Impl& impl = engine_impl(machine);
  return open_query(impl, [&impl, &goal, names] {
    const Cell term = import_term(impl, goal, names, impl.bindings);
    return start_goal(impl, term);
  });
}

Stop Machine::next() {
  Impl& machine = *impl_;
  if (machine.halt_now) return Stop{StopKind::Halt, machine.halt_code, {}, {}, false};
  return drop_on_throw(machine, [&machine] {
    if (!backtrack(machine)) return Stop{StopKind::Failure, 0, {}, {}, false};
    return run_loop(machine);
  });
}

}  // namespace mosa
