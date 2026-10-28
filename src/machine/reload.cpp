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
  snapshots.push_back(std::move(shot));
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
  if (!replace_file_clauses(machine, snapshots, machine.load_origin)) return false;
  take_picture(machine, module);
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
  if (predicate != nullptr) {
    shot.clauses = predicate->clauses;
    shot.dynamic = predicate->dynamic;
  }
  snapshots.push_back(std::move(shot));
}

void rollback(Impl& machine, const std::vector<Snapshot>& snapshots, std::size_t previous_size) {
  for (const Snapshot& shot : snapshots) {
    if (shot.created) continue;
    Predicate* predicate = predicate_in(machine, shot.home, shot.name, shot.arity);
    if (predicate == nullptr) continue;
    predicate->clauses = shot.clauses;
    predicate->dynamic = shot.dynamic;
  }
  machine.predicates.resize(previous_size);
}

}  // namespace mosa
