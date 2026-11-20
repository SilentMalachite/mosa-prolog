// Logtalk のスイートの分母は、ファイルに書かれた試験をすべて数える（レビュー 2026-11-20 の H-40）。
// 1 件ごとに 1 ファイルのスイートを tests/tmp/suite-count に書いて run_iso_suite で流し、報告の分母と合格数と不合格の行を見る。
// test/1 は test(Name, true) と同じで、前は読む側が黙って捨てていたので分母にも不合格にも出なかった。
// 読めない項のあるファイルは、前は case を捨てて不合格 1 件だった。試験が 3 件でも分母は 1 になった。

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

// 報告の「不合格:」から「別欄:」までの行。別欄の行を不合格と取り違えない。
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

// 不合格の行は「ファイル 名前 期待 実際」。期待の行は、ファイルの部分を除いて書く。
struct Count {
  std::string_view name;
  std::string_view text;
  int denominator;
  int passed;
  std::vector<std::string_view> failed;
};

void check_count(const std::filesystem::path& root, const Count& item) {
  const std::string name{item.name};
  const auto suite = root / name;
  mosa_test::check(write_text(suite / "logtalk" / "prolog" / "predicates" / "atom_1" / "tests.lgt", item.text),
                   name + ": write the suite");
  const auto report = root / (name + "-report.md");
  const int code = mosa::run_iso_suite(suite.string(), report.string());
  const std::string body = read_text(report);
  const std::string label = " in\n" + body;
  mosa_test::check(code == (item.failed.empty() ? 0 : 1), name + ": exit code " + std::to_string(code) + label);
  mosa_test::check(body.find("\n分母: " + std::to_string(item.denominator) + "\n") != std::string::npos,
                   name + ": denominator " + std::to_string(item.denominator) + label);
  mosa_test::check(body.find("\n合格数: " + std::to_string(item.passed) + "\n") != std::string::npos,
                   name + ": passed " + std::to_string(item.passed) + label);
  const std::string failed = failed_lines(body);
  // 空の行は、名前の無い「ファイルだけ」の不合格の行。
  for (const std::string_view line : item.failed) {
    const std::string full =
        line.empty() ? std::string{"predicates/atom_1/tests.lgt\n"} : "predicates/atom_1/tests.lgt " + std::string{line} + "\n";
    mosa_test::check(failed.find(full) != std::string::npos, name + ": " + full + label);
  }
}

void check_ends(const std::filesystem::path& root, std::string_view name_view, std::string_view text,
                const std::vector<std::string_view>& lines) {
  const std::string name{name_view};
  const auto suite = root / name;
  mosa_test::check(write_text(suite / "logtalk" / "prolog" / "predicates" / "atom_1" / "tests.lgt", text),
                   name + ": write the suite");
  const auto report = root / (name + "-report.md");
  const int code = mosa::run_iso_suite(suite.string(), report.string());
  const std::string body = read_text(report);
  mosa_test::check(code == 1, name + ": the suite fails in\n" + body);
  const std::string failed = failed_lines(body);
  for (const std::string_view line : lines) {
    const std::string full =
        line.empty() ? std::string{"predicates/atom_1/tests.lgt\n"} : "predicates/atom_1/tests.lgt " + std::string{line} + "\n";
    mosa_test::check(failed.find(full) != std::string::npos, name + ": " + full + " in\n" + body);
  }
}

std::string object(std::string_view clauses) {
  return ":- object(tests, extends(lgtunit)).\n" + std::string{clauses} + "\n:- end_object.\n";
}

// 本体の無い test(c). も、本体のある test(a) :- fail. も 1 件ずつ数える。
void check_test_1() {
  const std::filesystem::path root = std::filesystem::path{MOSA_SOURCE_DIR} / "tests" / "tmp" / "suite-count";
  Fixture fixture(root);
  check_count(root, {"test_1", object("\ttest(a) :- fail.\n\ttest(b, true).\n\ttest(c).\n"), 3, 2, {"a true fail"}});
  // 読み込みが落ちたファイルは、test/1 だけでも、その数だけ load で不合格になる。前は分母に何も足さなかった。
  check_count(root, {"test_1_load", object("\tatom_length(_, _).\n\ttest(d) :- true.\n\ttest(e).\n"), 2, 0,
                     {"d true load", "e true load"}});
}

// 読めない項は飛ばし、読めた case をすべて読めなかった理由で不合格にする。読めない項そのものは、ファイルの行で 1 件と数える。
// 選ばなかった枝の case は、読み込みが落ちたときと同じく別欄に置く。終止符そのものが誤りの項（foo(.）の後は、
// 読み手が終止符を手放さず先へ進まない（レビュー H-15）ので、そこで読むのを止める。数えるのは読めた分だけの下限。
void check_read_error() {
  const std::filesystem::path root = std::filesystem::path{MOSA_SOURCE_DIR} / "tests" / "tmp" / "suite-count";
  Fixture fixture(root);
  check_count(root, {"read_error", object("\ttest(a, true).\n\ttest(b, true) :- foo(a b).\n\ttest(c).\n"), 3, 0,
                     {"", "a true unexpected_token", "c true unexpected_token"}});
  check_count(root, {"read_error_guard",
                     object("\t:- if(fail).\n\ttest(d, true).\n\t:- else.\n\ttest(e, true).\n\t:- endif.\n"
                            "\ttest(f, true) :- ).\n"),
                     2, 0, {"", "e true unexpected_token"}});
  // 終止符そのものが誤りの項でも終わる。読み手が進まない誤りで回り続けると、ここで ctest の時間切れまで止まる。
  // 分母は H-15 を直すと c の分だけ増えるので見ず、読めた a と、ファイルの行だけを見る。
  check_ends(root, "read_stuck", object("\ttest(a, true).\n\ttest(b, true) :- foo(.\n\ttest(c).\n"),
             {"", "a true unexpected_token"});
  // if と endif の釣り合わないファイルは、どの case を数えるかも決まらないので、今までどおり 1 件。
  check_count(root, {"read_unbalanced", object("\t:- if(true).\n\ttest(g, true).\n"), 1, 0, {""}});
}

}  // namespace

int main() {
  check_test_1();
  check_read_error();
  return mosa_test::failures();
}
