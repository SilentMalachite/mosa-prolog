#include "harness.hpp"
#include "mosa/reader.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace {

struct Fix {
  mosa::AtomTable atoms;
  mosa::Heap heap;
  mosa::OperatorTable ops;

  Fix() : heap(atoms), ops(mosa::OperatorTable::iso_core()) {}

  [[nodiscard]] mosa::ReadOutcome read(std::string_view source,
                                       mosa::DoubleQuotes quotes = mosa::DoubleQuotes::Codes) {
    mosa::Reader reader(source, heap, ops, quotes);
    return reader.next();
  }
};

[[nodiscard]] bool named(const Fix& fix, mosa::Cell cell, std::string_view name) {
  cell = fix.heap.deref(cell);
  if (cell.tag() != mosa::Tag::Atom) return false;
  const auto id = static_cast<std::uint32_t>(cell.raw() >> 3);
  return fix.heap.atoms().name(id) == name;
}

[[nodiscard]] bool functor(const Fix& fix, mosa::Cell cell, std::string_view name, int arity) {
  cell = fix.heap.deref(cell);
  return cell.tag() == mosa::Tag::Str && fix.heap.functor_arity(cell) == static_cast<unsigned>(arity) &&
         fix.heap.atoms().name(fix.heap.functor_atom(cell)) == name;
}

[[nodiscard]] mosa::Cell arg(const Fix& fix, mosa::Cell cell, std::size_t index) {
  return fix.heap.deref(fix.heap.arg(fix.heap.deref(cell), index));
}

void check_term(std::string_view source, bool (*ok)(Fix&, const mosa::ReadTerm&), const char* label) {
  Fix fix;
  const auto outcome = fix.read(source);
  mosa_test::check(outcome.term && !outcome.failure && !outcome.eof && ok(fix, *outcome.term), label);
}

bool foo_ok(Fix& fix, const mosa::ReadTerm& term) { return named(fix, term.term, "foo"); }

bool minus_ok(Fix& fix, const mosa::ReadTerm& term) {
  const auto body = fix.heap.deref(term.term);
  const auto value = arg(fix, body, 0);
  return body.tag() != mosa::Tag::Int && functor(fix, body, "-", 1) && value.tag() == mosa::Tag::Int &&
         fix.heap.integer_value(value) == 42;
}

bool times_under_plus(Fix& fix, const mosa::ReadTerm& term) {
  return functor(fix, term.term, "+", 2) && named(fix, arg(fix, term.term, 0), "a") &&
         functor(fix, arg(fix, term.term, 1), "*", 2);
}

bool plus_under_times(Fix& fix, const mosa::ReadTerm& term) {
  return functor(fix, term.term, "*", 2) && functor(fix, arg(fix, term.term, 0), "+", 2) &&
         named(fix, arg(fix, term.term, 1), "c");
}

bool left_minus(Fix& fix, const mosa::ReadTerm& term) {
  return functor(fix, term.term, "-", 2) && functor(fix, arg(fix, term.term, 0), "-", 2);
}

bool right_caret(Fix& fix, const mosa::ReadTerm& term) {
  return functor(fix, term.term, "^", 2) && functor(fix, arg(fix, term.term, 1), "^", 2);
}

bool two_args(Fix& fix, const mosa::ReadTerm& term) { return functor(fix, term.term, "f", 2); }

bool comma_arg(Fix& fix, const mosa::ReadTerm& term) {
  return functor(fix, term.term, "f", 1) && functor(fix, arg(fix, term.term, 0), ",", 2);
}

bool empty_list(Fix& fix, const mosa::ReadTerm& term) {
  const auto cell = fix.heap.deref(term.term);
  return named(fix, cell, "[]") && cell.tag() != mosa::Tag::Lis;
}

bool proper_list(Fix& fix, const mosa::ReadTerm& term) {
  const auto cell = fix.heap.deref(term.term);
  if (cell.tag() != mosa::Tag::Lis) return false;
  const auto tail = fix.heap.deref(fix.heap.list_tail(cell));
  return named(fix, fix.heap.list_head(cell), "a") && tail.tag() == mosa::Tag::Lis &&
         named(fix, fix.heap.list_head(tail), "b") && named(fix, fix.heap.list_tail(tail), "[]");
}

bool list_bar(Fix& fix, const mosa::ReadTerm& term) {
  const auto cell = fix.heap.deref(term.term);
  if (cell.tag() != mosa::Tag::Lis || term.names.size() != 2) return false;
  const auto tail = fix.heap.list_tail(cell);
  return term.names[0].name == "H" && term.names[1].name == "T" && term.names[1].cell.raw() == tail.raw();
}

bool curly_one(Fix& fix, const mosa::ReadTerm& term) {
  return functor(fix, term.term, "{}", 1) && named(fix, arg(fix, term.term, 0), "a");
}

bool curly_atom(Fix& fix, const mosa::ReadTerm& term) { return named(fix, term.term, "{}"); }

bool curly_comma(Fix& fix, const mosa::ReadTerm& term) {
  return functor(fix, term.term, "{}", 1) && functor(fix, arg(fix, term.term, 0), ",", 2);
}

bool shared_var(Fix& fix, const mosa::ReadTerm& term) {
  const auto left = fix.heap.arg(fix.heap.deref(term.term), 0);
  const auto right = fix.heap.arg(fix.heap.deref(term.term), 1);
  return left.raw() == right.raw() && term.names.size() == 1 && term.names[0].name == "X";
}

bool anon_var(Fix& fix, const mosa::ReadTerm& term) {
  const auto left = fix.heap.arg(fix.heap.deref(term.term), 0);
  const auto right = fix.heap.arg(fix.heap.deref(term.term), 1);
  return left.raw() != right.raw() && term.names.empty();
}

bool singleton_b(Fix&, const mosa::ReadTerm& term) {
  return term.singletons.size() == 1 && term.singletons[0].name == "B";
}

bool codes_ab(Fix& fix, const mosa::ReadTerm& term) {
  const auto cell = fix.heap.deref(term.term);
  if (cell.tag() != mosa::Tag::Lis) return false;
  const auto tail = fix.heap.deref(fix.heap.list_tail(cell));
  return fix.heap.integer_value(fix.heap.list_head(cell)) == 97 && tail.tag() == mosa::Tag::Lis &&
         fix.heap.integer_value(fix.heap.list_head(tail)) == 98 && named(fix, fix.heap.list_tail(tail), "[]");
}

void check_shapes() {
  check_term("foo.", foo_ok, "atom");
  check_term("-42.", minus_ok, "unary minus");
  check_term("a+b*c.", times_under_plus, "priority times");
  check_term("(a+b)*c.", plus_under_times, "priority plus");
  check_term("a-b-c.", left_minus, "left assoc");
  check_term("a^b^c.", right_caret, "right assoc");
  check_term("f(a, b).", two_args, "arity 2");
  check_term("f((a, b)).", comma_arg, "comma arg");
}

void check_lists_and_curlies() {
  check_term("[].", empty_list, "empty list");
  check_term("[a, b].", proper_list, "list");
  check_term("[H|T].", list_bar, "list bar");
  check_term("{a}.", curly_one, "curly one");
  check_term("{}.", curly_atom, "empty curly");
  check_term("{a, b}.", curly_comma, "curly comma");
  Fix fix;
  const auto bad = fix.read("[a|b|c].");
  mosa_test::check(bad.failure && bad.failure->reason == mosa::SyntaxReason::UnexpectedToken && !bad.term,
                   "second bar");
}

void check_variables() {
  check_term("f(X, X).", shared_var, "shared var");
  check_term("f(_, _).", anon_var, "anon vars");
  check_term("f(A, A, B).", singleton_b, "singleton");
}

void check_quotes_and_eof() {
  check_term("\"ab\".", codes_ab, "codes");
  Fix chars;
  const auto as_chars = chars.read("\"ab\".", mosa::DoubleQuotes::Chars);
  const auto cell = chars.heap.deref(as_chars.term->term);
  const auto tail = chars.heap.deref(chars.heap.list_tail(cell));
  mosa_test::check(as_chars.term && cell.tag() == mosa::Tag::Lis && named(chars, chars.heap.list_head(cell), "a") &&
                       named(chars, chars.heap.list_head(tail), "b"),
                   "chars");
  Fix atom;
  const auto as_atom = atom.read("\"ab\".", mosa::DoubleQuotes::Atom);
  mosa_test::check(as_atom.term && named(atom, as_atom.term->term, "ab"), "atom quotes");
  Fix empty_codes;
  mosa_test::check(named(empty_codes, empty_codes.read("\"\".").term->term, "[]"), "empty codes");
  Fix empty_atom;
  const auto blank = empty_atom.read("\"\".", mosa::DoubleQuotes::Atom);
  mosa_test::check(blank.term && named(empty_atom, blank.term->term, ""), "empty atom quotes");
  Fix eof;
  const auto ended = eof.read("");
  mosa_test::check(ended.eof && !ended.term && !ended.failure, "eof");
  check_term("end_of_file.", [](Fix& fix, const mosa::ReadTerm& term) {
    return named(fix, term.term, "end_of_file");
  }, "end_of_file atom");
}

void check_errors_and_operator() {
  bool threw = false;
  try {
    Fix fix;
    const auto bad = fix.read("（a）.");
    mosa_test::check(bad.failure && bad.failure->reason == mosa::SyntaxReason::UnexpectedToken && !bad.eof,
                     "fullwidth paren");
  } catch (...) {
    threw = true;
  }
  mosa_test::check(!threw, "fullwidth paren does not throw");
  std::string many = "f(";
  for (int index = 0; index < 1024; ++index) {
    if (index != 0) many += ", ";
    many += "a";
  }
  many += ").";
  Fix wide;
  const auto ok = wide.read(many);
  mosa_test::check(ok.term && !ok.failure && functor(wide, ok.term->term, "f", 1024), "arity 1024");
  many.insert(many.size() - 2, ", a");
  Fix over;
  const auto too_many = over.read(many);
  mosa_test::check(too_many.failure && too_many.failure->reason == mosa::SyntaxReason::MaxArity &&
                       too_many.failure->line >= 1 && too_many.failure->column >= 1,
                   "arity 1025");
  Fix op;
  mosa_test::check(op.ops.define(700, mosa::Specifier::Xfx, "は") == mosa::OpResult::Ok, "define は");
  const auto term = op.read("太郎 は 学生.");
  mosa_test::check(term.term && functor(op, term.term->term, "は", 2), "japanese op");
  check_term("/* c */ foo.", foo_ok, "comment");
}

}  // namespace

int main() {
  check_shapes();
  check_lists_and_curlies();
  check_variables();
  check_quotes_and_eof();
  check_errors_and_operator();
  return mosa_test::failures();
}
