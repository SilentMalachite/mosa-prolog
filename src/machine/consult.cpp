#include "internal.hpp"

#include "mosa/writer.hpp"
#include "mosa/path_text.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace mosa {
namespace {

void prefix_consult_diagnostic(Impl& machine, std::string_view path) {
  if (machine.syntax_diag) {
    machine.diagnostic = std::string{path} + ":" + std::to_string(machine.diag_line) + ":" +
                         std::to_string(machine.diag_column) + ": " + machine.diagnostic;
    return;
  }
  machine.diagnostic = std::string{path} + ": " + machine.diagnostic;
}

void note_failure(Impl& machine, std::string message) {
  machine.diagnostic = std::move(message);
  machine.syntax_diag = false;
}

// 相対の include は、開いたファイルのディレクトリから探す。
LoadStatus load_consulted(Impl& machine, std::string_view path, std::string_view source) {
  const std::filesystem::path file = path_from_utf8(path);
  return load_source(machine, source, utf8_from_path(file.parent_path()), utf8_from_path(file));
}

// 相対パスは、今読んでいるファイルのディレクトリから開く。文字だけの load は、渡されたパスのまま。
std::filesystem::path resolve_file(std::string_view directory, std::string_view atom) {
  const std::filesystem::path file = path_from_utf8(atom);
  if (file.is_absolute() || directory.empty()) return file;
  return path_from_utf8(directory) / file;
}

struct SeenMark {
  bool seen = false;
  bool declared = false;
  bool meta = false;
};

struct ClauseBackup {
  std::uint32_t module = 0;
  std::uint32_t name = 0;
  std::uint16_t arity = 0;
  std::vector<Clause> clauses;
  bool dynamic = false;
};

std::vector<SeenMark> mark_seen(const Impl& machine) {
  std::vector<SeenMark> saved;
  saved.reserve(machine.predicates.size());
  for (const Predicate& predicate : machine.predicates) {
    saved.push_back(SeenMark{predicate.seen, predicate.dynamic_declared, predicate.meta_declared});
  }
  return saved;
}

void restore_seen(Impl& machine, const std::vector<SeenMark>& saved) {
  const auto count = saved.size();
  for (std::size_t index = 0; index < count && index < machine.predicates.size(); ++index) {
    machine.predicates[index].seen = saved[index].seen;
    machine.predicates[index].dynamic_declared = saved[index].declared;
    machine.predicates[index].meta_declared = saved[index].meta;
  }
  // 親のテキストは、consult が足した述語をまだ見ていない。次の節は置き換える。
  for (std::size_t index = count; index < machine.predicates.size(); ++index) {
    machine.predicates[index].seen = false;
    machine.predicates[index].dynamic_declared = false;
    machine.predicates[index].meta_declared = false;
  }
}

std::vector<ClauseBackup> backup_clauses(const Impl& machine) {
  std::vector<ClauseBackup> saved;
  saved.reserve(machine.predicates.size());
  for (const Predicate& predicate : machine.predicates) {
    saved.push_back(
        ClauseBackup{predicate.module, predicate.name, predicate.arity, predicate.clauses, predicate.dynamic});
  }
  return saved;
}

void restore_clauses(Impl& machine, const std::vector<ClauseBackup>& saved) {
  for (const ClauseBackup& item : saved) {
    for (Predicate& predicate : machine.predicates) {
      if (predicate.module != item.module || predicate.name != item.name || predicate.arity != item.arity) continue;
      predicate.clauses = item.clauses;
      predicate.dynamic = item.dynamic;
      break;
    }
  }
}

bool unbound_item(const Impl& machine, const WalkedList& walked) {
  for (const Cell item : walked.items) {
    if (machine.heap.unbound(item)) return true;
  }
  return false;
}

std::string formal_message(Impl& machine, Cell formal) {
  return write_term(machine.heap, machine.operators, formal, {}, {});
}

// :- consult(File) は :- [File] と同じ。相対パスは、読み込み中のファイルのディレクトリから開く。
int collect_consulted(Impl& machine, Cell body, std::vector<Cell>& items) {
  const Cell file = machine.heap.deref(machine.heap.arg(body, 0));
  if (machine.heap.unbound(file)) {
    note_failure(machine, formal_message(machine, formal_instantiation(machine)));
    return -1;
  }
  if (file.tag() != Tag::Atom) {
    note_failure(machine, formal_message(machine, formal_type(machine, "atom", file)));
    return -1;
  }
  items = {file};
  return 1;
}

// 1 はファイルのリスト。0 はリストではない。-1 はこの指示のエラー。
int collect_files(Impl& machine, Cell term, std::vector<Cell>& items) {
  term = machine.heap.deref(term);
  if (!is_directive(machine.heap, term)) return 0;
  const Cell body = machine.heap.deref(machine.heap.arg(term, 0));
  if (body.tag() == Tag::Str && machine.heap.functor_arity(body) == 1 &&
      machine.atoms.name(machine.heap.functor_atom(body)) == "consult") {
    return collect_consulted(machine, body, items);
  }
  const WalkedList walked = walk_list(machine, body);
  if (walked.shape == ListShape::Variable) return 0;
  if (walked.shape == ListShape::Partial || unbound_item(machine, walked)) {
    note_failure(machine, formal_message(machine, formal_instantiation(machine)));
    return -1;
  }
  if (walked.shape != ListShape::Proper) {
    if (body.tag() != Tag::Lis) return 0;
    note_failure(machine, formal_message(machine, formal_type(machine, "list", body)));
    return -1;
  }
  for (const Cell item : walked.items) {
    if (item.tag() == Tag::Atom) continue;
    note_failure(machine, formal_message(machine, formal_type(machine, "atom", item)));
    return -1;
  }
  items = walked.items;
  return 1;
}

int consult_one(Impl& machine, const std::filesystem::path& file, std::vector<std::string>& opening) {
  std::ifstream input{file, std::ios::binary};
  if (!input) {
    note_failure(machine, utf8_from_path(file) + ": ファイルを開けません");
    return -1;
  }
  std::ostringstream buffer;
  buffer << input.rdbuf();
  const auto saved = mark_seen(machine);
  const LoadStatus status =
      load_source(machine, buffer.str(), utf8_from_path(file.parent_path()), utf8_from_path(file), &opening);
  restore_seen(machine, saved);
  if (status.ok) return 1;
  prefix_consult_diagnostic(machine, utf8_from_path(file));
  return -1;
}

}  // namespace

int apply_file_list(Impl& machine, Cell term, std::string_view directory, std::vector<std::string>& opening) {
  std::vector<Cell> items;
  const int collected = collect_files(machine, term, items);
  if (collected <= 0) return collected;
  const auto backups = backup_clauses(machine);
  for (const Cell item : items) {
    const auto text = machine.atoms.name(static_cast<std::uint32_t>(item.raw() >> 3));
    const auto file = resolve_file(directory, text);
    if (consult_one(machine, file, opening) < 0) {
      // 初期化の例外は節を残す。それ以外は、このリストで置き換えた節を戻す。
      if (!machine.load_kept) restore_clauses(machine, backups);
      return -1;
    }
  }
  return 1;
}

bool Machine::consult(std::string_view path) {
  std::ifstream input{path_from_utf8(path), std::ios::binary};
  Impl& impl = engine_impl(*this);
  if (!input) {
    impl.diagnostic = std::string{path} + ": ファイルを開けません";
    impl.syntax_diag = false;
    return false;
  }
  std::ostringstream buffer;
  buffer << input.rdbuf();
  const LoadStatus status = load_consulted(impl, path, buffer.str());
  if (status.ok) {
    impl.diagnostic.clear();
    return true;
  }
  prefix_consult_diagnostic(impl, path);
  return false;
}

bool run_consult(Impl& machine, const Goal& goal) {
  const Cell path = machine.heap.deref(goal.arg(machine, 0));
  if (machine.heap.unbound(path)) {
    raise_formal(machine, formal_instantiation(machine), goal.name, goal.arity);
    return true;
  }
  if (path.tag() != Tag::Atom) {
    raise_formal(machine, formal_type(machine, "atom", path), goal.name, goal.arity);
    return true;
  }
  const auto text = std::string{machine.atoms.name(static_cast<std::uint32_t>(path.raw() >> 3))};
  std::ifstream input{path_from_utf8(text), std::ios::binary};
  if (!input) {
    machine.diagnostic = text + ": ファイルを開けません";
    machine.failed = true;
    return true;
  }
  std::ostringstream buffer;
  buffer << input.rdbuf();
  // 公開述語は consult を呼んだ文脈モジュールへ取り込む。
  const auto outer = machine.load_module;
  machine.load_module = goal.module;
  const LoadStatus status = load_consulted(machine, text, buffer.str());
  machine.load_module = outer;
  if (!status.ok) {
    prefix_consult_diagnostic(machine, text);
    machine.failed = true;
    return true;
  }
  machine.diagnostic.clear();
  machine.ip = goal.cont;
  return true;
}

}
