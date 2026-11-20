# version_check.cmake の自己テスト（レビュー L-46）。版の 3 件が使う台本そのものを偽の版表示に向けて動かし、
# 落ちたときは理由の文まで見る。落ちるだけを見ると、どの理由で落ちても通ってしまう。
# 使い方: cmake -DCHECK=<version_check.cmake> -DVERSION=<project VERSION> -P version_check_test.cmake
# FAKE を付けて呼ぶと偽の版表示になる。「mosa SHOW」を 1 行出し、CODE が 1 なら 1 で終わる。

if(FAKE)
  execute_process(COMMAND "${CMAKE_COMMAND}" -E echo "mosa ${SHOW}")
  if(CODE)
    message(FATAL_ERROR "the fake exits with 1")
  endif()
  return()
endif()

if(NOT DEFINED VERSION OR VERSION STREQUAL "")
  message(FATAL_ERROR "VERSION is empty: pass -DVERSION=\${PROJECT_VERSION}")
endif()

# 偽物に「mosa show」を出させて code で終わらせ、検査に版 version を渡す。want が空なら検査は通るはず。
# 空でなければ、検査は落ちて、その文を出すはず。
function(expect_check label show code version want)
  execute_process(COMMAND "${CMAKE_COMMAND}"
    "-DCOMMAND=${CMAKE_COMMAND};-DFAKE=1;-DSHOW=${show};-DCODE=${code};-P;${CMAKE_CURRENT_FUNCTION_LIST_FILE}"
    "-DVERSION=${version}" -P "${CHECK}"
    OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE got TIMEOUT 60 ENCODING UTF-8)
  if(want STREQUAL "")
    if(NOT got EQUAL 0)
      message(FATAL_ERROR "${label}: the check must pass: code=${got} out=[${out}] err=[${err}]")
    endif()
    return()
  endif()
  # CMake は FATAL_ERROR の文を空白の所で折り返すので、空白の並びを 1 つにしてから探す。
  string(REGEX REPLACE "[ \t\r\n]+" " " flat "${err}")
  string(FIND "${flat}" "${want}" at)
  if(got EQUAL 0 OR at LESS 0)
    message(FATAL_ERROR "${label}: the check must fail with [${want}]: code=${got} out=[${out}] err=[${err}]")
  endif()
endfunction()

expect_check("right line, exit 0" "${VERSION}" 0 "${VERSION}" "")
# PASS_REGULAR_EXPRESSION の下では、この偽物は Passed になった。
expect_check("right line, exit 1" "${VERSION}" 1 "${VERSION}" "exit code 1, want 0:")
expect_check("other version" "${VERSION}" 0 "9.9.9" "want [mosa 9.9.9]:")
expect_check("empty version" "${VERSION}" 0 "" "VERSION is empty")
