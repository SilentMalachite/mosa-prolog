// nb_setval(+atom, +term)、nb_getval(+atom, ?term)。非 ISO（SPEC 第7節の段階 G、レビュー M11）で、SWI-Prolog の
// pl-gvar.c に合わせる。
//   nb_setval は値を置いたときに写し、後戻りしても戻さない。鍵はモジュールに依らない。nb_getval は置いた値と単一化する。
//   SWI-Prolog の nb_getval は毎回同じ項を返すが、Mosa のヒープは後戻りで巻き戻るので値はヒープの外に写して持ち、
//   読むたびにヒープへ写し直す。値の中の変数は、2 回の nb_getval で別の変数になる。
//   値の大きさは findall の答えと同じく Heap::held に数え、--heap-max を超えればヒープの上限と同じ停止にする。
//   instantiation_error（鍵が変数）、type_error(atom, Key)、existence_error(variable, Key)（nb_getval で置いていない鍵。
//   SWI-Prolog の user:exception/3 の口は無い）。
// テスト: tests/pl/utilities.pl
#include "../machine/internal.hpp"

#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

namespace mosa {
namespace {

void raise(Impl& machine, const Goal& goal, Cell formal) { raise_formal(machine, formal, goal.name, goal.arity); }

std::optional<std::uint32_t> global_key(Impl& machine, const Goal& goal) {
  const Cell key = machine.heap.deref(goal.arg(machine, 0));
  if (machine.heap.unbound(key)) {
    raise(machine, goal, formal_instantiation(machine));
    return std::nullopt;
  }
  if (key.tag() != Tag::Atom) {
    raise(machine, goal, formal_type(machine, "atom", key));
    return std::nullopt;
  }
  return static_cast<std::uint32_t>(key.raw() >> 3);
}

void run_setval(Impl& machine, const Goal& goal) {
  const auto key = global_key(machine, goal);
  if (!key) return;
  GlobalValue fresh;
  fresh.value = export_term(machine, goal.arg(machine, 1));
  fresh.words = (footprint(fresh.value) + sizeof(std::uint64_t) - 1) / sizeof(std::uint64_t);
  const std::size_t words = fresh.words;
  GlobalValue& slot = machine.globals[*key];
  const std::size_t old_words = slot.words;
  slot = std::move(fresh);
  machine.heap.release(old_words);
  // 置いてから数える。上限で投げても、値と held の語数は合っている。
  machine.ip = goal.cont;
  hold_answer_words(machine, words);
}

void run_getval(Impl& machine, const Goal& goal) {
  const auto key = global_key(machine, goal);
  if (!key) return;
  const auto found = machine.globals.find(*key);
  if (found == machine.globals.end()) {
    raise(machine, goal, formal_existence(machine, "variable", machine.heap.atom(*key)));
    return;
  }
  const Cell value = import_term(machine, found->second.value);
  if (do_unify(machine, goal.arg(machine, 1), value)) machine.ip = goal.cont;
  else machine.failed = true;
}

}  // namespace

bool run_global_vars(Impl& machine, const Goal& goal) {
  if (goal.arity != 2) return false;
  const std::string_view name = machine.atoms.name(goal.name);
  if (name == "nb_setval") {
    run_setval(machine, goal);
    return true;
  }
  if (name == "nb_getval") {
    run_getval(machine, goal);
    return true;
  }
  return false;
}

}  // namespace mosa
