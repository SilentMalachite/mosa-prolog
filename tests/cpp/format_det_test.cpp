// 0.1.1。format/1,2,3 は成功して選択点を残さない。lib/format.pl の expand/5 と decimal_chars/3 が選択点を残し、
// 対話では `true` の後に終止符が付かず次の答えを待っていた（0.1.0 の Known limits）。
// tests/pl の solutions(...) は解の一覧だけを比べ、失敗する選択点が残っても見分けられないので、対話が促しを出すかを
// 決める Stop::more を見る。失敗や例外でも促しは出ないので、成功したことも合わせて見る（敵対的レビュー A1）。
// ctest の format_det が MOSA_LIB に lib/ を置く。
#include "harness.hpp"
#include "mosa/machine.hpp"

#include <array>
#include <string>
#include <string_view>

namespace {

// 指示ごとに通る道が違う（~Nd は decimal_chars/3 で、0 で埋める道と負の数がある。~t と ~| は column_stop/4 で、
// 埋め所が 1 つと 2 つ）。どれも最後は expand/5 を通る。text の項目が無い書式は 0.1.0 でも選択点を残さないので、
// 埋め所の書式にも ~w を入れる（敵対的レビュー A2）。
constexpr auto k_goals = std::to_array<std::string_view>({
    "format(\"abc\")",
    "format(\"\")",
    "format(\"~w\", [a])",
    "format(\"~q\", ['A'])",
    "format(\"~a\", [abc])",
    "format(\"~d\", [12])",
    "format(\"~2d\", [314])",
    "format(\"~3d\", [5])",
    "format(\"~2d\", [-314])",
    "format(\"~f\", [1.5])",
    "format(\"~e\", [1.5])",
    "format(\"~s\", [\"ab\"])",
    "format(\"~3s\", [\"ab\"])",
    "format(\"~c\", [0'x])",
    "format(\"~n\")",
    "format(\"~a~t~10|~w\", [ab, c])",
    "format(\"~`-t~w~10|\", [a])",
    "format(\"~t~w~t~11|\", [ab])",
    "format(\"~~\")",
    "format(\"~*c\", [3, 0'x])",
    "format(user_output, \"~w\", [a])",
    // フェーズ11のタスク6。~N は何も組んでいなければ '$stream_column'/2 を catch/3 の中で呼ぶ。
    "format(\"~a~i~a\", [a, b, c])",
    "format(\"~k\", ['A'])",
    "format(\"~W\", [a, [quoted(true)]])",
    "format(\"~p\", [a])",
    "format(\"~N~w~N~N\", [a])",
    "format(\"~t~w~N~t~w~6|\", [a, b])",
    "format(user_output, \"~N~w~n\", [a])",
    // フェーズ11のタスク7。~D は整数部を 3 桁ごとに区切る道を通る。区切りの無い短い数と多倍長も見る。
    "format(\"~16r\", [127])",
    "format(\"~R\", [-16])",
    "format(\"~16r\", [2^100])",
    "format(\"~D\", [123456789])",
    "format(\"~D\", [-1234567])",
    "format(\"~2D\", [123456789])",
    "format(\"~2D\", [5])",
    "format(\"~D\", [12])",
    "format(\"~D\", [2^70])",
    // フェーズ11のタスク8。~E、~g、~G は ~e と同じく float_directive/7 を通る。
    "format(\"~E\", [1.5])",
    "format(\"~g\", [0.00000123])",
    "format(\"~2G\", [392.65])",
    // フェーズ11のタスク9。~+ は ~| と同じ column_stop を通る。埋め所が 1 つと 2 つ。
    "format(\"~a~t~8+~w\", [ab, c])",
    "format(\"~t~w~t~+\", [ab])",
});

void check_goal(mosa::Machine& machine, std::string_view goal) {
  const auto answer = machine.query(std::string{goal} + ".");
  mosa_test::check(answer.kind == mosa::StopKind::Success,
                   std::string{goal} + ": does not succeed");
  mosa_test::check(!answer.more, std::string{goal} + ": leaves a choice point");
}

}  // namespace

int main() {
  auto machine = mosa::Machine::create();
  const auto loaded = machine->load_program(":- use_module(library(format)).\n");
  mosa_test::check(loaded.ok, "library(format) loads: " + loaded.message);
  for (const std::string_view goal : k_goals) check_goal(*machine, goal);
  return mosa_test::failures();
}
