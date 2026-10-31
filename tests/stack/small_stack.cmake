# 1 MiB のスタックでコマンドを動かし、標準出力を期待のファイルと比べる（ADR 0011）。POSIX では ulimit -s 1024 の
# 下で動かす。Windows では主スレッドの既定のスタックが 1 MiB なので、そのまま動かす。
# SUMMARY が真なら、出力の各行を「長さ|先頭 12 バイト|末尾 12 バイト」に縮めてから比べる。深い項の全文を
# 期待のファイルに置かないため。
# 使い方: cmake "-DCOMMAND=<;区切りのコマンド>" -DEXPECTED=<期待の出力のファイル> -DWORK=<作業ディレクトリ>
#   [-DSUMMARY=1] -P small_stack.cmake

file(MAKE_DIRECTORY "${WORK}")
if(CMAKE_HOST_WIN32)
  set(run ${COMMAND})
else()
  # exec で置き換えるので、ulimit はコマンド自身の主スレッドのスタックにかかる。
  set(run sh -c "ulimit -s 1024 && exec \"$@\"" small_stack ${COMMAND})
endif()
execute_process(COMMAND ${run} WORKING_DIRECTORY "${WORK}"
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE code TIMEOUT 600)
string(REPLACE "\r\n" "\n" out "${out}")
if(SUMMARY AND code EQUAL 0)
  set(summary "")
  while(NOT out STREQUAL "")
    string(FIND "${out}" "\n" end)
    if(end EQUAL -1)
      string(LENGTH "${out}" end)
    endif()
    string(SUBSTRING "${out}" 0 ${end} line)
    math(EXPR next "${end} + 1")
    string(SUBSTRING "${out}" ${next} -1 out)
    string(LENGTH "${line}" length)
    set(from 0)
    if(length GREATER 12)
      math(EXPR from "${length} - 12")
    endif()
    string(SUBSTRING "${line}" 0 12 head)
    string(SUBSTRING "${line}" ${from} 12 tail)
    string(APPEND summary "${length}|${head}|${tail}\n")
  endwhile()
  set(out "${summary}")
endif()
file(READ "${EXPECTED}" expected)
string(REPLACE "\r\n" "\n" expected "${expected}")
if(NOT code EQUAL 0 OR NOT out STREQUAL expected)
  message(FATAL_ERROR "small_stack: code=[${code}]\nout=[${out}]\nexpected=[${expected}]\nerr=[${err}]")
endif()
