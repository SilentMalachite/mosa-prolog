// 7.4.2 と SPEC 第5節。ゴールの consult/1 も、外の読み込みが開いているファイルの積みを継ぐ（レビュー 2026-11-20 の
// C-01、ADR 0026 の「読み込み中のゴールの読み込みは入れ子の読み込み」）。前は空の積みから始めたので、指示のゴールが
// 自分を consult するファイルは読み込みが終わらず、C++ のスタックが尽きて落ちた（CLI で rc=139、既定のスタックでも）。
// 期待は、同じ形で無いファイルを consult したときの今の動きから取る。違うのは診断の句だけ。
// initialization/1 のゴールはテキストを閉じた後に動くので、循環にはしない。代わりに入れ子の読み込みを 64 段までにし、
// 自分の初期化のゴールで自分を consult するファイルも、落ちずに読み込みの失敗で終える（C-01 の持ち主の決定 B）。
// 一時ファイルは tests/tmp/consult_cycle。

#include "harness.hpp"
#include "mosa/machine.hpp"
#include "mosa/path_text.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>

namespace {

constexpr std::string_view kCycle = "ファイルが循環しています";
constexpr std::string_view kDeep = "読み込みの入れ子が深すぎます";

bool write_text(const std::filesystem::path& path, std::string_view text) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output << text;
  return static_cast<bool>(output);
}

std::string prolog_path(const std::filesystem::path& path) {
  std::string out = "'";
  // 原子は UTF-8 で書く。Windows の generic_string は ANSI コードページで、日本語の名前が化ける。
  for (char unit : mosa::utf8_from_path(path)) {
    if (unit == '\'') out += "''";
    else if (unit == '\\') out += "\\\\";
    else out.push_back(unit);
  }
  out.push_back('\'');
  return out;
}

class CerrRedirect {
 public:
  explicit CerrRedirect(std::ostringstream& sink) : saved_(std::cerr.rdbuf(sink.rdbuf())) {}
  ~CerrRedirect() { std::cerr.rdbuf(saved_); }
  CerrRedirect(const CerrRedirect&) = delete;
  CerrRedirect& operator=(const CerrRedirect&) = delete;

 private:
  std::streambuf* saved_;
};

struct Asked {
  mosa::StopKind kind = mosa::StopKind::Failure;
  std::string err;
};

Asked ask(mosa::Machine& machine, const std::string& goal) {
  std::ostringstream captured;
  Asked asked;
  {
    const CerrRedirect redirect(captured);
    asked.kind = machine.query(goal).kind;
  }
  asked.err = captured.str();
  return asked;
}

bool holds(mosa::Machine& machine, const std::string& goal) {
  return ask(machine, goal).kind == mosa::StopKind::Success;
}

bool has_cycle(const Asked& asked) { return asked.err.find(kCycle) != std::string::npos; }

std::string consult_of(const std::filesystem::path& file) { return "consult(" + prolog_path(file) + ")."; }

// 指示のゴールが自分を consult する。無いファイルの対と同じく、指示が失敗して consult も失敗する。
void check_self(const std::filesystem::path& dir) {
  const auto self = dir / "self.pl";
  mosa_test::check(write_text(self, ":- (true, consult(" + prolog_path(self) + ")).\np(1).\n"), "write self");
  auto machine = mosa::Machine::create();
  const Asked asked = ask(*machine, consult_of(self));
  mosa_test::check(asked.kind == mosa::StopKind::Failure, "self: consult fails");
  mosa_test::check(has_cycle(asked), "self: cycle phrase in [" + asked.err + "]");
  // 同じ機械で、関係の無いファイルは読めて、同じ循環はもう一度同じ診断になる。積みが残っていない。
  const auto leaf = dir / "leaf.pl";
  mosa_test::check(write_text(leaf, "l(1).\n"), "write leaf");
  const Asked after = ask(*machine, consult_of(leaf));
  mosa_test::check(after.kind == mosa::StopKind::Success && !has_cycle(after),
                   "self: leaf loads after [" + after.err + "]");
  mosa_test::check(holds(*machine, "l(1)."), "self: leaf clause");
  const Asked again = ask(*machine, consult_of(self));
  mosa_test::check(again.kind == mosa::StopKind::Failure && has_cycle(again), "self: same cycle again");
}

// :- if の条件で自分を consult する。条件は偽になり、if の中は読まず、後ろは読む。
void check_self_if(const std::filesystem::path& dir) {
  const auto self = dir / "selfif.pl";
  mosa_test::check(write_text(self, ":- if(consult(" + prolog_path(self) + ")).\nq(1).\n:- endif.\nr(1).\n"),
                   "write selfif");
  auto machine = mosa::Machine::create();
  const Asked asked = ask(*machine, consult_of(self));
  mosa_test::check(asked.kind == mosa::StopKind::Success, "selfif: consult succeeds [" + asked.err + "]");
  mosa_test::check(has_cycle(asked), "selfif: cycle phrase in [" + asked.err + "]");
  mosa_test::check(holds(*machine, "r(1)."), "selfif: after endif");
  mosa_test::check(!holds(*machine, "catch(q(1), _, fail)."), "selfif: inside if skipped");
}

// a が b を、b が a を、指示のゴールで consult する。
void check_mutual(const std::filesystem::path& dir) {
  const auto a = dir / "a.pl";
  const auto b = dir / "b.pl";
  mosa_test::check(write_text(a, ":- (true, consult(" + prolog_path(b) + ")).\na(1).\n"), "write a");
  mosa_test::check(write_text(b, ":- (true, consult(" + prolog_path(a) + ")).\nb(1).\n"), "write b");
  auto machine = mosa::Machine::create();
  const Asked asked = ask(*machine, consult_of(a));
  mosa_test::check(asked.kind == mosa::StopKind::Failure, "mutual: consult fails");
  mosa_test::check(has_cycle(asked), "mutual: cycle phrase in [" + asked.err + "]");
}

// 外のファイルが指示 [F] で開いた selfn.pl が、ゴールで自分を consult する。積みには外が積んだ selfn.pl がある。
void check_outer(const std::filesystem::path& dir) {
  const auto outer = dir / "outer.pl";
  const auto selfn = dir / "selfn.pl";
  mosa_test::check(write_text(outer, ":- [" + prolog_path(selfn) + "].\no(1).\n"), "write outer");
  mosa_test::check(write_text(selfn, ":- (true, consult(" + prolog_path(selfn) + ")).\nn(1).\n"), "write selfn");
  auto machine = mosa::Machine::create();
  const Asked asked = ask(*machine, consult_of(outer));
  mosa_test::check(asked.kind == mosa::StopKind::Failure, "outer: consult fails");
  mosa_test::check(has_cycle(asked), "outer: cycle phrase in [" + asked.err + "]");
}

// 守り。循環しないゴールの consult と、入れ子でなく続けて 2 回読むことは、前後とも通る。
void check_guards(const std::filesystem::path& dir) {
  const auto leaf = dir / "leaf2.pl";
  const auto main = dir / "main.pl";
  mosa_test::check(write_text(leaf, "l2(1).\n"), "write leaf2");
  const std::string load = ":- (true, consult(" + prolog_path(leaf) + ")).\n";
  mosa_test::check(write_text(main, load + load + "m(1).\n"), "write main");
  auto machine = mosa::Machine::create();
  const Asked asked = ask(*machine, consult_of(main));
  mosa_test::check(asked.kind == mosa::StopKind::Success && !has_cycle(asked),
                   "guard: twice loads [" + asked.err + "]");
  mosa_test::check(holds(*machine, "l2(1).") && holds(*machine, "m(1)."), "guard: both clauses");
  // 初期化のゴールが外の状態で守って 1 回だけ自分を読み直す。テキストを閉じた後なので、今は循環にしない。
  const auto again = dir / "g.pl";
  mosa_test::check(write_text(again, ":- initialization((catch(seen, _, fail) -> true ; assertz(seen), consult(" +
                                         prolog_path(again) + "))).\ng(1).\n"),
                   "write g");
  const Asked init = ask(*machine, consult_of(again));
  mosa_test::check(init.kind == mosa::StopKind::Success && !has_cycle(init), "guard: init reload [" + init.err + "]");
  mosa_test::check(holds(*machine, "g(1)."), "guard: init reload clause");
}

// 入れ子の読み込みが構文の誤りで終わった後も、積みにそのファイルは残らない。直接読むと、元の誤りになる。
void check_after_error(const std::filesystem::path& dir) {
  const auto broken = dir / "broken.pl";
  const auto caller = dir / "caller.pl";
  mosa_test::check(write_text(broken, "x(.\n"), "write broken");
  mosa_test::check(write_text(caller, ":- (true, consult(" + prolog_path(broken) + ")).\nk(1).\n"), "write caller");
  auto machine = mosa::Machine::create();
  const Asked nested = ask(*machine, consult_of(caller));
  mosa_test::check(!has_cycle(nested), "error: nested gives no cycle [" + nested.err + "]");
  const Asked direct = ask(*machine, consult_of(broken));
  mosa_test::check(direct.kind == mosa::StopKind::Failure && !has_cycle(direct), "error: direct is the syntax error");
  mosa_test::check(direct.err.find("broken.pl") != std::string::npos,
                   "error: direct names the file [" + direct.err + "]");
}

bool has_deep(const Asked& asked) { return asked.err.find(kDeep) != std::string::npos; }

// 自分の初期化のゴールで自分を consult する。前は落ちた。64 段目の読み込みが失敗し、各段の初期化のゴールが失敗する。
// 初期化のゴールの失敗は警告だけなので、無いファイルを consult する対と同じく、一番外の consult は成功する。
void check_self_init(const std::filesystem::path& dir) {
  const auto self = dir / "selfi.pl";
  const auto missing = dir / "missing_init.pl";
  mosa_test::check(write_text(self, ":- initialization(consult(" + prolog_path(self) + ")).\ni(1).\n"), "write selfi");
  mosa_test::check(write_text(missing, ":- initialization(consult(" + prolog_path(dir / "none.pl") + ")).\nj(1).\n"),
                   "write missing_init");
  auto machine = mosa::Machine::create();
  const Asked pair = ask(*machine, consult_of(missing));
  const Asked asked = ask(*machine, consult_of(self));
  mosa_test::check(asked.kind == pair.kind, "selfi: same outcome as a missing file");
  mosa_test::check(has_deep(asked) && !has_cycle(asked), "selfi: depth phrase");
  mosa_test::check(holds(*machine, "i(1)."), "selfi: clause kept");
}

// 別々のファイルの鎖の 1 段。最後の段は葉の述語を定義する。指示の use_module/1 と reexport/1 の鎖はモジュールの
// ファイルにし、葉のモジュールだけが葉を公開する。
std::string chain_text(const std::string& stem, int index, int files, std::string_view form, bool directive,
                       const std::filesystem::path& next) {
  const std::string leaf = "leaf" + std::to_string(files);
  const bool modules = directive && (form == "use_module" || form == "reexport");
  const std::string head = modules ? ":- module(" + stem + std::to_string(index) + ", [" +
                                         (index == files ? leaf + "/1" : std::string{}) + "]).\n"
                                   : std::string{};
  if (index == files) return head + leaf + "(1).\n";
  const std::string call = std::string{form} + "(" + prolog_path(next) + ")";
  return head + (directive ? ":- " + call + ".\n" : ":- (true, " + call + ").\n");
}

// 別々のファイルの鎖。64 段までは読め、65 段目で失敗する。数えるのは同時に開いているファイルの読み込みで、ゴールの
// consult/1、use_module/1,2、ensure_loaded/1、指示の [F]、それらを呼ぶ初期化のゴール、指示の include/1、
// ensure_loaded/1、use_module/1,2、reexport/1,2（iso-notes 7.1）。指示の 3 つは今の読み込みの中で読むので、前は数えず、
// 1 MiB のスタックで 437 段まで重ねると落ちた（C-01 の 2 つ目の持ち主の決定 B）。mixed は指示の ensure_loaded/1 と
// ゴールの consult/1 を交互に重ね、両方を 1 つの数で数えることを見る。
void check_chain(const std::filesystem::path& dir, int files, bool loads, std::string_view form = "consult",
                 bool directive = false) {
  const std::string stem = std::string{form} + (directive ? "d" : "") + std::to_string(files) + "_";
  const bool mixed = form == "mixed";
  for (int index = 1; index <= files; ++index) {
    const auto next = dir / (stem + std::to_string(index + 1) + ".pl");
    const std::string_view step = mixed ? (index % 2 == 1 ? "ensure_loaded" : "consult") : form;
    const std::string text = chain_text(stem, index, files, step, directive || (mixed && index % 2 == 1), next);
    mosa_test::check(write_text(dir / (stem + std::to_string(index) + ".pl"), text), "write " + stem);
  }
  auto machine = mosa::Machine::create();
  const Asked asked = ask(*machine, consult_of(dir / (stem + "1.pl")));
  const std::string label = "chain " + stem + ": ";
  mosa_test::check((asked.kind == mosa::StopKind::Success) == loads,
                   label + "outcome [" + asked.err.substr(0, 200) + "]");
  mosa_test::check(has_deep(asked) != loads, label + "depth phrase");
  const std::string leaf = "leaf" + std::to_string(files) + "(1)";
  const bool modules = directive && (form == "use_module" || form == "reexport");
  const std::string called = modules ? stem + std::to_string(files) + ":" + leaf : leaf;
  mosa_test::check(holds(*machine, "catch(" + called + ", _, fail).") == loads, label + "leaf");
  // 上限はファイルを探す前に止めるので、ゴールの ensure_loaded/1 はパスを付けない。前は「: 」を前に付けた。
  if (!loads && form == "ensure_loaded" && !directive) {
    const bool bare = asked.err.rfind(kDeep, 0) == 0 || asked.err.find("\n" + std::string{kDeep}) != std::string::npos;
    mosa_test::check(bare, label + "no empty path [" + asked.err.substr(0, 200) + "]");
  }
  // 指示で止まった鎖は根の consult ごと失敗するので、診断の前には根のファイルのパスが付く。空のパスは付けない。
  if (!loads && directive) {
    mosa_test::check(asked.err.find(stem + "1.pl: " + std::string{kDeep}) != std::string::npos &&
                         asked.err.find("\n: ") == std::string::npos && asked.err.rfind(": ", 0) != 0,
                     label + "root path before the phrase [" + asked.err.substr(0, 300) + "]");
  }
}

}  // namespace

int main() {
  const auto dir = std::filesystem::path{MOSA_SOURCE_DIR} / "tests" / "tmp" / "consult_cycle";
  std::error_code error;
  std::filesystem::remove_all(dir, error);
  std::filesystem::create_directories(dir, error);
  mosa_test::check(!error, "make tmp dir");
  // 守りを先に流す。直す前は、循環の件で実行ファイルごと落ちる。
  check_guards(dir);
  check_after_error(dir);
  check_self(dir);
  check_self_if(dir);
  check_mutual(dir);
  check_outer(dir);
  check_chain(dir, 64, true);
  check_chain(dir, 65, false);
  check_chain(dir, 65, false, "ensure_loaded");
  check_chain(dir, 64, true, "include", true);
  check_chain(dir, 65, false, "include", true);
  check_chain(dir, 65, false, "ensure_loaded", true);
  check_chain(dir, 64, true, "use_module", true);
  check_chain(dir, 65, false, "use_module", true);
  check_chain(dir, 64, true, "reexport", true);
  check_chain(dir, 65, false, "reexport", true);
  check_chain(dir, 64, true, "mixed");
  check_chain(dir, 65, false, "mixed");
  check_self_init(dir);
  std::filesystem::remove_all(dir, error);
  return mosa_test::failures();
}
