C++ unit tests and `harness.hpp`. No external framework; each test is registered with CTest in `tests/tests.cmake` or in a file it includes: `tests/conformance_tests.cmake` for the conformance-suite runner and `tests/manual_tests.cmake` for the manual checker.

C++ の部品テストと `harness.hpp` を置く。外部フレームワークは使わず、各テストは `tests/tests.cmake` か、それが include するファイルで CTest に登録する。準拠スイートの試験器のテストは `tests/conformance_tests.cmake`、マニュアルの確かめのテストは `tests/manual_tests.cmake` にある。
