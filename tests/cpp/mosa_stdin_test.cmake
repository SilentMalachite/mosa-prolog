# mosa の標準入力。端末でない入力は Prolog の user_input のもので、mosa 自身が先に読み切らない。
# 先に読み切ると read/1 が end_of_file になり、閉じない入力では --version も返らない。
# 閉じない入力は移植性のある形で作れないので、入力が残っていることを read/1 で確かめる。
# 使い方: cmake -DMOSA=<mosa> -DWORK=<一時ディレクトリ> -P mosa_stdin_test.cmake

file(MAKE_DIRECTORY "${WORK}")
set(input "${WORK}/mosa_stdin_input.txt")
file(WRITE "${input}" "foo.\nbar.\n")

function(expect_output label regex)
  execute_process(COMMAND "${MOSA}" ${ARGN}
    INPUT_FILE "${input}"
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err
    RESULT_VARIABLE code
    TIMEOUT 20)
  if(NOT code EQUAL 0 OR NOT out MATCHES "${regex}")
    message(FATAL_ERROR "${label}: code=${code} out=[${out}] err=[${err}]")
  endif()
endfunction()

expect_output("read from stdin" "X = foo, Y = bar\\." -g "read(X), read(Y)." -t halt)
expect_output("version ignores stdin" "^mosa 0\\.0\\.1 \\(phase 6\\)" --version)
# レビュー M12。パイプの read/1 は 1.5 の小数点を終止符にしない。
file(WRITE "${input}" "X = 1.5.\nfoo(2.25).\n")
# = の項は = の右の被演算子として括弧に入る（レビュー M27）。
expect_output("float from stdin" "T = \\(_[A-Za-z0-9]*=1\\.5\\), U = foo\\(2\\.25\\)\\." -g "read(T), read(U)." -t halt)
# レビュー L10。-g は書いた順に動き、答えの行はそのゴールの write の出力のすぐ後に出る（端末でなくても）。
# 終わりにまとめて書くと a、b の後に答えが並ぶ（敵対的レビュー A1）。
expect_output("goals in order" "^a\r?\ntrue\\.\r?\nX = 1\\.\r?\nb\r?\ntrue\\.\r?\n$"
  -g "write(a), nl" -g "X = 1" -g "write(b), nl" -t halt)
file(REMOVE "${input}")
