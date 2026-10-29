// 計画 Task 4、ADR 0009。モジュールの静的述語から翻訳単位の C++ を組み立てる。
// 述語ごとに、節の関数（emit_head.cpp、emit_body.cpp、emit_control.cpp）、第1引数で節を選ぶ関数、状態の switch、
// 生成関数 p_名前_アリティを出し、最後にアトムと呼び先と NativeEntry の表と unit() を置く。
// 生成関数を出す述語は install_unit と同じ native_eligible で選び、指紋も同じ clause_fingerprint で出す。

#include "codegen.hpp"
#include "emit_state.hpp"

#include "mosa/writer.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mosa::codegen {
namespace {

// 生成コードの補助。どの述語も first_of を使い、same_float は浮動小数の鍵があるときだけ使う。
constexpr std::string_view kHelpers = R"(using Run = void (*)(Machine::Impl& m, const rt::Regs& r, std::uint64_t frame, std::uint32_t state);

// 候補の残りを選択点に積んでから先頭の節に入る。push_alternative はこの時点のヒープとトレイルの高さを
// 取るので、節の束縛より先に呼ぶ（enter_user と同じ順）。
void first_of(Machine::Impl& m, const rt::Regs& r, std::uint64_t frame, rt::NativeProc proc, Run run,
              std::span<const std::uint32_t> states) {
  if (states.empty()) return rt::fail(m);
  if (states.size() > 1) rt::push_alternative(m, r, frame, proc, states.subspan(1));
  run(m, r, frame, states.front());
}

// 第1引数の選び方は select_clauses と同じ。NaN どうしは等しく、0.0 と -0.0 も等しい。
[[maybe_unused]] bool same_float(Cell cell, double value) {
  const double actual = std::bit_cast<double>(*rt::cell_words(cell));
  if (actual != actual || value != value) return actual != actual && value != value;
  return actual == value;
}

)";

constexpr std::string_view kUsings = "using mosa::Cell;\nusing mosa::Machine;\nusing mosa::Tag;\nnamespace rt = mosa::rt;\n\n";

// 単位とモジュールの名前は引用したアトムなら改行を含みうる。1 行のコメントを壊さない。
std::string comment_safe(std::string_view text) {
  std::string out{text};
  for (char& unit : out) {
    if (unit == '\n' || unit == '\r') unit = ' ';
  }
  return out;
}

bool same_float_value(double left, double right) {
  if (std::isnan(left) || std::isnan(right)) return std::isnan(left) && std::isnan(right);
  return left == right;
}

std::size_t node_count(const IrTerm& term) {
  std::size_t count = 1;
  for (const IrTerm& arg : term.args) count += node_count(arg);
  return count;
}

void count_uses(const IrTerm& term, std::vector<int>& uses) {
  if (term.tag == IrTag::Var && term.slot < uses.size()) uses[term.slot] += 1;
  for (const IrTerm& arg : term.args) count_uses(arg, uses);
}

// コメント用に節を writeq の形で書く。IR を項に戻すのは clause/2 と同じ ir_to_term で、
// 書くヒープは元の Machine を変えないよう写したアトムの表を持つ別の Impl に置く。
class ClauseText {
 public:
  ClauseText(const Impl& source, std::uint32_t module, std::size_t limit)
      : source_(source), module_(module), scratch_(std::make_unique<Impl>(limit)) {
    scratch_->atoms = source.atoms;
  }

  // 節を writeq の形で書いた文（終わりの . を含む）。コメントと --debug の追跡が同じ文を使う。
  [[nodiscard]] std::string text(const Clause& clause) {
    const auto mark = scratch_->heap.size();
    std::vector<Cell> slots;
    const Cell head = ir_to_term(*scratch_, clause.head, slots, false);
    const Cell body = ir_to_term(*scratch_, clause.body, slots, true);
    const std::vector<VarName> names = var_names(clause, slots);
    WriteOptions options;
    options.numbervars = true;
    options.space_args = true;
    const OperatorView operators = op_view(source_, module_);
    std::string text = write_term(scratch_->heap, operators, head, options, names);
    if (clause.body.tag != IrTag::True) text += " :- " + write_term(scratch_->heap, operators, body, options, names);
    scratch_->heap.truncate(mark);
    return text + ".";
  }

 private:
  // 原文の名前を使う。名前の無い変数（原文の _）は 1 回だけなら _、2 回以上なら _スロット。
  static std::vector<VarName> var_names(const Clause& clause, const std::vector<Cell>& slots) {
    std::vector<int> uses(slots.size(), 0);
    count_uses(clause.head, uses);
    count_uses(clause.body, uses);
    std::vector<VarName> names;
    for (std::size_t slot = 0; slot < slots.size(); ++slot) {
      if (slots[slot].raw() == 0) continue;
      std::string name = slot < clause.names.size() ? clause.names[slot] : std::string{};
      if (name.empty()) name = uses[slot] <= 1 ? "_" : "_" + std::to_string(slot);
      names.push_back(VarName{std::move(name), slots[slot]});
    }
    return names;
  }

  const Impl& source_;
  std::uint32_t module_;
  std::unique_ptr<Impl> scratch_;
};

bool translatable(const Predicate& predicate, std::uint32_t module) {
  return predicate.module == module && !predicate.dynamic && !predicate.abolished && !predicate.clauses.empty();
}

std::size_t scratch_limit(const Impl& machine, std::uint32_t module) {
  std::size_t nodes = 0;
  for (const Predicate& predicate : machine.predicates) {
    if (!translatable(predicate, module)) continue;
    for (const Clause& clause : predicate.clauses) {
      nodes = std::max(nodes, node_count(clause.head) + node_count(clause.body));
    }
  }
  return std::max(std::size_t{1} << 14, nodes * 4 + 64);
}

bool any_key(KeyKind kind) { return kind == KeyKind::Any || kind == KeyKind::Var || kind == KeyKind::None; }

bool same_key(const Clause& left, const Clause& right) {
  if (left.key != right.key) return false;
  switch (left.key) {
    case KeyKind::Atom: return left.key_atom == right.key_atom;
    case KeyKind::Int: return left.key_int == right.key_int;
    case KeyKind::Float: return same_float_value(left.key_float, right.key_float);
    case KeyKind::Fun: return left.key_atom == right.key_atom && left.key_arity == right.key_arity;
    default: return true;
  }
}

// select_clauses の matches と同じ判定を、第1引数のセル first に対して書く。
std::string key_test(UnitTables& tables, const Clause& clause) {
  const auto atom = [&] { return "r.atoms[" + std::to_string(tables.atom(clause.key_atom)) + "]"; };
  switch (clause.key) {
    case KeyKind::Atom: return "first.raw() == rt::atom_cell(" + atom() + ").raw()";
    case KeyKind::Int:
      if (is_small_int(clause.key_int)) return "first.raw() == rt::small_int(" + int_literal(clause.key_int) + ").raw()";
      return "first.tag() == Tag::IntBox && *rt::cell_words(first) == std::uint64_t{" +
             hex64(std::bit_cast<std::uint64_t>(clause.key_int)) + "}";
    case KeyKind::Float: return "first.tag() == Tag::Float && same_float(first, " + float_literal(clause.key_float) + ")";
    case KeyKind::List: return "first.tag() == Tag::Lis";
    default:
      return "first.tag() == Tag::Str && rt::functor_word(first) == mosa::pack_functor(" + atom() + ", " +
             std::to_string(clause.key_arity) + ")";
  }
}

std::string states_array(std::string_view name, const std::vector<std::uint32_t>& clauses) {
  std::string items;
  for (const std::uint32_t clause : clauses) items += (items.empty() ? "" : ", ") + state_text(clause, 0);
  return "  static constexpr std::array<std::uint32_t, " + std::to_string(clauses.size()) + "> " + std::string{name} +
         "{" + items + "};\n";
}

// 状態 0。第1引数が未束縛なら全部の節、束縛済みなら鍵の合う節と鍵の無い節（select_clauses と同じ）。
std::string dispatch_text(UnitTables& tables, const Predicate& predicate, const std::string& proc) {
  const auto& clauses = predicate.clauses;
  std::vector<std::uint32_t> all;
  std::vector<std::uint32_t> other;
  for (std::uint32_t index = 0; index < clauses.size(); ++index) {
    all.push_back(index + 1);
    if (any_key(clauses[index].key)) other.push_back(index + 1);
  }
  const std::string call = "first_of(m, r, frame, &" + proc + ", &" + proc + "_run, ";
  std::string out = "void " + proc + "_call(Machine::Impl& m, const rt::Regs& r, std::uint64_t frame) {\n";
  out += states_array("kAll", all);
  if (predicate.arity == 0 || wraps_first(predicate)) return out + "  return " + call + "kAll);\n}\n\n";
  std::string tests;
  std::vector<std::size_t> leaders;
  for (std::size_t index = 0; index < clauses.size(); ++index) {
    if (any_key(clauses[index].key)) continue;
    bool fresh = true;
    for (const std::size_t leader : leaders) fresh = fresh && !same_key(clauses[leader], clauses[index]);
    if (!fresh) continue;
    std::vector<std::uint32_t> chosen;
    for (std::uint32_t member = 0; member < clauses.size(); ++member) {
      if (any_key(clauses[member].key) || same_key(clauses[member], clauses[index])) chosen.push_back(member + 1);
    }
    const std::string name = "kKey" + std::to_string(leaders.size());
    out += states_array(name, chosen);
    tests += "  if (" + key_test(tables, clauses[index]) + ") return " + call + name + ");\n";
    leaders.push_back(index);
  }
  out += states_array("kOther", other);
  out += "  const Cell first = rt::deref(rt::arg(r, frame, 0));\n";
  out += "  if (rt::unbound(first)) return " + call + "kAll);\n" + tests;
  return out + "  return " + call + "kOther);\n}\n\n";
}

std::string run_text(const std::string& proc, const std::vector<std::pair<std::uint32_t, std::string>>& cases) {
  std::string out = "void " + proc + "_run(Machine::Impl& m, const rt::Regs& r, std::uint64_t frame, std::uint32_t state) {\n";
  out += "  switch (state) {\n    case rt::kCall: return " + proc + "_call(m, r, frame);\n";
  for (const auto& [state, statement] : cases) {
    out += "    case " + state_text(state >> 16, state & 0xFFFFU) + ": " + statement + "\n";
  }
  return out + "    default: return rt::fail(m);\n  }\n}\n\n";
}

// 節に入る状態（goal 0）の case の前に追跡を置く。kTrace は単位の constexpr bool で、偽なら if constexpr が
// 文字列ごと捨てる。追跡の文はコメントと同じ文にする（生成コードを読む人が対応を目で引ける）。
void add_trace(std::vector<std::pair<std::uint32_t, std::string>>& cases, const std::vector<std::string>& texts) {
  for (auto& [state, statement] : cases) {
    const std::uint32_t clause = state >> 16;
    if ((state & 0xFFFFU) != 0 || clause == 0 || clause > texts.size()) continue;
    statement = "if constexpr (kTrace) rt::trace_clause(" + string_literal(texts[clause - 1]) + "); " + statement;
  }
}

// 単位の中の文面。helpers は無名の名前空間、procs はその外に置く。
struct UnitText {
  std::string forward;
  std::string helpers;
  std::string procs;
  std::string entries;
  std::size_t entry_count = 0;
};

std::string proc_signature(const std::string& proc) {
  return "void " + proc + "(mosa::Machine::Impl& m, std::uint64_t frame, std::uint32_t state)";
}

std::string pred_label(const Impl& machine, const Predicate& predicate) {
  return comment_safe(machine.atoms.name(predicate.name)) + "/" + std::to_string(predicate.arity);
}

// 生成関数を 1 つ出す。どの節も IR のままなら出さない（出しても interpret_clause を呼ぶだけになる）。
void emit_predicate(const Impl& machine, UnitTables& tables, ClauseText& writer, const Predicate& predicate,
                    UnitText& text, EmitResult& result) {
  std::vector<std::string> texts;
  std::string comments;
  for (const Clause& clause : predicate.clauses) {
    texts.push_back(writer.text(clause));
    comments += "// % " + texts.back() + "\n";
  }
  const std::string label = pred_label(machine, predicate);
  if (!native_eligible(machine, predicate)) {
    result.interpreted += live_count(predicate);
    text.helpers += "// " + label + " は install_unit が付けない述語なので IR のまま動かす。\n" + comments + "\n";
    return;
  }
  std::vector<bool> plans;
  std::size_t native = 0;
  for (const Clause& clause : predicate.clauses) {
    plans.push_back(translatable_body(machine, clause.body));
    native += plans.back() ? 1 : 0;
  }
  if (native == 0) {
    result.interpreted += predicate.clauses.size();
    text.helpers += "// " + label + " はどの節も IR のまま動かす。\n" + comments + "\n";
    return;
  }
  result.translated += 1;
  result.interpreted += predicate.clauses.size() - native;
  const std::string proc = "p" + mangle(machine.atoms.name(predicate.name)) + "_" + std::to_string(predicate.arity);
  const std::string entry = std::to_string(text.entry_count++);
  std::vector<std::pair<std::uint32_t, std::string>> cases;
  text.helpers += comments;
  for (std::uint32_t index = 0; index < predicate.clauses.size(); ++index) {
    const std::uint32_t clause = index + 1;
    if (!plans[index]) {
      cases.emplace_back(rt::state_of(clause, 0),
                         "return rt::interpret_clause(m, r, " + entry + ", frame, " + std::to_string(clause) + ");");
      continue;
    }
    ClauseCode code = emit_clause(machine, tables, predicate, ClauseName{proc, clause});
    text.helpers += code.functions;
    for (auto& item : code.cases) cases.push_back(std::move(item));
  }
  add_trace(cases, texts);
  text.helpers += "void " + proc + "_run(Machine::Impl& m, const rt::Regs& r, std::uint64_t frame, std::uint32_t state);\n\n";
  text.helpers += dispatch_text(tables, predicate, proc) + run_text(proc, cases);
  text.forward += proc_signature(proc) + ";\n";
  text.procs += proc_signature(proc) + " {\n  " + proc + "_run(m, rt::regs(m, kUnit), frame, state);\n}\n\n";
  text.entries += "    rt::NativeEntry{" + string_literal(machine.atoms.name(predicate.module)) + ", " +
                  string_literal(machine.atoms.name(predicate.name)) + ", " + std::to_string(predicate.arity) +
                  ", std::uint64_t{" + hex64(clause_fingerprint(machine.atoms, predicate)) + "}, &" + proc + "},\n";
}

std::string table_text(std::string_view type, std::string_view name, std::size_t count, const std::string& items) {
  return "constexpr std::array<" + std::string{type} + ", " + std::to_string(count) + "> " + std::string{name} +
         "{\n" + items + "};\n";
}

std::string unit_tables(const Impl& machine, const UnitTables& tables) {
  std::string atoms;
  for (const std::uint32_t atom : tables.atoms) atoms += "    " + string_literal(machine.atoms.name(atom)) + ",\n";
  std::string calls;
  for (const CalleeRef& key : tables.calls) {
    calls += "    rt::NativeCallee{" + string_literal(machine.atoms.name(key.module)) + ", " +
             string_literal(key.name) + ", " + std::to_string(key.arity) + "},\n";
  }
  return "constinit rt::UnitTag kUnit{};\n" + table_text("std::string_view", "kAtoms", tables.atoms.size(), atoms) +
         table_text("rt::NativeCallee", "kCalls", tables.calls.size(), calls) + "\n";
}

std::string banner(const Impl& machine, std::uint32_t module, std::string_view unit_name) {
  return "// mosac が単位 " + comment_safe(unit_name) + " のモジュール " + comment_safe(machine.atoms.name(module)) +
         " から出した（ADR 0009）。手で直さない。\n";
}

// 名前空間。既定はモジュールから。mosac emit --units（Task 11）はファイルごとの名前を渡す。
std::string unit_space(const Impl& machine, std::uint32_t module, std::string_view space) {
  if (!space.empty()) return "mosa_gen::" + std::string{space};
  return "mosa_gen::m" + mangle(machine.atoms.name(module));
}

std::string source_text(const Impl& machine, std::uint32_t module, std::string_view unit_name,
                        const UnitTables& tables, const UnitText& text, bool trace, std::string_view space_name) {
  const std::string space = unit_space(machine, module, space_name);
  std::string out = banner(machine, module, unit_name) + "\n#include \"mosa/runtime.hpp\"\n\nnamespace " + space + " {\n\n";
  out += text.forward + "\nnamespace {\n\n" + std::string{kUsings} + unit_tables(machine, tables);
  if (text.entry_count > 0) {
    // mosac build --debug の節の追跡（計画 Task 9）。マクロを使わず、この値で生成コードが切り替わる。
    out += std::string{"constexpr bool kTrace = "} + (trace ? "true" : "false") + ";\n\n" + std::string{kHelpers};
  }
  out += text.helpers + "}  // namespace\n\n" + text.procs + "namespace {\n\n";
  out += table_text("rt::NativeEntry", "kEntries", text.entry_count, text.entries) + "\n}  // namespace\n\n";
  out += "mosa::rt::NativeUnit unit(std::string_view source) {\n";
  out += "  return mosa::rt::NativeUnit{" + string_literal(unit_name) + ", source, &kUnit, kEntries, kAtoms, kCalls};\n}\n\n";
  return out + "}  // namespace " + space + "\n";
}

std::string header_text(const Impl& machine, std::uint32_t module, std::string_view unit_name, const UnitText& text,
                        std::string_view space_name) {
  const std::string space = unit_space(machine, module, space_name);
  std::string out = "#pragma once\n\n" + banner(machine, module, unit_name) + "\n#include \"mosa/runtime.hpp\"\n\n";
  out += "namespace " + space + " {\n\n// 埋め込む原文を受け取り、rt::install_unit に渡す単位を返す。\n";
  out += "[[nodiscard]] mosa::rt::NativeUnit unit(std::string_view source);\n" + text.forward;
  return out + "\n}  // namespace " + space + "\n";
}

}  // namespace

std::size_t UnitTables::atom(std::uint32_t id) {
  for (std::size_t index = 0; index < atoms.size(); ++index) {
    if (atoms[index] == id) return index;
  }
  atoms.push_back(id);
  return atoms.size() - 1;
}

std::size_t UnitTables::call(std::uint32_t module, std::string_view name, std::uint16_t arity) {
  for (std::size_t index = 0; index < calls.size(); ++index) {
    if (calls[index].module == module && calls[index].name == name && calls[index].arity == arity) return index;
  }
  calls.push_back(CalleeRef{module, std::string{name}, arity});
  return calls.size() - 1;
}

void ClauseEmit::line(std::string_view text) {
  out.append(static_cast<std::size_t>(depth) * 2, ' ');
  out += text;
  out += '\n';
}

std::string ClauseEmit::fresh(std::string_view stem) { return std::string{stem} + std::to_string(temp++); }

std::string hex64(std::uint64_t value) {
  constexpr std::string_view digits = "0123456789abcdef";
  std::string out = "0x";
  for (int shift = 60; shift >= 0; shift -= 4) out += digits[(value >> shift) & 0x0FU];
  return out;
}

// INT64_MIN は 10 進の字面にすると unsigned の数の符号反転になる。
std::string int_literal(std::int64_t value) {
  if (value == std::numeric_limits<std::int64_t>::min()) return "(-9223372036854775807 - 1)";
  return std::to_string(value);
}

// 字面の 10 進では NaN と無限大と -0.0 を書けない。ビットの並びから戻す。
std::string float_literal(double value) {
  return "std::bit_cast<double>(std::uint64_t{" + hex64(std::bit_cast<std::uint64_t>(value)) + "})";
}

// UTF-8 のバイトは 3 桁の 8 進にする。MSVC の既定のコードページでも字面が変わらない。長さは明示する（NUL を含みうる）。
// 8 進の直後に 10 進の数字が続くと MSVC が /W4 で C4125 を出す（警告は誤りにしている）ので、そこで字面を一度閉じて
// 隣の字面としてつなぐ。テスト: tests/cpp/codegen_test.cpp の octal_before_digit。
std::string string_literal(std::string_view text) {
  std::string out = "std::string_view{\"";
  for (std::size_t at = 0; at < text.size(); ++at) {
    const char unit = text[at];
    const auto byte = static_cast<unsigned char>(unit);
    if (unit == '"' || unit == '\\') {
      out += '\\';
      out += unit;
    } else if (byte >= 0x20 && byte < 0x7F) {
      out += unit;
    } else {
      out += '\\';
      out += static_cast<char>('0' + (byte >> 6));
      out += static_cast<char>('0' + ((byte >> 3) & 7));
      out += static_cast<char>('0' + (byte & 7));
      if (at + 1 < text.size() && text[at + 1] >= '0' && text[at + 1] <= '9') out += "\" \"";
    }
  }
  return out + "\", " + std::to_string(text.size()) + "}";
}

std::string state_text(std::uint32_t clause, std::uint32_t goal) {
  return "rt::state_of(" + std::to_string(clause) + ", " + std::to_string(goal) + ")";
}

// Heap::integer と同じ境界（61 ビットの即値）。
bool is_small_int(std::int64_t value) {
  return value >= -(std::int64_t{1} << 60) && value < (std::int64_t{1} << 60);
}

// materialize は名前 '.' の 2 引数の構造をリストにする。翻訳でも同じ形を作る。
bool is_dot(const Impl& machine, const IrTerm& term) {
  return term.tag == IrTag::Fun && term.args.size() == 2 && machine.atoms.name(term.atom) == ".";
}

bool is_name(const Impl& machine, const IrTerm& term, std::string_view name, std::size_t arity) {
  return term.tag == IrTag::Fun && term.args.size() == arity && machine.atoms.name(term.atom) == name;
}

namespace {

bool listed(std::span<const PredKey> keys, const Predicate& predicate) {
  return std::any_of(keys.begin(), keys.end(), [&](const PredKey& key) {
    return key.module == predicate.module && key.name == predicate.name && key.arity == predicate.arity;
  });
}

}  // namespace

EmitResult emit_module(const Machine& machine, std::uint32_t module, std::string_view unit_name,
                       std::span<const PredKey> interpreted_only, bool trace, std::string_view space) {
  const Impl& impl = engine_impl(machine);
  EmitResult result;
  UnitTables tables;
  UnitText text;
  ClauseText writer{impl, module, scratch_limit(impl, module)};
  for (const Predicate& predicate : impl.predicates) {
    if (!translatable(predicate, module)) continue;
    result.predicates += 1;
    if (listed(interpreted_only, predicate)) {
      result.interpreted += live_count(predicate);
      continue;
    }
    emit_predicate(impl, tables, writer, predicate, text, result);
  }
  result.source = source_text(impl, module, unit_name, tables, text, trace, space);
  result.header = header_text(impl, module, unit_name, text, space);
  return result;
}

}  // namespace mosa::codegen
