// 同じファイルの再 consult は、そのファイルの節だけを置き換える。
// 他ファイルの multifile 節は相対順のまま残し、新しい節は末尾に足す。
// 失敗した再読みは、節と公開と演算子を触る前へ戻す。

#include "internal.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace mosa {
namespace {

bool stack_busy(const Impl& machine) {
  for (const Entry& entry : machine.stack) {
    if (entry.pick != Pick::Cleanup && entry.pick != Pick::Catch) return true;
  }
  return false;
}

bool from_file(const Clause& clause, std::string_view origin) { return clause.origin == origin; }

bool holds_origin(const Predicate& predicate, std::string_view origin) {
  for (const Clause& clause : predicate.clauses) {
    if (from_file(clause, origin)) return true;
  }
  return false;
}

bool module_at(const Impl& machine, std::string_view origin, std::uint32_t& module) {
  for (const ModuleSource& item : machine.module_sources) {
    if (item.path != origin) continue;
    module = item.module;
    return true;
  }
  return false;
}

void remember_existing(std::vector<Snapshot>& snapshots, const Predicate& predicate) {
  for (const Snapshot& shot : snapshots) {
    if (shot.home == predicate.home && shot.name == predicate.name && shot.arity == predicate.arity) return;
  }
  Snapshot shot;
  shot.home = predicate.home;
  shot.name = predicate.name;
  shot.arity = predicate.arity;
  shot.clauses = predicate.clauses;
  shot.dynamic = predicate.dynamic;
  shot.imported = predicate.imported;
  shot.origin = predicate.origin;
  shot.origin_name = predicate.origin_name;
  shot.meta = predicate.meta;
  snapshots.push_back(std::move(shot));
}

void copy_shot(Predicate& predicate, const Snapshot& shot) {
  predicate.clauses = shot.clauses;
  predicate.dynamic = shot.dynamic;
  predicate.imported = shot.imported;
  predicate.origin = shot.origin;
  predicate.origin_name = shot.origin_name;
  predicate.meta = shot.meta;
}

void drop_origin(Predicate& predicate, std::string_view origin) {
  std::erase_if(predicate.clauses, [&](const Clause& clause) { return from_file(clause, origin); });
}

void take_picture(Impl& machine, std::uint32_t module) {
  ReloadPicture picture;
  picture.on = true;
  picture.module = module;
  for (const ExportMark& item : machine.exports) {
    if (item.module == module) picture.exports.push_back(item);
  }
  for (const ExportedOp& item : machine.exported_ops) {
    if (item.module == module) picture.exported_ops.push_back(item);
  }
  for (const ModuleOps& item : machine.module_ops) {
    if (item.home != module) continue;
    picture.had_ops = true;
    picture.ops = item;
    break;
  }
  machine.reload_picture = std::move(picture);
}

bool replace_file_clauses(Impl& machine, std::vector<Snapshot>& snapshots, std::string_view origin) {
  bool replacing = false;
  for (const Predicate& predicate : machine.predicates) {
    if (holds_origin(predicate, origin)) replacing = true;
  }
  // 選択点が残る再読みは、今までどおり置き換えない。multifile の追加とは別である。
  if (replacing && stack_busy(machine)) {
    machine.diagnostic = "実行中の述語は置き換えられない";
    machine.syntax_diag = false;
    return false;
  }
  for (Predicate& predicate : machine.predicates) {
    if (!holds_origin(predicate, origin)) continue;
    remember_existing(snapshots, predicate);
    drop_origin(predicate, origin);
  }
  return true;
}

// 節を落としたあとの空席は、同じ名前の取り込みを衝突にする。動的な空席は定義なので残す。
void drop_empty_locals(Impl& machine, std::vector<Snapshot>& snapshots, std::uint32_t module) {
  std::vector<Predicate> kept;
  kept.reserve(machine.predicates.size());
  for (Predicate& predicate : machine.predicates) {
    const bool hole = predicate.home == module && !predicate.imported && !predicate.dynamic &&
                      predicate.clauses.empty();
    if (!hole) {
      kept.push_back(std::move(predicate));
      continue;
    }
    remember_existing(snapshots, predicate);
  }
  machine.predicates = std::move(kept);
}

// ファイルの use_module はやり直す。残った席は、新しい局所節より先に古い出所を呼ぶ。
void drop_imports(Impl& machine, std::vector<Snapshot>& snapshots, std::uint32_t module) {
  std::vector<Predicate> kept;
  kept.reserve(machine.predicates.size());
  for (Predicate& predicate : machine.predicates) {
    if (predicate.home == module && predicate.imported) {
      remember_existing(snapshots, predicate);
      continue;
    }
    kept.push_back(std::move(predicate));
  }
  machine.predicates = std::move(kept);
}

void release_surface(Impl& machine, std::uint32_t module) {
  std::erase_if(machine.exports, [module](const ExportMark& item) { return item.module == module; });
  std::erase_if(machine.exported_ops, [module](const ExportedOp& item) { return item.module == module; });
  std::erase_if(machine.module_ops, [module](const ModuleOps& item) { return item.home == module; });
}

}  // namespace

ReloadFrame::ReloadFrame(Impl& machine) : machine(machine), saved(machine.reload_picture) {
  machine.reload_picture = {};
}

ReloadFrame::~ReloadFrame() { machine.reload_picture = std::move(saved); }

bool begin_file_reload(Impl& machine, std::vector<Snapshot>& snapshots) {
  // テキストだけの読み込みと、まだモジュールになっていないファイルは置き換えない。
  if (machine.load_origin.empty()) return true;
  std::uint32_t module = 0;
  if (!module_at(machine, machine.load_origin, module)) return true;
  // 落とす前に写す。初期化の assert は、この写しで戻す。
  for (const Predicate& predicate : machine.predicates) {
    if (predicate.home == module) remember_existing(snapshots, predicate);
  }
  if (!replace_file_clauses(machine, snapshots, machine.load_origin)) return false;
  take_picture(machine, module);
  drop_imports(machine, snapshots, module);
  drop_empty_locals(machine, snapshots, module);
  // 写しは失敗で戻す。成功したファイルの公開と演算子は、読み直した宣言だけにする。
  release_surface(machine, module);
  return true;
}

void restore_reload_picture(Impl& machine) {
  if (!machine.reload_picture.on) return;
  const auto module = machine.reload_picture.module;
  std::erase_if(machine.exports, [module](const ExportMark& item) { return item.module == module; });
  machine.exports.insert(machine.exports.end(), machine.reload_picture.exports.begin(),
                         machine.reload_picture.exports.end());
  std::erase_if(machine.exported_ops, [module](const ExportedOp& item) { return item.module == module; });
  machine.exported_ops.insert(machine.exported_ops.end(), machine.reload_picture.exported_ops.begin(),
                              machine.reload_picture.exported_ops.end());
  std::erase_if(machine.module_ops, [module](const ModuleOps& item) { return item.home == module; });
  if (machine.reload_picture.had_ops) machine.module_ops.push_back(machine.reload_picture.ops);
  machine.reload_picture.on = false;
}

void remember_pred(Impl& machine, std::vector<Snapshot>& snapshots, std::uint32_t name, std::uint16_t arity) {
  const auto home = machine.load_home;
  for (const Snapshot& shot : snapshots) {
    if (shot.home == home && shot.name == name && shot.arity == arity) return;
  }
  Predicate* predicate = predicate_in(machine, home, name, arity);
  Snapshot shot;
  shot.home = home;
  shot.name = name;
  shot.arity = arity;
  shot.created = predicate == nullptr;
  if (predicate != nullptr) remember_existing(snapshots, *predicate);
  else snapshots.push_back(std::move(shot));
}

void restore_one(Impl& machine, const Snapshot& shot) {
  Predicate* predicate = predicate_in(machine, shot.home, shot.name, shot.arity);
  if (predicate == nullptr) {
    Predicate made;
    made.home = shot.home;
    made.name = shot.name;
    made.arity = shot.arity;
    copy_shot(made, shot);
    machine.predicates.push_back(std::move(made));
    return;
  }
  copy_shot(*predicate, shot);
}

void rollback(Impl& machine, const std::vector<Snapshot>& snapshots) {
  // この読み込みが足した述語だけ消す。入れ子が確定したモジュールの節は別の番号なので残る。
  const auto failed = machine.current_load;
  if (failed != 0) {
    std::erase_if(machine.predicates, [failed](const Predicate& predicate) { return predicate.load_id == failed; });
  }
  for (const Snapshot& shot : snapshots) {
    if (!shot.created) restore_one(machine, shot);
  }
}

void stamp_load(Impl& machine, Predicate& predicate) { predicate.load_id = machine.current_load; }

void release_load_id(Impl& machine, std::uint32_t parent) {
  // モジュールを新しく載せていない読み込みは、親が失敗したら一緒に消す。
  const bool added_module = !machine.module_added.empty() && !machine.module_added.back().empty();
  // 成功した再読みの節は、親の失敗で消さない。モジュールは残るので、節が無いと読み直されない。
  if (added_module || machine.reload_picture.on || machine.current_load == 0 || machine.current_load == parent) {
    return;
  }
  const auto from = machine.current_load;
  for (Predicate& predicate : machine.predicates) {
    if (predicate.load_id == from) predicate.load_id = parent;
  }
}

LoadClock::LoadClock(Impl& machine) : machine(machine), saved(machine.current_load) {
  machine.current_load = ++machine.load_clock;
}

LoadClock::~LoadClock() { machine.current_load = saved; }

}  // namespace mosa
