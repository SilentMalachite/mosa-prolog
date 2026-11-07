// mosatest --manual の印の読み取り（リリース 0.1.0 Task 4）。印の 3 種、複数行の入力、CRLF、印の無い囲い、
// 既定の入力、console の区切りと引用を確かめる。走らせて比べるところは tests/manual の見本（tests.cmake）が見る。
// 述語の表の読み取りと、表と比べる処理系の一覧（組込と library の公開リスト）も見る（Task 5）。

#include "harness.hpp"
#include "mosa/machine.hpp"
#include "mosa/manual.hpp"

#include <cstddef>
#include <initializer_list>
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

// 版の突き合わせの並びは印の種類も含む。片方の版だけが skip: にすると違う並びになる（Task 4 の修正 5）。
void manual_parse_kind_sequence() {
  constexpr std::string_view checked = "<!-- check: a -->\n```text\n?- a.\n```\n<!-- file: b x.pl -->\n```prolog\nx.\n```\n";
  constexpr std::string_view skipped = "<!-- skip: a -->\n```text\n?- a.\n```\n<!-- file: b y.pl -->\n```prolog\ny.\n```\n";
  const std::string left = mosa::id_sequence(mosa::parse_manual(checked, "en.md").examples);
  const std::string right = mosa::id_sequence(mosa::parse_manual(skipped, "ja.md").examples);
  check(left == "check:a file:b", "kind sequence: check and file");
  check(right == "skip:a file:b", "kind sequence: skip and file");
  check(left != right, "kind sequence: skip in one edition differs from check in the other");
}

// 検査器が読まない囲い（引用の中、タブの後ろ、箇条書きの印と同じ行）は黙って飛ばさず誤りにする（修正 6）。
void manual_parse_unreadable() {
  constexpr std::string_view quoted =
      "> <!-- check: q -->\n"  // 1
      "> ```text\n"            // 2
      "> <!-- check: z -->\n"  // 3
      "> ```\n"                // 4
      "<!-- check: a -->\n"    // 5
      "```text\n"              // 6
      "?- a.\n"                // 7
      "```\n";                 // 8
  const mosa::ParsedManual parsed = mosa::parse_manual(quoted, "q.md");
  check(parsed.errors.size() == 1 && has_error(parsed, "q.md:2: "), "unreadable: fence in a block quote");
  check(parsed.examples.size() == 1 && parsed.examples.front().id == "a", "unreadable: lines inside are skipped");
  check(has_error(mosa::parse_manual("<!-- skip: t -->\n\t```text\nx\n\t```\n", "t.md"), "t.md:2: "),
        "unreadable: fence after a tab");
  check(has_error(mosa::parse_manual("- ```text\n  x\n  ```\n", "l.md"), "l.md:1: "),
        "unreadable: fence on a list marker line");
  check(has_error(mosa::parse_manual("1. ```text\n   x\n   ```\n", "n.md"), "n.md:1: "),
        "unreadable: fence on a numbered list marker line");
  const mosa::ParsedManual spaces = mosa::parse_manual("1. Step\n\n   <!-- skip: s -->\n   ```text\n   x\n   ```\n", "s.md");
  check(spaces.errors.empty() && spaces.examples.size() == 1, "fence indented with spaces in a list is read");
  check(mosa::parse_manual("> quoted text\n- item\n\tindented\n", "p.md").errors.empty(), "prose is not a fence");
}

// シェルが argv を変える字は誤りにする。'…' の中は字のまま（修正 7）。
void manual_parse_shell_characters() {
  const auto console = [](std::string_view command) {
    return "<!-- check: c -->\n```console\n$ " + std::string{command} + "\nout\n```\n";
  };
  check(has_error(mosa::parse_manual(console("mosa -g \"a\\\\b\""), "s.md"), "s.md:3: "), "shell: \\ in \"...\"");
  check(has_error(mosa::parse_manual(console("mosa -g \"X = $Y\""), "s.md"), "s.md:3: "), "shell: $ in \"...\"");
  check(has_error(mosa::parse_manual(console("mosa -g \"`x`\""), "s.md"), "s.md:3: "), "shell: ` in \"...\"");
  check(has_error(mosa::parse_manual(console("mosa a|b"), "s.md"), "s.md:3: "), "shell: | outside quotes");
  check(has_error(mosa::parse_manual(console("mosa x > out.txt"), "s.md"), "s.md:3: "), "shell: > outside quotes");
  check(has_error(mosa::parse_manual(console("mosa *.pl"), "s.md"), "s.md:3: "), "shell: glob outside quotes");
  const mosa::ParsedManual literal = mosa::parse_manual(console("mosa -g 'X = \"$a\\b|\" ; !'"), "s.md");
  check(literal.errors.empty() && literal.examples.size() == 1, "shell: everything is literal inside '...'");
  if (literal.examples.size() != 1) return;
  const std::vector<mosa::ConsoleCommand> commands = mosa::console_commands(literal.examples.front().body);
  check(commands.size() == 1 && commands.front().argv == std::vector<std::string>{"mosa", "-g", "X = \"$a\\b|\" ; !"},
        "shell: argv of the single-quoted argument");
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

// 述語の表（リリース 0.1.0 Task 5）。区切りは印から次の見出しまで。囲いの中の行は見出しにも表の行にもならない。
constexpr std::string_view kTables =
    "# Reference\n"                                // 1
    "<!-- predicates: between -->\n"               // 2
    "| Predicate | Meaning |\n"                    // 3
    "| --- | --- |\n"                              // 4
    "| `between/3` | plain |\n"                    // 5
    "|`','/2`| quoted, no spaces |\n"              // 6
    "| `'it''s'/0` | doubled quote |\n"            // 7
    "| `=../2` | symbol char |\n"                  // 8
    "| plain/1 | not a code span |\n"              // 9
    "| `foo` | no arity |\n"                       // 10
    "| `bar/x` | arity not a number |\n"           // 11
    "`succ/2` is not in a table row\n"             // 12
    "| `ひらがな_カタカナ/2` | Japanese name |\n"   // 13
    "## Next\n"                                    // 14
    "| `after/0` | after the heading |\n"          // 15
    "<!--predicates:system-->\n"                   // 16
    "<!-- skip: block -->\n"                       // 17
    "```console\n"                                 // 18
    "# a shell comment, not a heading\n"           // 19
    "| `inside/0` | in a code block |\n"           // 20
    "```\n"                                        // 21
    "| `atom/1` | after the code block |\n";       // 22

bool same(const std::vector<mosa::PredicateIndicator>& entries,
          const std::vector<mosa::PredicateIndicator>& wanted) {
  return entries == wanted;
}

void manual_parse_tables() {
  const mosa::ParsedManual parsed = mosa::parse_manual(kTables, "t.md");
  check(parsed.errors.empty(), "tables: no errors");
  if (parsed.tables.size() != 2) {
    check(false, "tables: two sections");
    return;
  }
  const mosa::ManualTable& first = parsed.tables[0];
  check(first.library == "between" && first.line == 2, "tables: first section and its marker line");
  check(same(first.entries, {{"between", 3}, {",", 2}, {"it's", 0}, {"=..", 2}, {"ひらがな_カタカナ", 2}}),
        "tables: entries of the first section, quotes removed");
  const mosa::ManualTable& second = parsed.tables[1];
  check(second.library == "system" && second.line == 16, "tables: marker without spaces");
  check(same(second.entries, {{"atom", 1}}), "tables: code block lines are neither headings nor rows");
  check(parsed.examples.size() == 1, "tables: the marked code block is still an example");
}

void manual_parse_table_errors() {
  check(has_error(mosa::parse_manual("<!-- predicates: -->\n", "e.md"), "e.md:1: "), "tables: no library name");
  check(has_error(mosa::parse_manual("<!-- predicates: a b -->\n", "e.md"), "e.md:1: "), "tables: two names");
  check(has_error(mosa::parse_manual("<!-- predicates: lists\n-->\n", "e.md"), "e.md:1: "), "tables: not closed");
  constexpr std::string_view twice =
      "<!-- predicates: between -->\n"  // 1
      "| `between/3` | |\n"             // 2
      "# Other\n"                       // 3
      "<!-- predicates: system -->\n"   // 4
      "| `between/3` | |\n"             // 5
      "<!-- predicates: between -->\n"  // 6
      "| `succ/2` | |\n"                // 7
      "| `between/3` | |\n";            // 8
  const mosa::ParsedManual parsed = mosa::parse_manual(twice, "d.md");
  check(parsed.errors.size() == 1 && has_error(parsed, "d.md:8: "),
        "tables: the same entry twice for one name in a file, other names apart");
}

bool has_indicator(const std::vector<mosa::PredicateIndicator>& all, std::string_view name, int arity) {
  for (const mosa::PredicateIndicator& item : all) {
    if (item.name == name && item.arity == arity) return true;
  }
  return false;
}

// 引用した原子にする。名前に ' と \ があっても読み戻せる。
std::string quoted(std::string_view name) {
  std::string out = "'";
  for (const char letter : name) {
    if (letter == '\'' || letter == '\\') out += '\\';
    out += letter;
  }
  return out + "'";
}

// 一覧のどれもが predicate_property(H, built_in) を満たす。表の system はこの性質の名前を並べる。
// :/2 だけは問えない。predicate_property は M:G をモジュールの限定として外すので、頭 _:_ は束縛の無い頭になる。
void manual_builtin_indicators() {
  const std::vector<mosa::PredicateIndicator> all = mosa::builtin_indicators();
  for (int arity = 1; arity <= 8; ++arity) check(has_indicator(all, "call", arity), "builtins: call/1..8");
  check(has_indicator(all, "halt", 0) && has_indicator(all, "halt", 1), "builtins: halt/0 and halt/1");
  check(has_indicator(all, ",", 2) && has_indicator(all, "length", 2), "builtins: ,/2 and length/2");
  check(has_indicator(all, ":", 2), "builtins: :/2");
  check(!has_indicator(all, "use_module", 1) && !has_indicator(all, "dynamic", 1), "builtins: no load directives");
  bool internal = false;
  for (const mosa::PredicateIndicator& item : all) internal = internal || item.name.starts_with('$');
  check(!internal, "builtins: no $ helpers");
  const auto machine = mosa::Machine::create();
  for (const mosa::PredicateIndicator& item : all) {
    if (item.name == ":" && item.arity == 2) continue;
    const std::string goal = "functor(H, " + quoted(item.name) + ", " + std::to_string(item.arity) +
                             "), predicate_property(H, built_in).";
    const mosa::Stop stop = machine->query(goal);
    check(stop.kind == mosa::StopKind::Success, "builtins: built_in holds for " + item.name);
  }
}

void manual_library_exports() {
  const auto between = mosa::library_exports("between");
  check(between && same(*between, {{"between", 3}, {"succ", 2}, {"plus", 3}}), "exports: library(between)");
  const auto lists = mosa::library_exports("lists");
  check(lists && has_indicator(*lists, "length", 2) && has_indicator(*lists, "append", 3), "exports: library(lists)");
  const auto japanese = mosa::library_exports("japanese");
  check(japanese && has_indicator(*japanese, "ひらがな_カタカナ", 2), "exports: library(japanese)");
  for (const std::string_view name : {"apply", "format", "strings"}) {
    check(mosa::library_exports(name).has_value(), "exports: library " + std::string{name});
  }
  for (const std::string_view name : {"messages_en", "messages_ja", "system", "nope"}) {
    check(!mosa::library_exports(name).has_value(), "exports: none for " + std::string{name});
  }
}

}  // namespace

int main() {
  manual_parse_markers();
  manual_parse_override();
  manual_parse_crlf();
  manual_parse_unmarked();
  manual_parse_misplaced();
  manual_parse_kind_sequence();
  manual_parse_unreadable();
  manual_parse_shell_characters();
  manual_parse_default_input();
  manual_parse_console();
  manual_parse_tables();
  manual_parse_table_errors();
  manual_builtin_indicators();
  manual_library_exports();
  return mosa_test::failures() == 0 ? 0 : 1;
}
