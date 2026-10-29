# mosa の標準入力。端末でない入力は Prolog の user_input のもので、mosa 自身が先に読み切らない。
# 先に読み切ると read/1 が end_of_file になり、閉じない入力では --version も返らない。
# 閉じない入力は移植性のある形で作れないので、入力が残っていることを read/1 で確かめる。
# 使い方: cmake -DMOSA=<mosa> -DWORK=<一時ディレクトリ> -P mosa_stdin_test.cmake

file(MAKE_DIRECTORY "${WORK}")
set(input "${WORK}/mosa_stdin_input.txt")
file(WRITE "${input}" "foo.\nbar.\n")

function(expect_output label regex)
  execute_process(COMMAND "${MOSA}" ${ARGN}
    INPUT_FILE "${input}"
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err
    RESULT_VARIABLE code
    TIMEOUT 20)
  if(NOT code EQUAL 0 OR NOT out MATCHES "${regex}")
    message(FATAL_ERROR "${label}: code=${code} out=[${out}] err=[${err}]")
  endif()
endfunction()

expect_output("read from stdin" "X = foo, Y = bar\\." -g "read(X), read(Y)." -t halt)
expect_output("version ignores stdin" "^mosa 0\\.0\\.1 \\(phase 6\\)" --version)
file(REMOVE "${input}")
