// 計画 Task 5。翻訳のときの静的な警告。export したのに定義の無い述語と、どこにも定義の無い述語を呼ぶ節を、
// Prolog の行で知らせる（SPEC 第6節のエラーの扱い）。差分用の --units（Task 11）は存在エラーを確かめる
// テストを翻訳するので、呼び出しの警告は export の警告と別の関数にして、呼ぶかどうかを選ばせる。

#include "emit_state.hpp"
#include "project.hpp"

#include <algorithm>
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

// 本体の中で、その場で呼ぶゴールだけを集める。meta 引数（findall/3 のゴールなど）と変数のゴールは、
// 何を呼ぶか読み込みの時点では決まらないので見ない。M:G は M で引く。\+ の中は翻訳器（emit_body.cpp）が
// 生成コードに展開するので、同じようにゴールの形に直して見る。
void collect_goals(const Impl& machine, const IrTerm& goal, std::uint32_t module, std::vector<PredKey>& out) {
  switch (goal.tag) {
    case IrTag::And:
    case IrTag::Or:
    case IrTag::IfThen:
    case IrTag::SoftIf:
      collect_goals(machine, goal.args[0], module, out);
      collect_goals(machine, goal.args[1], module, out);
      return;
    case IrTag::Catch:
      collect_goals(machine, goal.args[0], module, out);
      collect_goals(machine, goal.args[2], module, out);
      return;
    case IrTag::Atom: out.push_back(PredKey{module, goal.atom, 0}); return;
    case IrTag::Fun:
      if (machine.atoms.name(goal.atom) == ":" && goal.args.size() == 2 && goal.args[0].tag == IrTag::Atom) {
        collect_goals(machine, goal.args[1], goal.args[0].atom, out);
        return;
      }
      if (is_name(machine, goal, "\\+", 1) && (goal.args[0].tag == IrTag::Atom || goal.args[0].tag == IrTag::Fun)) {
        collect_goals(machine, goal_view(machine, goal.args[0]), module, out);
        return;
      }
      out.push_back(PredKey{module, goal.atom, static_cast<std::uint16_t>(goal.args.size())});
      return;
    default: return;
  }
}

// 組込、モジュールと取り込みの連鎖で引ける述語、宣言だけの手続き（iso-notes 7.4.2）を定義とみなす。
bool defined(Impl& machine, const PredKey& key) {
  if (is_builtin_procedure(machine, key.name, key.arity)) return true;
  if (resolve_pred(machine, key.module, key.name, key.arity) != nullptr) return true;
  return declared_only(machine, key.module, key.name, key.arity);
}

}  // namespace

std::vector<std::string> export_warnings(Impl& machine, const TranslationLog& log, const DisplayNames& names) {
  std::vector<std::string> lines;
  for (const Module& module : machine.modules) {
    const auto file = log_file(log, module.file);
    // 読めなかったファイルのモジュールは、定義が翻訳器に見えないので確かめない。
    if (!file || log.files[*file].library || log.files[*file].opaque) continue;
    for (const PredKey& exported : module.exports) {
      const PredKey key{module.name, exported.name, exported.arity};
      if (find_pred(machine, key) != nullptr || declared_only(machine, key.module, key.name, key.arity)) continue;
      const auto& place = log.files[*file];
      lines.push_back(names[*file] + ":" + std::to_string(place.line) + ":" + std::to_string(place.column) + ": " +
                      message_text(machine, "mosac_export_undefined", {indicator(machine, key)}));
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
    collect_goals(machine, predicate->clauses[site.clause].body, site.pred.module, goals);
    std::vector<std::string> seen;
    for (const PredKey& goal : goals) {
      if (defined(machine, goal)) continue;
      const std::string text = indicator(machine, goal);
      if (std::find(seen.begin(), seen.end(), text) != seen.end()) continue;
      seen.push_back(text);
      lines.push_back(names[site.file] + ":" + std::to_string(site.line) + ": " +
                      message_text(machine, "mosac_undefined_call", {text}));
    }
  }
  return lines;
}

}  // namespace mosa::codegen
