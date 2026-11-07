# リリース 0.1.0 Task 4。mosatest --manual を tests/manual の見本で動かし、終了コードと出力を確かめる。
# PASS_REGULAR_EXPRESSION だけでは終了コードを見ないので、両方を見るためにこの包みを通す。
# LANG は mosatest 自身の文面の言語。子の LANG は版で決まるので、ここで何を選んでも例の出力は変わらないはず。
# MOSA_HEAP_MAX=bogus は子に渡れば mosa が警告を出して例が違うので、mosatest が消していることも確かめる。
# 使い方: cmake -DMOSATEST=<mosatest> [-DMOSA=<mosa>] [-DDIR=<見本>] -DWORK=<作業場所> -DLANG_VALUE=<LANG>
#   -DCODE=<終了コード> -DPATTERN_1=<正規表現> [-DPATTERN_2=… -DPATTERN_4=…] -P manual_fixture_test.cmake
# 正規表現は出力の改行を ~ に置き換えたものに当てる。与えたものはすべて合わなければならない。
# DIR を省くと mosatest --manual だけを動かす（使い方の誤り）。MOSA を省くと既定（mosatest の隣の mosa）を使う。

set(ENV{LANG} "${LANG_VALUE}")
set(ENV{MOSA_HEAP_MAX} "bogus")
set(command "${MOSATEST}" --manual)
if(DIR)
  list(APPEND command "${DIR}" --work "${WORK}")
endif()
if(MOSA)
  list(APPEND command --mosa "${MOSA}")
endif()
# 同じ変数に stdout と stderr を受けると、CMake は一つの管にまとめ、書かれた順のまま読む。
execute_process(COMMAND ${command}
  OUTPUT_VARIABLE merged
  ERROR_VARIABLE merged
  RESULT_VARIABLE code
  TIMEOUT 300)
string(REPLACE "\r" "" merged "${merged}")
if(NOT code STREQUAL CODE)
  message(FATAL_ERROR "mosatest --manual: exit ${code}, expected ${CODE}\n${merged}")
endif()
# 改行は ~ にしてから比べる。改行を含む引数は Windows のコマンド行を通りにくいので、正規表現には ~ で書く。
string(REPLACE "\n" "~" flat "${merged}")
foreach(index RANGE 1 4)
  set(pattern "${PATTERN_${index}}")
  if(NOT pattern STREQUAL "" AND NOT flat MATCHES "${pattern}")
    message(FATAL_ERROR "mosatest --manual: output does not match [${pattern}]\n${merged}")
  endif()
endforeach()
