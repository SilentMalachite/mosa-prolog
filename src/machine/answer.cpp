#include "internal.hpp"

#include "mosa/writer.hpp"

#include <span>
#include <string>
#include <vector>

namespace mosa {
namespace {

void note_shown(const Heap& heap, Cell term, std::span<const VarName> names, std::vector<std::uint64_t>& shown,
                std::vector<std::uint64_t>& seen) {
  if (term.tag() == Tag::Ref) {
    for (const VarName& name : names) {
      if (name.cell.raw() != term.raw()) continue;
      const Cell value = heap.deref(term);
      shown.push_back(value.raw());
      return;
    }
  }
  term = heap.deref(term);
  if (heap.unbound(term)) {
    shown.push_back(term.raw());
    return;
  }
  if (term.tag() != Tag::Lis && term.tag() != Tag::Str) return;
  for (const auto raw : seen) {
    if (raw == term.raw()) return;
  }
  seen.push_back(term.raw());
  if (term.tag() == Tag::Lis) {
    note_shown(heap, heap.list_head(term), names, shown, seen);
    note_shown(heap, heap.list_tail(term), names, shown, seen);
    return;
  }
  for (std::uint16_t index = 0; index < heap.functor_arity(term); ++index) {
    note_shown(heap, heap.arg(term, index), names, shown, seen);
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
    std::vector<std::uint64_t> seen;
    note_shown(machine.heap(), value, names, shown, seen);
  }
  if (text.empty()) return "true";
  return text;
}

}
