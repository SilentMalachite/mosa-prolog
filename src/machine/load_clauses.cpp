// 読み込みが節と宣言を述語の表へ書く所。失敗した読み込みの巻き戻しも同じ表を書くので、ここに置く。

#include "load_internal.hpp"

#include "mosa/writer.hpp"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace mosa {
namespace {

bool same_pred(const Predicate& predicate, std::uint32_t module, std::uint32_t name, std::uint16_t arity) {
  return predicate.module == module && predicate.name == name && predicate.arity == arity;
}

std::uint32_t head_name(const Clause& clause) { return clause.head.atom; }

std::uint16_t head_arity(const Clause& clause) {
  if (clause.head.tag == IrTag::Atom) return 0;
  return clause.head.arity;
}

bool keeps_clauses(const Impl& machine, std::uint32_t module, std::uint32_t name, std::uint16_t arity) {
  for (const SharedPred& item : machine.declared) {
    if (item.shared && item.module == module && item.name == name && item.arity == arity) return true;
  }
  return false;
}

}  // namespace

void fail_load(Impl& machine, std::string message) {
  machine.diagnostic = std::move(message);
  machine.syntax_diag = false;
}

bool allows_gap(const std::vector<Apart>& apart, PredKey key) {
  for (const Apart& item : apart) {
    if (item.module == key.module && item.name == key.name && item.arity == key.arity) return true;
  }
  return false;
}

void note_declared(Impl& machine, PredKey key, bool shared) {
  if (shared ? keeps_clauses(machine, key.module, key.name, key.arity)
             : declared_only(machine, key.module, key.name, key.arity)) {
    return;
  }
  machine.declared.push_back(SharedPred{key.module, key.name, key.arity, shared});
}

bool declared_only(const Impl& machine, std::uint32_t module, std::uint32_t name, std::uint16_t arity) {
  for (const SharedPred& item : machine.declared) {
    if (item.module == module && item.name == name && item.arity == arity) return true;
  }
  return false;
}

void remember_pred(Impl& machine, std::vector<Snapshot>& snapshots, PredKey key) {
  const bool known = std::any_of(snapshots.begin(), snapshots.end(), [&](const Snapshot& shot) {
    return shot.module == key.module && shot.name == key.name && shot.arity == key.arity;
  });
  if (known) return;
  Predicate* predicate = find_pred(machine, key);
  Snapshot shot;
  shot.module = key.module;
  shot.name = key.name;
  shot.arity = key.arity;
  shot.created = predicate == nullptr;
  if (predicate != nullptr) {
    shot.clauses = predicate->clauses;
    shot.dynamic = predicate->dynamic;
    shot.abolished = predicate->abolished;
    shot.meta = predicate->meta;
  }
  snapshots.push_back(std::move(shot));
}

// 節は module に入る。頭部に M: が無ければ読み込み中のモジュールで、module/2 の無いテキストでは user。
bool add_clause(Impl& machine, std::uint32_t module, Clause clause, std::size_t& last,
                const std::vector<Apart>& apart) {
  const auto name = head_name(clause);
  const auto arity = head_arity(clause);
  std::size_t index = machine.predicates.size();
  for (std::size_t cursor = 0; cursor < machine.predicates.size(); ++cursor) {
    if (same_pred(machine.predicates[cursor], module, name, arity)) index = cursor;
  }
  if (index == machine.predicates.size()) {
    // 取り込んだ名前と同じ局所の述語は作らない（iso-notes 第5節の取り込みの衝突）。
    if (const auto formal = check_local(machine, module, name, arity)) {
      fail_load(machine, write_term(machine.heap, machine.operators, *formal, {}, {}));
      return false;
    }
    machine.predicates.push_back(Predicate{});
    machine.predicates.back().module = module;
    machine.predicates.back().name = name;
    machine.predicates.back().arity = arity;
    adopt_meta(machine, machine.predicates.back());
  }
  Predicate& predicate = machine.predicates[index];
  // 宣言の無い述語は、これまでと同じ診断のまま失敗する。
  if (predicate.seen && last != index && !allows_gap(apart, PredKey{module, name, arity})) {
    const std::string indicator = std::string(machine.atoms.name(name)) + "/" + std::to_string(arity);
    fail_load(machine, message_text(machine, "clauses_apart", {indicator}));
    return false;
  }
  if (predicate.abolished) {
    // ADR 0008。abolish 済みの枠を新しい定義に使い直す。死んだ節は開いた走査のために残す。
    predicate.abolished = false;
    predicate.dynamic = predicate.dynamic_declared;
    if (!predicate.meta_declared) predicate.meta.clear();
    // 印を外してから片付けるので、述語ごと消えて index が外れることは無い。
    static_cast<void>(purge_dead_clauses(machine, &predicate));
  } else if (!predicate.seen && !predicate.clauses.empty()) {
    bool busy = false;
    for (const Entry& entry : machine.stack) {
      if (entry.pick != Pick::Cleanup && entry.pick != Pick::Catch) busy = true;
    }
    if (busy) {
      fail_load(machine, message_text(machine, "predicate_running"));
      return false;
    }
    // 宣言が残っている述語は足す。無い述語は、これまでどおり置き換える。
    if (!keeps_clauses(machine, module, name, arity)) {
      predicate.clauses.clear();
      // 宣言が無い読み込みは、以前動的でも置き換えた述語を静的に戻す。
      if (!predicate.dynamic_declared) predicate.dynamic = false;
      // meta_predicate も同じ。読み直したテキストが宣言を消したら、限定もやめる。
      if (!predicate.meta_declared) predicate.meta.clear();
    }
  }
  predicate.seen = true;
  last = index;
  predicate.clauses.push_back(std::move(clause));
  return true;
}

void remember_clause(Impl& machine, std::vector<Snapshot>& snapshots, std::uint32_t module, const Clause& clause) {
  remember_pred(machine, snapshots, PredKey{module, head_name(clause), head_arity(clause)});
}

void rollback_preds(Impl& machine, const std::vector<Snapshot>& snapshots, std::size_t previous_size) {
  for (const Snapshot& shot : snapshots) {
    if (shot.created) continue;
    Predicate* predicate = find_pred(machine, PredKey{shot.module, shot.name, shot.arity});
    if (predicate != nullptr) {
      predicate->clauses = shot.clauses;
      predicate->dynamic = shot.dynamic;
      predicate->abolished = shot.abolished;
      predicate->meta = shot.meta;
    }
  }
  machine.predicates.resize(previous_size);
}

}
