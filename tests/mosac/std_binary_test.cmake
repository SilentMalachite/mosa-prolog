# レビュー L8（iso-notes「7.10.2 と 8.12」の決定 A）。標準入力に CRLF と 0x1A を渡す。mosa も mosac で作った実行ファイルも
# （REPL を含めて）、どの OS でもバイトのとおりに読み、標準出力と標準エラーに LF だけを書く。Windows のテキストモードなら、
# 0x1A で入力が終わり、CRLF の CR が落ち、nl が CRLF になる。
# 入力は CR と 0x1A を持つ std_binary*.in で、.gitattributes の -text で改行を変えずに取り出す（Windows の file(WRITE) は
# 改行を CRLF にしうる。L8 の敵対的レビュー A1）。execute_process の OUTPUT_VARIABLE は CRLF を LF にそろえるので
# （L8 の Codex レビュー）、出力はファイルに受けてバイトの 16 進で比べる。
# 使い方: cmake -DMOSA=<mosa> -DMOSAC=<mosac> -DSRC=<tests/mosac> -DWORK=<一時ディレクトリ>
#   -DCXX=<コンパイラ。空なら mosac の既定> -DEXE_SUFFIX=<.exe など> -DMOSA_LIB=<lib/> -P std_binary_test.cmake

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
file(COPY "${SRC}/std_binary.pl" DESTINATION "${WORK}")

set(ENV{MOSA_WERROR} 1)
set(compiler_env)
if(CXX)
  set(compiler_env "MOSA_CXX=${CXX}")
endif()

# INPUT を標準入力に渡して動かし、終了コードが 0 で、標準出力と標準エラーのバイトが WANT_OUT と WANT_ERR であること。
function(expect_bytes what input want_out want_err)
  string(HEX "${want_out}" want_out_hex)
  string(HEX "${want_err}" want_err_hex)
  execute_process(COMMAND ${ARGN}
    INPUT_FILE "${SRC}/${input}" WORKING_DIRECTORY "${WORK}"
    OUTPUT_FILE "${WORK}/run.out" ERROR_FILE "${WORK}/run.err" RESULT_VARIABLE code TIMEOUT 60)
  file(READ "${WORK}/run.out" out HEX)
  file(READ "${WORK}/run.err" err HEX)
  if(NOT code EQUAL 0 OR NOT out STREQUAL want_out_hex OR NOT err STREQUAL want_err_hex)
    message(FATAL_ERROR "std_binary: ${what}: code=[${code}] out=[${out}] err=[${err}]"
      " (hex; want out=[${want_out_hex}] err=[${want_err_hex}])")
  endif()
endfunction()

# mosac build を WORK で動かす。
function(build_program what)
  execute_process(COMMAND "${CMAKE_COMMAND}" -E env ${compiler_env} "MOSA_LIB=${MOSA_LIB}" "${MOSAC}" build ${ARGN}
    WORKING_DIRECTORY "${WORK}" OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE code TIMEOUT 600)
  if(NOT code EQUAL 0)
    message(FATAL_ERROR "std_binary: mosac build ${what}: code=[${code}] out=[${out}] err=[${err}]")
  endif()
endfunction()

set(main_out "[97,13,10,98,26,99,10]\n")
expect_bytes("mosa" std_binary.in "${main_out}" "done\n"
  "${CMAKE_COMMAND}" -E env "MOSA_LIB=${MOSA_LIB}" "${MOSA}" std_binary.pl)

build_program("main" std_binary.pl -o std_binary)
expect_bytes("the executable mosac built" std_binary.in "${main_out}" "done\n"
  "${CMAKE_COMMAND}" -E env --unset=MOSA_LIB "${WORK}/std_binary${EXE_SUFFIX}")

# REPL も同じ標準の流れを読み書きする。行末の CR は終止符の後の空白で、答えの行と促しは LF だけで書く。
build_program("--interactive" --interactive std_binary.pl -o std_binary_repl)
expect_bytes("the REPL of the executable mosac built" std_binary_repl.in "?- X = 1.\n?- a\ntrue.\n?- " ""
  "${CMAKE_COMMAND}" -E env --unset=MOSA_LIB "${WORK}/std_binary_repl${EXE_SUFFIX}")
