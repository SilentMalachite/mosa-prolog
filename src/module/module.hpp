#pragma once

#include "mosa/machine.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mosa {

struct Predicate;

struct PredKey {
  std::uint32_t module = 0;
  std::uint32_t name = 0;
  std::uint16_t arity = 0;
};

// alias は as の別名。別名が無いときは name と同じ値を入れる。
struct Import {
  std::uint32_t name = 0;
  std::uint16_t arity = 0;
  std::uint32_t alias = 0;
  std::uint32_t from = 0;
};

struct Module {
  std::uint32_t name = 0;
  std::uint32_t default_import = 0;
  std::vector<PredKey> exports;
  std::vector<Import> imports;
  std::string file;
};

// user と system を作る。system の既定の取り込み元は自分自身で、引く連鎖をそこで止める。
void install_modules(Machine::Impl& machine);
[[nodiscard]] const Module* find_module(const Machine::Impl& machine, std::uint32_t name);
[[nodiscard]] Module* find_module(Machine::Impl& machine, std::uint32_t name);
[[nodiscard]] Predicate* find_pred(Machine::Impl& machine, PredKey key);
[[nodiscard]] const Predicate* find_pred(const Machine::Impl& machine, PredKey key);
[[nodiscard]] Predicate* resolve_pred(Machine::Impl& machine, std::uint32_t context, std::uint32_t name,
                                      std::uint16_t arity);

// 読み込みの指令 module/2 と export/1。nullopt は成功で、値は読み込みの診断にする Formal である。
// first が偽なら、引数が正しくても permission_error(create, module, M) にする。
[[nodiscard]] std::optional<Cell> declare_module(Machine::Impl& machine, Cell name, Cell exports, bool first,
                                                 std::string_view file);
[[nodiscard]] std::optional<Cell> add_exports(Machine::Impl& machine, Cell indicators);
// from の公開述語を into から見えるようにする。同じ名前の取り込みは新しい方に置き換える。
void import_exports(Machine::Impl& machine, std::uint32_t from, std::uint32_t into);

}
