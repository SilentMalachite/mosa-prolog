// module/2、export/1、use_module/1,2 の読み込み（SPEC 第5節、計画の Task 4 と 5）。読み込みの失敗は例外ではなく診断なので C++ で見る。
#include "harness.hpp"
#include "mosa/machine.hpp"
#include "mosa/path_text.hpp"

#include <filesystem>
#include <fstream>
#include <string>

namespace {

bool succeeds(mosa::Machine& machine, std::string_view goal) {
  return machine.query(goal).kind == mosa::StopKind::Success;
}

bool mentions(const mosa::Machine& machine, std::string_view text) {
  return std::string{machine.diagnostic()}.find(text) != std::string::npos;
}

const std::string kModules = std::string{MOSA_SOURCE_DIR} + "/tests/pl/modules/";

// 一時ファイルは tests/tmp/ にだけ作る。
std::string temp_file(std::string_view name, std::string_view text) {
  const std::filesystem::path dir = std::filesystem::path{MOSA_SOURCE_DIR} / "tests" / "tmp" / "module_load";
  std::filesystem::create_directories(dir);
  // 名前は UTF-8。Windows で std::string から作ると ANSI コードページで化ける。
  const auto path = dir / mosa::path_from_utf8(name);
  std::ofstream{path, std::ios::binary} << text;
  return mosa::utf8_from_path(path);
}

std::string use_text(std::string_view module, std::string_view before, const std::string& file, std::string_view after) {
  return ":- module(" + std::string{module} + ", []).\n" + std::string{before} + ":- use_module('" + file + "'" +
         std::string{after} + ").\n";
}

void check_module_text() {
  auto machine = mosa::Machine::create();
  const auto status = machine->load_program(":- module(m, [p/1]). p(1). q(2).");
  mosa_test::check(status.ok, "module text loads");
  mosa_test::check(succeeds(*machine, "p(1)."), "export imported into user");
  mosa_test::check(succeeds(*machine, "m:q(2)."), "private reachable with m:");
  mosa_test::check(machine->query("q(2).").kind == mosa::StopKind::Exception, "private hidden from user");
}

void check_not_first() {
  auto machine = mosa::Machine::create();
  mosa_test::check(!machine->load_program("p(1).\n:- module(m, []).\n").ok, "module/2 after a clause fails");
  mosa_test::check(mentions(*machine, "permission_error(create,module,m)"), "permission_error(create, module, m)");
  mosa_test::check(machine->query("m:true.").kind == mosa::StopKind::Exception, "rejected module is not created");
}

void check_bad_module_name() {
  auto machine = mosa::Machine::create();
  mosa_test::check(!machine->load_program(":- module(1, []).").ok, "module name 1 fails");
  mosa_test::check(mentions(*machine, "type_error(atom,1)"), "type_error(atom, 1)");
  auto unbound = mosa::Machine::create();
  mosa_test::check(!unbound->load_program(":- module(_, []).").ok, "unbound module name fails");
  mosa_test::check(mentions(*unbound, "instantiation_error"), "instantiation_error");
}

void check_bad_export() {
  auto machine = mosa::Machine::create();
  mosa_test::check(!machine->load_program(":- module(m, [foo]).").ok, "export foo fails");
  mosa_test::check(mentions(*machine, "type_error(predicate_indicator,foo)"), "type_error(predicate_indicator, foo)");
}

void check_rollback() {
  auto machine = mosa::Machine::create();
  mosa_test::check(!machine->load_program(":- module(m, [p/1]). p(1). p(").ok, "syntax error fails");
  mosa_test::check(machine->query("m:true.").kind == mosa::StopKind::Exception, "failed load leaves no module");
  mosa_test::check(machine->load_program("r(1).").ok, "later text goes to user");
  mosa_test::check(succeeds(*machine, "user:r(1)."), "context back to user");
}

void check_use_conflict() {
  auto local_first = mosa::Machine::create();
  mosa_test::check(!local_first->load_program(use_text("衝突", "読み(a, b).\n", kModules + "人名.pl", "")).ok,
                   "local then import fails");
  mosa_test::check(mentions(*local_first, "permission_error(import_into,procedure,人名:読み/2)"),
                   "local then import: permission_error(import_into, procedure, 人名:読み/2)");
  auto import_first = mosa::Machine::create();
  const auto text = use_text("衝突後", "", kModules + "人名.pl", "") + "読み(a, b).\n";
  mosa_test::check(!import_first->load_program(text).ok, "import then local fails");
  mosa_test::check(mentions(*import_first, "permission_error(import_into,procedure,人名:読み/2)"),
                   "import then local: permission_error(import_into, procedure, 人名:読み/2)");
}

void check_two_sources() {
  const auto first = temp_file("甲.pl", ":- module(甲, [p/1]).\np(1).\n");
  const auto second = temp_file("乙.pl", ":- module(乙, [p/1]).\np(2).\n");
  auto machine = mosa::Machine::create();
  const auto text = use_text("両方", "", first, "") + ":- use_module('" + second + "').\n";
  mosa_test::check(!machine->load_program(text).ok, "same name from two modules fails");
  mosa_test::check(mentions(*machine, "permission_error(import_into,procedure,乙:p/1)"),
                   "permission_error(import_into, procedure, 乙:p/1)");
  auto again = mosa::Machine::create();
  const auto twice = use_text("二度", "", first, "") + ":- use_module('" + first + "').\n";
  mosa_test::check(again->load_program(twice).ok, "same module imported twice is fine");
}

void check_not_exported() {
  auto machine = mosa::Machine::create();
  mosa_test::check(!machine->load_program(use_text("未公開", "", kModules + "人名.pl", ", [内緒/1]")).ok,
                   "importing a private predicate fails");
  mosa_test::check(mentions(*machine, "existence_error(procedure,人名:内緒/1)"), "existence_error(procedure, 人名:内緒/1)");
  auto except = mosa::Machine::create();
  mosa_test::check(except->load_program(use_text("除く", "", kModules + "人名.pl", ", except([内緒/1])")).ok,
                   "except of a private predicate is ignored");
}

void check_not_module_file() {
  const auto plain = temp_file("ただの.pl", "q(1).\n");
  auto machine = mosa::Machine::create();
  mosa_test::check(!machine->load_program(":- use_module('" + plain + "').\n").ok, "non-module file fails");
  mosa_test::check(mentions(*machine, "permission_error(load,source_sink,"), "permission_error(load, source_sink, F)");
  mosa_test::check(machine->query("q(1).").kind == mosa::StopKind::Exception, "its clauses are rolled back");
}

void check_use_errors() {
  auto unbound = mosa::Machine::create();
  mosa_test::check(!unbound->load_program(":- use_module(_).").ok, "unbound file fails");
  mosa_test::check(mentions(*unbound, "instantiation_error"), "use_module(_): instantiation_error");
  auto number = mosa::Machine::create();
  mosa_test::check(!number->load_program(":- use_module(1).").ok, "number file fails");
  mosa_test::check(mentions(*number, "type_error(atom,1)"), "use_module(1): type_error(atom, 1)");
  auto bad_list = mosa::Machine::create();
  mosa_test::check(!bad_list->load_program(use_text("悪い", "", kModules + "人名.pl", ", bad")).ok, "bad imports");
  mosa_test::check(mentions(*bad_list, "type_error(list,bad)"), "imports bad: type_error(list, bad)");
  auto bad_item = mosa::Machine::create();
  mosa_test::check(!bad_item->load_program(use_text("悪い", "", kModules + "人名.pl", ", [foo]")).ok, "bad item");
  mosa_test::check(mentions(*bad_item, "type_error(predicate_indicator,foo)"), "type_error(predicate_indicator, foo)");
  auto bad_alias = mosa::Machine::create();
  mosa_test::check(!bad_alias->load_program(use_text("悪い", "", kModules + "人名.pl", ", [読み/2 as 1]")).ok,
                   "bad alias");
  mosa_test::check(mentions(*bad_alias, "type_error(atom,1)"), "alias 1: type_error(atom, 1)");
}

void check_use_failed_rollback() {
  const auto broken = temp_file("壊れ.pl", ":- module(壊れ, [p/1]).\np(1).\np(\n");
  auto machine = mosa::Machine::create();
  mosa_test::check(!machine->load_program(":- use_module('" + broken + "').\n").ok, "broken module fails");
  mosa_test::check(machine->query("壊れ:true.").kind == mosa::StopKind::Exception, "broken module is not left");
  mosa_test::check(machine->query("p(1).").kind == mosa::StopKind::Exception, "no import is left");
  const auto fixed = temp_file("壊れ.pl", ":- module(壊れ, [p/1]).\np(1).\n");
  mosa_test::check(machine->load_program(":- use_module('" + fixed + "').\n").ok, "fixed module loads after failure");
  mosa_test::check(succeeds(*machine, "p(1)."), "fixed module is imported");
}

void check_reconsult_module() {
  auto machine = mosa::Machine::create();
  const auto file = kModules + "家系.pl";
  mosa_test::check(machine->consult(file) && machine->consult(file), "consult module file twice");
  mosa_test::check(succeeds(*machine, "findall(X, 家系:親(X, _), [太郎, 一郎, 一郎])."), "clauses are not doubled");
  mosa_test::check(succeeds(*machine, "祖父(太郎, 花子)."), "exports still imported");
}

// 読み直しは取り込みを作り直す。ensure_loaded 済みのモジュールのファイルも、もう一度取り込む。
void check_reload_keeps_ensure_loaded() {
  temp_file("供給.pl", ":- module(供給, [q/0]).\nq.\n");
  const auto user = temp_file("利用.pl", ":- module(利用, [go/0]).\n:- ensure_loaded('供給.pl').\ngo :- q.\n");
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->consult(user) && machine->consult(user), "consult a module with ensure_loaded twice");
  mosa_test::check(succeeds(*machine, "go."), "ensure_loaded import survives the reload");
}

// 同じファイルが別のモジュール名で読み直されたら、use_module は新しいモジュールを見る。
void check_renamed_module_file() {
  auto machine = mosa::Machine::create();
  const auto file = temp_file("改名.pl", ":- module(改名前, [p/0]).\np.\n");
  mosa_test::check(machine->consult(file), "first name");
  temp_file("改名.pl", ":- module(改名後, [q/0]).\nq.\n");
  mosa_test::check(machine->consult(file), "renamed");
  mosa_test::check(machine->load_program(use_text("改名利用", "", file, ", [q/0]")).ok, "use_module sees the new name");
}

// 宣言で作る局所の述語も、取り込んだ名前とぶつかる。順序で結果が変わらない。
void check_dynamic_after_import() {
  auto machine = mosa::Machine::create();
  const auto text = use_text("動的", "", kModules + "人名.pl", "") + ":- dynamic(読み/2).\n";
  mosa_test::check(!machine->load_program(text).ok, "dynamic after import fails");
  mosa_test::check(mentions(*machine, "permission_error(import_into,procedure,人名:読み/2)"),
                   "dynamic after import: permission_error(import_into, procedure, 人名:読み/2)");
}

// 入れ子の初期化の例外で途中までしか読んでいないファイルは、次の use_module で読み直す。
void check_half_read_is_reread() {
  const auto init = temp_file("初期化.pl", ":- initialization(throw(初期化の例外)).\n");
  const auto half = temp_file("途中.pl", ":- module(途中, [p/0]).\n:- ['初期化.pl'].\np.\n");
  auto machine = mosa::Machine::create();
  mosa_test::check(!machine->consult(half), "nested initialization exception fails the consult");
  temp_file("初期化.pl", "% 例外を投げない版。\n");
  mosa_test::check(machine->load_program(":- use_module('" + half + "').\n").ok, "use_module after the failure");
  mosa_test::check(succeeds(*machine, "p."), "the half-read file is read again");
}

// 節の頭部と宣言の M:（Task 7）。無いモジュールは module/2 で作らないかぎり誤りになる。
void check_qualified_clauses() {
  auto machine = mosa::Machine::create();
  const auto status = machine->load_program(
      ":- module(m, []).\n:- dynamic(user:d/1).\nuser:u(1).\n(user:r(X) :- X = 2).\nuser:(s(X) :- t(X)).\nt(3).\n");
  mosa_test::check(status.ok, "qualified heads load");
  mosa_test::check(succeeds(*machine, "u(1), r(2)."), "qualified heads go to user");
  // 本体は節の述語のモジュールで動く（ADR 0006）。m の t/1 は user からは見えない。
  mosa_test::check(machine->query("s(3).").kind == mosa::StopKind::Exception, "body runs in user, not m");
  mosa_test::check(succeeds(*machine, "assertz(d(1)), d(1)."), "dynamic(user:d/1)");
  auto unknown = mosa::Machine::create();
  mosa_test::check(!unknown->load_program("nomod:p(1).\n").ok, "clause for an unknown module fails");
  mosa_test::check(mentions(*unknown, "existence_error(module,nomod)"), "existence_error(module, nomod)");
  auto unbound = mosa::Machine::create();
  mosa_test::check(!unbound->load_program("_:p(1).\n").ok, "clause for an unbound module fails");
  mosa_test::check(mentions(*unbound, "instantiation_error"), "unbound module: instantiation_error");
  // M: を外しても指示と文法規則は節にしない。外す前と同じく読み込みの誤りにする。
  auto rule = mosa::Machine::create();
  mosa_test::check(!rule->load_program("user:(a --> b).\n").ok, "qualified --> is rejected");
  auto directive = mosa::Machine::create();
  mosa_test::check(!directive->load_program("user:(:- true).\n").ok, "qualified directive is not a fact");
}

void check_qualified_declarations() {
  auto unknown = mosa::Machine::create();
  mosa_test::check(!unknown->load_program(":- dynamic(nomod:p/1).\n").ok, "dynamic of an unknown module fails");
  mosa_test::check(mentions(*unknown, "existence_error(module,nomod)"), "dynamic: existence_error(module, nomod)");
  auto unbound = mosa::Machine::create();
  mosa_test::check(!unbound->load_program(":- dynamic(_:p/1).\n").ok, "dynamic of an unbound module fails");
  mosa_test::check(mentions(*unbound, "instantiation_error"), "dynamic: instantiation_error");
  auto apart = mosa::Machine::create();
  const auto text = ":- module(m, []).\n:- discontiguous(m:q/1).\nuser:q(1).\nuser:r(1).\nuser:q(2).\n";
  mosa_test::check(!apart->load_program(text).ok, "discontiguous(m:q/1) does not cover user:q/1");
}

// meta_predicate/1 の誤りは読み込みの診断。読み直しで宣言が消えたら、限定もやめる（Task 9）。
void check_meta_declarations() {
  auto bad = mosa::Machine::create();
  mosa_test::check(!bad->load_program(":- meta_predicate p(foo).\n").ok, "bad meta spec fails");
  mosa_test::check(mentions(*bad, "domain_error(meta_argument_specifier,foo)"), "meta: domain_error");
  auto unbound = mosa::Machine::create();
  mosa_test::check(!unbound->load_program(":- meta_predicate p(_).\n").ok, "unbound meta spec fails");
  mosa_test::check(mentions(*unbound, "instantiation_error"), "meta: instantiation_error");
  auto builtin = mosa::Machine::create();
  mosa_test::check(!builtin->load_program(":- meta_predicate atom_length(0, ?).\n").ok, "meta of a built-in fails");
  mosa_test::check(mentions(*builtin, "permission_error(modify,static_procedure,atom_length/2)"), "meta: permission");
  const auto with = temp_file("限定.pl", ":- meta_predicate m(0, ?).\nm(G, G).\n");
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->consult(with), "consult file with meta_predicate");
  mosa_test::check(succeeds(*machine, "m(true, user:true)."), "argument wrapped");
  temp_file("限定.pl", "m(G, G).\n");
  mosa_test::check(machine->consult(with), "reconsult without meta_predicate");
  mosa_test::check(succeeds(*machine, "m(true, true)."), "declaration dropped on reconsult");
  // 宣言だけでは手続きを作らない。節が来たときに述語へ移す。
  auto alone = mosa::Machine::create();
  mosa_test::check(alone->load_program(":- meta_predicate p(0).\n:- meta_predicate foo.\n").ok, "declaration only");
  mosa_test::check(succeeds(*alone, "catch(p(true), error(existence_error(procedure, p/1), _), true)."),
                   "declared-only p/1 is still unknown");
  mosa_test::check(succeeds(*alone, "catch(foo, error(existence_error(procedure, foo/0), _), true)."),
                   "declared-only foo/0 is still unknown");
  mosa_test::check(!succeeds(*alone, "current_predicate(p/1)."), "declared-only p/1 is not current");
  mosa_test::check(alone->load_program("p(G) :- G = user:true.\n").ok, "clauses after the declaration");
  mosa_test::check(succeeds(*alone, "p(true)."), "later clauses get the declaration");
  auto rolled = mosa::Machine::create();
  // 前の読み込みで作った述語に宣言を足してから失敗する。新しい述語なら丸ごと消えるので、戻しを見られない。
  mosa_test::check(rolled->load_program("r(G) :- G.\n").ok, "define r/1");
  mosa_test::check(!rolled->load_program(":- meta_predicate r(0).\n:- foo bar.\n").ok, "failed load");
  mosa_test::check(!succeeds(*rolled, "predicate_property(r(_), meta_predicate(_))."), "meta rolled back");
}

}  // namespace

int main() {
  check_module_text();
  check_not_first();
  check_bad_module_name();
  check_bad_export();
  check_rollback();
  check_use_conflict();
  check_two_sources();
  check_not_exported();
  check_not_module_file();
  check_use_errors();
  check_use_failed_rollback();
  check_reconsult_module();
  check_reload_keeps_ensure_loaded();
  check_renamed_module_file();
  check_dynamic_after_import();
  check_half_read_is_reread();
  check_qualified_clauses();
  check_qualified_declarations();
  check_meta_declarations();
  return mosa_test::failures();
}
