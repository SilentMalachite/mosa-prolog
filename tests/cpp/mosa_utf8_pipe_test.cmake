# SPEC 第4節。UTF-8 をパイプで渡すと、read/1 と write/1 を通ってもバイト列が変わらない。
# Windows の CI でも流し、コードページに左右されないことを確かめる。
# 使い方: cmake -DMOSA=<mosa> -DWORK=<一時ディレクトリ> -P mosa_utf8_pipe_test.cmake

file(MAKE_DIRECTORY "${WORK}")
set(input "${WORK}/mosa_utf8_pipe_input.txt")
file(WRITE "${input}" "親(太郎). \n")
execute_process(COMMAND "${MOSA}" -g "read(X), write(X), nl." -t halt
  INPUT_FILE "${input}"
  OUTPUT_VARIABLE out
  ERROR_VARIABLE err
  RESULT_VARIABLE code
  TIMEOUT 20)
file(REMOVE "${input}")
string(FIND "${out}" "親(太郎)\n" at)
if(NOT code EQUAL 0 OR at EQUAL -1)
  message(FATAL_ERROR "utf8 pipe: code=${code} out=[${out}] err=[${err}]")
endif()
