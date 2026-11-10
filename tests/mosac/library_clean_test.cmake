# L12 の敵対的レビュー A4。未定義の呼び出しは翻訳を止めない警告になったので、lib/*.pl の呼び出し名の誤記は
# 本体のビルド（CMakeLists.txt の emit --library）では止まらない。同梱ライブラリを 1 つずつ訳し、標準エラーに
# 何も書かないことを見る。
# 使い方: cmake -DMOSAC=<mosac> -DMOSA_LIB=<lib/> -DWORK=<一時ディレクトリ> -P library_clean_test.cmake

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

file(GLOB sources "${MOSA_LIB}/*.pl")
if(NOT sources)
  message(FATAL_ERROR "library_clean: no .pl under ${MOSA_LIB}")
endif()
foreach(source ${sources})
  get_filename_component(stem "${source}" NAME_WE)
  # ENCODING UTF-8: 子は UTF-8 で書く。Windows の既定は CMP0176（3.31）が NEW でなければ AUTO で、コンソールのコードページで読み、932 などでは化ける。
  execute_process(COMMAND "${CMAKE_COMMAND}" -E env "MOSA_LIB=${MOSA_LIB}"
      "${MOSAC}" emit --library "${source}" -o "${WORK}/${stem}"
    OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE code TIMEOUT 120 ENCODING UTF-8)
  if(NOT code EQUAL 0 OR NOT err STREQUAL "")
    message(FATAL_ERROR "library_clean: ${stem}.pl: code=[${code}] out=[${out}] err=[${err}]")
  endif()
endforeach()
