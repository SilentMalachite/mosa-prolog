// モジュール表と述語の解決（SPEC 第5節、ADR 0006）。system の組み込みは run_builtin が先に引く。
#include "module.hpp"

#include "../machine/internal.hpp"

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
  machine.modules.push_back(Module{machine.system_module, machine.system_module, {}, {}, {}});
  machine.modules.push_back(Module{machine.user_module, machine.system_module, {}, {}, {}});
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

void import_exports(Impl& machine, std::uint32_t from, std::uint32_t into) {
  if (from == into) return;
  const Module* source = find_module(machine, from);
  Module* target = find_module(machine, into);
  if (source == nullptr || target == nullptr) return;
  for (const PredKey& key : source->exports) {
    const Import item{key.name, key.arity, key.name, from};
    bool replaced = false;
    for (Import& old : target->imports) {
      if (old.alias != item.alias || old.arity != item.arity) continue;
      old = item;
      replaced = true;
    }
    if (!replaced) target->imports.push_back(item);
  }
}

}
