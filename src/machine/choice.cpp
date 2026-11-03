#include "internal.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace mosa {
namespace {

const std::uint64_t* watermark(const Impl& machine) {
  if (machine.stack.empty()) return machine.heap.data();
  return machine.heap.data() + machine.stack.back().heap_mark;
}

// 水位は節に入る前。世代は呼んだ時点のもので、走査はそれより後に足した節と消した節を見ない（7.5.4）。
Entry clause_entry(const Impl& machine, const Predicate& predicate, std::uint64_t goal, std::size_t boundary) {
  Entry entry;
  entry.pick = Pick::Clauses;
  entry.redo_goal = goal;
  // 別名で呼んでも、やり直しは定義の名前で引き直す。
  entry.pred_module = predicate.module;
  entry.pred_name = predicate.name;
  entry.pred_arity = predicate.arity;
  entry.cut = boundary;
  entry.heap_mark = machine.heap.size();
  entry.trail_mark = machine.trail.slots.size();
  entry.generation = machine.db_generation;
  return entry;
}

// 第1引数で節を選べない呼び出しは生きた節が全部候補なので、列を作らずに次の節の番号だけを持つ（ADR 0015）。
// 次に見える節が無ければ積まない。列の長さが 1 のとき積まなかったのと同じで、決定的な答えは選択点を残さない。
void call_by_cursor(Impl& machine, const Predicate& predicate, std::uint64_t goal) {
  const auto generation = machine.db_generation;
  const std::size_t position = next_visible(predicate, 0, generation);
  if (position >= predicate.clauses.size()) {
    machine.failed = true;
    return;
  }
  const std::size_t after = next_visible(predicate, position + 1, generation);
  const auto boundary = machine.stack.size();
  if (after < predicate.clauses.size()) {
    Entry entry = clause_entry(machine, predicate, goal, boundary);
    entry.by_cursor = true;
    entry.cursor = predicate.clauses[after].id;
    push_frame(machine, std::move(entry));
  }
  enter_clause(machine, predicate, position, goal, boundary);
}

// 候補は今見える節の番号だけなので、最初の番号の節は在る。位置は真ん中から消していなければ差の 1 回で引ける。
void call_by_list(Impl& machine, const Predicate& predicate, Cell first, std::uint64_t goal) {
  auto candidates = key_candidates(machine, predicate, first, machine.db_generation);
  const auto position = candidates.empty() ? std::nullopt : clause_position(predicate, candidates.front());
  if (!position) {
    machine.failed = true;
    return;
  }
  const auto boundary = machine.stack.size();
  if (candidates.size() > 1) {
    Entry entry = clause_entry(machine, predicate, goal, boundary);
    entry.candidates = std::move(candidates);
    entry.next = 1;
    push_frame(machine, std::move(entry));
  }
  enter_clause(machine, predicate, *position, goal, boundary);
}

bool exhausted(const Entry& entry) {
  return entry.by_cursor ? entry.cursor == kNoClause : entry.next >= entry.candidates.size();
}

// カーソルの節はまだこの走査が届くので消さず、普通は在る。読み込みの戻しなどで無くなっていれば、その番号より
// 先で見える節から続ける。次に見える節を先に探しておき、無ければ尽きた印にする。
std::optional<std::size_t> advance_cursor(const Predicate& predicate, Entry& entry) {
  const std::size_t position = next_visible(predicate, clause_from(predicate, entry.cursor), entry.generation);
  if (position >= predicate.clauses.size()) {
    entry.cursor = kNoClause;
    return std::nullopt;
  }
  const std::size_t after = next_visible(predicate, position + 1, entry.generation);
  entry.cursor = after < predicate.clauses.size() ? predicate.clauses[after].id : kNoClause;
  return position;
}

// 候補は番号なので、前の節を消しても asserta しても位置を引き直せば同じ節に入る（ADR 0015）。まだ試して
// いない節は消さずに残すので普通は在る。無い番号が残っていれば飛ばす。
std::optional<std::size_t> advance_list(const Predicate& predicate, Entry& entry) {
  std::optional<std::size_t> position;
  while (!position && entry.next < entry.candidates.size()) {
    position = clause_position(predicate, entry.candidates[entry.next]);
    entry.next += 1;
  }
  return position;
}

// 積む位置とそれより上を指す記録は外した枠のものなので、足す前に捨てる。記録は位置の昇順に並び、枠の数より
// 増えない。
void note_scan(Impl& machine, const Entry& entry) {
  std::vector<ScanRecord>& log = machine.scan_log;
  while (!log.empty() && log.back().index >= machine.stack.size()) log.pop_back();
  log.push_back(ScanRecord{machine.stack.size(), entry.effect_id});
}

}  // namespace

void restore_state(Impl& machine, std::size_t heap_mark, std::size_t trail_mark, std::size_t extras_mark) {
  while (machine.trail.slots.size() > trail_mark) {
    auto* slot = machine.trail.slots.back();
    machine.trail.slots.pop_back();
    *slot = std::bit_cast<std::uint64_t>(slot);
  }
  machine.heap.truncate(heap_mark);
  shrink_extras(machine, extras_mark);
}

void shrink_extras(Impl& machine, std::size_t mark) {
  if (machine.extras.size() <= mark) return;
  machine.extras.resize(mark);
}

void note_catches(const Impl& machine, Entry& entry) { entry.catch_mark = machine.catch_log.size(); }

// entry が積まれた後に無効にした Catch を、新しい順に有効に戻す。外した枠や、入れ子の実行（run_goal、指令）
// の中の枠を指す記録は、位置か effect_id が合わないので飛ばす。
void apply_catches(Impl& machine, const Entry& entry) {
  while (machine.catch_log.size() > entry.catch_mark) {
    const CatchRecord record = machine.catch_log.back();
    machine.catch_log.pop_back();
    if (record.index >= machine.stack.size()) continue;
    Entry& target = machine.stack[record.index];
    if (target.pick == Pick::Catch && target.effect_id == record.id) target.armed = true;
  }
}

void prune_catch_log(Impl& machine) {
  while (!machine.catch_log.empty() && machine.catch_log.back().index >= machine.stack.size()) {
    machine.catch_log.pop_back();
  }
}

void prune_scan_log(Impl& machine) {
  std::erase_if(machine.scan_log, [&machine](const ScanRecord& record) {
    return record.index >= machine.stack.size() || machine.stack[record.index].effect_id != record.id;
  });
}

void push_frame(Impl& machine, Entry entry) {
  entry.extras_mark = machine.extras.size();
  // query の Cleanup はここを通さないので印は 0。catch_log と scan_log が、記録した枠がまだ同じ枠かをこれで確かめる。
  entry.effect_id = ++machine.effect_clock;
  note_catches(machine, entry);
  if (entry.pick == Pick::Clauses || entry.pick == Pick::ClauseScan) note_scan(machine, entry);
  machine.stack.push_back(std::move(entry));
  count_vectors(machine);
}

bool do_unify(Impl& machine, Cell left, Cell right) {
  machine.trail.mark = watermark(machine);
  return unify(machine.heap, machine.trail, machine.trail.slots.size(), machine.heap.size(), left, right);
}

bool unify_visible(Impl& machine, std::size_t floor, Cell left, Cell right) {
  const auto* saved = machine.trail.mark;
  // 節の中の匿名は Cleanup より新しい。選択点の水位だと、最初の候補の束縛が残る。
  machine.trail.mark = machine.heap.data() + floor;
  const bool ok = unify(machine.heap, machine.trail, machine.trail.slots.size(), machine.heap.size(), left, right);
  machine.trail.mark = saved;
  return ok;
}

void enter_clause(Impl& machine, const Predicate& predicate, std::size_t which, std::uint64_t goal, std::size_t cut) {
  const Clause& clause = predicate.clauses[which];
  // meta 引数の限定は呼び手の文脈。枠は書き換えず、節に入るたびに作る。やり直しで枠より新しい項を指さないため。
  const auto context = predicate.meta.empty() ? 0 : read_goal(machine, goal).module;
  std::vector<Cell> cells;
  cells.reserve(clause.vars);
  for (std::uint16_t index = 0; index < clause.vars; ++index) cells.push_back(machine.heap.fresh_ref());
  if (clause.head.tag == IrTag::Fun) {
    for (std::size_t index = 0; index < clause.head.args.size(); ++index) {
      const Cell expected = materialize(machine, clause.head.args[index], cells);
      Cell actual = Cell{machine.heap.word_at(goal + kFrameHead + index)};
      if (!predicate.meta.empty()) actual = meta_argument(machine, predicate, context, index, actual);
      if (!do_unify(machine, expected, actual)) {
        machine.failed = true;
        return;
      }
    }
  }
  const auto cont = machine.heap.word_at(goal + 3);
  // 本体のゴールは節の述語のモジュールで動く（ADR 0006）。呼び手の文脈は受け継がない。
  // 事実と 1 ゴールの本体は env を残さない。共有の env を作る確保を、よく通るこの道から省く。
  const IrTag tag = clause.body->tag;
  if (tag == IrTag::True) {
    machine.ip = cont;
    return;
  }
  if (is_call_leaf(tag)) {
    machine.ip = embody_leaf(machine, *clause.body, cont, cut, cells, predicate.module);
    return;
  }
  launch(machine, clause.body, cont, cut, make_env(machine.heap, std::move(cells)), predicate.module);
}

void enter_user(Impl& machine, const Goal& goal) {
  Predicate* predicate = resolve_pred(machine, goal.module, goal.name, goal.arity);
  if (predicate != nullptr && predicate->abolished) predicate = nullptr;
  if (predicate == nullptr) {
    // 旗 unknown=fail は、無い述語を失敗にする。error のときは従来の存在エラー。
    // multifile と discontiguous の宣言だけの述語は、節の無い手続きとして失敗する（iso-notes 7.8.3 の決定 B）。
    if (machine.unknown_fail || declared_only(machine, goal.module, goal.name, goal.arity)) {
      machine.failed = true;
      return;
    }
    Cell pi = cell_functor(machine, goal.name, goal.arity);
    // user の外では、どこで引けなかったかを示す（iso-notes 第5節の決定 A）。
    if (goal.module != machine.user_module) {
      pi = machine.heap.structure(machine.atoms.intern(":"), std::array<Cell, 2>{machine.heap.atom(goal.module), pi});
    }
    raise_formal(machine, formal_existence(machine, "procedure", pi), goal.name, goal.arity);
    return;
  }
  // 取り込みで引いた呼び出しも、付いていれば生成関数で動く。call_goal の近道は同じモジュールの呼び先だけ。
  if (predicate->native != nullptr) {
    call_native(machine, predicate->native, goal.at);
    return;
  }
  if (predicate->clauses.empty()) {
    // 節を消し切っても動的述語は残る。呼び出しは失敗であり、存在エラーではない。
    machine.failed = true;
    return;
  }
  const bool has_arg = goal.arity >= 1 && !wraps_first(*predicate);
  const Cell first = has_arg ? machine.heap.deref(goal.arg(machine, 0)) : Cell{};
  if (!has_arg || machine.heap.unbound(first)) call_by_cursor(machine, *predicate, goal.at);
  else call_by_list(machine, *predicate, first, goal.at);
}

bool redo_clause(Impl& machine) {
  // 候補の列は写さずに読む。一歩ごとに写すと、節を尽くすまでが候補の数の 2 乗になる（レビュー M3）。
  // apply_catches と find_pred はスタックの長さを変えないので、外すか進めるまで参照は有効。
  Entry& top = machine.stack.back();
  if (exhausted(top)) {
    machine.stack.pop_back();
    return false;
  }
  apply_catches(machine, top);
  const Predicate* predicate = find_pred(machine, PredKey{top.pred_module, top.pred_name, top.pred_arity});
  // 述語ごと消えると番号を引く先が無い。枠を残すとやり直しが終わらない。
  if (predicate == nullptr) {
    machine.stack.pop_back();
    return false;
  }
  const auto position = top.by_cursor ? advance_cursor(*predicate, top) : advance_list(*predicate, top);
  const auto goal = top.redo_goal;
  const auto cut = top.cut;
  // 最後の節に入るときは外す。呼び出しで 1 節なら積まなかったのと同じく、最後の答えは選択点を残さない。
  if (exhausted(top)) machine.stack.pop_back();
  if (!position) return false;
  enter_clause(machine, *predicate, *position, goal, cut);
  return true;
}

bool redo_else(Impl& machine) {
  const Entry top = machine.stack.back();
  if (top.soft_id != 0 && top.cond_ok) {
    machine.stack.pop_back();
    return false;
  }
  apply_catches(machine, top);
  GoalRef goal = top.else_goal;
  const EnvRef env = top.env;
  const auto cont = top.cont;
  const auto cut = top.cut;
  const auto module = top.module;
  machine.stack.pop_back();
  launch(machine, std::move(goal), cont, cut, env, module);
  return true;
}

bool redo_not(Impl& machine) {
  const Entry top = machine.stack.back();
  apply_catches(machine, top);
  const auto cont = top.cont;
  machine.stack.pop_back();
  machine.ip = cont;
  return true;
}

bool redo_repeat(Impl& machine) {
  apply_catches(machine, machine.stack.back());
  machine.ip = machine.stack.back().cont;
  return true;
}

bool backtrack(Impl& machine) {
  // 生成関数の呼び先は、次の step が消費しなければ失敗で無効になる。
  machine.native_next = nullptr;
  machine.failed = false;
  machine.exception = false;
  while (!machine.stack.empty()) {
    // 枠ごと写すと、候補の列まで一歩ごとに写る（レビュー M3）。redo_* は枠を外し、finish_collect は
    // スタックを変えうるので、要る値は呼ぶ前に手元へ移し、その後は参照に触れない。
    const Entry& top = machine.stack.back();
    const Pick pick = top.pick;
    const auto heap_mark = top.heap_mark;
    const auto trail_mark = top.trail_mark;
    const auto extras_mark = top.extras_mark;
    restore_state(machine, heap_mark, trail_mark, extras_mark);
    // 入れ子の run_loop（portray、指令など）の床。束縛は消すが、呼び出し元の節走査はやり直さない。
    if (pick == Pick::Barrier) return false;
    // 収集のゴールの解が尽きた。答えを束ねる launch か、bagof の空の失敗（failed）で返る（ADR 0013）。
    if (pick == Pick::Collect) {
      finish_collect(machine);
      return true;
    }
    if (pick == Pick::Catch || pick == Pick::Cleanup || pick == Pick::Once) {
      machine.stack.pop_back();
      continue;
    }
    // Goal の解が尽きた。Setup の直後まで戻した状態で後始末を動かし、その後でまた失敗する（ADR 0016）。
    if (pick == Pick::CallCleanup) {
      machine.pending_cleanups.push_back(PendingCleanup{top.pattern, top.module});
      machine.stack.pop_back();
      machine.failed = true;
      return true;
    }
    if (pick == Pick::Clauses && redo_clause(machine)) return true;
    if (pick == Pick::Native && redo_native(machine)) return true;
    if (pick == Pick::Retract && redo_retract(machine)) return true;
    // Clauses は節の本体を実行する。参照と列挙は項を返すだけで、本体は走らせない。
    if (pick == Pick::ClauseScan && redo_clause_scan(machine)) return true;
    if (pick == Pick::PredGen && redo_pred_gen(machine)) return true;
    if (pick == Pick::ElseGoal && redo_else(machine)) return true;
    if (pick == Pick::Negation && redo_not(machine)) return true;
    if (pick == Pick::Repeat && redo_repeat(machine)) return true;
    // アトムの候補は節ではない。やり直しを節の添字に載せると、別の述語を引いてしまう。
    if (pick == Pick::AtomGen && redo_atoms(machine)) return true;
    if (pick == Pick::StreamGen && redo_stream_prop(machine)) return true;
    if (pick != Pick::Clauses && pick != Pick::Native && pick != Pick::Retract && pick != Pick::ClauseScan &&
        pick != Pick::PredGen && pick != Pick::ElseGoal && pick != Pick::Negation && pick != Pick::Repeat &&
        pick != Pick::AtomGen && pick != Pick::StreamGen) {
      machine.stack.pop_back();
    }
  }
  return false;
}

}
