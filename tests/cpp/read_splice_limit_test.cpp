// レビュー R-M05。read/2 は壊れた UTF-8 のバイトを一時的に U+FFFD に置き換えて読み直す（L6）。読み直しがヒープの
// 上限で止まっても、置き換えたバイトは元に戻る。前は戻す処理を飛ばしたので、後の get_char/2 が
// representation_error(character) ではなく U+FFFD を返した。
// 一時ファイルは tests/tmp/read-splice-limit に置く。上限は 16 MiB（2^21 語）で、150 万要素のリストは読むだけで超える。
#include "harness.hpp"
#include "mosa/heap.hpp"
#include "mosa/machine.hpp"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>

namespace {

constexpr std::size_t k_limit = std::size_t{16} << 17;

bool write_bad_file(const std::filesystem::path& path) {
  std::error_code error;
  std::filesystem::create_directories(path.parent_path(), error);
  if (error) return false;
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  std::string text = "a(\xFF, [";
  for (int index = 0; index < 1500000; ++index) text += index == 0 ? "1" : ",1";
  text += "]).\n";
  output.write(text.data(), static_cast<std::streamsize>(text.size()));
  return static_cast<bool>(output);
}

bool stops_at_limit(mosa::Machine& machine, std::string_view goal) {
  try {
    static_cast<void>(machine.query(goal));
  } catch (const mosa::ResourceLimit&) {
    return true;
  }
  return false;
}

}  // namespace

int main() {
  const std::filesystem::path root = std::filesystem::path{MOSA_SOURCE_DIR} / "tests" / "tmp" / "read-splice-limit";
  const std::filesystem::path file = root / "bad.txt";
  mosa_test::check(write_bad_file(file), "write the file");
  {
    auto machine = mosa::Machine::create(k_limit);
    const std::string open = "open('" + file.generic_string() + "', read, _, [alias(rsl)]).";
    mosa_test::check(machine->query(open).kind == mosa::StopKind::Success, "open the file");
    mosa_test::check(stops_at_limit(*machine, "read(rsl, _)."), "read stops at the heap limit");
    // a と ( の次は壊れたバイト。元に戻っていれば representation_error になる。
    const mosa::Stop stop = machine->query(
        "get_char(rsl, _), get_char(rsl, _), "
        "catch((get_char(rsl, _), fail), error(representation_error(character), _), true).");
    mosa_test::check(stop.kind == mosa::StopKind::Success, "the bad byte is back after the limit");
  }
  std::error_code error;
  std::filesystem::remove_all(root, error);
  return mosa_test::failures();
}
