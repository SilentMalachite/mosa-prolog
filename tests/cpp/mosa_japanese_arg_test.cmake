# SPEC 第4節。日本語のファイル名と日本語のゴールをコマンドラインで渡せる（計画 Task 15b）。
# Windows の argv は ANSI コードページなので、CI の windows で意味を持つ。CMake は子を UTF-16 の引数で起こす。
# 使い方: cmake -DMOSA=<mosa> -DWORK=<一時ディレクトリ> -P mosa_japanese_arg_test.cmake

file(MAKE_DIRECTORY "${WORK}")
set(source "${WORK}/日本語の引数.pl")
file(WRITE "${source}" "読込済(はい).\n")
# ENCODING UTF-8: 子は UTF-8 で書く。Windows の既定は CMP0176（3.31）が NEW でなければ AUTO で、コンソールのコードページで読み、932 などでは化ける。
execute_process(COMMAND "${MOSA}" "${source}" -g "読込済(X), write(X), nl." -t halt
  OUTPUT_VARIABLE out
  ERROR_VARIABLE err
  RESULT_VARIABLE code
  TIMEOUT 20 ENCODING UTF-8)
file(REMOVE "${source}")
string(FIND "${out}" "はい\n" at)
if(NOT code EQUAL 0 OR at EQUAL -1)
  message(FATAL_ERROR "japanese arg: code=${code} out=[${out}] err=[${err}]")
endif()
