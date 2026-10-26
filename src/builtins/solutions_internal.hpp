#pragma once

// 8.10 の解の像。solutions.cpp が集め、solutions_answer.cpp が束縛する。
// 1 ファイル 800 行を超えないように分けてある。

#include "../machine/internal.hpp"

#include <cstdint>
#include <vector>

namespace mosa {
namespace sol {

enum class Finish { Ready, Threw, Halted };

enum class CopyKind { Ext, Loc, Atom, Int, Float, List, Str };

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
  Finish end = Finish::Ready;
  std::vector<Group> groups;
  std::vector<std::uint64_t> ext_raw;
  ExportedTerm ball;
  int halt_code = 0;
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
[[nodiscard]] bool bind_found(Impl& machine, const Goal& goal, const Packet& packet);

}  // namespace sol
}  // namespace mosa
