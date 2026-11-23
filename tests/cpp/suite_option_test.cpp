// test/3 の選択肢 setup(Goal) と cleanup(Goal) は、lgtunit のとおり試験ごとに動く（レビュー 2026-11-20 の M-50）。
// lgtunit.lgt（スイートと同じ 4decb64b）の run_test/3 は condition、setup、本体、cleanup の順で、setup が失敗するか
// 投げると試験は不合格で本体も cleanup も動かず、cleanup は本体の成否によらず動き、その失敗は結果を変えない。
// 選択肢のゴールは本体と変数を共有する。前は condition/1 しか読まず、setup と cleanup を黙って捨てた。
// 1 件ごとに 1 ファイルのスイートを tests/tmp/suite-option に書いて run_iso_suite で流し、分母と合格数と不合格の行を見る。

#include "harness.hpp"
#include "mosa/suite.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

bool write_text(const std::filesystem::path& path, std::string_view text) {
  std::error_code error;
  std::filesystem::create_directories(path.parent_path(), error);
  if (error) return false;
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output.write(text.data(), static_cast<std::streamsize>(text.size()));
  return static_cast<bool>(output);
}

std::string read_text(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

std::string failed_lines(std::string_view report) {
  const auto begin = report.find("不合格:\n");
  if (begin == std::string_view::npos) return {};
  const auto start = begin + std::string_view{"不合格:\n"}.size();
  const auto stop = report.find("別欄:", start);
  return std::string{report.substr(start, stop == std::string_view::npos ? std::string_view::npos : stop - start)};
}

struct Fixture {
  std::filesystem::path path;
  explicit Fixture(std::filesystem::path root) : path(std::move(root)) {
    std::error_code error;
    std::filesystem::remove_all(path, error);
  }
  Fixture(const Fixture&) = delete;
  Fixture& operator=(const Fixture&) = delete;
  ~Fixture() {
    std::error_code error;
    std::filesystem::remove_all(path, error);
  }
};

struct Suite {
  std::string_view name;
  std::string_view clauses;
  int denominator;
  int passed;
  std::vector<std::string_view> failed;
};

void check_suite(const std::filesystem::path& root, const Suite& item) {
  const std::string name{item.name};
  const auto suite = root / name;
  const std::string text = ":- object(tests, extends(lgtunit)).\n" + std::string{item.clauses} + "\n:- end_object.\n";
  mosa_test::check(write_text(suite / "logtalk" / "prolog" / "predicates" / "atom_1" / "tests.lgt", text),
                   name + ": write the suite");
  const auto report = root / (name + "-report.md");
  const int code = mosa::run_iso_suite(suite.string(), report.string());
  const std::string body = read_text(report);
  const std::string shown = " in\n" + body;
  mosa_test::check(code == (item.failed.empty() ? 0 : 1), name + ": exit code" + shown);
  mosa_test::check(body.find("\n分母: " + std::to_string(item.denominator) + "\n") != std::string::npos,
                   name + ": denominator " + std::to_string(item.denominator) + shown);
  mosa_test::check(body.find("\n合格数: " + std::to_string(item.passed) + "\n") != std::string::npos,
                   name + ": passed " + std::to_string(item.passed) + shown);
  const std::string failed = failed_lines(body);
  for (const std::string_view line : item.failed) {
    const std::string full = "predicates/atom_1/tests.lgt " + std::string{line} + "\n";
    mosa_test::check(failed.find(full) != std::string::npos, name + ": " + full + shown);
  }
}

// setup は本体の前に動き、その束縛は本体と期待に届く。失敗か例外なら不合格で、本体は動かない。
void check_setup(const std::filesystem::path& root) {
  check_suite(root, {"setup_runs", "\t:- dynamic(flag_set/0).\n\ttest(s1, true, [setup(assertz(flag_set))]) :- flag_set.\n",
                     1, 1, {}});
  check_suite(root, {"setup_binds", "\ttest(s2, false, [setup(X = 1)]) :- X == 1.\n", 1, 0, {"s2 false true"}});
  check_suite(root, {"setup_fails",
                     "\t:- dynamic(ran/0).\n\ttest(s3, false, [setup(fail)]) :- assertz(ran).\n"
                     "\ttest(s4, true, [setup(throw(x))]) :- true.\n\ttest(s5, false) :- ran.\n",
                     3, 1, {"s3 false setup", "s4 true setup"}});
}

// setup の後も未束縛の変数は、本体と期待で同じ変数のまま（レビュー R-M17）。前は _ と写して (_) を貼ったので、
// 本体の S = 1 が別の変数を束縛した。setup で別名にした 2 つの変数は、写しでも同じ変数を指す。
void check_unbound_setup(const std::filesystem::path& root) {
  check_suite(root, {"setup_unbound",
                     "\ttest(sv1, true(S == 1), [setup(var(S))]) :- S = 1.\n"
                     "\ttest(sv2, true, [setup(var(S))]) :- S = 1, S == 1.\n"
                     "\ttest(sv3, true, [setup(var(S))]) :- assertion(true), S = 1, S == 1.\n"
                     "\ttest(sv4, true(S == T), [setup(S = T)]) :- true.\n"
                     "\ttest(sv5, true, [setup(S = T)]) :- S = 1, T == 1.\n",
                     5, 5, {}});
}

// setup と cleanup も本体と同じく user:: を外して読む。exists の cleanup は、成り立った解の束縛で動く（レビュー
// R-L16）。前は user:: を残して構文エラーにし、exists には最初の解の束縛を渡した。
void check_option_forms(const std::filesystem::path& root) {
  check_suite(root, {"option_user",
                     "\t:- dynamic(r16_flag/0).\n\t:- dynamic(r16_left/0).\n"
                     "\ttest(u1, true, [setup(user::assertz(r16_flag))]) :- r16_flag.\n"
                     "\ttest(u2, true, [cleanup(user::retract(r16_left))]) :- assertz(r16_left).\n"
                     "\ttest(u3, false) :- r16_left.\n",
                     3, 3, {}});
  check_suite(root, {"option_exists",
                     "\t:- dynamic(r16_seen/1).\n"
                     "\ttest(e1, exists(X == 2), [cleanup(assertz(r16_seen(X)))]) :- member(X, [1, 2]).\n"
                     "\ttest(e2, true) :- r16_seen(2).\n",
                     2, 2, {}});
}

// cleanup は本体の後に、本体の束縛で動く。ストリームは字面で写しても同じものを指すので、同じ別名を次の試験が開ける。
// 本体が失敗しても動き、cleanup 自体の失敗と例外は結果を変えない。
void check_cleanup(const std::filesystem::path& root) {
  check_suite(root, {"cleanup_runs",
                     "\t:- dynamic(left_over/0).\n\ttest(c1, true, [cleanup(retract(left_over))]) :- assertz(left_over).\n"
                     "\ttest(c2, false) :- left_over.\n",
                     2, 2, {}});
  // 平たい道、判定器の道（deterministic は judge_query）、ランナー目標のある本体の道の 3 つ。
  check_suite(root, {"cleanup_streams",
                     "\ttest(c3, true, [cleanup(close(S))]) :- open(opt_a, write, S, [alias(opt_a)]).\n"
                     "\ttest(c4, true, [cleanup(close(S))]) :- open(opt_a, write, S, [alias(opt_a)]).\n"
                     "\ttest(c5, deterministic, [cleanup(close(S))]) :- open(opt_b, write, S, [alias(opt_b)]).\n"
                     "\ttest(c6, deterministic, [cleanup(close(S))]) :- open(opt_b, write, S, [alias(opt_b)]).\n"
                     "\ttest(c7, true, [cleanup(close(S))]) :- assertion(true), open(opt_c, write, S, [alias(opt_c)]).\n"
                     "\ttest(c8, true, [cleanup(close(S))]) :- assertion(true), open(opt_c, write, S, [alias(opt_c)]).\n",
                     6, 6, {}});
  check_suite(root, {"cleanup_after_failure",
                     "\ttest(c9, false, [cleanup(close(opt_d))]) :- open(opt_d, write, _, [alias(opt_d)]), fail.\n"
                     "\ttest(c10, true, [cleanup(close(opt_d))]) :- open(opt_d, write, _, [alias(opt_d)]).\n",
                     2, 2, {}});
  check_suite(root, {"cleanup_broken",
                     "\ttest(c11, true, [cleanup(fail)]) :- true.\n\ttest(c12, true, [cleanup(throw(x))]) :- true.\n",
                     2, 2, {}});
}

// 補助の節の本体の {Goal} は、Logtalk でただの Prolog の呼び出し。前は外さずに読み込み、呼ぶと {}/1 の
// existence_error になった。setup :- {retractall(v(_))}. の setup/0 は、試験ごとに動かして失敗を捨てていたので隠れた。
// DCG の {} と、データとしての {} は残す。
void check_curly_helpers(const std::filesystem::path& root) {
  check_suite(root, {"curly_helper",
                     "\t:- dynamic(v/1).\n\thelper :- {assertz(v(1))}, {v(1)}.\n\tdata(X) :- X = {a}.\n"
                     "\tgreeting --> {true}, [hello].\n"
                     "\ttest(h1, true) :- helper.\n\ttest(h2, true(X == {a})) :- data(X).\n"
                     "\ttest(h3, true) :- phrase(greeting, [hello]).\n",
                     3, 3, {}});
}

// オブジェクトの setup/0 は全部の試験の前に 1 回、cleanup/0 は後に 1 回動く（lgtunit の run/0）。前は試験ごとに動いた。
// setup/0 が失敗するか投げると、lgtunit は試験を飛ばす。Mosa は分母から落とさず、どの試験も実際 setup で不合格にする。
void check_object_hooks(const std::filesystem::path& root) {
  check_suite(root, {"hooks_once",
                     "\t:- dynamic(ran/0).\n\t:- dynamic(flag/0).\n\tsetup :- assertz(ran).\n\tcleanup :- retractall(flag).\n"
                     "\ttest(o1, true) :- assertz(flag).\n"
                     "\ttest(o2, true) :- flag, findall(x, ran, L), length(L, 1).\n",
                     2, 1 + 1, {}});
  check_suite(root, {"hooks_setup_fails", "\tsetup :- fail.\n\ttest(o3, true) :- true.\n\ttest(o4, false) :- fail.\n", 2, 0,
                     {"o3 true setup", "o4 false setup"}});
  check_suite(root, {"hooks_setup_throws", "\tsetup :- throw(x).\n\ttest(o5, true) :- true.\n", 1, 0, {"o5 true setup"}});
}

}  // namespace

int main() {
  const std::filesystem::path root = std::filesystem::path{MOSA_SOURCE_DIR} / "tests" / "tmp" / "suite-option";
  {
    Fixture fixture(root);
    check_setup(root);
    check_unbound_setup(root);
    check_option_forms(root);
    check_cleanup(root);
    check_curly_helpers(root);
    check_object_hooks(root);
  }
  return mosa_test::failures();
}
