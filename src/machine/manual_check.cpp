// リリース 0.1.0 Task 4。mosatest --manual <ディレクトリ> [--mosa <パス>] [--work <ディレクトリ>]。
// en/ と ja/ の *.md（README.md は読まない）から例を読み（manual_parse.cpp）、両版で同じ名前のファイルがそろい、
// 例の id の並びが同じことを確かめてから、check: の例を mosa で動かして書いてある出力と 1 字ずつ比べる。
// 子は標準出力と標準エラーを 1 つのファイルに受け、MOSA_TRANSCRIPT=1 で端末と同じ表示（打った行の書き返しと促し）を
// 出す（Task 3）。比べる前に両側の \r と、入力の終わりで待っている最後の促し "?- " を除く。終了コードは比べない。
// エラーを見せる例は 0 以外で終わるのが正しいことがある。LANG は版で決め、MOSA_LIB などは消して、開発者の環境に
// 左右させない。子の作業ディレクトリは章ごとの <work>/<版>/<章の語幹>/。run_process に作業ディレクトリの引数は無く、
// posix_spawn_file_actions_addchdir_np は移植できないので、起こす間だけこのプロセスの作業ディレクトリを移す。
// mosatest は 1 本の流れで動くのでほかと取り合わない。そのためパスは前もってすべて絶対にしておく。

#include "mosa/manual.hpp"

#include "mosa/exe_path.hpp"
#include "mosa/messages.hpp"
#include "mosa/path_text.hpp"
#include "mosa/process.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace mosa {
namespace {

constexpr std::array<std::string_view, 2> kEditions{"en", "ja"};
constexpr std::string_view kPrompt = "?- ";
constexpr std::string_view kReadme = "README.md";

std::string message(std::string_view key, std::vector<std::string> args = {}) {
  return render_message_fresh(default_message_language(), key, args);
}

std::optional<std::string> read_file(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) return std::nullopt;
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

bool write_file(const std::filesystem::path& path, std::string_view text) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output.write(text.data(), static_cast<std::streamsize>(text.size()));
  output.flush();
  return output.good();
}

struct Chapter {
  std::string name;    // 01-sample.md
  std::string shown;   // <ディレクトリ>/<版>/01-sample.md。渡された <ディレクトリ> のまま診断に出す
  bool usable = false; // 読めて誤りが無い。README.md は読まないので false
  std::vector<ManualExample> examples;
};

struct Edition {
  std::string name;
  std::vector<Chapter> chapters;  // 名前の順
};

std::vector<std::filesystem::path> markdown_files(const std::filesystem::path& dir) {
  std::vector<std::filesystem::path> files;
  std::error_code error;
  std::filesystem::directory_iterator entry(dir, error);
  for (; !error && entry != std::filesystem::directory_iterator{}; entry.increment(error)) {
    std::error_code kind;
    if (entry->is_regular_file(kind) && entry->path().extension() == ".md") files.push_back(entry->path());
  }
  std::sort(files.begin(), files.end());
  return files;
}

Chapter load_chapter(const std::filesystem::path& path, std::string shown, std::vector<std::string>& problems) {
  Chapter chapter;
  chapter.name = utf8_from_path(path.filename());
  chapter.shown = std::move(shown);
  if (chapter.name == kReadme) return chapter;
  const std::optional<std::string> text = read_file(path);
  if (!text) {
    problems.push_back(message("cannot_open_file", {chapter.shown}));
    return chapter;
  }
  ParsedManual parsed = parse_manual(*text, chapter.shown);
  problems.insert(problems.end(), parsed.errors.begin(), parsed.errors.end());
  chapter.usable = parsed.errors.empty();
  chapter.examples = std::move(parsed.examples);
  return chapter;
}

Edition load_edition(const std::filesystem::path& dir, const std::filesystem::path& shown, std::string_view name,
                     std::vector<std::string>& problems) {
  Edition edition{std::string{name}, {}};
  const std::filesystem::path sub = path_from_utf8(name);
  for (const std::filesystem::path& path : markdown_files(dir / sub)) {
    edition.chapters.push_back(load_chapter(path, utf8_from_path(shown / sub / path.filename()), problems));
  }
  return edition;
}

const Chapter* find_chapter(const Edition& edition, std::string_view name) {
  for (const Chapter& chapter : edition.chapters) {
    if (chapter.name == name) return &chapter;
  }
  return nullptr;
}

std::string id_list(const Chapter& chapter) {
  std::string out;
  for (const ManualExample& example : chapter.examples) {
    if (!out.empty()) out += ' ';
    out += example.id;
  }
  return out;
}

// どちらかの版にあるファイルは両方にあり、例の id（印の 3 種すべて）の並びが同じ。
void check_editions(const Edition& en, const Edition& ja, const std::filesystem::path& shown,
                    std::vector<std::string>& problems) {
  std::set<std::string> names;
  for (const Edition* edition : {&en, &ja}) {
    for (const Chapter& chapter : edition->chapters) names.insert(chapter.name);
  }
  for (const std::string& name : names) {
    const Chapter* left = find_chapter(en, name);
    const Chapter* right = find_chapter(ja, name);
    if (left == nullptr || right == nullptr) {
      const std::string& absent = left == nullptr ? en.name : ja.name;
      const std::string missing = utf8_from_path(shown / path_from_utf8(absent) / path_from_utf8(name));
      problems.push_back(message("manual_missing", {missing, (left != nullptr ? left : right)->shown}));
      continue;
    }
    // 誤りのあるファイルの並びは途中までなので比べない。誤りは読んだときに出してある。
    if (!left->usable || !right->usable) continue;
    const std::string left_ids = id_list(*left);
    const std::string right_ids = id_list(*right);
    if (left_ids != right_ids) {
      problems.push_back(message("manual_ids_differ", {left->shown, right->shown, left_ids, right_ids}));
    }
  }
}

// 子を起こす間だけ作業ディレクトリを移し、どの道で抜けても戻す。
class WorkingDirectory {
 public:
  explicit WorkingDirectory(const std::filesystem::path& dir) {
    std::error_code error;
    saved_ = std::filesystem::current_path(error);
    if (!error) std::filesystem::current_path(dir, error);
    entered_ = !error;
  }
  WorkingDirectory(const WorkingDirectory&) = delete;
  WorkingDirectory& operator=(const WorkingDirectory&) = delete;
  ~WorkingDirectory() {
    std::error_code error;
    if (entered_) std::filesystem::current_path(saved_, error);
  }
  [[nodiscard]] bool entered() const { return entered_; }

 private:
  std::filesystem::path saved_;
  bool entered_ = false;
};

struct Runner {
  std::filesystem::path mosa;
  std::filesystem::path tools;  // mosac と mosatest の場所（mosa の隣）
  std::string suffix;           // 実行ファイルの拡張子（Windows は .exe）。動いている mosatest と同じ
  std::filesystem::path io;     // 子の入力と出力のファイルの置き場。章のディレクトリには置かない
  std::vector<std::pair<std::string, std::string>> env;
  std::size_t checked = 0;
  std::size_t skipped = 0;
  std::size_t failed = 0;
};

// console のコマンドの名前を、起こすプログラムの絶対パスにする。./NAME は章のディレクトリのプログラム。
std::string program_path(const Runner& runner, const std::filesystem::path& chapter, std::string_view word) {
  if (word == "mosa") return utf8_from_path(runner.mosa);
  const bool local = word.starts_with("./");
  const std::string name = std::string{local ? word.substr(2) : word} + runner.suffix;
  return utf8_from_path((local ? chapter : runner.tools) / path_from_utf8(name));
}

// 1 つのコマンドを動かし、比べる形の出力を返す。起こせなければその診断を出力の代わりにする（違いとして見せる）。
std::string run_child(const Runner& runner, const std::filesystem::path& chapter, const std::vector<std::string>& argv,
                      std::string_view input) {
  const std::filesystem::path in = runner.io / "input.txt";
  const std::filesystem::path out = runner.io / "output.txt";
  if (!write_file(in, input)) return message("bench_cannot_write", {utf8_from_path(in)}) + '\n';
  std::optional<int> code;
  {
    const WorkingDirectory here(chapter);
    if (here.entered()) code = run_process(argv, out, runner.env, in);
  }
  if (!code) return message("bench_cannot_start", {argv.front()}) + '\n';
  std::string text = read_file(out).value_or(std::string{});
  std::erase(text, '\r');
  if (text.ends_with(kPrompt)) text.resize(text.size() - kPrompt.size());
  return text;
}

// 最後の改行の有無も違いになるので、無いときはそう書く。
void print_text(std::string_view text) {
  std::cout << text;
  if (!text.empty() && !text.ends_with('\n')) std::cout << '\n' << message("manual_no_newline") << '\n';
}

void report_failure(const ManualExample& example, std::size_t line, std::string_view expected,
                    std::string_view actual) {
  std::cout << example.file << ':' << line << ' ' << example.id << '\n' << message("manual_expected") << '\n';
  print_text(expected);
  std::cout << message("manual_actual") << '\n';
  print_text(actual);
}

bool check_repl(const Runner& runner, const ManualExample& example, const std::filesystem::path& chapter) {
  const std::string input = example.input ? *example.input : default_input(example.body);
  const std::string actual = run_child(runner, chapter, {utf8_from_path(runner.mosa)}, input);
  if (actual == example.body) return true;
  report_failure(example, example.line, example.body, actual);
  return false;
}

// コマンドを順に動かし、最初に違ったところで止める。後のコマンドは前の結果（作ったファイル）に頼りうる。
bool check_console(const Runner& runner, const ManualExample& example, const std::filesystem::path& chapter) {
  for (const ConsoleCommand& command : console_commands(example.body)) {
    std::vector<std::string> argv = command.argv;
    argv.front() = program_path(runner, chapter, argv.front());
    const std::string input = example.input ? *example.input : default_input(command.output);
    const std::string actual = run_child(runner, chapter, argv, input);
    if (actual == command.output) continue;
    report_failure(example, example.line + 1 + command.offset, command.output, actual);
    return false;
  }
  return true;
}

// 章のディレクトリは毎回空から作る。前の回のファイルや実行ファイルが残ると、作れなくなった例が通ってしまう。
void run_chapter(Runner& runner, const Chapter& chapter, const std::filesystem::path& dir) {
  std::error_code error;
  std::filesystem::remove_all(dir, error);
  error.clear();
  std::filesystem::create_directories(dir, error);
  if (error) {
    std::cout << message("bench_cannot_write", {utf8_from_path(dir)}) << '\n';
    ++runner.failed;
    return;
  }
  for (const ManualExample& example : chapter.examples) {
    if (example.kind == ExampleKind::Skip) {
      ++runner.skipped;
    } else if (example.kind == ExampleKind::File) {
      const std::filesystem::path path = dir / path_from_utf8(example.file_name);
      if (write_file(path, example.body)) continue;
      std::cout << message("bench_cannot_write", {utf8_from_path(path)}) << '\n';
      ++runner.failed;
    } else {
      ++runner.checked;
      const bool passed = example.kind == ExampleKind::Repl ? check_repl(runner, example, dir)
                                                            : check_console(runner, example, dir);
      if (!passed) ++runner.failed;
    }
  }
}

void run_edition(Runner& runner, const Edition& edition, const std::filesystem::path& work) {
  const std::string lang = edition.name == "ja" ? "ja_JP.UTF-8" : "en_US.UTF-8";
  const std::array<std::pair<std::string, std::string>, 2> set{{{"LANG", lang}, {"MOSA_TRANSCRIPT", "1"}}};
  const std::array<std::string, 3> unset{"MOSA_LIB", "MOSA_HEAP_MAX", "MOSA_GC_STRESS"};
  runner.env = environment_with(set, unset);
  for (const Chapter& chapter : edition.chapters) {
    if (!chapter.usable) continue;
    run_chapter(runner, chapter, work / path_from_utf8(edition.name) / path_from_utf8(chapter.name).stem());
  }
}

std::optional<Runner> make_runner(const ManualArgs& args, const std::filesystem::path& self,
                                  const std::filesystem::path& work) {
  Runner runner;
  std::error_code error;
  runner.suffix = utf8_from_path(self.extension());
  runner.mosa = args.mosa.empty() ? self.parent_path() / path_from_utf8("mosa" + runner.suffix)
                                  : std::filesystem::absolute(path_from_utf8(args.mosa), error);
  runner.tools = runner.mosa.parent_path();
  runner.io = work / "io";
  if (!error) std::filesystem::create_directories(runner.io, error);
  if (error) {
    std::cerr << message("bench_cannot_write", {utf8_from_path(runner.io)}) << '\n';
    return std::nullopt;
  }
  return runner;
}

}  // namespace

ManualArgs parse_manual_args(std::span<const std::string_view> args) {
  ManualArgs parsed;
  if (args.empty() || args.front().empty() || args.front().front() == '-') return parsed;
  parsed.dir = std::string{args.front()};
  if (args.size() % 2 != 1) return parsed;
  for (std::size_t index = 1; index < args.size(); index += 2) {
    const std::string value{args[index + 1]};
    if (value.empty()) return parsed;
    if (args[index] == "--mosa") parsed.mosa = value;
    else if (args[index] == "--work") parsed.work = value;
    else return parsed;
  }
  parsed.ok = true;
  return parsed;
}

int run_manual(const ManualArgs& args) {
  const auto self = executable_path();
  if (!self) {
    std::cerr << message("diff_no_self") << '\n';
    return 1;
  }
  std::error_code error;
  const std::filesystem::path shown = path_from_utf8(args.dir);
  const std::filesystem::path dir = std::filesystem::absolute(shown, error);
  bool editions = !error;
  for (const std::string_view edition : kEditions) {
    editions = editions && std::filesystem::is_directory(dir / path_from_utf8(edition), error);
  }
  const std::filesystem::path work = std::filesystem::absolute(path_from_utf8(args.work), error);
  if (!editions || error) {
    std::cerr << message("manual_no_editions", {args.dir}) << '\n';
    return 2;
  }
  std::vector<std::string> problems;
  const Edition en = load_edition(dir, shown, kEditions[0], problems);
  const Edition ja = load_edition(dir, shown, kEditions[1], problems);
  check_editions(en, ja, shown, problems);
  for (const std::string& problem : problems) std::cout << problem << '\n';
  std::optional<Runner> runner = make_runner(args, *self, work);
  if (!runner) return 1;
  run_edition(*runner, en, work);
  run_edition(*runner, ja, work);
  runner->failed += problems.size();
  std::cout << message("manual_summary", {std::to_string(runner->checked), std::to_string(runner->skipped),
                                          std::to_string(runner->failed)})
            << '\n';
  return runner->failed == 0 ? 0 : 1;
}

}  // namespace mosa
