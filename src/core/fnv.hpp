#pragma once

// 64 ビットの FNV-1a。節の指紋（src/machine/native.cpp、ADR 0009）と mosac のキャッシュの鍵（src/codegen/cache.cpp）が
// 同じ式を使う。指紋は翻訳器と読み込み器が別のプロセスで同じ値を出さねばならないので、式を 1 か所に置く。
// テスト: tests/cpp/native_test.cpp（指紋）、tests/mosac/build_test.cmake の cache_reuse（鍵）

#include <cstdint>
#include <string_view>

namespace mosa {

inline constexpr std::uint64_t kFnvBasis = 0xcbf29ce484222325ULL;
inline constexpr std::uint64_t kFnvPrime = 0x100000001b3ULL;

[[nodiscard]] constexpr std::uint64_t fnv1a_byte(std::uint64_t hash, std::uint8_t byte) {
  return (hash ^ byte) * kFnvPrime;
}

// seed から続けて bytes を入れる。種を渡せば、複数の断片を 1 つの値にまとめられる。
[[nodiscard]] constexpr std::uint64_t fnv1a(std::string_view bytes, std::uint64_t seed = kFnvBasis) {
  for (const char unit : bytes) seed = fnv1a_byte(seed, static_cast<std::uint8_t>(unit));
  return seed;
}

// 公開された FNV-1a 64 の試験値。式を書き換えて指紋がずれれば、どの翻訳単位もコンパイルの時点で止まる。
static_assert(fnv1a("") == kFnvBasis && fnv1a("a") == 0xaf63dc4c8601ec8cULL);

}  // namespace mosa
