// 計画 Task 11。docs/agreement.json と docs/agreement.md。JSON の上の鍵は shields.io の endpoint 形式（schemaVersion、
// label、message、color）で、directories にディレクトリごとの数を持つ。tests/pl と tests/conformance は別の CTest が
// 別々に書くので、既存の JSON を読んで同じ label の欄だけを置き換える。読むのは自分が書いた形（文字列、整数、入れ子の
// オブジェクト）だけで、形が違えば捨てて書き直す。どちらも生成物で、手で直さない（.ja.md も置かない）。

#include "diff.hpp"
#include "percent.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <functional>
#include <iterator>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mosa {
namespace {

struct Section {
  std::string label;
  DiffStats stats;
};

class JsonReader {
 public:
  explicit JsonReader(std::string_view text) : text_(text) {}
  // { "directories": { label: {...}, ... }, ... } を sections に足す。
  [[nodiscard]] bool read(std::vector<Section>& sections);

 private:
  using Field = std::function<bool(const std::string&)>;
  void skip() {
    while (at_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[at_])) != 0) ++at_;
  }
  bool peek(char c) {
    skip();
    return at_ < text_.size() && text_[at_] == c;
  }
  bool take(char c) {
    if (!peek(c)) return false;
    ++at_;
    return true;
  }
  std::optional<std::string> string();
  std::optional<long long> number();
  bool skip_value();
  bool object(const Field& field);
  bool read_stats(DiffStats& stats);

  std::string_view text_;
  std::size_t at_ = 0;
};

std::optional<std::string> JsonReader::string() {
  if (!take('"')) return std::nullopt;
  std::string out;
  while (at_ < text_.size() && text_[at_] != '"') {
    if (text_[at_] == '\\' && at_ + 1 < text_.size()) ++at_;
    out.push_back(text_[at_++]);
  }
  if (!take('"')) return std::nullopt;
  return out;
}

std::optional<long long> JsonReader::number() {
  skip();
  std::size_t end = at_;
  while (end < text_.size() && (std::isdigit(static_cast<unsigned char>(text_[end])) != 0 || text_[end] == '-')) ++end;
  if (end == at_) return std::nullopt;
  const long long value = std::stoll(std::string{text_.substr(at_, end - at_)});
  at_ = end;
  return value;
}

// { "key": value, ... }。鍵ごとに field を呼び、field が値を読む。
bool JsonReader::object(const Field& field) {
  if (!take('{')) return false;
  if (take('}')) return true;
  do {
    const auto key = string();
    if (!key || !take(':') || !field(*key)) return false;
  } while (take(','));
  return take('}');
}

bool JsonReader::skip_value() {
  if (peek('{')) return object([this](const std::string&) { return skip_value(); });
  if (peek('"')) return string().has_value();
  return number().has_value();
}

bool JsonReader::read_stats(DiffStats& stats) {
  return object([&](const std::string& key) {
    if (key == "untranslated") {
      return object([&](const std::string& shape) {
        const auto count = number();
        if (!count) return false;
        stats.untranslated.push_back(DiffShape{shape, static_cast<std::size_t>(*count)});
        return true;
      });
    }
    const auto value = number();
    if (!value) return false;
    const auto count = static_cast<std::size_t>(*value);
    if (key == "files") stats.files = count;
    else if (key == "compared") stats.compared = count;
    else if (key == "agreed") stats.agreed = count;
    else if (key == "differences") stats.differences = count;
    else if (key == "translatedGoals") stats.translated_goals = count;
    else if (key == "predicates") stats.predicates = count;
    else if (key == "translatedPredicates") stats.translated_predicates = count;
    else if (key == "attachedPredicates") stats.attached_predicates = count;
    return true;
  });
}

bool JsonReader::read(std::vector<Section>& sections) {
  return object([&](const std::string& key) {
    if (key != "directories") return skip_value();
    return object([&](const std::string& label) {
      Section section{label, {}};
      if (!read_stats(section.stats)) return false;
      sections.push_back(std::move(section));
      return true;
    });
  });
}

std::string quote(std::string_view text) {
  std::string out = "\"";
  for (const char unit : text) {
    if (unit == '"' || unit == '\\') out.push_back('\\');
    out.push_back(unit);
  }
  return out + "\"";
}

std::string ratio(std::size_t part, std::size_t whole) {
  return std::to_string(part) + "/" + std::to_string(whole) + " (" + percent_text(part, whole) + "%)";
}

DiffStats total_of(const std::vector<Section>& sections) {
  DiffStats total;
  for (const Section& section : sections) {
    total.files += section.stats.files;
    total.compared += section.stats.compared;
    total.agreed += section.stats.agreed;
    total.differences += section.stats.differences;
    total.translated_goals += section.stats.translated_goals;
    total.predicates += section.stats.predicates;
    total.translated_predicates += section.stats.translated_predicates;
    total.attached_predicates += section.stats.attached_predicates;
  }
  return total;
}

// バッジの色。違いが 0 だけを緑にする（差分テストの合格条件）。
std::string badge_color(const DiffStats& total) {
  if (total.compared == 0) return "lightgrey";
  if (total.differences == 0) return "brightgreen";
  return total.agreed * 100 >= total.compared * 90 ? "yellow" : "red";
}

void write_stats(std::ostream& out, const DiffStats& stats) {
  out << "{\"files\":" << stats.files << ",\"compared\":" << stats.compared << ",\"agreed\":" << stats.agreed
      << ",\"differences\":" << stats.differences << ",\"translatedGoals\":" << stats.translated_goals
      << ",\"predicates\":" << stats.predicates << ",\"translatedPredicates\":" << stats.translated_predicates
      << ",\"attachedPredicates\":" << stats.attached_predicates << ",\"untranslated\":{";
  for (std::size_t index = 0; index < stats.untranslated.size(); ++index) {
    out << (index == 0 ? "" : ",") << quote(stats.untranslated[index].shape) << ':' << stats.untranslated[index].count;
  }
  out << "}}";
}

void write_json(std::ostream& out, const std::vector<Section>& sections) {
  const DiffStats total = total_of(sections);
  const std::string message = total.compared == 0 ? std::string{"no tests"} : ratio(total.agreed, total.compared);
  out << "{\"schemaVersion\":1,\"label\":\"interpreter = translated\",\"message\":" << quote(message)
      << ",\"color\":" << quote(badge_color(total)) << ",\n";
  out << " \"generated\":\"mosatest --diff writes this file. Do not edit by hand.\",\n \"directories\":{";
  for (std::size_t index = 0; index < sections.size(); ++index) {
    out << (index == 0 ? "\n  " : ",\n  ") << quote(sections[index].label) << ':';
    write_stats(out, sections[index].stats);
  }
  out << "\n }\n}\n";
}

// 答えの比べ方と、字面が違っても代入が同じと分かっている 11 件（diff.cpp の render と answer_text.hpp の注）。
void write_rendering_note(std::ostream& out) {
  out << "\n## How answers are compared\n\n";
  out << "Each answer is one term `'$a'(V1, ..., Vn)` over all query variables in goal order, written without the "
         "query names: unnamed variables are `_G<n>` by first occurrence inside that one write, so sharing between "
         "bindings (`f(A), g(A)` vs `f(A), g(B)`) and aliasing of unbound variables (`X = Y`) stay visible, while the "
         "identity of the cells does not matter. `mosa`'s toplevel text itself differs for 11 goals, because native head "
         "unification binds the clause variables straight to the query cells and the IR goes through fresh cells (the "
         "writer prints a query variable's name for its own cell): control.pl cyclic_not, cyclic_once, cyclic_call_var, "
         "cyclic_findall, cyclic_forall, cyclic_catch_goal, cyclic_not_not_eq; lib_lists.pl length_cyclic, sort_cyclic, "
         "msort_cyclic; native.pl native_var_head (`L = [a,b|L]` vs `L = [a,b|_]`, `T = t(X,W,Y)` vs "
         "`T = t(_G1,_G2,_G1)`). As substitutions they agree, and the table above counts them as agreeing. For "
         "`solutions([...])` tests one more solution than expected is asked for on both sides.\n\n";
  out << "The term is written in canonical form (functional notation, lists in brackets, atoms and numbers quoted as "
         "`writeq/1` does, a re-entered open structure as `_`) by an iterative walk rather than by `write_term/2`: "
         "the writer recurses once per nesting level, and the answer of tests/pl/types.pl `ground_deep` "
         "(`f/1` nested 30000 deep) overflows an 8 MB stack. CTest raises the stack limit of its children, which hides "
         "the overflow there, but a run from a shell or on Windows would not survive it.\n";
}

void write_markdown(std::ostream& out, const std::vector<Section>& sections) {
  out << "# Interpreter / translated agreement\n\n";
  out << "This file is generated by `mosatest --diff`. Do not edit by hand.\n\n";
  out << "Each test file is loaded twice from the same bytes: as IR by the interpreter, and as C++ emitted by "
         "`mosac emit --units` and attached with `install_unit`. Both sides run the same `'$mosa_diff_goal'(K, Vars)` "
         "queries, and the answers, error terms and standard output are compared byte by byte. \"Compared\" counts "
         "the compared lines (one per goal, plus one load line per `.pl` file; `.lgt` files add their failure and summary "
         "lines). \"Translated predicates\" counts the static predicates of the test files that got a native procedure.\n\n";
  out << "| Directory | Files | Compared | Agree | Differences | Translated goals | Translated predicates | Attached |\n";
  out << "| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |\n";
  for (const Section& section : sections) {
    const DiffStats& s = section.stats;
    out << "| " << section.label << " | " << s.files << " | " << s.compared << " | " << s.agreed << " | "
        << s.differences << " | " << s.translated_goals << " | " << ratio(s.translated_predicates, s.predicates)
        << " | " << s.attached_predicates << "/" << s.translated_predicates << " |\n";
  }
  out << "\n\"Attached\" is the number of those procedures that `install_unit` actually attached at run time "
         "(fingerprint match); a file with fewer attached than emitted counts as a difference.\n";
  write_rendering_note(out);
  out << "\n## Goals not given the clause shape\n\n";
  out << "Goals that cannot take the shape `'$mosa_diff_goal'(K, Vars) :- Goal.` run as plain queries on both sides "
         "and count as not translated.\n\n";
  out << "| Directory | Shape | Count |\n| --- | --- | ---: |\n";
  for (const Section& section : sections) {
    for (const DiffShape& shape : section.stats.untranslated) {
      out << "| " << section.label << " | " << shape.shape << " | " << shape.count << " |\n";
    }
  }
  out << "\n- `judge`: the expected outcome is not one of true/false/error/assertion (deterministic, variant, "
         "subsumes, ball, exists, all), or the body uses Logtalk-only calls; the Logtalk runner judges these with its "
         "own machinery on both sides, so the goal text is not rewritten (the file's own predicates still run natively "
         "on the translated side).\n";
  out << "- `unsupported`: an outcome kind the Logtalk runner does not handle.\n";
  out << "- `unreadable`: the assembled goal text does not read back as one term.\n";
}

}  // namespace

void count_shape(DiffStats& stats, std::string_view shape) {
  for (DiffShape& item : stats.untranslated) {
    if (item.shape == shape) {
      ++item.count;
      return;
    }
  }
  stats.untranslated.push_back(DiffShape{std::string{shape}, 1});
}

bool write_agreement(const std::filesystem::path& json, std::string_view label, const DiffStats& stats) {
  std::vector<Section> sections;
  if (std::ifstream in{json, std::ios::binary}) {
    const std::string text{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
    JsonReader reader(text);
    if (!reader.read(sections)) sections.clear();
  }
  std::erase_if(sections, [&](const Section& section) { return section.label == label; });
  sections.push_back(Section{std::string{label}, stats});
  std::sort(sections.begin(), sections.end(),
            [](const Section& left, const Section& right) { return left.label < right.label; });
  std::ofstream out_json{json, std::ios::binary | std::ios::trunc};
  if (!out_json) return false;
  write_json(out_json, sections);
  std::ofstream out_md{std::filesystem::path{json}.replace_extension(".md"), std::ios::binary | std::ios::trunc};
  if (!out_md) return false;
  write_markdown(out_md, sections);
  return static_cast<bool>(out_json) && static_cast<bool>(out_md);
}

}  // namespace mosa
