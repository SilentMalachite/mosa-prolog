#pragma once

// ^^ の観測と、tester.lgt が名指しした Prolog。テストランナーの外には出さない。

#include "mosa/machine.hpp"

#include <filesystem>
#include <string>
#include <string_view>

namespace mosa {

struct LgtIo {
  std::filesystem::path tests_dir;
  std::filesystem::path scratch;
  std::filesystem::path suite;
};

enum class LgtSuperKind { Ok, No, Unknown };

struct LgtSuper {
  LgtSuperKind kind = LgtSuperKind::Unknown;
  std::string note;
  std::string binding_name;
  Cell binding{};
};

[[nodiscard]] LgtIo lgt_io_context(std::string_view tests_path, std::string_view relative_dir);
void lgt_io_cleanup(const LgtIo& io);
void lgt_load_tester(Machine& machine, const LgtIo& io);
[[nodiscard]] LgtSuper lgt_run_super(Machine& machine, const LgtIo& io, std::string_view goal);

}  // namespace mosa
