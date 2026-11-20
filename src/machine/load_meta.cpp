// :- meta_predicate と :- module/2、:- export/1 の指令（load_directive.cpp の consume_directive が振り分ける）。
// load_directive.cpp が 800 行に近づいたので分けた（レビュー H-10 の前）。
#include "load_internal.hpp"

#include "mosa/path_text.hpp"
#include "mosa/writer.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mosa {

namespace {

// 関手 name/arity の指令なら、その本体を返す。
std::optional<Cell> directive_body(Impl& machine, Cell directive, std::string_view name, std::uint16_t arity) {
  directive = machine.heap.deref(directive);
  if (directive.tag() != Tag::Str || machine.heap.functor_arity(directive) != 1) return std::nullopt;
  const Cell body = machine.heap.deref(machine.heap.arg(directive, 0));
  if (body.tag() != Tag::Str || machine.heap.functor_arity(body) != arity) return std::nullopt;
  if (machine.atoms.name(machine.heap.functor_atom(body)) != name) return std::nullopt;
  return body;
}

// meta_predicate A, B と meta_predicate [A, B] の両方を受ける。
void collect_heads(Impl& machine, Cell cell, std::vector<Cell>& heads) {
  cell = machine.heap.deref(cell);
  if (cell.tag() == Tag::Str && machine.heap.functor_arity(cell) == 2 &&
      machine.atoms.name(machine.heap.functor_atom(cell)) == ",") {
    collect_heads(machine, machine.heap.arg(cell, 0), heads);
    collect_heads(machine, machine.heap.arg(cell, 1), heads);
    return;
  }
  const WalkedList walked = walk_list(machine, cell);
  if (cell.tag() == Tag::Lis && walked.shape == ListShape::Proper) {
    for (const Cell item : walked.items) collect_heads(machine, item, heads);
    return;
  }
  heads.push_back(cell);
}

bool fail_with(Impl& machine, Cell formal) {
  fail_load(machine, write_term(machine.heap, machine.operators, formal, {}, {}));
  return false;
}

// 手続きがあればその述語へ、無ければモジュールの保留の表へ置く。節を足すとき adopt_meta が移す。
bool declare_meta(Impl& machine, LoadSnapshots& snapshots, Cell head) {
  PredKey key{machine.load_module, 0, 0};
  std::vector<MetaArg> args;
  auto formal = strip_module(machine, head, key.module);
  if (!formal) formal = read_meta_spec(machine, head, args);
  if (formal) return fail_with(machine, *formal);
  const Cell spec = machine.heap.deref(head);
  if (spec.tag() == Tag::Atom) key.name = static_cast<std::uint32_t>(spec.raw() >> 3);
  else if (spec.tag() == Tag::Lis) key.name = machine.atoms.intern(".");
  else key.name = machine.heap.functor_atom(spec);
  key.arity = static_cast<std::uint16_t>(args.size());
  if (is_builtin_procedure(machine, key.name, key.arity)) {
    return fail_with(machine, formal_permission(machine, "modify", "static_procedure",
                                                cell_functor(machine, key.name, key.arity)));
  }
  Predicate* predicate = find_pred(machine, key);
  if (predicate == nullptr) {
    if (const auto conflict = check_local(machine, key.module, key.name, key.arity)) return fail_with(machine, *conflict);
    // 宣言は手続きを作らない。読み込みの失敗はモジュール表ごと戻すので、保留の表も戻る。
    Module* module = find_module(machine, key.module);
    if (module == nullptr) return true;
    std::erase_if(module->pending_meta, [&](const PendingMeta& item) {
      return item.name == key.name && item.arity == key.arity;
    });
    module->pending_meta.push_back(PendingMeta{key.name, key.arity, std::move(args)});
    return true;
  }
  remember_pred(machine, snapshots, key, predicate);
  // meta は指紋に入る。付けたあとの宣言は、生成関数が知らない限定になる。
  detach_native(machine, *predicate);
  predicate->meta = std::move(args);
  predicate->meta_declared = true;
  snapshots.notes().metas.push_back(key);
  return true;
}

}  // namespace

int apply_meta_directive(Impl& machine, Cell directive, LoadSnapshots& snapshots) {
  const auto body = directive_body(machine, directive, "meta_predicate", 1);
  if (!body) return 0;
  std::vector<Cell> heads;
  collect_heads(machine, machine.heap.arg(*body, 0), heads);
  for (const Cell head : heads) {
    if (!declare_meta(machine, snapshots, head)) return -1;
  }
  return 1;
}

int apply_module_directive(Impl& machine, Cell directive, bool first, std::string_view origin) {
  std::optional<Cell> failure;
  if (const auto body = directive_body(machine, directive, "module", 2)) {
    // ファイルとモジュールの対応は正規化したパスで持つ。use_module がパスの書き方によらず見つける。
    const auto key = origin.empty() ? std::string{}
                                    : utf8_from_path(canonical_key(path_from_utf8(origin)));
    failure = declare_module(machine, machine.heap.arg(*body, 0), machine.heap.arg(*body, 1), first, key);
  } else if (const auto listed = directive_body(machine, directive, "export", 1)) {
    failure = add_exports(machine, machine.heap.arg(*listed, 0));
  } else {
    return 0;
  }
  if (!failure) return 1;
  fail_load(machine, write_term(machine.heap, machine.operators, *failure, {}, {}));
  return -1;
}

}  // namespace mosa
