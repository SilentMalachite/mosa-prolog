// 8.6.1 is/2 と 8.7.1 算術比較（=:=、=\=、<、=<、>、>=）が使う式の評価（7.9）。is(?term, @evaluable)、
// =:=(@evaluable, @evaluable) ほか。誤り: instantiation_error, type_error(evaluable, F/A),
// type_error(integer, X), type_error(float, X), type_error(integer_expression, E)（tab/1,2 が使う
// eval_int_expression）, evaluation_error(zero_divisor | undefined | int_overflow | float_overflow),
// resource_error(cyclic_term)（循環する式、iso-notes 7.3.3）。整数に上限は無い（旗 bounded は false、ADR 0025）。
// int_overflow を出すのは、無限大を整数にする丸めと、tab/1,2 の Big（タスク 8 まで）だけである。** はいつも float
// を返し、double にできない Big の引数と結果は float_overflow にする。ヒープに入らない大きさの結果は
// ResourceLimit("heap_limit") で止める（check_room。<< と ^ と + - * は計算の前に見積もる）。テスト: tests/pl/arith.pl,
// tests/pl/bigint.pl, tests/pl/native.pl, tests/stack/deep_builtins.pl, tests/cpp/evaluable_test.cpp,
// tests/cpp/bigint_heap_test.cpp, tests/cpp/bigint_limit_test.cpp

#include "arith_internal.hpp"
#include "evaluable.hpp"
#include "mosa/runtime.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace mosa {

// as_int が真なら、out は整数にする有限の整数値（9.1.6 の丸めの結果）。
int float_op(Impl& machine, const Goal& goal, std::string_view name, std::span<const double> in, double& out,
             bool& as_int);

namespace arith {
namespace {

void fail_inst(Impl& machine, const Goal& goal) {
  raise_formal(machine, formal_instantiation(machine), goal.name, goal.arity);
}

void fail_evaluable(Impl& machine, const Goal& goal, Cell culprit) {
  raise_formal(machine, formal_type(machine, "evaluable", culprit), goal.name, goal.arity);
}

// 循環する式（7.3.3 は未定義）は resource_error(cyclic_term)（iso-notes 7.3.3、レビュー L20 の持ち主の決定 A）。
void fail_cyclic(Impl& machine, const Goal& goal) {
  raise_formal(machine, formal_resource(machine, "cyclic_term"), goal.name, goal.arity);
}

Ev number_of(Impl& machine, Cell cell) {
  cell = machine.heap.deref(cell);
  Ev ev;
  if (cell.tag() == Tag::Int || cell.tag() == Tag::IntBox) {
    ev.ok = true;
    ev.num.ival = machine.heap.integer_value(cell);
    return ev;
  }
  if (cell.tag() == Tag::Float) {
    ev.ok = true;
    ev.num.flt = true;
    ev.num.fval = machine.heap.float_value(cell);
    return ev;
  }
  // Big は評価の間ヒープの外に写して持つ。式の途中で箱を指したままにしない。
  if (cell.tag() == Tag::Big) return from_big(machine, machine.heap.big_value(cell));
  return ev;
}

Cell indicator(Impl& machine, std::uint32_t name, std::uint16_t arity) {
  return cell_functor(machine, name, arity);
}

bool float_bad(Impl& machine, const Goal& goal, double value, bool inputs_finite) {
  if (std::isnan(value)) {
    if (!machine.float_undefined_error) return false;
    fail_eval(machine, goal, "undefined");
    return true;
  }
  if (!std::isfinite(value) && inputs_finite) {
    if (!machine.float_overflow_error) return false;
    fail_eval(machine, goal, "float_overflow");
    return true;
  }
  return false;
}

bool integer_exponent(Num right, std::int64_t& exp) {
  if (!right.flt) {
    exp = right.ival;
    return true;
  }
  if (!std::isfinite(right.fval) || right.fval != std::trunc(right.fval)) return false;
  if (right.fval < static_cast<double>(std::numeric_limits<std::int64_t>::min()) || right.fval >= 0x1p63) {
    return false;
  }
  exp = static_cast<std::int64_t>(right.fval);
  return true;
}

// 負の底の整数乗は、絶対値のべきに符号を付けてから、負の指数は逆数にする。奇偶は指数の整数の値から取る
// （mag は double なので、2^53 を越えるとどれも偶数に見える）。
double negative_base_pow(double base, double mag, bool odd, bool invert) {
  const double raised = std::pow(std::fabs(base), mag);
  const double signed_value = odd ? -raised : raised;
  if (!invert) return signed_value;
  if (signed_value == 0) return std::copysign(std::numeric_limits<double>::infinity(), signed_value);
  return 1.0 / signed_value;
}

// 負の底 a の冪で、指数が整数値のとき。整数値でない指数は偽で、呼び手が undefined にする。b は right の double。
bool negative_base_value(const Impl& machine, double a, Num right, double b, double& value) {
  if (right.big) {
    value = negative_base_pow(a, std::fabs(b), int_odd(machine, right), b < 0);
    return true;
  }
  std::int64_t exp = 0;
  if (!integer_exponent(right, exp)) return false;
  const bool invert = exp < 0;
  const auto mag = invert ? static_cast<std::uint64_t>(0) - static_cast<std::uint64_t>(exp)
                          : static_cast<std::uint64_t>(exp);
  value = negative_base_pow(a, static_cast<double>(mag), (mag % 2U) != 0U, invert);
  return true;
}

Ev eval_float_bin(Impl& machine, const Goal& goal, std::string_view name, Num left, Num right) {
  double a = 0;
  double b = 0;
  if (!to_float(machine, goal, left, a) || !to_float(machine, goal, right, b)) return {};
  const bool finite = std::isfinite(a) && std::isfinite(b);
  double value = 0;
  if (name == "+") value = a + b;
  else if (name == "-") value = a - b;
  else if (name == "*") value = a * b;
  else if (name == "/") {
    if (b == 0 && machine.float_zero_div_error) {
      fail_eval(machine, goal, "zero_divisor");
      return {};
    }
    value = a / b;
  } else if (name == "**" || name == "^") {
    // 9.3.1.3 d の本文どおり、0 の負の冪は undefined（iso-notes 9.3.1.3 d の決定 A）。pow は無限大を返すので先に止める。
    if (a == 0 && b < 0) {
      if (!machine.float_undefined_error) return from_float(std::numeric_limits<double>::quiet_NaN());
      fail_eval(machine, goal, "undefined");
      return {};
    }
    if (a < 0) {
      if (!negative_base_value(machine, a, right, b, value)) {
        if (!machine.float_undefined_error) return from_float(std::numeric_limits<double>::quiet_NaN());
        fail_eval(machine, goal, "undefined");
        return {};
      }
    } else {
      value = std::pow(a, b);
    }
  } else {
    return {};
  }
  if (float_bad(machine, goal, value, finite)) return {};
  return from_float(value);
}

// 9.1.6 はこの 6 つを F → I または F → F と定める。整数を黙って float に直さず型エラーにする（iso-notes 9.1.6 の決定 A）。
bool float_only(std::string_view name) {
  return name == "floor" || name == "ceiling" || name == "round" || name == "truncate" ||
         name == "float_integer_part" || name == "float_fractional_part";
}

Ev eval_float_wrap(Impl& machine, const Goal& goal, std::string_view name, const std::vector<Num>& args) {
  std::array<double, 2> inputs{};
  if (args.size() > inputs.size()) return {};
  if (args.size() == 1 && !args[0].flt && float_only(name)) {
    raise_formal(machine, formal_type(machine, "float", num_cell(machine, args[0])), goal.name, goal.arity);
    return {};
  }
  // integer/1 の整数はそのまま返す。double を通すと 2^53 を超える値が丸まり、max_integer は int_overflow になる
  // （L2 の敵対的レビュー A2）。
  if (args.size() == 1 && !args[0].flt && name == "integer") return from_num(args[0]);
  for (std::size_t index = 0; index < args.size(); ++index) {
    if (!to_float(machine, goal, args[index], inputs[index])) return {};
  }
  double value = 0;
  bool as_int = false;
  const int status = float_op(machine, goal, name, std::span<const double>{inputs.data(), args.size()}, value, as_int);
  if (status == 0) return {};
  if (status < 0) return {};
  if (as_int) return from_integral(machine, value);
  return from_float(value);
}

Ev eval_known(Impl& machine, const Goal& goal, std::string_view name, const std::vector<Num>& args) {
  if (name == "+" && args.size() == 1) return from_num(args[0]);
  if (name == "-" && args.size() == 1) {
    if (args[0].flt) return from_float(args[0].fval == 0 ? std::copysign(0.0, -std::copysign(1.0, args[0].fval))
                                                         : -args[0].fval);
    return negate_int(machine, args[0]);
  }
  if ((name == "+" || name == "-" || name == "*") && args.size() == 2) {
    if (both_int(args)) return eval_int_bin(machine, goal, name, args[0], args[1]);
    return eval_float_bin(machine, goal, name, args[0], args[1]);
  }
  if (name == "/" && args.size() == 2) {
    // int64 どうしは前のまま double にしてから割る。Big を含む整数どうしは商を 1 回だけ丸める（big_divide）。
    if (both_int(args) && (args[0].big || args[1].big)) return big_divide(machine, goal, args[0], args[1]);
    return eval_float_bin(machine, goal, name, args[0], args[1]);
  }
  if ((name == "//" || name == "rem" || name == "div" || name == "mod") && args.size() == 2) {
    return eval_int_bin(machine, goal, name, args[0], args[1]);
  }
  if (name == "^" && args.size() == 2 && !args[0].flt && !args[1].flt) return int_hat(machine, goal, args[0], args[1]);
  if ((name == "**" || name == "^") && args.size() == 2) {
    return eval_float_bin(machine, goal, "**", args[0], args[1]);
  }
  if (const Ev extra = eval_int_fn(machine, goal, name, args); extra.ok || machine.exception || machine.eval_abort) {
    return extra;
  }
  if (const Ev floated = eval_float_wrap(machine, goal, name, args);
      floated.ok || machine.exception || machine.eval_abort) {
    return floated;
  }
  return {};
}

Ev constant_of(std::string_view name) {
  if (name == "pi") return from_float(std::acos(-1.0));
  if (name == "e") return from_float(std::exp(1.0));
  if (name == "inf") return from_float(std::numeric_limits<double>::infinity());
  if (name == "nan") return from_float(std::numeric_limits<double>::quiet_NaN());
  if (name == "epsilon") return from_float(std::numeric_limits<double>::epsilon());
  if (name == "max_tagged_integer") return from_int((std::int64_t{1} << 60) - 1);
  return {};
}

// 子を持たない式と、評価できない関数子の誤り。評価できる複合項は nullopt で、呼び手が引数を評価する。
// 7.12.1 は誤りの条件が重なったときの選択を任せる。外側の関数子を引数より先に見る（iso-notes 7.9.1 の決定 B）。
std::optional<Ev> eval_leaf(Impl& machine, const Goal& goal, Cell cell) {
  if (machine.heap.unbound(cell)) {
    fail_inst(machine, goal);
    return Ev{};
  }
  if (const Ev num = number_of(machine, cell); num.ok) return num;
  if (cell.tag() == Tag::Atom) {
    const auto text = machine.atoms.name(static_cast<std::uint32_t>(cell.raw() >> 3));
    if (const Ev constant = constant_of(text); constant.ok) return constant;
    fail_evaluable(machine, goal, indicator(machine, static_cast<std::uint32_t>(cell.raw() >> 3), 0));
    return Ev{};
  }
  if (cell.tag() != Tag::Str) {
    fail_evaluable(machine, goal, cell);
    return Ev{};
  }
  const auto name = machine.atoms.name(machine.heap.functor_atom(cell));
  if (!is_evaluable(name, machine.heap.functor_arity(cell))) {
    fail_evaluable(machine, goal, indicator(machine, machine.heap.functor_atom(cell), machine.heap.functor_arity(cell)));
    return Ev{};
  }
  return std::nullopt;
}

// 複合項の値を求めたら、その引数の Big は要らない。値が Big ならそれだけを mark の位置へ寄せて残す。こうして
// eval_bigs の長さは式の木の深さほどに収まり、Big の葉や途中の値をたくさん持つ式でも全部を積み続けない。
// int64 の道では eval_bigs が伸びないので、比較 1 回で抜ける。語数は、消す前の mark 以降の全部を引き、残す値の分を
// 足し直す（上書きされる pool[mark] も引く側に入る）。
Ev settle_bigs(Impl& machine, std::size_t mark, Ev ev) {
  std::vector<BigInt>& pool = machine.eval_bigs;
  if (pool.size() <= mark) return ev;
  for (std::size_t i = mark; i < pool.size(); ++i) machine.eval_big_words -= arith::big_words(pool[i].bit_length());
  std::size_t keep = mark;
  if (ev.ok && ev.num.big) {
    const auto at = static_cast<std::size_t>(ev.num.ival);
    machine.eval_big_words += arith::big_words(pool[at].bit_length());
    if (at != mark) pool[mark] = std::move(pool[at]);
    ev.num.ival = static_cast<std::int64_t>(mark);
    keep = mark + 1;
  }
  pool.erase(pool.begin() + static_cast<std::ptrdiff_t>(keep), pool.end());
  return ev;
}

// 引数を評価し終えた複合項に、その関数子を当てる。
Ev eval_apply(Impl& machine, const Goal& goal, Cell cell, const std::vector<Num>& args) {
  const auto name = machine.atoms.name(machine.heap.functor_atom(cell));
  Ev known = eval_known(machine, goal, name, args);
  if (known.ok || machine.exception || machine.eval_abort) return known;
  fail_evaluable(machine, goal, indicator(machine, machine.heap.functor_atom(cell), machine.heap.functor_arity(cell)));
  return {};
}

// 評価途中の複合項。評価し終えた引数は nums に続けて置き、base は自分の分の始まり。
struct EvalFrame {
  Cell cell;
  std::size_t base;
  std::size_t bigs;  // 入ったときの eval_bigs の長さ（settle_bigs の mark）
};

// この深さまでは C++ の再帰で評価する。より深い式は手で積んで評価する（ADR 0011 の改訂 2）。
constexpr int kShallow = 64;

// 循環する式の見張り（Brent の方法を根からの道に当てる）。道の各段は下の段の引数なので、見張りの段は入る段の
// 祖先で、同じ複合項なら式は循環している。終わらない評価は、同じ部分項を毎回同じに評価するので、ある段から先の
// 消えない段は同じ周期で並ぶ。見張りは入った段の数が 1, 2, 4, … に届くたびにその段へ移り、その段が脇道ごと
// 評価を終えれば親へ下がるので、いずれ消えない段に留まり、窓が 1 周期より長くなればその周期のうちに同じ段と
// 出会う。比べるのは入るたびの 1 回だけで、見つけるまでの手間は循環しない部分と 1 周期の評価の数倍に収まる。
// 前は深さが倍になるたびに真ん中の段を探したので、閉路の中に深い脇道があると真ん中が脇道に落ちて見逃し続け、
// 手間が脇道の深さの 2 乗になった（レビュー L20 の敵対的レビュー A1）。C++ の再帰と eval_deep で 1 つを共有する。
// 再帰の側だけ深さで見張りを移すと、循環しない部分が 33〜63 段の閉路を 31 周まで回った（再確認の B1）。
struct CycleWatch {
  Cell watched{};
  std::size_t watched_depth = 0;
  std::size_t entered = 0;
  std::size_t window = 1;

  // 深さ depth（根は 0）の複合項 cell に入る。
  [[nodiscard]] bool enter(Cell cell, std::size_t depth) {
    if (depth == 0) {
      watched = cell;
      return false;
    }
    if (cell.raw() == watched.raw()) return true;
    if (++entered == window) {
      watched = cell;
      watched_depth = depth;
      entered = 0;
      window *= 2;
    }
    return false;
  }

  // 深さ depth の複合項 cell の子が評価を終えた。見張りがその子の側にあれば、この段へ下げる。
  void returned(Cell cell, std::size_t depth) {
    if (watched_depth > depth) {
      watched = cell;
      watched_depth = depth;
    }
  }
};

// 後順に評価する。引数は左から評価するので、最初に投げる誤りは再帰と同じになる。root は深さ depth で、
// eval_at が見張りに入れ終えている。前は循環する式で stack が --heap-max の外で限りなく育った（レビュー L20）。
Ev eval_deep(Impl& machine, const Goal& goal, Cell root, std::size_t depth, CycleWatch& watch) {
  std::vector<EvalFrame> stack{{root, 0, machine.eval_bigs.size()}};
  std::vector<Num> nums;
  while (true) {
    const EvalFrame top = stack.back();
    const std::size_t done = nums.size() - top.base;
    if (done < machine.heap.functor_arity(top.cell)) {
      const Cell next = machine.heap.deref(machine.heap.arg(top.cell, static_cast<std::uint16_t>(done)));
      if (const auto leaf = eval_leaf(machine, goal, next)) {
        if (!leaf->ok) return {};
        nums.push_back(leaf->num);
      } else {
        stack.push_back(EvalFrame{next, nums.size(), machine.eval_bigs.size()});
        if (watch.enter(next, depth + stack.size() - 1)) {
          fail_cyclic(machine, goal);
          return {};
        }
      }
      continue;
    }
    const std::vector<Num> args(nums.begin() + static_cast<std::ptrdiff_t>(top.base), nums.end());
    const Ev ev = settle_bigs(machine, top.bigs, eval_apply(machine, goal, top.cell, args));
    if (!ev.ok) return ev;
    nums.resize(top.base);
    stack.pop_back();
    if (stack.empty()) return ev;
    watch.returned(stack.back().cell, depth + stack.size() - 1);
    nums.push_back(ev.num);
  }
}

Ev eval_at(Impl& machine, const Goal& goal, Cell cell, int depth, CycleWatch& watch) {
  cell = machine.heap.deref(cell);
  if (auto leaf = eval_leaf(machine, goal, cell)) return *leaf;
  const auto level = static_cast<std::size_t>(depth);
  if (watch.enter(cell, level)) {
    fail_cyclic(machine, goal);
    return {};
  }
  if (depth >= kShallow) return eval_deep(machine, goal, cell, level, watch);
  const auto arity = machine.heap.functor_arity(cell);
  const std::size_t mark = machine.eval_bigs.size();
  std::vector<Num> args;
  args.reserve(arity);
  for (std::uint16_t index = 0; index < arity; ++index) {
    const Ev ev = eval_at(machine, goal, machine.heap.arg(cell, index), depth + 1, watch);
    if (!ev.ok) return {};
    watch.returned(cell, level);
    args.push_back(ev.num);
  }
  return settle_bigs(machine, mark, eval_apply(machine, goal, cell, args));
}

Ev eval_expr(Impl& machine, const Goal& goal, Cell cell) {
  CycleWatch watch;
  return eval_at(machine, goal, cell, 0, watch);
}

bool order_holds(int order, int mode) {
  if (mode == -2) return order < 0;
  if (mode == -1) return order <= 0;
  if (mode == 0) return order == 0;
  if (mode == 1) return order >= 0;
  return order > 0;
}

// 比べた結果を holds に置く。Big を double にできずに誤りを投げたら false。
bool cmp_nums(Impl& machine, const Goal& goal, Num left, Num right, int mode, bool& holds) {
  int order = 0;
  if (left.flt || right.flt) {
    double a = 0;
    double b = 0;
    if (!to_float(machine, goal, left, a) || !to_float(machine, goal, right, b)) return false;
    // NaN との比較は大小も等号も成り立たない。=\= は =:= の否定なので、ここが失敗すれば成立する。
    if (std::isnan(a) || std::isnan(b)) {
      holds = false;
      return true;
    }
    if (a < b) order = -1;
    else if (a > b) order = 1;
  } else if (left.big || right.big) {
    order = compare_int(machine, left, right);
  } else if (left.ival < right.ival) order = -1;
  else if (left.ival > right.ival) order = 1;
  holds = order_holds(order, mode);
  return true;
}

bool run_cmp(Impl& machine, const Goal& goal, int mode) {
  const BigScope scope(machine);
  const Ev left = eval_expr(machine, goal, goal.arg(machine, 0));
  if (!left.ok) return true;
  const Ev right = eval_expr(machine, goal, goal.arg(machine, 1));
  if (!right.ok) return true;
  bool holds = false;
  if (!cmp_nums(machine, goal, left.num, right.num, mode, holds)) return true;
  if (holds) machine.ip = goal.cont;
  else machine.failed = true;
  return true;
}

}  // namespace
}  // namespace arith

bool run_arith(Impl& machine, const Goal& goal) {
  const auto name = machine.atoms.name(goal.name);
  if (name == "is" && goal.arity == 2) {
    const arith::BigScope scope(machine);
    const arith::Ev value = arith::eval_expr(machine, goal, goal.arg(machine, 1));
    if (!value.ok) return true;
    if (!do_unify(machine, goal.arg(machine, 0), arith::num_cell(machine, value.num))) machine.failed = true;
    else machine.ip = goal.cont;
    return true;
  }
  if (goal.arity != 2) return false;
  if (name == "=:=") return arith::run_cmp(machine, goal, 0);
  if (name == "=\\=") {
    if (!arith::run_cmp(machine, goal, 0) || machine.exception || machine.eval_abort) return true;
    if (machine.failed) {
      machine.failed = false;
      machine.ip = goal.cont;
    } else {
      machine.failed = true;
    }
    return true;
  }
  if (name == "<") return arith::run_cmp(machine, goal, -2);
  if (name == "=<") return arith::run_cmp(machine, goal, -1);
  if (name == ">") return arith::run_cmp(machine, goal, 2);
  if (name == ">=") return arith::run_cmp(machine, goal, 1);
  return false;
}

std::optional<std::int64_t> eval_int_expression(Impl& machine, const Goal& goal, Cell cell) {
  const arith::BigScope scope(machine);
  const arith::Ev value = arith::eval_expr(machine, goal, cell);
  if (!value.ok) return std::nullopt;
  if (value.num.flt) {
    raise_formal(machine, formal_type(machine, "integer_expression", cell), goal.name, goal.arity);
    return std::nullopt;
  }
  // tab/1,2 は int64 で数える。Big の数はタスク 8 まで、前と同じく int_overflow にする（ival は添字なので読まない）。
  if (value.num.big) {
    arith::fail_eval(machine, goal, "int_overflow");
    return std::nullopt;
  }
  return value.num.ival;
}

namespace rt {

namespace {

// 生成コードから直に呼ぶときの誤りの文脈（context(is/2, _) など）。run_arith は枠から名前を読むが、ここには枠が無いので、
// 生成コードが単位の表から渡す述語のアトムを置く。
Goal arith_goal(std::uint32_t name) {
  Goal goal;
  goal.name = name;
  goal.arity = 2;
  return goal;
}

}  // namespace

bool arith_is(Machine::Impl& machine, std::uint32_t name, Cell left, Cell right) {
  const arith::BigScope scope(machine);
  const arith::Ev value = arith::eval_expr(machine, arith_goal(name), right);
  if (!value.ok) return false;
  if (do_unify(machine, left, arith::num_cell(machine, value.num))) return true;
  machine.failed = true;
  return false;
}

bool arith_compare(Machine::Impl& machine, Compare op, std::uint32_t name, Cell left, Cell right) {
  // Compare の順（Eq、Ne、Lt、Le、Gt、Ge）に run_cmp の mode を並べる。
  static constexpr std::array<int, 6> kModes{0, 0, -2, -1, 2, 1};
  const Goal goal = arith_goal(name);
  const arith::BigScope scope(machine);
  const arith::Ev lhs = arith::eval_expr(machine, goal, left);
  if (!lhs.ok) return false;
  const arith::Ev rhs = arith::eval_expr(machine, goal, right);
  if (!rhs.ok) return false;
  bool compared = false;
  if (!arith::cmp_nums(machine, goal, lhs.num, rhs.num, kModes[static_cast<std::size_t>(op)], compared)) return false;
  // =\= は =:= の否定（run_arith）。NaN はどちらとも比べられないので =\= だけが成り立つ（cmp_nums）。
  const bool holds = compared != (op == Compare::Ne);
  if (!holds) machine.failed = true;
  return holds;
}

}  // namespace rt

}
