# 計画 Task 10。cmake --install を一時の prefix に行い、SPEC 第6節の形（<prefix>/bin/ の 3 コマンド、
# <prefix>/lib/mosa/<OS>-<arch>-<compiler>/ の libmosa、include/mosa/、翻訳済みの lib/*.pl の単位）を確かめ、
# インストールした mosac で hello を build して動かす。mosac はインストールの形から実行時ライブラリを見つける。
# 使い方: cmake -DBUILD_DIR=<ビルド木> -DCONFIG=<--config の値> -DABI=<OS-arch-compiler> -DSRC=<tests/mosac>
#   -DWORK=<一時ディレクトリ> -DCXX=<コンパイラ。空なら mosac の既定> -DEXE_SUFFIX=<.exe など>
#   -DLIB_PREFIX=<lib など> -DLIB_SUFFIX=<.a など> -P installed_layout_test.cmake

# runtime の部品は Release だけ入れる（CMakeLists.txt の CONFIGURATIONS Release）。ほかの構成の木には確かめる物が
# 無いので、理由を出して飛ばす（tests.cmake の SKIP_REGULAR_EXPRESSION がこの文を skip と数える）。
if(NOT CONFIG STREQUAL "Release")
  message(STATUS "skipped: the runtime component is installed only for Release (this build's config is '${CONFIG}')")
  return()
endif()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/work")
set(prefix "${WORK}/prefix")

set(install_args --install "${BUILD_DIR}" --prefix "${prefix}")
if(CONFIG)
  list(APPEND install_args --config "${CONFIG}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" ${install_args}
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE code TIMEOUT 600)
if(NOT code EQUAL 0)
  message(FATAL_ERROR "install: code=[${code}] out=[${out}] err=[${err}]")
endif()

if(NOT ABI)
  message(FATAL_ERROR "ABI is empty: the build must define the <OS>-<arch>-<compiler> key")
endif()
set(runtime "${prefix}/lib/mosa/${ABI}")
foreach(expected
    "${prefix}/bin/mosa${EXE_SUFFIX}"
    "${prefix}/bin/mosac${EXE_SUFFIX}"
    "${prefix}/bin/mosatest${EXE_SUFFIX}"
    "${prefix}/bin/lib/lists.pl"
    "${prefix}/bin/lib/messages_ja.pl"
    "${runtime}/${LIB_PREFIX}mosa${LIB_SUFFIX}"
    "${runtime}/include/mosa/runtime.hpp"
    "${runtime}/pl/${LIB_PREFIX}mosa_pl_lists${LIB_SUFFIX}"
    "${runtime}/pl/${LIB_PREFIX}mosa_pl_messages_ja${LIB_SUFFIX}"
    "${runtime}/pl/${LIB_PREFIX}mosa_pl_messages_en${LIB_SUFFIX}"
    "${prefix}/lib/mosa/source/CMakeLists.txt"
    "${prefix}/lib/mosa/source/include/mosa/runtime.hpp")
  if(NOT EXISTS "${expected}")
    file(GLOB_RECURSE installed LIST_DIRECTORIES false RELATIVE "${prefix}" "${prefix}/*")
    list(FILTER installed EXCLUDE REGEX "^lib/mosa/source/")
    message(FATAL_ERROR "missing after install: ${expected}\ninstalled: ${installed}")
  endif()
endforeach()

# インストールした mosac で build する。MOSA_LIB は外し、翻訳のとき library(_) を <prefix>/bin/lib/ から読ませる。
set(ENV{MOSA_WERROR} 1)
set(compiler_env)
if(CXX)
  set(compiler_env "MOSA_CXX=${CXX}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -E env --unset=MOSA_LIB ${compiler_env}
    "${prefix}/bin/mosac${EXE_SUFFIX}" build "${SRC}/hello.pl" -o hello
  WORKING_DIRECTORY "${WORK}/work"
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE code TIMEOUT 600)
if(NOT code EQUAL 0 OR NOT EXISTS "${WORK}/work/hello${EXE_SUFFIX}")
  message(FATAL_ERROR "build with the installed mosac: code=[${code}] out=[${out}] err=[${err}]")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -E env --unset=MOSA_LIB "${WORK}/work/hello${EXE_SUFFIX}"
  WORKING_DIRECTORY "${WORK}/work"
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE code TIMEOUT 60)
string(REPLACE "\r\n" "\n" out "${out}")
if(NOT code EQUAL 0 OR NOT out STREQUAL "こんにちは\n")
  message(FATAL_ERROR "run hello built by the installed mosac: code=[${code}] out=[${out}] err=[${err}]")
endif()
