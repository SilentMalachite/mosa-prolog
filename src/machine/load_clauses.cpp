// 読み込みが節と宣言を述語の表へ書く所。失敗した読み込みの巻き戻しも同じ表を書くので、ここに置く。

#include "load_internal.hpp"

#include "mosa/writer.hpp"

#include <algorithm>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace mosa {
namespace {

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
  if (rule && !goal_leaves_all(*clause.body, [](IrTag tag) { return !is_number_ir(tag); })) {
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
  // 宣言し直したことは、表に既にあっても書き留める。読み直しが消す述語の宣言を残すかを決める（ADR 0020）。
  if (machine.load_snapshots != nullptr) machine.load_snapshots->notes().declared.push_back(key);
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
Snapshot::Snapshot(PredKey key, const Predicate* predicate, std::uint64_t taken_at)
    : module(key.module),
      name(key.name),
      arity(key.arity),
      clauses(predicate == nullptr ? ClauseList{} : predicate->clauses),
      serial(predicate == nullptr ? 0 : predicate->serial),
      defined_at(predicate == nullptr ? 0 : predicate->defined_at),
      taken_at(taken_at),
      text_clause_at(predicate == nullptr ? 0 : predicate->text_clause_at),
      defined_by_goal(predicate != nullptr && predicate->defined_by_goal),
      created(predicate == nullptr),
      dynamic(predicate != nullptr && predicate->dynamic),
      abolished(predicate != nullptr && predicate->abolished),
      meta(predicate == nullptr ? std::vector<MetaArg>{} : predicate->meta) {}

std::size_t LoadSnapshots::last_of(PredKey key) const {
  const std::size_t slot = index_.find(shots_, key);
  return slot == KeyIndex<Snapshot>::kNoSlot ? kNone : index_.at(slot);
}

// 確保は 3 か所（索引、earlier_、shots_）で、索引へ入れる前に済ませる。shots_ が伸びられなければ earlier_ を戻す。
void LoadSnapshots::push_back(Snapshot shot) {
  const PredKey key{shot.module, shot.name, shot.arity};
  index_.reserve(shots_, shots_.size() + 1);
  const std::size_t slot = index_.find(shots_, key);
  earlier_.push_back(slot == KeyIndex<Snapshot>::kNoSlot ? kNone : index_.at(slot));
  try {
    shots_.push_back(std::move(shot));
  } catch (...) {
    earlier_.pop_back();
    throw;
  }
  const auto at = static_cast<std::uint32_t>(shots_.size() - 1);
  if (slot == KeyIndex<Snapshot>::kNoSlot) index_.insert(shots_, at);
  else index_.repoint(slot, at);
}

// 名前の写しがあっても、ゴールが始めた今の定義が写していないものなら写す（ADR 0017）。ゴールが abolish して
// 作り直した述語へテキストが触れると、戻す先はその定義になる。テキストが始めた定義は写さない。この読み込みの写しが
// 前にあるのは、テキストが前の定義に触れたか、述語が無かったときで、失敗したら前の定義へ戻すか枠ごと消す。
// predicate は呼び手が表で引いたもの。同じ鍵の写しだけを新しい順に見る（ADR 0018）。
void remember_pred(const Impl& machine, LoadSnapshots& snapshots, PredKey key, const Predicate* predicate) {
  const bool by_goal = predicate != nullptr && predicate->defined_by_goal;
  for (std::size_t at = snapshots.last_of(key); at != LoadSnapshots::kNone; at = snapshots.earlier(at)) {
    const Snapshot& shot = snapshots[at];
    if (!by_goal || (!shot.created && shot.defined_at == predicate->defined_at)) return;
  }
  snapshots.push_back(Snapshot(key, predicate, machine.db_generation));
}

// 節は module に入る。頭部に M: が無ければ読み込み中のモジュールで、module/2 の無いテキストでは user。
bool add_clause(Impl& machine, std::uint32_t module, Clause clause, LoadSnapshots& snapshots,
                std::optional<PredKey>& last, const std::vector<Apart>& apart) {
  const auto name = head_name(clause);
  const auto arity = head_arity(clause);
  Predicate* found = find_pred(machine, PredKey{module, name, arity});
  // 失敗した読み込みは、この節を足す前の述語へ戻す。
  remember_pred(machine, snapshots, PredKey{module, name, arity}, found);
  if (found == nullptr) {
    // 取り込んだ名前と同じ局所の述語は作らない（iso-notes 第5節の取り込みの衝突）。
    if (const auto formal = check_local(machine, module, name, arity)) {
      fail_load(machine, write_term(machine.heap, machine.operators, *formal, {}, {}));
      return false;
    }
    Predicate created;
    created.module = module;
    created.name = name;
    created.arity = arity;
    found = &add_predicate(machine, std::move(created));
    adopt_meta(machine, *found);
  }
  Predicate& predicate = *found;
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
    begin_definition(machine, predicate, false);
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
  // 節ごとに渡し、同じ書き留めが続く間は note_defined が足さない。discontiguous か multifile の述語へ別のファイルが
  // 同じ読み込みで足すと後のファイルのものになり（L11 の Codex レビュー P2）、入れ子の consult の後で親が同じ述語の
  // 節を続けると親のものになる（同じ敵対的レビュー A5）。
  note_defined(machine, snapshots, predicate, true);
  predicate.seen = true;
  last = PredKey{module, name, arity};
  // 読み込みの節も assert と同じく世代を進める。読み込みの中のゴールが開いた走査は、後から読んだ節を見ない。
  append_clause(predicate, std::move(clause), ++machine.db_generation);
  predicate.text_clause_at = machine.db_generation;
  return true;
}

// 親が失敗したら、子のテキストが触れた述語も子が触れる前へ戻す。子の作った定義は写さなくても text_defined で消える
// ので、作る前の印（created）は渡さない。
void hand_up_snapshots(LoadSnapshots& parent, LoadSnapshots& own) {
  std::set<std::pair<std::size_t, std::size_t>> known;
  for (const Snapshot& shot : parent) {
    if (!shot.created) known.emplace(shot.serial, shot.defined_at);
  }
  for (Snapshot& shot : own) {
    if (shot.created || !known.emplace(shot.serial, shot.defined_at).second) continue;
    parent.push_back(std::move(shot));
  }
  hand_up_notes(parent, own);
}

void hand_up_notes(LoadSnapshots& parent, LoadSnapshots& own) {
  FileNotes& to = parent.notes();
  FileNotes& from = own.notes();
  to.defined.insert(to.defined.end(), from.defined.begin(), from.defined.end());
  to.files.insert(to.files.end(), from.files.begin(), from.files.end());
  to.metas.insert(to.metas.end(), from.metas.begin(), from.metas.end());
  to.declared.insert(to.declared.end(), from.declared.begin(), from.declared.end());
  from = FileNotes{};
}

// 文字だけの読み込み（load_program、mosatest の断片）も書き留める。持ち主は 0 で、ファイルが定義した述語を文字の
// テキストで置き換えると、そのファイルを読み直しても消さない（L11 の Codex レビュー P2）。
void note_defined(const Impl& machine, LoadSnapshots& snapshots, const Predicate& predicate, bool clauses) {
  std::vector<FileDef>& defined = snapshots.notes().defined;
  const FileDef item{key_of(predicate), machine.load_owner, predicate.defined_at, clauses};
  if (!defined.empty()) {
    const FileDef& back = defined.back();
    if (back.key.module == item.key.module && back.key.name == item.key.name && back.key.arity == item.key.arity &&
        back.file == item.file && back.defined_at == item.defined_at && back.clauses == item.clauses) {
      return;
    }
  }
  defined.push_back(item);
}

namespace {

bool key_less(const PredKey& a, const PredKey& b) {
  return std::tie(a.module, a.name, a.arity) < std::tie(b.module, b.name, b.arity);
}

// 読み込みの間にゴールが節を足したか消した。読み直しのゴールがした変更は、テキストを消しても残す（ADR 0017 と同じく、
// ゴールがしたことは残す）。足した節を全部 retract しても、新しい中身を初めて読んだときと同じく空の動的な述語が残る
// （L11 の 3 度目の Codex レビュー P2）。
bool goal_changed(const Predicate& predicate, std::uint64_t started_at) {
  return predicate.goal_changed_at > started_at;
}

// 書き留めた述語の鍵を名前の順に並べる。clauses_only なら節を与えたものだけ。
std::vector<PredKey> sorted_keys(const FileNotes& notes, bool clauses_only) {
  std::vector<PredKey> keys;
  keys.reserve(notes.defined.size());
  for (const FileDef& item : notes.defined) {
    if (!clauses_only || item.clauses) keys.push_back(item.key);
  }
  std::sort(keys.begin(), keys.end(), key_less);
  return keys;
}

// 読み終えたファイルの述語で、この読み込みのテキストが節を与えなかったもの。テキストが宣言しただけか、読み込みの
// ゴールが節を足し消ししたものは、述語を残して読み込みの前からの節だけを消す（emptied）。新しい中身を初めて読んだのと
// 同じになる（L11 の敵対的レビュー A1 と A2）。ほかは述語ごと消す（removed）。multifile の述語は、ほかのファイルが
// 節を足したかもしれないので残す。消すと表が詰まるので、鍵だけを集めてから消す。
struct StalePreds {
  std::vector<PredKey> removed;
  std::vector<PredKey> emptied;
};

StalePreds stale_preds(const Impl& machine, const FileNotes& notes, std::uint64_t started_at) {
  if (notes.files.empty()) return {};
  std::vector<std::uint32_t> files = notes.files;
  std::sort(files.begin(), files.end());
  const std::vector<PredKey> given = sorted_keys(notes, true);
  const std::vector<PredKey> named = sorted_keys(notes, false);
  // 述語ごとに machine.declared をなめると、multifile の述語の数の 2 乗になる（4 度目の Codex レビュー P2）。
  std::vector<PredKey> shared;
  for (const SharedPred& item : machine.declared) {
    if (item.shared) shared.push_back(PredKey{item.module, item.name, item.arity});
  }
  std::sort(shared.begin(), shared.end(), key_less);
  StalePreds stale;
  for (const Predicate& predicate : machine.predicates) {
    if (predicate.source_file == 0 || predicate.abolished) continue;
    if (!std::binary_search(files.begin(), files.end(), predicate.source_file)) continue;
    const PredKey key = key_of(predicate);
    if (std::binary_search(given.begin(), given.end(), key, key_less)) continue;
    if (std::binary_search(shared.begin(), shared.end(), key, key_less)) continue;
    const bool kept = std::binary_search(named.begin(), named.end(), key, key_less) || goal_changed(predicate, started_at);
    (kept ? stale.emptied : stale.removed).push_back(key);
  }
  return stale;
}

// 消した述語の discontiguous の宣言も外す。残すと宣言だけの手続きになり、呼び出しが existence_error にならず失敗する
// （6 度目の Codex レビュー P2）。この読み込みのテキストが宣言し直したものは、初めて読んだときと同じく残す。
// multifile の述語は消さないので、その宣言は見ない。
void drop_declarations(Impl& machine, std::vector<PredKey> removed, std::vector<PredKey> declared) {
  if (removed.empty()) return;
  std::sort(removed.begin(), removed.end(), key_less);
  std::sort(declared.begin(), declared.end(), key_less);
  std::erase_if(machine.declared, [&removed, &declared](const SharedPred& item) {
    const PredKey key{item.module, item.name, item.arity};
    return !item.shared && std::binary_search(removed.begin(), removed.end(), key, key_less) &&
           !std::binary_search(declared.begin(), declared.end(), key, key_less);
  });
}

}  // namespace

// 定義した述語の持ち主は、書き留めた順に付け替えるので、最後に定義したファイルになる。
void apply_file_notes(Impl& machine, FileNotes& notes, std::uint64_t started_at) {
  if (notes.files.empty() && notes.applied == notes.defined.size()) return;
  const StalePreds stale = stale_preds(machine, notes, started_at);
  retire_older(machine, stale.emptied, started_at);
  std::sort(notes.metas.begin(), notes.metas.end(), key_less);
  abolish_stale(machine, stale.removed, notes.metas);
  drop_declarations(machine, stale.removed, notes.declared);
  for (std::size_t at = notes.applied; at < notes.defined.size(); ++at) {
    const FileDef& item = notes.defined[at];
    Predicate* predicate = find_pred(machine, item.key);
    if (predicate == nullptr || predicate->abolished || predicate->defined_at != item.defined_at) continue;
    predicate->source_file = item.file;
  }
  notes.files.clear();
  notes.applied = notes.defined.size();
}

namespace {

void restore_shot(Impl& machine, Predicate& predicate, const Snapshot& shot) {
  // 写しへ戻した節にも生成関数は付け直さない（ADR 0009）。外したら、その述語はインタプリタで動き続ける。
  detach_native(machine, predicate);
  replace_clauses(predicate, shot.clauses);
  predicate.dynamic = shot.dynamic;
  predicate.abolished = shot.abolished;
  predicate.meta = shot.meta;
  predicate.defined_at = shot.defined_at;
  predicate.defined_by_goal = shot.defined_by_goal;
  predicate.text_clause_at = shot.text_clause_at;
}

// 写しの後にテキストが節を足していなければ、テキストは宣言しかしていない。ゴールが足し消しした節は残し、宣言だけを
// 戻す（ADR 0017 の改訂）。宣言が dynamic にした静的な空の述語へゴールが節を足していれば、dynamic のまま残す。
void restore_declarations(Impl& machine, Predicate& predicate, const Snapshot& shot) {
  detach_native(machine, predicate);
  predicate.meta = shot.meta;
  if (!shot.dynamic && live_count(predicate) == 0) predicate.dynamic = false;
}

bool text_defined(std::size_t defined_at, bool by_goal, std::size_t previous_serial) {
  return defined_at > previous_serial && !by_goal;
}

bool text_defined(const Predicate& predicate, std::size_t previous_serial) {
  return text_defined(predicate.defined_at, predicate.defined_by_goal, previous_serial);
}

// ゴールが作った述語は、モジュールの保留の meta_predicate をそのとき引き取った。保留は呼び手がモジュール表ごと
// 読み込みの前へ戻したので、失敗したテキストの宣言を残さないよう引き取り直す（M24 の 2 度目の Codex レビュー P2）。
// 写しの meta も、作ったときに引き取ったものなので同じく直す。abolish 済みの枠は、使い直すときに引き取る。
void readopt_meta(Impl& machine, std::size_t previous_serial) {
  for (Predicate& predicate : machine.predicates) {
    if (!predicate.defined_by_goal || predicate.defined_at <= previous_serial || predicate.abolished) continue;
    predicate.meta.clear();
    predicate.meta_declared = false;
    adopt_meta(machine, predicate);
  }
}

}  // namespace

// テキストがしたことを取り消し、指令のゴールがしたことは残す（ADR 0017）。写しは、写した時と同じ定義が残る枠だけ
// へ戻す。ゴールが abolish した定義へ戻すと、ゴールの abolish の後に作り直した述語を前の節で上書きする。
// テキストが始めた定義は、その枠の最後の写し（ゴールが abolish した後の枠など）へ戻し、写しが無ければ枠ごと消す。
// この読み込みのテキストが始めた定義の写し（子が親のテキストの定義を写したものなど）は戻す先にしない。その定義も
// 取り消すので、戻すと枠ごと消え、ゴールの abolish の下で開いている走査が残りの節を失う（2 度目の Codex レビュー P2）。
void rollback_preds(Impl& machine, const LoadSnapshots& snapshots, std::size_t previous_serial) {
  const auto usable = [previous_serial](const Snapshot& shot) {
    return !shot.created && !text_defined(shot.defined_at, shot.defined_by_goal, previous_serial);
  };
  std::unordered_map<std::size_t, const Snapshot*> last_of;
  for (const Snapshot& shot : snapshots) {
    if (usable(shot)) last_of[shot.serial] = &shot;
  }
  for (const Snapshot& shot : snapshots) {
    if (!usable(shot)) continue;
    Predicate* predicate = find_pred(machine, PredKey{shot.module, shot.name, shot.arity});
    if (predicate == nullptr || predicate->defined_at != shot.defined_at) continue;
    if (predicate->text_clause_at > shot.taken_at) restore_shot(machine, *predicate, shot);
    else restore_declarations(machine, *predicate, shot);
  }
  for (Predicate& predicate : machine.predicates) {
    if (!text_defined(predicate, previous_serial)) continue;
    // 宣言が始め、テキストが節を足していない定義にゴールが節を足していれば、ゴールが作った定義と同じく残す。
    // ゴールの定義になるので、readopt_meta が保留の meta_predicate を読み込みの前のものから引き取り直す。
    if (predicate.text_clause_at == 0 && live_count(predicate) > 0) {
      predicate.defined_by_goal = true;
      continue;
    }
    if (const auto found = last_of.find(predicate.serial); found != last_of.end()) {
      restore_shot(machine, predicate, *found->second);
    }
  }
  readopt_meta(machine, previous_serial);
  // 表の長さへ縮めると、指示が述語を消していたとき、作った述語が残るか、空の述語が末尾に足されて serial の昇順が
  // 崩れる（M18 の Codex レビュー P2）。この読み込みが宣言したモジュールの述語も消す。モジュールは呼び手が先に
  // 戻しているので、残すと呼べないモジュールの述語が current_predicate/1 に見える（宣言の無いモジュールへの assert
  // は existence_error になる）。
  machine.predicates.erase_if([&machine, previous_serial](const Predicate& predicate) {
    if (text_defined(predicate, previous_serial)) return true;
    return predicate.serial > previous_serial && find_module(machine, predicate.module) == nullptr;
  });
}

}
