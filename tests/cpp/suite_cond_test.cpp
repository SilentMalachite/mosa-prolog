// :- if と test/3 の condition(Goal) の評価は、一つのゴールとして ISO の制御（7.8）のとおりに動く（レビュー 2026-11-20
// の M-52）。前は旗か :: か predicate_property/2 を含む条件を部分ごとに別の問いにしたので、(C -> T ; E) の T が
// 失敗すると E へ進み、左の連言で束縛した変数が右に届かなかった。実行しない Logtalk のゴールは、通ったときだけ
// 未対応にし、catch/3 の変数の捕まえ手でも隠さない。catch/3 の捕まえ手は、変数でなくても ISO のとおり比べる。
// 1 件ごとに 1 ファイルのスイートを tests/tmp/suite-cond に書いて run_iso_suite で流し、分母と合格数と不合格の行を見る。

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
  // 別欄に出る case の名前（L-43）。
  std::vector<std::string_view> outside = {};
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
  const auto side = body.find("別欄:\n");
  const std::string notes = side == std::string::npos ? std::string{} : body.substr(side);
  for (const std::string_view line : item.outside) {
    const std::string full = "\npredicates/atom_1 " + std::string{line} + "\n";
    mosa_test::check(notes.find(full) != std::string::npos, name + ": outside " + std::string{line} + shown);
  }
}

// 枝の選び方。_wrong の付く試験は、選ばれると落ちる。前は g1_wrong と g2_wrong の枝を選んだ。
void check_gates(const std::filesystem::path& root) {
  check_suite(root, {"gate_if_then_else",
                     ":- if((\\+ current_logtalk_flag(a, b) -> fail ; true)).\n\ttest(g1_wrong, true) :- fail.\n"
                     ":- else.\n\ttest(g1, true).\n:- endif.\n",
                     1, 1, {}});
  check_suite(root, {"gate_binding",
                     ":- if((X = a, \\+ current_logtalk_flag(f, v), X == a)).\n\ttest(g2, true).\n"
                     ":- else.\n\ttest(g2_wrong, true) :- fail.\n:- endif.\n",
                     1, 1, {}});
}

// condition/1 が成り立たない試験は動かさず、分母にも入れない。前は c1 を動かして落とし、c2 と c3 を外へ出した。
void check_conditions(const std::filesystem::path& root) {
  check_suite(root, {"condition_if_then_else",
                     "\ttest(c1, true, [condition((\\+ current_logtalk_flag(a, b) -> fail ; true))]) :- fail.\n"
                     "\ttest(c0, true).\n",
                     1, 1, {}});
  check_suite(root, {"condition_binding",
                     "\ttest(c2, true, [condition((X = a, \\+ current_logtalk_flag(f, v), X == a))]).\n"
                     "\ttest(c3, true, [condition(((member(X, [1, 2]), \\+ current_logtalk_flag(a, b))"
                     " *-> X == 2 ; fail))]).\n",
                     2, 2, {}});
}

// 実行しない :: は通ったときだけ未対応で、catch/3 の変数の捕まえ手でも隠さない。ここは前後で同じ守り。
// ほかの例外は ISO のとおり捕まえ手と単一化して捕まえる。前は捕まえ手が変数のときしか回復を動かさず、k1 を外へ出した。
void check_guards(const std::filesystem::path& root) {
  check_suite(root, {"condition_unsupported",
                     "\ttest(u1, true, [condition(foo::bar)]).\n"
                     "\ttest(u2, true, [condition(catch(foo::bar, _, true))]).\n"
                     "\ttest(u3, true, [condition((fail, foo::bar))]).\n"
                     "\ttest(u4, true, [condition(((\\+ current_logtalk_flag(a, b) ; foo::bar), true))]).\n",
                     3, 1, {"u1 true unsupported", "u2 true unsupported"}});
  // ^^ も :: と同じく通ったときだけ未対応（レビュー R-L15）。前は ^^ を前置の演算子として読めず、通らない枝の ^^ も
  // 条件ごと未対応にした。
  check_suite(root, {"condition_super",
                     "\ttest(p1, true, [condition((fail, ^^foo))]).\n"
                     "\ttest(p2, true, [condition((true ; ^^foo))]).\n"
                     "\ttest(p3, true, [condition(^^foo)]).\n",
                     2, 1, {"p3 true unsupported"}});
  check_suite(root, {"condition_catch",
                     "\ttest(k1, true, [condition(catch((\\+ current_logtalk_flag(a, b), throw(e)), e, true))]).\n"
                     "\ttest(k2, true, [condition(catch((\\+ current_logtalk_flag(a, b), throw(e)), f, true))]).\n"
                     "\ttest(k3, true, [condition((\\+ current_logtalk_flag(a, b), "
                     "predicate_property(atom_length(_, _), built_in)))]).\n"
                     "\ttest(k4, true, [condition(os::operating_system_type(windows))]).\n",
                     2, 2, {}});
}

// レビュー L-43。:- if の枝の :- uses(lgtunit, [op(...)]) の演算子は、その枝が選ばれたときだけ機械に入る。前は枝を
// 問わず入れたので、選ばれない枝の ===> で書いた t1 が通った。今の t1 は字面を機械が読めず、実行器はそれを fail と
// 数える。選ばれた枝なら枝の中でも後ろでも使える。
void check_uses_gate(const std::filesystem::path& root) {
  check_suite(root, {"uses_not_taken",
                     ":- if(fail).\n:- uses(lgtunit, [op(700, xfx, ===>)]).\n:- endif.\n"
                     "\ttest(t1, true) :- X = (a ===> b), X == ===>(a, b).\n\ttest(t0, true).\n",
                     2, 1, {"t1 true fail"}});
  check_suite(root, {"uses_taken",
                     ":- if(true).\n:- uses(lgtunit, [op(700, xfx, ===>)]).\n"
                     "\ttest(t2, true) :- X = (a ===> b), X == ===>(a, b).\n:- endif.\n"
                     "\ttest(t3, true) :- X = (a ===> b), X == ===>(a, b).\n",
                     2, 2, {}});
}

// レビュー L-43。- を付けた case は、その項が - で始まるときだけ別欄にする。前は行の頭の字で決めたので、同じ行の
// 後ろの b を別欄にし、行の途中の - test(c, ...) は試験でない節として消えた。行の頭、字下げ、注釈の後は前と同じ。
void check_minus(const std::filesystem::path& root) {
  check_suite(root, {"minus_shared_line", "\t- test(a, true). test(b, true) :- fail.\n\ttest(m0, true).\n", 2, 1,
                     {"b true fail"}, {"a"}});
  check_suite(root, {"minus_after_term", "\tfoo. - test(c, true) :- fail.\n\ttest(m1, true).\n", 1, 1, {}, {"c"}});
  check_suite(root, {"minus_kept",
                     "- test(d, true) :- fail.\n\t- test(e, true) :- fail.\n\t/* x */ - test(f, true) :- fail.\n"
                     "\t% y\n\t- test(g, true) :- fail.\n\ttest(m2, true).\n",
                     1, 1, {}, {"d", "e", "f", "g"}});
}

}  // namespace

int main() {
  const std::filesystem::path root = std::filesystem::path{MOSA_SOURCE_DIR} / "tests" / "tmp" / "suite-cond";
  {
    Fixture fixture(root);
    check_gates(root);
    check_conditions(root);
    check_guards(root);
    check_uses_gate(root);
    check_minus(root);
  }
  return mosa_test::failures();
}
