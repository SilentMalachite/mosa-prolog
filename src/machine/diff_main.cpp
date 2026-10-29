// 計画 Task 11。mosatest --diff の実行ファイルの本体。mosac emit --units が出した main.cpp から呼ばれ、ディレクトリの
// 試験のファイルごとに同じ原文（diff_source）を 2 つの機械で読む。インタプリタ側は表の無い単位で load_unit（翻訳側の
// install_unit と同じ読み方で、付ける物が無い）、翻訳側は単位を install_unit。問い合わせも同じ '$mosa_diff_goal'(K, ...)
// なので、違いは生成コードの振る舞いだけに出る。出力は 1 件 1 行で比べ、違った件は両方の字面を出す。
// 文面は lib/messages_*.pl の鍵（ADR 0007）。

#include "diff.hpp"
#include "internal.hpp"

#include "mosa/command_line.hpp"
#include "mosa/console.hpp"
#include "mosa/messages.hpp"
#include "mosa/path_text.hpp"
#include "mosa/runtime.hpp"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace mosa::rt {
namespace {

std::string message(std::string_view key, std::vector<std::string> args = {}) {
  return render_message_fresh(default_message_language(), key, args);
}

std::string joined(std::span<const std::string_view> parts) {
  std::string text;
  for (const std::string_view part : parts) text += part;
  return text;
}

struct Args {
  std::string dir;
  std::string report;
  bool ok = false;
};

// <ディレクトリ> [--report <ファイル>]。報告先の既定は mosatest --diff が決めて渡す。
Args parse_args(std::span<const std::string> args) {
  Args parsed;
  if (args.size() < 2 || args[1].empty()) return parsed;
  parsed.dir = args[1];
  if (args.size() == 2) parsed.ok = true;
  if (args.size() == 4 && args[2] == "--report" && !args[3].empty()) {
    parsed.report = args[3];
    parsed.ok = true;
  }
  return parsed;
}

const DiffEntry* find_entry(std::span<const DiffEntry> entries, const std::filesystem::path& file) {
  const std::filesystem::path key = canonical_key(file);
  for (const DiffEntry& entry : entries) {
    if (!entry.files.empty() && canonical_key(path_from_utf8(entry.files.front().key)) == key) return &entry;
  }
  return nullptr;
}

// 1 つのファイルの両方の側が読む物。.lgt は実行器が組み立てた原文で、ファイルのパスでは引かないので埋め込まない。
struct Sides {
  const DiffEntry& entry;
  std::vector<std::string> texts;  // files と同じ添字。断片をつないだ原文
  std::filesystem::path file;
  bool lgt = false;
};

void prepare(Machine& machine, const Sides& sides) {
  if (sides.lgt) return;
  hook_test_facts(machine, sides.file);
  for (std::size_t index = 0; index < sides.entry.files.size(); ++index) {
    embed(machine, sides.entry.files[index].key, sides.texts[index]);
  }
}

bool load_interpreter(Machine& machine, const Sides& sides) {
  prepare(machine, sides);
  const DiffFile& root = sides.entry.files.front();
  try {
    return load_unit(engine_impl(machine), NativeUnit{root.key, sides.texts.front(), nullptr, {}, {}, {}});
  } catch (const ResourceLimit&) {
    return false;
  }
}

// 根の単位が include や use_module で読んだモジュールのファイルは、その単位の install_unit が付けるだけになる。
// 単位の無い根（どの機械でも読めない原文）はインタプリタ側と同じ読み方で、同じ失敗の字面を出す。
bool load_translated(Machine& machine, const Sides& sides) {
  if (sides.entry.files.front().unit == nullptr) return load_interpreter(machine, sides);
  prepare(machine, sides);
  try {
    for (std::size_t index = 0; index < sides.entry.files.size(); ++index) {
      const DiffFile& file = sides.entry.files[index];
      if (file.unit != nullptr && !install_unit(machine, file.unit(sides.texts[index]))) return false;
    }
    return true;
  } catch (const ResourceLimit&) {
    return false;
  }
}

struct Outcome {
  std::size_t compared = 0;
  std::size_t agreed = 0;
};

Outcome compare(const std::string& shown, const std::vector<DiffLine>& left, const std::vector<DiffLine>& right) {
  Outcome out;
  out.compared = std::max(left.size(), right.size());
  const DiffLine none{"(none)", "(none)"};
  for (std::size_t index = 0; index < out.compared; ++index) {
    const DiffLine& a = index < left.size() ? left[index] : none;
    const DiffLine& b = index < right.size() ? right[index] : none;
    if (a.label == b.label && a.text == b.text) {
      ++out.agreed;
      continue;
    }
    std::cout << message("diff_mismatch", {shown, a.label == b.label ? a.label : a.label + "/" + b.label}) << '\n'
              << message("diff_interpreter", {a.text}) << '\n'
              << message("diff_translated", {b.text}) << '\n';
  }
  return out;
}

std::size_t count_goals(DiffStats& stats, const DiffSource& source) {
  std::size_t translated = 0;
  for (const DiffGoal& goal : source.goals) {
    if (goal.query.empty()) count_shape(stats, goal.shape);
    else ++translated;
  }
  stats.translated_goals += translated;
  return translated;
}

void fail_file(DiffStats& stats, const std::string& line) {
  std::cout << line << '\n';
  ++stats.files;
  ++stats.differences;
}

void run_file(const std::filesystem::path& file, const std::filesystem::path& dir, std::span<const DiffEntry> entries,
              DiffStats& stats) {
  const std::string shown = utf8_from_path(file);
  const DiffSource source = diff_source(file);
  if (!source.error.empty()) return fail_file(stats, message("diff_cannot_read", {shown, source.error}));
  const DiffEntry* entry = find_entry(entries, file);
  // どの機械でも読めない原文（source.loads が偽）には単位が無く、両方の側が同じに失敗することを比べる。
  if (entry == nullptr || (entry->files.front().unit == nullptr && source.loads)) {
    return fail_file(stats, message("diff_not_emitted", {shown}));
  }
  Sides sides{*entry, {}, file, source.lgt};
  for (const DiffFile& item : entry->files) sides.texts.push_back(joined(item.source));
  if (sides.texts.front() != source.source) return fail_file(stats, message("diff_source_changed", {shown}));
  const auto interpreter = run_diff_file(file, dir, source, [&](Machine& machine, const std::filesystem::path&) {
    return load_interpreter(machine, sides);
  });
  const auto translated = run_diff_file(file, dir, source, [&](Machine& machine, const std::filesystem::path&) {
    return load_translated(machine, sides);
  });
  const Outcome outcome = compare(shown, interpreter, translated);
  const std::size_t goals = count_goals(stats, source);
  ++stats.files;
  stats.compared += outcome.compared;
  stats.agreed += outcome.agreed;
  stats.differences += outcome.compared - outcome.agreed;
  stats.predicates += entry->predicates;
  stats.translated_predicates += entry->translated;
  std::cout << message("diff_file", {shown, std::to_string(outcome.compared), std::to_string(outcome.agreed),
                                     std::to_string(goals)})
            << '\n';
}

// 小数第 1 位まで。第 2 位が 5 のときは切り上げる（iso_suite.cpp の合格率と同じ）。
std::string percent(std::size_t part, std::size_t whole) {
  if (whole == 0) return "0.0";
  const std::size_t tenths = (part * 2000 + whole) / (whole * 2);
  return std::to_string(tenths / 10) + "." + std::to_string(tenths % 10);
}

std::string label_of(const std::filesystem::path& dir) {
  std::string label = dir.lexically_normal().generic_string();
  while (label.size() > 1 && label.back() == '/') label.pop_back();
  return label;
}

}  // namespace

int diff_main(int argc, char** argv, std::span<const DiffLibrary> libraries, std::span<const DiffEntry> entries) {
  prepare_console();
  const std::vector<std::string> args = command_line_utf8(argc, argv);
  const Args parsed = parse_args(args);
  if (!parsed.ok) {
    std::cerr << message("diff_usage", {args.empty() ? std::string{"mosa_diff"} : args.front()}) << '\n';
    return 1;
  }
  // 同梱ライブラリは原文だけ。一覧はプロセスで 1 つなので、両方の側（と文面のための新しい機械）が同じ原文を読む。
  for (const DiffLibrary& library : libraries) bundle_library(library.name, joined(library.source));
  const std::filesystem::path dir = path_from_utf8(parsed.dir);
  const std::vector<std::filesystem::path> files = diff_files(dir);
  if (files.empty()) {
    std::cerr << message("diff_no_files", {parsed.dir}) << '\n';
    return 1;
  }
  DiffStats stats;
  for (const std::filesystem::path& file : files) run_file(file, dir, entries, stats);
  std::cout << message("diff_total", {std::to_string(stats.agreed), std::to_string(stats.compared),
                                      std::to_string(stats.translated_predicates), std::to_string(stats.predicates),
                                      percent(stats.translated_predicates, stats.predicates),
                                      std::to_string(stats.differences)})
            << '\n';
  if (!parsed.report.empty() && !write_agreement(path_from_utf8(parsed.report), label_of(dir), stats)) {
    std::cerr << message("bench_cannot_write", {parsed.report}) << '\n';
    return 1;
  }
  return stats.differences == 0 ? 0 : 1;
}

}  // namespace mosa::rt
