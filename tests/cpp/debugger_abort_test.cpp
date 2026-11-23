// フェーズ 14（ADR 0028）のデバッガの a。a が StopKind::Abort で問い合わせを止め、デバッグの状態を変えないこと
// （タスク 7）、a が止める実行ごとに内側から後始末を 1 回ずつ動かすこと（2026-11-18 の持ち主の決定 A）と、例外の
// ときと同じ束縛で動かすこと（同じ日の決定 B）と、その動かし方（ポート無し、誤りを捨てる、デバッガの状態を変えない。
// open-items の既定 A）、指令と initialization/1 の中の a（持ち主の決定 A）。debugger_test.cpp から分けた。
#include "debugger_support.hpp"

#include "machine/builtin_api.hpp"
#include "mosa/path_text.hpp"

#include <array>
#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>

namespace {

using namespace debugger_support;

// タスク 7。呼出のポートの a で問い合わせは Abort で止まり、デバッグの状態は変えない。印は次の問い合わせの
// drop_run が消す。その前の load_program の指令は、残った印で止まらない（HaltSetAside が脇に置く）。
void check_abort_stop() {
  auto machine = make_machine();
  Scripted script{" Call: (2) len([],_G1) ? ", "a", {}, {}, false};
  give_script(*machine, script);
  const mosa::Stop stop = machine->query("trace, len([a], N).");
  const auto& impl = mosa::engine_impl(*machine);
  mosa_test::check(stop.kind == mosa::StopKind::Abort, "abort_stop: Abort, got [" + stop_text(*machine, stop) + "]");
  mosa_test::check(impl.abort_now, "abort_stop: abort_now is set until the next query");
  mosa_test::check(impl.debug_on && impl.debug.mode == mosa::DebugMode::Creep, "abort_stop: the debug state is kept");
  mosa_test::check(occurrences(script.text, " Exit: ") == 0, "abort_stop: no port after a, [" + script.text + "]");
  mosa_test::check(machine->load_program(":- true.\n").ok, "abort_stop: a directive after the abort runs");
  mosa_test::check(impl.abort_now, "abort_stop: load_program keeps the mark of the aborted query");
  // ポートを出す指令も、残った印に止められずに最後まで動き、デバッガは creep のままポートを書く。true にはポートが
  // 無いので、それだけでは指令の中のポートを見ていなかった（Muse のレビュー）。
  const std::size_t before = script.text.size();
  mosa_test::check(machine->load_program(":- len([], 0).\n").ok, "abort_stop: a traced directive after the abort runs");
  const std::string directive_ports = script.text.substr(before);
  mosa_test::check(occurrences(directive_ports, " Call: (1) len([],0) ? ") == 1 &&
                       occurrences(directive_ports, " Exit: (1) len([],0) ? ") == 1,
                   "abort_stop: the directive's ports are written, [" + directive_ports + "]");
  mosa_test::check(impl.abort_now, "abort_stop: the traced directive keeps the mark too");
  const mosa::Stop next = machine->query("true.");
  mosa_test::check(next.kind == mosa::StopKind::Success, "abort_stop: the next query succeeds");
  mosa_test::check(!impl.abort_now, "abort_stop: the next query cleared abort_now");
}

// abort は halt と違い、止まる前に setup_call_cleanup/3 の後始末を動かす（2026-11-18 の持ち主の決定 A）。後始末は
// ポートを出さない。次の問い合わせが前を閉じるときにはもう残っていないので、数は 1 回になる。
void check_abort_cleanup() {
  std::string answer;
  std::string ports;
  const std::string count = run_cleanup_case({"abort_cleanup", "trace, r", " Exit: (1) r ? ", "a"}, answer, ports);
  mosa_test::check(answer == "abort", "abort_cleanup: Abort, got [" + answer + "]");
  mosa_test::check(count_is(count, 1), "abort_cleanup: N = 1, got [" + count + "]");
  // ポートの行は「 Call: (N) ゴール」。上の箱のゴールの字面にも assertz が入るので、行の形で数える。
  mosa_test::check(occurrences(ports, ") assertz(") == 0, "abort_cleanup: no port in the cleanup, [" + ports + "]");
}

constexpr std::string_view k_abort_program =
    ":- dynamic(log/1).\n"
    "t.\n"
    "w :- setup_call_cleanup(true, with_output_to(atom(_), setup_call_cleanup(true, t, assertz(log(inner)))),\n"
    "    assertz(log(outer))).\n";

// with_output_to/2 の入れ子の実行の中の a。内側の実行が床（Barrier）より上の後始末を先に動かし、外の実行が残りを
// 動かす。どちらも 1 回だけで、デバッグの状態と abort_now は残る。
void check_abort_nested_cleanup() {
  auto machine = mosa::Machine::create();
  auto& impl = mosa::engine_impl(*machine);
  mosa_test::check(machine->load_program(k_abort_program).ok, "abort_nested_cleanup: load the program");
  impl.message_lang = mosa::MessageLanguage::En;
  Scripted script{" Call: (5) t ? ", "a", {}, {}, false};
  give_script(*machine, script);
  const mosa::Stop stop = machine->query("trace, w.");
  mosa_test::check(stop.kind == mosa::StopKind::Abort,
                   "abort_nested_cleanup: Abort, got [" + stop_text(*machine, stop) + "], ports [" + script.text + "]");
  mosa_test::check(impl.abort_now, "abort_nested_cleanup: abort_now is kept");
  mosa_test::check(impl.debug_on && impl.debug.mode == mosa::DebugMode::Creep, "abort_nested_cleanup: state kept");
  mosa_test::check(occurrences(script.text, ") assertz(") == 0,
                   "abort_nested_cleanup: no port in the cleanups, [" + script.text + "]");
  const mosa::Stop logged = machine->query("nodebug, findall(X, log(X), L).");
  const std::string text = stop_text(*machine, logged);
  mosa_test::check(text.find("L = [inner,outer]") != std::string::npos,
                   "abort_nested_cleanup: inner then outer, once each, got [" + text + "]");
}

constexpr std::string_view k_abort_rules_program =
    ":- dynamic(log/1).\n"
    "t.\n"
    "e :- setup_call_cleanup(true, setup_call_cleanup(true, setup_call_cleanup(true, t, fail), throw(oops)),\n"
    "    assertz(log(after))).\n"
    "d :- setup_call_cleanup(true, t, (nodebug, assertz(log(nodebug)))).\n";

// abort の後始末の失敗と誤りは捨て、残りの後始末も動かして abort を続ける（既定 A、open-items）。
void check_abort_cleanup_errors() {
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->load_program(k_abort_rules_program).ok, "abort_cleanup_errors: load the program");
  mosa::engine_impl(*machine).message_lang = mosa::MessageLanguage::En;
  Scripted script{" Call: (5) t ? ", "a", {}, {}, false};
  give_script(*machine, script);
  const mosa::Stop stop = machine->query("trace, e.");
  mosa_test::check(stop.kind == mosa::StopKind::Abort,
                   "abort_cleanup_errors: Abort, got [" + stop_text(*machine, stop) + "], ports [" + script.text + "]");
  const std::string text = stop_text(*machine, machine->query("nodebug, findall(X, log(X), L)."));
  mosa_test::check(text.find("L = [after]") != std::string::npos, "abort_cleanup_errors: the rest ran, got [" + text + "]");
}

// 後始末がデバッガを切っても、abort の後のデバッガは打ったときのまま（マニュアルの「モードはそのまま」、既定 A）。
void check_abort_cleanup_keeps_mode() {
  auto machine = mosa::Machine::create();
  auto& impl = mosa::engine_impl(*machine);
  mosa_test::check(machine->load_program(k_abort_rules_program).ok, "abort_keeps_mode: load the program");
  impl.message_lang = mosa::MessageLanguage::En;
  Scripted script{" Call: (3) t ? ", "a", {}, {}, false};
  give_script(*machine, script);
  const mosa::Stop stop = machine->query("trace, d.");
  mosa_test::check(stop.kind == mosa::StopKind::Abort, "abort_keeps_mode: Abort, ports [" + script.text + "]");
  mosa_test::check(impl.debug_on && impl.debug.mode == mosa::DebugMode::Creep,
                   "abort_keeps_mode: still creep after a cleanup that ran nodebug");
  const std::string text = stop_text(*machine, machine->query("nodebug, findall(X, log(X), L)."));
  mosa_test::check(text.find("L = [nodebug]") != std::string::npos, "abort_keeps_mode: the cleanup ran, got [" + text + "]");
}

constexpr std::string_view k_abort_bindings_program =
    ":- dynamic(log/1).\n"
    "t.\n"
    "b(X) :- setup_call_cleanup(S = s, (X = g, t), assertz(log(S-X))).\n"
    "n(X) :- setup_call_cleanup(true, setup_call_cleanup(true, (X = g, t), X = inner), assertz(log(X))).\n"
    "c :- catch(setup_call_cleanup(true, t, (assertz(log(ran)), throw(oops))), _, assertz(log(caught))),\n"
    "    assertz(log(went_on)).\n"
    "boom :- throw(e).\n"
    "x(X) :- setup_call_cleanup(S = s, (X = g, boom), assertz(log(S-X))).\n"
    "o(X) :- setup_call_cleanup(S = s, (X = g, with_output_to(atom(_),\n"
    "    setup_call_cleanup(true, (Y = h, t), assertz(log(in(X-Y)))))), assertz(log(out(S-X)))).\n";

// goal を target のポートの a で止め、log を question で読んだ答えの字面を返す。
std::string abort_log(std::string_view label, std::string_view goal, std::string_view target, std::string_view question) {
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->load_program(k_abort_bindings_program).ok, std::string{label} + ": load the program");
  mosa::engine_impl(*machine).message_lang = mosa::MessageLanguage::En;
  Scripted script{std::string{target}, "a", {}, {}, false};
  give_script(*machine, script);
  const mosa::Stop stop = machine->query(std::string{goal} + ".");
  mosa_test::check(stop.kind == mosa::StopKind::Abort && script.used,
                   std::string{label} + ": Abort, got [" + stop_text(*machine, stop) + "], ports [" + script.text + "]");
  return stop_text(*machine, machine->query(std::string{question} + "."));
}

// abort の後始末は、例外が通るときと同じ束縛を見る（2026-11-18 の持ち主の決定 B）。Setup の束縛 S = s は見え、Goal の
// 束縛 X = g は見えない。入れ子なら内側の後始末の束縛 X = inner も、外側の後始末の前に戻る。前は abort の時点の束縛の
// まま動き、log(s-g) と log(g) が残った。
void check_abort_cleanup_bindings() {
  const std::string single = abort_log("abort_bindings", "trace, b(X)", " Call: (3) t ? ",
                                       "nodebug, findall(S-V, (log(S-X), (var(X) -> V = unbound ; V = X)), L)");
  mosa_test::check(single.find("L = [s-unbound]") != std::string::npos,
                   "abort_bindings: Setup's binding stays, Goal's is undone, got [" + single + "]");
  const std::string nested = abort_log("abort_bindings_nested", "trace, n(X)", " Call: (4) t ? ",
                                       "nodebug, findall(V, (log(X), (var(X) -> V = unbound ; V = X)), L)");
  mosa_test::check(nested.find("L = [unbound]") != std::string::npos,
                   "abort_bindings_nested: the inner cleanup's binding is undone too, got [" + nested + "]");
  // 例外のポートの a。propagate_throw が Entry に着く前に止まるので、後始末は abort が 1 回だけ、同じ束縛で動かす。
  const std::string thrown = abort_log("abort_bindings_thrown", "trace, x(X)", " Exception: (3) boom e ? ",
                                       "nodebug, findall(S-V, (log(S-X), (var(X) -> V = unbound ; V = X)), L)");
  mosa_test::check(thrown.find("L = [s-unbound]") != std::string::npos,
                   "abort_bindings_thrown: once, with Setup's binding only, got [" + thrown + "]");
  // with_output_to/2 の入れ子の実行をまたぐ変数。内側の後始末には、内側の Setup より前に外で付いた X = g は見え、内側の
  // Goal の Y = h は見えない。外の実行は内側のスタックを戻した後で自分の Entry を戻すので、外の後始末には X = g も見えない。
  const std::string crossed =
      abort_log("abort_bindings_barrier", "trace, o(X)", " Call: (5) t ? ",
                "nodebug, findall(K-P-Q, (log(E), E =.. [K, P0-Q0], (var(P0) -> P = u ; P = P0),"
                " (var(Q0) -> Q = u ; Q = Q0)), L)");
  mosa_test::check(crossed.find("L = [in-g-u,out-s-u]") != std::string::npos,
                   "abort_bindings_barrier: each run undoes to its own entries, got [" + crossed + "]");
}

// abort の後始末の誤りは、下に残る catch/3 にも捕まらない。後始末を Entry の上で動かすので、受けずに投げると catch/3 が
// 捕まえ、止めたはずの c が先へ進んでしまう。
void check_abort_cleanup_uncaught() {
  const std::string text =
      abort_log("abort_uncaught", "trace, c", " Call: (3) t ? ", "nodebug, findall(X, log(X), L)");
  // 後始末は投げる前に log(ran) を残す。空の答えだけでは、後始末が動かなかったときと区別できない（Muse のレビュー）。
  mosa_test::check(text.find("L = [ran]") != std::string::npos,
                   "abort_uncaught: the cleanup ran and no catch/3 saw its error, got [" + text + "]");
}

// カットが積んだまま abort に会った後始末（pending_cleanups）も動かす。解釈する節では、積んだ歩みの後、次のポートの
// 歩みより前に settle_cleanups が動かすので、ポートの a はこの枝に届かない。だから機械に直接積んで呼ぶ（PR #4 の 2 度目の
// 敵対的レビュー）。枝を外すと、積んだ後始末は捨てられ log(cut) が残らない。
void check_abort_pending_cleanup() {
  auto machine = mosa::Machine::create();
  auto& impl = mosa::engine_impl(*machine);
  mosa_test::check(machine->load_program(":- dynamic(log/1).\n").ok, "abort_pending: load");
  const std::array<mosa::Cell, 1> cut{impl.heap.atom(impl.atoms.intern("cut"))};
  const std::array<mosa::Cell, 1> logged{impl.heap.structure(impl.atoms.intern("log"), cut)};
  const mosa::Cell goal = impl.heap.structure(impl.atoms.intern("assertz"), logged);
  impl.pending_cleanups.push_back(mosa::PendingCleanup{goal, impl.user_module});
  mosa::run_abort_cleanups(impl);
  mosa_test::check(impl.pending_cleanups.empty() && impl.abort_now, "abort_pending: taken, and the abort goes on");
  const mosa::Stop got = machine->query("log(X).");
  const std::string text = stop_text(*machine, got);
  mosa_test::check(text.find("X = cut") != std::string::npos, "abort_pending: the cleanup ran, got [" + text + "]");
}

// 指令の中の a の後始末がヒープの上限で止まる（C++ の例外で抜ける）。ヒープの止まり方が abort に代わるので abort_now は
// 立たない。前は印が読み込みの後も残り、セッションは後の :- fail. を abort と書いた（PR #4 の 2 度目の敵対的レビュー。
// その字面は transcript_debug_abort_heap_limit が見る）。問い合わせの道は drop_run が印を消すので、読み込みの道で見る。
// 後の指令と問い合わせがふつうに動くことも見る。
void check_abort_cleanup_heap_limit() {
  auto machine = mosa::Machine::create(std::size_t{16} << 20);
  auto& impl = mosa::engine_impl(*machine);
  impl.message_lang = mosa::MessageLanguage::En;
  mosa_test::check(machine->load_program("t.\n").ok, "abort_heap_limit: load t/0");
  Scripted script{" Call: (1) t ? ", "a", {}, {}, false};
  give_script(*machine, script);
  // 上限は ResourceLimit で load_program の外へ抜ける。セッションと同じく受けてヒープを空ける（recover_heap）。
  bool limited = false;
  try {
    (void)machine->load_program(":- setup_call_cleanup(true, (trace, t), (length(L, 50000000), L = [_|_])).\n");
  } catch (const mosa::ResourceLimit&) {
    limited = true;
    machine->heap().truncate(0);
  }
  mosa_test::check(limited && script.used, "abort_heap_limit: the cleanup reached the limit, [" + script.text + "]");
  mosa_test::check(!impl.abort_now, "abort_heap_limit: abort_now stays clear after the heap stop");
  const mosa::LoadStatus next = machine->load_program(":- fail.\n");
  mosa_test::check(!next.ok && next.message.find("directive goal failed") != std::string::npos,
                   "abort_heap_limit: a later failed directive is a failure, got [" + next.message + "]");
  mosa_test::check(machine->query("true.").kind == mosa::StopKind::Success,
                   "abort_heap_limit: the next query is not stopped as an abort");
}

// ヒープが満ちたまま abort の後始末に入る。最初の後始末の項を作るところで ResourceLimit が抜け、run_aborted が印を
// 下ろす前に止まる。上と同じくヒープの止まり方が abort に代わるので、印は残さない。前は Restore が抜けるときに何も
// せず、ポートの a が立てた印が残った（PR #4 の 2 度目の修正への Muse のレビュー）。
void check_abort_cleanup_heap_full() {
  auto machine = mosa::Machine::create(std::size_t{2} << 20);
  auto& impl = mosa::engine_impl(*machine);
  const mosa::Cell goal = impl.heap.atom(impl.atoms.intern("true"));
  impl.pending_cleanups.push_back(mosa::PendingCleanup{goal, impl.user_module});
  // ポートの a と同じく印を立ててから呼ぶ（debug_port.cpp の ask）。
  impl.abort_now = true;
  bool full = false;
  try {
    for (;;) static_cast<void>(impl.heap.fresh_ref());
  } catch (const mosa::ResourceLimit&) {
    full = true;
  }
  bool limited = false;
  try {
    mosa::run_abort_cleanups(impl);
  } catch (const mosa::ResourceLimit&) {
    limited = true;
  }
  machine->heap().truncate(0);
  mosa_test::check(full && limited, "abort_heap_full: the first cleanup term reached the limit");
  mosa_test::check(!impl.abort_now, "abort_heap_full: abort_now is clear after the heap stop");
  mosa_test::check(machine->query("true.").kind == mosa::StopKind::Success, "abort_heap_full: the next query runs");
}

// 指令の中の a。指令が脇に置いたスタックの後始末を動かしてから読み込みを戻す。大域変数は読み込みの巻き戻しに
// 戻されないので、後始末が動いたことが見える。
void check_abort_directive_cleanup() {
  auto machine = make_machine();
  Scripted script{" Call: (1) dbg_t ? ", "a", {}, {}, false};
  give_script(*machine, script);
  mosa_test::check(machine->query("nb_setval(dbg_k, none).").kind == mosa::StopKind::Success,
                   "abort_directive_cleanup: nb_setval");
  const mosa::LoadStatus status =
      machine->load_program("dbg_t.\n:- setup_call_cleanup(true, (trace, dbg_t), nb_setval(dbg_k, done)).\n");
  mosa_test::check(!status.ok && script.used, "abort_directive_cleanup: the load is aborted, [" + script.text + "]");
  const mosa::Stop got = machine->query("nodebug, nb_getval(dbg_k, V).");
  const std::string text = stop_text(*machine, got);
  mosa_test::check(text.find("V = done") != std::string::npos, "abort_directive_cleanup: ran, got [" + text + "]");
}

// initialization/1 のゴールの abort。例外と同じく読み込みは失敗し、後の初期化は動かない。診断は書かない。節は
// 例外のときと同じく残る（持ち主の決定 A、2026-11-18）。指令で読んだファイルの分は abort_nested_* の記録で確かめる。
void check_abort_init() {
  auto machine = make_machine();
  Scripted script{" Call: (1) dbg_init ? ", "a", {}, {}, false};
  give_script(*machine, script);
  mosa_test::check(machine->query("trace.").kind == mosa::StopKind::Success, "abort_init: trace");
  const mosa::LoadStatus status = machine->load_program(
      "dbg_init.\nloaded(1).\n:- initialization(dbg_init).\n:- initialization(assertz(second_init)).\n");
  const auto& impl = mosa::engine_impl(*machine);
  mosa_test::check(!status.ok && status.message.empty(),
                   "abort_init: fails with no diagnostic, [" + status.message + "]");
  mosa_test::check(impl.abort_now, "abort_init: abort_now is kept for the caller");
  const mosa::Stop kept =
      machine->query("notrace, nodebug, current_predicate(loaded/1), \\+ current_predicate(second_init/0).");
  mosa_test::check(kept.kind == mosa::StopKind::Success,
                   "abort_init: clauses kept, no later init, got [" + stop_text(*machine, kept) + "]");
}

// パスは Prolog の引用の原子にして埋める。' と \ を含むパスでも読める（R-L23 と同じ形）。
std::string prolog_path(const std::filesystem::path& path) {
  std::string out = "'";
  for (char unit : mosa::utf8_from_path(path)) {
    if (unit == '\'') out += "''";
    else if (unit == '\\') out += "\\\\";
    else out.push_back(unit);
  }
  out.push_back('\'');
  return out;
}

// レビュー R-M06。portray/1 が行き先の流れを閉じた後の a でも、外の setup_call_cleanup/3 の後始末は 1 回動く
// （ADR 0028 の決定 A）。前は閉じた流れを existence_error で投げ、その枠を外したので後始末が消えた。
void check_abort_portray_closed() {
  const std::filesystem::path file =
      std::filesystem::path{MOSA_SOURCE_DIR} / "tests" / "tmp" / "debugger-abort-portray.txt";
  std::error_code error;
  std::filesystem::create_directories(file.parent_path(), error);
  auto machine = mosa::Machine::create();
  auto& impl = mosa::engine_impl(*machine);
  const std::string program = ":- dynamic(log/1).\n:- dynamic(s/1).\nt.\n"
                              "portray(x) :- s(S), close(S), t.\n"
                              "p :- open(" + prolog_path(file) + ", write, S), assertz(s(S)),\n"
                              "    setup_call_cleanup(true, print(S, x), assertz(log(outer))).\n";
  mosa_test::check(machine->load_program(program).ok, "abort_portray_closed: load the program");
  impl.message_lang = mosa::MessageLanguage::En;
  Scripted script{" Call: (5) t ? ", "a", {}, {}, false};
  give_script(*machine, script);
  const mosa::Stop stop = machine->query("trace, p.");
  mosa_test::check(script.used, "abort_portray_closed: the a was given, ports [" + script.text + "]");
  mosa_test::check(stop.kind == mosa::StopKind::Abort,
                   "abort_portray_closed: Abort, got [" + stop_text(*machine, stop) + "], ports [" + script.text + "]");
  const mosa::Stop logged = machine->query("nodebug, findall(X, log(X), L).");
  const std::string text = stop_text(*machine, logged);
  mosa_test::check(text.find("L = [outer]") != std::string::npos,
                   "abort_portray_closed: the outer cleanup ran once, got [" + text + "]");
  std::filesystem::remove(file, error);
}

}  // namespace

int main() {
  check_abort_stop();
  check_abort_cleanup();
  check_abort_nested_cleanup();
  check_abort_directive_cleanup();
  check_abort_cleanup_errors();
  check_abort_cleanup_keeps_mode();
  check_abort_cleanup_bindings();
  check_abort_cleanup_uncaught();
  check_abort_cleanup_heap_limit();
  check_abort_cleanup_heap_full();
  check_abort_pending_cleanup();
  check_abort_init();
  check_abort_portray_closed();
  return mosa_test::failures() == 0 ? 0 : 1;
}
