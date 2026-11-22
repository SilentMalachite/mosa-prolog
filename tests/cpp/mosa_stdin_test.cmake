# mosa の標準入力。端末でない入力は Prolog の user_input のもので、mosa 自身が先に読み切らない。
# 先に読み切ると read/1 が end_of_file になり、閉じない入力では --version も返らない。
# 閉じない入力は移植性のある形で作れないので、入力が残っていることを read/1 で確かめる。
# 使い方: cmake -DMOSA=<mosa> -DVERSION=<project VERSION> -DWORK=<一時ディレクトリ> -P mosa_stdin_test.cmake

if(NOT DEFINED VERSION OR VERSION STREQUAL "")
  # 空のままだと、どの版の行にも合ってしまう（レビュー L-46）。
  message(FATAL_ERROR "VERSION is empty: pass -DVERSION=\${PROJECT_VERSION}")
endif()
string(REPLACE "." "\\." version_regex "${VERSION}")
file(MAKE_DIRECTORY "${WORK}")
set(input "${WORK}/mosa_stdin_input.txt")
file(WRITE "${input}" "foo.\nbar.\n")

function(expect_output label regex)
  # 上限は止まった mosa を落とすためで速さは見ない。20 秒では Windows の CI が何十秒も止まる間に打ち切られた（run 44442297643、transcript_test.cmake と同じ）。
  # ENCODING UTF-8: 子は UTF-8 で書く。Windows の既定は CMP0176（3.31）が NEW でなければ AUTO で、コンソールのコードページで読み、932 などでは化ける。
  execute_process(COMMAND "${MOSA}" ${ARGN}
    INPUT_FILE "${input}"
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err
    RESULT_VARIABLE code
    TIMEOUT 180 ENCODING UTF-8)
  if(NOT code EQUAL 0 OR NOT out MATCHES "${regex}")
    message(FATAL_ERROR "${label}: code=${code} out=[${out}] err=[${err}]")
  endif()
endfunction()

expect_output("read from stdin" "X = foo, Y = bar\\." -g "read(X), read(Y)." -t halt)
expect_output("version ignores stdin" "^mosa ${version_regex}" --version)
# レビュー M12。パイプの read/1 は 1.5 の小数点を終止符にしない。
file(WRITE "${input}" "X = 1.5.\nfoo(2.25).\n")
# = の項は = の右の被演算子として括弧に入る（レビュー M27）。
expect_output("float from stdin" "T = \\(_[A-Za-z0-9]*=1\\.5\\), U = foo\\(2\\.25\\)\\." -g "read(T), read(U)." -t halt)
# レビュー L10。-g は書いた順に動き、答えの行はそのゴールの write の出力のすぐ後に出る（端末でなくても）。
# 終わりにまとめて書くと a、b の後に答えが並ぶ（敵対的レビュー A1）。
expect_output("goals in order" "^a\r?\ntrue\\.\r?\nX = 1\\.\r?\nb\r?\ntrue\\.\r?\n$"
  -g "write(a), nl" -g "X = 1" -g "write(b), nl" -t halt)
# レビュー M-42。空の装置（Windows の NUL、POSIX の /dev/null）は端末ではない。Windows の _isatty は文字の装置の NUL も
# 端末とするので、前は引数の無い mosa が REPL の ?- を出して 0 で終わった。端末でない入力と同じく使い方を出して 1 で終わる。
if(CMAKE_HOST_WIN32)
  set(null_device "NUL")
else()
  set(null_device "/dev/null")
endif()
execute_process(COMMAND "${MOSA}"
  INPUT_FILE "${null_device}"
  OUTPUT_VARIABLE out
  ERROR_VARIABLE err
  RESULT_VARIABLE code
  TIMEOUT 180 ENCODING UTF-8)
if(NOT code EQUAL 1 OR out MATCHES "\\?- ")
  message(FATAL_ERROR "the null device is not a terminal: code=${code} out=[${out}] err=[${err}]")
endif()
file(REMOVE "${input}")
