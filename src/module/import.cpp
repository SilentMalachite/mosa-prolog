// use_module/2 の取り込み指定と、取り込みの表への登録（SPEC 第5節、ADR 0006）。reexport/1,2 の公開の項目（ADR 0026）。
// 13211-1 に reexport の条項は無い。13211-2 での位置は未確認。
// モード: use_module(+atom)、use_module(+atom, +list)、reexport(+atom)、reexport(+atom, +list)（読み込みの指令。
//         ファイルを開くのは load.cpp）。
// エラー: instantiation_error、type_error(list, L)、type_error(predicate_indicator, PI)、type_error(atom, Alias)、
//         permission_error(import_into, procedure, From:N/A)、existence_error(procedure, M:N/A)。
// テスト: tests/pl/modules.pl、tests/pl/japanese.pl、tests/cpp/module_load_test.cpp
#include "module.hpp"

#include "../machine/internal.hpp"

#include <algorithm>
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

const Export* exported(const Module& module, std::uint32_t name, std::uint16_t arity) {
  for (const Export& entry : module.exports) {
    if (entry.name == name && entry.arity == arity) return &entry;
  }
  return nullptr;
}

bool excluded(const ImportSpec& spec, const Export& entry) {
  for (const PredKey& item : spec.except) {
    if (item.name == entry.name && item.arity == entry.arity) return true;
  }
  return false;
}

// spec の取り込みを、公開の項目の定義したモジュールとそこでの名前で作る（ADR 0026）。だから resolve_pred は再公開を
// 何段重ねても 1 回で定義に着き、add_import は取り込み元を定義したモジュールで比べる。
std::optional<Cell> spec_items(Impl& machine, const Module& source, const ImportSpec& spec,
                               std::vector<Import>& items) {
  if (spec.kind == ImportSpec::Kind::Only) {
    for (const Import& wanted : spec.only) {
      const Export* entry = exported(source, wanted.name, wanted.arity);
      if (entry == nullptr) {
        return formal_existence(machine, "procedure", qualified_pi(machine, source.name, wanted.name, wanted.arity));
      }
      items.push_back(Import{entry->from_name, entry->arity, wanted.alias, entry->from});
    }
    return std::nullopt;
  }
  for (const Export& entry : source.exports) {
    if (spec.kind == ImportSpec::Kind::Except && excluded(spec, entry)) continue;
    items.push_back(Import{entry.from_name, entry.arity, entry.name, entry.from});
  }
  return std::nullopt;
}

// 同じ名前と項数の公開は置き換える。module/2 の一覧や前の reexport の項目より、後の reexport が指す定義を公開する。
void put_export(Module& module, const Export& entry) {
  for (Export& old : module.exports) {
    if (old.name != entry.name || old.arity != entry.arity) continue;
    old = entry;
    return;
  }
  module.exports.push_back(entry);
}

bool same_op(const OpDef& left, const OpDef& right) {
  return left.priority == right.priority && left.specifier == right.specifier && left.name == right.name;
}

std::optional<Cell> add_import(Impl& machine, std::uint32_t into, const Import& item) {
  // 公開の項目は定義したモジュールを指すので、再公開の輪や再公開した側からの取り込みでは取り込み元が into 自身になる。
  // 同じ名前なら into の定義そのものであり、同じ定義に着く取り込みは衝突でない（ADR 0026）。表に入れずに捨てる。
  // 入れると、節が先なら上の find_pred が、節が後なら check_local が断り、結果が読む順で変わる。別名なら残す。
  if (item.from == into && item.name == item.alias) return std::nullopt;
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
  if (const auto formal = spec_items(machine, *source, spec, items)) return formal;
  if (const auto formal = add_all(machine, into, items)) return formal;
  if (spec.kind == ImportSpec::Kind::Only) return std::nullopt;
  // 公開の演算子は use_module/1、except、consult で入る。PI のリストでは入らない（iso-notes の決定 A）。
  return import_ops(machine, from, into);
}

std::optional<Cell> import_exports(Impl& machine, std::uint32_t from, std::uint32_t into) {
  return apply_imports(machine, from, into, ImportSpec{});
}

std::optional<Cell> add_reexports(Impl& machine, std::uint32_t from, std::uint32_t into, const ImportSpec& spec) {
  // user には公開の一覧が無い。モジュールでないテキストの reexport は use_module と同じになる（SPEC 第5節）。
  if (into == machine.user_module || from == into) return std::nullopt;
  const Module* source = find_module(machine, from);
  Module* target = find_module(machine, into);
  if (source == nullptr || target == nullptr) return std::nullopt;
  std::vector<Import> items;
  if (const auto formal = spec_items(machine, *source, spec, items)) return formal;
  for (const Import& item : items) put_export(*target, Export{item.alias, item.arity, item.from, item.name});
  // 演算子は取り込みと同じ規則で公開する。PI のリストでは公開しない。
  if (spec.kind == ImportSpec::Kind::Only) return std::nullopt;
  for (const OpDef& op : source->export_ops) {
    const auto known = [&op](const OpDef& old) { return same_op(old, op); };
    if (std::ranges::none_of(target->export_ops, known)) target->export_ops.push_back(op);
  }
  return std::nullopt;
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
