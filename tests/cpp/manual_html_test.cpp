// mosatest --manual の生の HTML のタグ（リリース 0.1.0 Task 5 の修正 4 とその追加）。predicates: の印を持つ章では、
// < の後ろに英字、/、?、! が続くタグ（<!-- を除く）はどの行でも誤り。除くのはコードスパン、囲い、<!-- … --> の注釈の
// 中だけで、どこまでがそうかは GitHub と同じに読む。誤りとした形は、どれも GitHub の /markdown でタグが映ることを
// 確かめた（listcomment だけは閉じない注釈が後ろを隠すが、どちらにも読めるので誤り）。manual_parse_test.cpp から
// 分けた。

#include "harness.hpp"
#include "mosa/manual.hpp"

#include <cstddef>
#include <initializer_list>
#include <string>
#include <string_view>

namespace {

using mosa_test::check;

constexpr std::string_view kProbeRows = "| Predicate | Meaning |\n| --- | --- |\n| `between/3` | x |\n";

bool has_error(const mosa::ParsedManual& parsed, std::string_view prefix) {
  for (const std::string& error : parsed.errors) {
    if (error.starts_with(prefix)) return true;
  }
  return false;
}

// text の後ろに空の行と区切りを 1 つ置く。タグの誤りは区切りを持つ章でだけ出る。
std::string with_section(std::string_view text) {
  return std::string{text} + "\n<!-- predicates: between -->\n" + std::string{kProbeRows};
}

// 0 桁目から 3 つまでの空白の後ろのタグ（修正 4 の決定）。<details> と </details> で囲んだ区切りも誤り。
void manual_html_line_start() {
  const std::string details = "<!-- predicates: between -->\n" + std::string{kProbeRows} +
                              "\n## hidden\n\n<details>\n<!-- predicates: between -->\n| P |\n| --- |\n"
                              "| `succ/2` | x |\n# x\n</details>\n";
  const mosa::ParsedManual hidden = mosa::parse_manual(details, "h.md");
  check(hidden.errors.size() == 2 && has_error(hidden, "h.md:8: ") && has_error(hidden, "h.md:14: "),
        "raw html: <details> and </details> around a section");
  for (const std::string_view open : {"<details>", "<script>", "<style>", "<table><tr><td>", "</div>", "  <details>",
                                      "   <pre>", "<?php", "<!DOCTYPE html>", "<![CDATA[", "<DIV>"}) {
    const std::string text = std::string{open} + "\n<!-- predicates: between -->\n" + std::string{kProbeRows} +
                             "\n| `nope/1` |\n";
    const mosa::ParsedManual parsed = mosa::parse_manual(text, "h.md");
    check(parsed.errors.size() == 2 && parsed.errors.front().starts_with("h.md:1: ") && has_error(parsed, "h.md:7: "),
          "raw html: " + std::string{open} + ", in line order");
  }
  check(mosa::parse_manual("<details>\n\n| P |\n| --- |\n| `succ/2` |\n</details>\n", "h.md").errors.empty(),
        "raw html: a chapter without a marker may use HTML");
}

struct Shape {
  std::string_view name;
  std::string_view text;
  std::size_t line = 0;  // タグの行（1 から）
};

// 行のどこにあってもタグは誤り（修正 4 の追加）。コードスパンや注釈に見えても、GitHub の読み方ではその外にある形。
void manual_html_any_line() {
  for (const Shape& shape : {
           Shape{"text", "Text <details>", 1},
           Shape{"after a comment", "<!-- x --><details>", 1},
           Shape{"indented code", "    <div> code", 1},
           Shape{"code span across lines", "x `a\nb`<details>`c`", 2},
           Shape{"cells split first", "| `a | <details> | b` |", 1},
           Shape{"after a comment block", "<!-- a --> `<details>`", 1},
           Shape{"no escape in a comment block", "<!-- a --> \\<details>", 1},
           Shape{"--!> ends a comment block", "<!-- a --!> <details>\n-->", 1},
           Shape{"--!> ends an inline comment", "Text <!-- a --!> <details> -->", 1},
           Shape{"inline comment across lines", "Text <!-- b\n`a --> <details>`", 2},
           Shape{"link destination", "[a](`) <details>`", 1},
           Shape{"link title", "[a](b \"`\") <details>`", 1},
           Shape{"link title across lines", "[a](b \"x\n`y\") <details>`", 2},
           Shape{"reference label", "[a][`] <details>`\n\n[`]: /u", 1},
           Shape{"indented comment", "    <!-- x\n<details>\n-->", 2},
           Shape{"listcomment", "- item\n  <!-- x\n<details>\n-->", 3},
           Shape{"marker tail", "<!-- skip: a--><details>-->\n```text\nx\n```", 1},
       }) {
    const mosa::ParsedManual parsed = mosa::parse_manual(with_section(shape.text), "t.md");
    const std::string prefix = "t.md:" + std::to_string(shape.line) + ": ";
    check(parsed.errors.size() == 1 && has_error(parsed, prefix), "any line: " + std::string{shape.name});
  }
  const std::string heading = "<!-- predicates: between -->\n" + std::string{kProbeRows} + "# Next <details>\n";
  const mosa::ParsedManual closed = mosa::parse_manual(heading, "t.md");
  check(closed.errors.size() == 1 && has_error(closed, "t.md:5: "), "any line: the heading that ends a section");
  const std::string cell = "<!-- predicates: between -->\n| P | Q |\n| --- | --- |\n| `between/3` | [x](`) <b>` |\n";
  const mosa::ParsedManual linked = mosa::parse_manual(cell, "t.md");
  check(linked.errors.size() == 1 && has_error(linked, "t.md:4: "), "any line: a link takes the backtick in a row");
}

// 誤りにしない形。コードスパン、\ で逃がした <、行の中で閉じる注釈、注釈のブロック、囲いの中、< と空白。
void manual_html_exempt() {
  for (const std::string_view text : {
           "\\<details> is escaped", "`<details>` in a code span", "Text <!-- <details> --> after",
           "Text <!-- a -- <details> -->", "`X<Y` and `=</2` compare.", "See [lists](lists.md) and `X<Y`.",
           "[a](b) `<details>`", "[a](b \"t\") `<i>` and [c][d] `<i>`", "<!-- a note -->\n<!--\n<div>\n-->",
           "< 3 is not a tag", "<!-- check: a\n?- X = '<b>'.\n-->\n```text\n?- a.\n```",
           "<!-- skip: html -->\n```html\n<div>\n```",
       }) {
    const mosa::ParsedManual parsed = mosa::parse_manual(with_section(text), "e.md");
    check(parsed.errors.empty() && parsed.tables.size() == 1, "exempt: " + std::string{text});
  }
  const std::string cell = "<!-- predicates: between -->\n| P | Q |\n| --- | --- |\n"
                           "| `between/3` | see [x](y.md) and `X<Y` |\n";
  check(mosa::parse_manual(cell, "e.md").errors.empty(), "exempt: a link before a code span in a row");
}

// 段落の中の注釈は - の直後の -->（--->）では閉じない（修正 5、rr4 の dashend、dashlater、dashmore、dashvar）。
// GitHub はその後ろの --> を探し、無ければ <!-- を字のまま映すので、中のタグが映る。
void manual_html_dash_close() {
  const mosa::ParsedManual end = mosa::parse_manual(with_section("Text <!-- <details> --->"), "d.md");
  check(end.errors.size() == 1 && has_error(end, "d.md:1: "), "dash close: dashend");
  constexpr std::string_view later =
      "G <!-- <i>g</i> ---> h --> <i>i</i>\n\nPara\n    <!-- <i>j</i> --->\n\nK <!--- note <i>k</i> --->";
  const mosa::ParsedManual seen = mosa::parse_manual(with_section(later), "d.md");
  check(seen.errors.size() == 3 && has_error(seen, "d.md:1: ") && has_error(seen, "d.md:4: ") &&
            has_error(seen, "d.md:6: "),
        "dash close: dashlater");
  constexpr std::string_view more =
      "A <!-- <i>a</i> --- b -->\n\nB <!-- <i>b</i> -- > c -->\n\nC <!-- <i>c</i> ---->\n\n"
      "D <!-- <i>d</i> -x-->\n\nE <!--<i>e</i>--->\n\nF <!-- <i>f</i> -->-->";
  const mosa::ParsedManual dashes = mosa::parse_manual(with_section(more), "d.md");
  check(dashes.errors.size() == 2 && has_error(dashes, "d.md:5: ") && has_error(dashes, "d.md:9: "),
        "dash close: dashmore");
  const mosa::ParsedManual var =
      mosa::parse_manual(with_section("Text <!-- <details> - -->\nText <!-- <i>x</i>--->"), "d.md");
  check(var.errors.size() == 1 && has_error(var, "d.md:2: "), "dash close: dashvar");
}

}  // namespace

int main() {
  manual_html_line_start();
  manual_html_any_line();
  manual_html_exempt();
  manual_html_dash_close();
  return mosa_test::failures() == 0 ? 0 : 1;
}
