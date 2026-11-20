// ADR 0015 とその改訂、レビュー M19。消した節の寿命。開いた走査（脇に置いたスタックの走査も）がまだ試す節は死んだ
// 節で残し、届かなくなった節を端から、または半分を超えたらまとめて消す。assert・retract・abolish（database.cpp）と
// 読み込みの置き換え（load_clauses.cpp）がここを呼ぶ。
// テスト: tests/pl/database.pl, tests/pl/directives.pl, tests/cpp/clause_scan_test.cpp

#include "database_internal.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

namespace mosa {

using database::erase_predicate;
using database::kill_clause;
using database::purge_dead;

namespace {

// cursor が節の番号である選択点だけ。述語の番号（PredGen）や状態（Native）を節の番号と比べない。
bool scans_clauses(Pick pick) {
  return pick == Pick::Clauses || pick == Pick::Retract || pick == Pick::ClauseScan;
}

// 同じ名前の述語はモジュールごとにある。番号はどの述語も同じ所から振るので、名前だけで比べると別のモジュールの
// 走査の候補を自分の節と取り違える。
bool scans_pred(const Entry& entry, std::uint32_t module, std::uint32_t name, std::uint16_t arity) {
  return scans_clauses(entry.pick) && entry.pred_module == module && entry.pred_name == name &&
         entry.pred_arity == arity;
}

// entry が、消した節 clause をまだ試しうるか（ADR 0015 とその改訂）。ほかの述語の走査と、節を足す前か消した後に
// 始めた走査はこの節を見ない。通り過ぎた節（試し終えた節と今動いている節）にはもう戻らず、走査の鍵に合わない節は
// 試さない。retract は別の retract が消した節を飛ばすので、開いた retract はどの消した節も試さない。番号は変わらない。
// clause/2 のカーソルより先の合わない節も残すが、述語の掃除を遅らせるだけで答えは変わらない。
bool scan_reaches(const Entry& entry, PredKey key, const Clause& clause) {
  if (!scans_pred(entry, key.module, key.name, key.arity) || entry.pick == Pick::Retract) return false;
  if (!visible(clause, entry.generation)) return false;
  return entry.cursor != kNoClause && entry.cursor <= clause.id && key_selects(clause, entry.key);
}

// 記録した節の走査を、脇に置いたスタック（Impl::parked）の古い順から今のスタックまで渡す。visit が偽を返したら
// やめる。脇に置いたスタックは指示が終わるまで動かせないので、外の走査も開いた走査である（レビュー M19）。渡す
// 位置は、脇に置いたスタックを今のスタックの下に重ねた通しの位置。脇に置いたスタックは戻すまで伸び縮みせず、
// 戻すと一番上のものが今のスタックになるので、通しの位置は戻した後も同じ枠を指す。脇に置いた記録は外した枠を
// 指すことがあるので、位置と effect_id で確かめて飛ばす。
template <typename Visit>
void each_open_scan(Impl& machine, Visit visit) {
  std::size_t base = 0;
  const auto walk = [&base, &visit](const std::vector<Entry>& stack, const std::vector<ScanRecord>& log) {
    for (const ScanRecord& record : log) {
      if (record.index >= stack.size() || stack[record.index].effect_id != record.id) continue;
      if (!visit(stack[record.index], ScanRecord{base + record.index, record.id})) return false;
    }
    base += stack.size();
    return true;
  };
  for (const ParkedScans& parked : machine.parked) {
    if (!walk(*parked.stack, *parked.scan_log)) return;
  }
  prune_scan_log(machine);
  walk(machine.stack, machine.scan_log);
}

// 開いた走査のどれかがこの節をまだ試しうるか。届かない節は消してよい。見るのは記録した節の走査だけで、retract の
// 選択点や制御の枠がいくつ積もっても手間は増えない。
bool reachable(Impl& machine, PredKey key, const Clause& clause) {
  bool found = false;
  each_open_scan(machine, [&found, key, &clause](const Entry& entry, ScanRecord) {
    found = scan_reaches(entry, key, clause);
    return !found;
  });
  return found;
}

// 消した世代を付ける。消した節に付け直すと、その間に始めた走査の見え方が変わる。飛び越しの幅は節を作ったときの 1 の
// ままで、自分だけの範囲を指す（ADR 0015 の改訂）。
bool mark_dead(Impl& machine, Predicate& predicate, Clause& clause) {
  if (clause.dead()) return false;
  clause.died = ++machine.db_generation;
  predicate.last_death = clause.died;
  predicate.live -= 1;
  return true;
}

// 開いた走査のために残した端の節は、その走査が閉じた後の最初の変更で消す。消すと隣が端になるので続けて見る。
// 端の節が生きていれば走査を見ないので、変更のたびの手間は O(1)。
void trim_ends(Impl& machine, Predicate& predicate, PredKey key) {
  const ClauseList& clauses = predicate.clauses;
  while (!clauses.empty() && clauses.front().dead() && !reachable(machine, key, clauses.front())) {
    erase_clause_id(predicate, clauses.front().id);
  }
  while (!clauses.empty() && clauses.back().dead() && !reachable(machine, key, clauses.back())) {
    erase_clause_id(predicate, clauses.back().id);
  }
}

// 前の掃除が残した節に届いた一番下の枠が、もうスタックに無いか。スタックは上から外すので、上の枠も無い。
// 位置は each_open_scan の通しの位置で、脇に置いたスタックの枠でもありうる（レビュー M19）。
bool holder_gone(const Impl& machine, const Predicate& predicate) {
  const ScanRecord& holder = predicate.held_by;
  std::size_t base = 0;
  for (const ParkedScans& parked : machine.parked) {
    const std::vector<Entry>& stack = *parked.stack;
    if (holder.index < base + stack.size()) return stack[holder.index - base].effect_id != holder.id;
    base += stack.size();
  }
  const std::size_t index = holder.index - base;
  return index >= machine.stack.size() || machine.stack[index].effect_id != holder.id;
}

// 残した節に届く枠がまだあるときにそれも数えると、変更のたびに掃除して残すだけになり 2 乗になる。そのときは
// 掃除の後に消した節だけで決め、枠が外れたら残した節も数える。どちらも掃除 1 回を半分の節で払う。
bool sweep_due(const Impl& machine, const Predicate& predicate) {
  if (predicate.abolished) return true;
  const std::size_t counted =
      predicate.held_dead == 0 || holder_gone(machine, predicate) ? predicate.dead_since
                                                                  : predicate.dead_since - predicate.held_dead;
  return 2 * counted > predicate.clauses.size();
}

// この読み込みのテキストが古い述語へした meta_predicate の宣言は、述語を消しても残す。宣言の無い述語への宣言と
// 同じく保留の表へ移し、後で作る定義が引き取る。消すと、初期化のゴールが assertz で作り直した述語が限定を失う
// （L11 の 4 度目の Codex レビュー P2）。
void pend_meta(Impl& machine, Predicate& predicate) {
  Module* module = find_module(machine, predicate.module);
  if (module == nullptr) return;
  std::erase_if(module->pending_meta, [&predicate](const PendingMeta& item) {
    return item.name == predicate.name && item.arity == predicate.arity;
  });
  module->pending_meta.push_back(PendingMeta{predicate.name, predicate.arity, std::move(predicate.meta)});
  predicate.meta.clear();
  predicate.meta_declared = false;
}

}  // namespace

void database::erase_predicate(Impl& machine, std::uint32_t module, std::uint32_t name, std::uint16_t arity) {
  // 述語を作り直すと番号は同じ所から振り直す。開いた走査を尽きた印にしておけば、新しい定義の節を指さない。
  // 指示が脇に置いた外の走査も同じにする（レビュー M19）。current_predicate の候補は Predicate::serial なので、
  // 表から消してもずらさない。
  const auto exhaust = [module, name, arity](std::vector<Entry>& stack) {
    for (Entry& entry : stack) {
      if (!scans_pred(entry, module, name, arity)) continue;
      entry.cursor = kNoClause;
    }
  };
  for (const ParkedScans& parked : machine.parked) exhaust(*parked.stack);
  exhaust(machine.stack);
  machine.predicates.erase(PredKey{module, name, arity});
}

// 先頭か末尾の節は、届く走査が無ければすぐ消す。端の節を消しても索引の組は端を外すだけで、ほかの節は動かない。
// 真ん中の節と届く節は残して数え、前の掃除から数えて半分を超えたら、次の変更がまとめて掃除する（purge_dead）。
// 真ん中を一つずつ詰めると 1 回が節の数だけかかり、索引も作り直しになる。開いた走査は世代で見分けて試す。
void database::kill_clause(Impl& machine, Predicate& predicate, PredKey key, std::size_t id) {
  const auto position = clause_position(predicate, id);
  if (!position) return;
  Clause& clause = predicate.clauses[*position];
  if (!mark_dead(machine, predicate, clause)) return;
  predicate.goal_changed_at = machine.db_generation;
  const bool end = *position == 0 || *position + 1 == predicate.clauses.size();
  if (end && !reachable(machine, key, clause)) {
    erase_clause_id(predicate, id);
    return;
  }
  ++predicate.dead_since;
}

// 変更の前に呼ぶ。端の死んだ節を消し、前の掃除から残した節が半分を超えたか abolish 済みなら一度に掃除する。
// 掃除は節の数に比例するが、半分を超える数の削除に 1 回なので、ならせば削除 1 回あたり O(1)。掃除は端の節も
// 消すので、掃除するときは端を一つずつ確かめない。節が尽きた abolish 済みの述語は表からも消す。
Predicate* database::purge_dead(Impl& machine, Predicate* predicate) {
  if (predicate == nullptr) return nullptr;
  const PredKey key{predicate->module, predicate->name, predicate->arity};
  if (!sweep_due(machine, *predicate)) trim_ends(machine, *predicate, key);
  if (sweep_due(machine, *predicate)) sweep_dead(machine, *predicate);
  if (!predicate->abolished || !predicate->clauses.empty()) return predicate;
  erase_predicate(machine, key.module, key.name, key.arity);
  return nullptr;
}

// abolish/1 と同じく、開いた走査があれば枠を abolish 済みにして残し、無ければ表から消す。1 つずつ表から消すと、
// 後ろの述語と索引を毎回ずらし、消す数と表の長さの積になる（L11 の Codex レビュー P2）。静的な述語は生成関数を
// 付けていることがあるので外す。外さないと、ほかの単位の呼び出しが消した述語の関数を呼び続ける（同 P2）。
void abolish_stale(Impl& machine, const std::vector<PredKey>& keys, const std::vector<PredKey>& metas) {
  const auto less = [](const PredKey& a, const PredKey& b) {
    return std::tie(a.module, a.name, a.arity) < std::tie(b.module, b.name, b.arity);
  };
  std::vector<PredKey> erased;
  for (const PredKey key : keys) {
    Predicate* predicate = find_pred(machine, key);
    if (predicate == nullptr) continue;
    detach_native(machine, *predicate);
    if (std::binary_search(metas.begin(), metas.end(), key, less)) pend_meta(machine, *predicate);
    retire_clauses(machine, *predicate);
    if (predicate->clauses.empty()) {
      erased.push_back(key);
      continue;
    }
    // 枠を残しても定義は無くなる。失敗した読み込みが、abolish の前の写しをこの枠へ戻さないようにする（ADR 0017）。
    // abolish/1 の枠と同じく動的にする。静的のままだと、開いた走査がある間の assertz と dynamic の宣言が、消した
    // 述語の新しい定義を静的な述語への変更として断る（L11 の 2 度目の Codex レビュー P2）。
    begin_definition(machine, *predicate, true);
    predicate->abolished = true;
    predicate->dynamic = true;
  }
  if (erased.empty()) return;
  std::sort(erased.begin(), erased.end(), less);
  const auto gone = [&erased, &less](PredKey key) { return std::binary_search(erased.begin(), erased.end(), key, less); };
  // erase_predicate と同じく、開いた走査を尽きた印にする。作り直した述語の節を指さない。
  const auto exhaust = [&gone](std::vector<Entry>& stack) {
    for (Entry& entry : stack) {
      if (scans_clauses(entry.pick) && gone(PredKey{entry.pred_module, entry.pred_name, entry.pred_arity})) {
        entry.cursor = kNoClause;
      }
    }
  };
  for (const ParkedScans& parked : machine.parked) exhaust(*parked.stack);
  exhaust(machine.stack);
  machine.predicates.erase_if([&gone](const Predicate& predicate) { return gone(key_of(predicate)); });
}

// retire_clauses と同じく、全部に消した世代を付けてから一度に掃除する。開いた走査がまだ試す節は死んだ節で残る。
void retire_older(Impl& machine, const std::vector<PredKey>& keys, std::uint64_t generation) {
  for (const PredKey key : keys) {
    Predicate* predicate = find_pred(machine, key);
    if (predicate == nullptr) continue;
    detach_native(machine, *predicate);
    for (Clause& clause : predicate->clauses) {
      if (clause.born <= generation) mark_dead(machine, *predicate, clause);
    }
    sweep_dead(machine, *predicate);
  }
}

// この述語の開いた走査を記録から一度だけ集め、死んだ節ごとにはその分だけを見る。節ごとに reachable で記録を
// なめると、ほかの述語の走査の数まで節の数に掛かる。読み込みが abolish 済みの枠を使い直すときにも呼ぶ（ADR 0008）。
void sweep_dead(Impl& machine, Predicate& predicate) {
  const PredKey key{predicate.module, predicate.name, predicate.arity};
  // 枠と通しの位置。掃除は枠を積みも外しもしないので、枠を指したままでよい。
  std::vector<std::pair<const Entry*, ScanRecord>> scans;
  each_open_scan(machine, [&scans, key](const Entry& entry, ScanRecord at) {
    if (scans_pred(entry, key.module, key.name, key.arity)) scans.emplace_back(&entry, at);
    return true;
  });
  // 下の枠から試すと、節ごとに最初に届いた枠がその節に届く一番下の枠になる。
  std::sort(scans.begin(), scans.end(), [](const auto& a, const auto& b) { return a.second.index < b.second.index; });
  std::vector<bool> held(predicate.clauses.size(), false);
  std::optional<ScanRecord> holder;
  for (std::size_t position = 0; position < held.size(); ++position) {
    const Clause& clause = predicate.clauses[position];
    if (!clause.dead()) continue;
    const auto first = std::find_if(scans.begin(), scans.end(), [&](const auto& scan) {
      return scan_reaches(*scan.first, key, clause);
    });
    if (first == scans.end()) continue;
    held[position] = true;
    if (!holder || first->second.index < holder->index) holder = first->second;
  }
  sweep_clauses(predicate, held, holder.value_or(ScanRecord{}));
}

// 静的な述語は assert も retract も abolish もされないので、変更の前の掃除（purge_dead）が走らない。読み込みの
// 置き換えが開いた走査のために残した死んだ節は、走査が閉じた後の呼び出しが消す（M19 の敵対的レビュー B2）。
// 消さないと、束縛しない呼び出しが毎回先頭の死んだ節を歩き、次に読み直すまで残る。
void purge_static(Impl& machine, Predicate& predicate) {
  static_cast<void>(purge_dead(machine, &predicate));
  // 端から外した節は dead_since から引かない。死んだ節が残っていなければ 0 に戻し、次の呼び出しからは来ない。
  if (live_count(predicate) == predicate.clauses.size()) {
    predicate.dead_since = 0;
    predicate.held_dead = 0;
  }
}

// 節ごとに端を確かめて消すと、節ごとにスタックをなめる。全部に消した世代を付けてから一度に掃除する。
void retire_clauses(Impl& machine, Predicate& predicate) {
  for (Clause& clause : predicate.clauses) mark_dead(machine, predicate, clause);
  sweep_dead(machine, predicate);
}

}  // namespace mosa
