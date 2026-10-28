// use_module/2 の取り込み指定と、取り込みの表への登録（SPEC 第5節、ADR 0006）。
// モード: use_module(+atom)、use_module(+atom, +list)（読み込みの指令。ファイルを開くのは load.cpp）。
// エラー: instantiation_error、type_error(list, L)、type_error(predicate_indicator, PI)、type_error(atom, Alias)、
//         permission_error(import_into, procedure, From:N/A)、existence_error(procedure, M:N/A)。
// テスト: tests/pl/modules.pl、tests/cpp/module_load_test.cpp
#include "module.hpp"

#include "../machine/internal.hpp"

#include <array>
#include <vector>

namespace mosa {

namespace {

bool named(const Impl& machine, Cell cell, std::string_view name, std::uint16_t arity) {
  return cell.tag() == Tag::Str && machine.heap.functor_arity(cell) == arity &&
         machine.atoms.name(machine.heap.functor_atom(cell)) == name;
}

// iso-notes の決定どおり、どのモジュールの述語かを付けた M:N/A を culprit にする。
Cell qualified_pi(Impl& machine, std::uint32_t module, std::uint32_t name, std::uint16_t arity) {
  const std::array<Cell, 2> parts{machine.heap.atom(module), cell_functor(machine, name, arity)};
  return machine.heap.structure(machine.atoms.intern(":"), parts);
}

std::optional<Cell> conflict(Impl& machine, std::uint32_t from, std::uint32_t name, std::uint16_t arity) {
  return formal_permission(machine, "import_into", "procedure", qualified_pi(machine, from, name, arity));
}

std::optional<Cell> read_item(Impl& machine, Cell cell, Import& item) {
  cell = machine.heap.deref(cell);
  Cell indicator = cell;
  Cell alias{};
  const bool aliased = named(machine, cell, "as", 2);
  if (aliased) {
    indicator = machine.heap.arg(cell, 0);
    alias = machine.heap.deref(machine.heap.arg(cell, 1));
  }
  PredKey key;
  if (const auto formal = read_indicator(machine, indicator, key)) return formal;
  if (aliased && machine.heap.unbound(alias)) return formal_instantiation(machine);
  if (aliased && alias.tag() != Tag::Atom) return formal_type(machine, "atom", alias);
  item.name = key.name;
  item.arity = key.arity;
  item.alias = aliased ? static_cast<std::uint32_t>(alias.raw() >> 3) : key.name;
  return std::nullopt;
}

// 引数を全部確かめてから spec を埋める。途中の失敗で半端な指定を残さない。
std::optional<Cell> read_list(Impl& machine, Cell list, std::vector<Cell>& items) {
  const WalkedList walked = walk_list(machine, list);
  if (walked.shape == ListShape::Variable || walked.shape == ListShape::Partial) {
    return formal_instantiation(machine);
  }
  if (walked.shape != ListShape::Proper) return formal_type(machine, "list", machine.heap.deref(list));
  items = walked.items;
  return std::nullopt;
}

bool exported(const Module& module, std::uint32_t name, std::uint16_t arity) {
  for (const PredKey& key : module.exports) {
    if (key.name == name && key.arity == arity) return true;
  }
  return false;
}

bool excluded(const ImportSpec& spec, const PredKey& key) {
  for (const PredKey& item : spec.except) {
    if (item.name == key.name && item.arity == key.arity) return true;
  }
  return false;
}

std::optional<Cell> add_import(Impl& machine, std::uint32_t into, const Import& item) {
  if (find_pred(machine, PredKey{into, item.alias, item.arity}) != nullptr) {
    return conflict(machine, item.from, item.name, item.arity);
  }
  Module* target = find_module(machine, into);
  if (target == nullptr) return std::nullopt;
  for (const Import& old : target->imports) {
    if (old.alias != item.alias || old.arity != item.arity) continue;
    // 同じ取り込み元からの取り込み直しは、読み直しと二重の use_module で起きる。衝突ではない。
    if (old.from == item.from && old.name == item.name) return std::nullopt;
    return conflict(machine, item.from, item.name, item.arity);
  }
  target->imports.push_back(item);
  return std::nullopt;
}

// 衝突を全部確かめてから表に足す。途中で失敗しても、この取り込みの一部だけが残らない。
std::optional<Cell> add_all(Impl& machine, std::uint32_t into, const std::vector<Import>& items) {
  Module* target = find_module(machine, into);
  const auto saved = target == nullptr ? std::vector<Import>{} : target->imports;
  for (const Import& item : items) {
    if (const auto formal = add_import(machine, into, item)) {
      if (target != nullptr) target->imports = saved;
      return formal;
    }
  }
  return std::nullopt;
}

}  // namespace

std::optional<Cell> parse_imports(Impl& machine, Cell imports, ImportSpec& spec) {
  imports = machine.heap.deref(imports);
  const bool except = named(machine, imports, "except", 1);
  std::vector<Cell> items;
  if (const auto formal = read_list(machine, except ? machine.heap.arg(imports, 0) : imports, items)) return formal;
  ImportSpec parsed;
  parsed.kind = except ? ImportSpec::Kind::Except : ImportSpec::Kind::Only;
  for (const Cell cell : items) {
    if (except) {
      PredKey key;
      if (const auto formal = read_indicator(machine, cell, key)) return formal;
      parsed.except.push_back(key);
      continue;
    }
    Import item;
    if (const auto formal = read_item(machine, cell, item)) return formal;
    parsed.only.push_back(item);
  }
  spec = std::move(parsed);
  return std::nullopt;
}

std::optional<Cell> apply_imports(Impl& machine, std::uint32_t from, std::uint32_t into, const ImportSpec& spec) {
  if (from == into) return std::nullopt;
  const Module* source = find_module(machine, from);
  if (source == nullptr) return std::nullopt;
  std::vector<Import> items;
  if (spec.kind == ImportSpec::Kind::Only) {
    for (Import item : spec.only) {
      if (!exported(*source, item.name, item.arity)) {
        return formal_existence(machine, "procedure", qualified_pi(machine, from, item.name, item.arity));
      }
      item.from = from;
      items.push_back(item);
    }
    return add_all(machine, into, items);
  }
  for (const PredKey& key : source->exports) {
    if (spec.kind == ImportSpec::Kind::Except && excluded(spec, key)) continue;
    items.push_back(Import{key.name, key.arity, key.name, from});
  }
  if (const auto formal = add_all(machine, into, items)) return formal;
  // 公開の演算子は use_module/1、except、consult で入る。PI のリストでは入らない（iso-notes の決定 A）。
  return import_ops(machine, from, into);
}

std::optional<Cell> import_exports(Impl& machine, std::uint32_t from, std::uint32_t into) {
  return apply_imports(machine, from, into, ImportSpec{});
}

std::optional<Cell> check_local(Impl& machine, std::uint32_t module, std::uint32_t name, std::uint16_t arity) {
  const Module* target = find_module(machine, module);
  if (target == nullptr) return std::nullopt;
  for (const Import& item : target->imports) {
    if (item.alias == name && item.arity == arity) return conflict(machine, item.from, item.name, item.arity);
  }
  return std::nullopt;
}

}
