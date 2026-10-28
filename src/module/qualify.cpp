// :/2 限定ゴールと、読み込みの指令 module/2、export/1（SPEC 第5節、ADR 0006）。
// モード: :(+atom, +callable)、module(+atom, +list)、export(+predicate_indicator_or_list)。
// エラー: instantiation_error、type_error(atom, M)、existence_error(module, M)、type_error(callable, M:G)、
//         permission_error(create, module, M)、type_error(list, L)、type_error(predicate_indicator, PI)、
//         type_error(integer, A)、domain_error(not_less_than_zero, A)、representation_error(max_arity)。
// テスト: tests/pl/modules.pl、tests/cpp/module_load_test.cpp
#include "module.hpp"

#include "../machine/internal.hpp"

#include <array>
#include <string>
#include <vector>

namespace mosa {

namespace {

std::uint32_t atom_of(Cell cell) { return static_cast<std::uint32_t>(cell.raw() >> 3); }

bool is_callable(Cell cell) { return cell.tag() == Tag::Atom || cell.tag() == Tag::Str || cell.tag() == Tag::Lis; }

bool is_integer(Cell cell) { return cell.tag() == Tag::Int || cell.tag() == Tag::IntBox; }

// 1 つの PI でもリストでもよい。module/2 の第 2 引数は呼び出し側がリストに限る。
std::optional<Cell> read_indicators(Impl& machine, Cell cell, std::uint32_t module, std::vector<PredKey>& keys) {
  cell = machine.heap.deref(cell);
  const WalkedList walked = walk_list(machine, cell);
  if (walked.shape == ListShape::Variable || walked.shape == ListShape::Partial) {
    return formal_instantiation(machine);
  }
  std::vector<Cell> items = walked.items;
  if (walked.shape != ListShape::Proper) {
    if (cell.tag() == Tag::Lis) return formal_type(machine, "list", cell);
    items = {cell};
  }
  for (const Cell item : items) {
    PredKey key{module, 0, 0};
    if (const auto formal = read_indicator(machine, item, key)) return formal;
    keys.push_back(key);
  }
  return std::nullopt;
}

void add_keys(Module& module, const std::vector<PredKey>& keys) {
  for (const PredKey& key : keys) {
    bool known = false;
    for (const PredKey& old : module.exports) known = known || (old.name == key.name && old.arity == key.arity);
    if (!known) module.exports.push_back(key);
  }
}

}  // namespace

bool run_qualify(Impl& machine, const Goal& goal) {
  if (goal.arity != 2 || machine.atoms.name(goal.name) != ":") return false;
  const Cell module = machine.heap.deref(goal.arg(machine, 0));
  const Cell inner = machine.heap.deref(goal.arg(machine, 1));
  if (machine.heap.unbound(module) || machine.heap.unbound(inner)) {
    raise_formal(machine, formal_instantiation(machine), goal.name, goal.arity);
    return true;
  }
  if (module.tag() != Tag::Atom) {
    raise_formal(machine, formal_type(machine, "atom", module), goal.name, goal.arity);
    return true;
  }
  if (find_module(machine, atom_of(module)) == nullptr) {
    raise_formal(machine, formal_existence(machine, "module", module), goal.name, goal.arity);
    return true;
  }
  const Compiled compiled = is_callable(inner) ? compile_goal(machine.heap, inner) : Compiled{};
  if (!is_callable(inner) || !body_callable(compiled.term)) {
    // culprit は利用者が書いた M:G 全体。call(M:G) の引数と同じ項になる（iso-notes の決定 A）。
    const std::array<Cell, 2> parts{module, inner};
    const Cell culprit = machine.heap.structure(goal.name, parts);
    raise_formal(machine, formal_type(machine, "callable", culprit), goal.name, goal.arity);
    return true;
  }
  // 枠のカットの境界をそのまま渡す。M:G はカットに透明である（iso-notes の決定 A）。
  launch(machine, compiled.term, goal.cont, goal.cut, compiled.env, atom_of(module));
  return true;
}

// abolish/1 と同じ順で PI を確かめる（8.9.4.3）。
std::optional<Cell> read_indicator(Impl& machine, Cell cell, PredKey& key) {
  cell = machine.heap.deref(cell);
  if (machine.heap.unbound(cell)) return formal_instantiation(machine);
  if (cell.tag() != Tag::Str || machine.heap.functor_arity(cell) != 2 ||
      machine.atoms.name(machine.heap.functor_atom(cell)) != "/") {
    return formal_type(machine, "predicate_indicator", cell);
  }
  const Cell name = machine.heap.deref(machine.heap.arg(cell, 0));
  const Cell arity = machine.heap.deref(machine.heap.arg(cell, 1));
  if (machine.heap.unbound(name) || machine.heap.unbound(arity)) return formal_instantiation(machine);
  if (name.tag() != Tag::Atom) return formal_type(machine, "atom", name);
  if (!is_integer(arity)) return formal_type(machine, "integer", arity);
  const auto value = machine.heap.integer_value(arity);
  if (value < 0) return formal_domain(machine, "not_less_than_zero", arity);
  if (value > 1024) return formal_representation(machine, "max_arity");
  key.name = atom_of(name);
  key.arity = static_cast<std::uint16_t>(value);
  return std::nullopt;
}

std::optional<Cell> declare_module(Impl& machine, Cell name, Cell exports, bool first, std::string_view file) {
  name = machine.heap.deref(name);
  exports = machine.heap.deref(exports);
  if (machine.heap.unbound(name)) return formal_instantiation(machine);
  if (name.tag() != Tag::Atom) return formal_type(machine, "atom", name);
  const WalkedList walked = walk_list(machine, exports);
  if (walked.shape == ListShape::Bad) return formal_type(machine, "list", exports);
  std::vector<PredKey> keys;
  // 公開リストの op(P, T, N) はフェーズ4 Task 10 で受ける。それまでは PI の型の誤りになる。
  if (const auto formal = read_indicators(machine, exports, atom_of(name), keys)) return formal;
  if (!first) return formal_permission(machine, "create", "module", name);
  Module* module = find_module(machine, atom_of(name));
  if (module == nullptr) {
    machine.modules.push_back(Module{atom_of(name), machine.user_module, {}, {}, {}, {}});
    module = &machine.modules.back();
  }
  // 読み直しは公開と取り込みを作り直す。取り込みはこの後の use_module/1,2 がもう一度足す。
  module->exports.clear();
  module->imports.clear();
  module->pending_meta.clear();
  module->file = std::string{file};
  // 同じファイルを別の名前で読み直したら、古いモジュールはもうそのファイルのものではない。
  for (Module& other : machine.modules) {
    if (!file.empty() && other.name != module->name && other.file == file) other.file.clear();
  }
  add_keys(*module, keys);
  machine.load_module = module->name;
  return std::nullopt;
}

std::optional<Cell> add_exports(Impl& machine, Cell indicators) {
  std::vector<PredKey> keys;
  if (const auto formal = read_indicators(machine, indicators, machine.load_module, keys)) return formal;
  Module* module = find_module(machine, machine.load_module);
  if (module != nullptr) add_keys(*module, keys);
  return std::nullopt;
}

}
