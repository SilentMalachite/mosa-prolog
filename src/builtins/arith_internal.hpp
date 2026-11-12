#pragma once

// 7.9 の式の評価を分けて持つ。arith.cpp が葉と木の歩き方と is/2・算術比較の入口を、arith_int.cpp が整数の演算を、
// arith_big.cpp が int64 に入らない整数（Big、ADR 0025）の値の置き場と遅い道を、arith_float.cpp が浮動小数点数の
// 関数を持つ。1 ファイル 800 行を超えないように分けてある。述語のモードと誤りは arith.cpp の見出し。

#include "../machine/internal.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>
#include <vector>

namespace mosa::arith {

// Big は値を持たず、Impl::eval_bigs の添字を ival に持つ。BigInt を値で持つと、int64 と double の道でも Num の写しの
// たびに std::vector の写しと解放の検査が入る（タスク 12 の bench が見る道）。Big の値はいつも int64 の外にある
// （from_big が正規形にする）。
struct Num {
  bool flt = false;
  bool big = false;  // flt が偽のときだけ真になりうる
  std::int64_t ival = 0;
  double fval = 0;
};
static_assert(std::is_trivially_copyable_v<Num>);
static_assert(sizeof(Num) == 2 * sizeof(std::int64_t) + sizeof(double));

struct Ev {
  bool ok = false;
  Num num;
};

// 評価の入口（is/2、算術比較、tab/1,2 の式、生成コードの入口）に 1 つ置く。比較は 2 つの式を評価してから比べるので、
// 置き場は式ごとではなく入口ごとに要る。入口を出るとき（ResourceLimit で抜けるときも）、入ったときの長さへ戻す。
// 入口の後に置いた値はみな消えるので、語数も入ったときの値に戻せばよい。数え直さないので、巻き戻しの中でも投げない。
class BigScope {
 public:
  explicit BigScope(Impl& machine)
      : machine_(machine), base_(machine.eval_bigs.size()), words_(machine.eval_big_words) {}
  ~BigScope() {
    if (machine_.eval_bigs.size() > base_) {
      machine_.eval_bigs.erase(machine_.eval_bigs.begin() + static_cast<std::ptrdiff_t>(base_),
                               machine_.eval_bigs.end());
    }
    machine_.eval_big_words = words_;
  }
  BigScope(const BigScope&) = delete;
  BigScope& operator=(const BigScope&) = delete;

 private:
  Impl& machine_;
  std::size_t base_;
  std::size_t words_;
};

// Big の値 1 つが置き場で占めると見なす語数。ヒープの箱と同じく、頭の語と端数の語を足す（check_room と同じ式）。
inline std::size_t big_words(std::uint64_t bits) { return static_cast<std::size_t>(bits / 64 + 2); }

inline void fail_eval(Impl& machine, const Goal& goal, std::string_view which) {
  raise_formal(machine, formal_evaluation(machine, which), goal.name, goal.arity);
}

inline void fail_integer(Impl& machine, const Goal& goal, Cell culprit) {
  raise_formal(machine, formal_type(machine, "integer", culprit), goal.name, goal.arity);
}

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

inline Ev from_num(Num num) {
  Ev ev;
  ev.ok = true;
  ev.num = num;
  return ev;
}

// num.big のときだけ。評価の間の置き場の値を写さずに読む。
[[nodiscard]] inline const BigInt& big_ref(const Impl& machine, Num num) {
  return machine.eval_bigs[static_cast<std::size_t>(num.ival)];
}

// 整数の Num が負か。Big の ival は添字なので、置き場の値の符号を見る。
[[nodiscard]] inline bool int_negative(const Impl& machine, Num num) {
  return num.big ? big_ref(machine, num).negative() : num.ival < 0;
}

// arith_big.cpp
// 正規形にする。int64 に収まれば int64 の Num、収まらなければ eval_bigs に置いた Big の Num。
[[nodiscard]] Ev from_big(Impl& machine, BigInt value);
// 数の Num を double にする。Big は最近接偶数に丸める。double の範囲の外なら、旗 float_overflow が error のときは
// evaluation_error(float_overflow) を投げて false、そうでなければ符号の付いた無限大にする（iso-notes の
// 「8.7, 9.3.8 and 9.3.9」の既定 A）。
[[nodiscard]] bool to_float(Impl& machine, const Goal& goal, Num num, double& out);
// 整数の値の大小（-1、0、1）。
[[nodiscard]] int compare_int(const Impl& machine, Num left, Num right);
// 結果の絶対値のビット数の見積もりが、ヒープの残りの語を越えるなら ResourceLimit("heap_limit") を投げる。
void check_room(const Impl& machine, std::uint64_t bits);
// + - * // rem div mod で、どちらかが Big のとき。引数はどちらも整数。
[[nodiscard]] Ev big_bin(Impl& machine, const Goal& goal, std::string_view name, Num left, Num right);
// 整数どうしの / で、どちらかが Big のとき。
[[nodiscard]] Ev big_divide(Impl& machine, const Goal& goal, Num left, Num right);
// 整数の Num が奇数か。Big は値の最下位のビットを見る。
[[nodiscard]] bool int_odd(const Impl& machine, Num num);
// 整数の ^ で、どちらかが Big か int64 の結果があふれたとき。exp は負でない。
[[nodiscard]] Ev big_hat(Impl& machine, Num base, Num exp);
// << と >> で、値かシフトの数が Big か int64 の左シフトがあふれたとき。count は負でなく、value は 0 でない。
[[nodiscard]] Ev big_shift(Impl& machine, std::string_view name, Num value, Num count);
// gcd で、どちらかが Big のとき。
[[nodiscard]] Ev big_gcd(Impl& machine, Num left, Num right);
// /\ \/ xor で、どちらかが Big のとき。
[[nodiscard]] Ev big_bits(Impl& machine, std::string_view name, Num left, Num right);
// 有限で整数値の double を整数の Num にする（9.1.6 の丸めの結果）。
[[nodiscard]] Ev from_integral(Impl& machine, double value);
// Num を項にする。Big の箱はここでだけ作る。
[[nodiscard]] Cell num_cell(Impl& machine, Num num);

// arith_int.cpp
[[nodiscard]] Ev negate_int(Impl& machine, Num num);
[[nodiscard]] Ev eval_int_bin(Impl& machine, const Goal& goal, std::string_view name, Num left, Num right);
// 整数どうしの ^。base と exp はどちらも整数（Big を含む）。
[[nodiscard]] Ev int_hat(Impl& machine, const Goal& goal, Num base, Num exp);
// abs、sign、min、max、gcd、msb、succ_or_zero、ビット演算、シフト。名前と引数の数が合わなければ ok でない Ev。
[[nodiscard]] Ev eval_int_fn(Impl& machine, const Goal& goal, std::string_view name, const std::vector<Num>& args);

}  // namespace mosa::arith
