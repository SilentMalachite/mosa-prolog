// 同じファイルの再読みを、選択点の無い新しい機械で確かめる。
// modules.pl は consult の選択点を残すので、ここでは使わない。
// テスト: 節の置き換え、multifile の順序、失敗した再読みの復元、module/2 の拒否。

#include "harness.hpp"
#include "mosa/machine.hpp"
#include "mosa/writer.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

namespace fs = std::filesystem;

fs::path review_dir() { return fs::path{MOSA_SOURCE_DIR} / "tests" / "tmp" / "phase4_review"; }

bool write_text(const fs::path& file, std::string_view body) {
  std::error_code error;
  fs::create_directories(file.parent_path(), error);
  if (error) return false;
  std::ofstream out{file, std::ios::binary};
  out << body;
  return static_cast<bool>(out);
}

std::string join(const std::vector<std::string>& items) {
  std::string out;
  for (const std::string& item : items) {
    if (!out.empty()) out += ",";
    out += item;
  }
  return out;
}

std::string written(mosa::Machine& machine, mosa::Cell cell) {
  return mosa::write_term(machine.heap(), machine.operators(), machine.heap().deref(cell), mosa::WriteOptions{}, {});
}

struct Answers {
  mosa::StopKind kind = mosa::StopKind::Failure;
  std::vector<std::string> values;
};

Answers answers(mosa::Machine& machine, std::string_view goal) {
  Answers out;
  mosa::Stop stop = machine.query(goal);
  out.kind = stop.kind;
  while (stop.kind == mosa::StopKind::Success && !stop.bindings.empty()) {
    out.values.push_back(written(machine, stop.bindings[0].variable));
    if (!stop.more) break;
    stop = machine.next();
    out.kind = stop.kind;
  }
  return out;
}

void expect_values(mosa::Machine& machine, std::string_view goal, const std::vector<std::string>& want,
                   std::string_view label) {
  const Answers got = answers(machine, goal);
  const bool same = got.kind != mosa::StopKind::Exception && got.values == want;
  mosa_test::check(same, std::string{label} + " [" + join(got.values) + "]");
}

void expect_success(mosa::Machine& machine, std::string_view goal, std::string_view label) {
  const mosa::Stop stop = machine.query(goal);
  mosa_test::check(stop.kind == mosa::StopKind::Success, label);
}

void expect_failure(mosa::Machine& machine, std::string_view goal, std::string_view label) {
  const mosa::Stop stop = machine.query(goal);
  mosa_test::check(stop.kind == mosa::StopKind::Failure, label);
}

// 未定義は existence_error で、失敗ではない。成功しなければ取り込みは無い。
void expect_no_success(mosa::Machine& machine, std::string_view goal, std::string_view label) {
  const mosa::Stop stop = machine.query(goal);
  mosa_test::check(stop.kind != mosa::StopKind::Success, label);
}

bool consult_ok(mosa::Machine& machine, const fs::path& file, std::string_view label) {
  const bool ok = machine.consult(file.string());
  mosa_test::check(ok, std::string{label} + " " + std::string{machine.diagnostic()});
  return ok;
}

bool consult_denies(mosa::Machine& machine, const fs::path& file, std::string_view needle, std::string_view label) {
  const bool ok = machine.consult(file.string());
  const auto diagnostic = std::string{machine.diagnostic()};
  mosa_test::check(!ok && diagnostic.find(needle) != std::string::npos, std::string{label} + " " + diagnostic);
  return !ok;
}

void check_replace() {
  const fs::path file = review_dir() / "box.pl";
  mosa_test::check(write_text(file, ":- module(box, [item/1]).\nitem(old).\n"), "write box");
  auto machine = mosa::Machine::create();
  if (!consult_ok(*machine, file, "box first")) return;
  expect_values(*machine, "box:item(X).", {"old"}, "box old");
  mosa_test::check(write_text(file, ":- module(box, [item/1]).\nitem(new).\n"), "rewrite box");
  if (!consult_ok(*machine, file, "box reload")) return;
  expect_values(*machine, "box:item(X).", {"new"}, "box replaced");
  if (!consult_ok(*machine, file, "box again")) return;
  expect_values(*machine, "box:item(X).", {"new"}, "box not doubled");
}

void check_multifile_order() {
  const fs::path owner = review_dir() / "owner.pl";
  const fs::path extra = review_dir() / "extra.pl";
  const char* owner_v1 = ":- module(owner, [shared/1]).\n:- multifile(shared/1).\nshared(own).\n";
  const char* extra_v1 = ":- module(extra, []).\n:- multifile(owner:shared/1).\nowner:shared(extra).\n";
  mosa_test::check(write_text(owner, owner_v1) && write_text(extra, extra_v1), "write multifile");
  auto machine = mosa::Machine::create();
  if (!consult_ok(*machine, owner, "owner first") || !consult_ok(*machine, extra, "extra first")) return;
  expect_values(*machine, "owner:shared(X).", {"own", "extra"}, "shared both");
  const char* owner_v2 = ":- module(owner, [shared/1]).\n:- multifile(shared/1).\nshared(own2).\n";
  mosa_test::check(write_text(owner, owner_v2), "rewrite owner");
  if (!consult_ok(*machine, owner, "owner reload")) return;
  expect_values(*machine, "owner:shared(X).", {"extra", "own2"}, "owner appended");
  const char* extra_v2 = ":- module(extra, []).\n:- multifile(owner:shared/1).\nowner:shared(extra2).\n";
  mosa_test::check(write_text(extra, extra_v2), "rewrite extra");
  if (!consult_ok(*machine, extra, "extra reload")) return;
  expect_values(*machine, "owner:shared(X).", {"own2", "extra2"}, "extra appended");
}

void check_imports_again() {
  const fs::path file = review_dir() / "host.pl";
  mosa_test::check(write_text(file, ":- module(host, []).\nlocal(1).\n"), "write host");
  auto machine = mosa::Machine::create();
  if (!consult_ok(*machine, file, "host first")) return;
  expect_no_success(*machine, "host:member(a, [a]).", "host member absent");
  const char* again = ":- module(host, []).\n:- use_module(library(lists)).\nlocal(1).\n";
  mosa_test::check(write_text(file, again), "rewrite host");
  if (!consult_ok(*machine, file, "host reload")) return;
  expect_success(*machine, "host:member(a, [a]).", "host member imported");
}

void check_failed_restore() {
  const fs::path file = review_dir() / "keep.pl";
  const char* first =
      ":- module(keep, [item/1]).\n:- op(500, xfx, keepop).\n:- use_module(library(lists)).\nitem(old).\n";
  mosa_test::check(write_text(file, first), "write keep");
  auto machine = mosa::Machine::create();
  if (!consult_ok(*machine, file, "keep first")) return;
  const char* broken =
      ":- module(keep, [item/1, extra/1]).\n"
      ":- op(500, xfx, keepop).\n"
      ":- op(500, xfx, newop).\n"
      ":- use_module(library(apply)).\n"
      "item(new).\n"
      ":- phase4_bad_directive.\n";
  mosa_test::check(write_text(file, broken), "rewrite keep");
  if (!consult_denies(*machine, file, "この指示はフェーズ2では使えない", "keep rejected")) return;
  expect_values(*machine, "keep:item(X).", {"old"}, "keep clause restored");
  expect_success(*machine, "keep:current_op(500, xfx, keepop).", "keep op restored");
  expect_failure(*machine, "keep:current_op(500, xfx, newop).", "new op absent");
  expect_success(*machine, "keep:member(a, [a]).", "keep import restored");
  expect_no_success(*machine, "keep:maplist(true, []).", "apply import absent");
  expect_success(*machine, "current_module(apply).", "apply stays loaded");
  expect_success(*machine, "apply:maplist(true, []).", "apply clauses stay");
  expect_success(*machine, "(use_module(library(apply)), maplist(true, [])).", "apply imports again");
  const std::string imported = "use_module('" + file.string() + "').";
  if (machine->query(imported).kind != mosa::StopKind::Success) {
    mosa_test::check(false, "reimport keep");
    return;
  }
  expect_success(*machine, "current_predicate(item/1).", "item still exported");
  expect_failure(*machine, "current_predicate(extra/1).", "extra export absent");
}

void check_local_becomes_import() {
  const fs::path file = review_dir() / "swap_in.pl";
  mosa_test::check(write_text(file, ":- module(swap_in, []).\nmember(a, [a]).\n"), "write swap in");
  auto machine = mosa::Machine::create();
  if (!consult_ok(*machine, file, "swap in first")) return;
  expect_no_success(*machine, "swap_in:member(b, [a, b]).", "local member is not lists");
  mosa_test::check(write_text(file, ":- module(swap_in, []).\n:- use_module(library(lists)).\n"), "rewrite swap in");
  if (!consult_ok(*machine, file, "swap in reload")) return;
  expect_success(*machine, "swap_in:member(b, [a, b]).", "lists member imported");
}

void check_import_becomes_local() {
  const fs::path file = review_dir() / "swap_out.pl";
  const char* first = ":- module(swap_out, []).\n:- use_module(library(lists)).\n";
  mosa_test::check(write_text(file, first), "write swap out");
  auto machine = mosa::Machine::create();
  if (!consult_ok(*machine, file, "swap out first")) return;
  expect_success(*machine, "swap_out:member(b, [a, b]).", "lists member before");
  mosa_test::check(write_text(file, ":- module(swap_out, []).\nmember(zzz, []).\n"), "rewrite swap out");
  if (!consult_ok(*machine, file, "swap out reload")) return;
  expect_success(*machine, "swap_out:member(zzz, []).", "local member fact");
  expect_no_success(*machine, "swap_out:member(b, [a, b]).", "lists member dropped");
}

void check_import_restored() {
  const fs::path file = review_dir() / "swap_keep.pl";
  const char* first = ":- module(swap_keep, []).\n:- use_module(library(lists)).\n";
  mosa_test::check(write_text(file, first), "write swap keep");
  auto machine = mosa::Machine::create();
  if (!consult_ok(*machine, file, "swap keep first")) return;
  const char* broken = ":- module(swap_keep, []).\nmember(zzz, []).\n:- phase4_bad_directive.\n";
  mosa_test::check(write_text(file, broken), "rewrite swap keep");
  if (!consult_denies(*machine, file, "この指示はフェーズ2では使えない", "swap keep rejected")) return;
  expect_success(*machine, "swap_keep:member(b, [a, b]).", "import restored");
  expect_no_success(*machine, "swap_keep:member(zzz, []).", "local fact absent");
}

void check_failed_dependency() {
  const fs::path file = review_dir() / "need_apply.pl";
  const char* body = ":- module(need_apply, []).\n:- use_module(library(apply)).\n:- phase4_bad_directive.\n";
  mosa_test::check(write_text(file, body), "write need apply");
  auto machine = mosa::Machine::create();
  if (!consult_denies(*machine, file, "この指示はフェーズ2では使えない", "need apply rejected")) return;
  expect_failure(*machine, "current_module(need_apply).", "need apply gone");
  expect_success(*machine, "current_module(apply).", "first apply stays");
  expect_success(*machine, "apply:maplist(true, []).", "first apply clauses");
  expect_success(*machine, "(use_module(library(apply)), maplist(true, [])).", "first apply reimport");
}

void check_second_directive() {
  const fs::path file = review_dir() / "two_mod.pl";
  mosa_test::check(write_text(file, ":- module(once_name, []).\n:- module(again_name, []).\n"), "write two");
  auto machine = mosa::Machine::create();
  consult_denies(*machine, file, "permission_error(modify,module,again_name)", "second module");
  expect_failure(*machine, "current_module(once_name).", "once gone");
  expect_failure(*machine, "current_module(again_name).", "again gone");
}

class TempFile {
 public:
  TempFile(fs::path path, std::string_view body) : path_(std::move(path)) { ready_ = write_text(path_, body); }
  ~TempFile() { std::error_code error; fs::remove(path_, error); }
  explicit operator bool() const { return ready_; }

 private:
  fs::path path_;
  bool ready_ = false;
};

void check_bare_stem() {
  // 拡張子の無い原子は、そのパスの次に .pl を試す。cwd には stem 本体が無い。
  TempFile stem{fs::current_path() / "phase4_stem.pl",
                ":- module(phase4_stem, [stem_mark/1]).\nstem_mark(1).\n"};
  mosa_test::check(static_cast<bool>(stem), "write stem");
  auto machine = mosa::Machine::create();
  expect_success(*machine, "use_module(phase4_stem).", "bare stem");
  expect_success(*machine, "stem_mark(1).", "stem imported");
}

void check_beside_loader() {
  const fs::path dir = review_dir() / "nest";
  const fs::path parent = dir / "parent.pl";
  const fs::path kid = dir / "phase4_kid.pl";
  const bool wrote = write_text(parent, ":- module(phase4_parent, []).\n:- use_module(phase4_kid).\n") &&
                     write_text(kid, ":- module(phase4_kid, [kid/1]).\nkid(1).\n");
  mosa_test::check(wrote, "write nest");
  auto machine = mosa::Machine::create();
  if (!consult_ok(*machine, parent, "parent loads kid")) return;
  expect_success(*machine, "phase4_parent:kid(1).", "kid beside parent");
}

void check_loaded_beats_file() {
  const fs::path home = review_dir() / "beat_home.pl";
  mosa_test::check(write_text(home, ":- module(phase4_beat, [from_mod/1]).\nfrom_mod(1).\n"), "write beat");
  TempFile decoy{fs::current_path() / "phase4_beat.pl",
                 ":- module(phase4_beat_file, [from_file/1]).\nfrom_file(1).\n"};
  auto machine = mosa::Machine::create();
  if (!consult_ok(*machine, home, "beat module")) return;
  expect_success(*machine, "use_module(phase4_beat).", "loaded atom");
  expect_success(*machine, "from_mod(1).", "module wins");
  expect_no_success(*machine, "current_module(phase4_beat_file).", "file not loaded");
  expect_no_success(*machine, "from_file(1).", "file predicate absent");
}

void check_missing_file() {
  constexpr std::string_view path = "/no/such/mosa-phase4-missing.pl";
  auto used = mosa::Machine::create();
  const mosa::Stop stop = used->query("use_module('/no/such/mosa-phase4-missing.pl').");
  const auto used_diag = std::string{used->diagnostic()};
  mosa_test::check(stop.kind == mosa::StopKind::Failure && used_diag.find("ファイルを開けません") != std::string::npos,
                   "missing use " + used_diag);
  auto consulted = mosa::Machine::create();
  mosa_test::check(!consulted->consult(path), "missing consult");
  const auto consult_diag = std::string{consulted->diagnostic()};
  mosa_test::check(consult_diag.find("ファイルを開けません") != std::string::npos, "missing consult " + consult_diag);
  mosa_test::check(used_diag.find(path) != std::string::npos && consult_diag.find(path) != std::string::npos,
                   "missing path " + used_diag + " / " + consult_diag);
}

void check_other_file() {
  const fs::path first = review_dir() / "held.pl";
  const fs::path other = review_dir() / "held_other.pl";
  mosa_test::check(write_text(first, ":- module(held, [kept/1]).\nkept(1).\n"), "write held");
  mosa_test::check(write_text(other, ":- module(held, []).\nintruder(1).\n"), "write held other");
  auto machine = mosa::Machine::create();
  if (!consult_ok(*machine, first, "held first")) return;
  consult_denies(*machine, other, "permission_error(modify,module,held)", "other file");
  expect_success(*machine, "current_module(held).", "held stays");
  expect_success(*machine, "held:kept(1).", "held clause stays");
  expect_no_success(*machine, "held:intruder(1).", "intruder absent");
}

}  // namespace

int main() {
  check_replace();
  check_multifile_order();
  check_imports_again();
  check_local_becomes_import();
  check_import_becomes_local();
  check_import_restored();
  check_failed_dependency();
  check_failed_restore();
  check_second_directive();
  check_other_file();
  check_bare_stem();
  check_beside_loader();
  check_loaded_beats_file();
  check_missing_file();
  return mosa_test::failures();
}
