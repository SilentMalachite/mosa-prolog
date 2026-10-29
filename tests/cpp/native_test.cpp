// ADR 0009 の差し替え。手書きの生成関数（native_units.cpp）を install_unit で登録した機械と、
// 登録しない機械（load_program だけ）で同じ問い合わせが同じ答えを返すことを確かめる。
// 生成関数が走ったかは Hits で別に数える（答えが同じでも、外れていれば数が増えない）。
// Review Focus 2 は native_reconsult_replaces と native_assert_static、3 は fingerprint_mismatch_falls_back、
// 5（同梱ライブラリの一覧。計画 Task 10）は check_bundled_library。一時ファイルは tests/tmp/native1。

#include "harness.hpp"
#include "native_units.hpp"

#include "machine/internal.hpp"
#include "mosa/path_text.hpp"
#include "mosa/writer.hpp"

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace {

using mosa::Machine;
using mosa::rt::NativeEntry;
using mosa_test::check;
using native_units::hits;

// 翻訳器の見え方の原文から指紋を出す。Task 4 の翻訳器も同じ clause_fingerprint を呼ぶ。
std::uint64_t fingerprint_of(std::string_view view, std::string_view name, std::uint16_t arity) {
  auto scratch = Machine::create();
  if (!scratch->load_program(view).ok) return 0;
  auto& impl = mosa::engine_impl(*scratch);
  const auto* predicate = mosa::find_pred(impl, mosa::PredKey{impl.user_module, impl.atoms.intern(name), arity});
  return predicate == nullptr ? 0 : mosa::clause_fingerprint(impl.atoms, *predicate);
}

NativeEntry entry_of(std::string_view view, std::string_view name, std::uint16_t arity, mosa::rt::NativeProc proc) {
  return NativeEntry{"user", name, arity, fingerprint_of(view, name, arity), proc};
}

// 添字は native_units.cpp の kCutcEntry などと同じ順。
std::vector<NativeEntry> list_entries() {
  const auto view = native_units::kListSource;
  return {entry_of(view, "app", 3, &native_units::app_3), entry_of(view, "nrev", 2, &native_units::nrev_2),
          entry_of(view, "chk", 1, &native_units::chk_1), entry_of(view, "cutc", 1, &native_units::cutc_1)};
}

std::unique_ptr<Machine> native_list_machine() {
  static const std::vector<NativeEntry> entries = list_entries();
  auto machine = Machine::create();
  const mosa::rt::NativeUnit unit{"native_list", native_units::kListSource, &native_units::list_tag(), entries,
                                  native_units::list_atoms(), native_units::list_calls()};
  check(mosa::rt::install_unit(*machine, unit), "install_unit list");
  return machine;
}

std::unique_ptr<Machine> interp_machine(std::string_view source) {
  auto machine = Machine::create();
  check(machine->load_program(source).ok, "load_program");
  return machine;
}

// 全解を「;」でつなぐ。最後は残りが無ければ「.」、尽きて失敗なら「false」、例外なら throw(Ball)。
std::string answers(Machine& machine, std::string_view goal) {
  std::string out;
  auto stop = machine.query(goal);
  for (int count = 0; count < 20; ++count) {
    if (stop.kind == mosa::StopKind::Exception) {
      return out + "throw(" + mosa::write_term(machine.heap(), machine.operators(), stop.exception, {}, {}) + ")";
    }
    if (stop.kind != mosa::StopKind::Success) return out + "false";
    out += mosa::format_answer(machine, stop);
    if (!stop.more) return out + ".";
    out += " ; ";
    stop = machine.next();
  }
  return out + "...";
}

// 同じ問い合わせを両方の機械で流し、答えが expected と一致すること。
void same_answers(Machine& native, Machine& interp, std::string_view goal, std::string_view expected) {
  const auto got = answers(native, goal);
  const auto want = answers(interp, goal);
  check(got == want, std::string{"native == interp: "} + std::string{goal} + " => " + got + " / " + want);
  check(want == expected, std::string{"interp: "} + std::string{goal} + " => " + want);
}

void check_app_solutions(Machine& native, Machine& interp) {
  const auto before = hits().app;
  same_answers(native, interp, "app(X, Y, [1,2]).",
               "X = [], Y = [1,2] ; X = [1], Y = [2] ; X = [1,2], Y = [] ; false");
  check(hits().app > before, "app/3 ran natively");
  same_answers(native, interp, "app([1], [2], L).", "L = [1,2].");
  same_answers(native, interp, "once(app(X, _, [1,2])).", "X = [].");
  same_answers(native, interp, "findall(X-Y, app(X, Y, [a]), L).", "X = X, Y = Y, L = [[]-[a],[a]-[]].");
  same_answers(native, interp, "app(X, [c], [a,b,c]).", "X = [a,b] ; false");
  same_answers(native, interp, "app([a|T], Y, [b]).", "false");
}

// 節の変数の向き。X と Y が両方未束縛のとき、節の側（X）が Y を指す。W は X と Y の間に作った変数。
void check_bind_direction(Machine& native, Machine& interp) {
  same_answers(native, interp, "g(X, W, Y) = g(X, W, Y), app([X], [], [Y]), (X @< W -> O = lt ; O = gt).",
               "X = Y, W = W, O = gt.");
  same_answers(native, interp, "app(X, Y, Z), X = [E], !, (Y @< E -> O = lt ; O = gt).",
               "X = [E], Y = _G1, Z = [E|_G1], O = gt.");
}

void check_nrev_and_cut(Machine& native, Machine& interp) {
  const auto before = hits().nrev;
  same_answers(native, interp, "nrev([1,2,3], R).", "R = [3,2,1].");
  same_answers(native, interp, "once(nrev(P, [a,b,c])).", "P = [c,b,a].");
  check(hits().nrev > before, "nrev/2 ran natively");
  const auto cut_before = hits().cutc;
  same_answers(native, interp, "findall(X, cutc(X), L).", "X = X, L = [first,second].");
  same_answers(native, interp, "cutc(first).", "true ; false");
  same_answers(native, interp, "cutc(third).", "false");
  same_answers(native, interp, "cutc(second).", "true.");
  same_answers(native, interp, "findall(X, (cutc(X), X \\== first), L).", "X = X, L = [second].");
  check(hits().cutc > cut_before, "cutc/1 ran natively");
}

void check_body_exception(Machine& native, Machine& interp) {
  const auto before = hits().chk;
  same_answers(native, interp, "catch(chk(X), error(E, _), true).", "X = X, E = type_error(atom,[a]).");
  same_answers(native, interp, "catch((chk(X), fail ; X = none), error(E, _), true).",
               "X = X, E = type_error(atom,[a]).");
  same_answers(native, interp, "catch(chk(_), error(_, _), true), app(X, [b], [a, b]).", "X = [a] ; false");
  // 捕まえない例外は問い合わせの外まで届く。Context は比べず、Formal だけを見る。
  const auto uncaught = answers(native, "chk(X).");
  check(uncaught == answers(interp, "chk(X)."), "uncaught: native == interp");
  check(uncaught.starts_with("throw(error(type_error(atom,[a]),"), "uncaught formal: " + uncaught);
  check(hits().chk > before, "chk/1 ran natively");
}

void check_assert_static(Machine& native, Machine& interp) {
  same_answers(native, interp, "catch(assertz(app(x, y, z)), error(E, _), true).",
               "E = permission_error(modify,static_procedure,app/3).");
  same_answers(native, interp, "catch(retract(app(_, _, _)), error(E, _), true).",
               "E = permission_error(modify,static_procedure,app/3).");
  same_answers(native, interp, "catch(abolish(app/3), error(E, _), true).",
               "E = permission_error(modify,static_procedure,app/3).");
  same_answers(native, interp, "predicate_property(app(_, _, _), static).", "true ; false");
  same_answers(native, interp, "predicate_property(app(_, _, _), dynamic).", "false");
  same_answers(native, interp, "predicate_property(app(_, _, _), number_of_clauses(N)).", "N = 2.");
  const auto before = hits().app;
  same_answers(native, interp, "app([1], [2], L).", "L = [1,2].");
  check(hits().app > before, "native_assert_static: app/3 stays native after the refused writes");
}

bool write_text(const std::filesystem::path& path, std::string_view text) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output << text;
  return static_cast<bool>(output);
}

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

// 別のファイルの consult/1 が app/3 を定義し直す。生成関数は外れ、nrev/2 の呼び先の表からも消える。
void check_reconsult_replaces(const std::filesystem::path& dir) {
  const auto file = dir / "app2.pl";
  check(write_text(file, "app(_, _, replaced).\n"), "write app2.pl");
  auto native = native_list_machine();
  auto interp = interp_machine(native_units::kListSource);
  const auto consult = "consult(" + prolog_path(file) + ").";
  same_answers(*native, *interp, consult, "true.");
  const auto app_before = hits().app;
  const auto nrev_before = hits().nrev;
  same_answers(*native, *interp, "app(a, b, X).", "X = replaced.");
  same_answers(*native, *interp, "nrev([1,2], R).", "R = replaced.");
  same_answers(*native, *interp, "predicate_property(app(_, _, _), number_of_clauses(N)).", "N = 1.");
  same_answers(*native, *interp, "predicate_property(app(_, _, _), static).", "true ; false");
  check(hits().app == app_before, "native_reconsult_replaces: app/3 detached");
  check(hits().nrev > nrev_before, "native_reconsult_replaces: nrev/2 still native");
}

// multifile の追加も節を書く。付けたあとに足した節は、生成関数が知らない節である。
void check_multifile_detaches() {
  constexpr std::string_view kMore = ":- multifile(app/3).\napp(extra, x, y).\n";
  auto native = native_list_machine();
  auto interp = interp_machine(native_units::kListSource);
  check(native->load_program(kMore).ok, "multifile load (native)");
  check(interp->load_program(kMore).ok, "multifile load (interp)");
  const auto before = hits().app;
  same_answers(*native, *interp, "predicate_property(app(_, _, _), number_of_clauses(N)).", "N = 3.");
  same_answers(*native, *interp, "findall(A-B, app(extra, A, B), L).", "A = A, B = B, L = [x-y].");
  check(hits().app == before, "multifile addition detaches app/3");
}

// 読み込みが失敗して述語を写しへ戻しても、その述語は付け直さない（ADR 0009）。答えは元の節のまま。
void check_rollback_detaches() {
  constexpr std::string_view kBroken = "app(_, _, gone).\nbroken(.\n";
  auto native = native_list_machine();
  auto interp = interp_machine(native_units::kListSource);
  check(!native->load_program(kBroken).ok && !interp->load_program(kBroken).ok, "broken load fails");
  const auto before = hits().app;
  same_answers(*native, *interp, "app([1], [2], L).", "L = [1,2].");
  check(hits().app == before, "rolled-back load detaches app/3");
}

// 指示のゴールの中の op/3 が rel/1 の IR を変える。rel/1 はインタプリタで動き、same/1 だけが付く。
void check_fingerprint_mismatch() {
  const auto view = native_units::kOpTranslatedView;
  const std::vector<NativeEntry> entries{entry_of(view, "rel", 1, &native_units::rel_1),
                                         entry_of(view, "same", 1, &native_units::same_1)};
  check(entries[0].fingerprint != 0 && entries[1].fingerprint != 0, "fingerprints computed");
  check(entries[0].fingerprint != fingerprint_of(native_units::kOpSource, "rel", 1), "rel/1 IR changes at load");
  check(entries[1].fingerprint == fingerprint_of(native_units::kOpSource, "same", 1), "same/1 IR is unchanged");
  auto native = Machine::create();
  const mosa::rt::NativeUnit unit{"native_op", native_units::kOpSource, &native_units::op_tag(), entries,
                                  native_units::op_atoms(), {}};
  check(mosa::rt::install_unit(*native, unit), "install_unit op");
  auto interp = interp_machine(native_units::kOpSource);
  const auto rel_before = hits().rel;
  const auto same_before = hits().same;
  same_answers(*native, *interp, "rel(X), X = (L ===> R).", "X = a===>b-c, L = a, R = b-c.");
  same_answers(*native, *interp, "same(X).", "X = f(a,2.5,-3).");
  same_answers(*native, *interp, "same(f(A, B, C)).", "A = a, B = 2.5, C = -3.");
  same_answers(*native, *interp, "same(f(a, 2.5, -4)).", "false");
  same_answers(*native, *interp, "same(f(b, _, _)).", "false");
  check(hits().rel == rel_before, "fingerprint_mismatch_falls_back: rel/1 not attached");
  check(hits().same > same_before, "fingerprint_mismatch_falls_back: same/1 attached");
}

// meta_predicate を宣言した述語も、指紋が合えば付く（計画 Task 7）。限定は生成関数が rt::qualify で行い、
// 呼び手の文脈モジュールが M: に出る。もう M: が付いた引数は包み直さない。
void check_meta_attached() {
  const auto source = native_units::kMetaSource;
  const std::vector<NativeEntry> entries{entry_of(source, "twice", 1, &native_units::twice_1),
                                         entry_of(source, "keep", 2, &native_units::keep_2)};
  check(entries[0].fingerprint != 0, "twice/1 fingerprint computed");
  auto native = Machine::create();
  const mosa::rt::NativeUnit unit{"native_meta", source, &native_units::meta_tag(), entries, {},
                                  native_units::meta_calls()};
  check(mosa::rt::install_unit(*native, unit), "install_unit meta");
  auto interp = interp_machine(source);
  const auto before = hits().twice;
  const auto kept = hits().keep;
  same_answers(*native, *interp, "findall(x, twice(true), L).", "L = [x].");
  same_answers(*native, *interp, "twice(fail).", "false");
  same_answers(*native, *interp, "findall(X, twice((X = a ; X = b)), L).", "X = X, L = [a,b].");
  same_answers(*native, *interp, "predicate_property(twice(_), meta_predicate(M)).", "M = twice(0) ; false");
  same_answers(*native, *interp, "keep(foo, Q).", "Q = user:foo.");
  same_answers(*native, *interp, "keep(m:foo, Q).", "Q = m:foo.");
  check(hits().twice > before, "meta_predicate twice/1 attached");
  check(hits().keep > kept, "meta_predicate keep/2 attached");
}

}  // namespace

// 計画 Task 5。ファイルの無いパスで embed した原文を install_unit で読む。include/1 と use_module/1 は
// ファイルシステムより先に埋め込みを引く。後の単位（m）は根の use_module が読み終えているので付けるだけになる。
constinit mosa::rt::UnitTag kEmbedRoot{};
constinit mosa::rt::UnitTag kEmbedModule{};

void check_embedded_first() {
  const auto dir = std::filesystem::path{MOSA_SOURCE_DIR} / "tests" / "tmp" / "native_embed_absent";
  std::filesystem::remove_all(dir);
  const std::string root = mosa::utf8_from_path(dir / "app.pl");
  const std::string module = mosa::utf8_from_path(dir / "m.pl");
  const std::string root_text = ":- include(inc).\n:- use_module(m).\nboth(X, Y) :- inc_fact(X), twice(X, Y).\n";
  const std::string module_text = ":- module(m, [twice/2]).\ntwice(X, Y) :- Y is X * 2.\n";
  auto machine = Machine::create();
  mosa::rt::embed(*machine, root, root_text);
  mosa::rt::embed(*machine, mosa::utf8_from_path(dir / "inc.pl"), "inc_fact(21).\n");
  mosa::rt::embed(*machine, module, module_text);
  const mosa::rt::NativeUnit root_unit{root, root_text, &kEmbedRoot, {}, {}, {}};
  const mosa::rt::NativeUnit module_unit{module, module_text, &kEmbedModule, {}, {}, {}};
  check(mosa::rt::install_unit(*machine, root_unit), "embedded root loads: " + std::string{machine->diagnostic()});
  check(mosa::rt::install_unit(*machine, module_unit), "embedded module attaches");
  check(!std::filesystem::exists(dir), "embedded sources are not on disk");
  check(machine->query("inc_fact(21).").kind == mosa::StopKind::Success, "included predicate");
  check(machine->query("twice(3, 6).").kind == mosa::StopKind::Success, "used predicate");
  check(machine->query("both(21, 42).").kind == mosa::StopKind::Success, "root predicate");
  check(machine->query("findall(c, predicate_property(m:twice(_, _), number_of_clauses(1)), [c]).").kind ==
            mosa::StopKind::Success,
        "module read once");
}

// 計画 Task 10。実行ファイルに同梱したライブラリは use_module(library(Name)) が SPEC 第5節の探索より先に引く。
// 一覧はプロセスで 1 つなので、後から作ったマシンも同じ物を読む。単位は原文を読み終えてから指紋の合う述語に付く。
// bx/1 の proc は付いたことだけを見るので呼ばない（same_1 は別の述語の生成関数で、番地が要るだけ）。
constinit mosa::rt::UnitTag kBundledTag{};

void check_bundled_library() {
  const std::string_view source = ":- module(bundled_x, [bx/1, by/1]).\nbx(1).\nby(2).\n";
  auto scratch = Machine::create();
  check(scratch->load_program(source).ok, "bundled source loads as text");
  auto& scratch_impl = mosa::engine_impl(*scratch);
  const auto module = scratch_impl.atoms.intern("bundled_x");
  const auto* bx = mosa::find_pred(scratch_impl, mosa::PredKey{module, scratch_impl.atoms.intern("bx"), 1});
  check(bx != nullptr, "bx/1 in bundled_x");
  const std::vector<NativeEntry> entries{
      NativeEntry{"bundled_x", "bx", 1, bx == nullptr ? 0 : mosa::clause_fingerprint(scratch_impl.atoms, *bx),
                  &native_units::same_1},
      NativeEntry{"bundled_x", "by", 1, 0x1234, &native_units::same_1}};
  mosa::rt::bundle_library("bundled_x", mosa::rt::NativeUnit{"library(bundled_x)", source, &kBundledTag, entries, {}, {}});
  mosa::rt::bundle_library("bundled_y", ":- module(bundled_y, [yy/0]).\nyy.\n");
  auto machine = Machine::create();
  check(machine->load_program(":- use_module(library(bundled_x)).\n:- use_module(library('bundled_y.pl')).\n").ok,
        "use_module finds the bundled libraries before any file: " + std::string{machine->diagnostic()});
  check(machine->query("by(2).").kind == mosa::StopKind::Success, "the interpreted clause of a bundled unit runs");
  check(machine->query("yy.").kind == mosa::StopKind::Success, "a bundled source without a unit is consulted");
  auto& impl = mosa::engine_impl(*machine);
  const auto* attached = mosa::find_pred(impl, mosa::PredKey{impl.atoms.intern("bundled_x"), impl.atoms.intern("bx"), 1});
  const auto* skipped = mosa::find_pred(impl, mosa::PredKey{impl.atoms.intern("bundled_x"), impl.atoms.intern("by"), 1});
  check(attached != nullptr && attached->native == &native_units::same_1, "the bundled unit attaches by fingerprint");
  check(skipped != nullptr && skipped->native == nullptr, "a mismatched fingerprint stays interpreted");
  check(!std::filesystem::exists(mosa::bundled_library_path("bundled_x")), "no library/ file is written");
  auto second = Machine::create();
  check(second->load_program(":- use_module(library(bundled_y)).").ok && second->query("yy.").kind == mosa::StopKind::Success,
        "a later machine sees the same bundled list");
}

int main() {
  auto native = native_list_machine();
  auto interp = interp_machine(native_units::kListSource);
  check_app_solutions(*native, *interp);
  check_bind_direction(*native, *interp);
  check_nrev_and_cut(*native, *interp);
  check_body_exception(*native, *interp);
  check_assert_static(*native, *interp);
  const auto dir = std::filesystem::path{MOSA_SOURCE_DIR} / "tests" / "tmp" / "native1";
  std::filesystem::create_directories(dir);
  check_reconsult_replaces(dir);
  check_multifile_detaches();
  check_rollback_detaches();
  check_fingerprint_mismatch();
  check_meta_attached();
  check_embedded_first();
  check_bundled_library();
  return mosa_test::failures() == 0 ? 0 : 1;
}
