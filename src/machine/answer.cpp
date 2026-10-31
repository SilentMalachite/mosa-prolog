#include "internal.hpp"

#include "mosa/writer.hpp"

#include <algorithm>
#include <cstdint>
#include <span>
#include <string>
#include <unordered_set>
#include <vector>

namespace mosa {
namespace {

// 答えに出た変数を shown に集める。C++ の再帰を使わず手で積む（ADR 0011）。seen は辿った複合項で、
// 共有された部分項と循環を 1 度だけ辿る。
void note_shown(const Heap& heap, Cell root, std::span<const VarName> names, std::vector<std::uint64_t>& shown) {
  std::unordered_set<std::uint64_t> seen;
  std::vector<Cell> todo{root};
  while (!todo.empty()) {
    Cell term = todo.back();
    todo.pop_back();
    if (term.tag() == Tag::Ref) {
      const bool named = std::any_of(names.begin(), names.end(),
                                     [term](const VarName& name) { return name.cell.raw() == term.raw(); });
      if (named) {
        shown.push_back(heap.deref(term).raw());
        continue;
      }
    }
    term = heap.deref(term);
    if (heap.unbound(term)) {
      shown.push_back(term.raw());
      continue;
    }
    if (term.tag() != Tag::Lis && term.tag() != Tag::Str) continue;
    if (!seen.insert(term.raw()).second) continue;
    if (term.tag() == Tag::Lis) {
      todo.push_back(heap.list_tail(term));
      todo.push_back(heap.list_head(term));
      continue;
    }
    for (std::uint16_t index = heap.functor_arity(term); index > 0; --index) todo.push_back(heap.arg(term, index - 1U));
  }
}

bool already(const std::vector<std::uint64_t>& shown, std::uint64_t raw) {
  for (const auto item : shown) {
    if (item == raw) return true;
  }
  return false;
}

}  // namespace

std::string format_answer(const Machine& machine, const Stop& stop) {
  if (stop.kind == StopKind::Failure) return "false";
  if (stop.kind != StopKind::Success || stop.bindings.empty()) return "true";
  std::vector<VarName> names;
  names.reserve(stop.bindings.size());
  for (const Binding& binding : stop.bindings) names.push_back(VarName{binding.name, binding.variable});
  std::vector<std::uint64_t> shown;
  std::string text;
  for (const Binding& binding : stop.bindings) {
    const Cell value = machine.heap().deref(binding.variable);
    if (machine.heap().unbound(value) && already(shown, value.raw())) continue;
    if (!text.empty()) text += ", ";
    text += binding.name;
    text += " = ";
    text += write_term(machine.heap(), machine.operators(), value, {}, names);
    note_shown(machine.heap(), value, names, shown);
  }
  if (text.empty()) return "true";
  return text;
}

}
