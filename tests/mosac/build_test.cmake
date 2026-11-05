# 計画 Task 9。mosac build と mosac run を、作業場所 WORK（.mosac-cache はその下にできる）で動かして確かめる。
# CMake の道と直接の道（MOSA_NO_CMAKE=1）の両方で同じ確かめを流す。どちらの道かは PATH_KIND で受ける。
# 使い方: cmake -DMOSAC=<mosac> -DCASE=<build_hello|run_exit_code|run_args|cache_reuse|internal_error|debug_trace>
#   -DSRC=<tests/mosac> -DWORK=<一時ディレクトリ> -DCXX=<コンパイラ。空なら mosac の既定>
#   -DFAKE_CXX=<生成コードだけ失敗するコンパイラ> -DFAILING_CXX=<いつも失敗するコンパイラ>
#   -DEXE_SUFFIX=<.exe など> -DMOSA_LIB=<lib/> -DPATH_KIND=<cmake|direct> -P build_test.cmake

# 前回の .mosac-cache が残ると、1 回目の build がコンパイラを呼ぶことの確かめが意味を持たない。
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

# 生成コードは警告なしで通ること。mosac build は MOSA_WERROR=1 のときだけ警告を誤りにする。
set(ENV{MOSA_WERROR} 1)

# 本体と同じコンパイラを MOSA_CXX で渡す（Windows は空で、mosac が vswhere で探す）。
set(compiler_env)
if(CXX)
  set(compiler_env "MOSA_CXX=${CXX}")
endif()

# mosac を WORK で動かす。結果は code、out、err に入る。
macro(mosac)
  execute_process(COMMAND "${CMAKE_COMMAND}" -E env ${compiler_env} "MOSA_LIB=${MOSA_LIB}" "${MOSAC}" ${ARGN}
    WORKING_DIRECTORY "${WORK}"
    OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE code TIMEOUT 600)
  string(REPLACE "\r\n" "\n" out "${out}")
  string(REPLACE "\r\n" "\n" err "${err}")
endmacro()

function(fail what)
  message(FATAL_ERROR "${CASE} (${PATH_KIND}): ${what}: code=[${code}] out=[${out}] err=[${err}]")
endfunction()

# --verbose は子プロセスを起こすたびに「mosac: 実行: <命令>」を書く。CMake の道は cmake --build を、
# 直接の道はそれを起こさない。CMake の命令は -S か --build の語で見分ける（パスに cmake を含む場所でも誤らない）。
set(run_line "mosac: 実行: ")
function(expect_compiled what)
  if(NOT code EQUAL 0 OR NOT err MATCHES "${run_line}")
    fail("${what} must call the compiler")
  endif()
  if(PATH_KIND STREQUAL "cmake" AND NOT err MATCHES "${run_line}[^\n]*cmake[^\n]* --build ")
    fail("${what} must go through CMake")
  endif()
  if(PATH_KIND STREQUAL "direct" AND err MATCHES "${run_line}[^\n]* (-S|--build) ")
    fail("${what} must not use CMake")
  endif()
endfunction()

function(expect_reused what)
  if(NOT code EQUAL 0 OR err MATCHES "${run_line}" OR NOT err MATCHES "mosac: キャッシュを使います: ")
    fail("${what} must reuse the cache")
  endif()
endfunction()

# レビュー M5。PROGRAM.pl を mosac run で動かし、終了コードが CODE で、WORK/TARGET の中身が TEXT であること。
# mosac は out・err・code を書き換えるので、引数にその名前を使わない。
function(expect_unclosed program expected_code target expected_text)
  file(REMOVE "${WORK}/${target}")
  mosac(run "${SRC}/${program}.pl")
  if(NOT code EQUAL expected_code OR NOT EXISTS "${WORK}/${target}")
    fail("${program}: exit code or ${target} missing")
  endif()
  file(READ "${WORK}/${target}" got)
  if(NOT got STREQUAL expected_text)
    fail("${program}: ${target} holds [${got}]")
  endif()
endfunction()

if(CASE STREQUAL "build_hello")
  mosac(build "${SRC}/hello.pl" -o hello)
  if(NOT code EQUAL 0 OR NOT EXISTS "${WORK}/hello${EXE_SUFFIX}")
    fail("build")
  endif()
  execute_process(COMMAND "${WORK}/hello${EXE_SUFFIX}"
    OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE code TIMEOUT 60)
  string(REPLACE "\r\n" "\n" out "${out}")
  if(NOT code EQUAL 0 OR NOT out STREQUAL "こんにちは\n")
    fail("run the built executable")
  endif()
elseif(CASE STREQUAL "run_exit_code")
  # halt.pl は指示の halt(3) で読み込みを止める。mosac run は子の終了コードをそのまま返す。
  mosac(run "${SRC}/halt.pl")
  if(NOT code EQUAL 3 OR NOT out STREQUAL "a\n")
    fail("exit code")
  endif()
  # ADR 0014。Call の節点として call/1 で動く catch の Goal と Recovery の中の halt/1 も、生成コードの終了コードになる。
  # op_kept はレビュー M4。生成コードでも op/3 が戻らなければ 3 で終わる。undefined_handled はレビュー L12。未定義の
  # 呼び出しは警告で、mosac run は止まらずに作って動かす。
  foreach(late halt_late_goal:4 halt_late_recovery:5 halt_noncallable_goal:0 halt_noncallable_recovery:0 halt_noncallable_not:0 noncallable_undefined:0 op_kept:3
                undefined_handled:0)
    string(REPLACE ":" ";" late "${late}")
    list(GET late 0 late_file)
    list(GET late 1 late_code)
    mosac(run "${SRC}/${late_file}.pl")
    if(NOT code EQUAL late_code)
      fail("${late_file}: exit code")
    endif()
    # 警告は止めないので、出るべき警告と出てはならない警告を標準エラーで見る（L12 の敵対的レビュー A1 と A2）。
    # noncallable_undefined は数のある Call 節点を検査しないこと（ADR 0014）。頭は文面の言語で変わる。
    if(late_file STREQUAL "undefined_handled")
      if(NOT err MATCHES "undefined_handled\\.pl:5: (Warning|警告): [^\n]*nosuch/0"
         OR NOT err MATCHES "undefined_handled\\.pl:5: (Warning|警告): [^\n]*nosuch2/0")
        fail("${late_file}: mosac run must print the undefined-call warnings")
      endif()
    elseif(err MATCHES "\\.pl:[0-9]+: (Warning|警告): ")
      fail("${late_file}: unexpected undefined-call warning")
    endif()
  endforeach()
  # レビュー M5。close を書かない書き込みも、halt と main の終わりで実行ファイルがファイルへ書き出す。
  # 書き先は相対パスなので、mosac run の作業場所 WORK にできる。
  expect_unclosed(open_unclosed_halt 7 m5_unclosed_halt.txt "halted")
  expect_unclosed(open_unclosed_end 0 m5_unclosed_end.txt "ended\n")
  # レビュー M1。min_integer の字面を持つプログラムを訳して動かせる。
  mosac(run "${SRC}/min_integer.pl")
  string(REPLACE "\r\n" "\n" out "${out}")
  if(NOT code EQUAL 0 OR NOT out STREQUAL "[-9223372036854775808,head,-9223372036854775808]\n")
    fail("min_integer literal")
  endif()
  # 敵対的レビュー A3（M11、ADR 0016）。選択点を残して成功した main も、答えの後で切って後始末を動かす。
  mosac(run "${SRC}/cleanup_main.pl")
  if(NOT code EQUAL 0 OR NOT out STREQUAL "got(1)\ncleanup(1)\n")
    fail("main cleanup")
  endif()
elseif(CASE STREQUAL "run_args")
  mosac(run "${SRC}/args.pl" -- a 漢字)
  if(NOT code EQUAL 0 OR NOT out STREQUAL "[a,漢字]\n")
    fail("argv")
  endif()
elseif(CASE STREQUAL "cache_reuse")
  # 原文は WORK に写して書き換える。キャッシュの鍵が原文、--debug、--clean のどれでも古い物を出さないことを見る。
  file(COPY "${SRC}/hello.pl" DESTINATION "${WORK}")
  mosac(build hello.pl -o hello --verbose)
  expect_compiled("first build")
  mosac(build hello.pl -o hello --verbose)
  expect_reused("second build")
  mosac(build hello.pl -o hello --verbose --debug)
  expect_compiled("--debug build")
  file(APPEND "${WORK}/hello.pl" "main2 :- write(changed), nl.\n")
  mosac(build hello.pl -o hello --verbose)
  expect_compiled("build after the source changed")
  mosac(build hello.pl -o hello --verbose)
  expect_reused("build after the change")
  mosac(build hello.pl -o hello --verbose --clean)
  expect_compiled("--clean build")
  if(NOT IS_DIRECTORY "${WORK}/.mosac-cache")
    fail("the cache must live in the working directory")
  endif()
elseif(CASE STREQUAL "internal_error")
  set(compiler_env "MOSA_CXX=${FAKE_CXX}")
  set(internal "^内部エラー: 生成コードがコンパイルできません。\\.mosac-cache/[0-9a-f]+/ を添えて報告してください\n$")
  # 名指しの cl は vcvarsall を通らない（持ち主の決定 A）。INCLUDE が無ければ、コンパイルの前に開発者用のプロンプトを
  # 案内して止まる。偽の cl を生成コードまで届かせるため、その後は INCLUDE を置いて開発者用のプロンプトの形にする。
  if(CMAKE_HOST_WIN32)
    set(compiler_env "--unset=INCLUDE" "MOSA_CXX=${FAKE_CXX}")
    mosac(build "${SRC}/hello.pl" -o hello)
    if(NOT code EQUAL 1 OR NOT err MATCHES "^mosac: cl が Visual Studio の環境無し" OR err MATCHES "内部エラー")
      fail("a named cl without INCLUDE must point to a Developer Command Prompt")
    endif()
    set(compiler_env "MOSA_CXX=${FAKE_CXX}" "INCLUDE=${WORK}")
  endif()
  # Windows の偽物は cl.exe の名（tests.cmake）なので、MOSA_NO_CMAKE=0 なら CMake の道を通る。偽の cl では CMake の
  # configure（コンパイラの確かめ）が通らず、それは内部エラーでなく環境の問題として出ること。生成コードの段の
  # 内部エラーは直接の道（mosac_internal_error_direct）で確かめる。
  if(PATH_KIND STREQUAL "cmake" AND CMAKE_HOST_WIN32)
    mosac(build "${SRC}/hello.pl" -o hello)
    if(NOT code EQUAL 1 OR NOT err MATCHES "^mosac: CMake の構成に失敗しました" OR err MATCHES "内部エラー")
      fail("configure failure with a fake cl")
    endif()
  else()
    mosac(build "${SRC}/hello.pl" -o hello)
    if(NOT code EQUAL 1 OR NOT err MATCHES "${internal}" OR NOT out STREQUAL "")
      fail("internal error without --verbose")
    endif()
    mosac(build "${SRC}/hello.pl" -o hello --verbose)
    if(NOT code EQUAL 1 OR NOT err MATCHES "fake-cxx: always fails"
       OR NOT err MATCHES "内部エラー: 生成コードがコンパイルできません。\\.mosac-cache/[0-9a-f]+/ を添えて報告してください")
      fail("internal error with --verbose")
    endif()
  endif()
  # CMake の道で configure が通らないのは環境の問題で、内部エラーではない（Windows は上の偽の cl で見た）。
  if(PATH_KIND STREQUAL "cmake" AND NOT CMAKE_HOST_WIN32)
    set(compiler_env "MOSA_CXX=${FAILING_CXX}")
    mosac(build "${SRC}/hello.pl" -o hello)
    if(NOT code EQUAL 1 OR NOT err MATCHES "^mosac: CMake の構成に失敗しました" OR err MATCHES "内部エラー")
      fail("configure failure")
    endif()
  endif()
elseif(CASE STREQUAL "debug_trace")
  # 生成関数が節に入るたびに、その節のコメントと同じ文を標準エラーに書く。
  mosac(run --debug "${SRC}/hello.pl")
  if(NOT code EQUAL 0 OR NOT out STREQUAL "こんにちは\n" OR NOT err MATCHES "(^|\n)main :- ")
    fail("trace")
  endif()
else()
  message(FATAL_ERROR "unknown case ${CASE}")
endif()
