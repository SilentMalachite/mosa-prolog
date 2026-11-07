// リリース 0.1.0 Task 4。マニュアルの 1 ファイルから例を読む（mosatest --manual）。規則は tests/manual/README.md。
// 囲いは必ず直前の行に印を持つ。印の無い囲いを誤りにするのは、検査から黙って漏れる例を作らないため。
// 行は 1 行ずつ前から読む。印は次の行が開きの囲いのときだけ効き、間に何かあればその印を誤りにする。
// \r は行末で落とす。Windows で checkout したファイル（CRLF）でも同じ例と同じ入力になる。

#include "mosa/manual.hpp"

#include "mosa/messages.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mosa {
namespace {

enum class MarkerKind { Check, Skip, File };

struct Marker {
  MarkerKind kind = MarkerKind::Skip;
  std::size_t line = 0;  // 印の最初の行（1 から）
  std::string id;
  std::string name;
  std::optional<std::string> input;
};

struct MarkerHead {
  MarkerKind kind = MarkerKind::Skip;
  std::string_view rest;
  bool closed = false;  // 同じ行に --> がある
};

struct Fence {
  char mark = '`';
  std::size_t length = 0;
  std::size_t indent = 0;
  std::string_view language;
};

constexpr std::string_view kCommandPrefix = "$ ";
constexpr std::string_view kQueryPrefix = "?- ";

std::vector<std::string_view> split_lines(std::string_view text) {
  std::vector<std::string_view> lines;
  std::size_t start = 0;
  while (start < text.size()) {
    std::size_t end = text.find('\n', start);
    if (end == std::string_view::npos) end = text.size();
    std::string_view line = text.substr(start, end - start);
    if (line.ends_with('\r')) line.remove_suffix(1);
    lines.push_back(line);
    start = end + 1;
  }
  return lines;
}

bool blank(char letter) { return letter == ' ' || letter == '\t'; }

std::string_view trim(std::string_view text) {
  while (!text.empty() && blank(text.front())) text.remove_prefix(1);
  while (!text.empty() && blank(text.back())) text.remove_suffix(1);
  return text;
}

std::vector<std::string_view> split_words(std::string_view text) {
  std::vector<std::string_view> words;
  std::size_t index = 0;
  while (index < text.size()) {
    while (index < text.size() && blank(text[index])) ++index;
    const std::size_t start = index;
    while (index < text.size() && !blank(text[index])) ++index;
    if (index > start) words.push_back(text.substr(start, index - start));
  }
  return words;
}

// CommonMark の囲い（``` か ~~~ を 3 つ以上）。リストの中の囲いも見落とさないよう、字下げの深さは問わない。
std::optional<Fence> opening_fence(std::string_view line) {
  Fence fence;
  while (fence.indent < line.size() && line[fence.indent] == ' ') ++fence.indent;
  if (fence.indent == line.size()) return std::nullopt;
  fence.mark = line[fence.indent];
  if (fence.mark != '`' && fence.mark != '~') return std::nullopt;
  while (fence.indent + fence.length < line.size() && line[fence.indent + fence.length] == fence.mark) ++fence.length;
  if (fence.length < 3) return std::nullopt;
  const std::string_view info = trim(line.substr(fence.indent + fence.length));
  if (fence.mark == '`' && info.find('`') != std::string_view::npos) return std::nullopt;
  fence.language = info.substr(0, info.find_first_of(" \t"));
  return fence;
}

bool closes(const Fence& fence, std::string_view line) {
  const std::string_view text = trim(line);
  std::size_t length = 0;
  while (length < text.size() && text[length] == fence.mark) ++length;
  return length >= fence.length && length == text.size();
}

// 囲いの字下げの分だけ、中の行の頭の空白を落とす（CommonMark と同じ）。
std::string_view dedent(std::string_view line, std::size_t indent) {
  std::size_t index = 0;
  while (index < indent && index < line.size() && line[index] == ' ') ++index;
  return line.substr(index);
}

// 箇条書きの印（- * + か 1. 1)）とその後の空白 1 つの長さ。印でなければ 0。
std::size_t list_marker(std::string_view text) {
  std::size_t index = 0;
  if (!text.empty() && (text.front() == '-' || text.front() == '*' || text.front() == '+')) {
    index = 1;
  } else {
    while (index < text.size() && text[index] >= '0' && text[index] <= '9') ++index;
    if (index == 0 || index == text.size() || (text[index] != '.' && text[index] != ')')) return 0;
    ++index;
  }
  return index < text.size() && blank(text[index]) ? index + 1 : 0;
}

// 行頭のうち、検査器が囲いとして読まないもの（> の引用、タブ、同じ行の箇条書きの印、その間の空白）の長さ。
// 空白だけなら 0（opening_fence が読む）。この後ろの囲いを黙って飛ばすと検査から漏れるので、誤りにする。
std::size_t unreadable_prefix(std::string_view line) {
  std::size_t index = 0;
  bool unreadable = false;
  while (index < line.size()) {
    if (line[index] == ' ') {
      ++index;
      continue;
    }
    const std::size_t step = line[index] == '\t' || line[index] == '>' ? 1 : list_marker(line.substr(index));
    if (step == 0) break;
    unreadable = true;
    index += step;
  }
  return unreadable ? index : 0;
}

// 印の行なら種類と残りを返す。印でない注釈（<!-- predicates: … --> など）は nullopt で、ただの本文として扱う。
std::optional<MarkerHead> marker_head(std::string_view line) {
  std::string_view text = trim(line);
  if (!text.starts_with("<!--")) return std::nullopt;
  text = trim(text.substr(4));
  MarkerHead head;
  head.closed = text.ends_with("-->");
  if (head.closed) text = trim(text.substr(0, text.size() - 3));
  constexpr std::array<std::pair<std::string_view, MarkerKind>, 3> kinds{{
      {"check:", MarkerKind::Check}, {"skip:", MarkerKind::Skip}, {"file:", MarkerKind::File}}};
  for (const auto& [prefix, kind] : kinds) {
    if (!text.starts_with(prefix)) continue;
    head.kind = kind;
    head.rest = trim(text.substr(prefix.size()));
    return head;
  }
  return std::nullopt;
}

// file: の NAME と ./NAME は章のディレクトリの中の名前だけにする。作業場所の外へ書かせない。
bool plain_file_name(std::string_view name) {
  return !name.empty() && name != "." && name != ".." && name.find_first_of("/\\:") == std::string_view::npos;
}

bool allowed_program(std::string_view word) {
  if (word == "mosa" || word == "mosac" || word == "mosatest") return true;
  return word.starts_with("./") && plain_file_name(word.substr(2));
}

struct Words {
  std::vector<std::string> words;
  std::string_view problem;  // 空でなければ誤りの鍵（lib/messages_*.pl）
  char culprit = '\0';       // manual_console_special のときの字
};

// POSIX のシェルが特別に扱う字。引用の外ではこれ、"…" の中では \ $ ` を誤りにして、シェルが動かすのと同じ argv
// だけを受ける（読者は同じ行をシェルに打つ）。'…' の中はシェルでもすべて字のまま。
constexpr std::string_view kShellSpecial = "\\$`|&;<>()*?[#~";
constexpr std::string_view kDoubleQuoteSpecial = "\\$`";

// シェルの語の分け方のうち、空白と '…' "…" だけ。引用は隣の字とつながる（-g"x" は -gx）。
Words split_command(std::string_view text) {
  Words result;
  std::string word;
  bool in_word = false;
  char quote = '\0';
  for (const char letter : text) {
    std::string_view special = kShellSpecial;
    if (quote != '\0') special = quote == '"' ? kDoubleQuoteSpecial : std::string_view{};
    if (special.find(letter) != std::string_view::npos) {
      result.problem = "manual_console_special";
      result.culprit = letter;
      return result;
    }
    if (quote != '\0') {
      if (letter == quote) quote = '\0';
      else word += letter;
    } else if (letter == '\'' || letter == '"') {
      quote = letter;
      in_word = true;
    } else if (blank(letter)) {
      if (in_word) result.words.push_back(std::move(word));
      word.clear();
      in_word = false;
    } else {
      word += letter;
      in_word = true;
    }
  }
  if (quote != '\0') result.problem = "manual_console_quote";
  else if (in_word) result.words.push_back(std::move(word));
  return result;
}

class Parser {
 public:
  Parser(std::string_view text, std::string_view file) : lines_(split_lines(text)), file_(file) {}
  ParsedManual run();

 private:
  void error(std::size_t line, std::string_view key, std::vector<std::string> args = {});
  std::size_t read_marker(std::size_t index, const MarkerHead& head);
  std::size_t read_override(std::size_t index, Marker& marker);
  std::size_t read_block(std::size_t index, const Fence& fence);
  std::size_t skip_unreadable(std::size_t index, const Fence& fence);
  void add_example(const Marker& marker, const Fence& fence, std::size_t line, std::string body);
  std::optional<ExampleKind> check_kind(const Fence& fence, std::size_t line, std::string_view body);
  void check_console(std::size_t line, std::string_view body);

  std::vector<std::string_view> lines_;
  std::string file_;
  ParsedManual result_;
  std::optional<Marker> pending_;
};

ParsedManual Parser::run() {
  std::size_t index = 0;
  while (index < lines_.size()) {
    const std::string_view line = lines_[index];
    if (const auto fence = opening_fence(line)) {
      index = read_block(index, *fence);
      continue;
    }
    if (const std::size_t prefix = unreadable_prefix(line); prefix > 0) {
      if (const auto fence = opening_fence(line.substr(prefix))) {
        index = skip_unreadable(index, *fence);
        continue;
      }
    }
    if (pending_) {
      error(pending_->line, "manual_marker_alone");
      pending_.reset();
    }
    if (const auto head = marker_head(line)) {
      index = read_marker(index, *head);
      continue;
    }
    ++index;
  }
  if (pending_) error(pending_->line, "manual_marker_alone");
  return std::move(result_);
}

void Parser::error(std::size_t line, std::string_view key, std::vector<std::string> args) {
  result_.errors.push_back(file_ + ":" + std::to_string(line) + ": " +
                           render_message_fresh(default_message_language(), key, args));
}

// 印を読み、次の行の囲いのために取っておく。返すのは印の次の行の位置。
std::size_t Parser::read_marker(std::size_t index, const MarkerHead& head) {
  Marker marker;
  marker.kind = head.kind;
  marker.line = index + 1;
  const std::vector<std::string_view> words = split_words(head.rest);
  const std::size_t wanted = head.kind == MarkerKind::File ? 2 : 1;
  const bool multi_line = !head.closed && head.kind == MarkerKind::Check;
  if (words.size() != wanted || (!head.closed && !multi_line)) {
    error(marker.line, "manual_marker_bad", {std::string{trim(lines_[index])}});
    return index + 1;
  }
  marker.id = std::string{words[0]};
  if (head.kind == MarkerKind::File) {
    if (!plain_file_name(words[1])) {
      error(marker.line, "manual_file_name", {std::string{words[1]}});
      return index + 1;
    }
    marker.name = std::string{words[1]};
  }
  const std::size_t next = multi_line ? read_override(index + 1, marker) : index + 1;
  if (multi_line && next > lines_.size()) return lines_.size();
  pending_ = std::move(marker);
  return next;
}

// 複数行の check: の入力を --> の行まで読む。行はそのまま（空の行も）。閉じなければ誤りにし、size() + 1 を返す。
std::size_t Parser::read_override(std::size_t index, Marker& marker) {
  std::string input;
  while (index < lines_.size() && trim(lines_[index]) != "-->") {
    input += lines_[index];
    input += '\n';
    ++index;
  }
  if (index == lines_.size()) {
    error(marker.line, "manual_marker_unclosed");
    return lines_.size() + 1;
  }
  marker.input = std::move(input);
  return index + 1;
}

// 囲いの中を閉じの囲いまで読み、取っておいた印と合わせて例にする。返すのは閉じの次の行の位置。
std::size_t Parser::read_block(std::size_t index, const Fence& fence) {
  const std::size_t line = index + 1;
  std::optional<Marker> marker = std::move(pending_);
  pending_.reset();
  std::string body;
  std::size_t next = index + 1;
  while (next < lines_.size() && !closes(fence, lines_[next])) {
    body += dedent(lines_[next], fence.indent);
    body += '\n';
    ++next;
  }
  if (next == lines_.size()) {
    error(line, "manual_fence_unclosed");
    return next;
  }
  if (!marker) {
    error(line, "manual_unmarked");
    return next + 1;
  }
  add_example(*marker, fence, line, std::move(body));
  return next + 1;
}

// 読めない囲いは誤りにし、閉じの囲いまで飛ばす。中の行を印や囲いとして読み違えないため。直前の印も捨てる。
std::size_t Parser::skip_unreadable(std::size_t index, const Fence& fence) {
  error(index + 1, "manual_fence_unsupported");
  pending_.reset();
  std::size_t next = index + 1;
  while (next < lines_.size()) {
    const std::string_view line = lines_[next++];
    if (closes(fence, line.substr(unreadable_prefix(line)))) break;
  }
  return next;
}

void Parser::add_example(const Marker& marker, const Fence& fence, std::size_t line, std::string body) {
  std::optional<ExampleKind> kind = ExampleKind::Skip;
  if (marker.kind == MarkerKind::Check) kind = check_kind(fence, line, body);
  if (marker.kind == MarkerKind::File) {
    kind = ExampleKind::File;
    if (fence.language != "prolog") {
      error(line, "manual_file_kind");
      kind.reset();
    }
  }
  for (const ManualExample& earlier : result_.examples) {
    if (earlier.id != marker.id) continue;
    error(line, "manual_duplicate_id", {marker.id});
    kind.reset();
    break;
  }
  if (!kind) return;
  result_.examples.push_back(ManualExample{marker.id, *kind, file_, line, std::move(body), marker.input, marker.name});
}

std::optional<ExampleKind> Parser::check_kind(const Fence& fence, std::size_t line, std::string_view body) {
  if (fence.language == "console") {
    check_console(line, body);
    return ExampleKind::Console;
  }
  if (fence.language == "text" && body.starts_with(kQueryPrefix)) return ExampleKind::Repl;
  error(line, "manual_check_kind");
  return std::nullopt;
}

// 最初の行は "$ " で始まり、どのコマンドも許したプログラムで始まり、引用が閉じる。誤りはその行の番号で出す。
void Parser::check_console(std::size_t line, std::string_view body) {
  const std::vector<std::string_view> lines = split_lines(body);
  if (lines.empty() || !lines.front().starts_with(kCommandPrefix)) {
    error(line + 1, "manual_console_start");
    return;
  }
  for (std::size_t offset = 0; offset < lines.size(); ++offset) {
    if (!lines[offset].starts_with(kCommandPrefix)) continue;
    const Words words = split_command(lines[offset].substr(kCommandPrefix.size()));
    if (!words.problem.empty()) {
      std::vector<std::string> args;
      if (words.culprit != '\0') args.emplace_back(1, words.culprit);
      error(line + 1 + offset, words.problem, std::move(args));
    } else if (words.words.empty() || !allowed_program(words.words.front())) {
      error(line + 1 + offset, "manual_console_program",
            {words.words.empty() ? std::string{} : words.words.front()});
    }
  }
}

}  // namespace

ParsedManual parse_manual(std::string_view text, std::string_view file) { return Parser{text, file}.run(); }

std::vector<ConsoleCommand> console_commands(std::string_view body) {
  std::vector<ConsoleCommand> commands;
  const std::vector<std::string_view> lines = split_lines(body);
  for (std::size_t offset = 0; offset < lines.size(); ++offset) {
    const std::string_view line = lines[offset];
    if (line.starts_with(kCommandPrefix)) {
      ConsoleCommand command;
      Words words = split_command(line.substr(kCommandPrefix.size()));
      if (words.problem.empty()) command.argv = std::move(words.words);
      command.offset = offset;
      commands.push_back(std::move(command));
    } else if (!commands.empty()) {
      commands.back().output += line;
      commands.back().output += '\n';
    }
  }
  return commands;
}

std::string default_input(std::string_view expected) {
  std::string input;
  for (const std::string_view line : split_lines(expected)) {
    if (!line.starts_with(kQueryPrefix)) continue;
    input += line.substr(kQueryPrefix.size());
    input += '\n';
  }
  return input;
}

// 版の突き合わせは id だけでなく印の種類も見る。片方の版だけが skip: にして検査を逃れることを防ぐ。
std::string id_sequence(std::span<const ManualExample> examples) {
  std::string out;
  for (const ManualExample& example : examples) {
    if (!out.empty()) out += ' ';
    if (example.kind == ExampleKind::Skip) out += "skip:";
    else if (example.kind == ExampleKind::File) out += "file:";
    else out += "check:";
    out += example.id;
  }
  return out;
}

}  // namespace mosa
