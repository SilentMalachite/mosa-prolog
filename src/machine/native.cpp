// ADR 0009。翻訳単位の登録、節の指紋、静的述語への生成関数の付け外し、生成コードが呼ぶ束縛と呼び出し。
// 選択点と継続枠（Pick::Native、Ctrl::Native）は native_redo.cpp に置く。

#include "internal.hpp"

#include "mosa/goal_word.hpp"
#include "mosa/path_text.hpp"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace mosa {
namespace {

// 64 ビットの FNV-1a。翻訳器と読み込み器が別のプロセスで同じ値を出すよう、語は下位のバイトから入れる。
struct Fnv {
  std::uint64_t value = 14695981039346656037ULL;

  void byte(std::uint8_t data) {
    value ^= data;
    value *= 1099511628211ULL;
  }
  void word(std::uint64_t data) {
    for (unsigned shift = 0; shift < 64; shift += 8) byte(static_cast<std::uint8_t>(data >> shift));
  }
  // アトムは番号ではなく名前で数える。番号は読み込みの順で変わる。長さを先に入れて区切りを残す。
  void text(std::string_view data) {
    word(data.size());
    for (const char unit : data) byte(static_cast<std::uint8_t>(unit));
  }
};

bool names_atom(IrTag tag) { return tag == IrTag::Atom || tag == IrTag::Fun || tag == IrTag::Call; }

// 前順にたどる。長いリストの原文でも C++ の呼び出しを積まないよう、手で積む。
void hash_term(Fnv& hash, const AtomTable& atoms, const IrTerm& root) {
  std::vector<const IrTerm*> pending{&root};
  while (!pending.empty()) {
    const IrTerm* term = pending.back();
    pending.pop_back();
    hash.word(static_cast<std::uint64_t>(term->tag));
    hash.word(term->slot);
    hash.word(term->arity);
    hash.word(static_cast<std::uint64_t>(term->integer));
    hash.word(std::bit_cast<std::uint64_t>(term->floating));
    if (names_atom(term->tag)) hash.text(atoms.name(term->atom));
    hash.word(term->args.size());
    for (auto it = term->args.rbegin(); it != term->args.rend(); ++it) pending.push_back(&*it);
  }
}

bool same_key(const rt::NativeCall& call, const Predicate& predicate) {
  return call.module == predicate.module && call.name == predicate.name && call.arity == predicate.arity;
}

PredKey key_of(Impl& machine, std::string_view module, std::string_view name, std::uint16_t arity) {
  return PredKey{machine.atoms.intern(module), machine.atoms.intern(name), arity};
}

// 番号は単位の中だけで振る。鍵はこの Machine のアトムで作り直す。
rt::UnitState build_unit(Impl& machine, const rt::NativeUnit& unit) {
  rt::UnitState state;
  state.tag = unit.tag;
  for (const std::string_view atom : unit.atoms) state.atoms.push_back(machine.atoms.intern(atom));
  for (const rt::NativeCallee& callee : unit.calls) {
    const PredKey key = key_of(machine, callee.module, callee.name, callee.arity);
    state.calls.push_back(rt::NativeCall{key.module, key.name, key.arity, nullptr});
  }
  for (const rt::NativeEntry& entry : unit.entries) {
    state.entries.push_back(key_of(machine, entry.module, entry.name, entry.arity));
  }
  return state;
}

bool attachable(const Impl& machine, const Predicate* predicate, std::uint64_t fingerprint) {
  if (predicate == nullptr || !native_eligible(machine, *predicate)) return false;
  return clause_fingerprint(machine.atoms, *predicate) == fingerprint;
}

void attach(Impl& machine, Predicate& predicate, rt::NativeProc proc) {
  predicate.native = proc;
  for (rt::UnitState& unit : machine.native_units) {
    for (rt::NativeCall& call : unit.calls) {
      if (same_key(call, predicate)) call.proc = proc;
    }
  }
}

std::size_t unit_slot(Impl& machine, const rt::UnitTag* tag) {
  for (std::size_t index = 0; index < machine.native_units.size(); ++index) {
    if (machine.native_units[index].tag == tag) return index;
  }
  machine.native_units.emplace_back();
  return machine.native_units.size() - 1;
}

void attach_unit(Impl& machine, std::size_t slot, const rt::NativeUnit& unit) {
  for (std::size_t index = 0; index < unit.entries.size(); ++index) {
    Predicate* predicate = find_pred(machine, machine.native_units[slot].entries[index]);
    if (attachable(machine, predicate, unit.entries[index].fingerprint)) {
      attach(machine, *predicate, unit.entries[index].proc);
    }
  }
  // 先に入った単位が付けた述語も、この単位から直に呼ぶ。呼び先は同じモジュールで引き、取り込みは辿らない。
  for (rt::NativeCall& call : machine.native_units[slot].calls) {
    const Predicate* predicate = find_pred(machine, PredKey{call.module, call.name, call.arity});
    call.proc = predicate == nullptr ? nullptr : predicate->native;
  }
}

// 埋め込んだファイルの単位は、そのパスのファイルとして読む（計画 Task 5）。根の単位の include/1 や
// use_module/1 が先に読んでいれば読み直さず、付けるだけにする。埋め込みの無い単位は今までどおり文字だけで読む。
bool load_unit(Impl& machine, const rt::NativeUnit& unit) {
  const auto key = utf8_from_path(canonical_key(path_from_utf8(unit.name)));
  const bool embedded = std::any_of(machine.embedded.begin(), machine.embedded.end(),
                                    [&](const EmbeddedSource& source) { return source.key == key; });
  if (!embedded) return load_source(machine, unit.source).ok;
  if (std::find(machine.prepared.begin(), machine.prepared.end(), key) != machine.prepared.end()) return true;
  try {
    if (load_source(machine, unit.source, utf8_from_path(path_from_utf8(key).parent_path()), key).ok) return true;
  } catch (const ResourceLimit&) {
    // 上限に達したヒープではカタログを読めない。message_text は新しいマシンで文面を作る。
    machine.heap.truncate(0);
    machine.diagnostic = message_text(machine, "heap_limit");
    return false;
  }
  prefix_consult_diagnostic(machine, unit.name);
  return false;
}

std::uint64_t* writable(Impl& machine, std::size_t at) {
  // 語はヒープそのもの。data() は読み出し用の const なので、書く番地へ戻す。
  return const_cast<std::uint64_t*>(machine.heap.data()) + at;
}

}  // namespace

std::uint64_t clause_fingerprint(const AtomTable& atoms, const Predicate& predicate) {
  Fnv hash;
  hash.word(predicate.meta.size());
  for (const MetaArg& arg : predicate.meta) {
    hash.word(static_cast<std::uint64_t>(arg.kind));
    hash.word(arg.extra);
    hash.text(atoms.name(arg.mode));
  }
  hash.word(live_count(predicate));
  for (const Clause& clause : predicate.clauses) {
    if (clause.dead) continue;
    hash.word(clause.vars);
    hash_term(hash, atoms, clause.head);
    hash_term(hash, atoms, clause.body);
  }
  return hash.value;
}

// 組込と同じ名前の利用者述語は、インタプリタでは run_builtin が先に取るので決して呼ばれない。
// 付けると call_goal の近道だけがそれを呼んでしまうので付けない。
// meta_predicate の述語は付ける。限定する引数は生成コードが頭部で rt::qualify に通す（計画 Task 7）。宣言は指紋に
// 入るので、翻訳のあとで宣言が変われば付かない。
// 死んだ節を持つ述語は付けない。指紋は死んだ節を数えないが、interpret_clause は節のベクタをそのまま引く。
// 翻訳器（src/codegen）もこの関数で生成関数を出す述語を選ぶ。出したのに付かない述語を作らない。
bool native_eligible(const Impl& machine, const Predicate& predicate) {
  if (predicate.dynamic || predicate.abolished || predicate.clauses.empty()) return false;
  if (live_count(predicate) != predicate.clauses.size()) return false;
  return !is_builtin_procedure(machine, predicate.name, predicate.arity);
}

void detach_native(Impl& machine, Predicate& predicate) {
  if (predicate.native == nullptr) return;
  predicate.native = nullptr;
  for (rt::UnitState& unit : machine.native_units) {
    for (rt::NativeCall& call : unit.calls) {
      if (same_key(call, predicate)) call.proc = nullptr;
    }
  }
}

namespace rt {

bool install_unit(Machine& machine, const NativeUnit& unit) {
  Impl& impl = engine_impl(machine);
  const auto slot = unit_slot(impl, unit.tag);
  impl.native_units[slot] = build_unit(impl, unit);
  // 埋め込みの原文は今の読み込み器で読む。library(_) を単位で引くのは Task 10（ADR 0009）。
  if (!load_unit(impl, unit)) return false;
  attach_unit(impl, slot, unit);
  return true;
}

Regs regs(Machine::Impl& machine, const UnitTag& unit) {
  // 単位は数個なので番地を順に比べる。1 回の起動に 1 回だけ。
  for (const UnitState& state : machine.native_units) {
    if (state.tag == &unit) return Regs{machine.heap.data(), state.atoms.data(), &state, machine.native_cut};
  }
  return Regs{machine.heap.data(), nullptr, nullptr, machine.native_cut};
}

std::uint64_t* alloc(Machine::Impl& machine, std::size_t count) {
  return writable(machine, machine.heap.allocate_words(count));
}

void bind(Machine::Impl& machine, Cell variable, Cell value) {
  auto* slot = const_cast<std::uint64_t*>(cell_words(variable));
  *slot = value.raw();
  // do_unify の水位と同じ。いちばん新しい選択点より古い変数だけをトレイルに積む。
  const std::uint64_t* mark =
      machine.stack.empty() ? machine.heap.data() : machine.heap.data() + machine.stack.back().heap_mark;
  if (slot < mark) machine.trail.slots.push_back(slot);
}

// 変数と同じセルは do_unify の Attempt を作らずに済ませる。束ねる向きは unify_cells と同じく左から。
bool unify(Machine::Impl& machine, Cell left, Cell right) {
  left = deref(left);
  right = deref(right);
  if (left.raw() == right.raw() && left.tag() != Tag::Float) return true;
  if (unbound(left)) {
    bind(machine, left, right);
    return true;
  }
  if (unbound(right)) {
    bind(machine, right, left);
    return true;
  }
  return do_unify(machine, left, right);
}

void call_goal(Machine::Impl& machine, const Regs& regs, std::uint32_t number, std::span<const Cell> args,
               std::uint64_t cont, std::size_t cut) {
  const NativeCall& callee = regs.unit->calls[number];
  const auto at = machine.heap.allocate_words(kFrameHead + args.size());
  auto* words = writable(machine, at);
  words[0] = callee.name;
  words[1] = pack_goal_word(callee.arity, callee.module);
  words[2] = cut;
  words[3] = cont;
  for (std::size_t index = 0; index < args.size(); ++index) words[kFrameHead + index] = args[index].raw();
  machine.ip = at;
  // 外した述語と取り込んだ述語の proc は空。枠は普通のゴール枠なので run_builtin と enter_user が引く。
  machine.native_next = callee.proc;
}

void cut_to(Machine::Impl& machine, std::size_t barrier) { trim_stack(machine, barrier); }

void proceed(Machine::Impl& machine, std::uint64_t cont) { machine.ip = cont; }

void fail(Machine::Impl& machine) { machine.failed = true; }

void interpret_clause(Machine::Impl& machine, const Regs& regs, std::uint32_t entry, std::uint64_t frame,
                      std::uint32_t clause) {
  const Predicate* predicate = find_pred(machine, regs.unit->entries[entry]);
  // 付けたあとで述語が書き換わっても、残った選択点はここへ来る。無い節は redo_clause と同じく失敗にする。
  if (predicate == nullptr || clause == 0 || clause > predicate->clauses.size()) {
    machine.failed = true;
    return;
  }
  enter_clause(machine, *predicate, clause - 1, frame, regs.cut);
}

}  // namespace rt
}  // namespace mosa
