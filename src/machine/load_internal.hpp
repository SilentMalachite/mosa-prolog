#pragma once

// load.cpp と load_directive.cpp だけが共有する。読み込みの外からは使わない。
#include "internal.hpp"

#include "mosa/reader.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

namespace mosa {

struct Snapshot {
  // predicate が null なら、この読み込みが作る述語の印。在れば節を番号ごと写す（定義は load_clauses.cpp）。
  // taken_at は写した時の Impl::db_generation。
  Snapshot(PredKey key, const Predicate* predicate, std::uint64_t taken_at);
  // Predicate と同じく、写しの vector が伸びるたびに節を写さないよう移動を noexcept にする。
  Snapshot(const Snapshot&) = default;
  Snapshot& operator=(const Snapshot&) = default;
  Snapshot(Snapshot&&) noexcept = default;
  Snapshot& operator=(Snapshot&&) noexcept = default;
  ~Snapshot() = default;

  std::uint32_t module = 0;
  std::uint32_t name = 0;
  std::uint16_t arity = 0;
  ClauseList clauses;
  // 写した枠と定義（ADR 0017）。戻すのは同じ定義が残る枠だけで、テキストが始めた定義はその枠の最後の写しへ戻す。
  std::size_t serial = 0;
  std::size_t defined_at = 0;
  // 写した時の世代と Predicate::text_clause_at。後にテキストが節を足したかを比べ、丸ごと戻すときは印も戻す。
  std::uint64_t taken_at = 0;
  std::uint64_t text_clause_at = 0;
  bool defined_by_goal = false;
  bool created = false;
  bool dynamic = false;
  bool abolished = false;
  std::vector<MetaArg> meta;
};

static_assert(std::is_nothrow_move_constructible_v<Snapshot>);
static_assert(std::is_nothrow_move_assignable_v<Snapshot>);

// テキストが定義した述語、その時に読んでいたファイル（Impl::load_owner）、その時の定義（Predicate::defined_at）。
// 後でゴールが abolish して作り直した定義は、このファイルのものにしない（ADR 0020）。clauses は節を与えたことで、
// 偽なら dynamic の宣言だけである。宣言は前の節を置き換えないので、読み直しの終わりに前の節を消す。
struct FileDef {
  PredKey key;
  std::uint32_t file = 0;
  std::size_t defined_at = 0;
  bool clauses = false;
};

// 読み終えたファイルと、テキストが定義した述語の順の列。入れ子の読み込みは写しと一緒に親へ渡し、失敗したら捨てる。
// 一番外の読み込みが成功したら apply_file_notes が使う。applied はもう持ち主を付けた defined の数で、初期化の
// ゴールの後の 2 度目の適用は、それより前の定義も残す述語の印に使う（L11 の 4 度目の Codex レビュー P2）。
// metas は、テキストが既にある述語へ meta_predicate と宣言した鍵。その述語を消すとき、宣言を保留の表へ移す。
// 印（meta_declared）は入れ子の consult の後で親の印へ戻るので使えない（5 度目の Codex レビュー P2）。
// declared は、テキストが discontiguous か multifile と宣言した鍵。消す述語の古い宣言は、宣言し直していなければ外す。
struct FileNotes {
  std::vector<FileDef> defined;
  std::vector<std::uint32_t> files;
  std::vector<PredKey> metas;
  std::vector<PredKey> declared;
  std::size_t applied = 0;
};

// 読み込みの写しの列と、鍵ごとの写しの鎖（ADR 0018）。remember_pred は同じ鍵の写しだけを新しい順に見る。全部の
// 写しを後ろから見ると、初めて触れる述語ごとに写しの数だけかかり、P 個の述語の consult が P の 2 乗になった
// （レビュー M25）。索引は鍵ごとに最後の写しを指し、earlier_ が同じ鍵の一つ前の写しを指す。定義は load_clauses.cpp。
class LoadSnapshots {
 public:
  static constexpr std::size_t kNone = SIZE_MAX;

  [[nodiscard]] std::size_t size() const { return shots_.size(); }
  [[nodiscard]] const Snapshot& operator[](std::size_t at) const { return shots_[at]; }
  // 書ける反復子は、成功した入れ子の読み込みが写しを親へ移すためにある。鍵は書き換えない。
  [[nodiscard]] std::vector<Snapshot>::iterator begin() { return shots_.begin(); }
  [[nodiscard]] std::vector<Snapshot>::iterator end() { return shots_.end(); }
  [[nodiscard]] std::vector<Snapshot>::const_iterator begin() const { return shots_.begin(); }
  [[nodiscard]] std::vector<Snapshot>::const_iterator end() const { return shots_.end(); }
  // key の最後の写しと、at と同じ鍵の一つ前の写しの位置。無ければ kNone。
  [[nodiscard]] std::size_t last_of(PredKey key) const;
  [[nodiscard]] std::size_t earlier(std::size_t at) const { return earlier_[at]; }
  void push_back(Snapshot shot);
  [[nodiscard]] FileNotes& notes() { return notes_; }

 private:
  std::vector<Snapshot> shots_;
  FileNotes notes_;
  std::vector<std::size_t> earlier_;
  KeyIndex<Snapshot> index_;
};

// この読み込みの中だけで隙間を許す。述語ベクタには入れない。宣言は手続きを作らない。
struct Apart {
  std::uint32_t module = 0;
  std::uint32_t name = 0;
  std::uint16_t arity = 0;
};

void fail_load(Impl& machine, std::string message);
[[nodiscard]] bool allows_gap(const std::vector<Apart>& apart, PredKey key);
void note_declared(Impl& machine, PredKey key, bool shared);
// predicate は key を表で引いた結果（無ければ null）。
void remember_pred(const Impl& machine, LoadSnapshots& snapshots, PredKey key, const Predicate* predicate);
[[nodiscard]] bool run_inits(Impl& machine, const std::vector<Cell>& goals);
// 定義は load_clauses.cpp。テキストに置けない節なら、assertz/1 がその節に出すのと同じ Formal を返す（レビュー M10）。
// term は module の修飾を外した節、clause はそれを訳したもの。
[[nodiscard]] std::optional<Cell> refused_clause(Impl& machine, Cell term, const Clause& clause);
// 定義は load_clauses.cpp。節は module に入り、足す前の述語を snapshots へ写す。last は直前に節を足した述語の名前。
[[nodiscard]] bool add_clause(Impl& machine, std::uint32_t module, Clause clause, LoadSnapshots& snapshots,
                              std::optional<PredKey>& last, const std::vector<Apart>& apart);
// 失敗した読み込みの述語を写しへ戻し、この読み込みで作った述語（serial が previous_serial より後）を捨てる。
void rollback_preds(Impl& machine, const LoadSnapshots& snapshots, std::size_t previous_serial);
// 成功した入れ子の読み込みの写しを親の写しへ足す。親が同じ定義を写していれば、親の古い写しを残す（ADR 0017）。
// ファイルの書き留め（ADR 0020）も hand_up_notes で親の後ろへ足す。
void hand_up_snapshots(LoadSnapshots& parent, LoadSnapshots& own);
void hand_up_notes(LoadSnapshots& parent, LoadSnapshots& own);
// テキストが predicate に節を与えたか（clauses）dynamic と宣言したことを書き留める（ADR 0020）。
void note_defined(const Impl& machine, LoadSnapshots& snapshots, const Predicate& predicate, bool clauses);
// 定義は load_clauses.cpp。一番外の読み込みが成功したとき、読み終えたファイルが前に定義し、この読み込みのテキストが
// 定義しなかった述語を消し、定義した述語をそのファイルのものにする（ADR 0020）。started_at はその読み込みの始めの
// Impl::db_generation。
void apply_file_notes(Impl& machine, FileNotes& notes, std::uint64_t started_at);
// first は、この項がファイルの最初の項であること。module/2 はそこでしか受けない。
[[nodiscard]] int consume_directive(Impl& machine, Reader& reader, Cell term, LoadSnapshots& snapshots,
                                    std::vector<Apart>& apart, std::vector<Cell>& inits, bool first,
                                    std::string_view origin);

}
