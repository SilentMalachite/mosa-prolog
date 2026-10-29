# 計画 Task 11。mosatest --diff DIR をリポジトリの根で流し、終了コード 0（違い 0）と、REQUIRE に挙げたファイルが
# 比べた一覧に出ることを確かめる（AGENTS.md: japanese.pl は必ず差分テストに含める）。
# 使い方: cmake -DMOSATEST=<mosatest> -DDIR=<tests/pl> -DWORK=<一時ディレクトリ> -DCMAKE_PATH=<cmake>
#   -DCONFIG=<構成> -DREQUIRE=<japanese.pl;native.pl> -DROOT=<リポジトリの根> -P diff_test.cmake
# 翻訳側の configure は本体と同じ生成器、コンパイラ、旗で行う。CMake は環境変数 CMAKE_GENERATOR、CXX、CXXFLAGS、
# LDFLAGS を読むので、tests.cmake が ENVIRONMENT で渡す。生成コードは警告なしで通ること（MOSA_WERROR=1）。

foreach(name MOSATEST DIR WORK CMAKE_PATH CONFIG ROOT)
  if(NOT DEFINED ${name})
    message(FATAL_ERROR "diff_test: -D${name} is required")
  endif()
endforeach()

set(ENV{MOSA_WERROR} 1)
execute_process(COMMAND "${MOSATEST}" --diff "${DIR}" --work "${WORK}" --cmake "${CMAKE_PATH}" --config "${CONFIG}"
  WORKING_DIRECTORY "${ROOT}"
  OUTPUT_VARIABLE out
  ERROR_VARIABLE err
  RESULT_VARIABLE code
  TIMEOUT 3000)
message("${out}")
if(NOT code EQUAL 0)
  message(FATAL_ERROR "mosatest --diff ${DIR}: code=[${code}] err=[${err}]")
endif()

# 比べたファイルの行は「<DIR>/<名前>: 」で始まる。文字列で探し、パスの . を正規表現の任意の文字にしない。
foreach(name ${REQUIRE})
  string(FIND "${out}" "${DIR}/${name}: " at)
  if(at EQUAL -1)
    message(FATAL_ERROR "mosatest --diff ${DIR} did not compare ${name}: out=[${out}]")
  endif()
endforeach()
