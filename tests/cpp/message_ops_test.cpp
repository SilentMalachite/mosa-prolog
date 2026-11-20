// レビュー L19。利用者の op/3 は文面を変えない。カタログと library(format) は ISO の表で読み、目録のゴールは項から
// 利用者の表で書いて同じ表で読み直す。前は表を変えると、文面が鍵の形（goal_raised: …）に落ちた。
#include "harness.hpp"
#include "mosa/repl.hpp"

#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct Shown {
  std::string_view before;
  std::string_view goal;
  std::string_view text;
};

void check_shown(const Shown& item) {
  const std::string arg0 = "mosa";
  std::vector<std::string_view> args{arg0};
  const std::string input = "set_prolog_flag(message_language, en).\n" + std::string{item.before} + "\n" +
                            std::string{item.goal} + "\n";
  auto result = mosa::run_session(args, input, true);
  mosa_test::check(result.err.find(item.text) != std::string::npos,
                   std::string{item.before} + " then " + std::string{item.goal} + " -> " + result.err);
}

// 日本語の目録と全角記号モードでも同じ（L19 の敵対的レビュー A4）。
void check_japanese() {
  const std::string arg0 = "mosa";
  std::vector<std::string_view> args{arg0};
  auto result = mosa::run_session(args,
                                  "set_prolog_flag(message_language, ja).\nset_prolog_flag(japanese_punctuation, true).\n"
                                  "op(1100, xfx, =).\natom_length(X, Y).\n",
                                  true);
  mosa_test::check(result.err.find("警告: ゴールは例外を送出しました: error(instantiation_error") != std::string::npos,
                   "ja catalog under op(1100, xfx, =) -> " + result.err);
}

// カタログを読んだ後も、利用者の表は戻っている（L19 の敵対的レビュー A4）。
void check_table_restored() {
  const std::string arg0 = "mosa";
  std::vector<std::string_view> args{arg0};
  auto result =
      mosa::run_session(args, "op(700, xfx, ===).\natom_length(X, Y).\nX = (a === b), write(X), nl.\n", true);
  mosa_test::check(result.out.find("a===b") != std::string::npos, "user op kept after a message -> " + result.out);
}

// ctest の message_ops_gc が、読み込みの指示で GC を起こすカタログを MOSA_LIB に置いて呼ぶ。前は引数をカタログを
// 読んだ後で書いたので、GC が動かした例外の項の古い番地を読んだ（L19 の Codex レビュー）。そのカタログの文面は
// lib/messages_en.pl と違うので、カタログが読まれず代わりの表示に落ちたときも落ちる（レビュー H-35）。
void check_gc_during_load() {
  check_shown({"true.", "throw(f(g(1), h(2), [a, b, c])).",
               "Warning (messages_gc catalog): goal raised exception: f(g(1),h(2),[a,b,c])"});
}

// 文面を作る機械へ写した引数も、利用者の機械で書いたときと同じ字面になる（ADR 0007 の改訂 3、Muse のレビュー）。
// 共有する部分、浮動小数点数、大きな整数、符号、引用、深い項、部分リスト。字面は写す前の版で書いたものと同じ。
// 巡回の行は、写すときに巡回で止まらなくならないことを見る。目録のゴールは字面にして読み直すので、巡回は _ と書かれて
// 新しい変数になり、字面は巡回の無い f(A, B, B) と同じになる。巡回を新しい変数に替えて写す形とは字面で区別できない。
void check_carried() {
  const std::array<Shown, 8> cases{{
      {"true.", "Y = h(Z), throw(k(Y, Y, Z, \"ab\", [1,2|T])).",
       "Warning: goal raised exception: k(h(_G1),h(_G1),_G1,[97,98],[1,2|_G2])\n"},
      {"true.", "X = f(X, Y, Y), throw(X).", "Warning: goal raised exception: f(_G1,_G2,_G2)\n"},
      {"true.", "throw(g(1.5, -0.0, 1.0e300)).", "Warning: goal raised exception: g(1.5,-0.0,1.0e+300)\n"},
      {"true.", "throw(123456789012345678901234567890).",
       "Warning: goal raised exception: 123456789012345678901234567890\n"},
      {"true.", "throw(-(1)).", "Warning: goal raised exception: - (1)\n"},
      {"true.", "throw('hello world').", "Warning: goal raised exception: 'hello world'\n"},
      {"true.", "throw(f(f(f(f(f(f(f(f(f(f(f(f(a))))))))))))).",
       "Warning: goal raised exception: f(f(f(f(f(f(f(f(f(f(f(f(a))))))))))))\n"},
      {"true.", "length(L, 3), throw(L).", "Warning: goal raised exception: [_G1,_G2,_G3]\n"},
  }};
  for (const Shown& item : cases) check_shown(item);
}

// 文面を作る機械も利用者のヒープの上限を越えない（ADR 0007 の改訂 3、Muse のレビュー）。8 MiB では 60000 個の要素の
// リストの文面が作れず、利用者の機械で作っていた前と同じく鍵の形に落ちる。前の新しい機械は既定の上限を持ったので、
// --heap-max を越えてヒープを使い、文面も前と変わった。3 MiB では問い合わせそのものが止まるので、余裕を見て 8 にする。
void check_heap_limit_kept() {
  const std::string arg0 = "mosa";
  std::vector<std::string_view> args{arg0};
  auto result = mosa::run_session(args, "set_prolog_flag(message_language, en).\nlength(L, 60000), throw(L).\n", true,
                                  std::size_t{8} << 17);
  mosa_test::check(result.err.find("goal_raised: [_G1,_G2,") != std::string::npos &&
                       result.err.find("Warning: goal raised exception:") == std::string::npos,
                   "the renderer keeps the 8 MiB heap limit -> " + result.err.substr(0, 120));
}

}  // namespace

int main(int argc, char** argv) {
  if (argc > 1 && std::string_view{argv[1]} == "gc") {
    check_gc_during_load();
    return mosa_test::failures();
  }
  const std::array<Shown, 10> cases{{
      // カタログを読む前に表を変える。前は library(format) が読めず、鍵の形に落ちた。
      {"op(1100, xfx, =).", "X = .", "Syntax error: line 1 column 5: unexpected token at end of input"},
      {"op(1100, xfx, =).", "atom_length(X, Y).", "Warning: goal raised exception: error(instantiation_error"},
      {"op(700, xfx, ->).", "atom_length(X, Y).", "Warning: goal raised exception: error(instantiation_error"},
      // カタログを読んだ後で : を消す。前は目録のゴールの messages_en:message(…) が読めなかった。
      {"help.\nop(0, xfy, :).", "atom_length(X, Y).", "Warning: goal raised exception: error(instantiation_error"},
      {"help.\nop(1100, xfx, =).", "X = .", "Syntax error: line 1 column 5: unexpected token at end of input"},
      // 引数は利用者の表で書いて読み直すので、~q も利用者の表で書く。
      {"op(700, xfx, ===).", "throw(a === b).", "Warning: goal raised exception: a===b"},
      {"op(200, xfy, ::).", "throw(m::g).", "Warning: goal raised exception: m::g"},
      // ゴールが使う名前を演算子にしても、書き手が括弧を付けて同じ項に戻す。
      {"help.\nop(900, fy, fail).", "atom_length(X, Y).", "Warning: goal raised exception: error(instantiation_error"},
      {"help.\nop(700, xfx, format), op(700, fx, catch).", "atom_length(X, Y).",
       "Warning: goal raised exception: error(instantiation_error"},
      {"help.\nop(0, xfy, :), op(1100, xfx, :).", "atom_length(X, Y).",
       "Warning: goal raised exception: error(instantiation_error"},
  }};
  for (const Shown& item : cases) check_shown(item);
  check_japanese();
  check_table_restored();
  check_heap_limit_kept();
  check_carried();
  return mosa_test::failures();
}
