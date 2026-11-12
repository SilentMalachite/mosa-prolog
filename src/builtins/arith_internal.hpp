#pragma once

// 7.9 の式の評価を分けて持つ。arith.cpp が葉と木の歩き方と is/2・算術比較の入口を、arith_int.cpp が整数の演算を、
// arith_float.cpp が浮動小数点数の関数を持つ。1 ファイル 800 行を超えないように分けてある。
// 述語のモードと誤りは arith.cpp の見出し。

#include "../machine/internal.hpp"

#include <cstdint>
#include <string_view>
#include <vector>

namespace mosa::arith {

struct Num {
  bool flt = false;
  std::int64_t ival = 0;
  double fval = 0;
};

struct Ev {
  bool ok = false;
  Num num;
};

inline void fail_eval(Impl& machine, const Goal& goal, std::string_view which) {
  raise_formal(machine, formal_evaluation(machine, which), goal.name, goal.arity);
}

inline void fail_integer(Impl& machine, const Goal& goal, Cell culprit) {
  raise_formal(machine, formal_type(machine, "integer", culprit), goal.name, goal.arity);
}

inline double as_float(Num num) { return num.flt ? num.fval : static_cast<double>(num.ival); }

inline bool both_int(const std::vector<Num>& args) {
  for (const Num& num : args) {
    if (num.flt) return false;
  }
  return true;
}

inline Ev from_int(std::int64_t value) {
  Ev ev;
  ev.ok = true;
  ev.num.ival = value;
  return ev;
}

inline Ev from_float(double value) {
  Ev ev;
  ev.ok = true;
  ev.num.flt = true;
  ev.num.fval = value;
  return ev;
}

// arith_int.cpp
[[nodiscard]] bool sub_i(std::int64_t a, std::int64_t b, std::int64_t& out);
[[nodiscard]] Ev eval_int_bin(Impl& machine, const Goal& goal, std::string_view name, Num left, Num right);
[[nodiscard]] Ev int_hat(Impl& machine, const Goal& goal, std::int64_t base, std::int64_t exp);
// abs、sign、min、max、gcd、msb、succ_or_zero、ビット演算、シフト。名前と引数の数が合わなければ ok でない Ev。
[[nodiscard]] Ev eval_int_fn(Impl& machine, const Goal& goal, std::string_view name, const std::vector<Num>& args);

}  // namespace mosa::arith
