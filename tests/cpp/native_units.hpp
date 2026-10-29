#pragma once

// native_units.cpp の手書きの生成関数。native_test が単位を組み立てて install_unit に渡す。
// 指紋は翻訳のときの見え方から native_test が計算するので、ここには置かない。

#include "mosa/runtime.hpp"

#include <cstddef>
#include <span>
#include <string_view>

namespace native_units {

// 生成関数が実際に走った回数。差し替えが効いたか外れたかを、答えの一致とは別に確かめる。
struct Hits {
  std::size_t app = 0;
  std::size_t nrev = 0;
  std::size_t chk = 0;
  std::size_t cutc = 0;
  std::size_t rel = 0;
  std::size_t same = 0;
  std::size_t twice = 0;
};

Hits& hits();

// 単位 1: app/3、nrev/2、chk/1、cutc/1。entries の順は native_test の kListEntries と同じ。
inline constexpr std::string_view kListSource =
    "app([], L, L).\n"
    "app([H|T], L, [H|R]) :- app(T, L, R).\n"
    "nrev([], []).\n"
    "nrev([H|T], R) :- nrev(T, S), app(S, [H], R).\n"
    "chk(X) :- app(_, X, [a]), atom_length(X, _), app(_, _, X).\n"
    "cutc(first).\n"
    "cutc(X) :- !, X = second.\n"
    "cutc(third).\n";

void app_3(mosa::Machine::Impl& m, std::uint64_t frame, std::uint32_t state);
void nrev_2(mosa::Machine::Impl& m, std::uint64_t frame, std::uint32_t state);
void chk_1(mosa::Machine::Impl& m, std::uint64_t frame, std::uint32_t state);
void cutc_1(mosa::Machine::Impl& m, std::uint64_t frame, std::uint32_t state);
const mosa::rt::UnitTag& list_tag();
std::span<const std::string_view> list_atoms();
std::span<const mosa::rt::NativeCallee> list_calls();

// 単位 2: 指示のゴールの中の op/3 が rel/1 の IR を変える。same/1 は変わらない。
// 翻訳のときの読み込みは指示のゴールを飛ばすので、翻訳器の見え方は 2 行目の無い原文である。
inline constexpr std::string_view kOpSource =
    ":- op(200, xfx, ===>).\n"
    ":- true, op(700, xfx, ===>).\n"
    "rel(a ===> b - c).\n"
    "same(f(a, 2.5, -3)).\n";
inline constexpr std::string_view kOpTranslatedView =
    ":- op(200, xfx, ===>).\n"
    "rel(a ===> b - c).\n"
    "same(f(a, 2.5, -3)).\n";

void rel_1(mosa::Machine::Impl& m, std::uint64_t frame, std::uint32_t state);
void same_1(mosa::Machine::Impl& m, std::uint64_t frame, std::uint32_t state);
const mosa::rt::UnitTag& op_tag();
std::span<const std::string_view> op_atoms();

// 単位 3: meta_predicate の述語。付いたら節をそのまま IR で動かすだけの関数で、走れば数が増える。
inline constexpr std::string_view kMetaSource =
    ":- meta_predicate(twice(0)).\n"
    "twice(G) :- call(G), call(G).\n";

void twice_1(mosa::Machine::Impl& m, std::uint64_t frame, std::uint32_t state);
const mosa::rt::UnitTag& meta_tag();

}
