// 計画 Task 5。翻訳のときの静的な警告。export したのに定義の無い述語と、どこにも定義の無い述語を呼ぶ節を、
// Prolog の行で知らせる（SPEC 第6節のエラーの扱い）。export の誤りは翻訳を止め、呼び出しは警告だけで続ける
// （レビュー L12）ので、2 つを別の関数にする。差分用の --units（Task 11）は存在エラーを確かめるテストを
// 訳すので、呼び出しの警告を呼ばない。

#include "../module/module.hpp"
#include "emit_state.hpp"
#include "mosa/writer.hpp"
#include "project.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <string_view>
#include <vector>

namespace mosa::codegen {
namespace {

std::string indicator(const Impl& machine, const PredKey& key) {
  std::string text;
  if (key.module != machine.user_module) text = std::string{machine.atoms.name(key.module)} + ":";
  return text + std::string{machine.atoms.name(key.name)} + "/" + std::to_string(key.arity);
}

// collect_goals が辿る項。data は \+ の中のデータの形の項で、goal_view がゴールの形に直す位置にある。
struct Collected {
  const IrTerm* goal = nullptr;
  std::uint32_t module = 0;
  bool data = false;
};

// 辿るときの印。data の項は goal_view と同じに読む（, ; -> *-> と catch/3 は構文、true fail false ! halt と throw/1
// halt/1 は何も名指さないので True にする）。ゴールの形の項はそのまま。
IrTag collected_tag(const Impl& machine, const IrTerm& goal, bool data) {
  if (!data || (goal.tag != IrTag::Atom && goal.tag != IrTag::Fun)) return goal.tag;
  const auto name = machine.atoms.name(goal.atom);
  if (goal.tag == IrTag::Atom) {
    const bool control = name == "true" || name == "fail" || name == "false" || name == "!" || name == "halt";
    return control ? IrTag::True : IrTag::Atom;
  }
  const std::size_t arity = goal.args.size();
  if (arity == 2 && name == ",") return IrTag::And;
  if (arity == 2 && name == ";") return IrTag::Or;
  if (arity == 2 && name == "->") return IrTag::IfThen;
  if (arity == 2 && name == "*->") return IrTag::SoftIf;
  if (arity == 3 && name == "catch") return IrTag::Catch;
  if (arity == 1 && (name == "throw" || name == "halt")) return IrTag::True;
  return IrTag::Fun;
}

// 本体の中で、その場で呼ぶゴールだけを、本体に現れる順に集める。meta 引数（findall/3 のゴールなど）と変数のゴールは、
// 何を呼ぶか読み込みの時点では決まらないので見ない。M:G は M で引く。\+ の中は翻訳器（emit_walk.cpp）が
// 生成コードに展開するので、同じようにゴールの形として読む。手で積むスタックで辿り、\+ の中も写さずにその場で読む
// （前は 1 段ごとに C++ の呼び出しと goal_view の写しを重ね、深い本体で落ちた。ADR 0011 の改訂 5）。
void collect_goals(const Impl& machine, const IrTerm& root, std::uint32_t root_module, std::vector<PredKey>& out) {
  std::deque<IrTerm> views;  // Call の中をゴールの形に直した物
  std::vector<Collected> pending{Collected{&root, root_module, false}};
  while (!pending.empty()) {
    const Collected item = pending.back();
    pending.pop_back();
    const IrTerm& goal = *item.goal;
    const auto push = [&pending](const IrTerm& sub, std::uint32_t module, bool data) {
      pending.push_back(Collected{&sub, module, data});
    };
    switch (collected_tag(machine, goal, item.data)) {
      case IrTag::And:
      case IrTag::Or:
      case IrTag::IfThen:
      case IrTag::SoftIf:
        push(goal.args[1], item.module, item.data);
        push(goal.args[0], item.module, item.data);
        continue;
      case IrTag::Catch:
        push(goal.args[2], item.module, item.data);
        push(goal.args[0], item.module, item.data);
        continue;
      // ADR 0014。ゴール位置に変数を含む catch/3 の Goal と Recovery。中身は catch が動くときに訳すが、字面で名指す
      // 述語は今と同じに見る。変数は goal_view の後も Var のままで、何も集めない。
      // 呼べない部分（数）のあるゴールは call/1 が何も動かす前に拒むので（レビュー M15）、名指す述語は呼ばれない。
      // 1 回だけ現れる変数（Anon）は束縛で呼べるかが決まるので、ここでは拒まない。
      case IrTag::Call:
        if (goal.args.empty()) continue;
        views.push_back(goal_view(machine, goal.args[0]));
        if (!goal_leaves_all(views.back(), [](IrTag tag) { return !is_number_ir(tag); })) {
          views.pop_back();
          continue;
        }
        push(views.back(), item.module, false);
        continue;
      case IrTag::Atom: out.push_back(PredKey{item.module, goal.atom, 0}); continue;
      case IrTag::Fun:
        // M:G の G は項のまま（データの形）で残るので、, ; -> などを構文として読む。前はデータの ','/2 を 1 つの呼び出しと
        // 見て中を辿らず、M:(undef, true) を知らせなかった（レビュー M-46）。ゴールの形の G は data でも変わらない。
        if (machine.atoms.name(goal.atom) == ":" && goal.args.size() == 2 && goal.args[0].tag == IrTag::Atom) {
          push(goal.args[1], goal.args[0].atom, true);
          continue;
        }
        if (is_name(machine, goal, "\\+", 1) && (goal.args[0].tag == IrTag::Atom || goal.args[0].tag == IrTag::Fun)) {
          push(goal.args[0], item.module, true);
          continue;
        }
        out.push_back(PredKey{item.module, goal.atom, static_cast<std::uint16_t>(goal.args.size())});
        continue;
      // Big は数なので述語を名指さない。呼べない葉は Call の節点で call/1 が拒む。
      case IrTag::Big:
      default: continue;
    }
  }
}

// :- dynamic(~w). の案内は貼ればそのまま読める形にする。'Foo' や 'a b' は引用が要るので、名前/アリティの項を
// writeq の形（write_term の quoted）で書く。演算子の名前は書き手が括弧に入れる（7.10.5、(-)/1）。記号文字で
// 終わる名前（+++ など）は書き手なら +++ /1 と空白で分けるが、案内では (+++)/1 と括弧で区切って見せる（6.4.2）。
std::string dynamic_indicator(Impl& machine, const PredKey& key) {
  const OperatorView operators = op_view(machine, key.module);
  const Cell name_cell = machine.heap.atom(key.name);
  const std::string name = write_term(machine.heap, operators, name_cell, WriteOptions{}, {});
  constexpr std::string_view kSymbolChars = "+-*/\\^<>=~:.?@#&$";
  const bool glues = !name.empty() && kSymbolChars.find(name.back()) != std::string_view::npos;
  if (glues && operators.lookup(machine.atoms.name(key.name)).empty()) {
    return "(" + name + ")/" + std::to_string(key.arity);
  }
  const std::array<Cell, 2> args{name_cell, machine.heap.integer(key.arity)};
  const Cell indicator = machine.heap.structure(machine.atoms.intern("/"), args);
  return write_term(machine.heap, operators, indicator, WriteOptions{}, {});
}

// 組込、モジュールと取り込みの連鎖で引ける述語、宣言だけの手続き（iso-notes 7.4.2）を定義とみなす。
bool defined(Impl& machine, const PredKey& key) {
  if (is_builtin_procedure(machine, key.name, key.arity)) return true;
  if (resolve_pred(machine, key.module, key.name, key.arity) != nullptr) return true;
  return declared_only(machine, key.module, key.name, key.arity);
}

// name のモジュールのファイルの記録。原文の無いモジュール（user、system）には無い。
const TranslationLog::File* module_file(const Impl& machine, const TranslationLog& log, std::uint32_t name) {
  const Module* module = find_module(machine, name);
  if (module == nullptr) return nullptr;
  const auto file = log_file(log, module->file);
  return file ? &log.files[*file] : nullptr;
}

}  // namespace

// ビルド時に取らなかった分岐を持つファイルのモジュールは、その分岐が定義するかもしれないので、呼び手が止めずに
// 警告にする。untaken でその 2 つを分けて答える（ADR 0027）。reexport した述語は、定義したモジュールのファイルの
// 取らなかった分岐でも警告の側に入れる。
std::vector<std::string> export_warnings(Impl& machine, const TranslationLog& log, const DisplayNames& names,
                                         bool untaken) {
  std::vector<std::string> lines;
  for (const Module& module : machine.modules) {
    const auto file = log_file(log, module.file);
    // 読めなかったファイルのモジュールは、定義が翻訳器に見えないので確かめない。
    if (!file || log.files[*file].library || log.files[*file].opaque) continue;
    for (const Export& exported : module.exports) {
      // 定義は公開の項目が指す、定義したモジュールとそこでの名前で確かめる。reexport の項目はほかのモジュールの
      // 述語を指す（ADR 0026）。警告は、このモジュールが公開した名前で出す。
      const PredKey key{exported.from, exported.from_name, exported.arity};
      // reexport した述語は、定義したモジュールのファイルの印でも見る。同じモジュールを 2 つ目に reexport した側は
      // そのファイルを読み直さないので、印が上がってこない（I2 の直し 3 回目の続きのレビュー A1）。
      const TranslationLog::File* from =
          exported.from == module.name ? nullptr : module_file(machine, log, exported.from);
      if (from != nullptr && from->opaque) continue;
      if ((log.files[*file].untaken_branch || (from != nullptr && from->untaken_branch)) != untaken) continue;
      // 組込を公開し直す export（library(lists) の length/2。SPEC 第5節の表）は定義がある物とみなす。
      if (find_pred(machine, key) != nullptr || declared_only(machine, key.module, key.name, key.arity) ||
          is_builtin_procedure(machine, key.name, key.arity)) {
        continue;
      }
      const PredKey shown{module.name, exported.name, exported.arity};
      const auto& place = log.files[*file];
      lines.push_back(names[*file] + ":" + std::to_string(place.line) + ":" + std::to_string(place.column) + ": " +
                      message_text(machine, "mosac_export_undefined", {indicator(machine, shown)}));
    }
  }
  return lines;
}

std::vector<std::string> call_warnings(Impl& machine, const TranslationLog& log, const DisplayNames& names) {
  std::vector<std::string> lines;
  for (const TranslationLog::Site& site : log.sites) {
    if (log.files[site.file].library) continue;
    const Predicate* predicate = find_pred(machine, site.pred);
    if (predicate == nullptr || site.clause >= predicate->clauses.size()) continue;
    std::vector<PredKey> goals;
    collect_goals(machine, *predicate->clauses[site.clause].body, site.pred.module, goals);
    std::vector<std::string> seen;
    for (const PredKey& goal : goals) {
      if (defined(machine, goal)) continue;
      const std::string text = indicator(machine, goal);
      if (std::find(seen.begin(), seen.end(), text) != seen.end()) continue;
      seen.push_back(text);
      // assertz だけで作る述語もここに来る。翻訳は止めない（レビュー L12、持ち主の決定 A）。直し方として、定義する
      // 側のファイルに書く :- dynamic(Name/Arity). を示す（ISO の演算子表に dynamic は無いので括弧の形）。モジュールの
      // 中ではモジュールを付けずに書くので、名前とアリティだけにする。
      lines.push_back(names[site.file] + ":" + std::to_string(site.line) + ": " +
                      message_text(machine, "mosac_undefined_call", {text, dynamic_indicator(machine, goal)}));
    }
  }
  return lines;
}

}  // namespace mosa::codegen
