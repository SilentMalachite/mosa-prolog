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

// 計画 Task 7。カットは節の境界まで切る。入口では r.cut、呼び出しのあとの再開では継続枠の語 2 が境界。
void cut_translated() {
  const EmitResult result = emit_user("cutc(first).\ncutc(X) :- !, X = second.\ncutc(third).\n");
  check(result.translated == 1, "cutc translated == 1");
  check(result.interpreted == 0, "cutc interpreted == 0");
  check(has(result.source, "rt::cut_to(m, r.cut);"), "cutc cut at entry");
  const EmitResult later = emit_user("first_color(C) :- color(C), !.\ncolor(red).\ncolor(green).\n");
  check(later.translated == 2, "first_color translated == 2");
  check(has(later.source, "rt::cut_to(m, rt::frame_cut(r, frame));"), "cut after a call uses the frame barrier");
}

// 計画 Task 7。; は else の枝を Pick::Native の選択点に積んでから左の枝を続ける。節はすべて生成コードで動く。
// 動かせない形（halt）を含む節だけが IR のまま残る。
void disjunction_translated() {
  const EmitResult only = emit_user("q(X) :- (X = a ; X = b).\n");
  check(only.translated == 1, "q translated == 1");
  check(only.interpreted == 0, "q interpreted == 0");
  check(has(only.source, "rt::push_branch("), "q pushes the else branch");
  const EmitResult mixed = emit_user("r(a).\nr(X) :- (X = b ; X = c), s(X).\nr(_) :- halt.\ns(_).\n");
  check(mixed.translated == 2, "r and s translated");
  check(mixed.interpreted == 1, "halt clause interpreted");
  check(has(mixed.source, "rt::interpret_clause(m, r, 0, frame, 3)"), "r clause 3 interpreted");
}

// 計画 Task 7。(A -> B ; C) は積む前の選択点の高さを then の枠の整数セルで運び、A の成功でそこまで切る。
void if_then_else_translated() {
  const EmitResult result =
      emit_user("sign(X, S) :- (X > 0 -> S = positive ; S = other).\nonly(X) :- (X = 1 -> true).\n");
  check(result.translated == 2, "if-then-else translated == 2");
  check(result.interpreted == 0, "if-then-else interpreted == 0");
  check(has(result.source, "rt::choice_mark(m);"), "condition takes the choice mark");
  check(has(result.source, "rt::cut_to(m, static_cast<std::size_t>(rt::small_value(rt::var(r, frame, 0))));"),
        "then cuts back to the mark");
  check(has(result.source, "rt::push_branch("), "else branch pushed");
}

// 計画 Task 7。(A *-> B ; C) は印の付く選択点に else を積み、A の成功で印を付ける。印の付いた else はやり直しで飛ぶ。
void soft_cut_translated() {
  const EmitResult result = emit_user(
      "soft(X) :- (color(X) *-> true ; X = none).\nbare(X) :- (color(X) *-> X \\== b).\n"
      "dead(X) :- (fail *-> X = yes ; X = none).\ncolor(a).\ncolor(b).\n");
  check(result.translated == 4, "soft cut translated == 4");
  check(result.interpreted == 0, "soft cut interpreted == 0");
  check(has(result.source, "rt::push_soft("), "soft else pushed");
  check(has(result.source, "rt::soft_commit(m, static_cast<std::size_t>(rt::small_value(rt::var(r, frame, 0))));"),
        "then commits the soft cut");
  // 条件が fail で始まれば then の枠は使われない。作ると未使用の変数で生成コードが通らないので、else だけになる。
  check(!has(result.source, "pdead_1_c1_s1("), "dead condition leaves only the else branch");
}

// 計画 Task 7。\+ A は床（Pick::Negation）を積み、A が成功すれば床まで切って失敗する。中が変数なら組み込みに任せる。
void negation_translated() {
  const EmitResult result = emit_user("nc(X) :- \\+ color(X).\nnv(G) :- \\+ G.\ncolor(a).\n");
  check(result.translated == 3, "negation translated == 3");
  check(result.interpreted == 0, "negation interpreted == 0");
  check(has(result.source, "rt::push_negation("), "negation floor pushed");
  check(has(result.source, "std::string_view{\"\\\\+\", 2}"), "variable inner calls the builtin");
  // 中が呼び出し無しに失敗する \+ は true と同じ。床を積むと done の枠が使われず、生成コードが通らない。
  const EmitResult dead = emit_user("nd(X) :- \\+ fail, X = 1.\n");
  check(dead.translated == 1, "dead negation translated == 1");
  check(!has(dead.source, "rt::push_negation("), "dead negation leaves no floor");
}

// 計画 Task 7。変数のゴールは call/1、リストのゴールは '.'/2 の呼び出しとして出す（launch_var、launch_list）。
// call/N は組み込みなので、他の組み込みと同じく call_goal で呼ぶ。
void var_goal_translated() {
  const EmitResult result =
      emit_user("ag(G) :- G.\nat(G, Y) :- G, Y = done.\nce(X) :- call(color, X).\nlg(F) :- [F].\ncolor(a).\n");
  check(result.translated == 5, "var goal translated == 5");
  check(result.interpreted == 0, "var goal interpreted == 0");
  check(has(result.source, "std::string_view{\"call\", 4}, 1}"), "var goal calls call/1");
  check(has(result.source, "std::string_view{\"call\", 4}, 2}"), "call/2 is an ordinary callee");
  check(has(result.source, "std::string_view{\".\", 1}, 2}"), "list goal calls ./2");
}

// 計画 Task 7。throw/1 はボールを組み立てて rt::throw_term に渡す。C++ の例外ではなく propagate_throw で巻き戻す。
void throw_translated() {
  const EmitResult result =
      emit_user("tb(B) :- throw(B).\nta(X) :- X = 1, throw(got(X)).\ntc(X) :- (throw(cond) -> X = 1 ; X = 2).\n");
  check(result.translated == 3, "throw translated == 3");
  check(result.interpreted == 0, "throw interpreted == 0");
  check(has(result.source, "return rt::throw_term(m, "), "throw passes the ball");
  // 条件が呼び出し無しに throw で終わる構文は、その throw と同じ。then と else の枠を作ると使われない。
  check(!has(result.source, "ptc_1_c1_s1("), "throwing condition leaves no branch frames");
}

// 計画 Task 7。catch/3 は回復ゴールの枠と抜けたあとの枠を先に作り、Catch を積んでからゴールを局所の境界で動かす。
void catch_translated() {
  const EmitResult result = emit_user(
      "ci(G, R) :- catch(G, E, R = caught(E)).\ncc(X) :- (X = 1 ; X = 2), catch(throw(t), t, !), X = 2.\n");
  check(result.translated == 2, "catch translated == 2");
  check(result.interpreted == 0, "catch interpreted == 0");
  check(has(result.source, "rt::push_catch("), "catch pushed");
  check(has(result.source, "rt::catch_exit(m, static_cast<std::size_t>(rt::small_value(rt::var(r, frame, 0))));"),
        "exit disarms the catch");
  // 回復ゴールのカットは回復ゴールに閉じる。境界は枠に入った時点の選択点の高さ。
  check(has(result.source, "rt::cut_to(m, cut0);"), "recovery cut is local");
}

// 計画 Task 7。M:G は、M が翻訳のときに読み込み済みなら G の並びを M の文脈で置く。変数の M や G は組み込みの :/2 へ。
void qualified_translated() {
  const EmitResult result = emit_user("q(X) :- user:color(X).\nqc(X) :- (user:(color(X), !) ; X = other).\ncolor(a).\n");
  check(result.translated == 3, "qualified translated == 3");
  check(result.interpreted == 0, "qualified interpreted == 0");
  check(!has(result.source, "std::string_view{\":\", 1}"), "known module is inlined, not called through :/2");
  // color(X) のあとの ! は再開の状態にあり、境界は継続枠の語 2（節の境界）。局所なら cutN になる。
  check(has(result.source, "rt::cut_to(m, rt::frame_cut(r, frame));"), "cut inside M:G is transparent");
  const EmitResult var = emit_user("qv(M, G) :- M:G.\nqm(X) :- no_such_module:color(X).\ncolor(a).\n");
  check(var.translated == 3, "qualified var translated == 3");
  check(has(var.source, "std::string_view{\":\", 1}, 2}"), "variable or unknown module calls :/2");
}

// 計画 Task 7。is/2 と算術比較は call_goal を通さず組み込みの C++ を直に呼び、同じ関数で続きを進める。
void arith_translated() {
  const EmitResult result = emit_user(
      "inc(X, Y) :- Y is X + 1.\nsign(X, S) :- (X > 0 -> S = positive ; X =:= 0 -> S = zero ; S = negative).\n");
  check(result.translated == 2, "arith translated == 2");
  check(result.interpreted == 0, "arith interpreted == 0");
  // 述語のアトムは単位の表から渡す（誤りの文脈のため）。呼び先の表には入らない。
  check(has(result.source, "if (!rt::arith_is(m, r.atoms["), "is/2 is a direct call");
  check(has(result.source, "if (!rt::arith_compare(m, rt::Compare::Gt, r.atoms["), "> is a direct call");
  check(has(result.source, "if (!rt::arith_compare(m, rt::Compare::Eq, r.atoms["), "=:= is a direct call");
  check(!has(result.source, "std::string_view{\"is\", 2}, 2}"), "is/2 is not a callee");
  check(!has(result.source, "std::string_view{\">\", 1}, 2}"), ">/2 is not a callee");
}

// 計画 Task 7。meta_predicate の述語も訳す。限定する引数だけを、呼び手の文脈（枠の語 1）で M: に包んでから頭部に渡す。
void meta_translated() {
  const EmitResult result = emit_user(":- meta_predicate twice(0).\ntwice(G) :- call(G), call(G).\n");
  check(result.translated == 1, "meta translated == 1");
  check(result.interpreted == 0, "meta interpreted == 0");
  check(has(result.source, "void ptwice_1("), "twice has a function");
  check(has(result.source, "rt::qualify(m, rt::arg(r, frame, 0), rt::frame_module(r, frame))"), "twice qualifies G");
  const EmitResult plain = emit_user(":- meta_predicate plain(+, 0).\nplain(X, G) :- call(G), X = 1.\n");
  check(has(plain.source, "rt::qualify(m, rt::arg(r, frame, 1), rt::frame_module(r, frame))"), "plain qualifies G");
  check(!has(plain.source, "rt::qualify(m, rt::arg(r, frame, 0)"), "plain leaves X unqualified");
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

// 8 進の直後に 10 進の数字が続く字面は MSVC の /W4 で C4125 になる。そこで字面を閉じ、隣の字面としてつなぐこと。
void octal_before_digit() {
  const EmitResult result = emit_user("p('あ1').\n");
  check(has(result.source, "\"\\343\\201\\202\" \"1\", 4}"), "octal escape closed before a digit");
  check(!has(result.source, "\\2021"), "no digit directly after an octal escape");
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
  check(mosa::codegen::translate_program(mosa::utf8_from_path(dir / "skip.pl"), units).ok, "check_calls off");
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
  cut_translated();
  disjunction_translated();
  if_then_else_translated();
  soft_cut_translated();
  negation_translated();
  var_goal_translated();
  throw_translated();
  catch_translated();
  qualified_translated();
  arith_translated();
  meta_translated();
  dynamic_skipped();
  japanese_unit();
  octal_before_digit();
  project_modules();
  project_opaque();
  return mosa_test::failures() == 0 ? 0 : 1;
}
