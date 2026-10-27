// docs/SPEC.md 第5節
// use_module(+atom)
// use_module(+atom, +list)
//   instantiation_error
//   type_error(atom, Culprit)
//   permission_error(modify, module, Module) は module/2 が読み込みを拒んだとき
// テスト: tests/pl/modules.pl

#include "../machine/internal.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace mosa {

std::filesystem::path canonical_key(const std::filesystem::path& file);

namespace {

struct Wanted {
  std::uint32_t name = 0;
  std::uint16_t arity = 0;
  // 呼び出し側の名前。別名が無いときは name と同じ。
  std::uint32_t local = 0;
};

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

bool module_at_path(const Impl& machine, std::string_view key, std::uint32_t& module) {
  for (const ModuleSource& item : machine.module_sources) {
    if (item.path != key) continue;
    module = item.module;
    return true;
  }
  return false;
}

void import_slot(Impl& machine, std::uint32_t source, std::uint32_t caller, std::uint32_t name,
                 std::uint16_t arity, std::uint32_t local) {
  if (predicate_in(machine, caller, local, arity) != nullptr) return;
  Predicate slot;
  slot.home = caller;
  slot.name = local;
  slot.arity = arity;
  slot.imported = true;
  slot.origin = source;
  slot.origin_name = name;
  machine.predicates.push_back(std::move(slot));
}

void import_exports(Impl& machine, std::uint32_t source, std::uint32_t caller) {
  for (const ExportMark& item : machine.exports) {
    if (item.module != source) continue;
    import_slot(machine, source, caller, item.name, item.arity, item.name);
  }
}

bool is_export(const Impl& machine, std::uint32_t source, const Wanted& item) {
  for (const ExportMark& mark : machine.exports) {
    if (mark.module == source && mark.name == item.name && mark.arity == item.arity) return true;
  }
  return false;
}

// リストに無い公開述語は席を作らない。呼び出し側からは見えない。
void import_listed(Impl& machine, std::uint32_t source, std::uint32_t caller, const std::vector<Wanted>& wanted) {
  for (const Wanted& item : wanted) {
    if (!is_export(machine, source, item)) continue;
    import_slot(machine, source, caller, item.name, item.arity, item.local);
  }
}

bool indicator(const Impl& machine, Cell cell, Wanted& item) {
  cell = machine.heap.deref(cell);
  if (cell.tag() != Tag::Str || machine.heap.functor_arity(cell) != 2) return false;
  if (machine.atoms.name(machine.heap.functor_atom(cell)) != "/") return false;
  const Cell left = machine.heap.deref(machine.heap.arg(cell, 0));
  const Cell right = machine.heap.deref(machine.heap.arg(cell, 1));
  if (left.tag() != Tag::Atom) return false;
  if (right.tag() != Tag::Int && right.tag() != Tag::IntBox) return false;
  const auto value = machine.heap.integer_value(right);
  if (value < 0 || value > 1024) return false;
  item.name = static_cast<std::uint32_t>(left.raw() >> 3);
  item.arity = static_cast<std::uint16_t>(value);
  return true;
}

bool nil_atom(const Impl& machine, Cell cell) {
  cell = machine.heap.deref(cell);
  if (cell.tag() != Tag::Atom) return false;
  return machine.atoms.name(static_cast<std::uint32_t>(cell.raw() >> 3)) == "[]";
}

bool wanted_item(const Impl& machine, Cell cell, Wanted& item) {
  cell = machine.heap.deref(cell);
  if (cell.tag() == Tag::Str && machine.heap.functor_arity(cell) == 2 &&
      machine.atoms.name(machine.heap.functor_atom(cell)) == "as") {
    const Cell alias = machine.heap.deref(machine.heap.arg(cell, 1));
    if (alias.tag() != Tag::Atom) return false;
    if (!indicator(machine, machine.heap.arg(cell, 0), item)) return false;
    item.local = static_cast<std::uint32_t>(alias.raw() >> 3);
    return true;
  }
  if (!indicator(machine, cell, item)) return false;
  item.local = item.name;
  return true;
}

bool read_wanted(const Impl& machine, Cell list, std::vector<Wanted>& out) {
  list = machine.heap.deref(list);
  if (nil_atom(machine, list)) return true;
  while (list.tag() == Tag::Lis) {
    Wanted item;
    if (!wanted_item(machine, machine.heap.list_head(list), item)) return false;
    out.push_back(item);
    list = machine.heap.deref(machine.heap.list_tail(list));
  }
  return nil_atom(machine, list);
}

bool raise_module_fault(Impl& machine, const Goal& goal) {
  if (machine.module_fault == 0) return false;
  const auto name = machine.module_fault;
  machine.module_fault = 0;
  const Cell culprit = machine.heap.atom(name);
  raise_formal(machine, formal_permission(machine, "modify", "module", culprit), goal.name, goal.arity);
  return true;
}

bool path_text(Impl& machine, const Goal& goal, std::string& text) {
  const Cell path = machine.heap.deref(goal.arg(machine, 0));
  if (machine.heap.unbound(path)) {
    raise_formal(machine, formal_instantiation(machine), goal.name, goal.arity);
    return false;
  }
  if (path.tag() != Tag::Atom) {
    raise_formal(machine, formal_type(machine, "atom", path), goal.name, goal.arity);
    return false;
  }
  // intern がアトム表を伸ばすと、名前の string_view は無効になる。
  text = std::string{machine.atoms.name(static_cast<std::uint32_t>(path.raw() >> 3))};
  return true;
}

bool resolve_module(Impl& machine, const Goal& goal, std::string_view text, std::uint32_t& loaded) {
  const auto key = canonical_key(std::filesystem::path{std::string{text}}).generic_string();
  // 定義ファイルは読み直さない。公開リストは最初の module/2 のまま取り込む。
  if (module_at_path(machine, key, loaded)) return true;
  std::size_t before = 0;
  if (!open_module(machine, text, before)) {
    if (!raise_module_fault(machine, goal)) machine.failed = true;
    return false;
  }
  if (machine.modules.size() <= before) {
    machine.failed = true;
    return false;
  }
  loaded = machine.modules.back();
  return true;
}

}  // namespace

bool run_use_module(Impl& machine, const Goal& goal) {
  if (machine.atoms.name(goal.name) != "use_module" || (goal.arity != 1 && goal.arity != 2)) return false;
  std::string text;
  if (!path_text(machine, goal, text)) return true;
  std::vector<Wanted> wanted;
  // 名前は読み込みの前に控える。失敗した読み込みはヒープを切り詰める。
  if (goal.arity == 2 && !read_wanted(machine, goal.arg(machine, 1), wanted)) {
    machine.failed = true;
    return true;
  }
  std::uint32_t loaded = 0;
  if (!resolve_module(machine, goal, text, loaded)) return true;
  if (goal.arity == 2) import_listed(machine, loaded, machine.calling, wanted);
  else import_exports(machine, loaded, machine.calling);
  machine.diagnostic.clear();
  machine.ip = goal.cont;
  return true;
}

}  // namespace mosa
