// meta_predicate/1 の宣言と、呼び出しで引数に付ける限定（SPEC 第5節、ISO/IEC 13211-2 7.2.2）。
// モード: meta_predicate(+head)。引数は 0〜9、:、^、//、+、-、?、*。
// エラー: instantiation_error、type_error(callable, H)、domain_error(meta_argument_specifier, A)。
// テスト: tests/pl/modules.pl、tests/cpp/module_load_test.cpp
#include "module.hpp"

#include "../machine/internal.hpp"

#include <array>
#include <string_view>

namespace mosa {

namespace {

bool qualified(const Impl& machine, Cell cell) {
  return cell.tag() == Tag::Str && machine.heap.functor_arity(cell) == 2 &&
         machine.atoms.name(machine.heap.functor_atom(cell)) == ":";
}

std::optional<MetaArg> spec_of_atom(Impl& machine, std::string_view text) {
  if (text == ":") return MetaArg{MetaArg::Kind::Colon, 0, 0};
  if (text == "^") return MetaArg{MetaArg::Kind::Caret, 0, 0};
  if (text == "//") return MetaArg{MetaArg::Kind::Dcg, 0, 0};
  if (text == "+" || text == "-" || text == "?" || text == "*") {
    return MetaArg{MetaArg::Kind::None, 0, machine.atoms.intern(text)};
  }
  return std::nullopt;
}

std::optional<Cell> read_arg(Impl& machine, Cell cell, MetaArg& arg) {
  cell = machine.heap.deref(cell);
  if (machine.heap.unbound(cell)) return formal_instantiation(machine);
  if (cell.tag() == Tag::Int) {
    const auto value = machine.heap.integer_value(cell);
    if (value >= 0 && value <= 9) {
      arg = MetaArg{MetaArg::Kind::Goal, static_cast<std::uint8_t>(value), 0};
      return std::nullopt;
    }
  }
  if (cell.tag() == Tag::Atom) {
    if (const auto found = spec_of_atom(machine, machine.atoms.name(static_cast<std::uint32_t>(cell.raw() >> 3)))) {
      arg = *found;
      return std::nullopt;
    }
  }
  return formal_domain(machine, "meta_argument_specifier", cell);
}

Cell spec_cell(Impl& machine, const MetaArg& arg) {
  switch (arg.kind) {
    case MetaArg::Kind::Goal: return machine.heap.integer(arg.extra);
    case MetaArg::Kind::Colon: return machine.heap.atom(machine.atoms.intern(":"));
    case MetaArg::Kind::Caret: return machine.heap.atom(machine.atoms.intern("^"));
    case MetaArg::Kind::Dcg: return machine.heap.atom(machine.atoms.intern("//"));
    case MetaArg::Kind::None: break;
  }
  return machine.heap.atom(arg.mode);
}

Cell head_of(Impl& machine, std::uint32_t name, const std::vector<MetaArg>& args) {
  if (args.empty()) return machine.heap.atom(name);
  std::vector<Cell> cells;
  cells.reserve(args.size());
  for (const MetaArg& arg : args) cells.push_back(spec_cell(machine, arg));
  return machine.heap.structure(name, cells);
}

struct BuiltinMeta {
  std::string_view name;
  std::uint16_t arity;
  // 1 文字が引数 1 つ。数字は閉包の追加引数の数、ほかは同じ名前のアトム。
  std::string_view specs;
};

// 制御構文と、ゴールを受ける組込。call/N は表に置かず、アリティから作る。
constexpr auto kBuiltinMeta = std::to_array<BuiltinMeta>({
    {",", 2, "00"},     {";", 2, "00"},      {"->", 2, "00"},  {"*->", 2, "00"}, {"\\+", 1, "0"},
    {"once", 1, "0"},   {"catch", 3, "0?0"}, {"findall", 3, "?0-"}, {"findall", 4, "?0-?"},
    {"bagof", 3, "?^-"}, {"setof", 3, "?^-"}, {"forall", 2, "00"}, {"if", 3, "000"},
});

std::vector<MetaArg> parse_specs(Impl& machine, std::string_view specs) {
  std::vector<MetaArg> args;
  for (const char letter : specs) {
    if (letter >= '0' && letter <= '9') {
      args.push_back(MetaArg{MetaArg::Kind::Goal, static_cast<std::uint8_t>(letter - '0'), 0});
      continue;
    }
    args.push_back(spec_of_atom(machine, std::string_view{&letter, 1}).value_or(MetaArg{}));
  }
  return args;
}

}  // namespace

std::optional<Cell> read_meta_spec(Impl& machine, Cell head, std::vector<MetaArg>& args) {
  head = machine.heap.deref(head);
  if (machine.heap.unbound(head)) return formal_instantiation(machine);
  if (head.tag() == Tag::Atom) {
    args.clear();
    return std::nullopt;
  }
  if (head.tag() != Tag::Str) return formal_type(machine, "callable", head);
  std::vector<MetaArg> parsed;
  for (std::uint16_t index = 0; index < machine.heap.functor_arity(head); ++index) {
    MetaArg arg;
    if (const auto formal = read_arg(machine, machine.heap.arg(head, index), arg)) return formal;
    parsed.push_back(arg);
  }
  args = std::move(parsed);
  return std::nullopt;
}

Cell qualify_goal(Impl& machine, std::uint32_t context, Cell actual) {
  const Cell value = machine.heap.deref(actual);
  // 呼び手がもう M: を付けていれば、その M を使う。二重に包むと、行き先が呼び手の文脈に戻る。
  if (qualified(machine, value)) return actual;
  const std::array<Cell, 2> parts{machine.heap.atom(context), actual};
  return machine.heap.structure(machine.atoms.intern(":"), parts);
}

Cell meta_argument(Impl& machine, const Predicate& predicate, std::uint32_t context, std::size_t index,
                   Cell actual) {
  if (index >= predicate.meta.size() || predicate.meta[index].kind == MetaArg::Kind::None) return actual;
  return qualify_goal(machine, context, actual);
}

void adopt_meta(Impl& machine, Predicate& predicate) {
  Module* module = find_module(machine, predicate.module);
  if (module == nullptr) return;
  auto& pending = module->pending_meta;
  for (auto it = pending.begin(); it != pending.end(); ++it) {
    if (it->name != predicate.name || it->arity != predicate.arity) continue;
    predicate.meta = std::move(it->args);
    predicate.meta_declared = true;
    pending.erase(it);
    return;
  }
}

bool wraps_first(const Predicate& predicate) {
  return !predicate.meta.empty() && predicate.meta[0].kind != MetaArg::Kind::None;
}

std::optional<Cell> meta_head(Impl& machine, const Predicate& predicate) {
  if (predicate.meta.empty()) return std::nullopt;
  return head_of(machine, predicate.name, predicate.meta);
}

std::optional<Cell> builtin_meta_head(Impl& machine, std::uint32_t name, std::uint16_t arity) {
  const auto text = machine.atoms.name(name);
  if (text == "call" && arity >= 1) {
    std::vector<MetaArg> args{MetaArg{MetaArg::Kind::Goal, static_cast<std::uint8_t>(arity - 1), 0}};
    for (std::uint16_t index = 1; index < arity; ++index) args.push_back(*spec_of_atom(machine, "?"));
    return head_of(machine, name, args);
  }
  for (const BuiltinMeta& item : kBuiltinMeta) {
    if (item.name == text && item.arity == arity) return head_of(machine, name, parse_specs(machine, item.specs));
  }
  return std::nullopt;
}

}
