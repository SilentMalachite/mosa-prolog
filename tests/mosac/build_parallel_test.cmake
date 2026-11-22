# レビュー M-44。1 つの作業場所で同じプログラムの mosac build を 3 つ同時に流しても、どれも通る。前はどれも同じ
# .mosac-cache/<hash>/ で作り、作り始めに互いのディレクトリを消したので、3 回に 2〜3 つが「起動できません」や
# 「CMake の構成に失敗しました」で止まった。今は各自が <hash>/w-*/ で作り、先に終えた 1 つを <hash>/current が指す。
# 回をまたいで同じ -o へ書くので、一度動かした実行ファイルへ作り直した物を写す道も通る（前は上書きの写しで inode が
# 同じまま中身が替わり、macOS が SIGKILL（Code Signature Invalid）で殺した）。
# CMake の道と直接の道（MOSA_NO_CMAKE=1）の両方で流す。どちらの道かは PATH_KIND で受ける（tests/mosac_tests.cmake）。
# 使い方: cmake -DMOSAC=<mosac> -DSRC=<tests/mosac> -DWORK=<一時ディレクトリ> -DCXX=<コンパイラ。空なら mosac の既定>
#   -DEXE_SUFFIX=<.exe など> -DMOSA_LIB=<lib/> -DPATH_KIND=<cmake|direct> -P build_parallel_test.cmake

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
set(ENV{MOSA_WERROR} 1)
set(compiler_env)
if(CXX)
  set(compiler_env "MOSA_CXX=${CXX}")
endif()
file(COPY_FILE "${SRC}/hello.pl" "${WORK}/hello.pl")

function(fail what)
  message(FATAL_ERROR "build_parallel (${PATH_KIND}): ${what}: codes=[${codes}] code=[${code}] out=[${out}] err=[${err}]")
endfunction()

# execute_process の COMMAND を並べると同時に動く。並べた COMMAND は管でつながるが、成功した mosac build は標準出力に
# 何も書かず、標準入力も読まないので、管は使われない。毎回 .mosac-cache を消して、3 つとも外れから作らせる。
set(build "${CMAKE_COMMAND}" -E env ${compiler_env} "MOSA_LIB=${MOSA_LIB}" "${MOSAC}" build hello.pl)
foreach(round RANGE 1 3)
  file(REMOVE_RECURSE "${WORK}/.mosac-cache")
  execute_process(COMMAND ${build} -o a COMMAND ${build} -o b COMMAND ${build} -o c
                  WORKING_DIRECTORY "${WORK}" RESULTS_VARIABLE codes OUTPUT_VARIABLE out ERROR_VARIABLE err
                  TIMEOUT 1200 ENCODING UTF-8)
  if(NOT codes STREQUAL "0;0;0")
    fail("round ${round}: all three builds must succeed")
  endif()
  foreach(exe a b c)
    # ENCODING UTF-8: 子は UTF-8 で書く。Windows の既定は CMP0176（3.31）が NEW でなければ AUTO で、コンソールのコードページで読み、932 などでは化ける。
    execute_process(COMMAND "${WORK}/${exe}${EXE_SUFFIX}" WORKING_DIRECTORY "${WORK}"
                    OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE code TIMEOUT 120 ENCODING UTF-8)
    string(REPLACE "\r\n" "\n" out "${out}")
    if(NOT code EQUAL 0 OR NOT out STREQUAL "こんにちは\n")
      fail("round ${round}: ${exe} must print こんにちは")
    endif()
  endforeach()
  # 置き場所は <hash>/ の 1 つだけで、その中は current と、それが指す作業用のディレクトリ 1 つだけ。負けた 2 つの
  # 作業用のディレクトリも、current を置くための一時のファイル（current.tmp-*）も残らない。
  file(GLOB entries LIST_DIRECTORIES true RELATIVE "${WORK}/.mosac-cache" "${WORK}/.mosac-cache/*")
  if(NOT entries MATCHES "^[0-9a-f]+$")
    fail("round ${round}: .mosac-cache must hold exactly one entry, found [${entries}]")
  endif()
  file(GLOB inside LIST_DIRECTORIES true RELATIVE "${WORK}/.mosac-cache/${entries}" "${WORK}/.mosac-cache/${entries}/*")
  list(SORT inside)
  file(READ "${WORK}/.mosac-cache/${entries}/current" current)
  if(NOT inside MATCHES "^current;w-[0-9a-f]+$" OR NOT inside STREQUAL "current;${current}")
    fail("round ${round}: the entry must hold current and the one w-* it names, found [${inside}] current=[${current}]")
  endif()
endforeach()

# exe が動いて「こんにちは」を出すこと。
function(expect_runs exe what)
  execute_process(COMMAND "${exe}" WORKING_DIRECTORY "${WORK}" OUTPUT_VARIABLE out ERROR_VARIABLE err
                  RESULT_VARIABLE code TIMEOUT 120 ENCODING UTF-8)
  string(REPLACE "\r\n" "\n" out "${out}")
  if(NOT code EQUAL 0 OR NOT out STREQUAL "こんにちは\n")
    fail("${what}")
  endif()
endfunction()
macro(build_to out_name)
  execute_process(COMMAND ${build} -o "${out_name}" WORKING_DIRECTORY "${WORK}" RESULT_VARIABLE code
                  OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 600 ENCODING UTF-8)
endmacro()

# 名前に .tmp-* を足すと名前の上限（255 バイト）を超える -o も、前と同じく上書きで書ける。Windows はパス全体の長さで
# 別に止まりうるので見ない。
if(NOT CMAKE_HOST_WIN32)
  string(REPEAT "x" 250 long_name)
  build_to("${long_name}")
  if(NOT code EQUAL 0)
    fail("a 250-byte -o must be written")
  endif()
  expect_runs("${WORK}/${long_name}" "the 250-byte -o must run")
endif()

# 記号リンクと、ファイルでない -o。Windows は記号リンクを作るのに権限が要り、FIFO が無いので見ない。
if(NOT CMAKE_HOST_WIN32)
  # -o が記号リンクなら、前（上書きの写しがリンクを辿った）と同じく、リンクは残してその先を新しい実行ファイルに替える。
  file(WRITE "${WORK}/target" "not an executable\n")
  file(CREATE_LINK "target" "${WORK}/link" SYMBOLIC)
  build_to(link)
  if(NOT code EQUAL 0 OR NOT IS_SYMLINK "${WORK}/link")
    fail("a symlinked -o must stay a link")
  endif()
  expect_runs("${WORK}/target" "the link's target must be the new executable")
  # 先の無いリンクは、リンクの中身のパス（相対ならリンクのディレクトリから）に書く。
  file(CREATE_LINK "made" "${WORK}/dangling" SYMBOLIC)
  build_to(dangling)
  if(NOT code EQUAL 0 OR NOT IS_SYMLINK "${WORK}/dangling")
    fail("a dangling symlinked -o must stay a link")
  endif()
  expect_runs("${WORK}/made" "the dangling link's target must be made")
  # FIFO の -o は置き換えず、前と同じく書けないとして止まる。
  execute_process(COMMAND mkfifo "${WORK}/fifo" RESULT_VARIABLE made_fifo)
  if(made_fifo EQUAL 0)
    build_to(fifo)
    execute_process(COMMAND sh -c "test -p fifo" WORKING_DIRECTORY "${WORK}" RESULT_VARIABLE still_fifo)
    if(code EQUAL 0 OR NOT still_fifo EQUAL 0)
      fail("a FIFO -o must be refused and kept")
    endif()
  endif()
endif()
# -o の隣に一時のファイル（<out>.tmp-*）が残らない。
file(GLOB temps "${WORK}/*.tmp-*")
if(temps)
  fail("no temporary file may be left next to -o, found [${temps}]")
endif()
