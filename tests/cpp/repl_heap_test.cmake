# レビュー M-12。REPL は続きの行ごとにためた入力の全体を読み直す。前はそのたびに読んだ項をヒープに残し、構文の誤りの
# 項も残したので、1 MiB のヒープで 200 行のゴールや 3,000 行の誤りが上限の知らせで止まった。今はどちらも知らせを出さず、
# 後の問い合わせの答えを出す。
# 使い方: cmake -DMOSA=<mosa> -DWORK=<一時ディレクトリ> -P repl_heap_test.cmake

file(MAKE_DIRECTORY "${WORK}")
set(input "${WORK}/repl_heap_input.txt")
set(ENV{MOSA_TRANSCRIPT} 1)

function(expect_answer label answer_regex)
  # ENCODING UTF-8: 子は UTF-8 で書く。Windows の既定は CMP0176（3.31）が NEW でなければ AUTO で、コンソールのコードページで読み、932 などでは化ける。
  execute_process(COMMAND "${MOSA}" --heap-max 1
    INPUT_FILE "${input}" OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE code TIMEOUT 300 ENCODING UTF-8)
  # 上限の知らせの文面は言語で変わるので、上限の MiB の数で見る（ja: 上限（1 MiB）、en: limit (1 MiB)）。
  if(NOT code EQUAL 0 OR "${out}${err}" MATCHES "1 MiB" OR NOT out MATCHES "${answer_regex}")
    string(LENGTH "${out}" out_length)
    string(SUBSTRING "${out}" 0 400 out_head)
    message(FATAL_ERROR "repl_heap: ${label}: code=${code} out(${out_length} bytes)=[${out_head}] err=[${err}]")
  endif()
endfunction()

# 200 行に分けた 1 つのゴール。行ごとに前の行もまとめて読み直す。
set(goal "")
foreach(index RANGE 0 199)
  string(APPEND goal "X${index} = f(a,b,c,d,e,f,g,h,i,j),\n")
endforeach()
file(WRITE "${input}" "${goal}true.\nY = ok.\n")
expect_answer("a goal of 200 lines" "X199 = f\\(a,b,c,d,e,f,g,h,i,j\\)\\.[^\n]*\n.*Y = ok\\.")

# 3,000 行の構文の誤り。どの行も 52 引数の項を読んでから誤る。
set(bad "X = f(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t,u,v,w,x,y,z,a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t,u,v,w,x,y,z) )).\n")
string(REPEAT "${bad}" 3000 errors)
file(WRITE "${input}" "${errors}Y = ok.\n")
expect_answer("3000 syntax errors" "Y = ok\\.")
file(REMOVE "${input}")
