// 試験器だけが、テストの機械の user へ 6 ライブラリを取り込む（iso-notes Section 5 Autoload の決定 A）。
// mosa の user は変えない。Logtalk のスイートはこれらを組込として呼ぶので、取り込まないと存在エラーで落ちる。

#include "lgt_io.hpp"

#include "internal.hpp"

#include "mosa/writer.hpp"
#include "mosa/heap_option.hpp"

#include <array>
#include <string>
#include <string_view>

namespace mosa {
namespace {

constexpr std::array<std::string_view, 6> suite_libraries{"lists", "apply", "between", "format", "strings", "japanese"};

// use_module/2 は指令でしか使えないので、クエリではなく読み込みの指令として渡す。
bool directive_ok(Machine& machine, const std::string& directive) {
  try {
    return machine.load_program(":- " + directive + ".\n").ok;
  } catch (...) {
    if (!is_heap_exhaustion()) throw;
    return false;
  }
}

// tests.lgt や隣の Prolog が同じ名前を定義していると、取り込みは import_into の衝突になる。
// 衝突の判定と同じく user の述語表にある名前を except に入れ、テスト側の定義を残す。
Cell local_names(Impl& impl, const Module& library) {
  Heap& heap = impl.heap;
  Cell list = heap.atom(impl.atoms.intern("[]"));
  for (auto key = library.exports.rbegin(); key != library.exports.rend(); ++key) {
    if (find_pred(impl, PredKey{impl.user_module, key->name, key->arity}) == nullptr) continue;
    const std::array<Cell, 2> parts{heap.atom(key->name), heap.integer(key->arity)};
    list = heap.list(heap.structure(impl.atoms.intern("/"), parts), list);
  }
  return list;
}

void import_library(Machine& machine, std::string_view name) {
  const std::string library = "library(" + std::string{name} + ")";
  // 先に何も取り込まずに読み、公開リストを見てから except 付きで取り込む。
  if (!directive_ok(machine, "use_module(" + library + ", [])")) return;
  Impl& impl = engine_impl(machine);
  const Module* module = find_module(impl, impl.atoms.intern(name));
  if (module == nullptr) return;
  const Cell except = local_names(impl, *module);
  const std::string text = write_term(machine.heap(), machine.operators(), except, WriteOptions{}, {});
  // 失敗しても他のライブラリは続ける。呼ぶテストが存在エラーで落ちる。
  if (!directive_ok(machine, "use_module(" + library + ", except(" + text + "))")) return;
}

}  // namespace

void lgt_import_libraries(Machine& machine) {
  for (const std::string_view name : suite_libraries) import_library(machine, name);
}

}  // namespace mosa
