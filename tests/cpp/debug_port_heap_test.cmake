# レビュー M-13。デバッガのポートで w と g を打つたびに、ゴールの項をヒープに作って残した。ポートで待つ間は GC が
# 動かないので、500 引数のゴールに w を 400 回打つと 1 MiB のヒープで問い合わせが止まった。今は書いた後に項を捨てる。
# 使い方: cmake -DMOSA=<mosa> -DWORK=<一時ディレクトリ> -P debug_port_heap_test.cmake

file(MAKE_DIRECTORY "${WORK}")
set(program "${WORK}/debug_port_heap.pl")
set(input "${WORK}/debug_port_heap_input.txt")
set(ENV{MOSA_TRANSCRIPT} 1)

set(blanks "_")
set(numbers "0")
foreach(index RANGE 1 499)
  string(APPEND blanks ",_")
  string(APPEND numbers ",${index}")
endforeach()
file(WRITE "${program}" "p(${blanks}).\n")
string(REPEAT "w\n" 400 writes)
string(REPEAT "g\n" 400 goals)
file(WRITE "${input}" "trace, p(${numbers}).\n${writes}${goals}a\nnotrace, Y = ok.\n")

# ENCODING UTF-8: 子は UTF-8 で書く。Windows の既定は CMP0176（3.31）が NEW でなければ AUTO で、コンソールのコードページで読み、932 などでは化ける。
execute_process(COMMAND "${MOSA}" --heap-max 1 "${program}"
  INPUT_FILE "${input}" OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE code TIMEOUT 300 ENCODING UTF-8)
# 上限の知らせの文面は言語で変わるので、上限の MiB の数で見る（ja: 上限（1 MiB）、en: limit (1 MiB)）。
if(NOT code EQUAL 0 OR "${out}${err}" MATCHES "1 MiB" OR NOT out MATCHES "\nY = ok\\.")
  string(LENGTH "${out}" out_length)
  string(SUBSTRING "${out}" 0 300 out_head)
  message(FATAL_ERROR "debug_port_heap: code=${code} out(${out_length} bytes)=[${out_head}] err=[${err}]")
endif()
file(REMOVE "${program}" "${input}")
