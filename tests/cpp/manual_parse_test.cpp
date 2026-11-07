// mosatest --manual の印の読み取り（リリース 0.1.0 Task 4）。印の 3 種、複数行の入力、CRLF、印の無い囲い、
// 既定の入力、console の区切りと引用を確かめる。走らせて比べるところは tests/manual の見本（tests.cmake）が見る。

#include "harness.hpp"
#include "mosa/manual.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace {

using mosa::ExampleKind;
using mosa_test::check;

// 印の 3 種と console。行番号は開きの囲いの行（1 から）。
constexpr std::string_view kMarkers =
    "# Sample\n"                       // 1
    "\n"                               // 2
    "<!-- check: first -->\n"          // 3
    "```text\n"                        // 4
    "?- X = 1.\n"                      // 5
    "X = 1.\n"                         // 6
    "```\n"                            // 7
    "\n"                               // 8
    "<!-- skip: second -->\n"          // 9
    "```console\n"                     // 10
    "$ brew install something\n"       // 11
    "```\n"                            // 12
    "<!-- file: third family.pl -->\n" // 13
    "```prolog\n"                      // 14
    "parent(tom, bob).\n"              // 15
    "```\n"                            // 16
    "<!-- check: fourth -->\n"         // 17
    "```console\n"                     // 18
    "$ mosa --version\n"               // 19
    "mosa 0.1.0\n"                     // 20
    "```\n";                           // 21

void manual_parse_markers() {
  const mosa::ParsedManual parsed = mosa::parse_manual(kMarkers, "f.md");
  check(parsed.errors.empty(), "markers: no errors");
  if (parsed.examples.size() != 4) {
    check(false, "markers: four examples");
    return;
  }
  const auto& first = parsed.examples[0];
  check(first.id == "first" && first.kind == ExampleKind::Repl, "markers: check on text is Repl");
  check(first.file == "f.md" && first.line == 4, "markers: file and fence line of first");
  check(first.body == "?- X = 1.\nX = 1.\n", "markers: body of first");
  check(!first.input.has_value(), "markers: no input override");
  const auto& second = parsed.examples[1];
  check(second.id == "second" && second.kind == ExampleKind::Skip && second.line == 10, "markers: skip");
  const auto& third = parsed.examples[2];
  check(third.id == "third" && third.kind == ExampleKind::File && third.line == 14, "markers: file");
  check(third.file_name == "family.pl" && third.body == "parent(tom, bob).\n", "markers: file name and body");
  const auto& fourth = parsed.examples[3];
  check(fourth.id == "fourth" && fourth.kind == ExampleKind::Console && fourth.line == 18, "markers: console");
}

void manual_parse_override() {
  constexpr std::string_view text =
      "<!-- check: more\n"
      "(X = 1 ; X = 2).\n"
      ";\n"
      "\n"
      "-->\n"
      "```text\n"
      "?- (X = 1 ; X = 2).\n"
      "```\n";
  const mosa::ParsedManual parsed = mosa::parse_manual(text, "o.md");
  check(parsed.errors.empty() && parsed.examples.size() == 1, "override: one example");
  if (parsed.examples.size() != 1) return;
  const auto& example = parsed.examples.front();
  check(example.id == "more" && example.kind == ExampleKind::Repl && example.line == 6, "override: id, kind, line");
  check(example.input == std::string{"(X = 1 ; X = 2).\n;\n\n"}, "override: input verbatim with the empty line");
}

std::string with_crlf(std::string_view text) {
  std::string out;
  for (const char letter : text) {
    if (letter == '\n') out += '\r';
    out += letter;
  }
  return out;
}

bool same(const mosa::ManualExample& left, const mosa::ManualExample& right) {
  return left.id == right.id && left.kind == right.kind && left.file == right.file && left.line == right.line &&
         left.body == right.body && left.input == right.input && left.file_name == right.file_name;
}

void manual_parse_crlf() {
  const mosa::ParsedManual plain = mosa::parse_manual(kMarkers, "f.md");
  const mosa::ParsedManual crlf = mosa::parse_manual(with_crlf(kMarkers), "f.md");
  check(crlf.errors.empty(), "crlf: no errors");
  bool equal = plain.examples.size() == crlf.examples.size();
  for (std::size_t index = 0; equal && index < plain.examples.size(); ++index) {
    equal = same(plain.examples[index], crlf.examples[index]);
  }
  check(equal, "crlf: same examples as LF");
  const std::string override_text = with_crlf("<!-- check: a\nx.\n\n-->\n```text\n?- x.\n```\n");
  const mosa::ParsedManual parsed = mosa::parse_manual(override_text, "c.md");
  check(parsed.examples.size() == 1 && parsed.examples.front().input == std::string{"x.\n\n"},
        "crlf: override input without CR");
}

bool has_error(const mosa::ParsedManual& parsed, std::string_view prefix) {
  for (const std::string& error : parsed.errors) {
    if (error.starts_with(prefix)) return true;
  }
  return false;
}

void manual_parse_unmarked() {
  const mosa::ParsedManual parsed = mosa::parse_manual("Text.\n\n```text\n?- a.\n```\n", "u.md");
  check(parsed.errors.size() == 1 && has_error(parsed, "u.md:3: "), "unmarked: one error naming line 3");
  check(parsed.examples.empty(), "unmarked: no examples");
}

// 印の置き場所と種類の誤り。どれも file:line: で始まる。
void manual_parse_misplaced() {
  check(has_error(mosa::parse_manual("<!-- check: a -->\n\n```text\n?- a.\n```\n", "m.md"), "m.md:1: "),
        "misplaced: marker not right before a fence");
  check(has_error(mosa::parse_manual("<!-- check: a -->\n```prolog\na.\n```\n", "m.md"), "m.md:2: "),
        "misplaced: check on a prolog block");
  check(has_error(mosa::parse_manual("<!-- file: a ../x.pl -->\n```prolog\na.\n```\n", "m.md"), "m.md:1: "),
        "misplaced: file name with a directory");
  check(has_error(mosa::parse_manual("<!-- file: a x.pl -->\n```text\na.\n```\n", "m.md"), "m.md:2: "),
        "misplaced: file on a text block");
  check(has_error(mosa::parse_manual("<!-- check: a -->\n```console\n$ ls -l\n```\n", "m.md"), "m.md:3: "),
        "misplaced: console program not allowed");
  check(has_error(mosa::parse_manual("<!-- check: a -->\n```console\n$ mosa -g \"x\n```\n", "m.md"), "m.md:3: "),
        "misplaced: unbalanced quote");
  check(has_error(mosa::parse_manual("<!-- check: a -->\n```text\n?- a.\n", "m.md"), "m.md:2: "),
        "misplaced: fence not closed");
  check(has_error(mosa::parse_manual("<!-- check: a\nx.\n", "m.md"), "m.md:1: "), "misplaced: override not closed");
  constexpr std::string_view twice = "<!-- skip: a -->\n```\nx\n```\n<!-- skip: a -->\n```\ny\n```\n";
  check(has_error(mosa::parse_manual(twice, "m.md"), "m.md:6: "), "misplaced: id used twice");
  check(mosa::parse_manual("<!-- predicates: system -->\n", "m.md").errors.empty(), "other comments are not markers");
}

void manual_parse_default_input() {
  check(mosa::default_input("?- a.\nyes.\n?- b.\n") == "a.\nb.\n", "default input from ?- lines");
  check(mosa::default_input("X = 1.\n").empty(), "default input empty without ?- lines");
}

void manual_parse_console() {
  constexpr std::string_view text =
      "<!-- check: two -->\n"
      "```console\n"
      "$ mosa --version\n"
      "mosa 0.1.0\n"
      "$ mosa -g \"X = 'a b'.\" -t halt\n"
      "a b\n"
      "\n"
      "```\n";
  const mosa::ParsedManual parsed = mosa::parse_manual(text, "k.md");
  check(parsed.errors.empty() && parsed.examples.size() == 1, "console: one example");
  if (parsed.examples.size() != 1) return;
  const std::vector<mosa::ConsoleCommand> commands = mosa::console_commands(parsed.examples.front().body);
  if (commands.size() != 2) {
    check(false, "console: two commands");
    return;
  }
  check(commands[0].argv == std::vector<std::string>{"mosa", "--version"}, "console: first argv");
  check(commands[0].output == "mosa 0.1.0\n" && commands[0].offset == 0, "console: first output and offset");
  check(commands[1].argv == std::vector<std::string>{"mosa", "-g", "X = 'a b'.", "-t", "halt"},
        "console: quoted argument is one");
  check(commands[1].output == "a b\n\n" && commands[1].offset == 2, "console: second output and offset");
}

}  // namespace

int main() {
  manual_parse_markers();
  manual_parse_override();
  manual_parse_crlf();
  manual_parse_unmarked();
  manual_parse_misplaced();
  manual_parse_default_input();
  manual_parse_console();
  return mosa_test::failures() == 0 ? 0 : 1;
}
