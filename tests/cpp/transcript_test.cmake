# リリース 0.1.0 Task 3。MOSA_TRANSCRIPT=1 の mosa は管の標準入力を端末として扱い、読んだ行をそのまま書き返す。
# マニュアルの検査（mosatest --manual）は stdout と stderr を一つにした出力を、端末に映る表示（端末が打った字を映す）と
# 突き合わせるので、その二つが同じになることを確かめる。期待値は macOS の script(1) で端末の表示を記録し、\r と
# 端末が映す ^D を除いたもの。入力の終わりで mosa は何も足さず、出力は最後の促し "?- "（改行なし）で終わる。
# 使い方: cmake -DMOSA=<mosa> -DDATA=<tests/cpp/data/transcript> -DCASE=<名前> -DLANG_VALUE=<LANG> -P transcript_test.cmake

# 同じ変数に stdout と stderr を受けると、CMake は一つの管にまとめ、書かれた順のまま読む。
function(run_mosa transcript out_var code_var)
  set(ENV{MOSA_TRANSCRIPT} "${transcript}")
  set(ENV{LANG} "${LANG_VALUE}")
  # ENCODING UTF-8: 子は UTF-8 で書く。Windows の既定は CMP0176（3.31）が NEW でなければ AUTO で、コンソールのコードページで読み、932 などでは化ける。
  execute_process(COMMAND "${MOSA}"
    INPUT_FILE "${DATA}/${CASE}.in"
    OUTPUT_VARIABLE merged
    ERROR_VARIABLE merged
    RESULT_VARIABLE code
    TIMEOUT 20 ENCODING UTF-8)
  string(REPLACE "\r" "" merged "${merged}")
  set(${out_var} "${merged}" PARENT_SCOPE)
  set(${code_var} "${code}" PARENT_SCOPE)
endfunction()

run_mosa(1 actual code)
file(READ "${DATA}/${CASE}.out" expected)
string(REPLACE "\r" "" expected "${expected}")
if(NOT code EQUAL 0 OR NOT actual STREQUAL expected)
  message(FATAL_ERROR "${CASE}: code=${code}\nexpected=[${expected}]\nactual=[${actual}]")
endif()

# 値がちょうど 1 のときだけ効く。他の値では端末でない入力のままで、ファイルも -g も無いので使い方を出して 1 で終わる。
if(CASE STREQUAL "transcript_query")
  run_mosa(true actual code)
  if(NOT code EQUAL 1 OR NOT actual MATCHES "^mosa: usage: ")
    message(FATAL_ERROR "${CASE} with MOSA_TRANSCRIPT=true: code=${code} actual=[${actual}]")
  endif()
endif()
