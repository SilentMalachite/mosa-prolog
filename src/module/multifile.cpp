// 7.4.2 と docs/SPEC.md 第5節。multifile/1 は指示。
//   multifile(+PI) は読み込み中のモジュール。multifile(+Module:PI) はそのモジュール。
//   両側が宣言した Owner:Head と (Owner:Head :- Body) は Owner に足す。
//   足りなければ permission_error(modify, static_procedure, PI)。
// テスト: tests/pl/modules.pl tests/pl/directives.pl

#include "../machine/internal.hpp"

#include "mosa/writer.hpp"

#include <cstdint>
#include <vector>

namespace mosa {
namespace {

bool listed(const std::vector<SharedPred>& items, std::uint32_t home, std::uint32_t name, std::uint16_t arity) {
  for (const SharedPred& item : items) {
    if (item.home == home && item.name == name && item.arity == arity) return true;
  }
  return false;
}

bool keeps(const Impl& machine, std::uint32_t home, std::uint32_t name, std::uint16_t arity) {
  return listed(machine.multifile, home, name, arity);
}

bool declared_here(const Impl& machine, std::uint32_t home, std::uint32_t name, std::uint16_t arity) {
  return listed(machine.multifile_here, home, name, arity);
}

bool refuse_static(Impl& machine, std::uint32_t name, std::uint16_t arity) {
  return refuse_load(machine, "modify", "static_procedure", name, arity);
}

bool colon_term(const Impl& machine, Cell term) {
  term = machine.heap.deref(term);
  return term.tag() == Tag::Str && machine.heap.functor_arity(term) == 2 &&
         machine.atoms.name(machine.heap.functor_atom(term)) == ":";
}

bool neck(const Impl& machine, Cell term) {
  term = machine.heap.deref(term);
  return term.tag() == Tag::Str && machine.heap.functor_arity(term) == 2 &&
         machine.atoms.name(machine.heap.functor_atom(term)) == ":-";
}

bool head_indicator(Impl& machine, Cell head, std::uint32_t& name, std::uint16_t& arity) {
  head = machine.heap.deref(head);
  if (head.tag() == Tag::Atom) {
    name = static_cast<std::uint32_t>(head.raw() >> 3);
    arity = 0;
    return true;
  }
  if (head.tag() != Tag::Str) return false;
  name = machine.heap.functor_atom(head);
  arity = machine.heap.functor_arity(head);
  return true;
}

bool qualified_head(Impl& machine, Cell looked, std::uint32_t& named, Cell& inner, std::uint32_t& pred,
                    std::uint16_t& arity) {
  if (!colon_term(machine, looked)) return false;
  const Cell module = machine.heap.deref(machine.heap.arg(looked, 0));
  if (module.tag() != Tag::Atom) return false;
  named = static_cast<std::uint32_t>(module.raw() >> 3);
  if (!module_loaded(machine, named)) return false;
  inner = machine.heap.deref(machine.heap.arg(looked, 1));
  return head_indicator(machine, inner, pred, arity);
}

}  // namespace

// consult と use_module は、この記録を permission_error(action, type, PI) として投げ直す。
bool refuse_load(Impl& machine, std::string_view action, std::string_view type, std::uint32_t name,
                 std::uint16_t arity) {
  machine.load_perm = true;
  machine.load_perm_action = action;
  machine.load_perm_type = type;
  machine.load_perm_name = name;
  machine.load_perm_arity = arity;
  const Cell formal = formal_permission(machine, action, type, cell_functor(machine, name, arity));
  machine.diagnostic = write_term(machine.heap, machine.operators, formal, {}, {});
  machine.syntax_diag = false;
  return false;
}

bool multifile_keeps(const Impl& machine, std::uint32_t home, std::uint32_t name, std::uint16_t arity) {
  return keeps(machine, home, name, arity);
}

void note_multifile(Impl& machine, std::uint32_t home, std::uint32_t name, std::uint16_t arity) {
  if (!declared_here(machine, home, name, arity)) {
    machine.multifile_here.push_back(SharedPred{home, name, arity});
  }
  // 他モジュールへの宣言は、持つ側が自分で宣言したことにはしない。
  if (home != machine.load_home || keeps(machine, home, name, arity)) return;
  machine.multifile.push_back(SharedPred{home, name, arity});
}

bool retarget_multifile_clause(Impl& machine, Cell& term, std::uint32_t& home) {
  Cell looked = machine.heap.deref(term);
  Cell body;
  const bool rule = neck(machine, looked);
  // (Module:Head :- Body) は外側が :- なので、事実の剥がしでは頭部に届かない。
  if (rule) {
    body = machine.heap.arg(looked, 1);
    looked = machine.heap.deref(machine.heap.arg(looked, 0));
  }
  std::uint32_t named = 0;
  std::uint32_t pred = 0;
  std::uint16_t arity = 0;
  Cell inner;
  if (!colon_term(machine, looked)) return true;
  // 載っていないモジュールや頭部でない右辺のまま足すと、:/2 の節になる。:/2 は system の述語。
  if (!qualified_head(machine, looked, named, inner, pred, arity)) {
    return refuse_static(machine, machine.atoms.intern(":"), 2);
  }
  // 外部の節は、持つ側の宣言と、このファイルの宣言が両方いる。自分のモジュールは限定を剥がすだけ。
  if (named != machine.load_home) {
    if (!keeps(machine, named, pred, arity) || !declared_here(machine, named, pred, arity)) {
      return refuse_static(machine, pred, arity);
    }
    home = named;
  }
  if (!rule) {
    term = inner;
    return true;
  }
  const std::vector<Cell> args{inner, body};
  term = machine.heap.structure(machine.atoms.intern(":-"), args);
  return true;
}

}  // namespace mosa
