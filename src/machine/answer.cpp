#include "internal.hpp"

#include "mosa/writer.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
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

// 答えの「名前 = 右辺」の 1 つ。value が真なら右辺は値を書いた字面（Picked::values の次の項）、偽なら名前 alias。
struct Piece {
  std::string_view name;
  std::string_view alias;
  bool value = false;
};

// 選んだ束縛と、書く値。値は write_terms で 1 つの番号付けで書く（レビュー M22）。
struct Picked {
  std::vector<Piece> pieces;
  std::vector<Cell> values;
};

bool own_variable(std::span<const Binding> bindings, Cell variable) {
  return std::any_of(bindings.begin(), bindings.end(),
                     [variable](const Binding& binding) { return binding.variable.raw() == variable.raw(); });
}

// ADR 0019。値が同じ未束縛の変数である名前の組は、鎖 N1 = N2, … で示す。前の束縛に出たかによらず示さないと、
// 名前が消える。問い合わせの変数そのものでない変数は、ほかの値の中で組の最後の名前で書くよう names に足す。
// 名前が 1 つの組は前と同じく、問い合わせの変数そのものが前の束縛に出ていれば書かない。作った名前で出た変数は、
// どの名前の変数か字面から分からないので必ず書く。
Picked pick_bindings(const Heap& heap, std::span<const Binding> bindings, std::vector<VarName>& names) {
  std::vector<Cell> values;
  values.reserve(bindings.size());
  std::unordered_map<std::uint64_t, std::vector<std::size_t>> classes;
  for (std::size_t at = 0; at < bindings.size(); ++at) {
    values.push_back(heap.deref(bindings[at].variable));
    if (heap.unbound(values.back())) classes[values.back().raw()].push_back(at);
  }
  Picked picked;
  std::vector<std::uint64_t> shown;
  for (std::size_t at = 0; at < bindings.size(); ++at) {
    const Cell value = values[at];
    if (heap.unbound(value)) {
      const std::vector<std::size_t>& members = classes[value.raw()];
      const bool own = own_variable(bindings, value);
      if (members.size() > 1) {
        if (!own && at == members.front()) names.push_back(VarName{bindings[members.back()].name, value});
        const auto self = std::find(members.begin(), members.end(), at);
        if (self + 1 != members.end()) {
          picked.pieces.push_back(Piece{bindings[at].name, bindings[*(self + 1)].name, false});
        }
        continue;
      }
      if (own && already(shown, value.raw())) continue;
    }
    picked.pieces.push_back(Piece{bindings[at].name, {}, true});
    picked.values.push_back(value);
    note_shown(heap, value, names, shown);
  }
  return picked;
}

}  // namespace

std::string format_answer(const Machine& machine, const Stop& stop) {
  if (stop.kind == StopKind::Failure) return "false";
  if (stop.kind != StopKind::Success || stop.bindings.empty()) return "true";
  std::vector<VarName> names;
  names.reserve(stop.bindings.size());
  for (const Binding& binding : stop.bindings) names.push_back(VarName{binding.name, binding.variable});
  const Picked picked = pick_bindings(machine.heap(), stop.bindings, names);
  // 答えは利用者が同じ旗のまま貼り直せるように書く（レビュー M8）。
  WriteOptions options;
  options.fullwidth = engine_impl(machine).japanese_punct;
  // 束縛ごとに write_term を呼ぶと、作る名前が束縛ごとに _G1 から始まり、違う変数が同じ名前になる（レビュー M22）。
  const std::vector<std::string> texts =
      write_terms(machine.heap(), machine.operators(), picked.values, options, names);
  std::string text;
  std::size_t written = 0;
  for (const Piece& piece : picked.pieces) {
    if (!text.empty()) text += ", ";
    text += piece.name;
    text += " = ";
    if (piece.value) text += texts[written++];
    else text += piece.alias;
  }
  if (text.empty()) return "true";
  return text;
}

}
