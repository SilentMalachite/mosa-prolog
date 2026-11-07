// リリース 0.1.0 Task 4。mosatest --manual <ディレクトリ> [--mosa <パス>] [--work <ディレクトリ>]。
// en/ と ja/ の *.md（README.md は読まない）から例を読み（manual_parse.cpp）、両版で同じ名前のファイルがそろい、
// 例の印の種類と id の並びが同じことを確かめてから、check: の例を mosa で動かして書いてある出力と 1 字ずつ比べる。
// 子は標準出力と標準エラーを 1 つのファイルに受け、MOSA_TRANSCRIPT=1 で端末と同じ表示（打った行の書き返しと促し）を
// 出す（Task 3）。比べる前に両側の \r と、入力の終わりで待っている最後の促し "?- "（入力があり、最後の行が halt. で
// ないときだけ）を除く。終了コードは比べない。エラーを見せる例は 0 以外で終わるのが正しいことがある。ただし落ちた子
// （シグナル、Windows の NTSTATUS のエラー）と出力の無い子は失敗にする。LANG は版で決め、MOSA_LIB などは消して、
// 開発者の環境に左右させない。子の作業ディレクトリは章ごとの <work>/<版>/<章の語幹>/。run_process に作業
// ディレクトリの引数は無く、posix_spawn_file_actions_addchdir_np は移植できないので、起こす間だけこのプロセスの
// 作業ディレクトリを移す。
// mosatest は 1 本の流れで動くのでほかと取り合わない。そのためパスは前もってすべて絶対にしておく。
// 述語の表（Task 5）は例を動かす前に、このプロセスの中で処理系の一覧と両方向で比べる。組込は engine.cpp の表、
// ライブラリは新しい機械に読んだ公開リスト。子と同じ mosa の一覧と比べるため、mosatest は mosa の隣に置く。

#include "mosa/manual.hpp"

#include "internal.hpp"

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
#include <map>
#include <memory>
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
constexpr std::string_view kSystem = "system";
// 表に書けるライブラリ。lib/ の messages_* は文面の表で、述語を公開しない。
constexpr std::array<std::string_view, 6> kLibraries{"apply", "between", "format", "japanese", "lists", "strings"};

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
  std::vector<ManualTable> tables;
};

struct Edition {
  std::string name;
  std::vector<Chapter> chapters;  // 名前の順
};

// 一覧の途中で読めなくなったら nullopt。黙って途中までで済ませると、章が抜けたまま通る。
std::optional<std::vector<std::filesystem::path>> markdown_files(const std::filesystem::path& dir) {
  std::vector<std::filesystem::path> files;
  std::error_code error;
  std::filesystem::directory_iterator entry(dir, error);
  for (; !error && entry != std::filesystem::directory_iterator{}; entry.increment(error)) {
    std::error_code kind;
    if (entry->is_regular_file(kind) && entry->path().extension() == ".md") files.push_back(entry->path());
  }
  if (error) return std::nullopt;
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
  chapter.tables = std::move(parsed.tables);
  return chapter;
}

std::optional<Edition> load_edition(const std::filesystem::path& dir, const std::filesystem::path& shown,
                                    std::string_view name, std::vector<std::string>& problems) {
  Edition edition{std::string{name}, {}};
  const std::filesystem::path sub = path_from_utf8(name);
  const auto files = markdown_files(dir / sub);
  if (!files) {
    std::cerr << message("manual_cannot_list", {utf8_from_path(shown / sub)}) << '\n';
    return std::nullopt;
  }
  for (const std::filesystem::path& path : *files) {
    edition.chapters.push_back(load_chapter(path, utf8_from_path(shown / sub / path.filename()), problems));
  }
  return edition;
}

bool has_checks(const Edition& edition) {
  for (const Chapter& chapter : edition.chapters) {
    for (const ManualExample& example : chapter.examples) {
      if (example.kind == ExampleKind::Repl || example.kind == ExampleKind::Console) return true;
    }
  }
  return false;
}

const Chapter* find_chapter(const Edition& edition, std::string_view name) {
  for (const Chapter& chapter : edition.chapters) {
    if (chapter.name == name) return &chapter;
  }
  return nullptr;
}

// 版の突き合わせに使う表の並び。"between system" の形。表の中身は両版とも処理系と比べるので、名前だけを見る。
std::string table_sequence(const std::vector<ManualTable>& tables) {
  std::string out;
  for (const ManualTable& table : tables) {
    if (!out.empty()) out += ' ';
    out += table.library;
  }
  return out;
}

// どちらかの版にあるファイルは両方にあり、例の印の種類と id（印の 3 種すべて）の並びと、表の並びが同じ。
// 表の並びも見るのは、片方の版だけが表の印を落として比べられずに通ることを防ぐため。
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
    const std::string left_ids = id_sequence(left->examples);
    const std::string right_ids = id_sequence(right->examples);
    if (left_ids != right_ids) {
      problems.push_back(message("manual_ids_differ", {left->shown, right->shown, left_ids, right_ids}));
    }
    const std::string left_tables = table_sequence(left->tables);
    const std::string right_tables = table_sequence(right->tables);
    if (left_tables != right_tables) {
      problems.push_back(message("manual_tables_differ", {left->shown, right->shown, left_tables, right_tables}));
    }
  }
}

using Expected = std::map<std::string, std::optional<std::vector<PredicateIndicator>>>;

// 表と比べる処理系の一覧。ライブラリは新しい機械に読むので、同じ名前は 1 度だけ作る。
const std::optional<std::vector<PredicateIndicator>>& expected_list(Expected& lists, const std::string& library) {
  if (const auto found = lists.find(library); found != lists.end()) return found->second;
  std::optional<std::vector<PredicateIndicator>> made =
      library == kSystem ? std::optional{builtin_indicators()} : library_exports(library);
  return lists.emplace(library, std::move(made)).first->second;
}

// 同じ章の同じ名前の表は 1 つにまとめる（見出しごとに分けた表）。診断は最初の印の行に出す。
std::vector<ManualTable> merged_tables(const std::vector<ManualTable>& tables) {
  std::vector<ManualTable> out;
  for (const ManualTable& table : tables) {
    const auto found = std::ranges::find(out, table.library, &ManualTable::library);
    if (found == out.end()) out.push_back(table);
    else found->entries.insert(found->entries.end(), table.entries.begin(), table.entries.end());
  }
  return out;
}

// from にあって in に無い述語を、from の順に空白で区切って並べる。
std::string absent(const std::vector<PredicateIndicator>& from, const std::vector<PredicateIndicator>& in) {
  std::set<std::string> known;
  for (const PredicateIndicator& item : in) known.insert(indicator_text(item));
  std::string out;
  for (const PredicateIndicator& item : from) {
    std::string text = indicator_text(item);
    if (known.contains(text)) continue;
    if (!out.empty()) out += ' ';
    out += text;
  }
  return out;
}

std::string library_names() {
  std::string out;
  for (const std::string_view name : kLibraries) {
    if (!out.empty()) out += ", ";
    out += name;
  }
  return out;
}

// 表にあって処理系に無い述語と、処理系にあって表に無い述語の両方を 1 つの誤りに名指す。
void check_tables(const Edition& edition, Expected& lists, std::vector<std::string>& problems) {
  for (const Chapter& chapter : edition.chapters) {
    if (!chapter.usable) continue;
    for (const ManualTable& table : merged_tables(chapter.tables)) {
      const std::string where = chapter.shown + ":" + std::to_string(table.line) + ": ";
      const std::optional<std::vector<PredicateIndicator>>& wanted = expected_list(lists, table.library);
      if (!wanted) {
        problems.push_back(where + message("manual_predicates_unknown", {table.library, library_names()}));
        continue;
      }
      std::string extra = absent(table.entries, *wanted);
      std::string missing = absent(*wanted, table.entries);
      if (extra.empty() && missing.empty()) continue;
      problems.push_back(
          where + message("manual_predicates_differ", {table.library, std::move(extra), std::move(missing)}));
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

// 1 回の実行の結果。ran が false なら、出力が期待どおり（空の期待も）でも失敗にする。
struct Outcome {
  std::string output;  // 比べる形の出力。失敗の理由はその後ろに 1 行足してある
  bool ran = false;    // 起こせて、自分で終わり、出力を読めた
};

Outcome broken(std::string output, std::string_view key, std::vector<std::string> args) {
  if (!output.empty() && !output.ends_with('\n')) output += '\n';
  return Outcome{output + message(key, std::move(args)) + '\n', false};
}

// Task 3 が決めた形。入力があり、その最後の行が halt. でなければ、mosa は入力の終わりで促し "?- " を出して待つ。
// それ以外の "?- " は読者の端末でも出たまま待つものなので除かない（-g だけで -t halt の無い mosa など）。
bool waits_at_end(std::string_view input) {
  if (input.empty()) return false;
  if (input.ends_with('\n')) input.remove_suffix(1);
  const std::size_t start = input.rfind('\n');
  return (start == std::string_view::npos ? input : input.substr(start + 1)) != "halt.";
}

// 1 つのコマンドを動かし、比べる形の出力を返す。前の実行の出力を読まないよう、出力のファイルは先に消す。
Outcome run_child(const Runner& runner, const std::filesystem::path& chapter, const std::vector<std::string>& argv,
                  std::string_view input) {
  const std::filesystem::path in = runner.io / "input.txt";
  const std::filesystem::path out = runner.io / "output.txt";
  std::error_code error;
  std::filesystem::remove(out, error);
  if (error) return broken({}, "bench_cannot_write", {utf8_from_path(out)});
  if (!write_file(in, input)) return broken({}, "bench_cannot_write", {utf8_from_path(in)});
  std::optional<int> code;
  {
    const WorkingDirectory here(chapter);
    if (here.entered()) code = run_process(argv, out, runner.env, in);
  }
  if (!code) return broken({}, "bench_cannot_start", {argv.front()});
  std::optional<std::string> text = read_file(out);
  if (!text) return broken({}, "cannot_open_file", {utf8_from_path(out)});
  std::erase(*text, '\r');
  if (waits_at_end(input) && text->ends_with(kPrompt)) text->resize(text->size() - kPrompt.size());
  if (exited_abnormally(*code)) {
    return broken(std::move(*text), "manual_abnormal_exit", {argv.front(), std::to_string(*code)});
  }
  return Outcome{std::move(*text), true};
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
  const Outcome outcome = run_child(runner, chapter, {utf8_from_path(runner.mosa)}, input);
  if (outcome.ran && outcome.output == example.body) return true;
  report_failure(example, example.line, example.body, outcome.output);
  return false;
}

// コマンドを順に動かし、最初に違ったところで止める。後のコマンドは前の結果（作ったファイル）に頼りうる。
bool check_console(const Runner& runner, const ManualExample& example, const std::filesystem::path& chapter) {
  for (const ConsoleCommand& command : console_commands(example.body)) {
    std::vector<std::string> argv = command.argv;
    argv.front() = program_path(runner, chapter, argv.front());
    const std::string input = example.input ? *example.input : default_input(command.output);
    const Outcome outcome = run_child(runner, chapter, argv, input);
    if (outcome.ran && outcome.output == command.output) continue;
    report_failure(example, example.line + 1 + command.offset, command.output, outcome.output);
    return false;
  }
  return true;
}

// 章のディレクトリは毎回空から作る。前の回のファイルや実行ファイルが残ると、作れなくなった例が通ってしまうので、
// 消せないか消した後も残っていれば誤りの鍵を返す（その章は動かさない）。
std::optional<std::string_view> fresh_directory(const std::filesystem::path& dir) {
  std::error_code error;
  std::filesystem::remove_all(dir, error);
  if (error) return "manual_cannot_clear";
  if (std::filesystem::exists(dir, error) || error) return "manual_cannot_clear";
  std::filesystem::create_directories(dir, error);
  if (error) return "bench_cannot_write";
  return std::nullopt;
}

void run_chapter(Runner& runner, const Chapter& chapter, const std::filesystem::path& dir) {
  if (const auto key = fresh_directory(dir)) {
    std::cout << message(*key, {utf8_from_path(dir)}) << '\n';
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

std::vector<PredicateIndicator> builtin_indicators() {
  std::vector<PredicateIndicator> all;
  for (const BuiltinPi& item : builtin_procedures()) {
    // $ で始まる名前はライブラリが使う内側の補助で、初めて読む人のマニュアルには載せない（Task 5 の決定）。
    if (item.name.starts_with('$')) continue;
    all.push_back(PredicateIndicator{std::string{item.name}, item.arity});
  }
  return all;
}

std::optional<std::vector<PredicateIndicator>> library_exports(std::string_view library) {
  if (std::ranges::find(kLibraries, library) == kLibraries.end()) return std::nullopt;
  const std::unique_ptr<Machine> machine = Machine::create();
  // use_module/2 は指令でしか使えない。[] で何も取り込まずに読み、user を変えずに公開リストだけを見る。
  if (!machine->load_program(":- use_module(library(" + std::string{library} + "), []).\n").ok) return std::nullopt;
  const Impl& impl = engine_impl(*machine);
  const Module* module = find_module(impl, machine->atoms().intern(library));
  if (module == nullptr) return std::nullopt;
  std::vector<PredicateIndicator> exports;
  for (const PredKey& key : module->exports) {
    exports.push_back(PredicateIndicator{std::string{impl.atoms.name(key.name)}, key.arity});
  }
  return exports;
}

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
  const std::optional<Edition> en = load_edition(dir, shown, kEditions[0], problems);
  const std::optional<Edition> ja = load_edition(dir, shown, kEditions[1], problems);
  if (!en || !ja) return 2;
  check_editions(*en, *ja, shown, problems);
  Expected lists;
  check_tables(*en, lists, problems);
  check_tables(*ja, lists, problems);
  // 何も確かめずに「失敗 0」で通ったことにしない。誤りがあればそちらを出して 1 にする。
  if (problems.empty() && !has_checks(*en) && !has_checks(*ja)) {
    std::cerr << message("manual_no_examples", {args.dir}) << '\n';
    return 2;
  }
  for (const std::string& problem : problems) std::cout << problem << '\n';
  std::optional<Runner> runner = make_runner(args, *self, work);
  if (!runner) return 1;
  run_edition(*runner, *en, work);
  run_edition(*runner, *ja, work);
  runner->failed += problems.size();
  std::cout << message("manual_summary", {std::to_string(runner->checked), std::to_string(runner->skipped),
                                          std::to_string(runner->failed)})
            << '\n';
  return runner->failed == 0 ? 0 : 1;
}

}  // namespace mosa
