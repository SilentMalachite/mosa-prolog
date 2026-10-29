# 計画 Task 11。mosatest --diff DIR をリポジトリの根で流し、終了コード 0（違い 0）と、REQUIRE に挙げたファイルが
# 比べた一覧に出ることを確かめる（AGENTS.md: japanese.pl は必ず差分テストに含める）。ATTACHED に挙げたファイルは
# 行の末尾の「付けた述語 A/T」の A が 0 でないことも確かめ、生成関数が 1 つも付かずに IR だけを比べた回を落とす。
# 使い方: cmake -DMOSATEST=<mosatest> -DDIR=<tests/pl> -DWORK=<一時ディレクトリ> -DCMAKE_PATH=<cmake>
#   -DCONFIG=<構成> -DREQUIRE=<japanese.pl;native.pl> -DATTACHED=<native.pl> -DROOT=<リポジトリの根> -P diff_test.cmake
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

# ファイルの行の末尾は、どの言語の文面でも「付けた数/出した数」で終わる（lib/messages_*.pl の diff_file）。
# その行だけを切り出してから比を読む。
foreach(name ${ATTACHED})
  string(FIND "${out}" "${DIR}/${name}: " at)
  if(at EQUAL -1)
    message(FATAL_ERROR "mosatest --diff ${DIR} did not compare ${name}: out=[${out}]")
  endif()
  string(SUBSTRING "${out}" ${at} -1 line)
  string(FIND "${line}" "\n" eol)
  if(NOT eol EQUAL -1)
    string(SUBSTRING "${line}" 0 ${eol} line)
  endif()
  string(REGEX MATCH "([0-9]+)/([0-9]+)[^0-9/]*$" ratio "${line}")
  if("${ratio}" STREQUAL "")
    message(FATAL_ERROR "mosatest --diff ${DIR}: no attached count on the line for ${name}: [${line}]")
  endif()
  if(NOT CMAKE_MATCH_1 GREATER 0)
    message(FATAL_ERROR "mosatest --diff ${DIR}: no native procedure attached for ${name}: [${line}]")
  endif()
endforeach()
