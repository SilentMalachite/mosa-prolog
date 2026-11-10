// レビュー L23。initialization(G, main) のゴールは、読み込みの後で指令の項のまま動く。前は指令の時の演算子の表で
// 字面に書き、読み込みの後の表で読み直したので、後の op/3 でゴールが別の項になるか、読めずに黙って終了コード 1 で
// 終わった。各場合はファイルを tests/tmp/main_goal に書いて mosa の会話で読み、終了コードと警告を見る。
#include "harness.hpp"
#include "mosa/path_text.hpp"
#include "mosa/repl.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct Source {
  std::string_view name;
  std::string_view text;
};

struct Case {
  std::string_view label;
  std::vector<Source> files;
  int code = 0;
  // 空でなければ、警告にこの字面が出る。
  std::string_view warning;
};

void check_case(const std::string& dir, const Case& item) {
  const std::string arg0 = "mosa";
  std::vector<std::string> paths;
  for (const Source& source : item.files) {
    const std::string path = dir + "/" + std::string(source.name);
    std::ofstream file(mosa::path_from_utf8(path), std::ios::binary);
    file << source.text;
    paths.push_back(path);
  }
  std::vector<std::string_view> args{arg0};
  for (const std::string& path : paths) args.emplace_back(path);
  const auto result = mosa::run_session(args, "", false);
  const std::string seen = std::string(item.label) + " -> code " + std::to_string(result.code) + ", out [" +
                           result.out + "], err [" + result.err + "]";
  mosa_test::check(result.code == item.code, seen);
  if (item.warning.empty()) {
    mosa_test::check(result.err.empty(), seen);
  } else {
    mosa_test::check(result.err.find(item.warning) != std::string::npos, seen);
  }
}

}  // namespace

int main() {
  // パスは session_test と同じく文字列で繋ぐ。Windows で path の string() は利用者のコードページに変わる。
  const std::string dir = std::string(MOSA_SOURCE_DIR) + "/tests/tmp/main_goal";
  std::filesystem::create_directories(mosa::path_from_utf8(dir));
  const std::vector<Case> cases{
      // 後で = を演算子から外しても、ゴールは foo(a = b) のまま。前は読めずに何も出さず 1 で終わった。
      {"infix removed",
       {{"removed.pl", ":- initialization(check(a = b), main).\ncheck(X) :- X == (a = b).\n:- op(0, xfx, =).\n"}},
       0,
       {}},
      // 例外を投げるゴールも、黙って 1 で終わらずに例外を伝える。警告の項は今の表で書くので -(a,b)。
      {"throw after op",
       {{"thrown.pl", ":- initialization(throw(a-b), main).\n:- op(0, yfx, -).\n"}},
       2,
       "-(a,b)"},
      // 前置演算子を外すと、前は - (1) の字面が読めなかった。
      {"prefix removed",
       {{"prefix.pl", ":- initialization(check(-(1)), main).\ncheck(X) :- X == -(1).\n:- op(0, fy, -).\n"}},
       0,
       {}},
      // 結合の向きを変えると、前は 1-2-3 が 1-(2-3) と読まれ、別のゴールが失敗した。
      {"associativity changed",
       {{"assoc.pl",
         ":- initialization(check(1 - 2 - 3), main).\ncheck(X) :- X == (1 - 2) - 3.\n:- op(500, xfy, -).\n"}},
       0,
       {}},
      // ゴールの中の変数の共有も保つ。
      {"shared variables",
       {{"shared.pl",
         ":- initialization((X = f(Y), check(X, Y)), main).\ncheck(f(A), B) :- A == B.\n:- op(0, xfx, =).\n"}},
       0,
       {}},
      // 演算子を変えるのが後のファイルでも同じ。mosa a.pl b.pl。
      {"later file",
       {{"first.pl", ":- initialization(check(a = b), main).\ncheck(X) :- X == (a = b).\n"},
        {"second.pl", ":- op(0, xfx, =).\n"}},
       0,
       {}},
      // モジュールのファイルのゴールは、そのモジュールの文脈で動く。
      {"module goal",
       {{"in_module.pl",
         ":- module(in_module, []).\n:- initialization(go(a = b), main).\ngo(X) :- X == (a = b).\n"
         ":- op(0, xfx, =).\n"}},
       0,
       {}},
  };
  for (const Case& item : cases) check_case(dir, item);
  // Muse のレビューの中。構文エラーで取り消した読み込みの initialization(G, main) は残さない。
  // ゴールが consult の失敗から立ち直って外の読み込みが成功しても、取り消したファイルの G は動かない。
  // 引数に渡すのは外のファイルだけ。取り消すファイルは consult で読む。
  std::ofstream(mosa::path_from_utf8(dir + "/rejected_main.pl"), std::ios::binary)
      << ":- initialization(bad_main, main).\nfoo( .\n";
  const std::string outer = "bad_main :- halt(7).\n:- (consult('" + dir + "/rejected_main.pl') -> true ; true).\n";
  check_case(dir, Case{"rejected load drops main", {{"outer_main.pl", outer}}, 0, {}});
  return mosa_test::failures();
}
