// mosatest --manual の印の読み取り（リリース 0.1.0 Task 4）。印の 3 種、複数行の入力、CRLF、印の無い囲い、
// 既定の入力、console の区切りと引用を確かめる。走らせて比べるところは tests/manual の見本（tests.cmake）が見る。
// 述語の表の読み取りと、表と比べる処理系の一覧（組込と library の公開リスト）も見る（Task 5）。ライブラリは
// 組み上げで写した隣の lib/ と、読めないライブラリの見本 tests/manual/bad-lib/lib（MOSA_SOURCE_DIR）を読む。

#include "harness.hpp"
#include "mosa/exe_path.hpp"
#include "mosa/machine.hpp"
#include "mosa/manual.hpp"
#include "mosa/version.hpp"

#include <cstddef>
#include <filesystem>
#include <initializer_list>
#include <optional>
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

// 述語の表（リリース 0.1.0 Task 5）。区切りは印から次の 0 桁目の ATX の見出しまでで、中に書ける行は決めた種類だけ
// （修正 3）。区切りの外の囲いの中の行は、見出しにも印にもならない。
constexpr std::string_view kTables =
    "# Reference\n"                                // 1
    "<!-- predicates: between -->\n"               // 2
    "| Predicate | Meaning |\n"                    // 3
    "| --- | --- |\n"                              // 4
    "| `between/3` | plain |\n"                    // 5
    "|`','/2`| quoted, no spaces |\n"              // 6
    "| `'it''s'/0` | doubled quote |\n"            // 7
    "| `=../2` | symbol char |\n"                  // 8
    "| ` \\+/1 ` | backslash unquoted, padded |\n" // 9
    "| `ひらがな_カタカナ/2` | Japanese name |\n"   // 10
    "\n"                                           // 11
    "Prose after the table, without code.\n"       // 12
    "2 more words; 日本語の地の文も書ける。\n"      // 13
    "## Next\n"                                    // 14
    "| `after/0` | after the heading |\n"          // 15
    "<!-- skip: block -->\n"                       // 16
    "```console\n"                                 // 17
    "# a shell comment, not a heading\n"           // 18
    "<!-- predicates: inside -->\n"                // 19
    "```\n"                                        // 20
    "<!--predicates:system-->\n"                   // 21
    "| Name | |\n"                                 // 22
    "|:---|---:|\n"                                // 23
    "| `atom/1` | after the code block |\n";       // 24

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
  check(same(first.entries, {{"between", 3}, {",", 2}, {"it's", 0}, {"=..", 2}, {"\\+", 1}, {"ひらがな_カタカナ", 2}}),
        "tables: entries of the first section, quotes removed");
  const mosa::ManualTable& second = parsed.tables[1];
  check(second.library == "system" && second.line == 21, "tables: marker without spaces, not the one in the block");
  check(same(second.entries, {{"atom", 1}}), "tables: code block lines are neither headings nor rows");
  check(parsed.examples.size() == 1, "tables: the marked code block is still an example");
}

// 区切りの中の 1 行で、書けない種類のもの。どれも 1 つの誤りで、何も数えない（修正 3 の決定）。
void manual_parse_section_lines() {
  for (const std::string_view line :
       {"    | `code/1` |", "\t| `tab/1` |", " | `space/1` |", "- In a list item", "* item", "+ item", "1. item",
        "2) item", "> A quote", "* * *", "- - -", "___", "===", "---", "<!-- a note -->", "<!--", "<!-->", "<div>",
        "~~~", "```text\n| `in/1` |\n```", "#hashtag", "`code` first", "Prose with `code` in it.",
        "| `nope/1` | a code span in the header position |", "| --- |", "  ## indented heading"}) {
    const std::string text = "<!-- predicates: lists -->\n" + std::string{line} + "\n";
    const mosa::ParsedManual parsed = mosa::parse_manual(text, "w.md");
    check(parsed.errors.size() == 1 && has_error(parsed, "w.md:2: "), "section: error for " + std::string{line});
    check(parsed.tables.size() == 1 && parsed.tables.front().entries.empty(), "section: nothing counted for " +
                                                                                    std::string{line});
  }
  check(mosa::parse_manual("<!-- predicates: lists -->\n2026 was a year.\n3.5 is a float.\n", "w.md").errors.empty(),
        "section: prose that starts with a number but is not a list item");
}

// 再レビューの見本（rr2/probes）。区切りの中の箇条書き、字下げの続きの行、太字の見出し、区切りの線、引用、注釈は
// 誤りで、章 14 の形は通る。first は最初の誤りの行（0 なら誤り無し）、entries は数えた行。
struct Probe {
  std::string_view name;
  std::string_view text;
  std::size_t first = 0;
  std::vector<mosa::PredicateIndicator> entries;
};

constexpr std::string_view kProbeRows = "| Predicate | Meaning |\n| --- | --- |\n| `between/3` | x |\n";

std::vector<Probe> probes() {
  const std::string_view marker = "<!-- predicates: between -->\n";
  static const std::string boldhead = std::string{marker} + "| **`nope/1`** | Not in Mosa |\n| --- | --- |\n"
                                      "| `between/3` | x |\n| `succ/2` | x |\n";
  static const std::string contline = std::string{marker} + "Predicates of the library:\n"
                                      "    `nope/1` | Not in Mosa |\n| --- | --- |\n| `between/3` | x |\n";
  static const std::string listhead = std::string{marker} + "- | `nope/1` | Not in Mosa |\n  | --- | --- |\n"
                                      "  | `between/3` | x |\n";
  static const std::string listhead2 = std::string{marker} + "1. `nope/1` | Not in Mosa |\n   | --- | --- |\n"
                                       "   | `between/3` | x |\n";
  static const std::string listok = std::string{marker} + "- | Predicate | Meaning |\n  | --- | --- |\n"
                                    "  | `between/3` | x |\n";
  static const std::string listbreak = std::string{marker} + std::string{kProbeRows} + "\n- a list item\n\n * * *\n\n"
                                       "    | Predicate |\n    | --- |\n    | `succ/2` |\n";
  static const std::string thematic = std::string{marker} + std::string{kProbeRows} +
                                      "\n___\n\n    | Predicate |\n    | --- |\n    | `succ/2` |\n";
  static const std::string listquote = std::string{marker} + std::string{kProbeRows} +
                                       "\n- > | Predicate | Meaning |\n  > | --- | --- |\n  > | `nope/1` | x |\n";
  static const std::string sentence = std::string{marker} + "`between/3` and its friends are listed below.\n" +
                                      std::string{kProbeRows};
  return {{"boldhead", boldhead, 2, {{"between", 3}, {"succ", 2}}},
          {"contline", contline, 3, {{"between", 3}}},
          {"listhead", listhead, 2, {}},
          {"listhead2", listhead2, 2, {}},
          {"listok", listok, 2, {}},
          {"listbreak", listbreak, 6, {{"between", 3}}},
          {"thematic3", thematic, 6, {{"between", 3}}},
          {"listquote", listquote, 6, {{"between", 3}}},
          {"sentence_after", sentence, 2, {{"between", 3}}}};
}

void manual_parse_probes() {
  for (const Probe& probe : probes()) {
    const mosa::ParsedManual parsed = mosa::parse_manual(probe.text, "p.md");
    const std::string prefix = "p.md:" + std::to_string(probe.first) + ": ";
    check(!parsed.errors.empty() && parsed.errors.front().starts_with(prefix),
          "probe " + std::string{probe.name} + ": first error at line " + std::to_string(probe.first));
    check(parsed.tables.size() == 1 && same(parsed.tables.front().entries, probe.entries),
          "probe " + std::string{probe.name} + ": counted rows");
  }
}

// 章 14 の形（見出し、文、印、| Predicate | Meaning |、| --- | --- |、行）は通る。印の前の文はコードスパンを持ってよい。
constexpr std::string_view kChapter14 =
    "# Ref\n\n<!-- check: query -->\n```text\n?- X = 1.\nX = 1.\n```\n\n"            // 1-8
    "## between\n\n<!-- predicates: between -->\n| Predicate | Meaning |\n| --- | --- |\n"  // 9-13
    "| `between/3` | x |\n| `succ/2` | x |\n| `plus/3` | x |\n\n"                        // 14-17
    "## more\n\n`between/3` comes first. Use it for ranges.\n\n"                         // 18-21
    "<!-- predicates: lists -->\n| 述語 | 意味 |\n| --- | --- |\n| `append/3` | x |\n";    // 22-25

// 注釈は --> を含む行で閉じる。<!--> と <!---> も同じ行で閉じ、後ろの印を隠さない（修正 3）。区切りの中の注釈は誤り
// だが、後ろの行は読む。
void manual_parse_comment_probes() {
  const mosa::ParsedManual chapter = mosa::parse_manual(kChapter14, "c.md");
  check(chapter.errors.empty() && chapter.examples.size() == 1 && chapter.tables.size() == 2, "chapter 14: passes");
  if (chapter.tables.size() == 2) {
    check(same(chapter.tables[0].entries, {{"between", 3}, {"succ", 2}, {"plus", 3}}) &&
              same(chapter.tables[1].entries, {{"append", 3}}) && chapter.tables[1].line == 22,
          "chapter 14: both tables");
  }
  for (const std::string_view open : {"<!-->", "<!--->"}) {
    const std::string before = std::string{open} + "\n\n<!-- predicates: between -->\n" + std::string{kProbeRows};
    const mosa::ParsedManual seen = mosa::parse_manual(before, "c.md");
    check(seen.errors.empty() && seen.tables.size() == 1 && same(seen.tables.front().entries, {{"between", 3}}),
          "comments: the marker after " + std::string{open} + " is seen");
    const std::string inside = "<!-- predicates: between -->\n" + std::string{kProbeRows} + "\n" +
                               std::string{open} + "\n\n| P |\n| --- |\n| `nope/1` | x |\n\n<!-- end -->\n";
    const mosa::ParsedManual counted = mosa::parse_manual(inside, "c.md");
    check(has_error(counted, "c.md:6: ") && has_error(counted, "c.md:12: ") && counted.tables.size() == 1 &&
              same(counted.tables.front().entries, {{"between", 3}, {"nope", 1}}),
          "comments: " + std::string{open} + " in a section is an error and the row after it is counted");
  }
}

// 区切りの中の複数行の注釈、字下げのコードブロック、箇条書きの中の表は、どれも書けない行（修正 3。修正 1 では数えない
// 行として読み飛ばした）。
void manual_parse_hidden_rows() {
  constexpr std::string_view text =
      "<!-- predicates: lists -->\n"  // 1
      "| Predicate |\n"               // 2
      "| --- |\n"                     // 3
      "| `append/3` |\n"              // 4
      "\n"                            // 5
      "<!--\n"                        // 6
      "| Predicate |\n"               // 7
      "| --- |\n"                     // 8
      "| `hidden/1` |\n"              // 9
      "-->\n"                         // 10
      "\n"                            // 11
      "    | Predicate |\n"           // 12
      "    | --- |\n"                 // 13
      "    | `code/1` |\n"            // 14
      "\n"                            // 15
      "- In a list item:\n"           // 16
      "\n"                            // 17
      "    | Predicate |\n"           // 18
      "    | --- |\n"                 // 19
      "    | `length/2` |\n";         // 20
  const mosa::ParsedManual parsed = mosa::parse_manual(text, "h.md");
  check(has_error(parsed, "h.md:6: ") && has_error(parsed, "h.md:10: ") && has_error(parsed, "h.md:12: ") &&
            has_error(parsed, "h.md:16: ") && has_error(parsed, "h.md:18: "),
        "hidden rows: the comment, the indented code and the list item are errors");
}

// 見出しと | --- | の下の 4 行目に row を置いた区切り。
std::string table_with(std::string_view row) {
  return "<!-- predicates: lists -->\n| Predicate | Meaning |\n| --- | --- |\n" + std::string{row} + "\n";
}

// 本体の行はどれも | で始まり、最初の欄はちょうど 1 つのコードスパン `name/arity`。ほかは読み飛ばさずに誤り。
void manual_parse_bad_rows() {
  for (const std::string_view row : {"| `nope/1` (deprecated) | |", "| [`append/3`](#append) | |",
                                     "| **`append/3`** | |", "`append/3` | no leading pipe |",
                                     "| `between/03` | leading zero |", "| `foo` | no arity |",
                                     "| `bar/x` | arity not a number |", "| plain/1 | not a code span |",
                                     "| `a/1` `b/1` | two code spans |", "| `'a'b'/1` | bad quotes |",
                                     "| `/1` | no name |", "Prose right after the rows."}) {
    const mosa::ParsedManual parsed = mosa::parse_manual(table_with(row), "r.md");
    check(parsed.errors.size() == 1 && has_error(parsed, "r.md:4: "), "bad rows: error for " + std::string{row});
    check(parsed.tables.size() == 1 && parsed.tables.front().entries.empty(), "bad rows: not counted " +
                                                                                  std::string{row});
  }
  const mosa::ParsedManual zero = mosa::parse_manual(table_with("| `halt/0` | |\n| `atom_length/2` | |"), "r.md");
  check(zero.errors.empty() && zero.tables.size() == 1 && same(zero.tables.front().entries, {{"halt", 0},
                                                                                            {"atom_length", 2}}),
        "bad rows: arity 0 and 2 are fine");
  const mosa::ParsedManual header = mosa::parse_manual("<!-- predicates: lists -->\n| Predicate |\n\n", "r.md");
  check(has_error(header, "r.md:2: "), "bad rows: a | header without a | --- | row under it");
  const mosa::ParsedManual loose = mosa::parse_manual("<!-- predicates: lists -->\n`append/3` | joins |\n", "r.md");
  check(loose.errors.size() == 1 && has_error(loose, "r.md:2: "), "bad rows: code span and | without a leading |");
  const mosa::ParsedManual prose =
      mosa::parse_manual("<!-- predicates: lists -->\nUse `a | b` here.\nText with a|bar.\n", "r.md");
  check(prose.errors.size() == 1 && has_error(prose, "r.md:2: "), "bad rows: prose with a code span, not a plain |");
}

// 見出しの行は比べないので、最初の欄がコードスパンの見出しは誤り。頭の | があってもなくても（修正 2 の N1）。
// | --- | は | で始まる見出しの行のすぐ下だけに書ける（修正 3）。
void manual_parse_header_span() {
  for (const std::string_view header : {"| `nope/1` | Not in Mosa |", "`nope/1` | Not in Mosa |", "`nope/1`"}) {
    const std::string text = "<!-- predicates: between -->\n" + std::string{header} +
                             "\n| --- | --- |\n| `between/3` | x |\n";
    const mosa::ParsedManual parsed = mosa::parse_manual(text, "s.md");
    check(parsed.errors.size() == 1 && has_error(parsed, "s.md:2: "), "header span: error for " + std::string{header});
    check(parsed.tables.size() == 1 && same(parsed.tables.front().entries, {{"between", 3}}),
          "header span: body rows still counted for " + std::string{header});
  }
  const mosa::ParsedManual plain =
      mosa::parse_manual("<!-- predicates: between -->\nPredicates\n| --- |\n| `between/3` |\n", "s.md");
  check(plain.errors.size() == 1 && has_error(plain, "s.md:3: "), "header span: | --- | under prose");
}

// 区切りの中の注釈、区切りの線、引用はどれも書けない行で、後ろの行はまた読む（修正 2 の見本、修正 3 で誤りに）。
void manual_parse_visible_rows() {
  constexpr std::string_view trailing =
      "<!-- predicates: between -->\n"            // 1
      "| P |\n| --- |\n| `between/3` |\n"        // 2-4
      "\n"                                        // 5
      "<!-- note --> text after the comment\n"    // 6
      "\n"                                        // 7
      "| P |\n| --- |\n| `succ/2` |\n"           // 8-10
      "\n"                                        // 11
      "<!-- predicates: lists -->\n"              // 12
      "| P |\n| --- |\n| `append/3` |\n";        // 13-15
  const mosa::ParsedManual seen = mosa::parse_manual(trailing, "v.md");
  check(seen.errors.size() == 1 && has_error(seen, "v.md:6: ") && seen.tables.size() == 2,
        "visible rows: a comment in a section is an error");
  if (seen.tables.size() == 2) {
    check(same(seen.tables[0].entries, {{"between", 3}, {"succ", 2}}) &&
              same(seen.tables[1].entries, {{"append", 3}}) && seen.tables[1].line == 12,
          "visible rows: the rows and the marker after it");
  }
  constexpr std::string_view rule =
      "<!-- predicates: between -->\n| P |\n| --- |\n| `between/3` |\n\n"  // 1-5
      "* * *\n\n    | P |\n    | --- |\n    | `succ/2` |\n\n"            // 6-11
      "- - -\n\n    | `plus/3` |\n";                                      // 12-14
  const mosa::ParsedManual broken = mosa::parse_manual(rule, "v.md");
  check(has_error(broken, "v.md:6: ") && has_error(broken, "v.md:8: ") && has_error(broken, "v.md:12: ") &&
            has_error(broken, "v.md:14: ") && same(broken.tables.front().entries, {{"between", 3}}),
        "visible rows: thematic breaks and indented lines are errors");
  constexpr std::string_view quote =
      "<!-- predicates: between -->\n| P |\n| --- |\n| `between/3` |\n\n"  // 1-5
      "> A quote without a table.\n\n"                                     // 6-7
      "> | P |\n> | --- |\n> | `nope/1` |\n";                             // 8-10
  const mosa::ParsedManual quoted = mosa::parse_manual(quote, "v.md");
  check(quoted.errors.size() == 4 && has_error(quoted, "v.md:6: ") && has_error(quoted, "v.md:10: "),
        "visible rows: every quote line is an error");
  check(quoted.tables.size() == 1 && same(quoted.tables.front().entries, {{"between", 3}}),
        "visible rows: the quoted rows are not counted");
}

void manual_parse_table_errors() {
  check(has_error(mosa::parse_manual("<!-- predicates: -->\n", "e.md"), "e.md:1: "), "tables: no library name");
  check(has_error(mosa::parse_manual("<!-- predicates: a b -->\n", "e.md"), "e.md:1: "), "tables: two names");
  check(has_error(mosa::parse_manual("<!-- predicates: lists\n-->\n", "e.md"), "e.md:1: "), "tables: not closed");
  for (const std::string_view marker : {"<!-- predicate: lists -->", "<!-- Predicates: lists -->",
                                        "<!-- PREDICATES lists -->", "<!-- predicates lists -->"}) {
    const mosa::ParsedManual parsed = mosa::parse_manual(std::string{marker} + "\n", "e.md");
    check(parsed.errors.size() == 1 && has_error(parsed, "e.md:1: ") && parsed.tables.empty(),
          "tables: misspelled marker " + std::string{marker});
  }
  check(mosa::parse_manual("<!-- a note on predicates -->\n<!-- TODO -->\n", "e.md").errors.empty(),
        "tables: other comments are not markers");
  constexpr std::string_view misspelled =
      "<!-- predicates: between -->\n"  // 1
      "| P |\n| --- |\n"                // 2, 3
      "| `between/3` |\n"               // 4
      "\n"                              // 5
      "<!-- predicate: lists -->\n"     // 6
      "| P |\n| --- |\n"                // 7, 8
      "| `append/3` |\n";               // 9
  const mosa::ParsedManual parsed = mosa::parse_manual(misspelled, "e.md");
  check(parsed.tables.size() == 1 && same(parsed.tables.front().entries, {{"between", 3}}),
        "tables: rows after a misspelled marker belong to no section");
  constexpr std::string_view twice =
      "<!-- predicates: between -->\n"  // 1
      "| P |\n| --- |\n"                // 2, 3
      "| `between/3` | |\n"             // 4
      "# Other\n"                       // 5
      "<!-- predicates: system -->\n"   // 6
      "| P |\n| --- |\n"                // 7, 8
      "| `between/3` | |\n"             // 9
      "<!-- predicates: between -->\n"  // 10
      "| P |\n| --- |\n"                // 11, 12
      "| `succ/2` | |\n"                // 13
      "| `between/3` | |\n";            // 14
  const mosa::ParsedManual repeated = mosa::parse_manual(twice, "d.md");
  check(repeated.errors.size() == 1 && has_error(repeated, "d.md:14: "),
        "tables: the same entry twice for one name in a file, other names apart");
}

bool has_indicator(const std::vector<mosa::PredicateIndicator>& all, std::string_view name, int arity) {
  for (const mosa::PredicateIndicator& item : all) {
    if (item.name == name && item.arity == arity) return true;
  }
  return false;
}

// 引用した原子にする。名前に ' と \ があっても読み戻せる。std::quoted と ADL でぶつからない名前にする。
std::string quoted_name(std::string_view name) {
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
    const std::string goal = "functor(H, " + quoted_name(item.name) + ", " + std::to_string(item.arity) +
                             "), predicate_property(H, built_in).";
    const mosa::Stop stop = machine->query(goal);
    check(stop.kind == mosa::StopKind::Success, "builtins: built_in holds for " + item.name);
  }
}

// mosatest の隣の lib/。mosatest --manual が mosa の隣の lib/ を読むのと同じ置き方（組み上げで写す）。
std::filesystem::path beside_lib() {
  const std::optional<std::filesystem::path> self = mosa::executable_path();
  return self ? self->parent_path() / "lib" : std::filesystem::path{"lib"};
}

void manual_library_exports() {
  const std::filesystem::path lib = beside_lib();
  const mosa::LibraryExports between = mosa::library_exports("between", lib);
  check(between.exports && same(*between.exports, {{"between", 3}, {"succ", 2}, {"plus", 3}}),
        "exports: library(between)");
  const mosa::LibraryExports lists = mosa::library_exports("lists", lib);
  check(lists.exports && has_indicator(*lists.exports, "length", 2) && has_indicator(*lists.exports, "append", 3),
        "exports: library(lists)");
  const mosa::LibraryExports japanese = mosa::library_exports("japanese", lib);
  check(japanese.exports && has_indicator(*japanese.exports, "ひらがな_カタカナ", 2), "exports: library(japanese)");
  for (const std::string_view name : {"apply", "format", "strings"}) {
    check(mosa::library_exports(name, lib).exports.has_value(), "exports: library " + std::string{name});
  }
  for (const std::string_view name : {"system", "nope"}) {
    const mosa::LibraryExports none = mosa::library_exports(name, lib);
    check(!none.exports && !none.error.empty(), "exports: none, with a reason, for " + std::string{name});
  }
  const std::filesystem::path bad = std::filesystem::path{MOSA_SOURCE_DIR} / "tests" / "manual" / "bad-lib" / "lib";
  for (const std::string_view name : {"broken", "nomodule", "renamed"}) {
    const mosa::LibraryExports none = mosa::library_exports(name, bad);
    check(!none.exports && !none.error.empty(), "exports: unloadable " + std::string{name});
  }
  check(!mosa::library_exports("between", bad).exports, "exports: only the given directory is searched");
}

void manual_libraries_and_version() {
  const auto names = mosa::manual_libraries(beside_lib());
  check(names == std::vector<std::string>{"apply", "between", "format", "japanese", "lists", "strings"},
        "libraries: the *.pl beside mosatest without messages_*");
  check(!mosa::manual_libraries(beside_lib() / "nope").has_value(), "libraries: unreadable directory");
  const std::string line = mosa::version_line();
  check(mosa::same_version(line + "\n") && mosa::same_version(line + "\r\n") && mosa::same_version(line),
        "version: the same line");
  check(!mosa::same_version("cmake version 3.31.0\n") && !mosa::same_version("") &&
            !mosa::same_version(line + "\nmore\n") && !mosa::same_version(line + "-dev\n"),
        "version: another line");
}

void manual_args() {
  const std::vector<std::string_view> all{"docs/manual", "--all-tables", "--mosa", "build/mosa"};
  const mosa::ManualArgs parsed = mosa::parse_manual_args(all);
  check(parsed.ok && parsed.all_tables && parsed.dir == "docs/manual" && parsed.mosa == "build/mosa",
        "args: --all-tables with --mosa");
  const std::vector<std::string_view> plain{"docs/manual"};
  check(mosa::parse_manual_args(plain).ok && !mosa::parse_manual_args(plain).all_tables, "args: no --all-tables");
  const std::vector<std::string_view> bare{"--all-tables"};
  check(!mosa::parse_manual_args(bare).ok, "args: --all-tables without a directory");
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
  manual_parse_section_lines();
  manual_parse_probes();
  manual_parse_comment_probes();
  manual_parse_hidden_rows();
  manual_parse_bad_rows();
  manual_parse_header_span();
  manual_parse_visible_rows();
  manual_parse_table_errors();
  manual_builtin_indicators();
  manual_library_exports();
  manual_libraries_and_version();
  manual_args();
  return mosa_test::failures() == 0 ? 0 : 1;
}
