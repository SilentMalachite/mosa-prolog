# 計画 Task 5。mosac emit の出した目録を、本体と同じ生成器、コンパイラ、構成で configure、build、実行する。
# 生成した C++ が 3 OS の CI でコンパイルできること（SPEC 第6節の不変条件）も、この build が確かめる。
# 使い方: cmake -DMOSAC=<mosac> -DCASE=<hello|broken|undefined|undefined_catch|undefined_quoted|undefined_handled|refused|duplicate|silent|newop|refused_op|halt|trigraph|bigint> -DSRC=<tests/mosac> -DWORK=<一時ディレクトリ>
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
# ENCODING UTF-8: 子は UTF-8 で書く。Windows の既定は CMP0176（3.31）が NEW でなければ AUTO で、コンソールのコードページで読み、932 などでは化ける。
execute_process(COMMAND ${emit_env} "${MOSAC}" emit "${CASE}.pl" -o "${out_dir}"
  WORKING_DIRECTORY "${SRC}"
  OUTPUT_VARIABLE emit_out
  ERROR_VARIABLE emit_err
  RESULT_VARIABLE emit_code
  TIMEOUT 60 ENCODING UTF-8)

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

# 未定義の述語の呼び出しは警告にして翻訳を続け、実行ファイルは mosa と同じく呼んだときに動く（レビュー L12、
# 持ち主の決定 A）。警告の確かめの後は、ほかの場合と同じく build して実行する。警告の頭は文面の言語で変わる。
set(warning_head "(Warning|警告): ")
if(CASE STREQUAL "undefined")
  if(NOT emit_err MATCHES "^undefined\\.pl:3: ${warning_head}[^\n]*nosuch/1[^\n]*:- dynamic\\(nosuch/1\\)\\.")
    message(FATAL_ERROR "undefined: code=${emit_code} out=[${emit_out}] err=[${emit_err}]")
  endif()
endif()

# ADR 0014。ゴール位置に変数を含む catch/3 の Goal と Recovery は Call の節点になる。その中で名指す述語も
# 翻訳の時点で未定義と分かる（iso-notes 7.7.7 and SPEC Section 5）。
if(CASE STREQUAL "undefined_catch")
  foreach(want "undefined_catch\\.pl:3: ${warning_head}[^\n]*nosuch_goal/0" "undefined_catch\\.pl:4: ${warning_head}[^\n]*nosuch_recovery/0")
    if(NOT emit_err MATCHES "${want}")
      message(FATAL_ERROR "undefined_catch: ${want}: code=${emit_code} out=[${emit_out}] err=[${emit_err}]")
    endif()
  endforeach()
endif()

# :- dynamic(~w). の案内は貼れば読める形で書く。引用の要る名前は writeq の形、演算子と記号文字の名前は括弧に入れる。
if(CASE STREQUAL "undefined_quoted")
  foreach(hint "'Foo'/1" "'a b'/0" "\\(-\\)/1" "'it\\\\'s'/2" "\\(\\+\\+\\+\\)/1")
    if(NOT emit_err MATCHES "undefined_quoted\\.pl:3: ${warning_head}[^\n]*:- dynamic\\(${hint}\\)\\.")
      message(FATAL_ERROR "undefined_quoted: ${hint}: code=${emit_code} out=[${emit_out}] err=[${emit_err}]")
    endif()
  endforeach()
endif()

# レビュー L12 の再現。catch で existence_error を捕まえるか unknown を fail にしたプログラムも、警告を出して作る。
if(CASE STREQUAL "undefined_handled")
  foreach(want "undefined_handled\\.pl:5: ${warning_head}[^\n]*nosuch/0" "undefined_handled\\.pl:5: ${warning_head}[^\n]*nosuch2/0")
    if(NOT emit_err MATCHES "${want}")
      message(FATAL_ERROR "undefined_handled: ${want}: code=${emit_code} out=[${emit_out}] err=[${emit_err}]")
    endif()
  endforeach()
endif()

# レビュー M10（iso-notes 7.4 の決定 A）。組込述語の節は、インタプリタと同じ読み込みの誤りで止まり、.cpp は書かない。
if(CASE STREQUAL "refused")
  if(NOT emit_code EQUAL 1 OR NOT emit_err MATCHES "refused\\.pl[^\n]*permission_error\\(modify,static_procedure,atom_length/2\\)")
    message(FATAL_ERROR "refused: code=${emit_code} out=[${emit_out}] err=[${emit_err}]")
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

# 翻訳のときの読み込みは宣言だけを実行する。silent.pl の :- write(x). はここでは何も書かない。未定義の呼び出しの
# 無い原文は標準エラーにも何も書かない。警告は翻訳を止めないので、誤った警告はここで見る（L12 の敵対的レビュー A1）。
if(NOT CASE MATCHES "^undefined")
  set(clean_err "${emit_err}")
else()
  set(clean_err "")
endif()
if(NOT emit_code EQUAL 0 OR NOT emit_out STREQUAL "" OR NOT clean_err STREQUAL "")
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
  OUTPUT_VARIABLE configure_out ERROR_VARIABLE configure_err RESULT_VARIABLE configure_code TIMEOUT 300 ENCODING UTF-8)
if(NOT configure_code EQUAL 0)
  message(FATAL_ERROR "${CASE}: configure code=${configure_code} out=[${configure_out}] err=[${configure_err}]")
endif()

set(build_args --build "${out_dir}/build")
if(CONFIG)
  list(APPEND build_args --config "${CONFIG}")
endif()
# ここだけ AUTO: cmake --build はビルドツールの出力を変えずに渡し、MSBuild と cl はコンソールのコードページで書く。
# UTF-8 で読むと、日本語版の Windows で失敗したときの診断が化ける（Codex のレビューの P3）。成否は終了コードで見る。
execute_process(COMMAND "${CMAKE_COMMAND}" ${build_args}
  OUTPUT_VARIABLE build_out ERROR_VARIABLE build_err RESULT_VARIABLE build_code TIMEOUT 600 ENCODING AUTO)
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
  OUTPUT_VARIABLE run_out ERROR_VARIABLE run_err RESULT_VARIABLE run_code TIMEOUT 60 ENCODING UTF-8)

# newop.pl は翻訳のときに飛ばした指示のゴールが op/3 を定義する。翻訳器は後の項を読めないが誤りにせず、
# その述語は起動時に IR のまま動く。refused_op.pl は同じ指示の後の節を翻訳が読み違えて断るが、起動時には正しい節と
# 読む（M10 の Codex レビュー）。halt.pl は指示の halt(3) で読み込みを止め、後の指示も main も動かさない。
# trigraph.pl は字面の ? を \? にしないと -Wtrigraphs の警告でコンパイルできない（レビュー L9）。
set(expected_code 0)
if(CASE STREQUAL "hello")
  set(expected "こんにちは\n")
elseif(CASE STREQUAL "newop")
  set(expected "a-b\n")
elseif(CASE STREQUAL "refused_op")
  set(expected "ok\n")
elseif(CASE STREQUAL "halt")
  set(expected "a\n")
  set(expected_code 3)
elseif(CASE STREQUAL "trigraph")
  set(expected "a??=b??/c??(d??)e??!f??<g??>h??-i??'j\n")
elseif(CASE STREQUAL "bigint")
  # フェーズ10 タスク 10。値は mosa で同じ原文を動かした答えと同じ。
  string(CONCAT expected
    "p100=[a0,a2,a4,last]\n"
    "p100_1=[a1,a3,a5,last]\n"
    "neg_p100=[a6,last]\n"
    "p63=[a7,last]\n"
    "neg_p63_1=[a8,last]\n"
    "min_int=[a9,last]\n"
    "p62=[last]\n"
    "float=[last]\n"
    "computed=[a0,a2,a4,last]-[a1,a3,a5,last]\n"
    "heads=[1267650600228229401496703205376,1267650600228229401496703205377,1267650600228229401496703205376,"
    "1267650600228229401496703205377,1267650600228229401496703205376,1267650600228229401496703205377,"
    "-1267650600228229401496703205376,9223372036854775808,-9223372036854775809,-9223372036854775808]\n"
    "square=15241578753238836750495351562536198787501905199875019052100\n"
    "boundary=[9223372036854775808,-9223372036854775809,18446744073709551616,-340282366920938463463374607431768211457]\n"
    "boundary_2p64=yes\n"
    "nested_out=[1267650600228229401496703205376,-9223372036854775809,ok]\n"
    "nested_other=no\n"
    "nested_build=f([1267650600228229401496703205376,-9223372036854775809])\n"
    "twice=same\n"
    "wrap=w(18446744073709551616,-18446744073709551616)\n"
    "compare=yes\n"
    "qualified=type_error(callable,user:123456789012345678901234567890)\n"
    "negated=type_error(callable,(true,123456789012345678901234567890))\n")
  # IR のまま動いても同じ答えになるので、Big の定数と鍵が生成コードに出て、どの述語も IR に回っていないことも見る。
  file(READ "${out_dir}/muser.cpp" bigint_source)
  foreach(want "rt::put_big(" "rt::big_key_of(first)")
    string(FIND "${bigint_source}" "${want}" bigint_at)
    if(bigint_at EQUAL -1)
      message(FATAL_ERROR "bigint: ${want} is not in the generated code")
    endif()
  endforeach()
  if(bigint_source MATCHES "IR のまま動かす|rt::interpret_clause\\(")
    message(FATAL_ERROR "bigint: a predicate stays IR")
  endif()
elseif(CASE STREQUAL "undefined" OR CASE STREQUAL "undefined_quoted")
  # mosa と同じく、捕まえなかった existence_error で 2 で終わる。呼んだ述語の名前は文面の中に出る。
  set(expected "hello")
  set(expected_code 2)
  set(expected_err "existence_error\\(procedure,nosuch/1\\)")
  if(CASE STREQUAL "undefined_quoted")
    set(expected_err "existence_error\\(procedure,'Foo'/1\\)")
  endif()
elseif(CASE STREQUAL "undefined_catch")
  set(expected "hello")
elseif(CASE STREQUAL "undefined_handled")
  set(expected "caught\nfailed\n")
else()
  set(expected "x")
endif()
string(REPLACE "\r\n" "\n" run_out "${run_out}")
if(NOT run_code EQUAL expected_code OR NOT run_out STREQUAL expected OR (DEFINED expected_err AND NOT run_err MATCHES "${expected_err}"))
  message(FATAL_ERROR "${CASE}: run code=${run_code} out=[${run_out}] err=[${run_err}]")
endif()
