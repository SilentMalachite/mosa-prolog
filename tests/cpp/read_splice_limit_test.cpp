// レビュー R-M05。read/2 は壊れた UTF-8 のバイトを一時的に U+FFFD に置き換えて読み直す（L6）。読み直しがヒープの
// 上限で止まっても、置き換えたバイトは元に戻る。前は戻す処理を飛ばしたので、後の get_char/2 が
// representation_error(character) ではなく U+FFFD を返した。
// 一時ファイルは tests/tmp/read-splice-limit に置く。上限は 16 MiB（2^21 語）で、150 万要素のリストは読むだけで超える。
// レビュー R-M07 も同じ上限の下で見る。終止符の後ろの 3 バイトの字（U+3000）がファイルの最初の塊（65,536 バイト）の
// 切れ目を跨いでも、read/2 はその終止符で止まる。前は次の塊でその字の頭からしか試さず、後ろの 20 MB の注釈まで
// 取り込んで上限で止まった。
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

// 終止符をバイト 65,534 に置き、後ろの U+3000 を 65,535 から 65,537 に跨がせる。
bool write_straddle_file(const std::filesystem::path& path) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  std::string text = "a('" + std::string(65534 - 5, 'x') + "').";
  text += "\xE3\x80\x80%";
  text += std::string(std::size_t{20} << 20, 'y');
  text += "\nb.\n";
  output.write(text.data(), static_cast<std::streamsize>(text.size()));
  return static_cast<bool>(output) && text.find('.') == 65534;
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
  {
    const std::filesystem::path straddle = root / "straddle.txt";
    mosa_test::check(write_straddle_file(straddle), "write the straddle file");
    auto machine = mosa::Machine::create(k_limit);
    const std::string goal = "open('" + straddle.generic_string() + "', read, S), read(S, T), close(S), functor(T, a, 1).";
    bool read = false;
    try {
      read = machine->query(goal).kind == mosa::StopKind::Success;
    } catch (const mosa::ResourceLimit&) {
      read = false;
    }
    mosa_test::check(read, "a full stop before a split U+3000 ends the read");
  }
  std::error_code error;
  std::filesystem::remove_all(root, error);
  return mosa_test::failures();
}
