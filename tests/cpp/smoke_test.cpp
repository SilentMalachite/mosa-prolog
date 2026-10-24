#include "harness.hpp"

int main() {
  mosa_test::check(1 + 1 == 2, "1 + 1 == 2");
  return mosa_test::failures();
}
