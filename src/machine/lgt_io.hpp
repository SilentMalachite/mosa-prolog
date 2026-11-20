#pragma once

// ^^ の観測と、tester.lgt が名指しした Prolog。テストランナーの外には出さない。

#include "mosa/machine.hpp"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace mosa {

struct LgtIo {
  std::filesystem::path tests_dir;
  std::filesystem::path scratch;
  std::filesystem::path suite;
};

enum class LgtSuperKind { Ok, No, Unknown };

// 本体の ^^ の単一化が束縛した、名前付きの変数。
struct LgtBound {
  std::string name;
  Cell value{};
};

struct LgtSuper {
  LgtSuperKind kind = LgtSuperKind::Unknown;
  std::string note;
  // 補助が出力引数 slot に返す値。単一化は呼び手がする。本体と節で、変数の在りかが違う。
  bool output = false;
  Cell slot{};
  Cell value{};
  std::vector<LgtBound> bindings;
};

[[nodiscard]] LgtIo lgt_io_context(std::string_view tests_path, std::string_view relative_dir);
void lgt_io_cleanup(const LgtIo& io);
void lgt_arm_super(Machine& machine, const LgtIo& io);
void lgt_disarm_super(Machine& machine);
void lgt_load_tester(Machine& machine, const LgtIo& io);
// テストの機械の user へ lists、apply、between、format、strings、japanese を取り込む。lgt_libs.cpp。
void lgt_import_libraries(Machine& machine);
[[nodiscard]] LgtSuper lgt_run_super(Machine& machine, const LgtIo& io, std::string_view goal);

}  // namespace mosa
