#include "harness.hpp"
#include "mosa/atom.hpp"
#include "mosa/cell.hpp"
#include "mosa/heap.hpp"

#include <array>
#include <bit>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>

void check_functor_and_atoms() {
  mosa_test::check(static_cast<unsigned>(mosa::Tag::Ref) == 0 && static_cast<unsigned>(mosa::Tag::Atom) == 1 &&
                       static_cast<unsigned>(mosa::Tag::Int) == 2 && static_cast<unsigned>(mosa::Tag::Str) == 3 &&
                       static_cast<unsigned>(mosa::Tag::Lis) == 4 && static_cast<unsigned>(mosa::Tag::Float) == 5 &&
                       static_cast<unsigned>(mosa::Tag::Big) == 6 && static_cast<unsigned>(mosa::Tag::IntBox) == 7,
                   "tags 0 through 7");
  mosa_test::check(mosa::pack_functor(1, 2) == ((std::uint64_t{1} << 14) | (std::uint64_t{2} << 3)),
                   "functor word");
  mosa_test::check(mosa::functor_arity(mosa::pack_functor(3, 1024)) == 1024, "arity 1024");
  mosa_test::check((mosa::pack_functor(3, 1024) & 7U) == 0, "functor low bits clear");
  mosa::AtomTable atoms;
  mosa_test::check(atoms.intern("foo") == 1 && atoms.intern("foo") == 1, "intern stable");
  mosa_test::check(atoms.intern("foo") != 0 && !atoms.valid(0), "atom id 0 is unused");
  mosa_test::check(atoms.intern("") == 2 && atoms.intern("") != 0, "empty atom is not zero");
  mosa_test::check(atoms.ascii(atoms.intern("abc")), "ascii flag");
  const auto kanji = atoms.intern("親");
  mosa_test::check(!atoms.ascii(kanji), "non ascii");
  mosa_test::check(atoms.intern_char(U'\u6771') == atoms.intern("東"), "char atom");
  const auto composed = atoms.intern("\u304c");
  const auto decomposed = atoms.intern("\u304b\u3099");
  mosa_test::check(composed != decomposed, "no normalization");
  mosa_test::check(atoms.name(atoms.intern("foo")) == "foo", "name roundtrip");
}

bool second_ref_hits_cap(mosa::Heap& heap) {
  try {
    (void)heap.fresh_ref();
  } catch (const mosa::ResourceLimit& error) {
    return std::string_view{error.what()} == "heap_limit";
  }
  return false;
}

void check_heap_cap_and_ref() {
  mosa::AtomTable atoms;
  // REF は 1 語。上限 1 の次の REF が、メッセージを出して既存の語を残す。
  mosa::Heap heap(atoms, 1);
  const auto* base = heap.data();
  const auto first = heap.fresh_ref();
  mosa_test::check(base != nullptr && first.tag() == mosa::Tag::Ref &&
                       first.raw() == std::bit_cast<std::uint64_t>(base) && *base == first.raw() &&
                       heap.unbound(first),
                   "unbound ref holds its own address");
  mosa_test::check(second_ref_hits_cap(heap) && heap.unbound(first) && *base == first.raw(),
                   "cap throws and keeps the first ref");
  mosa::Heap two(atoms, 2);
  const auto left = two.fresh_ref();
  const auto right = two.fresh_ref();
  mosa_test::check(two.unbound(left) && two.unbound(right) && second_ref_hits_cap(two),
                   "cap 2 holds two refs");
}

void check_numbers_and_terms() {
  mosa::AtomTable atoms;
  mosa::Heap wide(atoms);
  mosa_test::check(wide.limit() == mosa::kDefaultHeapMax, "default cap");
  const auto* base = wide.data();
  const auto reserved = wide.capacity();
  const auto small = wide.integer((std::int64_t{1} << 60) - 1);
  const auto edge = wide.integer(std::int64_t{1} << 60);
  const auto neg = wide.integer(-(std::int64_t{1} << 60));
  const auto boxed_neg = wide.integer(-(std::int64_t{1} << 60) - 1);
  mosa_test::check(small.tag() == mosa::Tag::Int && edge.tag() == mosa::Tag::IntBox, "positive edge");
  mosa_test::check(neg.tag() == mosa::Tag::Int && boxed_neg.tag() == mosa::Tag::IntBox, "negative edge");
  mosa_test::check(wide.integer_value(edge) == (std::int64_t{1} << 60), "box value");
  mosa_test::check(wide.integer_value(small) == (std::int64_t{1} << 60) - 1, "small value");
  mosa_test::check(wide.integer_value(neg) == -(std::int64_t{1} << 60), "neg value");
  mosa_test::check(wide.integer_value(boxed_neg) == -(std::int64_t{1} << 60) - 1, "boxed neg value");
  const auto term =
      wide.structure(atoms.intern("f"), std::array<mosa::Cell, 2>{wide.atom(atoms.intern("a")), small});
  mosa_test::check(term.tag() == mosa::Tag::Str && wide.functor_arity(term) == 2 &&
                       wide.functor_atom(term) == atoms.intern("f"),
                   "structure");
  const auto empty = wide.atom(atoms.intern("[]"));
  const auto cons = wide.list(wide.atom(atoms.intern("a")), empty);
  mosa_test::check(empty.tag() == mosa::Tag::Atom && empty.tag() != mosa::Tag::Lis, "empty list is atom");
  mosa_test::check(cons.tag() == mosa::Tag::Lis && wide.list_tail(cons).raw() == empty.raw(), "lis cell");
  const auto flt = wide.floating(1.25);
  mosa_test::check(flt.tag() == mosa::Tag::Float && wide.float_value(flt) == 1.25, "boxed float");
  mosa_test::check(small.tag() != mosa::Tag::Big && edge.tag() != mosa::Tag::Big &&
                       term.tag() != mosa::Tag::Big && cons.tag() != mosa::Tag::Big &&
                       flt.tag() != mosa::Tag::Big && static_cast<unsigned>(mosa::Tag::Big) == 6,
                   "big cells are not created");
  mosa_test::check(wide.data() == base && wide.capacity() == reserved, "heap does not reallocate");
  const auto payload = wide.word(edge);
  mosa_test::check(payload != nullptr && wide.word(flt) != nullptr &&
                       (edge.raw() & 7U) == static_cast<unsigned>(mosa::Tag::IntBox),
                   "box payload is a separate word");
}

int main() {
  check_functor_and_atoms();
  check_heap_cap_and_ref();
  check_numbers_and_terms();
  return mosa_test::failures();
}
