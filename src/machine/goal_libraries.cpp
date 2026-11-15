// フェーズ12 Task 5（iso-notes の問い 2 の A、SPEC 第5節と第6節）。mosac の翻訳の読み込みは指令だけを辿って同梱の
// 閉包を作るので、節の本体や initialization/1 のゴールにだけ書いた use_module(library(N)) のライブラリを実行ファイルが
// 見つけられなかった。読んだ項からその N を書き留め、翻訳の読み込みの最後に読んで閉包に入れる。
// テスト: tests/cpp/codegen_test.cpp（check_goal_library_names、check_goal_library_deep）、
//         tests/mosac/standalone_test.cmake（goal_use_body、goal_use_init）
#include "internal.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mosa {

namespace {

bool named(const Impl& machine, Cell term, std::string_view name, std::uint16_t arity) {
  return machine.heap.functor_arity(term) == arity && machine.atoms.name(machine.heap.functor_atom(term)) == name;
}

// term（複合項）が use_module(library(N)) か use_module(library(N), _) で、N が原子なら N。
std::optional<std::string_view> goal_library_name(const Impl& machine, Cell term) {
  if (!named(machine, term, "use_module", 1) && !named(machine, term, "use_module", 2)) return std::nullopt;
  const Cell spec = machine.heap.deref(machine.heap.arg(term, 0));
  if (spec.tag() != Tag::Str || !named(machine, spec, "library", 1)) return std::nullopt;
  const Cell name = machine.heap.deref(machine.heap.arg(spec, 0));
  if (name.tag() != Tag::Atom) return std::nullopt;
  return machine.atoms.name(static_cast<std::uint32_t>(name.raw() >> 3));
}

// use_module(library(N), []) を :-(G) にする。load_goal_term は検査済みの指令の形で受ける（run_load_goal と同じ）。
Cell library_directive(Impl& machine, std::string_view name) {
  const std::array<Cell, 1> spec_args{machine.heap.atom(machine.atoms.intern(name))};
  const std::array<Cell, 2> use_args{machine.heap.structure(machine.atoms.intern("library"), spec_args),
                                     machine.heap.atom(machine.atoms.intern("[]"))};
  const std::array<Cell, 1> body{machine.heap.structure(machine.atoms.intern("use_module"), use_args)};
  return machine.heap.structure(machine.atoms.intern(":-"), body);
}

}  // namespace

void note_goal_libraries(Impl& machine, Cell term) {
  TranslationLog* log = machine.translation;
  if (log == nullptr) return;
  // 1,000,000 段の項でも C++ のスタックを使い切らないよう、再帰ではなく明示のスタックで辿る。読んだ項は巡回しない。
  // 見つけた use_module の中も辿る（引数にまた use_module を書いた項も拾う）。
  std::vector<Cell> pending{term};
  while (!pending.empty()) {
    const Cell current = machine.heap.deref(pending.back());
    pending.pop_back();
    if (current.tag() == Tag::Lis) {
      pending.push_back(machine.heap.list_tail(current));
      pending.push_back(machine.heap.list_head(current));
      continue;
    }
    if (current.tag() != Tag::Str) continue;
    if (const auto name = goal_library_name(machine, current)) {
      std::vector<std::string>& names = log->goal_libraries;
      if (std::find(names.begin(), names.end(), *name) == names.end()) names.emplace_back(*name);
    }
    for (std::uint16_t index = 0; index < machine.heap.functor_arity(current); ++index) {
      pending.push_back(machine.heap.arg(current, index));
    }
  }
}

void load_goal_libraries(Impl& machine) {
  TranslationLog* log = machine.translation;
  if (log == nullptr) return;
  // 試みの失敗の診断はプログラムの読み込みの誤りではない。今の translate_program はこの後で diagnostic も
  // syntax_diag も読まないが、残すと後で読んだ所がプログラムの失敗と取り違える（タスク 3b の op/3 の誤りと同じ形）。
  // Machine::diagnostic() がプログラムの読み込みの物のままになるよう、読み終えたら戻す。
  const std::string diagnostic = machine.diagnostic;
  const bool syntax_diag = machine.syntax_diag;
  const std::uint32_t outer = machine.load_module;
  // 読むライブラリの節がまた名前を足すので、添字で読み、毎回 size() を見直す。伸びると要素が動くので、名前は写して持つ。
  for (std::size_t index = 0; index < log->goal_libraries.size(); ++index) {
    const std::string name = log->goal_libraries[index];
    Cell directive = library_directive(machine, name);
    // 項は読み込みの間だけ GC の根にする（run_load_goal と同じ）。
    const RootScope roots(machine, [&directive](GcVisitor& visitor) { visitor.cell(directive); });
    const std::size_t files = log->files.size();
    const std::size_t sites = log->sites.size();
    const std::size_t opens = log->library_opens.size();
    // 取り込みの無い use_module なので、user に述語は増えない。機械の巻き戻しは load_goal_term が済ませる。
    machine.load_module = machine.user_module;
    if (load_goal_term(machine, directive).ok) continue;
    // 翻訳の記録は load_goal_term が戻さない。残すと、読めなかったライブラリ（モジュールでない、構文の誤り）の原文を
    // library/N.pl の鍵で埋め込み、その鍵がプログラムのファイルの鍵とぶつかると翻訳を止める（mosac_duplicate_key）。
    // この試みが足した分を落とす。goal_libraries は今読んでいる列なので触らない（足された名前は害なく試すだけ）。
    log->files.resize(files);
    log->sites.resize(sites);
    log->library_opens.resize(opens);
  }
  machine.load_module = outer;
  machine.diagnostic = diagnostic;
  machine.syntax_diag = syntax_diag;
}

}
