// docs/SPEC.md 第5節
// use_module(+atom)
//   instantiation_error
//   type_error(atom, Culprit)
//   permission_error(modify, module, Module) は module/2 が読み込みを拒んだとき
// テスト: tests/pl/modules.pl

#include "../machine/internal.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace mosa {
namespace {

void prefix_diagnostic(Impl& machine, std::string_view path) {
  if (machine.syntax_diag) {
    machine.diagnostic = std::string{path} + ":" + std::to_string(machine.diag_line) + ":" +
                         std::to_string(machine.diag_column) + ": " + machine.diagnostic;
    return;
  }
  if (machine.diagnostic.empty()) return;
  machine.diagnostic = std::string{path} + ": " + machine.diagnostic;
}

bool open_module(Impl& machine, std::string_view text, std::size_t& before) {
  std::ifstream input{std::string{text}, std::ios::binary};
  if (!input) {
    machine.diagnostic = std::string{text} + ": ファイルを開けません";
    machine.syntax_diag = false;
    return false;
  }
  std::ostringstream buffer;
  buffer << input.rdbuf();
  before = machine.modules.size();
  const std::filesystem::path file{std::string{text}};
  const LoadStatus status = load_source(machine, buffer.str(), file.parent_path().string(), file.string());
  if (status.ok) return true;
  prefix_diagnostic(machine, text);
  return false;
}

void import_exports(Impl& machine, std::uint32_t source, std::uint32_t caller) {
  for (const ExportMark& item : machine.exports) {
    if (item.module != source) continue;
    if (predicate_in(machine, caller, item.name, item.arity) != nullptr) continue;
    Predicate slot;
    slot.home = caller;
    slot.name = item.name;
    slot.arity = item.arity;
    slot.imported = true;
    slot.origin = source;
    machine.predicates.push_back(std::move(slot));
  }
}

bool raise_module_fault(Impl& machine, const Goal& goal) {
  if (machine.module_fault == 0) return false;
  const auto name = machine.module_fault;
  machine.module_fault = 0;
  const Cell culprit = machine.heap.atom(name);
  raise_formal(machine, formal_permission(machine, "modify", "module", culprit), goal.name, goal.arity);
  return true;
}

}  // namespace

bool run_use_module(Impl& machine, const Goal& goal) {
  if (machine.atoms.name(goal.name) != "use_module" || goal.arity != 1) return false;
  const Cell path = machine.heap.deref(goal.arg(machine, 0));
  if (machine.heap.unbound(path)) {
    raise_formal(machine, formal_instantiation(machine), goal.name, goal.arity);
    return true;
  }
  if (path.tag() != Tag::Atom) {
    raise_formal(machine, formal_type(machine, "atom", path), goal.name, goal.arity);
    return true;
  }
  const auto text = machine.atoms.name(static_cast<std::uint32_t>(path.raw() >> 3));
  std::size_t before = 0;
  if (!open_module(machine, text, before)) {
    if (!raise_module_fault(machine, goal)) machine.failed = true;
    return true;
  }
  // このファイルが足したモジュールの公開述語だけを、呼び出し側の席にする。
  if (machine.modules.size() <= before) {
    machine.failed = true;
    return true;
  }
  import_exports(machine, machine.modules.back(), machine.calling);
  machine.diagnostic.clear();
  machine.ip = goal.cont;
  return true;
}

}  // namespace mosa
