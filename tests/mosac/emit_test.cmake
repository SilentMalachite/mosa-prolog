# 計画 Task 5。mosac emit の出した目録を、本体と同じ生成器、コンパイラ、構成で configure、build、実行する。
# 生成した C++ が 3 OS の CI でコンパイルできること（SPEC 第6節の不変条件）も、この build が確かめる。
# 使い方: cmake -DMOSAC=<mosac> -DCASE=<hello|broken|undefined|undefined_quoted|duplicate|silent|newop|halt> -DSRC=<tests/mosac> -DWORK=<一時ディレクトリ>
#   -DGENERATOR=<生成器> -DPLATFORM=<-A の値> -DCXX=<コンパイラ> -DBUILD_TYPE=<構成> -DCONFIG=<--config の値>
#   -DCXX_FLAGS=<CMAKE_CXX_FLAGS> -DLINKER_FLAGS=<CMAKE_EXE_LINKER_FLAGS> -DEXE_SUFFIX=<.exe など>
#   -DMOSA_LIB=<lib/> -P emit_test.cmake

# 生成コードは警告なしで通ること（生成した CMakeLists.txt の MOSA_WARNINGS_AS_ERRORS を立てる。計画 Task 9）。
set(ENV{MOSA_WERROR} 1)

# 前回の out/ が残ると、.cpp が無いことの確かめが意味を持たない。
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
set(out_dir "${WORK}/out")

# duplicate.pl は MOSA_LIB=mylib の library(x) と include した library/x.pl が同じ鍵 library/x.pl になる（計画 Task 10）。
set(emit_env)
if(CASE STREQUAL "duplicate")
  set(emit_env "${CMAKE_COMMAND}" -E env "MOSA_LIB=${SRC}/mylib")
endif()

# 診断の頭はコマンド行に書いたパスのままなので、ファイル名だけを渡す。
execute_process(COMMAND ${emit_env} "${MOSAC}" emit "${CASE}.pl" -o "${out_dir}"
  WORKING_DIRECTORY "${SRC}"
  OUTPUT_VARIABLE emit_out
  ERROR_VARIABLE emit_err
  RESULT_VARIABLE emit_code
  TIMEOUT 60)

function(expect_no_cpp)
  file(GLOB written "${out_dir}/*.cpp")
  if(written)
    message(FATAL_ERROR "${CASE}: .cpp written on error: ${written}")
  endif()
endfunction()

if(CASE STREQUAL "broken")
  if(NOT emit_code EQUAL 1 OR NOT emit_err MATCHES "^broken\\.pl:2:[0-9]+: ")
    message(FATAL_ERROR "broken: code=${emit_code} out=[${emit_out}] err=[${emit_err}]")
  endif()
  expect_no_cpp()
  return()
endif()

if(CASE STREQUAL "undefined")
  if(NOT emit_code EQUAL 1 OR NOT emit_err MATCHES "^undefined\\.pl:3: [^\n]*nosuch/1[^\n]*:- dynamic\\(nosuch/1\\)\\.")
    message(FATAL_ERROR "undefined: code=${emit_code} out=[${emit_out}] err=[${emit_err}]")
  endif()
  expect_no_cpp()
  return()
endif()

# :- dynamic(~w). の案内は貼れば読める形で書く。引用の要る名前は writeq の形、演算子と記号文字の名前は括弧に入れる。
if(CASE STREQUAL "undefined_quoted")
  foreach(hint "'Foo'/1" "'a b'/0" "\\(-\\)/1" "'it\\\\'s'/2" "\\(\\+\\+\\+\\)/1")
    if(NOT emit_err MATCHES "undefined_quoted\\.pl:3: [^\n]*:- dynamic\\(${hint}\\)\\.")
      message(FATAL_ERROR "undefined_quoted: ${hint}: code=${emit_code} out=[${emit_out}] err=[${emit_err}]")
    endif()
  endforeach()
  if(NOT emit_code EQUAL 1)
    message(FATAL_ERROR "undefined_quoted: code=${emit_code} out=[${emit_out}] err=[${emit_err}]")
  endif()
  expect_no_cpp()
  return()
endif()

# 同じ鍵で 2 つのファイルを埋め込めない。翻訳の段で両方のファイルと鍵を挙げて止まり、.cpp は書かない。
if(CASE STREQUAL "duplicate")
  if(NOT emit_code EQUAL 1 OR NOT emit_err MATCHES "^mosac: [^\n]*library/x\\.pl[^\n]*mylib/x\\.pl[^\n]*library/x\\.pl")
    message(FATAL_ERROR "duplicate: code=${emit_code} out=[${emit_out}] err=[${emit_err}]")
  endif()
  expect_no_cpp()
  return()
endif()

# 翻訳のときの読み込みは宣言だけを実行する。silent.pl の :- write(x). はここでは何も書かない。
if(NOT emit_code EQUAL 0 OR NOT emit_out STREQUAL "")
  message(FATAL_ERROR "${CASE}: emit code=${emit_code} out=[${emit_out}] err=[${emit_err}]")
endif()

# 旗（ASan など）も本体に合わせる。libmosa の記号を引く側が同じ旗でないとリンクできない。
set(configure_args -S "${out_dir}" -B "${out_dir}/build" -G "${GENERATOR}" "-DCMAKE_BUILD_TYPE=${BUILD_TYPE}"
  "-DCMAKE_CXX_FLAGS=${CXX_FLAGS}" "-DCMAKE_EXE_LINKER_FLAGS=${LINKER_FLAGS}")
# Visual Studio の生成器はコンパイラを -A と構成から選ぶ。
if(NOT GENERATOR MATCHES "^Visual Studio")
  list(APPEND configure_args "-DCMAKE_CXX_COMPILER=${CXX}")
endif()
if(PLATFORM)
  list(APPEND configure_args -A "${PLATFORM}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" ${configure_args}
  OUTPUT_VARIABLE configure_out ERROR_VARIABLE configure_err RESULT_VARIABLE configure_code TIMEOUT 300)
if(NOT configure_code EQUAL 0)
  message(FATAL_ERROR "${CASE}: configure code=${configure_code} out=[${configure_out}] err=[${configure_err}]")
endif()

set(build_args --build "${out_dir}/build")
if(CONFIG)
  list(APPEND build_args --config "${CONFIG}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" ${build_args}
  OUTPUT_VARIABLE build_out ERROR_VARIABLE build_err RESULT_VARIABLE build_code TIMEOUT 600)
if(NOT build_code EQUAL 0)
  message(FATAL_ERROR "${CASE}: build code=${build_code} out=[${build_out}] err=[${build_err}]")
endif()

# 複数構成の生成器（Visual Studio）は構成名のディレクトリへ出す。
set(exe "${out_dir}/build/${CONFIG}/${CASE}${EXE_SUFFIX}")
if(NOT EXISTS "${exe}")
  set(exe "${out_dir}/build/${CASE}${EXE_SUFFIX}")
endif()
# 単独の実行ファイルは lib/ を探さない（計画 Task 10）。MOSA_LIB を外して、文面の表も同梱の単位から読ませる。
execute_process(COMMAND "${CMAKE_COMMAND}" -E env --unset=MOSA_LIB "${exe}"
  OUTPUT_VARIABLE run_out ERROR_VARIABLE run_err RESULT_VARIABLE run_code TIMEOUT 60)

# newop.pl は翻訳のときに飛ばした指示のゴールが op/3 を定義する。翻訳器は後の項を読めないが誤りにせず、
# その述語は起動時に IR のまま動く。halt.pl は指示の halt(3) で読み込みを止め、後の指示も main も動かさない。
set(expected_code 0)
if(CASE STREQUAL "hello")
  set(expected "こんにちは\n")
elseif(CASE STREQUAL "newop")
  set(expected "a-b\n")
elseif(CASE STREQUAL "halt")
  set(expected "a\n")
  set(expected_code 3)
else()
  set(expected "x")
endif()
string(REPLACE "\r\n" "\n" run_out "${run_out}")
if(NOT run_code EQUAL expected_code OR NOT run_out STREQUAL expected)
  message(FATAL_ERROR "${CASE}: run code=${run_code} out=[${run_out}] err=[${run_err}]")
endif()
