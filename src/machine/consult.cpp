#include "internal.hpp"

#include "mosa/writer.hpp"
#include "mosa/path_text.hpp"

#include <algorithm>
#include <filesystem>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace mosa {

void prefix_consult_diagnostic(Impl& machine, std::string_view path) {
  if (machine.syntax_diag) {
    machine.diagnostic = std::string{path} + ":" + std::to_string(machine.diag_line) + ":" +
                         std::to_string(machine.diag_column) + ": " + machine.diagnostic;
    return;
  }
  machine.diagnostic = std::string{path} + ": " + machine.diagnostic;
}

namespace {

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

// 印は述語の名前（モジュール・名前・項数）で持つ。印は「親のテキストがこの名前に節を与えたか」で、子のテキストが
// 述語を消して作り直しても親の印は残る。添字で持つと、子の読み込みが abolish で述語を表から消したとき、後ろの述語に
// 隣の印が付く（M19 で見つけた不具合）。Predicate::serial で持つと、子が消して作り直した述語に親の印が付かず、親の
// 続きの節が子の節を置き換えて述語を静的にする（その直しの敵対的レビュー A1）。親のテキストで節が離れていれば、
// 子が作り直しても clauses_apart になる（同 B1）。子が消したままの述語は、親のテキストの abolish と同じく印を失う。
struct SeenMark {
  PredKey key;
  std::size_t serial = 0;
  bool seen = false;
  bool declared = false;
  bool meta = false;
};

struct ClauseBackup {
  // 節は番号ごと写す。戻したとき、開いた走査の候補が同じ節を指す（ADR 0015）。
  explicit ClauseBackup(const Predicate& predicate)
      : module(predicate.module),
        name(predicate.name),
        arity(predicate.arity),
        clauses(predicate.clauses),
        dynamic(predicate.dynamic) {}
  // Predicate と同じく、写しの vector が伸びても節を写さないよう移動を noexcept にする。
  ClauseBackup(const ClauseBackup&) = default;
  ClauseBackup& operator=(const ClauseBackup&) = default;
  ClauseBackup(ClauseBackup&&) noexcept = default;
  ClauseBackup& operator=(ClauseBackup&&) noexcept = default;
  ~ClauseBackup() = default;

  std::uint32_t module = 0;
  std::uint32_t name = 0;
  std::uint16_t arity = 0;
  ClauseList clauses;
  bool dynamic = false;
};

static_assert(std::is_nothrow_move_constructible_v<ClauseBackup>);
static_assert(std::is_nothrow_move_assignable_v<ClauseBackup>);

bool key_less(const PredKey& a, const PredKey& b) {
  return std::tie(a.module, a.name, a.arity) < std::tie(b.module, b.name, b.arity);
}

// 述語の表は serial の昇順（M18）なので、写しも serial の昇順になる。
std::vector<SeenMark> mark_seen(const Impl& machine) {
  std::vector<SeenMark> saved;
  saved.reserve(machine.predicates.size());
  for (const Predicate& predicate : machine.predicates) {
    const PredKey key{predicate.module, predicate.name, predicate.arity};
    saved.push_back(
        SeenMark{key, predicate.serial, predicate.seen, predicate.dynamic_declared, predicate.meta_declared});
  }
  return saved;
}

void put_mark(Predicate& predicate, const SeenMark* mark) {
  // 写しに無い名前は子の読み込みが初めて作った。親のテキストはまだ見ていないので、次の節は置き換える。
  predicate.seen = mark != nullptr && mark->seen;
  predicate.dynamic_declared = mark != nullptr && mark->declared;
  predicate.meta_declared = mark != nullptr && mark->meta;
}

// 表と写しを serial の順に並べて歩く。同じ serial は子の前からある述語。子が消した述語の印だけを名前で並べ、子が
// 作った述語はそこから同じ名前を探す。全部を名前で並べ替えると、consult のたびに述語の数 n について n log n かかる
// （敵対的レビュー B3）。
void restore_seen(Impl& machine, const std::vector<SeenMark>& saved) {
  std::vector<const SeenMark*> gone;
  std::vector<Predicate*> created;
  auto mark = saved.begin();
  for (Predicate& predicate : machine.predicates) {
    for (; mark != saved.end() && mark->serial < predicate.serial; ++mark) gone.push_back(&*mark);
    if (mark != saved.end() && mark->serial == predicate.serial) {
      put_mark(predicate, &*mark);
      ++mark;
    } else {
      created.push_back(&predicate);
    }
  }
  for (; mark != saved.end(); ++mark) gone.push_back(&*mark);
  const auto before = [](const SeenMark* a, const SeenMark* b) { return key_less(a->key, b->key); };
  std::sort(gone.begin(), gone.end(), before);
  for (Predicate* predicate : created) {
    const SeenMark wanted{PredKey{predicate->module, predicate->name, predicate->arity}};
    const auto found = std::lower_bound(gone.begin(), gone.end(), &wanted, before);
    const bool kept = found != gone.end() && !key_less(wanted.key, (*found)->key);
    put_mark(*predicate, kept ? *found : nullptr);
  }
}

std::vector<ClauseBackup> backup_clauses(const Impl& machine) {
  std::vector<ClauseBackup> saved;
  saved.reserve(machine.predicates.size());
  for (const Predicate& predicate : machine.predicates) {
    saved.emplace_back(predicate);
  }
  return saved;
}

void restore_clauses(Impl& machine, const std::vector<ClauseBackup>& saved) {
  for (const ClauseBackup& item : saved) {
    for (Predicate& predicate : machine.predicates) {
      if (predicate.module != item.module || predicate.name != item.name || predicate.arity != item.arity) continue;
      // 戻した節にも生成関数は付け直さない（ADR 0009）。
      detach_native(machine, predicate);
      replace_clauses(predicate, item.clauses);
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
  const auto text = read_source(machine, file);
  if (!text) {
    note_failure(machine, message_text(machine, "cannot_open_file", {utf8_from_path(file)}));
    return -1;
  }
  const auto saved = mark_seen(machine);
  const LoadStatus status =
      load_source(machine, *text, utf8_from_path(file.parent_path()), utf8_from_path(file), &opening);
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
  Impl& impl = engine_impl(*this);
  // 問い合わせと同じく、前の分の空きを生まない GC の数を持ち越さない（ADR 0010 の改訂 B）。
  reset_gc_judgement(impl);
  const auto text = read_source(impl, path_from_utf8(path));
  if (!text) {
    impl.diagnostic = message_text(impl, "cannot_open_file", {path});
    impl.syntax_diag = false;
    return false;
  }
  const LoadStatus status = load_consulted(impl, path, *text);
  if (status.ok) {
    impl.diagnostic.clear();
    return true;
  }
  prefix_consult_diagnostic(impl, path);
  return false;
}

bool run_consult(Impl& machine, const Goal& call) {
  const Cell path = machine.heap.deref(call.arg(machine, 0));
  if (machine.heap.unbound(path)) {
    raise_formal(machine, formal_instantiation(machine), call.name, call.arity);
    return true;
  }
  if (path.tag() != Tag::Atom) {
    raise_formal(machine, formal_type(machine, "atom", path), call.name, call.arity);
    return true;
  }
  const auto text = std::string{machine.atoms.name(static_cast<std::uint32_t>(path.raw() >> 3))};
  const auto source = read_source(machine, path_from_utf8(text));
  if (!source) {
    machine.diagnostic = message_text(machine, "cannot_open_file", {text});
    machine.failed = true;
    return true;
  }
  // 読み込みの指示は入れ子の run_loop で動き、GC が枠を動かす。読んだ後は登録した写し goal を使う。
  Goal goal = call;
  const RootScope roots(machine, [&goal](GcVisitor& visitor) {
    visitor.frame(goal.at);
    visitor.frame(goal.cont);
  });
  // 公開述語は consult を呼んだ文脈モジュールへ取り込む。
  const auto outer = machine.load_module;
  machine.load_module = goal.module;
  // 読み込みの中の指示から呼ぶと、子の読み込みが親の印を落とす。[File] と同じく戻す（M19 で見つけた不具合）。
  // 戻さないと、親の次の節が自分の前の節を置き換え、dynamic の宣言も忘れる。読み込みの外では印は使わない。
  const auto saved = mark_seen(machine);
  const LoadStatus status = load_consulted(machine, text, *source);
  restore_seen(machine, saved);
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
