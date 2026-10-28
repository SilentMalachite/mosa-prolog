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
  // 正規化したパス。use_module がファイルからモジュールを引く。ファイルでないテキストは空。
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
[[nodiscard]] std::optional<Cell> read_indicator(Machine::Impl& machine, Cell cell, PredKey& key);

// use_module/2 の第 2 引数。All は use_module/1、Only は [PI, PI as Alias]、Except は except([PI])。
struct ImportSpec {
  enum class Kind { All, Only, Except };
  Kind kind = Kind::All;
  std::vector<Import> only;
  std::vector<PredKey> except;
};

[[nodiscard]] std::optional<Cell> parse_imports(Machine::Impl& machine, Cell imports, ImportSpec& spec);
// from の公開述語を spec のとおり into から見えるようにする。衝突と未公開は読み込みの診断にする Formal を返す。
[[nodiscard]] std::optional<Cell> apply_imports(Machine::Impl& machine, std::uint32_t from, std::uint32_t into,
                                                const ImportSpec& spec);
[[nodiscard]] std::optional<Cell> import_exports(Machine::Impl& machine, std::uint32_t from, std::uint32_t into);
// 局所の述語を作る前に呼ぶ。同じ名前を取り込み済みなら permission_error(import_into, ...) を返す。
[[nodiscard]] std::optional<Cell> check_local(Machine::Impl& machine, std::uint32_t module, std::uint32_t name,
                                              std::uint16_t arity);

}
