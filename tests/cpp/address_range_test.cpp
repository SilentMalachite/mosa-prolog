#include "harness.hpp"
#include "mosa/address_range.hpp"

#include <cstddef>
#include <cstdint>
#include <utility>

namespace {

constexpr std::size_t kWordsInMiB = (std::size_t{1} << 20) / sizeof(std::uint64_t);

void range_reserve_large() {
  auto range = mosa::AddressRange::reserve(std::size_t{1} << 32);
  mosa_test::check(range.has_value(), "reserve 4GiB");
  if (!range) return;
  mosa_test::check(range->committed_bytes() == 0, "nothing committed after reserve");
  mosa_test::check(range->reserved_bytes() >= (std::size_t{1} << 32), "reserved size");
}

void range_commit_grows() {
  auto range = mosa::AddressRange::reserve(std::size_t{1} << 32);
  if (!range) return mosa_test::check(false, "reserve for grow");
  mosa_test::check(range->commit(std::size_t{1} << 20), "commit 1MiB");
  std::uint64_t* const before = range->words();
  before[0] = 0x1122334455667788ULL;
  before[kWordsInMiB - 1] = 42;
  mosa_test::check(range->commit(std::size_t{1} << 24), "commit 16MiB");
  mosa_test::check(range->words() == before, "base does not move");
  mosa_test::check(range->words()[0] == 0x1122334455667788ULL && range->words()[kWordsInMiB - 1] == 42,
                   "contents survive growth");
  mosa_test::check(range->commit(std::size_t{1} << 20), "shrinking request is a no-op");
  mosa_test::check(range->committed_bytes() >= (std::size_t{1} << 24), "committed stays");
}

void range_commit_over() {
  auto range = mosa::AddressRange::reserve(std::size_t{1} << 30);
  if (!range) return mosa_test::check(false, "reserve for over");
  mosa_test::check(!range->commit(range->reserved_bytes() + 1), "commit beyond reservation fails");
  mosa_test::check(range->committed_bytes() == 0, "failed commit commits nothing");
}

void range_move() {
  auto range = mosa::AddressRange::reserve(std::size_t{1} << 30);
  if (!range) return mosa_test::check(false, "reserve for move");
  mosa_test::check(range->commit(4096), "commit before move");
  range->words()[0] = 7;
  std::uint64_t* const base = range->words();
  mosa::AddressRange moved(std::move(*range));
  mosa_test::check(moved.words() == base && moved.words()[0] == 7, "moved-to keeps the words");
  mosa_test::check(range->words() == nullptr && range->reserved_bytes() == 0, "moved-from owns nothing");
  mosa::AddressRange other = std::move(moved);
  mosa_test::check(other.words() == base, "move construct again");
}

}

int main() {
  range_reserve_large();
  range_commit_grows();
  range_commit_over();
  range_move();
  return mosa_test::failures() == 0 ? 0 : 1;
}
