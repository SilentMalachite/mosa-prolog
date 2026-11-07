// ADR 0022（レビュー M30）。assert した節の共有の項（Clause::shared）のスロットを付け直して表を作る。共有の項を作るのは
// compile.cpp の Build::share_data、組むのは engine.cpp の materialize（SharedMemo）。
// テスト: tests/pl/database.pl の m30_*、tests/cpp/assert_shared_test.cpp
#include "internal.hpp"

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace mosa {
namespace {

// 深い項でも C++ の呼び出しを積まないよう手で積む（ADR 0011）。
template <typename Visit>
void each_var(IrTerm& root, Visit visit) {
  std::vector<IrTerm*> pending{&root};
  while (!pending.empty()) {
    IrTerm& term = *pending.back();
    pending.pop_back();
    if (term.tag == IrTag::Var) visit(term);
    for (IrTerm& arg : term.args) pending.push_back(&arg);
  }
}

}  // namespace

void renumber_shared(IrTerm& head, IrTerm& body, std::vector<SharedTerm>& terms, std::uint32_t base) {
  if (terms.empty()) return;
  static_cast<void>(slot_number(static_cast<std::size_t>(base) + terms.size()));
  const std::uint32_t floor = kProvisionalShared - static_cast<std::uint32_t>(terms.size() - 1);
  const auto fix = [floor, base](IrTerm& var) {
    if (var.slot >= floor && var.slot <= kProvisionalShared) var.slot = base + (kProvisionalShared - var.slot);
  };
  each_var(head, fix);
  each_var(body, fix);
  for (std::size_t index = 0; index < terms.size(); ++index) {
    each_var(terms[index].term, fix);
    terms[index].slot = base + static_cast<std::uint32_t>(index);
  }
}

std::shared_ptr<const SharedTable> make_shared_table(std::vector<SharedTerm> terms, std::uint32_t base) {
  if (terms.empty()) return nullptr;
  auto table = std::make_shared<SharedTable>();
  table->terms = std::move(terms);
  table->base = base;
  return table;
}

// 変数はスロットの順に作る（変数の標準順序は番地で決まり、前と同じ順に保つ）。共有の項は env に置かない。
std::vector<Cell> clause_env(Impl& machine, const Clause& clause) {
  std::vector<Cell> cells;
  cells.reserve(clause.vars);
  for (std::uint32_t index = 0; index < clause.vars; ++index) cells.push_back(machine.heap.fresh_ref());
  return cells;
}

}  // namespace mosa
