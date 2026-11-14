C++ unit tests and `harness.hpp`. No external framework; each test is registered with CTest in `tests/tests.cmake` or in a file it includes: `tests/conformance_tests.cmake` (mostly the tests of the conformance-suite runner and its report) and `tests/manual_tests.cmake` (the tests of the manual checker).

C++ の部品テストと `harness.hpp` を置く。外部フレームワークは使わず、各テストは `tests/tests.cmake` か、それが include するファイルで CTest に登録する。`tests/conformance_tests.cmake` には主に準拠スイートの試験器とその報告書のテストが、`tests/manual_tests.cmake` にはマニュアルの確かめのテストがある。
