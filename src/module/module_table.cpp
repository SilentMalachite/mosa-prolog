// モジュール表と述語の解決（SPEC 第5節、ADR 0006）。system の組み込みは run_builtin が先に引く。
#include "module.hpp"

#include "../machine/internal.hpp"

#include <algorithm>
#include <cstddef>
#include <utility>

namespace mosa {

namespace {

bool same_key(const Predicate& predicate, PredKey key) {
  return predicate.module == key.module && predicate.name == key.name && predicate.arity == key.arity;
}

const Import* find_import(const Module& module, std::uint32_t name, std::uint16_t arity) {
  for (const Import& item : module.imports) {
    if (item.alias == name && item.arity == arity) return &item;
  }
  return nullptr;
}

}  // namespace

void install_modules(Impl& machine) {
  machine.user_module = machine.atoms.intern("user");
  machine.system_module = machine.atoms.intern("system");
  Module system;
  system.name = machine.system_module;
  system.default_import = machine.system_module;
  machine.modules.push_back(std::move(system));
  Module user;
  user.name = machine.user_module;
  user.default_import = machine.system_module;
  machine.modules.push_back(std::move(user));
  machine.load_module = machine.user_module;
}

const Module* find_module(const Impl& machine, std::uint32_t name) {
  for (const Module& module : machine.modules) {
    if (module.name == name) return &module;
  }
  return nullptr;
}

Module* find_module(Impl& machine, std::uint32_t name) {
  for (Module& module : machine.modules) {
    if (module.name == name) return &module;
  }
  return nullptr;
}

Predicate* find_pred(Impl& machine, PredKey key) {
  for (Predicate& predicate : machine.predicates) {
    if (same_key(predicate, key)) return &predicate;
  }
  return nullptr;
}

const Predicate* find_pred(const Impl& machine, PredKey key) {
  for (const Predicate& predicate : machine.predicates) {
    if (same_key(predicate, key)) return &predicate;
  }
  return nullptr;
}

Predicate& add_predicate(Impl& machine, Predicate made) {
  made.serial = ++machine.pred_serial;
  made.defined_at = made.serial;
  machine.predicates.push_back(std::move(made));
  return machine.predicates.back();
}

// 番号は serial と同じ数から取るので、定義の始まりどうしも枠の作成とも重ならず、読み込みの始めの pred_serial と
// 比べれば読み込みの間に始めた定義かがわかる。serial は振り直さないので、表の昇順（M18）は崩れない。
void begin_definition(Impl& machine, Predicate& predicate, bool by_goal) {
  predicate.defined_at = ++machine.pred_serial;
  predicate.defined_by_goal = by_goal;
}

// 表は serial の昇順なので二分探索で引く。current_predicate/1 のやり直しごとに表をなめない。
const Predicate* pred_by_serial(const Impl& machine, std::size_t serial) {
  const auto found = std::ranges::lower_bound(machine.predicates, serial, {}, &Predicate::serial);
  if (found == machine.predicates.end() || found->serial != serial) return nullptr;
  return &*found;
}

Predicate* resolve_pred(Impl& machine, std::uint32_t context, std::uint32_t name, std::uint16_t arity) {
  // 既定の取り込み元をたどる。表の大きさで打ち切り、取り込み元の循環で止まらなくならないようにする。
  std::uint32_t current = context;
  for (std::size_t step = 0; step <= machine.modules.size(); ++step) {
    if (Predicate* own = find_pred(machine, PredKey{current, name, arity})) return own;
    const Module* module = find_module(machine, current);
    if (module == nullptr) return nullptr;
    if (const Import* imported = find_import(*module, name, arity)) {
      return find_pred(machine, PredKey{imported->from, imported->name, arity});
    }
    if (module->default_import == current) return nullptr;
    current = module->default_import;
  }
  return nullptr;
}

}
