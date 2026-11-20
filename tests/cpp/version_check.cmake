# 版表示のテスト（レビュー L-46）。PASS_REGULAR_EXPRESSION を付けると CTest は終了コードを見ないので、終了コードと
# 出力の両方をここで見る。版は手で書かず、CMakeLists.txt の project VERSION を VERSION で受け取る。
# 使い方: cmake "-DCOMMAND=<実行ファイル>;--version" -DVERSION=<project VERSION> -P version_check.cmake
# COMMAND は引数ごとのリストで受ける。version_check_test.cmake が偽の版表示を同じ形で渡すため。

if(NOT DEFINED VERSION OR VERSION STREQUAL "")
  # 空のままだと、どの版の行にも合ってしまう。
  message(FATAL_ERROR "VERSION is empty: pass -DVERSION=\${PROJECT_VERSION}")
endif()
string(REPLACE "." "\\." version_regex "${VERSION}")
# ENCODING UTF-8: 子は UTF-8 で書く。Windows の既定は CMP0176（3.31）が NEW でなければ AUTO で、コンソールのコードページで読む。
execute_process(COMMAND ${COMMAND} OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE code
  TIMEOUT 60 ENCODING UTF-8)
if(NOT code EQUAL 0)
  message(FATAL_ERROR "exit code ${code}, want 0: ${COMMAND}: out=[${out}] err=[${err}]")
endif()
if(NOT out MATCHES "^mosa ${version_regex}\r?\n?$")
  message(FATAL_ERROR "want [mosa ${VERSION}]: ${COMMAND}: out=[${out}]")
endif()
