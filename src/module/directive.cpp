// docs/SPEC.md 第5節
// module(+atom, +list) は指示。述語ではない。
//   permission_error(modify, module, Module)
// テスト: tests/pl/modules.pl

#include "../machine/internal.hpp"

#include "mosa/writer.hpp"

#include <algorithm>

namespace mosa {
namespace {

bool module_name(Impl& machine, Cell directive, std::uint32_t& name, Cell& listed) {
  directive = machine.heap.deref(directive);
  if (!is_directive(machine.heap, directive)) return false;
  const Cell body = machine.heap.deref(machine.heap.arg(directive, 0));
  if (body.tag() != Tag::Str || machine.heap.functor_arity(body) != 2) return false;
  if (machine.atoms.name(machine.heap.functor_atom(body)) != "module") return false;
  const Cell module = machine.heap.deref(machine.heap.arg(body, 0));
  if (module.tag() != Tag::Atom) return false;
  name = static_cast<std::uint32_t>(module.raw() >> 3);
  listed = machine.heap.deref(machine.heap.arg(body, 1));
  return true;
}

bool export_indicator(const Impl& machine, Cell cell, std::uint32_t& name, std::uint16_t& arity) {
  cell = machine.heap.deref(cell);
  if (cell.tag() != Tag::Str || machine.heap.functor_arity(cell) != 2) return false;
  if (machine.atoms.name(machine.heap.functor_atom(cell)) != "/") return false;
  const Cell left = machine.heap.deref(machine.heap.arg(cell, 0));
  const Cell right = machine.heap.deref(machine.heap.arg(cell, 1));
  if (left.tag() != Tag::Atom) return false;
  if (right.tag() != Tag::Int && right.tag() != Tag::IntBox) return false;
  const auto value = machine.heap.integer_value(right);
  if (value < 0 || value > 1024) return false;
  name = static_cast<std::uint32_t>(left.raw() >> 3);
  arity = static_cast<std::uint16_t>(value);
  return true;
}

bool read_exports(Impl& machine, std::uint32_t module, Cell list, std::vector<ExportMark>& out,
                  std::vector<ExportedOp>& ops) {
  list = machine.heap.deref(list);
  const auto nil = machine.atoms.intern("[]");
  if (list.tag() == Tag::Atom && static_cast<std::uint32_t>(list.raw() >> 3) == nil) return true;
  while (list.tag() == Tag::Lis) {
    const Cell head = machine.heap.list_head(list);
    ExportMark mark;
    mark.module = module;
    ExportedOp op;
    if (export_indicator(machine, head, mark.name, mark.arity)) {
      out.push_back(mark);
    } else if (take_exported_op(machine, head, op)) {
      op.module = module;
      ops.push_back(std::move(op));
    } else {
      return false;
    }
    list = machine.heap.deref(machine.heap.list_tail(list));
  }
  return list.tag() == Tag::Atom && static_cast<std::uint32_t>(list.raw() >> 3) == nil;
}

bool module_known(const Impl& machine, std::uint32_t name) {
  return std::find(machine.modules.begin(), machine.modules.end(), name) != machine.modules.end();
}

void remember_module(Impl& machine, std::uint32_t name) {
  if (module_known(machine, name)) return;
  machine.modules.push_back(name);
  if (!machine.module_added.empty()) machine.module_added.back().push_back(name);
}

bool export_body(const Impl& machine, Cell term, Cell& spec) {
  term = machine.heap.deref(term);
  if (!is_directive(machine.heap, term)) return false;
  const Cell head = machine.heap.deref(machine.heap.arg(term, 0));
  if (head.tag() != Tag::Str || machine.heap.functor_arity(head) != 1) return false;
  if (machine.atoms.name(machine.heap.functor_atom(head)) != "export") return false;
  spec = machine.heap.deref(machine.heap.arg(head, 0));
  return true;
}

int apply_export_directive(Impl& machine, Cell term) {
  Cell spec{};
  if (!export_body(machine, term, spec)) return 0;
  std::uint32_t name = 0;
  std::uint16_t arity = 0;
  if (!export_indicator(machine, spec, name, arity)) {
    machine.diagnostic = "述語指示子ではありません";
    machine.syntax_diag = false;
    return -1;
  }
  // 読み込み中のモジュールの公開リストへ足す。呼び出し元ではない。
  note_export(machine, machine.load_home, name, arity);
  return 1;
}

}  // namespace

void drop_load_modules(Impl& machine) {
  if (machine.module_added.empty()) return;
  for (const std::uint32_t name : machine.module_added.back()) {
    std::vector<std::uint32_t>& mods = machine.modules;
    mods.erase(std::remove(mods.begin(), mods.end(), name), mods.end());
    std::erase_if(machine.exports, [name](const ExportMark& item) { return item.module == name; });
    std::erase_if(machine.exported_ops, [name](const ExportedOp& item) { return item.module == name; });
    std::erase_if(machine.module_ops, [name](const ModuleOps& item) { return item.home == name; });
    std::erase_if(machine.module_sources, [name](const ModuleSource& item) { return item.module == name; });
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
  Cell listed{};
  if (!module_name(machine, term, name, listed)) return 0;
  // 第5節は1ファイル1モジュール。最初の項でない名前も、既にある名前も残さない。
  // 同じファイルの読み直しは、まだこの検査と分けていない。
  if (machine.load_terms != 1 || module_known(machine, name)) {
    reject_module(machine, name);
    return -1;
  }
  std::vector<ExportMark> listed_exports;
  std::vector<ExportedOp> listed_ops;
  if (!read_exports(machine, name, listed, listed_exports, listed_ops)) {
    machine.diagnostic = "エクスポートが述語指示ではない";
    machine.syntax_diag = false;
    return -1;
  }
  machine.load_home = name;
  remember_module(machine, name);
  machine.exports.insert(machine.exports.end(), listed_exports.begin(), listed_exports.end());
  machine.exported_ops.insert(machine.exported_ops.end(), listed_ops.begin(), listed_ops.end());
  if (!machine.load_origin.empty()) machine.module_sources.push_back(ModuleSource{name, machine.load_origin});
  return 1;
}

int apply_module_load_directive(Impl& machine, Cell term) {
  const int meta = apply_meta_directive(machine, term);
  if (meta != 0) return meta;
  const int exported = apply_export_directive(machine, term);
  if (exported != 0) return exported;
  return apply_use_directive(machine, term);
}

}  // namespace mosa
