// 13211-1 に無い（ADR 0028）。デバッガの 9 つの述語と、スパイポイントの組。ポートはここでは出さない。
// trace/0, notrace/0, debug/0, nodebug/0, nospyall/0, debugging/0
//   引数なし。投げる誤りは無い。nodebug/0 は進み方を leap に戻す（trace の後の spy が creep で動き出さないように）。
// spy(+Spec), nospy(+Spec)
//   Spec は Name、Name/Arity、M:Spec、そのリスト。
//   instantiation_error（Spec か Arity か Name が変数、リストの尾か要素が変数）、type_error(predicate_indicator, S)、
//   type_error(atom, Name)、type_error(integer, Arity)、domain_error(not_less_than_zero, Arity)、
//   representation_error(max_arity)、type_error(list, L)、strip_module の誤り（M が変数、無いモジュール）。
// leash(+Ports)
//   Ports は all、none、Port、+Port、-Port、そのリスト。
//   instantiation_error、type_error(atom, X)、type_error(list, L)、domain_error(port, X)。
// テスト: tests/pl/debugger.pl、tests/pl/japanese.pl、tests/mosac/debugger_state.pl
#include "../machine/internal.hpp"
#include "write_internal.hpp"

#include <algorithm>
#include <array>
#include <set>
#include <string>
#include <string_view>

namespace mosa {
namespace {

constexpr std::array<std::string_view, 5> kPortNames{"call", "exit", "redo", "fail", "exception"};
constexpr std::uint16_t kMaxArity = 1024;

// 名前だけの Spec は arity が無い。
struct SpyTarget {
  std::uint32_t module = 0;
  std::uint32_t name = 0;
  std::optional<std::uint16_t> arity;
};

std::uint32_t atom_of(Cell cell) { return static_cast<std::uint32_t>(cell.raw() >> 3); }

void raise(Impl& machine, const Goal& goal, Cell formal) { raise_formal(machine, formal, goal.name, goal.arity); }

void succeed(Impl& machine, const Goal& goal) { machine.ip = goal.cont; }

// 知らせと debugging/0 は user_error に書く。mosac の実行ファイルでも同じ所に出る（計画の設計との違い 2）。
// user_error は番号 2。流れを通すので、後の ~N や '$stream_column'/2 が知らせの終わりの位置を見る。
void say(Impl& machine, std::string_view key, std::initializer_list<std::string_view> args = {}) {
  write_internal::emit_to_stream(machine, 2, message_text(machine, key, args) + "\n");
}

// 組込は system、定義がある述語は定義したモジュール、無ければ文脈のモジュールに置く。
SpyKey key_for(Impl& machine, std::uint32_t context, std::uint32_t name, std::uint16_t arity) {
  if (is_builtin_procedure(machine, name, arity)) return SpyKey{machine.system_module, name, arity};
  const Predicate* found = resolve_pred(machine, context, name, arity);
  if (found != nullptr && !found->abolished) return SpyKey{found->module, name, arity};
  return SpyKey{context, name, arity};
}

std::string indicator_text(const Impl& machine, const SpyKey& key) {
  return std::string{machine.atoms.name(key.name)} + "/" + std::to_string(key.arity);
}

// 文脈のモジュールから見える Name/_ の鍵。表にある述語と組込の arity を集め、見えるものだけ残す。
std::vector<SpyKey> visible_keys(Impl& machine, const SpyTarget& target) {
  std::set<std::uint16_t> arities;
  for (const Predicate& predicate : machine.predicates) {
    if (!predicate.abolished && predicate.name == target.name) arities.insert(predicate.arity);
  }
  for (const BuiltinPi& builtin : builtin_procedures()) {
    if (builtin.name == machine.atoms.name(target.name)) arities.insert(builtin.arity);
  }
  std::vector<SpyKey> keys;
  for (const std::uint16_t arity : arities) {
    const bool builtin = is_builtin_procedure(machine, target.name, arity);
    const Predicate* found = resolve_pred(machine, target.module, target.name, arity);
    if (builtin || (found != nullptr && !found->abolished)) keys.push_back(key_for(machine, target.module, target.name, arity));
  }
  return keys;
}

// Name/Arity の 2 つの項の検査。parse_indicator（database.cpp）と同じ誤りで、Formal を返す。
std::optional<Cell> arity_problem(Impl& machine, Cell name, Cell arity) {
  if (machine.heap.unbound(name) || machine.heap.unbound(arity)) return formal_instantiation(machine);
  if (name.tag() != Tag::Atom) return formal_type(machine, "atom", name);
  if (!is_integer_tag(arity.tag())) return formal_type(machine, "integer", arity);
  if (integer_negative(machine, arity)) return formal_domain(machine, "not_less_than_zero", arity);
  if (arity.tag() == Tag::Big || machine.heap.integer_value(arity) > kMaxArity) {
    return formal_representation(machine, "max_arity");
  }
  return std::nullopt;
}

// 1 つの Spec（リストの要素）。誤りを投げたら偽。
bool parse_item(Impl& machine, const Goal& goal, Cell cell, SpyTarget& target) {
  std::uint32_t module = goal.module;
  cell = machine.heap.deref(cell);
  if (machine.heap.unbound(cell)) return raise(machine, goal, formal_instantiation(machine)), false;
  if (const auto formal = strip_module(machine, cell, module)) return raise(machine, goal, *formal), false;
  cell = machine.heap.deref(cell);
  target.module = module;
  if (machine.heap.unbound(cell)) return raise(machine, goal, formal_instantiation(machine)), false;
  if (cell.tag() == Tag::Atom) {
    target.name = atom_of(cell);
    return true;
  }
  const bool shaped = cell.tag() == Tag::Str && machine.heap.functor_arity(cell) == 2 &&
                      machine.atoms.name(machine.heap.functor_atom(cell)) == "/";
  if (!shaped) return raise(machine, goal, formal_type(machine, "predicate_indicator", cell)), false;
  const Cell name = machine.heap.deref(machine.heap.arg(cell, 0));
  const Cell arity = machine.heap.deref(machine.heap.arg(cell, 1));
  if (const auto formal = arity_problem(machine, name, arity)) return raise(machine, goal, *formal), false;
  target.name = atom_of(name);
  target.arity = static_cast<std::uint16_t>(machine.heap.integer_value(arity));
  return true;
}

// spy/1 と nospy/1 の引数を、検査を済ませた Spec の並びにする。誤りを投げたら偽（何も変えていない）。
bool parse_specs(Impl& machine, const Goal& goal, std::vector<SpyTarget>& targets) {
  Cell arg = machine.heap.deref(goal.arg(machine, 0));
  std::uint32_t module = goal.module;
  if (machine.heap.unbound(arg)) return raise(machine, goal, formal_instantiation(machine)), false;
  if (const auto formal = strip_module(machine, arg, module)) return raise(machine, goal, *formal), false;
  arg = machine.heap.deref(arg);
  const bool empty = arg.tag() == Tag::Atom && machine.atoms.name(atom_of(arg)) == "[]";
  if (empty) return true;
  if (arg.tag() != Tag::Lis) {
    // 剥いだ後の文脈で読む。M:Spec の M を item 側にも伝えるため、Goal の module を差し替えた写しを使う。
    Goal inner = goal;
    inner.module = module;
    targets.emplace_back();
    return parse_item(machine, inner, arg, targets.back());
  }
  const WalkedList walked = walk_list(machine, arg);
  if (walked.shape == ListShape::Partial) return raise(machine, goal, formal_instantiation(machine)), false;
  if (walked.shape != ListShape::Proper) return raise(machine, goal, formal_type(machine, "list", arg)), false;
  Goal inner = goal;
  inner.module = module;
  for (const Cell item : walked.items) {
    targets.emplace_back();
    if (!parse_item(machine, inner, item, targets.back())) return false;
  }
  return true;
}

std::vector<SpyKey> expand(Impl& machine, const SpyTarget& target) {
  if (target.arity) return {key_for(machine, target.module, target.name, *target.arity)};
  return visible_keys(machine, target);
}

void run_spy(Impl& machine, const Goal& goal) {
  std::vector<SpyTarget> targets;
  if (!parse_specs(machine, goal, targets)) return;
  for (const SpyTarget& target : targets) {
    const auto keys = expand(machine, target);
    if (keys.empty()) say(machine, "debug_spy_none", {machine.atoms.name(target.name)});
    for (const SpyKey& key : keys) {
      static_cast<void>(set_spy(machine, key, true));  // 置いてあっても知らせは同じ
      say(machine, "debug_spy_set", {indicator_text(machine, key)});
    }
  }
  machine.debug_on = true;
  succeed(machine, goal);
}

// 名前だけの Spec は、置いたときと同じ引き当てで同じ名前になる鍵だけを外す。
std::vector<SpyKey> placed_keys(Impl& machine, const SpyTarget& target) {
  if (target.arity) return {key_for(machine, target.module, target.name, *target.arity)};
  std::vector<SpyKey> keys;
  const auto placed = machine.debug.spies;
  for (const SpyKey& key : placed) {
    if (key.name == target.name && key_for(machine, target.module, key.name, key.arity).module == key.module) {
      keys.push_back(key);
    }
  }
  return keys;
}

void run_nospy(Impl& machine, const Goal& goal) {
  std::vector<SpyTarget> targets;
  if (!parse_specs(machine, goal, targets)) return;
  for (const SpyTarget& target : targets) {
    for (const SpyKey& key : placed_keys(machine, target)) {
      if (set_spy(machine, key, false)) say(machine, "debug_spy_removed", {indicator_text(machine, key)});
    }
  }
  succeed(machine, goal);
}

std::optional<std::size_t> port_index(std::string_view text) {
  for (std::size_t index = 0; index < kPortNames.size(); ++index) {
    if (kPortNames[index] == text) return index;
  }
  return std::nullopt;
}

std::uint8_t port_bit(std::size_t index) { return static_cast<std::uint8_t>(1U << index); }

// leash の 1 項目を mask に当てる。Formal を返したら誤り。
std::optional<Cell> apply_leash_item(Impl& machine, Cell item, std::uint8_t& mask) {
  item = machine.heap.deref(item);
  if (machine.heap.unbound(item)) return formal_instantiation(machine);
  if (item.tag() == Tag::Atom) {
    const auto text = machine.atoms.name(atom_of(item));
    if (text == "all") mask = kAllPorts;
    else if (text == "none") mask = 0;
    else if (const auto index = port_index(text)) mask = static_cast<std::uint8_t>(mask | port_bit(*index));
    else return formal_domain(machine, "port", item);
    return std::nullopt;
  }
  const bool signed_port = item.tag() == Tag::Str && machine.heap.functor_arity(item) == 1 &&
                           (machine.atoms.name(machine.heap.functor_atom(item)) == "+" ||
                            machine.atoms.name(machine.heap.functor_atom(item)) == "-");
  if (!signed_port) return formal_type(machine, "atom", item);
  const bool add = machine.atoms.name(machine.heap.functor_atom(item)) == "+";
  const Cell port = machine.heap.deref(machine.heap.arg(item, 0));
  if (machine.heap.unbound(port)) return formal_instantiation(machine);
  if (port.tag() != Tag::Atom) return formal_type(machine, "atom", port);
  const auto index = port_index(machine.atoms.name(atom_of(port)));
  if (!index) return formal_domain(machine, "port", port);
  mask = add ? static_cast<std::uint8_t>(mask | port_bit(*index)) : static_cast<std::uint8_t>(mask & ~port_bit(*index));
  return std::nullopt;
}

// 要素を左から順に当てる。誤りがあれば leashed は変えない。
void run_leash(Impl& machine, const Goal& goal) {
  const Cell arg = machine.heap.deref(goal.arg(machine, 0));
  std::vector<Cell> items{arg};
  if (arg.tag() == Tag::Lis) {
    const WalkedList walked = walk_list(machine, arg);
    if (walked.shape == ListShape::Partial) return raise(machine, goal, formal_instantiation(machine));
    if (walked.shape != ListShape::Proper) return raise(machine, goal, formal_type(machine, "list", arg));
    items = walked.items;
  } else if (arg.tag() == Tag::Atom && machine.atoms.name(atom_of(arg)) == "[]") {
    items.clear();
  }
  std::uint8_t mask = machine.debug.leashed;
  for (const Cell item : items) {
    if (const auto formal = apply_leash_item(machine, item, mask)) return raise(machine, goal, *formal);
  }
  machine.debug.leashed = mask;
  succeed(machine, goal);
}

std::string_view mode_name(DebugMode mode) {
  if (mode == DebugMode::Creep) return "creep";
  return mode == DebugMode::Skip ? "skip" : "leap";
}

std::string join_spies(const Impl& machine) {
  std::string text;
  for (const SpyKey& key : machine.debug.spies) {
    if (!text.empty()) text += ", ";
    text += indicator_text(machine, key);
  }
  return text;
}

std::string join_ports(std::uint8_t mask) {
  std::string text;
  for (std::size_t index = 0; index < kPortNames.size(); ++index) {
    if ((mask & port_bit(index)) == 0) continue;
    if (!text.empty()) text += ' ';
    text += kPortNames[index];
  }
  return text;
}

void run_debugging(Impl& machine, const Goal& goal) {
  const std::string none = message_text(machine, "debug_status_none");
  if (machine.debug_on) say(machine, "debug_status_on", {mode_name(machine.debug.mode)});
  else say(machine, "debug_status_off");
  const std::string spies = join_spies(machine);
  const std::string ports = join_ports(machine.debug.leashed);
  say(machine, "debug_status_spies", {spies.empty() ? std::string_view{none} : std::string_view{spies}});
  say(machine, "debug_status_leash", {ports.empty() ? std::string_view{none} : std::string_view{ports}});
  succeed(machine, goal);
}

}  // namespace

void set_debug(Impl& machine, bool on, DebugMode mode) {
  machine.debug_on = on;
  machine.debug.mode = mode;
}

bool spied(const Impl& machine, std::uint32_t module, std::uint32_t name, std::uint16_t arity) {
  return std::any_of(machine.debug.spies.begin(), machine.debug.spies.end(), [&](const SpyKey& key) {
    return key.module == module && key.name == name && key.arity == arity;
  });
}

bool set_spy(Impl& machine, const SpyKey& key, bool on) {
  auto& spies = machine.debug.spies;
  const auto same = [&](const SpyKey& other) {
    return other.module == key.module && other.name == key.name && other.arity == key.arity;
  };
  const auto found = std::find_if(spies.begin(), spies.end(), same);
  if (on) {
    if (found != spies.end()) return false;
    spies.push_back(key);
    return true;
  }
  if (found == spies.end()) return false;
  spies.erase(found);
  return true;
}

bool run_debugger(Impl& machine, const Goal& goal) {
  const auto name = machine.atoms.name(goal.name);
  if (goal.arity == 0) {
    if (name == "trace") set_debug(machine, true, DebugMode::Creep);
    else if (name == "notrace") set_debug(machine, machine.debug_on, DebugMode::Leap);
    else if (name == "debug") set_debug(machine, true, DebugMode::Leap);
    else if (name == "nodebug") set_debug(machine, false, DebugMode::Leap);
    else if (name == "nospyall") machine.debug.spies.clear();
    else if (name == "debugging") return run_debugging(machine, goal), true;
    else return false;
    succeed(machine, goal);
    return true;
  }
  if (goal.arity != 1) return false;
  if (name == "spy") run_spy(machine, goal);
  else if (name == "nospy") run_nospy(machine, goal);
  else if (name == "leash") run_leash(machine, goal);
  else return false;
  return true;
}

}  // namespace mosa
