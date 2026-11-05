// レビュー L21 の Codex 敵対的レビュー。lib/format.pl と lib/strings.pl は背骨の形を length/2 に見させる。
// \+ \+ length(List, _) と書くと、\+ が実行時にゴールを訳し直してリストを IR に写し、共有の多い要素を木に広げた
// （深さ 18 の共有項はヒープでは f/2 が 18 個だが、写すと 2^18 個）。8 MiB の上限の下で、そういう要素を持つリストも
// list_tail/2 で辿っていた前と同じ誤りか答えになること。ctest の lib_shared が MOSA_LIB に lib/ を置く。
#include "harness.hpp"
#include "mosa/path_text.hpp"
#include "mosa/repl.hpp"

#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

// 語で数える。8 MiB。
constexpr std::size_t k_eight_mib = std::size_t{1} << 20;

// 誤りの項は共有項を含むので、書けば木に広がる。答えには誤りの形だけを束縛して返す。
constexpr std::string_view k_program =
    ":- use_module(library(format)).\n"
    ":- use_module(library(strings)).\n"
    "dag(0, a) :- !.\n"
    "dag(N, f(T, T)) :- N > 0, M is N - 1, dag(M, T).\n"
    "outcome(F, R) :- ( var(F) -> R = none ; F = type_error(T, _) -> R = type_error(T) ; functor(F, N, A), R = N/A ).\n"
    "fmt_args(R) :- dag(18, X), catch(format(\"~w\", [X|bad]), error(F, _), true), outcome(F, R).\n"
    "fmt_partial(R) :- dag(18, X), catch(format(\"~w\", [X|_]), error(F, _), true), outcome(F, R).\n"
    "fmt_s(R) :- dag(18, X), catch(format(\"~s\", [[X|bad]]), error(F, _), true), outcome(F, R).\n"
    "alc2(R) :- dag(18, X), catch(atomic_list_concat([X|bad], _), error(F, _), true), outcome(F, R).\n"
    "alc2_partial(R) :- dag(18, X), catch(atomic_list_concat([X|_], _), error(F, _), true), outcome(F, R).\n"
    "alc3(R) :- dag(18, X), catch(atomic_list_concat([X|bad], -, _), error(F, _), true), outcome(F, R).\n"
    "alc3_split(R) :- dag(18, X), catch(atomic_list_concat([X|bad], -, 'a-b'), error(F, _), true), outcome(F, R).\n"
    // 部分リストは分ける側へ進み、共有項の要素に原子を結ぶ atom_chars/2 が型の誤りを出す。
    "alc3_split_partial(R) :- dag(18, X), catch(atomic_list_concat([X|_], -, 'a-b'), error(F, _), true),\n"
    "    outcome(F, R).\n";

struct Case {
  std::string_view goal;
  std::string_view out;
};

void check_case(const std::string& path, const Case& item) {
  const std::string arg0 = "mosa";
  const std::string arg_g = "-g";
  const std::string goal = std::string{item.goal} + "(R).";
  const std::string arg_t = "-t";
  const std::string halt = "halt";
  const std::vector<std::string_view> args{arg0, path, arg_g, goal, arg_t, halt};
  const auto result = mosa::run_session(args, "", false, k_eight_mib);
  mosa_test::check(result.code == 0 && result.out.find(item.out) != std::string::npos,
                   goal + " -> " + result.out + result.err);
}

}  // namespace

int main() {
  // パスは session_test と同じく文字列で繋ぐ。Windows で path の string() は利用者のコードページに変わる。
  const std::string dir = std::string(MOSA_SOURCE_DIR) + "/tests/tmp/lib_shared";
  std::filesystem::create_directories(mosa::path_from_utf8(dir));
  const std::string path = dir + "/shared.pl";
  std::ofstream file(mosa::path_from_utf8(path), std::ios::binary);
  file << k_program;
  file.close();
  const std::array<Case, 8> cases{{
      {"fmt_args", "R = type_error(list)."},
      {"fmt_partial", "R = instantiation_error/0."},
      {"fmt_s", "R = type_error(list)."},
      {"alc2", "R = type_error(list)."},
      {"alc2_partial", "R = instantiation_error/0."},
      {"alc3", "R = type_error(list)."},
      {"alc3_split", "R = type_error(list)."},
      {"alc3_split_partial", "R = type_error(atom)."},
  }};
  for (const Case& item : cases) check_case(path, item);
  return mosa_test::failures();
}
