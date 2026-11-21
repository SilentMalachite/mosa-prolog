# レビュー 2026-11-20 の H-29。生成した CMakeLists.txt は mosac 自身のディレクトリ（実行時のディレクトリの既定）を
# 角括弧の引数で書く。その名前に ]==] があっても引数が破れず、configure が名前のとおりの場所を見ることを確かめる。
# mosac だけを妙な名前のディレクトリに写し、隣に空の include/mosa/runtime.hpp と空の libmosa を置く（configure は
# EXISTS と find_library しか見ない）。lib/ は置かないので、翻訳の library(_) は MOSA_LIB から読み、どのライブラリも
# 原文だけの同梱になって pl/ の書庫を探さない。
# 名前は「]==]) の後に命令、[==[ で開き直し、]== で終わる」形。改行を挟めると命令が動き（前は message(WARNING) が出た）、
# 挟めなくても前は configure が構文の誤りで止まった。]== で終わる名前は、閉じの ]==] とまたがって先に閉じる。
# Windows はファイル名に改行を使えないので、そこだけ空白にする。
# 使い方: cmake -DMOSAC=<mosac> -DSRC=<tests/mosac> -DWORK=<一時ディレクトリ> -DMOSA_LIB=<lib/>
#   -DGENERATOR=<生成器> -DPLATFORM=<-A の値> -DCXX=<コンパイラ> -DBUILD_TYPE=<構成> -DEXE_SUFFIX=<.exe など>
#   -P bracket_path_test.cmake

file(REMOVE_RECURSE "${WORK}")
if(CMAKE_HOST_WIN32)
  set(gap " ")
else()
  set(gap "\n")
endif()
set(runtime "${WORK}/y]==])${gap}message(WARNING INJECTED_BY_PATH)${gap}set(dummy [==[ z]==")
file(MAKE_DIRECTORY "${runtime}/include/mosa")
file(WRITE "${runtime}/include/mosa/runtime.hpp" "")
file(WRITE "${runtime}/libmosa.a" "")
file(WRITE "${runtime}/mosa.lib" "")
get_filename_component(mosac_name "${MOSAC}" NAME)
file(COPY_FILE "${MOSAC}" "${runtime}/${mosac_name}")

function(fail what)
  message(FATAL_ERROR "bracket_path: ${what}: code=[${code}] out=[${out}] err=[${err}]")
endfunction()

# cmake -E env は改行を含むパスの実行ファイルを起こせない（no such file or directory）ので、環境はここで置く。
set(ENV{MOSA_LIB} "${MOSA_LIB}")
execute_process(COMMAND "${runtime}/${mosac_name}" emit hello.pl -o "${WORK}/out"
  WORKING_DIRECTORY "${SRC}"
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE code TIMEOUT 60 ENCODING UTF-8)
if(NOT code EQUAL 0 OR NOT EXISTS "${WORK}/out/CMakeLists.txt")
  fail("emit")
endif()

set(configure_args -S "${WORK}/out" -B "${WORK}/out/build" -G "${GENERATOR}" "-DCMAKE_BUILD_TYPE=${BUILD_TYPE}")
# Visual Studio の生成器はコンパイラを -A と構成から選ぶ（emit_test.cmake と同じ）。
if(NOT GENERATOR MATCHES "^Visual Studio")
  list(APPEND configure_args "-DCMAKE_CXX_COMPILER=${CXX}")
endif()
if(PLATFORM)
  list(APPEND configure_args -A "${PLATFORM}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" ${configure_args}
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE code TIMEOUT 300 ENCODING UTF-8)
# 0 で終われば、EXISTS が名前のとおりの runtime.hpp を見つけ、find_library がその隣の libmosa を見つけた
# （引数が途中で閉じれば、インストールの形の既定へ進んで find_library が止まるか、構文の誤りで止まる）。
if(NOT code EQUAL 0)
  fail("configure")
endif()
if("${out}${err}" MATCHES "INJECTED_BY_PATH")
  fail("a command written in mosac's directory name ran")
endif()
