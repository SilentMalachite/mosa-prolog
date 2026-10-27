// docs/SPEC.md 第5節
// use_module(+atom)
// use_module(library(+atom)) は $MOSA_LIB、次に実行ファイルの隣の lib/
// use_module(+atom, +list)
// use_module(+atom, except(+list))
//   instantiation_error
//   type_error(atom, Culprit)
//   permission_error(modify, module, Module) は module/2 が読み込みを拒んだとき
//   permission_error(import_into, procedure, PI) は同じ名前を二箇所から取り込むとき
// :- use_module は読み込み中のモジュールへ席を作る。
// テスト: tests/pl/modules.pl、tests/pl/morning.pl

#include "../machine/internal.hpp"
#include "../platform/executable.hpp"

#include <cstdlib>
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

struct SlotPlan {
  std::uint32_t name = 0;
  std::uint16_t arity = 0;
  std::uint32_t local = 0;
};

void collect_exports(const Impl& machine, std::uint32_t source, std::vector<SlotPlan>& out) {
  for (const ExportMark& item : machine.exports) {
    if (item.module != source) continue;
    out.push_back(SlotPlan{item.name, item.arity, item.name});
  }
}

// 同じ出所の同じ名前は取り込み直しではない。別の席があるときだけ衝突する。
bool import_clash(Impl& machine, std::uint32_t source, std::uint32_t caller, const SlotPlan& plan) {
  const Predicate* slot = predicate_in(machine, caller, plan.local, plan.arity);
  if (slot == nullptr) return false;
  if (!slot->imported || slot->origin != source) return true;
  const auto source_name = slot->origin_name != 0 ? slot->origin_name : slot->name;
  return source_name != plan.name;
}

bool commit_slots(Impl& machine, const Goal& goal, std::uint32_t source, std::uint32_t caller,
                  const std::vector<SlotPlan>& plans) {
  for (const SlotPlan& plan : plans) {
    if (!import_clash(machine, source, caller, plan)) continue;
    // 一つでも衝突したら、この呼び出しが足す席は一つも作らない。
    const Cell pi = cell_functor(machine, plan.local, plan.arity);
    raise_formal(machine, formal_permission(machine, "import_into", "procedure", pi), goal.name, goal.arity);
    return false;
  }
  for (const SlotPlan& plan : plans) {
    import_slot(machine, source, caller, plan.name, plan.arity, plan.local);
  }
  return true;
}

bool import_exports(Impl& machine, const Goal& goal, std::uint32_t source, std::uint32_t caller) {
  std::vector<SlotPlan> plans;
  collect_exports(machine, source, plans);
  return commit_slots(machine, goal, source, caller, plans);
}

bool is_export(const Impl& machine, std::uint32_t source, const Wanted& item) {
  for (const ExportMark& mark : machine.exports) {
    if (mark.module == source && mark.name == item.name && mark.arity == item.arity) return true;
  }
  return false;
}

// リストに無い公開述語は席を作らない。呼び出し側からは見えない。
bool import_listed(Impl& machine, const Goal& goal, std::uint32_t source, std::uint32_t caller,
                   const std::vector<Wanted>& wanted) {
  std::vector<SlotPlan> plans;
  for (const Wanted& item : wanted) {
    if (!is_export(machine, source, item)) continue;
    plans.push_back(SlotPlan{item.name, item.arity, item.local});
  }
  return commit_slots(machine, goal, source, caller, plans);
}

bool excluded(const std::vector<Wanted>& wanted, std::uint32_t name, std::uint16_t arity) {
  for (const Wanted& item : wanted) {
    if (item.name == name && item.arity == arity) return true;
  }
  return false;
}

// except に書いた公開述語は席を作らない。残りは use_module/1 と同じく取り込む。
bool import_except(Impl& machine, const Goal& goal, std::uint32_t source, std::uint32_t caller,
                   const std::vector<Wanted>& wanted) {
  std::vector<SlotPlan> plans;
  for (const ExportMark& mark : machine.exports) {
    if (mark.module != source || excluded(wanted, mark.name, mark.arity)) continue;
    plans.push_back(SlotPlan{mark.name, mark.arity, mark.name});
  }
  return commit_slots(machine, goal, source, caller, plans);
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

enum class ImportHow { Listed, Except };

struct ImportAsk {
  ImportHow how = ImportHow::Listed;
  std::vector<Wanted> items;
};

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

bool read_ask(const Impl& machine, Cell cell, ImportAsk& ask) {
  cell = machine.heap.deref(cell);
  if (cell.tag() == Tag::Str && machine.heap.functor_arity(cell) == 1 &&
      machine.atoms.name(machine.heap.functor_atom(cell)) == "except") {
    ask.how = ImportHow::Except;
    return read_wanted(machine, machine.heap.arg(cell, 0), ask.items);
  }
  ask.how = ImportHow::Listed;
  return read_wanted(machine, cell, ask.items);
}

bool apply_ask(Impl& machine, const Goal& goal, std::uint32_t source, std::uint32_t caller, const ImportAsk& ask) {
  if (ask.how == ImportHow::Except) return import_except(machine, goal, source, caller, ask.items);
  return import_listed(machine, goal, source, caller, ask.items);
}

bool raise_module_fault(Impl& machine, const Goal& goal) {
  if (machine.module_fault == 0) return false;
  const auto name = machine.module_fault;
  machine.module_fault = 0;
  const Cell culprit = machine.heap.atom(name);
  raise_formal(machine, formal_permission(machine, "modify", "module", culprit), goal.name, goal.arity);
  return true;
}

bool regular_file(const std::filesystem::path& file) {
  std::error_code error;
  return std::filesystem::is_regular_file(file, error) && !error;
}

// 空の MOSA_LIB は未設定と同じ。相対の Name.pl を作業ディレクトリから開かない。
std::filesystem::path library_from_env(const std::filesystem::path& leaf) {
  const char* root = std::getenv("MOSA_LIB");
  if (root == nullptr || root[0] == '\0') return {};
  const auto file = std::filesystem::path{root} / leaf;
  if (!regular_file(file)) return {};
  return file;
}

// 無い名前は実行ファイルの隣のパスのまま開き、consult と同じ失敗にする。
bool library_file(const Impl& machine, Cell cell, std::string& text) {
  cell = machine.heap.deref(cell);
  if (cell.tag() != Tag::Str || machine.heap.functor_arity(cell) != 1) return false;
  if (machine.atoms.name(machine.heap.functor_atom(cell)) != "library") return false;
  const Cell name = machine.heap.deref(machine.heap.arg(cell, 0));
  if (name.tag() != Tag::Atom) return false;
  const auto leaf =
      std::filesystem::path{std::string{machine.atoms.name(static_cast<std::uint32_t>(name.raw() >> 3))} + ".pl"};
  if (const auto from_env = library_from_env(leaf); !from_env.empty()) {
    text = from_env.string();
    return true;
  }
  text = (executable_path().parent_path() / "lib" / leaf).string();
  return true;
}

bool path_text(Impl& machine, const Goal& goal, std::string& text) {
  const Cell path = machine.heap.deref(goal.arg(machine, 0));
  if (machine.heap.unbound(path)) {
    raise_formal(machine, formal_instantiation(machine), goal.name, goal.arity);
    return false;
  }
  if (library_file(machine, path, text)) return true;
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

// 載っているモジュールの原子は、同じ綴りのファイルより先にそのモジュールを指す。
bool loaded_module_atom(const Impl& machine, Cell cell, std::uint32_t& module) {
  cell = machine.heap.deref(cell);
  if (cell.tag() != Tag::Atom) return false;
  module = static_cast<std::uint32_t>(cell.raw() >> 3);
  return module_loaded(machine, module);
}

bool import_found(Impl& machine, const Goal& goal, std::uint32_t loaded, const ImportAsk& ask, bool listed) {
  const bool imported = listed ? apply_ask(machine, goal, loaded, machine.calling, ask)
                               : import_exports(machine, goal, loaded, machine.calling);
  if (!imported) return false;
  machine.diagnostic.clear();
  return true;
}

// 成功だけ true。失敗は failed か例外を残し、ip は触らない。
bool perform_use(Impl& machine, const Goal& goal) {
  ImportAsk ask;
  // 名前は読み込みの前に控える。失敗した読み込みはヒープを切り詰める。
  if (goal.arity == 2 && !read_ask(machine, goal.arg(machine, 1), ask)) {
    machine.failed = true;
    return false;
  }
  std::uint32_t named = 0;
  if (loaded_module_atom(machine, goal.arg(machine, 0), named)) {
    return import_found(machine, goal, named, ask, goal.arity == 2);
  }
  std::string text;
  if (!path_text(machine, goal, text)) return false;
  std::uint32_t loaded = 0;
  if (!resolve_module(machine, goal, text, loaded)) return false;
  return import_found(machine, goal, loaded, ask, goal.arity == 2);
}

bool use_directive_body(const Impl& machine, Cell term, Cell& body) {
  if (!is_directive(machine.heap, term)) return false;
  body = machine.heap.deref(machine.heap.arg(term, 0));
  if (body.tag() != Tag::Str) return false;
  const auto arity = machine.heap.functor_arity(body);
  if (arity != 1 && arity != 2) return false;
  return machine.atoms.name(machine.heap.functor_atom(body)) == "use_module";
}

Goal frame_for_use(Impl& machine, Cell body) {
  const auto arity = machine.heap.functor_arity(body);
  const auto at = machine.heap.allocate_words(kFrameHead + arity);
  machine.heap.set_word(at, machine.heap.functor_atom(body));
  machine.heap.set_word(at + 1, arity);
  machine.heap.set_word(at + 2, 0);
  machine.heap.set_word(at + 3, kNoFrame);
  for (std::uint16_t index = 0; index < arity; ++index) {
    machine.heap.set_word(at + kFrameHead + index, machine.heap.arg(body, index).raw());
  }
  return read_goal(machine, at);
}

}  // namespace

bool run_use_module(Impl& machine, const Goal& goal) {
  if (machine.atoms.name(goal.name) != "use_module" || (goal.arity != 1 && goal.arity != 2)) return false;
  if (perform_use(machine, goal)) machine.ip = goal.cont;
  return true;
}

int apply_use_directive(Impl& machine, Cell term) {
  Cell body;
  if (!use_directive_body(machine, term, body)) return 0;
  const Goal goal = frame_for_use(machine, body);
  // 席は読み込み中のモジュールへ作る。calling のままだと user に入る。
  const auto saved = machine.calling;
  machine.calling = machine.load_home;
  const bool ok = perform_use(machine, goal);
  machine.calling = saved;
  return ok ? 1 : -1;
}

}  // namespace mosa
