// docs/SPEC.md 第5節
// module(+atom, +list) は指示。述語ではない。
//   この段階では誤りを投げない
// テスト: tests/pl/modules.pl

#include "../machine/internal.hpp"

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

void remember_module(Impl& machine, std::uint32_t name) {
  for (const std::uint32_t existing : machine.modules) {
    if (existing == name) return;
  }
  machine.modules.push_back(name);
}

}  // namespace

int apply_module_directive(Impl& machine, Cell term) {
  std::uint32_t name = 0;
  if (!module_name(machine, term, name)) return 0;
  // このファイルの残りの節は、呼び出し元ではなく宣言したモジュールへ入る。
  machine.load_home = name;
  remember_module(machine, name);
  return 1;
}

}  // namespace mosa
