// フェーズ6 Task 4。翻訳器（src/codegen）が IR から出す C++ を文面で確かめる。出した C++ を実際に
// コンパイルして動かすのは Task 5 の mosac emit から。ここは名前の変換、コメント、数え方、呼び出しの形を見る。
// translated は生成関数を出した述語の数、interpreted は IR のまま動く節の数（codegen.hpp）。

#include "harness.hpp"

#include "codegen/codegen.hpp"
#include "machine/internal.hpp"
#include "mosa/path_text.hpp"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>

namespace {

using mosa::Machine;
using mosa::codegen::EmitResult;
using mosa::codegen::emit_module;
using mosa::codegen::mangle;
using mosa_test::check;

bool has(const std::string& text, std::string_view part) { return text.find(part) != std::string::npos; }

EmitResult emit_user(std::string_view source) {
  auto machine = Machine::create();
  check(machine->load_program(source).ok, "load_program");
  return emit_module(*machine, mosa::engine_impl(*machine).user_module, "t");
}

void mangle_names() {
  check(mangle("親") == "_e8_a6_aa", "mangle 親");
  check(mangle("a_b") == "a_5fb", "mangle a_b");
  check(mangle("app") == "app", "mangle app");
  check(mangle("=..") == "_3d_2e_2e", "mangle =..");
}

constexpr std::string_view kApp = "app([], L, L).\napp([H|T], L, [H|R]) :- app(T, L, R).\n";

void app_unit() {
  const EmitResult result = emit_user(kApp);
  check(has(result.source, "% app([], L, L)."), "app comment fact");
  check(has(result.source, "% app([H|T], L, [H|R]) :- app(T, L, R)."), "app comment rule");
  check(has(result.source, "void papp_3("), "app function");
  check(has(result.source, "namespace mosa_gen::muser"), "app namespace");
  check(has(result.source, "#include \"mosa/runtime.hpp\""), "app include");
  check(has(result.header, "papp_3"), "app header declares the function");
  check(result.translated == 1, "app translated == 1");
  check(result.interpreted == 0, "app interpreted == 0");
}

// 指紋は install_unit が起動時に出す値と同じ関数で出す。翻訳した Machine で同じ値を数えて比べる。
void app_fingerprint() {
  auto machine = Machine::create();
  check(machine->load_program(kApp).ok, "load_program fingerprint");
  auto& impl = mosa::engine_impl(*machine);
  const auto* predicate = mosa::find_pred(impl, mosa::PredKey{impl.user_module, impl.atoms.intern("app"), 3});
  check(predicate != nullptr, "app predicate");
  if (predicate == nullptr) return;
  const EmitResult result = emit_module(*machine, impl.user_module, "t");
  std::ostringstream text;
  text << "0x" << std::hex << std::setw(16) << std::setfill('0') << mosa::clause_fingerprint(impl.atoms, *predicate);
  check(has(result.source, text.str()), "app fingerprint literal");
}

// 2 つ目以降の本体のゴールは Ctrl::Native の継続枠から再開する。
void nrev_resume() {
  const EmitResult result = emit_user(std::string{"nrev([], []).\nnrev([H|T], R) :- nrev(T, S), app(S, [H], R).\n"} +
                                      std::string{kApp});
  check(has(result.source, "rt::resume_frame("), "nrev resume_frame");
  check(has(result.source, "case rt::state_of(2, 1):"), "nrev resume state");
  check(has(result.source, "rt::call_goal("), "nrev call_goal");
  check(result.translated == 2, "nrev translated == 2");
  check(result.interpreted == 0, "nrev interpreted == 0");
}

// ; を含む節は IR のまま動かす。節がすべてそうなら生成関数を出さない。
void disjunction_interpreted() {
  const EmitResult only = emit_user("q(X) :- (X = a ; X = b).\n");
  check(only.translated == 0, "q translated == 0");
  check(only.interpreted == 1, "q interpreted == 1");
  check(!has(only.source, "void pq_1("), "q has no function");
  const EmitResult mixed = emit_user("r(a).\nr(X) :- (X = b ; X = c).\n");
  check(mixed.translated == 1, "r translated == 1");
  check(mixed.interpreted == 1, "r interpreted == 1");
  check(has(mixed.source, "rt::interpret_clause(m, r, 0, frame, 2)"), "r clause 2 interpreted");
}

// install_unit が付けない述語（meta_predicate の宣言あり）は生成関数を出さず、節を interpreted に数える。
void meta_skipped() {
  const EmitResult result = emit_user(":- meta_predicate twice(0).\ntwice(G) :- call(G), call(G).\n");
  check(result.translated == 0, "twice translated == 0");
  check(result.interpreted == 1, "twice interpreted == 1");
  check(!has(result.source, "void ptwice_1("), "twice has no function");
}

// 動的述語は翻訳の対象ではない。
void dynamic_skipped() {
  const EmitResult result = emit_user(":- dynamic(counter/1).\ncounter(0).\n");
  check(result.translated == 0, "dynamic translated == 0");
  check(result.interpreted == 0, "dynamic interpreted == 0");
}

// コメントと文字列を除いた識別子に __ が無いか見る（C++ は __ を含む識別子を予約する）。
bool has_double_underscore(std::string_view text) {
  std::string word;
  for (std::size_t at = 0; at < text.size(); ++at) {
    const char unit = text[at];
    if (unit == '/' && at + 1 < text.size() && text[at + 1] == '/') {
      while (at < text.size() && text[at] != '\n') ++at;
    } else if (unit == '"') {
      for (++at; at < text.size() && text[at] != '"'; ++at) {
        if (text[at] == '\\') ++at;
      }
    } else if (unit == '_' || (unit >= '0' && unit <= '9') || (unit >= 'a' && unit <= 'z') || (unit >= 'A' && unit <= 'Z')) {
      word += unit;
      continue;
    }
    if (word.find("__") != std::string::npos) return true;
    word.clear();
  }
  return word.find("__") != std::string::npos;
}

void no_double_underscore() {
  const EmitResult result = emit_user("a_b(1).\n親_子(太郎, _).\nx_(1).\n" + std::string{kApp});
  check(has(result.source, "void pa_5fb_1("), "underscore function");
  check(has(result.source, "void p_e8_a6_aa_5f_e5_ad_90_2("), "japanese underscore function");
  check(!has_double_underscore(result.source), "no double underscore in source");
  check(!has_double_underscore(result.header), "no double underscore in header");
}

// 日本語の名前は関数名を mangle し、アトムの字面は 8 進のエスケープにする。コメントは原文のまま。
void japanese_unit() {
  const EmitResult result = emit_user("親(太郎, 花子).\n");
  check(has(result.source, "% 親(太郎, 花子)."), "japanese comment");
  check(has(result.source, "void p_e8_a6_aa_2("), "japanese function");
  check(has(result.source, "\"\\345\\244\\252\\351\\203\\216\""), "japanese atom literal");
  check(result.translated == 1, "japanese translated == 1");
}

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
  check(project.ok, "project ok: " + project.diagnostic);
  check(find_file(project, "muser.cpp") != nullptr && find_file(project, "mtwo.cpp") != nullptr, "unit per module");
  const auto* main_file = find_file(project, "main.cpp");
  check(main_file != nullptr && has(main_file->text, "part.pl"), "include embedded");
  const auto user_at = main_file == nullptr ? std::string::npos : main_file->text.find("muser::unit(");
  const auto two_at = main_file == nullptr ? std::string::npos : main_file->text.find("mtwo::unit(");
  check(user_at != std::string::npos && two_at != std::string::npos && user_at < two_at, "root unit first");
  write_file(dir / "skip.pl", ":- assertz(seen(1)).\nmain :- seen(1).\n");
  const auto skipped = mosa::codegen::translate_program(mosa::utf8_from_path(dir / "skip.pl"), {});
  check(!skipped.ok && has(skipped.diagnostic, ":2: ") && has(skipped.diagnostic, "seen/1"), "directive skipped");
  mosa::codegen::ProjectOptions units;
  units.check_calls = false;
  check(mosa::codegen::translate_program(mosa::utf8_from_path(dir / "skip.pl"), units).ok, "units do not stop on calls");
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

int main() {
  mangle_names();
  no_double_underscore();
  app_unit();
  app_fingerprint();
  nrev_resume();
  disjunction_interpreted();
  meta_skipped();
  dynamic_skipped();
  japanese_unit();
  project_modules();
  project_opaque();
  return mosa_test::failures() == 0 ? 0 : 1;
}
