#include "harness.hpp"

namespace {

// main が返す値は POSIX では下位 8 ビットしか残らない（レビュー H-33）。失敗の数を
// そのまま返すと 256 件の倍数の失敗で終了コードが 0 になり、ctest が通過と数えるので、
// 256 件の失敗を数えたときに main へ返す値の下位 8 ビットが 0 でないことを確かめる。
void check_exit_status_survives_256_failures() {
  const int saved = mosa_test::failure_count();
  mosa_test::failure_count() = 256;
  const int status = mosa_test::failures();
  mosa_test::failure_count() = saved;
  mosa_test::check((status & 0xFF) != 0, "256 failures give a non-zero exit status");
}

}

int main() {
  mosa_test::check(1 + 1 == 2, "1 + 1 == 2");
  check_exit_status_survives_256_failures();
  return mosa_test::failures();
}
