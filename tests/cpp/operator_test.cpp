#include "harness.hpp"
#include "mosa/operator_table.hpp"

#include <string_view>
#include <vector>

namespace {

template <class Has>
void check_high_priority(Has has) {
  mosa_test::check(has(1200, mosa::Specifier::Xfx, ":-"), ":- xfx");
  mosa_test::check(has(1200, mosa::Specifier::Fx, ":-"), ":- fx");
  mosa_test::check(has(1200, mosa::Specifier::Xfx, "-->"), "-->");
  mosa_test::check(has(1200, mosa::Specifier::Fx, "?-"), "?-");
  mosa_test::check(has(1100, mosa::Specifier::Xfy, ";"), ";");
  mosa_test::check(has(1050, mosa::Specifier::Xfy, "->"), "->");
  mosa_test::check(has(1000, mosa::Specifier::Xfy, ","), "comma");
  mosa_test::check(has(900, mosa::Specifier::Fy, "\\+"), "\\+");
}

template <class Has>
void check_comparison(Has has) {
  mosa_test::check(has(700, mosa::Specifier::Xfx, "="), "=");
  mosa_test::check(has(700, mosa::Specifier::Xfx, "\\="), "\\=");
  mosa_test::check(has(700, mosa::Specifier::Xfx, "=="), "==");
  mosa_test::check(has(700, mosa::Specifier::Xfx, "\\=="), "\\==");
  mosa_test::check(has(700, mosa::Specifier::Xfx, "@<"), "@<");
  mosa_test::check(has(700, mosa::Specifier::Xfx, "@=<"), "@=<");
  mosa_test::check(has(700, mosa::Specifier::Xfx, "@>"), "@>");
  mosa_test::check(has(700, mosa::Specifier::Xfx, "@>="), "@>=");
  mosa_test::check(has(700, mosa::Specifier::Xfx, "=.."), "=..");
  mosa_test::check(has(700, mosa::Specifier::Xfx, "is"), "is");
  mosa_test::check(has(700, mosa::Specifier::Xfx, "=:="), "=:=");
  mosa_test::check(has(700, mosa::Specifier::Xfx, "=\\="), "=\\=");
  mosa_test::check(has(700, mosa::Specifier::Xfx, "<"), "<");
  mosa_test::check(has(700, mosa::Specifier::Xfx, "=<"), "=<");
  mosa_test::check(has(700, mosa::Specifier::Xfx, ">"), ">");
  mosa_test::check(has(700, mosa::Specifier::Xfx, ">="), ">=");
}

template <class Has>
void check_arithmetic(Has has) {
  mosa_test::check(has(500, mosa::Specifier::Yfx, "+"), "+ infix");
  mosa_test::check(has(500, mosa::Specifier::Yfx, "-"), "- infix");
  mosa_test::check(has(500, mosa::Specifier::Yfx, "/\\"), "/\\");
  mosa_test::check(has(500, mosa::Specifier::Yfx, "\\/"), "\\/");
  mosa_test::check(has(400, mosa::Specifier::Yfx, "*"), "*");
  mosa_test::check(has(400, mosa::Specifier::Yfx, "/"), "/");
  mosa_test::check(has(400, mosa::Specifier::Yfx, "//"), "//");
  mosa_test::check(has(400, mosa::Specifier::Yfx, "rem"), "rem");
  mosa_test::check(has(400, mosa::Specifier::Yfx, "mod"), "mod");
  mosa_test::check(has(400, mosa::Specifier::Yfx, "<<"), "<<");
  mosa_test::check(has(400, mosa::Specifier::Yfx, ">>"), ">>");
  mosa_test::check(has(400, mosa::Specifier::Yfx, "div"), "div");
  mosa_test::check(has(200, mosa::Specifier::Xfx, "**"), "**");
  mosa_test::check(has(200, mosa::Specifier::Xfy, "^"), "^");
  mosa_test::check(has(200, mosa::Specifier::Fy, "+"), "+ prefix");
  mosa_test::check(has(200, mosa::Specifier::Fy, "-"), "- prefix");
  mosa_test::check(has(200, mosa::Specifier::Fy, "\\"), "\\");
}

void check_absent_and_order(const mosa::OperatorTable& ops) {
  mosa_test::check(ops.lookup(":").empty(), "no module colon");
  mosa_test::check(ops.lookup("|").empty(), "bar is not predefined");
  mosa_test::check(ops.lookup("*->").empty(), "no *->");
  mosa_test::check(ops.lookup("xor").empty(), "xor is not an operator");
  const auto defined = ops.lookup(":-");
  mosa_test::check(defined.size() == 2 && defined[0].name == ":-" &&
                       defined[0].specifier == mosa::Specifier::Xfx && defined[0].priority == 1200 &&
                       defined[1].name == ":-" && defined[1].specifier == mosa::Specifier::Fx &&
                       defined[1].priority == 1200,
                   ":- order xfx then fx");
  // Fy の宣言は Yfx より後だが、優先度 200 が 500 より小さいので前置が先に来る。
  const auto plus = ops.lookup("+");
  mosa_test::check(plus.size() == 2 && plus[0].name == "+" && plus[0].specifier == mosa::Specifier::Fy &&
                       plus[0].priority == 200 && plus[1].name == "+" &&
                       plus[1].specifier == mosa::Specifier::Yfx && plus[1].priority == 500,
                   "+ order prefix then infix");
}

void check_core() {
  auto ops = mosa::OperatorTable::iso_core();
  auto has = [&](int priority, mosa::Specifier spec, std::string_view name) {
    for (const auto& op : ops.lookup(name)) {
      if (op.priority == priority && op.specifier == spec) return true;
    }
    return false;
  };
  check_high_priority(has);
  check_comparison(has);
  check_arithmetic(has);
  check_absent_and_order(ops);
  mosa_test::check(ops.define(0, mosa::Specifier::Fy, "+") == mosa::OpResult::Ok, "remove prefix +");
  mosa_test::check(!has(200, mosa::Specifier::Fy, "+") && has(500, mosa::Specifier::Yfx, "+"),
                   "infix + remains");
}

bool plus_intact(const mosa::OperatorTable& ops) {
  const auto plus = ops.lookup("+");
  return plus.size() == 2 && plus[0].priority == 200 && plus[0].specifier == mosa::Specifier::Fy &&
         plus[1].priority == 500 && plus[1].specifier == mosa::Specifier::Yfx;
}

void check_bad_priority_and_comma(mosa::OperatorTable& ops) {
  const auto bogus = static_cast<mosa::Specifier>(99);
  mosa_test::check(ops.define(100, bogus, "q") == mosa::OpResult::BadSpecifier, "bad specifier");
  mosa_test::check(ops.lookup("q").empty() && plus_intact(ops), "bad specifier keeps table");
  mosa_test::check(ops.define(-1, mosa::Specifier::Xfx, "+") == mosa::OpResult::BadPriority,
                   "priority -1");
  mosa_test::check(ops.define(1201, mosa::Specifier::Xfx, "+") == mosa::OpResult::BadPriority,
                   "priority 1201");
  mosa_test::check(plus_intact(ops), "bad priority keeps table");
  mosa_test::check(ops.define(1000, mosa::Specifier::Xfy, ",") == mosa::OpResult::ModifyComma,
                   "modify comma");
  mosa_test::check(ops.define(0, mosa::Specifier::Xfy, ",") == mosa::OpResult::ModifyComma,
                   "remove comma");
  const auto comma = ops.lookup(",");
  mosa_test::check(comma.size() == 1 && comma[0].priority == 1000 &&
                       comma[0].specifier == mosa::Specifier::Xfy,
                   "comma remains");
}

void check_empty(mosa::OperatorTable& ops) {
  mosa_test::check(ops.define(500, mosa::Specifier::Xfx, "[]") == mosa::OpResult::CreateEmpty, "[]");
  mosa_test::check(ops.define(0, mosa::Specifier::Xfx, "[]") == mosa::OpResult::CreateEmpty, "remove []");
  mosa_test::check(ops.define(500, mosa::Specifier::Xfx, "{}") == mosa::OpResult::CreateEmpty, "{}");
  mosa_test::check(ops.define(0, mosa::Specifier::Fy, "{}") == mosa::OpResult::CreateEmpty, "remove {}");
  mosa_test::check(ops.lookup("[]").empty() && ops.lookup("{}").empty(), "empty not created");
}

void check_bar(mosa::OperatorTable& ops) {
  mosa_test::check(ops.define(1100, mosa::Specifier::Fx, "|") == mosa::OpResult::CreateBar, "| fx");
  mosa_test::check(ops.define(1100, mosa::Specifier::Fy, "|") == mosa::OpResult::CreateBar, "| fy");
  mosa_test::check(ops.define(1100, mosa::Specifier::Xf, "|") == mosa::OpResult::CreateBar, "| xf");
  mosa_test::check(ops.define(1100, mosa::Specifier::Yf, "|") == mosa::OpResult::CreateBar, "| yf");
  mosa_test::check(ops.define(1000, mosa::Specifier::Xfx, "|") == mosa::OpResult::CreateBar,
                   "| xfx 1000");
  mosa_test::check(ops.lookup("|").empty(), "| still absent");
  mosa_test::check(ops.define(1001, mosa::Specifier::Xfx, "|") == mosa::OpResult::Ok, "| xfx 1001");
  const auto bar = ops.lookup("|");
  mosa_test::check(bar.size() == 1 && bar[0].priority == 1001 && bar[0].specifier == mosa::Specifier::Xfx &&
                       bar[0].name == "|",
                   "| inserted");
  // 優先度 0 は中置の | を消す。Cor.2 の op(0, xfy, '|') と同じ。
  mosa_test::check(ops.define(0, mosa::Specifier::Xfx, "|") == mosa::OpResult::Ok, "| priority 0");
  mosa_test::check(ops.lookup("|").empty(), "| removed");
}

void check_mix_and_remove(mosa::OperatorTable& ops) {
  // 優先度 0 は無い組を消すだけ。中置が残っていても後置の削除は併存にしない。
  mosa_test::check(ops.define(0, mosa::Specifier::Xf, "+") == mosa::OpResult::Ok,
                   "remove absent postfix +");
  mosa_test::check(plus_intact(ops), "absent postfix removal keeps +");
  mosa_test::check(ops.define(100, mosa::Specifier::Xf, "+") == mosa::OpResult::InfixAndPostfix,
                   "postfix on infix +");
  mosa_test::check(plus_intact(ops), "rejected postfix keeps +");
  mosa_test::check(ops.define(100, mosa::Specifier::Yf, "pp") == mosa::OpResult::Ok, "add postfix");
  mosa_test::check(ops.define(100, mosa::Specifier::Xfx, "pp") == mosa::OpResult::InfixAndPostfix,
                   "infix on postfix");
  const auto postfix = ops.lookup("pp");
  mosa_test::check(postfix.size() == 1 && postfix[0].specifier == mosa::Specifier::Yf &&
                       postfix[0].priority == 100,
                   "postfix remains");
  mosa_test::check(ops.define(0, mosa::Specifier::Yfx, "+") == mosa::OpResult::Ok, "remove infix +");
  const auto plus = ops.lookup("+");
  mosa_test::check(plus.size() == 1 && plus[0].priority == 200 && plus[0].specifier == mosa::Specifier::Fy,
                   "prefix + remains");
}

void check_replace(mosa::OperatorTable& ops) {
  mosa_test::check(ops.define(1200, mosa::Specifier::Xfx, "edge") == mosa::OpResult::Ok, "priority 1200");
  mosa_test::check(ops.define(300, mosa::Specifier::Xfx, "edge") == mosa::OpResult::Ok, "replace edge");
  const auto edge = ops.lookup("edge");
  mosa_test::check(edge.size() == 1 && edge[0].priority == 300 && edge[0].specifier == mosa::Specifier::Xfx &&
                       edge[0].name == "edge",
                   "replaced priority");
  mosa_test::check(ops.define(0, mosa::Specifier::Xfx, "edge") == mosa::OpResult::Ok, "remove edge");
  mosa_test::check(ops.lookup("edge").empty(), "edge gone");
  mosa_test::check(ops.define(0, mosa::Specifier::Xf, "missing") == mosa::OpResult::Ok, "remove missing");
  mosa_test::check(ops.define(30, mosa::Specifier::Xfx, "zz") == mosa::OpResult::Ok, "add xfx");
  mosa_test::check(ops.define(30, mosa::Specifier::Xfy, "zz") == mosa::OpResult::Ok, "replace with xfy");
  const auto replaced = ops.lookup("zz");
  mosa_test::check(replaced.size() == 1 && replaced[0].specifier == mosa::Specifier::Xfy &&
                       replaced[0].priority == 30,
                   "one infix");
}

void check_rejections() {
  auto ops = mosa::OperatorTable::iso_core();
  check_bad_priority_and_comma(ops);
  check_empty(ops);
  check_bar(ops);
  check_mix_and_remove(ops);
  check_replace(ops);
}

}

int main() {
  check_core();
  check_rejections();
  return mosa_test::failures();
}
