// 計画 Task 4、ADR 0009。節の変数のセル、頭部の単一化、項の組み立てを C++ の文にする。
// 頭部は引数ごとに deref してタグで分け、未束縛なら組み立てて bind、同じ形なら中へ進む。
// 単一化の向きは enter_clause と同じく節の側が左（do_unify の左が節の項）。変数どうしでは節の変数が
// 呼び手の変数を指すので、標準順序の代表が変わらない（native.pl の native_var_head）。

#include "emit_state.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace mosa::codegen {
namespace {

// 照合の入れ子の深さの上限。これより深い部分項は組み立ててから rt::unify に渡す。enter_clause と同じ
// 意味のままで、長いリストの頭部でも生成コードの括弧の入れ子がコンパイラの上限に届かない。
constexpr int kMatchDepth = 6;
// 照合する部分項の語の数の上限。照合の枝は未束縛の側で部分項を丸ごと組み立てるので、深さごとに組み立てが重なる。
// 長いリストの事実（2 万要素）では生成コードが深さの倍だけ膨れ、コンパイルに何分もかかった。大きい項は 1 回だけ
// 組み立てて rt::unify に渡す（kMatchDepth と同じく意味は変わらない）。
constexpr std::size_t kMatchWords = 256;

void count_slots(const IrTerm& term, std::vector<SlotState>& slots) {
  walk_term(term, [&](const IrTerm& sub, int) {
    if (sub.tag == IrTag::Var && sub.slot < slots.size()) slots[sub.slot].left += 1;
    return true;
  });
}

// 頭部を前順にたどり、各スロットが最初に出た深さを記す。-1 は頭部に出ない（本体だけの変数）。
void first_depths(const IrTerm& term, std::vector<int>& firsts) {
  walk_term(term, [&](const IrTerm& sub, int depth) {
    if (sub.tag == IrTag::Var && sub.slot < firsts.size() && firsts[sub.slot] < 0) firsts[sub.slot] = depth;
    return true;
  });
}

// 組み立てに要るヒープの語の数。即値のアトムと整数と変数は語を取らない。
std::size_t build_words(const Impl& machine, const IrTerm& term) {
  std::size_t words = 0;
  walk_term(term, [&](const IrTerm& sub, int) {
    switch (sub.tag) {
      case IrTag::Anon:
      case IrTag::Float: words += 1; return false;
      case IrTag::Int: words += is_small_int(sub.integer) ? 0 : 1; return false;
      case IrTag::List:
      case IrTag::Fun: words += sub.tag == IrTag::List || is_dot(machine, sub) ? 2 : 1 + sub.args.size(); return true;
      default: return false;
    }
  });
  return words;
}

std::string atom_expr(ClauseEmit& emit, std::uint32_t atom) {
  return "rt::atom_cell(r.atoms[" + std::to_string(emit.tables.atom(atom)) + "])";
}

// 語を取らない項の式。取る項は nullopt。
std::optional<std::string> atomic_expr(ClauseEmit& emit, const IrTerm& term) {
  if (term.tag == IrTag::Atom) return atom_expr(emit, term.atom);
  if (term.tag == IrTag::Int && is_small_int(term.integer)) return "rt::small_int(" + int_literal(term.integer) + ")";
  return std::nullopt;
}

std::string use_var(ClauseEmit& emit, std::uint16_t slot) {
  SlotState& state = emit.slots[slot];
  state.seen = true;
  state.left -= 1;
  return var_name(slot);
}

std::string at(std::string_view block, std::size_t offset) {
  return offset == 0 ? std::string{block} : std::string{block} + " + " + std::to_string(offset);
}

// 組み立て途中の複合項。子の式を集め終えたら親の文を出す。
struct PlaceFrame {
  const IrTerm* term;
  std::string here;
  bool list;
  std::vector<std::string> parts;
};

// 複合項でない項の式。語を取る項は塊の次の語に置く。複合項は nullopt で、呼び手が枠を積む。
std::optional<std::string> place_leaf(ClauseEmit& emit, const IrTerm& term, std::string_view block, std::size_t& offset) {
  if (term.tag == IrTag::Var) return use_var(emit, term.slot);
  if (const auto atomic = atomic_expr(emit, term)) return *atomic;
  const std::string here = at(block, offset);
  switch (term.tag) {
    case IrTag::Anon: offset += 1; return "rt::put_ref(" + here + ")";
    case IrTag::Int: offset += 1; return "rt::put_int_box(" + here + ", " + int_literal(term.integer) + ")";
    case IrTag::Float: offset += 1; return "rt::put_float(" + here + ", " + float_literal(term.floating) + ")";
    default: return std::nullopt;
  }
}

std::string finish_compound(ClauseEmit& emit, const PlaceFrame& frame) {
  const std::string name = emit.fresh("t");
  if (frame.list) {
    emit.line("const Cell " + name + " = rt::put_list(" + frame.here + ", " + frame.parts[0] + ", " + frame.parts[1] + ");");
    return name;
  }
  std::string items;
  for (const std::string& part : frame.parts) items += (items.empty() ? "" : ", ") + part;
  const std::string args = emit.fresh("a");
  emit.line("const std::array<Cell, " + std::to_string(frame.parts.size()) + "> " + args + "{" + items + "};");
  emit.line("const Cell " + name + " = rt::put_struct(" + frame.here + ", r.atoms[" +
            std::to_string(emit.tables.atom(frame.term->atom)) + "], " + args + ");");
  return name;
}

// 塊の語を前順に割り当てる。複合項は親を先に置き、子の式は親の文より前に出す。長いリストでも C++ の呼び出しを
// 積まないよう、組み立て途中の複合項を手で積む（walk_term と同じ理由）。
std::string place(ClauseEmit& emit, const IrTerm& root, std::string_view block, std::size_t& offset) {
  std::vector<PlaceFrame> stack;
  const auto open = [&](const IrTerm& term) -> std::optional<std::string> {
    if (auto leaf = place_leaf(emit, term, block, offset)) return leaf;
    const bool list = term.tag == IrTag::List || is_dot(emit.machine, term);
    stack.push_back(PlaceFrame{&term, at(block, offset), list, {}});
    offset += list ? 2 : 1 + term.args.size();
    return std::nullopt;
  };
  if (auto leaf = open(root)) return *leaf;
  while (true) {
    PlaceFrame& top = stack.back();
    if (top.parts.size() < top.term->args.size()) {
      const IrTerm& next = top.term->args[top.parts.size()];
      // open が枠を積むと top は無効になるので、積まなかったときだけ書く。
      if (auto leaf = open(next)) stack.back().parts.push_back(std::move(*leaf));
      continue;
    }
    std::string done = finish_compound(emit, top);
    stack.pop_back();
    if (stack.empty()) return done;
    stack.back().parts.push_back(std::move(done));
  }
}

void match(ClauseEmit& emit, const IrTerm& term, const std::string& expr, int depth);

void match_var(ClauseEmit& emit, std::uint16_t slot, const std::string& expr) {
  SlotState& state = emit.slots[slot];
  state.left -= 1;
  if (state.seen) {
    emit.line("if (!rt::unify(m, " + var_name(slot) + ", " + expr + ")) return rt::fail(m);");
    return;
  }
  state.seen = true;
  if (state.left <= 0) return;
  // 節の入口で作ったセルは捨て、呼び手の項を値にする。enter_clause が新しい変数を束ねるのと同じ。
  if (state.allocated) emit.line(var_name(slot) + " = " + expr + ";");
  else emit.line("const Cell " + var_name(slot) + " = " + expr + ";");
}

void match_atomic(ClauseEmit& emit, const std::string& value, const std::string& expr) {
  const std::string cell = emit.fresh("c");
  emit.line("{");
  emit.depth += 1;
  emit.line("const Cell " + cell + " = rt::deref(" + expr + ");");
  emit.line("if (rt::unbound(" + cell + ")) rt::bind(m, " + cell + ", " + value + ");");
  emit.line("else if (" + cell + ".raw() != " + value + ".raw()) return rt::fail(m);");
  emit.depth -= 1;
  emit.line("}");
}

// 組み立ててから単一化する。浮動小数と箱の整数は値で比べるので do_unify に任せる。
void match_built(ClauseEmit& emit, const IrTerm& term, const std::string& expr) {
  const std::string built = emit_build(emit, term);
  emit.line("if (!rt::unify(m, " + built + ", " + expr + ")) return rt::fail(m);");
}

std::string shape_test(ClauseEmit& emit, const IrTerm& term, const std::string& cell) {
  if (term.tag == IrTag::List || is_dot(emit.machine, term)) return cell + ".tag() == Tag::Lis";
  return cell + ".tag() == Tag::Str && rt::functor_word(" + cell + ") == mosa::pack_functor(r.atoms[" +
         std::to_string(emit.tables.atom(term.atom)) + "], " + std::to_string(term.args.size()) + ")";
}

std::string sub_cell(const std::string& cell, std::size_t index, bool list) {
  if (list) return (index == 0 ? "rt::head(" : "rt::tail(") + cell + ")";
  return "rt::struct_arg(" + cell + ", " + std::to_string(index) + ")";
}

// 未束縛なら組み立てて束ね、同じ形なら引数ごとに照合する。2 つの枝は同じ変数を同じ回数だけ使う。
void match_compound(ClauseEmit& emit, const IrTerm& term, const std::string& expr, int depth) {
  const bool list = term.tag == IrTag::List || is_dot(emit.machine, term);
  const std::string cell = emit.fresh("c");
  emit.line("{");
  emit.depth += 1;
  emit.line("const Cell " + cell + " = rt::deref(" + expr + ");");
  emit.line("if (rt::unbound(" + cell + ")) {");
  emit.depth += 1;
  const std::vector<SlotState> before = emit.slots;
  const std::string built = emit_build(emit, term);
  emit.line("rt::bind(m, " + cell + ", " + built + ");");
  emit.slots = before;
  emit.depth -= 1;
  emit.line("} else if (" + shape_test(emit, term, cell) + ") {");
  emit.depth += 1;
  for (std::size_t index = 0; index < term.args.size(); ++index) {
    match(emit, term.args[index], sub_cell(cell, index, list), depth + 1);
  }
  emit.depth -= 1;
  emit.line("} else {");
  emit.line("  return rt::fail(m);");
  emit.line("}");
  emit.depth -= 1;
  emit.line("}");
}

void match(ClauseEmit& emit, const IrTerm& term, const std::string& expr, int depth) {
  switch (term.tag) {
    case IrTag::Anon: return;
    case IrTag::Var: return match_var(emit, term.slot, expr);
    case IrTag::Atom: return match_atomic(emit, atom_expr(emit, term.atom), expr);
    case IrTag::Int:
      if (is_small_int(term.integer)) return match_atomic(emit, "rt::small_int(" + int_literal(term.integer) + ")", expr);
      return match_built(emit, term, expr);
    case IrTag::Float: return match_built(emit, term, expr);
    case IrTag::List:
    case IrTag::Fun:
      if (depth >= kMatchDepth || build_words(emit.machine, term) > kMatchWords) return match_built(emit, term, expr);
      return match_compound(emit, term, expr, depth);
    default: return match_built(emit, term, expr);
  }
}

}  // namespace

std::string var_name(std::uint16_t slot) { return "v" + std::to_string(slot); }

// 未束縛のまま残りうる変数のセルを、enter_clause と同じくスロットの順に作る（Review Focus 1）。
// 本体の回数は emit_body.cpp が届くゴールだけを数えたもの。fail のあとの変数を作ると使われない宣言になる。
void plan_slots(const Clause& clause, const std::vector<int>& body_counts, ClauseEmit& emit) {
  emit.slots.assign(clause.vars, SlotState{});
  count_slots(clause.head, emit.slots);
  for (std::size_t slot = 0; slot < emit.slots.size() && slot < body_counts.size(); ++slot) {
    emit.slots[slot].left += body_counts[slot];
  }
  std::vector<int> firsts(clause.vars, -1);
  if (clause.head.tag == IrTag::Fun) {
    for (const IrTerm& arg : clause.head.args) first_depths(arg, firsts);
  }
  for (std::size_t slot = 0; slot < emit.slots.size(); ++slot) {
    emit.slots[slot].allocated = emit.slots[slot].left > 0 && firsts[slot] != 0;
  }
}

void emit_slot_cells(ClauseEmit& emit) {
  std::size_t count = 0;
  for (const SlotState& state : emit.slots) count += state.allocated ? 1 : 0;
  if (count == 0) return;
  emit.line("auto* cells = rt::alloc(m, " + std::to_string(count) + ");");
  std::size_t offset = 0;
  for (std::size_t slot = 0; slot < emit.slots.size(); ++slot) {
    if (!emit.slots[slot].allocated) continue;
    emit.line("Cell " + var_name(static_cast<std::uint16_t>(slot)) + " = rt::put_ref(" + at("cells", offset) + ");");
    offset += 1;
  }
}

// meta_predicate で限定する引数は、呼び手の文脈（ゴール枠の語 1 のモジュール）で M: に包んでから照合する
// （enter_clause の meta_argument）。包んだ項が節の変数の値になるので、本体はそれを呼ぶ。
void emit_head(ClauseEmit& emit, const IrTerm& head, const Predicate& predicate) {
  if (head.tag != IrTag::Fun) return;
  for (std::size_t index = 0; index < head.args.size(); ++index) {
    std::string actual = "rt::arg(r, frame, " + std::to_string(index) + ")";
    if (index < predicate.meta.size() && predicate.meta[index].kind != MetaArg::Kind::None) {
      actual = "rt::qualify(m, " + actual + ", rt::frame_module(r, frame))";
    }
    match(emit, head.args[index], actual, 0);
  }
}

// 項を 1 つの塊に組み立て、そのセルの式を返す。語を取らない項は式だけで文を出さない。
std::string emit_build(ClauseEmit& emit, const IrTerm& term) {
  const std::size_t words = build_words(emit.machine, term);
  if (words == 0) {
    if (term.tag == IrTag::Var) return use_var(emit, term.slot);
    if (const auto atomic = atomic_expr(emit, term)) return *atomic;
  }
  const std::string block = emit.fresh("b");
  emit.line("auto* " + block + " = rt::alloc(m, " + std::to_string(words) + ");");
  std::size_t offset = 0;
  return place(emit, term, block, offset);
}

}  // namespace mosa::codegen
