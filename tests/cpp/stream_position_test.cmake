# レビュー M-33（8.11.9.3）。ありえない位置への set_stream_position/2 は、文字の数を越えたところで、流れを読み足さずに
# domain_error(stream_position, P) を出す。前は終わりまで読み足してから断り、4 MB のファイルでも 1 MiB のヒープの上限で
# 止まった。着ける位置へは今までどおり移る。
# 使い方: cmake -DMOSA=<mosa> -DWORK=<一時ディレクトリ> -P stream_position_test.cmake

file(MAKE_DIRECTORY "${WORK}")
set(data "${WORK}/stream_position_data.txt")
# 改行を入れない。Windows の改行の書き方によらず、位置 (1,1,2) が b の前になる。
string(REPEAT "ab" 2000000 text)
file(WRITE "${data}" "${text}")

# -g の中に ; を書かない。CMake の引数の ; は並びの区切りになる。
set(goal "open('stream_position_data.txt', read, S, [reposition(true)]), catch(set_stream_position(S, stream_position(1,1,1)), error(E, _), true), write(E), nl, set_stream_position(S, stream_position(1,1,2)), get_char(S, C), write(C), nl, close(S)")
# ENCODING UTF-8: 子は UTF-8 で書く。Windows の既定は CMP0176（3.31）が NEW でなければ AUTO で、コンソールのコードページで読み、932 などでは化ける。
execute_process(COMMAND "${MOSA}" --heap-max 1 -g "${goal}" -t halt WORKING_DIRECTORY "${WORK}"
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE code TIMEOUT 120 ENCODING UTF-8)
# 上限の知らせの文面は言語で変わるので、上限の MiB の数で見る。
if(NOT code EQUAL 0 OR "${out}${err}" MATCHES "1 MiB"
   OR NOT out MATCHES "^domain_error\\(stream_position,stream_position\\(1,1,1\\)\\)\r?\nb\r?\n")
  message(FATAL_ERROR "stream_position: code=${code} out=[${out}] err=[${err}]")
endif()
# レビュー R-L11。1 行目の位置 (C,1,P) は C = P - 1 のときだけありうる。行と行の位置から着けないと分かる位置は、文字の
# 数まで読み足さずに断る。前は 300 万字目まで読み足し、1 MiB のヒープの上限で止まった。
set(goal "open('stream_position_data.txt', read, S, [reposition(true)]), catch(set_stream_position(S, stream_position(3000000,1,1)), error(E, _), true), write(E), nl, close(S)")
execute_process(COMMAND "${MOSA}" --heap-max 1 -g "${goal}" -t halt WORKING_DIRECTORY "${WORK}"
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE code TIMEOUT 120 ENCODING UTF-8)
if(NOT code EQUAL 0 OR "${out}${err}" MATCHES "1 MiB"
   OR NOT out MATCHES "^domain_error\\(stream_position,stream_position\\(3000000,1,1\\)\\)\r?\n")
  message(FATAL_ERROR "stream_position first line: code=${code} out=[${out}] err=[${err}]")
endif()
file(REMOVE "${data}")
