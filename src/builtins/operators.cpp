// 規格 8.14.3 op/3。
// op(+integer, +operator_specifier, +atom_or_atom_list)
// 具体化、型、ドメイン、許可の順。名前のリストは全部を確かめてから表を替える。
// 拒否のときは表を変えない。同じ類の指定子は define が置き換える。
// 優先度 0 はその指定子だけを消す。| の優先度 0 も削除として成功する。
// instantiation_error, type_error(integer), type_error(atom), type_error(list),
// domain_error(operator_priority), domain_error(operator_specifier),
// permission_error(modify, operator), permission_error(create, operator)
// テスト: tests/pl/operators.pl, tests/pl/japanese.pl

#include "../machine/internal.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mosa {
namespace {

struct OpName {
  Cell cell;
  std::string text;
};

bool is_int(Cell cell) { return cell.tag() == Tag::Int || cell.tag() == Tag::IntBox; }

std::string_view atom_text(const Impl& machine, Cell cell) {
  return machine.atoms.name(static_cast<std::uint32_t>(cell.raw() >> 3));
}

std::optional<Specifier> specifier_of(std::string_view text) {
  if (text == "xf") return Specifier::Xf;
  if (text == "yf") return Specifier::Yf;
  if (text == "xfx") return Specifier::Xfx;
  if (text == "xfy") return Specifier::Xfy;
  if (text == "yfx") return Specifier::Yfx;
  if (text == "fx") return Specifier::Fx;
  if (text == "fy") return Specifier::Fy;
  return std::nullopt;
}

void raise_inst(Impl& machine, const Goal& goal) {
  raise_formal(machine, formal_instantiation(machine), goal.name, goal.arity);
}

void raise_type(Impl& machine, const Goal& goal, std::string_view type, Cell culprit) {
  raise_formal(machine, formal_type(machine, type, culprit), goal.name, goal.arity);
}

void raise_domain(Impl& machine, const Goal& goal, std::string_view domain, Cell culprit) {
  raise_formal(machine, formal_domain(machine, domain, culprit), goal.name, goal.arity);
}

void raise_permit(Impl& machine, const Goal& goal, std::string_view action, Cell culprit) {
  raise_formal(machine, formal_permission(machine, action, "operator", culprit), goal.name, goal.arity);
}

bool name_list_unbound(const Impl& machine, Cell names) {
  if (names.tag() == Tag::Atom) return false;
  const WalkedList walked = walk_list(machine, names);
  if (walked.shape == ListShape::Partial) return true;
  if (walked.shape != ListShape::Proper) return false;
  for (const Cell item : walked.items) {
    if (machine.heap.unbound(item)) return true;
  }
  return false;
}

bool reject_types(Impl& machine, const Goal& goal, Cell priority, Cell specifier, Cell names,
                  WalkedList& walked) {
  if (!is_int(priority)) {
    raise_type(machine, goal, "integer", priority);
    return true;
  }
  if (specifier.tag() != Tag::Atom) {
    raise_type(machine, goal, "atom", specifier);
    return true;
  }
  if (names.tag() == Tag::Atom) return false;
  walked = walk_list(machine, names);
  if (walked.shape != ListShape::Proper) {
    raise_type(machine, goal, "list", names);
    return true;
  }
  for (const Cell item : walked.items) {
    if (item.tag() != Tag::Atom) {
      raise_type(machine, goal, "atom", item);
      return true;
    }
  }
  return false;
}

bool reject_domain(Impl& machine, const Goal& goal, Cell priority, Cell specifier, Specifier& spec) {
  const auto value = machine.heap.integer_value(priority);
  if (value < 0 || value > 1200) {
    raise_domain(machine, goal, "operator_priority", priority);
    return true;
  }
  const std::optional<Specifier> parsed = specifier_of(atom_text(machine, specifier));
  if (!parsed) {
    raise_domain(machine, goal, "operator_specifier", specifier);
    return true;
  }
  spec = *parsed;
  return false;
}

std::vector<OpName> op_names(const Impl& machine, Cell names, const WalkedList& walked) {
  std::vector<OpName> found;
  if (names.tag() == Tag::Atom) {
    found.push_back(OpName{names, std::string(atom_text(machine, names))});
    return found;
  }
  for (const Cell item : walked.items) {
    found.push_back(OpName{item, std::string(atom_text(machine, item))});
  }
  return found;
}

bool commit_ops(Impl& machine, const Goal& goal, int priority, Specifier spec,
                const std::vector<OpName>& names) {
  OperatorTable trial = machine.operators;
  for (const OpName& name : names) {
    const OpResult result = trial.define(priority, spec, name.text);
    if (result == OpResult::Ok) continue;
    raise_permit(machine, goal, result == OpResult::ModifyComma ? "modify" : "create", name.cell);
    return false;
  }
  machine.operators = std::move(trial);
  return true;
}

void run_op(Impl& machine, const Goal& goal) {
  const Cell priority = machine.heap.deref(goal.arg(machine, 0));
  const Cell specifier = machine.heap.deref(goal.arg(machine, 1));
  const Cell names = machine.heap.deref(goal.arg(machine, 2));
  if (machine.heap.unbound(priority) || machine.heap.unbound(specifier) || machine.heap.unbound(names) ||
      name_list_unbound(machine, names)) {
    raise_inst(machine, goal);
    return;
  }
  WalkedList walked;
  if (reject_types(machine, goal, priority, specifier, names, walked)) return;
  Specifier spec = Specifier::Xfx;
  if (reject_domain(machine, goal, priority, specifier, spec)) return;
  const auto priority_value = machine.heap.integer_value(priority);
  if (!commit_ops(machine, goal, static_cast<int>(priority_value), spec, op_names(machine, names, walked))) {
    return;
  }
  machine.ip = goal.cont;
}

}  // namespace

bool run_operators(Impl& machine, const Goal& goal) {
  if (machine.atoms.name(goal.name) != "op" || goal.arity != 3) return false;
  run_op(machine, goal);
  return true;
}

}  // namespace mosa
