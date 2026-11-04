// 読み込みが節と宣言を述語の表へ書く所。失敗した読み込みの巻き戻しも同じ表を書くので、ここに置く。

#include "load_internal.hpp"

#include "mosa/writer.hpp"

#include <algorithm>
#include <optional>
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

// この読み込みで初めて見る述語に、前の読み込みの生きた節が残っている。宣言が無ければ置き換える。
bool take_over(Impl& machine, Predicate& predicate, PredKey key) {
  for (const Entry& entry : machine.stack) {
    if (entry.pick == Pick::Cleanup || entry.pick == Pick::Catch || entry.pick == Pick::CallCleanup) continue;
    fail_load(machine, message_text(machine, "predicate_running"));
    return false;
  }
  // 宣言が残っている述語は足す。無い述語は、これまでどおり置き換える。
  if (keeps_clauses(machine, key.module, key.name, key.arity)) return true;
  // 消すのは retract と同じく世代を付けてから。consult の指示と初期化のゴールの間は外の走査が脇に置かれていて
  // （Impl::parked）、それがまだ試す節は残る（レビュー M19）。今のスタックの選択点は上で断っている。
  retire_clauses(machine, predicate);
  // 宣言が無い読み込みは、以前動的でも置き換えた述語を静的に戻す。
  if (!predicate.dynamic_declared) predicate.dynamic = false;
  // meta_predicate も同じ。読み直したテキストが宣言を消したら、限定もやめる。
  if (!predicate.meta_declared) predicate.meta.clear();
  return true;
}

}  // namespace

std::optional<Cell> refused_clause(Impl& machine, Cell term, const Clause& clause) {
  // 7.4 と iso-notes 7.4 の決定 A。足すと本体に届かない隠れ節になり、current_predicate/1 だけが真になる。
  // 順は assertz/1 と同じ（8.9.1.3）。数は 7.6.2 でゴールに変えられず、変数と call(1) は呼ぶときの誤りなので断らない。
  term = machine.heap.deref(term);
  const bool rule = term.tag() == Tag::Str && machine.heap.functor_arity(term) == 2 &&
                    machine.atoms.name(machine.heap.functor_atom(term)) == ":-";
  if (rule && !goal_leaves_all(*clause.body, [](IrTag tag) { return tag != IrTag::Int && tag != IrTag::Float; })) {
    return formal_type(machine, "callable", machine.heap.deref(machine.heap.arg(term, 1)));
  }
  const auto name = head_name(clause);
  const auto arity = head_arity(clause);
  // :-/1 は指示、:-/2 は節の首。組込の表に無いが、assertz/1 と同じく静的として断る。
  const bool neck = machine.atoms.name(name) == ":-" && (arity == 1 || arity == 2);
  if (!neck && !is_builtin_procedure(machine, name, arity)) return std::nullopt;
  return formal_permission(machine, "modify", "static_procedure", cell_functor(machine, name, arity));
}

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

// 節は番号ごと写す。戻したとき、開いた走査の候補が同じ節を指す（ADR 0015）。
Snapshot::Snapshot(PredKey key, const Predicate* predicate)
    : module(key.module),
      name(key.name),
      arity(key.arity),
      clauses(predicate == nullptr ? ClauseList{} : predicate->clauses),
      created(predicate == nullptr),
      dynamic(predicate != nullptr && predicate->dynamic),
      abolished(predicate != nullptr && predicate->abolished),
      meta(predicate == nullptr ? std::vector<MetaArg>{} : predicate->meta) {}

void remember_pred(Impl& machine, std::vector<Snapshot>& snapshots, PredKey key) {
  const bool known = std::any_of(snapshots.begin(), snapshots.end(), [&](const Snapshot& shot) {
    return shot.module == key.module && shot.name == key.name && shot.arity == key.arity;
  });
  if (known) return;
  snapshots.emplace_back(key, find_pred(machine, key));
}

// 節は module に入る。頭部に M: が無ければ読み込み中のモジュールで、module/2 の無いテキストでは user。
bool add_clause(Impl& machine, std::uint32_t module, Clause clause, std::optional<PredKey>& last,
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
    Predicate created;
    created.module = module;
    created.name = name;
    created.arity = arity;
    adopt_meta(machine, add_predicate(machine, std::move(created)));
  }
  Predicate& predicate = machine.predicates[index];
  // 宣言の無い述語は、これまでと同じ診断のまま失敗する。
  // last は直前に節を足した述語の名前。印（seen）と同じく名前で持つ。添字で持つと、間の consult が述語を表から
  // 消したとき、同じ述語の続きの節を離れた節と取り違える（M19 で見つけた不具合）。
  const bool next_to = last && last->module == module && last->name == name && last->arity == arity;
  if (predicate.seen && !next_to && !allows_gap(apart, PredKey{module, name, arity})) {
    const std::string indicator = std::string(machine.atoms.name(name)) + "/" + std::to_string(arity);
    fail_load(machine, message_text(machine, "clauses_apart", {indicator}));
    return false;
  }
  if (predicate.abolished) {
    // ADR 0008。abolish 済みの枠を新しい定義に使い直す。死んだ節は開いた走査のために残す。
    predicate.abolished = false;
    predicate.dynamic = predicate.dynamic_declared;
    if (!predicate.meta_declared) predicate.meta.clear();
    // 開いた走査が届かない死んだ節を一度に消す。sweep_dead は述語を表から消さないので、index は外れない。
    sweep_dead(machine, predicate);
  } else if (!predicate.seen && live_count(predicate) > 0) {
    // 死んだ節だけの述語は空の述語と同じに扱う。死んだ節は開いた走査のためだけにあり、置き換えても消すものは無く、
    // 走査は後から足す節を見ない。分けると、選択点の確かめと dynamic を戻すかが、開いた走査がまだ節を持つかで
    // 変わる（M19 の敵対的レビュー B1）。abolish の後の dynamic が印を外した述語もここを通らない（Codex レビュー P2）。
    if (!take_over(machine, predicate, PredKey{module, name, arity})) return false;
  }
  // 置き換え、multifile の追加、abolish 済みの枠の使い直しのどれでも、付けた生成関数の知らない節になる。
  detach_native(machine, predicate);
  predicate.seen = true;
  last = PredKey{module, name, arity};
  // 読み込みの節も assert と同じく世代を進める。読み込みの中のゴールが開いた走査は、後から読んだ節を見ない。
  append_clause(predicate, std::move(clause), ++machine.db_generation);
  return true;
}

void remember_clause(Impl& machine, std::vector<Snapshot>& snapshots, std::uint32_t module, const Clause& clause) {
  remember_pred(machine, snapshots, PredKey{module, head_name(clause), head_arity(clause)});
}

void rollback_preds(Impl& machine, const std::vector<Snapshot>& snapshots, std::size_t previous_serial) {
  for (const Snapshot& shot : snapshots) {
    if (shot.created) continue;
    Predicate* predicate = find_pred(machine, PredKey{shot.module, shot.name, shot.arity});
    if (predicate != nullptr) {
      // 写しへ戻した節にも生成関数は付け直さない（ADR 0009）。外したら、その述語はインタプリタで動き続ける。
      detach_native(machine, *predicate);
      replace_clauses(*predicate, shot.clauses);
      predicate->dynamic = shot.dynamic;
      predicate->abolished = shot.abolished;
      predicate->meta = shot.meta;
    }
  }
  // 読み込みの間に作った述語を消す。表の長さへ縮めると、指示が述語を消していたとき、作った述語が残るか、空の
  // 述語が末尾に足されて serial の昇順が崩れる（M18 の Codex レビュー P2）。
  std::erase_if(machine.predicates, [previous_serial](const Predicate& predicate) {
    return predicate.serial > previous_serial;
  });
}

}
