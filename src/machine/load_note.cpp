// 翻訳（mosac）のときに、読み込んだファイルの原文と節の位置を書き留める部分（note_site、mark_opaque、FileNote）。
// load.cpp が 800 行の上限に近く、タスク 3 で read_terms に条件付きコンパイルの分を足す余地が要るので分けた。振る舞いは変えない。
#include "load_internal.hpp"

#include "mosa/path_text.hpp"
#include "mosa/reader.hpp"

#include <filesystem>
#include <string>
#include <string_view>
#include <utility>

namespace mosa {

// 定義は load_path.cpp。
std::filesystem::path canonical_key(const std::filesystem::path& file);

// 翻訳のときだけ、足した節の位置を書き留める。どこにも定義の無い述語を呼ぶ警告がこの行を出す。
// key は add_clause が節を足した述語。
void note_site(Impl& machine, PredKey key, std::size_t line) {
  TranslationLog* log = machine.translation;
  if (log == nullptr || log->current >= log->files.size()) return;
  const Predicate* found = find_pred(machine, key);
  if (found == nullptr) return;
  const Predicate& predicate = *found;
  log->sites.push_back(TranslationLog::Site{PredKey{predicate.module, predicate.name, predicate.arity},
                                            predicate.clauses.size() - 1, log->current, line});
}

// 飛ばしたゴールの op/3 は後の項の読みを変えうるので、翻訳の読みで誤りになった項も起動時には読めるかもしれない
// （計画 Task 5）。誤りにせず、このファイルの述語を IR のまま動かす印を付ける。飛ばしたゴールが無ければ誤りのまま。
bool mark_opaque(TranslationLog* log) {
  if (log == nullptr || log->current >= log->files.size() || !log->files[log->current].skipped_goal) return false;
  log->files[log->current].opaque = true;
  return true;
}

FileNote::FileNote(Impl& machine, std::string_view origin, std::string_view source) : log(machine.translation) {
  if (log == nullptr) return;
  outer = log->current;
  if (origin.empty()) return;
  const auto key = utf8_from_path(canonical_key(path_from_utf8(origin)));
  for (std::size_t index = 0; index < log->files.size(); ++index) {
    if (log->files[index].key == key) log->current = index;
  }
  if (log->current != outer) return;
  // library(Name) で開いた物はその探索の根、ライブラリから素のパスや include で開いた物は親と同じ根。
  std::string root;
  for (const TranslationLog::LibraryOpen& open : log->library_opens) {
    if (open.key == key) root = open.root;
  }
  if (root.empty() && outer < log->files.size()) root = log->files[outer].library_root;
  TranslationLog::File file{key, std::string{source}, 0, 0, !root.empty()};
  file.library_root = std::move(root);
  log->files.push_back(std::move(file));
  log->current = log->files.size() - 1;
}

// include した先で飛ばしたゴールの op/3 は、読み込んだ側のテキストにも効く。
FileNote::~FileNote() {
  if (log == nullptr) return;
  if (log->current < log->files.size() && outer < log->files.size() && log->files[log->current].skipped_goal) {
    log->files[outer].skipped_goal = true;
  }
  log->current = outer;
}

bool FileNote::tolerate_syntax() const { return mark_opaque(log); }
void FileNote::first_term(const ReadTerm& read) const {
  if (log == nullptr || log->current >= log->files.size() || log->files[log->current].line != 0) return;
  log->files[log->current].line = read.line;
  log->files[log->current].column = read.column;
}

}  // namespace mosa
