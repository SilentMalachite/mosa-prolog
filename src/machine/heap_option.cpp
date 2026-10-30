#include "mosa/heap_option.hpp"

#include "mosa/exe_path.hpp"
#include "mosa/heap.hpp"
#include "mosa/path_text.hpp"

#include <limits>
#include <new>
#include <string>

namespace mosa {
namespace {

// 1 MiB = 2^17 語。
constexpr std::size_t k_words_per_mib_shift = 17;

}  // namespace

std::optional<std::size_t> parse_heap_max_mib(std::string_view text) {
  if (text.empty()) return std::nullopt;
  constexpr std::size_t k_largest = std::numeric_limits<std::size_t>::max() >> k_words_per_mib_shift;
  std::size_t mib = 0;
  for (const char ch : text) {
    if (ch < '0' || ch > '9') return std::nullopt;
    const auto digit = static_cast<std::size_t>(ch - '0');
    // 語数にしたときにあふれる値は、途中で打ち切る。
    if (mib > (k_largest - digit) / 10) return std::nullopt;
    mib = mib * 10 + digit;
  }
  if (mib == 0) return std::nullopt;
  return mib << k_words_per_mib_shift;
}

std::size_t resolve_heap_max(std::optional<std::string_view> option, std::optional<std::string_view> env) {
  if (option) {
    if (const auto words = parse_heap_max_mib(*option)) return *words;
  }
  if (env) {
    if (const auto words = parse_heap_max_mib(*env)) return *words;
  }
  return kDefaultHeapMax;
}

bool is_heap_exhaustion() noexcept {
  try {
    throw;
  } catch (const ResourceLimit&) {
    return true;
  } catch (const std::bad_alloc&) {
    return true;
  } catch (...) {
    return false;
  }
}

std::string_view heap_stop_key() noexcept {
  try {
    throw;
  } catch (const HeapReserveFailed&) {
    return "heap_reserve";
  } catch (...) {
    return "heap_limit_size";
  }
}

std::size_t heap_max_from_environment() {
  const auto value = environment_path("MOSA_HEAP_MAX");
  if (!value) return kDefaultHeapMax;
  const std::string text = utf8_from_path(*value);
  return resolve_heap_max(std::nullopt, std::string_view{text});
}

std::string heap_limit_mib_text(std::size_t limit_words) {
  constexpr std::size_t k_words_per_mib = std::size_t{1} << k_words_per_mib_shift;
  // 割ってから端数を足す。足してから割ると、最大の上限であふれる。
  const std::size_t mib = limit_words / k_words_per_mib + (limit_words % k_words_per_mib != 0 ? 1 : 0);
  return std::to_string(mib);
}

}
