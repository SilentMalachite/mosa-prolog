// library(Name) は $MOSA_LIB の Name.pl を、実行ファイルの隣の lib/ より先に読む。
// 一時ファイルは tests/tmp/library-env。実行ファイルの隣に置く分は探索先なので、終わったら消す。

#include "environment.hpp"
#include "executable.hpp"
#include "harness.hpp"
#include "mosa/machine.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace {

constexpr std::string_view k_env = ":- module(phase4_env, [env_mark/1]).\nenv_mark(from_env).\n";
constexpr std::string_view k_exe = ":- module(phase4_env, [env_mark/1]).\nenv_mark(from_exe).\n";

bool write_text(const std::filesystem::path& file, std::string_view body) {
  std::error_code error;
  std::filesystem::create_directories(file.parent_path(), error);
  if (error) return false;
  std::ofstream out{file, std::ios::binary};
  out << body;
  return static_cast<bool>(out);
}

class EnvGuard {
 public:
  explicit EnvGuard(const std::filesystem::path& dir) {
    saved_ = mosa::environment_value("MOSA_LIB");
    had_ = !saved_.empty();
    ready_ = mosa::set_environment("MOSA_LIB", dir.string().c_str());
  }

  ~EnvGuard() {
    if (had_) static_cast<void>(mosa::set_environment("MOSA_LIB", saved_.c_str()));
    else mosa::unset_environment("MOSA_LIB");
  }

  [[nodiscard]] bool ready() const { return ready_; }

  EnvGuard(const EnvGuard&) = delete;
  EnvGuard& operator=(const EnvGuard&) = delete;

 private:
  std::string saved_;
  bool had_ = false;
  bool ready_ = false;
};

void check_mosa_lib_first() {
  const auto root = std::filesystem::path{MOSA_SOURCE_DIR} / "tests" / "tmp" / "library-env";
  const auto exe_file = mosa::executable_path().parent_path() / "lib" / "phase4_env.pl";
  mosa_test::check(write_text(root / "phase4_env.pl", k_env), "write MOSA_LIB fixture");
  mosa_test::check(write_text(exe_file, k_exe), "write exe lib fixture");
  {
    const EnvGuard guard{root};
    mosa_test::check(guard.ready(), "set MOSA_LIB");
    auto machine = mosa::Machine::create();
    const auto loaded = machine->query("use_module(library(phase4_env)).");
    const auto mark = machine->query("env_mark(from_env).");
    mosa_test::check(loaded.kind == mosa::StopKind::Success, "library loads");
    mosa_test::check(mark.kind == mosa::StopKind::Success, "MOSA_LIB clause");
  }
  std::filesystem::remove(exe_file);
  std::filesystem::remove_all(root);
}

}  // namespace

int main() {
  check_mosa_lib_first();
  return mosa_test::failures();
}
