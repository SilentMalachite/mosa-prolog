#pragma once

// 8.10 の解の像。solutions.cpp が集め、solutions_answer.cpp が束縛する。ゴールの検査は solutions_goal.cpp。
// 1 ファイル 800 行を超えないように分けてある。

#include "../machine/internal.hpp"

#include <cstdint>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace mosa {
namespace sol {

// Big は integer が符号（負なら 1）で、kids が絶対値の語を下位から持つ Int の節点（ADR 0025、ExportedTerm と同じ持ち方）。
enum class CopyKind { Ext, Loc, Atom, Int, Float, List, Str, Big };

struct CopyNode {
  CopyKind kind = CopyKind::Atom;
  int id = -1;
  std::uint32_t atom = 0;
  std::uint16_t arity = 0;
  std::int64_t integer = 0;
  double floating = 0;
  std::vector<int> kids;
};

// 証人リストとテンプレートを一つの像にする。外部変数の id は解をまたいで共有する。
struct Image {
  std::vector<CopyNode> nodes;
  int witness = -1;
  int templ = -1;
};

struct Group {
  ExportedTerm key;
  std::vector<ExportedTerm> templs;
  std::vector<Image> shots;
};

struct Packet {
  std::vector<Group> groups;
  std::vector<std::uint64_t> ext_raw;
  // 鍵の目印（key_hash）から groups の添字へ。組を探すときは目印の同じ組だけを same_term で確かめる。前はどの答えでも
  // すべての組を比べ、組の数の 2 乗になった（レビュー M-27）。集めるときだけ使う。
  std::unordered_multimap<std::uint64_t, std::size_t> index;
};

struct Answer {
  Cell witness{};
  std::vector<Cell> values;
  Cell list{};
};

[[nodiscard]] std::vector<Answer> answers_from(Impl& machine, const std::vector<Group>& groups, bool sorting,
                                               const std::vector<std::uint64_t>& ext_raw);
void bind_answers(Impl& machine, const Goal& goal, const std::vector<Cell>& witnesses,
                  const std::vector<Answer>& answers);
void bind_found(Impl& machine, const Goal& goal, const Packet& packet);

// ゴールを動かす前の検査と証人（solutions_goal.cpp）。
enum class Which { None, Findall, Bagof, Setof };
Which which_solution(std::string_view name, std::uint16_t arity);
std::optional<Cell> strip_caret(Impl& machine, Cell& goal, std::vector<Cell>& hidden);
std::vector<Cell> free_witnesses(Impl& machine, Cell goal, Cell templ, const std::vector<Cell>& hidden);
bool goal_shape_ok(Impl& machine, const Goal& goal, Cell cell);
bool goal_body_ok(Impl& machine, const Goal& goal, Cell cell, Compiled& compiled);
bool bad_list(Impl& machine, const Goal& goal);
bool bad_tail(Impl& machine, const Goal& goal);

}  // namespace sol
}  // namespace mosa
