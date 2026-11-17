// モジュールを見るゴールの組み込み（SPEC 第5節、フェーズ12）。13211-1 に条項は無い。13211-2 での位置は未確認。
// strip_module(?Term, ?Module, ?Plain)
//   Term の頭の M: を、M が原子である間だけ外す。Module は一番内側のその M、M: が無ければ文脈モジュール。Plain は残り。
//   誤りは、外す M: の連なりが巡回するときの resource_error(cyclic_term) だけ（iso-notes 7.3.3 の持ち主の決定 A）。
// context_module(?M)
//   文脈モジュール（ゴール枠のモジュール）。誤りは投げない。
// module_property(?M, ?P)
//   性質は file(F)、exports(L)、exported_operators(L) の順。M が変数なら current_module/1 の順にたどる。
//   性質を持つのは module/2 で作ったモジュールだけで、user と system は持たない。file はファイルから読んだときだけ、
//   exported_operators は公開の演算子があるときだけ。L は公開した順で、再公開の項目も公開した名前で入る。
//   type_error(atom, M)、domain_error(module_property, P)、existence_error(module, M)。この順に確かめる。
// import(+M:PI)
//   M の公開述語 PI を 1 つ、文脈モジュールへ取り込む（use_module/2 の PI のリストの 1 項目と同じ規則。as は受けない）。
//   M: が無ければ文脈モジュール自身からの取り込みで、何も変えずに成功する。M1:M2:PI は内側の M2 から取り込む。
//   instantiation_error、type_error(atom, M)、type_error(predicate_indicator, PI)、existence_error(module, M)、
//   existence_error(procedure, M:N/A)、permission_error(import_into, procedure, From:N/A)。M の形、PI の形、
//   M があるか、取り込めるか、の順に確かめる。M: の連なりが巡回すれば resource_error(cyclic_term)（M の形の検査の後）。
//   PI の誤りは use_module/2 の項目と同じ（read_indicator）で、ほかに type_error(atom, N)、type_error(integer, A)、
//   domain_error(not_less_than_zero, A)、representation_error(max_arity) を投げる。
// module_property/2 は候補を選言にして選択点を残す。ほかは決定的。
// テスト: tests/pl/modules.pl（tests/pl/modules/ctx.pl、tests/pl/modules/imp_ctx.pl）、tests/pl/japanese.pl
#include "../machine/internal.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace mosa {

namespace {

std::uint32_t atom_id(Cell cell) { return static_cast<std::uint32_t>(cell.raw() >> 3); }

bool colon_term(const Impl& machine, Cell term) {
  return term.tag() == Tag::Str && machine.heap.functor_arity(term) == 2 &&
         machine.atoms.name(machine.heap.functor_atom(term)) == ":";
}

// M が変数や数の M:T は外さず、全体を Plain にする。モジュールがあるかどうかは見ない（:/2 と違い、呼ばないので）。
bool atom_qualified(const Impl& machine, Cell term) {
  return colon_term(machine, term) && machine.heap.deref(machine.heap.arg(term, 0)).tag() == Tag::Atom;
}

void raise(Impl& machine, const Goal& goal, Cell formal) { raise_formal(machine, formal, goal.name, goal.arity); }

void finish(Impl& machine, const Goal& goal, bool unified) {
  if (unified) machine.ip = goal.cont;
  else machine.failed = true;
}

// 外側から順に外すので、重なった M: は一番内側が残る。束ねるのは外し終えてから。
void prove_strip_module(Impl& machine, const Goal& goal) {
  Cell term = machine.heap.deref(goal.arg(machine, 0));
  std::uint32_t module = goal.module;
  LayerWatch watch(term);
  while (atom_qualified(machine, term)) {
    module = atom_id(machine.heap.deref(machine.heap.arg(term, 0)));
    term = machine.heap.deref(machine.heap.arg(term, 1));
    // 一番内側の M が無いので答えが無い。前は何も割り当てずに回り続け、--heap-max でも止まらなかった。
    if (watch.returned_to(term)) {
      raise(machine, goal, formal_resource(machine, "cyclic_term"));
      return;
    }
  }
  finish(machine, goal,
         do_unify(machine, goal.arg(machine, 1), machine.heap.atom(module)) &&
             do_unify(machine, goal.arg(machine, 2), term));
}

// 枠のモジュールは、節の本体ならその節のモジュール、M:G なら M、meta_predicate の引数なら呼び手が包んだモジュール。
void prove_context_module(Impl& machine, const Goal& goal) {
  finish(machine, goal, do_unify(machine, goal.arg(machine, 0), machine.heap.atom(goal.module)));
}

// 性質の形だけを見る。引数の値は単一化に任せ、合わなければ失敗する。
bool module_property_shape(const Impl& machine, Cell property) {
  if (machine.heap.unbound(property)) return true;
  if (property.tag() != Tag::Str || machine.heap.functor_arity(property) != 1) return false;
  const auto name = machine.atoms.name(machine.heap.functor_atom(property));
  return name == "file" || name == "exports" || name == "exported_operators";
}

// 公開した名前で並べる。再公開の項目の from_name は定義したモジュールでの名前なので使わない。
Cell export_list(Impl& machine, const Module& module) {
  std::vector<Cell> items;
  for (const Export& entry : module.exports) items.push_back(cell_functor(machine, entry.name, entry.arity));
  return list_from_cells(machine, items);
}

Cell export_op_list(Impl& machine, const Module& module) {
  const auto op = machine.atoms.intern("op");
  std::vector<Cell> items;
  for (const OpDef& def : module.export_ops) {
    const std::array<Cell, 3> parts{machine.heap.integer(def.priority),
                                    machine.heap.atom(machine.atoms.intern(specifier_name(def.specifier))),
                                    machine.heap.atom(machine.atoms.intern(def.name))};
    items.push_back(machine.heap.structure(op, parts));
  }
  return list_from_cells(machine, items);
}

// M と P を一度に選べるよう、候補を M-P の形にする。user と system は module/2 で作らないので性質を持たない。
void add_module_properties(Impl& machine, const Module& module, std::vector<Cell>& choices) {
  if (module.name == machine.user_module || module.name == machine.system_module) return;
  const auto dash = machine.atoms.intern("-");
  const auto add = [&](std::string_view name, Cell value) {
    const Cell property = machine.heap.structure(machine.atoms.intern(name), std::array<Cell, 1>{value});
    choices.push_back(machine.heap.structure(dash, std::array<Cell, 2>{machine.heap.atom(module.name), property}));
  };
  if (!module.file.empty()) add("file", machine.heap.atom(machine.atoms.intern(module.file)));
  add("exports", export_list(machine, module));
  if (!module.export_ops.empty()) add("exported_operators", export_op_list(machine, module));
}

void prove_module_property(Impl& machine, const Goal& goal) {
  const Cell module = machine.heap.deref(goal.arg(machine, 0));
  const Cell property = machine.heap.deref(goal.arg(machine, 1));
  if (!machine.heap.unbound(module) && module.tag() != Tag::Atom) {
    raise(machine, goal, formal_type(machine, "atom", module));
    return;
  }
  if (!module_property_shape(machine, property)) {
    raise(machine, goal, formal_domain(machine, "module_property", property));
    return;
  }
  std::vector<Cell> choices;
  if (machine.heap.unbound(module)) {
    for (const Module& item : machine.modules) add_module_properties(machine, item, choices);
  } else if (const Module* found = find_module(machine, atom_id(module))) {
    add_module_properties(machine, *found, choices);
  } else {
    raise(machine, goal, formal_existence(machine, "module", module));
    return;
  }
  const Cell target = machine.heap.structure(machine.atoms.intern("-"), std::array<Cell, 2>{module, property});
  offer_choices(machine, goal, target, choices);
}

// 外側から M: を外し、一番内側の M を取り込み元にする。取り込むのは内側からなので、外側の M があるかは見ない。
std::optional<Cell> import_source(Impl& machine, Cell& indicator, std::uint32_t& from) {
  indicator = machine.heap.deref(indicator);
  LayerWatch watch(indicator);
  while (colon_term(machine, indicator)) {
    const Cell name = machine.heap.deref(machine.heap.arg(indicator, 0));
    if (machine.heap.unbound(name)) return formal_instantiation(machine);
    if (name.tag() != Tag::Atom) return formal_type(machine, "atom", name);
    from = atom_id(name);
    indicator = machine.heap.deref(machine.heap.arg(indicator, 1));
    if (watch.returned_to(indicator)) return formal_resource(machine, "cyclic_term");
  }
  return std::nullopt;
}

// 引数の形を全部確かめてから、取り込み元があるかを見る。apply_imports は無い取り込み元を黙って通すので、ここで断る。
void prove_import(Impl& machine, const Goal& goal) {
  Cell indicator = goal.arg(machine, 0);
  std::uint32_t from = goal.module;
  PredKey key;
  auto formal = import_source(machine, indicator, from);
  if (!formal) formal = read_indicator(machine, indicator, key);
  if (!formal && find_module(machine, from) == nullptr) {
    formal = formal_existence(machine, "module", machine.heap.atom(from));
  }
  // 取り込み元が文脈モジュール自身なら、apply_imports は何も変えずに成功する。
  if (!formal) {
    const ImportSpec spec{ImportSpec::Kind::Only, {Import{key.name, key.arity, key.name, 0}}, {}};
    formal = apply_imports(machine, from, goal.module, spec);
  }
  if (formal) {
    raise(machine, goal, *formal);
    return;
  }
  machine.ip = goal.cont;
}

}  // namespace

bool run_module_goal(Impl& machine, const Goal& goal) {
  const auto name = machine.atoms.name(goal.name);
  if (name == "strip_module" && goal.arity == 3) {
    prove_strip_module(machine, goal);
    return true;
  }
  if (name == "context_module" && goal.arity == 1) {
    prove_context_module(machine, goal);
    return true;
  }
  if (name == "module_property" && goal.arity == 2) {
    prove_module_property(machine, goal);
    return true;
  }
  if (name == "import" && goal.arity == 1) {
    prove_import(machine, goal);
    return true;
  }
  return false;
}

}  // namespace mosa
