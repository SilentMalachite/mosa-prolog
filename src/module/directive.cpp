// docs/SPEC.md 第5節
// module(+atom, +list) は指示。述語ではない。
//   permission_error(modify, module, Module)
// テスト: tests/pl/modules.pl

#include "../machine/internal.hpp"

#include "mosa/writer.hpp"

#include <algorithm>

namespace mosa {
namespace {

bool module_name(Impl& machine, Cell directive, std::uint32_t& name) {
  directive = machine.heap.deref(directive);
  if (!is_directive(machine.heap, directive)) return false;
  const Cell body = machine.heap.deref(machine.heap.arg(directive, 0));
  if (body.tag() != Tag::Str || machine.heap.functor_arity(body) != 2) return false;
  if (machine.atoms.name(machine.heap.functor_atom(body)) != "module") return false;
  const Cell module = machine.heap.deref(machine.heap.arg(body, 0));
  if (module.tag() != Tag::Atom) return false;
  name = static_cast<std::uint32_t>(module.raw() >> 3);
  return true;
}

bool module_known(const Impl& machine, std::uint32_t name) {
  return std::find(machine.modules.begin(), machine.modules.end(), name) != machine.modules.end();
}

void remember_module(Impl& machine, std::uint32_t name) {
  if (module_known(machine, name)) return;
  machine.modules.push_back(name);
  if (!machine.module_added.empty()) machine.module_added.back().push_back(name);
}

}  // namespace

void drop_load_modules(Impl& machine) {
  if (machine.module_added.empty()) return;
  for (const std::uint32_t name : machine.module_added.back()) {
    std::vector<std::uint32_t>& mods = machine.modules;
    mods.erase(std::remove(mods.begin(), mods.end(), name), mods.end());
  }
  machine.module_added.pop_back();
}

void reject_module(Impl& machine, std::uint32_t name) {
  // ヒープは失敗で切り詰まる。投げ直すのはアトム番号だけ残す。
  machine.module_fault = name;
  const Cell culprit = machine.heap.atom(name);
  const Cell formal = formal_permission(machine, "modify", "module", culprit);
  machine.diagnostic = write_term(machine.heap, machine.operators, formal, {}, {});
  machine.syntax_diag = false;
}

int apply_module_directive(Impl& machine, Cell term) {
  std::uint32_t name = 0;
  if (!module_name(machine, term, name)) return 0;
  // 第5節は1ファイル1モジュール。最初の項でない名前も、既にある名前も残さない。
  // 同じファイルの読み直しは、まだこの検査と分けていない。
  if (machine.load_terms != 1 || module_known(machine, name)) {
    reject_module(machine, name);
    return -1;
  }
  machine.load_home = name;
  remember_module(machine, name);
  return 1;
}

}  // namespace mosa
