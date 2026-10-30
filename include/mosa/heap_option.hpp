#pragma once

// ヒープの上限の決め方（ADR 0010）。--heap-max → MOSA_HEAP_MAX → kDefaultHeapMax の順。単位は MiB の整数。
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace mosa {

// 1 以上の 10 進整数 MiB を語数にする。0、符号、空、数字以外、桁あふれは nullopt。
[[nodiscard]] std::optional<std::size_t> parse_heap_max_mib(std::string_view text);

// option、env の順に、解釈できた最初の値（語）。どちらも無いか解釈できなければ kDefaultHeapMax。
// 解釈できない option は使い方の誤りなので、呼んだ側が先に parse_heap_max_mib で検査する。
[[nodiscard]] std::size_t resolve_heap_max(std::optional<std::string_view> option,
                                           std::optional<std::string_view> env);

// catch (...) の中で呼ぶ。今の例外がヒープの上限（ResourceLimit）か、OS がメモリを渡さなかった（std::bad_alloc。
// trail・選択点スタック・extras の std::vector）なら true。どちらも同じ止め方にするので、判定を一か所に置く。
[[nodiscard]] bool is_heap_exhaustion() noexcept;

// catch (...) の中で呼ぶ。マシンを作るときに止まった文面の鍵。予約を断られた（HeapReserveFailed）なら
// heap_reserve、ほかは heap_limit_size。どちらも引数は上限の MiB 1 つ。
[[nodiscard]] std::string_view heap_stop_key() noexcept;

// 環境変数 MOSA_HEAP_MAX を読んだ上限（語）。無いか解釈できなければ kDefaultHeapMax。
[[nodiscard]] std::size_t heap_max_from_environment();

// 上限（語）を MiB に直した十進の文字列。端数は切り上げる（小さな上限でも 0 にならない）。
[[nodiscard]] std::string heap_limit_mib_text(std::size_t limit_words);

}
