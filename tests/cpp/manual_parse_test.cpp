// mosatest --manual の印の読み取り（リリース 0.1.0 Task 4）。印の 3 種、複数行の入力、CRLF、印の無い囲い、
// 既定の入力、console の区切りと引用を確かめる。走らせて比べるところは tests/manual の見本（tests.cmake）が見る。
// 述語の表の読み取りも見る（Task 5）。表と比べる処理系の一覧（組込と library の公開リスト）は manual_lists_test.cpp、
// 生の HTML のタグは manual_html_test.cpp。

#include "harness.hpp"
#include "mosa/manual.hpp"

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
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

bool same_examples(const mosa::ParsedManual& left, const mosa::ParsedManual& right) {
  bool equal = left.examples.size() == right.examples.size();
  for (std::size_t index = 0; equal && index < left.examples.size(); ++index) {
    equal = same(left.examples[index], right.examples[index]);
  }
  return equal;
}

// \n の付かない \r も CommonMark と同じく行の終わり（修正 4）。
void manual_parse_crlf() {
  const mosa::ParsedManual plain = mosa::parse_manual(kMarkers, "f.md");
  const mosa::ParsedManual crlf = mosa::parse_manual(with_crlf(kMarkers), "f.md");
  check(crlf.errors.empty(), "crlf: no errors");
  check(same_examples(plain, crlf), "crlf: same examples as LF");
  std::string lone{kMarkers};
  std::ranges::replace(lone, '\n', '\r');
  const mosa::ParsedManual cr = mosa::parse_manual(lone, "f.md");
  check(cr.errors.empty() && same_examples(plain, cr), "lone cr: same examples as LF");
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

// 検査器が読まない囲い（引用の中、タブの後ろ、箇条書きの印と同じ行、Task 5 の修正 5 から字下げも）は黙って飛ばさず
// 誤りにする（修正 6）。
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
  check(spaces.errors.size() == 1 && has_error(spaces, "s.md:4: ") && spaces.examples.empty(),
        "unreadable: a fence indented with spaces in a list");
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

// 述語の表（リリース 0.1.0 Task 5）。区切りは印から次の 0 桁目の ATX の見出しまでで、中に書けるのは表だけ（修正 3、
// 修正 4 で地の文も外へ）。文は印の前に書く。区切りの外の囲いの中の行は、見出しにも印にもならない。
constexpr std::string_view kTables =
    "# Reference\n"                                // 1
    "Prose above the marker, with `code`.\n"       // 2
    "2 more words; 日本語の地の文も書ける。\n"      // 3
    "<!-- predicates: between -->\n"               // 4
    "| Predicate | Meaning |\n"                    // 5
    "| --- | --- |\n"                              // 6
    "| `between/3` | plain |\n"                    // 7
    "|`','/2`| quoted, no spaces |\n"              // 8
    "| `'it''s'/0` | doubled quote |\n"            // 9
    "| `=../2` | symbol char |\n"                  // 10
    "| ` \\+/1 ` | backslash unquoted, padded |\n" // 11
    "| `ひらがな_カタカナ/2` | Japanese name |\n"   // 12
    "| `</2` | `X < Y` in a code span |\n"         // 13
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
  check(first.library == "between" && first.line == 4, "tables: first section and its marker line");
  check(same(first.entries, {{"between", 3}, {",", 2}, {"it's", 0}, {"=..", 2}, {"\\+", 1}, {"ひらがな_カタカナ", 2},
                             {"<", 2}}),
        "tables: entries of the first section, quotes removed");
  const mosa::ManualTable& second = parsed.tables[1];
  check(second.library == "system" && second.line == 21, "tables: marker without spaces, not the one in the block");
  check(same(second.entries, {{"atom", 1}}), "tables: code block lines are neither headings nor rows");
  check(parsed.examples.size() == 1, "tables: the marked code block is still an example");
}

// 区切りの中の 1 行で、書けない種類のもの。どれも 1 つの誤りで、何も数えない（修正 3 の決定）。修正 4 で地の文、
// 空白だけの行、< を持つ見出しの行、| だけの行（欄が 0 で | --- | と数が合わない）も誤り。タブは manual_parse_control。
void manual_parse_section_lines() {
  for (const std::string_view line :
       {"    | `code/1` |", " | `space/1` |", "- In a list item", "* item", "+ item", "1. item",
        "2) item", "> A quote", "* * *", "- - -", "___", "===", "---", "<!-- a note -->", "<!--", "<!-->", "<div>",
        "~~~", "```text\n| `in/1` |\n```", "#hashtag", "`code` first", "Prose with `code` in it.",
        "| `nope/1` | a code span in the header position |", "| --- |", "  ## indented heading", "Plain prose.",
        "2026 was a year.", "   ", "Name | Meaning", ":--- | :---", "[x]: /url '", "Text <!--", "</div>",
        "| <code>a/1</code> |", "|"}) {
    const std::string text = "<!-- predicates: lists -->\n" + std::string{line} + "\n";
    const mosa::ParsedManual parsed = mosa::parse_manual(text, "w.md");
    check(parsed.errors.size() == 1 && has_error(parsed, "w.md:2: "), "section: error for " + std::string{line});
    check(parsed.tables.size() == 1 && parsed.tables.front().entries.empty(), "section: nothing counted for " +
                                                                                    std::string{line});
  }
}

// 再レビューの見本（rr2/probes と rr3/p）。区切りの中の箇条書き、字下げの続きの行、太字の見出し、区切りの線、引用、
// 注釈は誤りで、章 14 の形は通る。first は最初の誤りの行、entries は数えた行。誤りの後ろの行は数えることがあるが、
// 誤りが 1 つでもあればその章は通らない。
struct Probe {
  std::string_view name;
  std::string_view text;
  std::size_t first = 0;
  std::vector<mosa::PredicateIndicator> entries;
};

constexpr std::string_view kProbeRows = "| Predicate | Meaning |\n| --- | --- |\n| `between/3` | x |\n";

// rr3/p の見本（修正 4）。区切りに書けるのは表だけなので、: で始まる | の無い表とその下の HTML、注釈やリンクの定義の
// 中に隠れた表は誤り。欄の数が | --- | と違う見出し、| だけの見出し、<code> の見出し、\f と \v の行も誤り。
// \n の付かない \r は行を終え（GitHub は次の行を表の行として見せる）、その行は誤り。
std::vector<Probe> rr3_probes(std::string_view marker) {
  static const std::string rows = std::string{marker} + std::string{kProbeRows} + "\n";
  static const std::string colon = rows + "Name | Meaning\n:--- | :---\nnope/1 | not in Mosa\n";
  static const std::string colon_html = rows + "Name | Meaning\n:--- | :---\nsee | <code>nope/1</code>\n";
  static const std::string lonecr = std::string{marker} + "| Predicate | Meaning |\n| --- | --- |\n"
                                    "| `between/3` | x |\r| `nope/1` | y |\n| `succ/2` | x |\n";
  static const std::string crcrlf = with_crlf(std::string{marker} + std::string{kProbeRows}) +
                                    "\r\r\n| `nope/1` | x |\r\n";
  static const std::string colmismatch = std::string{marker} + "| Predicate | Meaning |\n| --- |\n"
                                         "| `between/3` | x |\n";
  static const std::string baronly = std::string{marker} + "|\n| --- |\n| `between/3` | x |\n";
  static const std::string comment = rows + "Text <!--\n| P | Q |\n| --- |\n| `succ/2` | x | -->\n";
  static const std::string linkref = rows + "[x]: /url '\n| P | Q |\n| --- |\n| `succ/2` | x |'\n";
  static const std::string ff_delim = rows + "Name | Meaning\n\f--- | ---\nnope/1 | x\n";
  static const std::string vt_delim = rows + "Name | Meaning\n\v--- | ---\nnope/1 | x\n";
  static const std::string htmlhead = std::string{marker} + "| <code>nope/1</code> | Not in Mosa |\n| --- | --- |\n"
                                      "| `between/3` | x |\n";
  return {{"colon", colon, 6, {{"between", 3}}},
          {"colon_html", colon_html, 6, {{"between", 3}}},
          {"lonecr", lonecr, 4, {{"between", 3}, {"nope", 1}, {"succ", 2}}},
          {"crcrlf", crcrlf, 5, {{"between", 3}}},
          {"colmismatch", colmismatch, 2, {{"between", 3}}},
          {"baronly", baronly, 2, {{"between", 3}}},
          {"inlinecomment", comment, 6, {{"between", 3}, {"succ", 2}}},
          {"linkref", linkref, 6, {{"between", 3}, {"succ", 2}}},
          {"ff_delim", ff_delim, 6, {{"between", 3}}},
          {"vt_delim", vt_delim, 6, {{"between", 3}}},
          {"htmlhead", htmlhead, 2, {{"between", 3}}}};
}

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
  // contline の最初の誤りは、修正 4 から地の文の 2 行目（字下げの続きの行は 3 行目）。
  std::vector<Probe> all{{"boldhead", boldhead, 2, {{"between", 3}, {"succ", 2}}},
                         {"contline", contline, 2, {{"between", 3}}},
                         {"listhead", listhead, 2, {}},
                         {"listhead2", listhead2, 2, {}},
                         {"listok", listok, 2, {}},
                         {"listbreak", listbreak, 6, {{"between", 3}}},
                         {"thematic3", thematic, 6, {{"between", 3}}},
                         {"listquote", listquote, 6, {{"between", 3}}},
                         {"sentence_after", sentence, 2, {{"between", 3}}}};
  std::ranges::move(rr3_probes(marker), std::back_inserter(all));
  return all;
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
// コードスパンの外の < も誤り（修正 4）。GFM は欄を分けてからコードスパンを読むので、| をまたぐ ` は閉じない。
void manual_parse_bad_rows() {
  for (const std::string_view row : {"| `nope/1` (deprecated) | |", "| [`append/3`](#append) | |",
                                     "| **`append/3`** | |", "`append/3` | no leading pipe |",
                                     "| `between/03` | leading zero |", "| `foo` | no arity |",
                                     "| `bar/x` | arity not a number |", "| plain/1 | not a code span |",
                                     "| `a/1` `b/1` | two code spans |", "| `'a'b'/1` | bad quotes |",
                                     "| `/1` | no name |", "Prose right after the rows.", "| `a/1` | x < y |",
                                     "| `a/1` | `x | <b>y</b>` |", "| `a/1` | \\`x < y` |", "| `a/1` | <!-- c --> |",
                                     "| `a/1` | `a`` <i>y</i> |"}) {
    const mosa::ParsedManual parsed = mosa::parse_manual(table_with(row), "r.md");
    check(parsed.errors.size() == 1 && has_error(parsed, "r.md:4: "), "bad rows: error for " + std::string{row});
    check(parsed.tables.size() == 1 && parsed.tables.front().entries.empty(), "bad rows: not counted " +
                                                                                  std::string{row});
  }
  const mosa::ParsedManual zero = mosa::parse_manual(table_with("| `halt/0` | |\n| `atom_length/2` | |"), "r.md");
  check(zero.errors.empty() && zero.tables.size() == 1 && same(zero.tables.front().entries, {{"halt", 0},
                                                                                            {"atom_length", 2}}),
        "bad rows: arity 0 and 2 are fine");
  const mosa::ParsedManual angle = mosa::parse_manual(
      table_with("| `=</2` | `X =< Y` |\n| `a/1` | ``x`<`` and \\| bar |\n| `b/1` | ``x` <i>y</i>` |"), "r.md");
  check(angle.errors.empty() && angle.tables.size() == 1 &&
            same(angle.tables.front().entries, {{"=<", 2}, {"a", 1}, {"b", 1}}),
        "bad rows: < inside code spans is fine");
  const mosa::ParsedManual header = mosa::parse_manual("<!-- predicates: lists -->\n| Predicate |\n\n", "r.md");
  check(has_error(header, "r.md:2: "), "bad rows: a | header without a | --- | row under it");
  const mosa::ParsedManual loose = mosa::parse_manual("<!-- predicates: lists -->\n`append/3` | joins |\n", "r.md");
  check(loose.errors.size() == 1 && has_error(loose, "r.md:2: "), "bad rows: code span and | without a leading |");
  const mosa::ParsedManual prose =
      mosa::parse_manual("<!-- predicates: lists -->\nUse `a | b` here.\nText with a|bar.\n", "r.md");
  check(prose.errors.size() == 2 && has_error(prose, "r.md:2: ") && has_error(prose, "r.md:3: "),
        "bad rows: prose is an error with or without a code span");
}

// 見出しの行と | --- | は欄の数が同じ（違えば GFM は表にしない。rr3 の colmismatch と baronly）。欄は \ の付かない |
// で分け、最後の | の後ろが空白だけなら欄にしない（修正 4）。
void manual_parse_cell_counts() {
  using Rows = std::initializer_list<std::pair<std::string_view, std::string_view>>;
  const auto parse = [](std::string_view header, std::string_view delimiter) {
    return mosa::parse_manual("<!-- predicates: between -->\n" + std::string{header} + "\n" + std::string{delimiter} +
                                  "\n| `between/3` | x |\n", "n.md");
  };
  for (const auto& [header, delimiter] : Rows{{"| P | Q |", "|---|:---:|"}, {"| a \\| b | c |", "| --- | --- |"},
                                              {"| P ||", "| --- | --- |"}, {"| P | Q |  ", "| --- | --- |  "},
                                              {"| P", "| --- |"}, {"| a \\\\| b |", "| --- |"}}) {
    const mosa::ParsedManual parsed = parse(header, delimiter);
    check(parsed.errors.empty() && parsed.tables.size() == 1 && same(parsed.tables.front().entries, {{"between", 3}}),
          "cells: same count for " + std::string{header});
  }
  for (const auto& [header, delimiter] : Rows{{"| P | Q |", "| --- |"}, {"| a \\| b |", "| --- | --- |"},
                                              {"|", "| --- |"}, {"| P |", "|---|---|"}, {"| P | |", "| --- |"}}) {
    const mosa::ParsedManual parsed = parse(header, delimiter);
    check(parsed.errors.size() == 1 && has_error(parsed, "n.md:2: "), "cells: different count for " +
                                                                          std::string{header});
  }
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
  check(plain.errors.size() == 1 && has_error(plain, "s.md:2: "), "header span: prose above | --- | (修正 4)");
}

// 区切りの行に残った 0x20 未満の字（タブ、\f、\v、NUL、ESC）は誤り。\n の付かない \r は行を終え、区切りの中ではその
// 行も誤り（rr3 の lonecr と crcrlf は manual_parse_probes）。区切りの外の \r も行を終えるので、後ろの印は隠れない。
void manual_parse_control() {
  std::string nul = "| `between/3` | x |";
  nul += '\0';
  nul += "| `nope/1` |";
  for (const auto& [name, row] : std::initializer_list<std::pair<std::string_view, std::string>>{
           {"tab", "\t| `tab/1` |"}, {"trailing tab", "| `between/3` | x |\t"}, {"form feed", "| `between/3` | x\f |"},
           {"vertical tab", "\v| `between/3` | x |"}, {"nul", nul}, {"escape", "| `between/3` | \x1b[1mx |"},
           {"lone cr", "| `between/3` | x |\r| `succ/2` | x |"}}) {
    const mosa::ParsedManual parsed = mosa::parse_manual(table_with(row), "k.md");
    check(has_error(parsed, "k.md:4: "), "control: " + std::string{name});
    // 文面に出す行の制御文字は ? にする。NUL や ESC を端末に流さない。
    check(std::ranges::none_of(parsed.errors, [](const std::string& error) {
            return std::ranges::any_of(error, [](char letter) { return static_cast<unsigned char>(letter) < 0x20; });
          }), "control: no control character in the messages for " + std::string{name});
  }
  const mosa::ParsedManual tab = mosa::parse_manual("<!-- predicates: lists -->\n\t| `tab/1` |\n", "k.md");
  check(!tab.errors.empty() && has_error(tab, "k.md:2: ") && tab.tables.front().entries.empty(),
        "control: a tab-led line under the marker");
  const std::string after_cr = "Text\r<!-- predicates: between -->\n" + std::string{kProbeRows};
  const mosa::ParsedManual outside = mosa::parse_manual(after_cr, "k.md");
  check(outside.errors.empty() && outside.tables.size() == 1 && outside.tables.front().line == 2 &&
            same(outside.tables.front().entries, {{"between", 3}}),
        "control: a lone CR outside a section ends the line, and the marker after it is seen");
  std::string prose = "Tab\there, a form feed \f, a NUL ";
  prose += '\0';
  prose += " outside a section.\n";
  check(mosa::parse_manual(prose, "k.md").errors.empty(), "control: only the lines of a section");
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

// 囲いの行は開きも閉じも 0 桁目だけ（Task 5 の修正 5）。字下げした囲いは箇条書きの中でも誤りで、中の行は読まない。
// GitHub は字下げした囲いを文の続き、字下げのコード、箇条書きの中の囲いのどれとしても読み、中の行や後ろの表を検査器と
// 違う形で映す（rr4 の fence4、fence4t、listfence、closeindent）。字下げした閉じの行は誤りにして、そこで閉じる。
void manual_parse_fence_column() {
  const std::string rows = "\n<!-- predicates: between -->\n| P |\n| --- |\n| `between/3` |\n";
  struct Case {
    std::string_view name;
    std::string_view text;
    std::size_t line = 0;
  };
  for (const Case& item : {
           Case{"fence4", "# R\n\nThe output is:\n    <!-- skip: x -->\n    ```text\n    <details>\n    ```\n", 5},
           Case{"fence4t", "# R\n\nThe output is:\n    <!-- skip: x -->\n    ~~~text\n    <details>\n    ~~~\n", 5},
           Case{"listfence", "# R\n\n1. Run:\n\n   <!-- skip: x -->\n   ```text\n   ?- X = 1.\nX = 1.\n   ```\n", 6},
           Case{"closeindent", "# R\n\n<!-- skip: x -->\n```text\nhello\n    ```\n", 6},
           Case{"one space", "<!-- skip: x -->\n ```text\nx\n ```\n", 2},
           Case{"closing line indented", "<!-- check: x -->\n```text\n?- x.\n  ```\n", 4},
       }) {
    const mosa::ParsedManual parsed = mosa::parse_manual(std::string{item.text} + rows, "f.md");
    const std::string name{item.name};
    check(parsed.errors.size() == 1 && has_error(parsed, "f.md:" + std::to_string(item.line) + ": "),
          "fence column: " + name);
    check(parsed.tables.size() == 1 && same(parsed.tables.front().entries, {{"between", 3}}),
          "fence column: the section after " + name);
  }
}

// rr4 の opencomment と markerclose。複数行の注釈を閉じる行の印は、GitHub では注釈の中に隠れる。
constexpr std::string_view kOpenComment =
    "# Reference\n\nThe library `between` enumerates integers, and `X<Y` stays code.\n\n"  // 1-4
    "<!-- predicates: between -->\n| Predicate | Meaning |\n| --- | --- |\n"           // 5-7
    "| `between/3` | `between(Low, High, X)` |\n| `succ/2` | successor |\n"            // 8, 9
    "| `plus/3` | addition |\n\n## More\n\n"                                          // 10-13
    "<!-- TODO: plus/3 is listed above\n<!-- predicates: between -->\n"               // 14, 15
    "| Predicate | Meaning |\n| --- | --- |\n| `nope/1` | x |\n";                    // 16-18
constexpr std::string_view kMarkerClose =
    "# Reference\n\n<!-- predicates: between -->\n| Predicate | Meaning |\n| --- | --- |\n"  // 1-5
    "| `between/3` | `between(Low, High, X)` |\n| `succ/2` | successor |\n"               // 6, 7
    "| `plus/3` | addition |\n\n## More\n\n"                                             // 8-11
    "<!-- note\n<!-- skip: x -->\n```text\ny\n```\n\n"                                   // 12-17
    "<!-- predicates: between -->\n| Predicate | Meaning |\n| --- | --- |\n| `nope/1` | x |\n";  // 18-21

// 複数行の注釈を閉じる行の印は誤り（修正 5）。読み飛ばすと表が比べられずに映り（opencomment）、見えない印で例を読む
// （markerclose）。誤りの後は印として読み、印の無い囲いの誤りを重ねない。書き違えて閉じない印の注釈を閉じる行の印、
// 複数行の check: の入力の中の -->（GitHub はそこで注釈を閉じる。DCG）も誤り。
void manual_parse_comment_markers() {
  const mosa::ParsedManual open = mosa::parse_manual(kOpenComment, "c.md");
  check(open.errors.size() == 1 && has_error(open, "c.md:15: ") && open.tables.size() == 2,
        "comment markers: opencomment");
  const mosa::ParsedManual close = mosa::parse_manual(kMarkerClose, "c.md");
  check(close.errors.size() == 1 && has_error(close, "c.md:13: ") && close.examples.size() == 1 &&
            close.tables.size() == 2,
        "comment markers: markerclose");
  const mosa::ParsedManual tail = mosa::parse_manual("<!-- note\nend --> <!-- predicates: lists -->\n", "c.md");
  check(tail.errors.size() == 1 && has_error(tail, "c.md:2: "), "comment markers: after the end of the comment");
  const mosa::ParsedManual own = mosa::parse_manual("<!-- skip: a\n<!-- check: b -->\n```text\n?- b.\n```\n", "c.md");
  check(own.errors.size() == 2 && has_error(own, "c.md:1: ") && has_error(own, "c.md:2: "),
        "comment markers: the line that closes a bad marker");
  check(mosa::parse_manual("<!-- note\nmore -->\n<!-- skip: x -->\n```text\ny\n```\n", "c.md").errors.empty(),
        "comment markers: a marker after the comment is fine");
  const mosa::ParsedManual dcg =
      mosa::parse_manual("<!-- check: g\ngreeting --> [hello].\n-->\n```text\n?- a.\n```\n", "c.md");
  check(dcg.errors.size() == 1 && has_error(dcg, "c.md:2: "), "comment markers: --> inside a multi-line input");
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
  manual_parse_cell_counts();
  manual_parse_header_span();
  manual_parse_control();
  manual_parse_visible_rows();
  manual_parse_table_errors();
  manual_parse_fence_column();
  manual_parse_comment_markers();
  return mosa_test::failures() == 0 ? 0 : 1;
}
