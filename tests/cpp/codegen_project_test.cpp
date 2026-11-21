// 計画 Task 5 と Task 10 の目録（translate_program）、フェーズ12 と 13 の翻訳の読み込みの書き留め（TranslationLog）を
// 文面で確かめる。codegen_test.cpp が 800 行の上限に近づいたので分けた（レビュー 2026-11-20 の H-27 の前）。
// 終わりの nmake_paths だけは翻訳ではなく、build.hpp の nmake_safe_path（Windows の MSVC で NMake の道に渡せるパスか）を
// 見る（リリース 0.1.0 Task 10）。コンパイラを起こさずに、どの OS でも確かめられるので、ここに置く。

#include "harness.hpp"

#include "codegen/build.hpp"
#include "codegen/codegen.hpp"
#include "machine/internal.hpp"
#include "mosa/path_text.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

using mosa::Machine;
using mosa::codegen::EmitResult;
using mosa::codegen::emit_module;
using mosa_test::check;

bool has(const std::string& text, std::string_view part) { return text.find(part) != std::string::npos; }

constexpr std::string_view kApp = "app([], L, L).\napp([H|T], L, [H|R]) :- app(T, L, R).\n";

}  // namespace

// 計画 Task 5。読んだファイルのモジュールごとに単位を 1 つ出し、根の単位を先に入れる。
// 翻訳のときの読み込みは宣言だけを実行するので、:- assertz の指示は節を足さない（足せば seen/1 の呼び出しは
// 定義があることになり、下の未定義の警告が出なくなる）。
void write_file(const std::filesystem::path& path, std::string_view text) {
  std::ofstream out{path, std::ios::binary};
  out << text;
}

const mosa::codegen::ProjectFile* find_file(const mosa::codegen::Project& project, std::string_view name) {
  for (const auto& file : project.files) {
    if (file.name == name) return &file;
  }
  return nullptr;
}

void project_modules() {
  const auto dir = std::filesystem::path{MOSA_SOURCE_DIR} / "tests/tmp/codegen_project";
  std::filesystem::create_directories(dir);
  write_file(dir / "app.pl", ":- use_module(two).\n:- include(part).\nmain :- twice(1, X), part(X).\n");
  write_file(dir / "part.pl", "part(_).\n");
  write_file(dir / "two.pl", ":- module(two, [twice/2]).\ntwice(X, Y) :- Y is X * 2.\n");
  const auto project = mosa::codegen::translate_program(mosa::utf8_from_path(dir / "app.pl"), {});
  check(project.ok && project.warnings.empty(), "project ok: " + project.diagnostic + project.warnings);
  check(find_file(project, "muser.cpp") != nullptr && find_file(project, "mtwo.cpp") != nullptr, "unit per module");
  const auto* main_file = find_file(project, "main.cpp");
  check(main_file != nullptr && has(main_file->text, "part.pl"), "include embedded");
  const auto user_at = main_file == nullptr ? std::string::npos : main_file->text.find("muser::unit(");
  const auto two_at = main_file == nullptr ? std::string::npos : main_file->text.find("mtwo::unit(");
  check(user_at != std::string::npos && two_at != std::string::npos && user_at < two_at, "root unit first");
  write_file(dir / "skip.pl", ":- assertz(seen(1)).\nmain :- seen(1).\n");
  const auto skipped = mosa::codegen::translate_program(mosa::utf8_from_path(dir / "skip.pl"), {});
  // 未定義の呼び出しは翻訳を止めない警告になる（レビュー L12、持ち主の決定 A。前は ok が偽だった）。
  check(skipped.ok && has(skipped.warnings, ":2: ") && has(skipped.warnings, "seen/1"), "directive skipped");
  mosa::codegen::ProjectOptions units;
  units.check_calls = false;
  const auto unchecked = mosa::codegen::translate_program(mosa::utf8_from_path(dir / "skip.pl"), units);
  check(unchecked.ok && unchecked.warnings.empty(), "check_calls off");
  // 止めた理由の export の誤りを先に、呼び出しの警告を後ろに書く（L12 の敵対的レビュー A8）。
  write_file(dir / "bad_export.pl", ":- module(bad_export, [p/0, q/0]).\np :- nosuch.\n");
  const auto refused = mosa::codegen::translate_program(mosa::utf8_from_path(dir / "bad_export.pl"), {});
  const auto export_at = refused.diagnostic.find("q/0");
  const auto call_at = refused.diagnostic.find("nosuch/0");
  check(!refused.ok && refused.warnings.empty() && export_at != std::string::npos && call_at != std::string::npos &&
            export_at < call_at,
        "export error before call warning: " + refused.diagnostic);
}

// 翻訳のときに飛ばした指示のゴールが op/3 を定義すると、後の項は翻訳器に読めない。誤りにせず、
// そのファイルの述語は生成関数を出さずに IR のまま動かす（計画 Task 5）。飛ばしたゴールが無ければ誤りのまま。
void project_opaque() {
  const auto dir = std::filesystem::path{MOSA_SOURCE_DIR} / "tests/tmp/codegen_project";
  std::filesystem::create_directories(dir);
  write_file(dir / "newop.pl", "fast(1).\n:- true, op(700, xfx, ===>).\nr(a ===> b).\n");
  const auto project = mosa::codegen::translate_program(mosa::utf8_from_path(dir / "newop.pl"), {});
  check(project.ok, "opaque ok: " + project.diagnostic);
  const auto* unit = find_file(project, "muser.cpp");
  check(unit != nullptr && !has(unit->text, "pfast_1") && !has(unit->text, "NativeEntry{"), "opaque has no entries");
  write_file(dir / "plain_broken.pl", "r(a ===> b).\n:- true, op(700, xfx, ===>).\n");
  const auto broken = mosa::codegen::translate_program(mosa::utf8_from_path(dir / "plain_broken.pl"), {});
  check(!broken.ok && has(broken.diagnostic, "plain_broken.pl:1:"), "syntax error before skip stays an error");
  // 読めなかった項の定義は翻訳器に見えない。それを呼ぶ節に未定義の警告を出さない（L12 の敵対的レビュー A1）。
  write_file(dir / "opaque_call.pl", "p :- r(_).\n:- true, op(700, xfx, ===>).\nr(a ===> b).\n");
  const auto quiet = mosa::codegen::translate_program(mosa::utf8_from_path(dir / "opaque_call.pl"), {});
  check(quiet.ok && quiet.warnings.empty(), "no call warning past an opaque term: " + quiet.warnings);
  auto machine = Machine::create();
  check(machine->load_program(kApp).ok, "load app");
  const auto& impl = mosa::engine_impl(*machine);
  const std::vector<mosa::PredKey> only{mosa::PredKey{impl.user_module, machine->atoms().intern("app"), 3}};
  const EmitResult kept = emit_module(*machine, impl.user_module, "t", only);
  check(kept.translated == 0 && kept.interpreted == 2 && !has(kept.source, "papp_3"), "interpreted_only counts");
  write_file(dir / "all.pl", "a.\n");
  const auto reserved = mosa::codegen::translate_program(mosa::utf8_from_path(dir / "all.pl"), {});
  const auto* cmake = find_file(reserved, "CMakeLists.txt");
  check(cmake != nullptr && has(cmake->text, "project(app "), "reserved target name");
}

// フェーズ13 Task 4（問い 1 の A、ADR 0027）。name のファイルに text を書いて訳し、muser.cpp の本文を返す。
// 翻訳が止まれば診断を添えて落とし、空を返す。診断が残っていても落とす（条件の例外と halt が残した物）。
std::string cond_unit(std::string_view name, std::string_view text) {
  const auto dir = std::filesystem::path{MOSA_SOURCE_DIR} / "tests/tmp/codegen_cond";
  std::filesystem::create_directories(dir);
  write_file(dir / name, text);
  const auto project = mosa::codegen::translate_program(mosa::utf8_from_path(dir / name), {});
  check(project.ok && project.diagnostic.empty(), std::string{name} + " translates: " + project.diagnostic);
  const auto* unit = find_file(project, "muser.cpp");
  check(unit != nullptr, std::string{name} + " has muser.cpp");
  return unit == nullptr ? std::string{} : unit->text;
}

// mosac は条件をビルド時にも解き、取った分岐の節だけを訳す。条件の例外と halt は「取らない」とだけ扱って翻訳を
// 止めない（Review Focus 4）。取らなかった分岐の無いライブラリ（Review Focus 5）と構文の誤りでも止まらない。
// 取らなかった分岐の指令は飛ばしたゴールと同じ印になり、その op/3 を使う後ろの項はファイルを IR のままにする。
void check_cond_translation() {
  const std::string emit = cond_unit("cond_emit.pl", ":- if(true).\nthenpred.\n:- else.\nelsepred.\n:- endif.\n");
  check(has(emit, "pthenpred_0") && !has(emit, "pelsepred_0"), "only the branch taken is translated");
  const std::string thrown = cond_unit("cond_throw.pl", ":- if(nope).\nskippedpred.\n:- endif.\nlaterpred.\n");
  check(has(thrown, "platerpred_0") && !has(thrown, "pskippedpred_0"), "a condition that throws is not taken");
  const std::string halted = cond_unit("cond_halt.pl", ":- if(halt).\nhaltedpred.\n:- endif.\nafterhalt.\n");
  check(has(halted, "pafterhalt_0"), "a condition that halts does not stop the translation");
  const std::string dialect = cond_unit(
      "cond_dialect.pl",
      ":- if(current_prolog_flag(dialect, swi)).\n:- use_module(library(clpfd)).\n:- else.\nmosapred.\n:- endif.\n");
  check(has(dialect, "pmosapred_0"), "a missing library in a branch not taken does not stop the translation");
  const std::string syntax = cond_unit("cond_syntax.pl", ":- if(fail).\nx(1r3).\n:- endif.\nysyntax.\n");
  check(has(syntax, "pysyntax_0"), "a syntax error in a branch not taken is passed over");
  // 前に一般の指令が無いので、取らなかった分岐の指令が skipped_goal を立てなければ翻訳が止まる。
  const std::string op_only =
      cond_unit("cond_op_only.pl", ":- if(fail).\n:- op(700, xfx, ===>).\n:- endif.\nr(a ===> b).\n");
  check(!op_only.empty() && !has(op_only, "NativeEntry{"), "an op/3 in a branch not taken leaves the file as IR");
}

// name のファイルに text を書いて訳し、翻訳が止まって診断に part が入ることを見る。診断を返す。
std::string cond_stop(std::string_view name, std::string_view text, std::string_view part) {
  const auto dir = std::filesystem::path{MOSA_SOURCE_DIR} / "tests/tmp/codegen_cond";
  std::filesystem::create_directories(dir);
  write_file(dir / name, text);
  const auto project = mosa::codegen::translate_program(mosa::utf8_from_path(dir / name), {});
  check(!project.ok && has(project.diagnostic, part),
        std::string{name} + " stops the translation: " + project.diagnostic);
  return project.diagnostic;
}

// I2（持ち主の選択 A、ADR 0027）。ビルド時に取った分岐の中で指令が失敗しても、その条件より前に飛ばしたゴールが
// あれば翻訳を止めない。起動時はそのゴールが条件を変え、分岐を取らないかもしれない。ファイルは IR のまま動く。
// 前のファイルで飛ばしたゴールも数え、分岐の中で読んで途中で失敗したモジュールの公開の未定義でも止めない。
void check_cond_build_failure() {
  const std::string skip = ":- dynamic(skip/0).\n:- assertz(skip).\n";
  const std::string branch = skip + ":- if(\\+ skip).\n";
  const std::string plain =
      cond_unit("cond_fail_plain.pl", branch + ":- use_module(no_such_helper).\n:- endif.\nq.\n");
  check(!plain.empty() && !has(plain, "NativeEntry{"),
        "a failing use_module after a skipped goal leaves the file as IR");
  const std::string lib =
      cond_unit("cond_fail_lib.pl", branch + ":- use_module(library(no_such_lib)).\n:- endif.\nq.\n");
  check(!lib.empty() && !has(lib, "NativeEntry{"), "a missing library after a skipped goal leaves the file as IR");
  const auto dir = std::filesystem::path{MOSA_SOURCE_DIR} / "tests/tmp/codegen_cond";
  write_file(dir / "cond_fail_part.pl", ":- if(\\+ skip).\n:- use_module(no_such_helper).\n:- endif.\n");
  (void)cond_unit("cond_fail_include.pl", skip + ":- include(cond_fail_part).\n");
  write_file(dir / "cond_fail_helper.pl", ":- module(cond_fail_helper, [p/0]).\n:- use_module(no_such_helper).\np.\n");
  (void)cond_unit("cond_fail_module.pl", branch + ":- use_module(cond_fail_helper).\n:- endif.\n");
  // 起動時の結果が変わりえなければ、今までどおり止める。分岐の外、飛ばしたゴールの無い分岐、条件を解いた後で
  // 飛ばしたゴールしか無い分岐（そのゴールはもう解いた条件を変えない）。
  // 診断の頭は根のファイルのパスなので、それを外して比べる。
  const auto reason = [&dir](const std::string& diagnostic, std::string_view name) {
    const std::string head = mosa::utf8_from_path(dir / name) + ": ";
    return diagnostic.starts_with(head) ? diagnostic.substr(head.size()) : diagnostic;
  };
  const std::string outside =
      cond_stop("cond_fail_outside.pl", skip + ":- use_module(no_such_helper).\n", "no_such_helper");
  const std::string unskipped = cond_stop("cond_fail_unskipped.pl",
                                          ":- if(true).\n:- use_module(no_such_helper).\n:- endif.\n", "no_such_helper");
  check(reason(outside, "cond_fail_outside.pl") == reason(unskipped, "cond_fail_unskipped.pl"),
        "the same reason outside and inside a branch: " + outside + " / " + unskipped);
  (void)cond_stop("cond_fail_unskipped_lib.pl", ":- if(true).\n:- use_module(library(no_such_lib)).\n:- endif.\n",
                  "existence_error(source_sink,library(no_such_lib))");
  (void)cond_stop("cond_fail_late.pl", ":- if(true).\n:- assertz(x).\n:- use_module(no_such_helper).\n:- endif.\n",
                  "no_such_helper");
  // :- fail. は翻訳の読み込みでは呼ばない一般の指令で、分岐の中でも失敗しない。ファイルは今までどおり訳す。
  const std::string failed = cond_unit("cond_fail_goal.pl", ":- if(true).\n:- fail.\n:- endif.\nfailpin.\n");
  check(has(failed, "pfailpin_0"), "a :- fail. in a branch is skipped at build time");
}

// I2 の直し 1 回目。同じ if の前の分岐で読み飛ばした指令は、後ろの elif の条件を変えない（起動時にその分岐を取れば、
// elif は読まない）ので数えない。空の elif を挟んでも else と同じく止める。分岐の中で途中まで読んで失敗したファイルは、
// 後ろの分岐の外の use_module がもう一度読み、そこで止める。
void check_cond_failure_chain() {
  const std::string tail = ":- use_module(no_such_helper).\n:- endif.\n";
  (void)cond_stop("cond_fail_elif.pl", ":- if(fail).\n:- foo.\n:- elif(true).\n" + tail, "no_such_helper");
  (void)cond_stop("cond_fail_else.pl", ":- if(fail).\n:- foo.\n:- else.\n" + tail, "no_such_helper");
  (void)cond_stop("cond_fail_elif_else.pl", ":- if(fail).\n:- foo.\n:- elif(fail).\n:- else.\n" + tail,
                  "no_such_helper");
  const auto dir = std::filesystem::path{MOSA_SOURCE_DIR} / "tests/tmp/codegen_cond";
  write_file(dir / "cond_fail_again_m.pl",
             ":- module(cond_fail_again_m, [p/0]).\n:- use_module(no_such_helper).\np.\n");
  (void)cond_stop("cond_fail_again.pl",
                  ":- dynamic(skip/0).\n:- assertz(skip).\n:- if(\\+ skip).\n:- use_module(cond_fail_again_m).\n"
                  ":- endif.\n:- use_module(cond_fail_again_m).\n",
                  "no_such_helper");
}

// I2 の直し 2 回目。任せた指令が途中まで読んで失敗したファイルだけが準備済みの印を失う。その中で読み終えたモジュール
// （節が 2 つずつの述語を持つ cond_fail_q と library(lists)）は印を残し、分岐の外の use_module が同じ読み込みで
// 読み直して節が離れていると誤らない。同じファイルを 2 つの分岐で任せた後でも、分岐の外の use_module は読み直して止める。
void check_cond_failure_prepared() {
  const auto dir = std::filesystem::path{MOSA_SOURCE_DIR} / "tests/tmp/codegen_cond";
  const std::string skip = ":- dynamic(skip/0).\n:- assertz(skip).\n";
  write_file(dir / "cond_fail_q.pl", ":- module(cond_fail_q, [q/1, r/1]).\nq(1).\nq(2).\nr(a).\nr(b).\n");
  write_file(dir / "cond_fail_qm.pl",
             ":- module(cond_fail_qm, [p/0]).\n:- use_module(cond_fail_q).\n:- use_module(no_such_helper).\np.\n");
  (void)cond_unit("cond_fail_q_main.pl", skip + ":- if(\\+ skip).\n:- use_module(cond_fail_qm).\n:- endif.\n"
                                                ":- use_module(cond_fail_q).\nmain :- q(_), r(_).\n");
  write_file(dir / "cond_fail_lm.pl", ":- module(cond_fail_lm, [p/0]).\n:- use_module(library(lists)).\n"
                                      ":- use_module(no_such_helper).\np.\n");
  (void)cond_unit("cond_fail_l_main.pl", skip + ":- if(\\+ skip).\n:- use_module(cond_fail_lm).\n:- endif.\n"
                                                ":- use_module(library(lists)).\nmain :- append([a], [b], _).\n");
  write_file(dir / "cond_fail_twice_m.pl",
             ":- module(cond_fail_twice_m, [p/0]).\n:- use_module(no_such_helper).\np.\n");
  const std::string block = ":- if(\\+ skip).\n:- use_module(cond_fail_twice_m).\n:- endif.\n";
  (void)cond_stop("cond_fail_twice.pl", skip + block + block + ":- use_module(cond_fail_twice_m).\n", "no_such_helper");
}

// I2 の直し 3 回目。条件より前に飛ばしたゴールがあれば、分岐の中の節の誤りと構文の誤りも指令と同じく任せる。前は
// 今のファイルの印（mark_opaque）だけを見たので、include したファイルの分岐と、文法規則の誤りでは止まった。
// 分岐の外と、飛ばしたゴールの無い分岐では今までどおり止める。
void check_cond_failure_clause() {
  const auto dir = std::filesystem::path{MOSA_SOURCE_DIR} / "tests/tmp/codegen_cond";
  std::filesystem::create_directories(dir);
  const std::string skip = ":- dynamic(skip/0).\n:- assertz(skip).\n";
  const std::string branch = ":- if(\\+ skip).\n";
  // include した部品は読んだ側のファイルの一部なので、部品の印は親にも付き、親の q/0 も IR のまま動く。
  const auto included = [&](std::string_view name, const std::string& head, const std::string& body) {
    const std::string part = std::string{name} + "_part";
    write_file(dir / (part + ".pl"), branch + body + ":- endif.\n");
    const std::string unit = cond_unit(std::string{name} + ".pl", skip + head + ":- include(" + part + ").\nq.\n");
    check(!unit.empty() && !has(unit, "NativeEntry{"), std::string{name} + " leaves the including file as IR");
    return unit;
  };
  (void)included("cond_clause_refused", "", "atom_length(a, 1).\n");
  (void)included("cond_clause_syntax", "", "a ===> b.\n");
  (void)included("cond_clause_dcg", "", "a --> 1.\n");
  // 節が離れて足せなかった後も続きの節を読み、分岐を閉じる。その節のある r/1 は IR のまま動く。
  const std::string apart = included("cond_clause_apart", "p(1).\nq0.\n", "p(2).\nr(1).\nr(2).\n");
  check(!apart.empty() && !has(apart, "pr_1"), "the clauses after a clause apart run as IR");
  const std::string same = cond_unit("cond_clause_dcg_same.pl", skip + branch + "a --> 1.\n:- endif.\nq.\n");
  check(!same.empty() && !has(same, "NativeEntry{"), "a bad grammar rule after a skipped goal leaves the file as IR");
  write_file(dir / "cond_clause_out_part.pl", "atom_length(a, 1).\n");
  (void)cond_stop("cond_clause_out.pl", skip + ":- include(cond_clause_out_part).\n", "atom_length/2");
  (void)cond_stop("cond_clause_sure.pl", ":- if(true).\natom_length(a, 1).\n:- endif.\n", "atom_length/2");
  (void)cond_stop("cond_clause_sure_dcg.pl", ":- if(true).\na --> 1.\n:- endif.\n", "type_error(callable,1)");
}

// 直し 3 回目の Opus レビュー A1 と、その続きのレビューの A1・A2。任せた失敗の項が定義するはずだった公開述語では
// 止めない。モジュールのファイルが include か ensure_loaded で読んだ部品の印はモジュールのファイルへ上げ、reexport
// の公開は定義したモジュールのファイルの印でも確かめる（2 つ目の reexport は読み直さないので、印が上がらない）。
// 自分のモジュールを持つファイルの印は読んだ側へ上げず、読んだ側の公開の誤りは今までどおり止める。
void check_cond_failure_export() {
  const auto dir = std::filesystem::path{MOSA_SOURCE_DIR} / "tests/tmp/codegen_cond";
  std::filesystem::create_directories(dir);
  const std::string skip = ":- dynamic(skip/0).\n:- assertz(skip).\n";
  const std::string branch = ":- if(\\+ user:skip).\n";
  const auto exported = [&](std::string_view name, std::string_view load, const std::string& body) {
    const std::string module = std::string{name} + "_m";
    write_file(dir / (module + "_part.pl"), branch + body + ":- endif.\n");
    write_file(dir / (module + ".pl"), ":- module(" + module + ", [p/0]).\n:- " + std::string{load} + "(" + module +
                                           "_part).\n");
    (void)cond_unit(std::string{name} + ".pl", skip + ":- use_module(" + module + ").\n");
  };
  exported("cond_clause_export", "include", "p :- 1.\n");
  exported("cond_clause_export_dir", "include", ":- include(no_such_part).\n");
  exported("cond_clause_export_ensure", "ensure_loaded", "p :- 1.\n");
  write_file(dir / "cond_clause_rc.pl", ":- module(cond_clause_rc, [p/0]).\n" + branch + "p :- 1.\n:- endif.\n");
  write_file(dir / "cond_clause_ra.pl", ":- module(cond_clause_ra, []).\n:- reexport(cond_clause_rc).\n");
  write_file(dir / "cond_clause_rb.pl", ":- module(cond_clause_rb, []).\n:- reexport(cond_clause_rc).\n");
  (void)cond_unit("cond_clause_reexport.pl", skip + ":- use_module(cond_clause_ra).\n:- use_module(cond_clause_rb).\n");
  write_file(dir / "cond_clause_typo_h.pl",
             ":- module(cond_clause_typo_h, []).\n" + branch + ":- include(no_such_part).\n:- endif.\n");
  write_file(dir / "cond_clause_typo_r.pl",
             ":- module(cond_clause_typo_r, [typo/0]).\n:- use_module(cond_clause_typo_h).\n");
  (void)cond_stop("cond_clause_typo.pl", skip + ":- use_module(cond_clause_typo_r).\n", "typo/0");
  // 取らなかった分岐の印も同じ。2 つ目に reexport した側も、誤りでなく警告にする（Task 4 から、同じレビュー）。
  write_file(dir / "cond_untaken_rc.pl", ":- module(cond_untaken_rc, [p/0]).\n:- if(fail).\np.\n:- endif.\n");
  write_file(dir / "cond_untaken_ra.pl", ":- module(cond_untaken_ra, []).\n:- reexport(cond_untaken_rc).\n");
  write_file(dir / "cond_untaken_rb.pl", ":- module(cond_untaken_rb, []).\n:- reexport(cond_untaken_rc).\n");
  write_file(dir / "cond_untaken_reexport.pl", ":- use_module(cond_untaken_ra).\n:- use_module(cond_untaken_rb).\n");
  const auto untaken = mosa::codegen::translate_program(mosa::utf8_from_path(dir / "cond_untaken_reexport.pl"), {});
  check(untaken.ok && untaken.diagnostic.empty() && has(untaken.warnings, "cond_untaken_rb.pl:1:1: ") &&
            has(untaken.warnings, "p/0"),
        "a second reexport of a module with a branch not taken warns: " + untaken.warnings);
  // 自分のモジュールを持つファイルが、限定した頭（cond_qual_r:p）でほかのモジュールの公開述語を定義する節を任せた
  // ときは、そのモジュールのファイルにも印を付ける（直し 4 回目のレビュー A3。d86485b で印を上げなくなった退行）。
  write_file(dir / "cond_qual_m.pl", ":- module(cond_qual_m, []).\n" + branch + "cond_qual_r:p :- 1.\n:- endif.\n");
  write_file(dir / "cond_qual_r.pl", ":- module(cond_qual_r, [p/0]).\n:- use_module(cond_qual_m).\n");
  (void)cond_unit("cond_qual.pl", skip + ":- use_module(cond_qual_r).\n");
}

// I2 の直し 4 回目（持ち主の選択 A）。起動時には別の分岐を取りうるブロックを閉じた後は、その分岐が残した物で節を足せ
// なくても（節が離れている、取り込んだ述語との衝突）、起動時の読み込みに任せる。include の先でも元でも、取らなかった
// 分岐の discontiguous/1 でも同じ。ブロックより前の誤りと、分岐に関わらない誤り（文法規則）は今までどおり止める。
void check_cond_failure_after() {
  const auto dir = std::filesystem::path{MOSA_SOURCE_DIR} / "tests/tmp/codegen_cond";
  std::filesystem::create_directories(dir);
  const std::string skip = ":- dynamic(skip/0).\n:- assertz(skip).\n";
  const std::string block = ":- if(\\+ skip).\np(1).\n:- endif.\n";
  const auto builds = [&](std::string_view name, const std::string& text) {
    const std::string unit = cond_unit(name, text);
    check(!unit.empty() && !has(unit, "NativeEntry{"), std::string{name} + " leaves the file as IR");
  };
  builds("cond_after_apart.pl", skip + "q(1).\n" + block + "q(2).\nq(3).\nr.\n");
  write_file(dir / "cond_after_down_part.pl", "q(2).\nq(3).\n");
  builds("cond_after_down.pl", skip + "q(1).\n" + block + ":- include(cond_after_down_part).\nr.\n");
  write_file(dir / "cond_after_up_part.pl", block);
  // 部品の p/1 は部品の印のまま訳してよい。足せなかった q/1 のある読んだ側だけが IR になる。
  const std::string up = cond_unit("cond_after_up.pl", skip + "q(1).\n:- include(cond_after_up_part).\nq(2).\n");
  check(!up.empty() && !has(up, "pq_1"), "a clause apart after an included block leaves its predicate as IR");
  write_file(dir / "cond_after_h.pl", ":- module(cond_after_h, [p/0]).\np.\n");
  builds("cond_after_clash.pl", skip + ":- if(\\+ skip).\n:- use_module(cond_after_h).\n:- endif.\np.\n");
  builds("cond_after_discont.pl", skip + ":- if(skip).\n:- discontiguous(q/1).\n:- endif.\nq(1).\np.\nq(2).\n");
  (void)cond_stop("cond_after_before.pl", skip + "q(1).\np(1).\nq(2).\n" + block, "q/1");
  (void)cond_stop("cond_after_dcg.pl", skip + block + "a --> 1.\n", "type_error(callable,1)");
  (void)cond_stop("cond_after_sure.pl", "q(1).\n:- if(true).\np(1).\n:- endif.\nq(2).\n", "q/1");
}

// 読み飛ばした項のライブラリも書き留める。reexport の 2 つの形も拾う（計画の「設計との違い」3）。積んだ条件の節の数は
// 読み終えれば 0 に戻る。
void check_cond_log() {
  auto machine = Machine::create();
  auto& impl = mosa::engine_impl(*machine);
  mosa::TranslationLog log;
  impl.translation = &log;
  const auto loaded = machine->load_program(
      ":- if(fail).\n:- use_module(library(strings)).\n:- reexport(library(between)).\n:- endif.\n");
  impl.translation = nullptr;
  check(loaded.ok, "cond log load: " + loaded.message);
  check(log.goal_libraries == std::vector<std::string>{"strings", "between"}, "libraries named in skipped terms");
  check(log.open_conditions == 0, "no condition stays open after the load");
  // 条件のゴールに字面で書いたライブラリも、解いても（if）解かなくても（取った分岐の後ろの elif）書き留める。
  mosa::TranslationLog goals;
  impl.translation = &goals;
  const auto conditions = machine->load_program(
      ":- if((fail, use_module(library(apply)))).\n:- elif(true).\n:- elif(use_module(library(dcg))).\n:- endif.\n");
  impl.translation = nullptr;
  check(conditions.ok, "cond goal log load: " + conditions.message);
  check(goals.goal_libraries == std::vector<std::string>{"apply", "dcg"}, "libraries named in condition goals");
  check(goals.open_conditions == 0, "no condition stays open after the second load");
}

// フェーズ12 Task 5（問い 2 の A）。翻訳の読み込みは、節の本体（m: の下や catch の中も）と initialization/1 の
// ゴールに字面で書いた use_module(library(N)) の N を書き留める。N が変数の物は書き留めない。
void check_goal_library_names() {
  auto machine = Machine::create();
  auto& impl = mosa::engine_impl(*machine);
  mosa::TranslationLog log;
  impl.translation = &log;
  const auto loaded = machine->load_program(
      "p :- catch(m:use_module(library(apply)), _, true).\n"
      "q(N) :- use_module(library(N)).\n"
      ":- initialization(use_module(library(between), [between/3])).\n");
  impl.translation = nullptr;
  check(loaded.ok, "goal library names load: " + loaded.message);
  check(log.goal_libraries == std::vector<std::string>{"apply", "between"}, "goal library names");
}

// 1,000,000 段の f(…) の奥の use_module(library(dcg)) も、C++ のスタックを使い切らずに書き留める。
void check_goal_library_deep() {
  auto machine = Machine::create();
  auto& impl = mosa::engine_impl(*machine);
  mosa::TranslationLog log;
  impl.translation = &log;
  const std::array<mosa::Cell, 1> name{impl.heap.atom(impl.atoms.intern("dcg"))};
  const std::array<mosa::Cell, 1> spec{impl.heap.structure(impl.atoms.intern("library"), name)};
  mosa::Cell term = impl.heap.structure(impl.atoms.intern("use_module"), spec);
  const std::uint32_t wrapper = impl.atoms.intern("f");
  for (int depth = 0; depth < 1'000'000; ++depth) {
    const std::array<mosa::Cell, 1> inner{term};
    term = impl.heap.structure(wrapper, inner);
  }
  mosa::note_goal_libraries(impl, term);
  impl.translation = nullptr;
  check(log.goal_libraries == std::vector<std::string>{"dcg"}, "goal library in a deep term");
}

// 読めないライブラリ（モジュールでない原文だけの同梱ライブラリと、どこにも無いライブラリ）は翻訳を止めない。診断は
// 前の値に戻り、翻訳の記録にもその原文と節を残さない（残すと原文を埋め込み、その鍵がプログラムのファイルの鍵と
// ぶつかると翻訳を止める）。後ろの apply は読めて記録に入る。前の値は目印にする（apply を読むと診断は空になるので）。
void check_goal_library_failure() {
  mosa::rt::bundle_library("goal_library_plain", "goal_library_plain(1).\n");
  auto machine = Machine::create();
  auto& impl = mosa::engine_impl(*machine);
  mosa::TranslationLog log;
  impl.translation = &log;
  log.goal_libraries = {"goal_library_plain", "goal_library_nope", "apply"};
  impl.diagnostic = "kept";
  impl.syntax_diag = true;
  mosa::load_goal_libraries(impl);
  impl.translation = nullptr;
  check(impl.diagnostic == "kept" && impl.syntax_diag, "goal libraries keep the diagnostic: " + impl.diagnostic);
  const auto logged = [&log](std::string_view part) {
    return std::any_of(log.files.begin(), log.files.end(), [part](const auto& file) { return has(file.key, part); });
  };
  const std::uint32_t plain = impl.atoms.intern("goal_library_plain");
  const bool plain_site =
      std::any_of(log.sites.begin(), log.sites.end(), [plain](const auto& site) { return site.pred.name == plain; });
  check(!logged("goal_library_plain") && !plain_site, "a failed goal library leaves nothing in the log");
  check(logged("apply"), "goal libraries go on past a failure");
}

// Windows の NMake は cl へのインラインの応答ファイルを ANSI の符号ページで書き、ASCII の外の文字を落とす
// （ci run 42438279645 の「展開 先」）。どの OS でも、UTF-8 で 0x80 以上のバイトが 1 つでもあれば渡せないと答える。
void nmake_paths() {
  using mosa::codegen::nmake_safe_path;
  check(nmake_safe_path("C:/Program Files (x86)/Microsoft Visual Studio/2022"), "ASCII with spaces and (x86)");
  check(nmake_safe_path(""), "an empty path is NMake-safe");
  check(!nmake_safe_path("D:/a/tests/tmp/package/展開 先/unpacked"), "a Japanese folder is not NMake-safe");
  check(!nmake_safe_path("C:/Users/José/work"), "a Latin-1 letter is not NMake-safe");
  check(!nmake_safe_path("C:/work/\xf0\x9f\x98\x80/x"), "an emoji (U+1F600) is not NMake-safe");
}

int main() {
  project_modules();
  project_opaque();
  check_cond_translation();
  check_cond_build_failure();
  check_cond_failure_chain();
  check_cond_failure_prepared();
  check_cond_failure_clause();
  check_cond_failure_export();
  check_cond_failure_after();
  check_cond_log();
  check_goal_library_names();
  check_goal_library_deep();
  check_goal_library_failure();
  nmake_paths();
  return mosa_test::failures() == 0 ? 0 : 1;
}
