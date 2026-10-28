// is/2 が引数より先に引く評価関数の表（src/builtins/evaluable.hpp）と、実際に計算する側が揃っているかを見る。
// 表にだけ残った名前は、引数の誤りが先に出て iso-notes 7.9.1 の決定 B が崩れるので、ここで落とす。
#include "builtins/evaluable.hpp"
#include "harness.hpp"
#include "mosa/machine.hpp"

#include <span>
#include <string>
#include <string_view>

namespace {

// 名前を引用符で囲まずに済むよう、文字コードの並びから原子を作る。
std::string codes_of(std::string_view name) {
  std::string out = "[";
  for (std::size_t i = 0; i < name.size(); ++i) {
    if (i != 0) out += ',';
    out += std::to_string(static_cast<unsigned char>(name[i]));
  }
  return out + "]";
}

// 引数 1 で評価して、type_error(evaluable, _) 以外の結果（成功、失敗、他の誤り）なら計算する側にある。
bool computes(mosa::Machine& machine, std::string_view name, std::string_view args) {
  const std::string query = "atom_codes(N, " + codes_of(name) + "), T =.. [N|" + std::string(args) +
                            "], catch((_ is T -> true ; true), E, E \\= error(type_error(evaluable, _), _)).";
  return machine.query(query).kind == mosa::StopKind::Success;
}

void check_table(std::string_view label, std::span<const std::string_view> names, std::string_view args) {
  auto machine = mosa::Machine::create();
  for (const std::string_view name : names) {
    mosa_test::check(computes(*machine, name, args), std::string(label) + " " + std::string(name));
  }
}

}  // namespace

int main() {
  check_table("unary", mosa::kUnaryEvaluable, "[1]");
  check_table("binary", mosa::kBinaryEvaluable, "[1, 1]");
  return mosa_test::failures() == 0 ? 0 : 1;
}
