# テストの定義（CTest）。本体の CMakeLists.txt が MOSA_BUILD_TESTS のときだけ include する（計画 Task 10）。
# mosac build --runtime-source は同梱の源から libmosa だけを作るので、tests/ の無い場所でも configure できる必要がある。
# include なので変数と相対パスは本体と同じ（CMAKE_CURRENT_SOURCE_DIR はリポジトリの根）。

enable_testing()

add_executable(smoke_test tests/cpp/smoke_test.cpp)
target_include_directories(smoke_test PRIVATE tests/cpp)
target_link_libraries(smoke_test PRIVATE mosa_warnings)

add_test(NAME smoke COMMAND $<TARGET_FILE:smoke_test>)

add_executable(char_class_test tests/cpp/char_class_test.cpp)
target_include_directories(char_class_test PRIVATE tests/cpp)
target_link_libraries(char_class_test PRIVATE mosa_lib)
add_test(NAME char_class COMMAND $<TARGET_FILE:char_class_test>)

add_executable(lexer_test tests/cpp/lexer_test.cpp)
target_include_directories(lexer_test PRIVATE tests/cpp)
target_link_libraries(lexer_test PRIVATE mosa_lib)
add_test(NAME lexer COMMAND $<TARGET_FILE:lexer_test>)

add_executable(operator_test tests/cpp/operator_test.cpp)
target_include_directories(operator_test PRIVATE tests/cpp)
target_link_libraries(operator_test PRIVATE mosa_lib)
add_test(NAME operator_test COMMAND $<TARGET_FILE:operator_test>)

add_executable(cell_test tests/cpp/cell_test.cpp)
target_include_directories(cell_test PRIVATE tests/cpp)
target_link_libraries(cell_test PRIVATE mosa_lib)
add_test(NAME cell COMMAND $<TARGET_FILE:cell_test>)

add_executable(unify_test tests/cpp/unify_test.cpp)
target_include_directories(unify_test PRIVATE tests/cpp)
target_link_libraries(unify_test PRIVATE mosa_lib)
add_test(NAME unify COMMAND $<TARGET_FILE:unify_test>)

add_executable(reader_test tests/cpp/reader_test.cpp)
target_include_directories(reader_test PRIVATE tests/cpp)
target_link_libraries(reader_test PRIVATE mosa_lib)
add_test(NAME reader COMMAND $<TARGET_FILE:reader_test>)

add_executable(writer_test tests/cpp/writer_test.cpp)
target_include_directories(writer_test PRIVATE tests/cpp)
target_link_libraries(writer_test PRIVATE mosa_lib)
add_test(NAME writer COMMAND $<TARGET_FILE:writer_test>)

# 差分テストの答えの字面（計画 Task 11）。深い項を既定のスタックで書けることまで見る。
add_executable(answer_text_test tests/cpp/answer_text_test.cpp)
target_include_directories(answer_text_test PRIVATE tests/cpp src/machine)
target_link_libraries(answer_text_test PRIVATE mosa_lib)
add_test(NAME answer_text COMMAND $<TARGET_FILE:answer_text_test>)

add_test(NAME roundtrip COMMAND $<TARGET_FILE:mosatest> --roundtrip ${CMAKE_SOURCE_DIR}/tests/pl/syntax.pl)
# テストのパスはリポジトリの根から書く。build の場所に左右されない（計画 Task 15）。
add_test(NAME prolog_tests COMMAND $<TARGET_FILE:mosatest> ${CMAKE_SOURCE_DIR}/tests/pl
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(heap_growth_test tests/cpp/heap_growth_test.cpp)
target_include_directories(heap_growth_test PRIVATE tests/cpp)
target_link_libraries(heap_growth_test PRIVATE mosa_lib)
add_test(NAME heap_growth COMMAND $<TARGET_FILE:heap_growth_test>)

# mark-and-compact の GC（ADR 0010）。ヒープ・extras・トレイルの頭打ちと、生きた項が保たれること。
add_executable(gc_test tests/cpp/gc_test.cpp)
target_include_directories(gc_test PRIVATE tests/cpp src)
target_compile_definitions(gc_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(gc_test PRIVATE mosa_lib)
add_test(NAME gc COMMAND $<TARGET_FILE:gc_test>)
# 百万要素のリストや loop を GC 付きで回す。ストレス（MOSA_GC_STRESS）では既定の 1500 秒を超えうる。
set_tests_properties(gc heap_growth PROPERTIES TIMEOUT 1800)

add_executable(session_test tests/cpp/session_test.cpp)
target_include_directories(session_test PRIVATE tests/cpp)
target_compile_definitions(session_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(session_test PRIVATE mosa_lib)
add_test(NAME session COMMAND $<TARGET_FILE:session_test>)
# 今ある日本語の文面の確認を、流す機械の LANG に左右させない（ADR 0007）。
set_tests_properties(session PROPERTIES ENVIRONMENT "LANG=ja_JP.UTF-8")
# MOSA_LIB は実行ファイルの lib/ より先に探す。鍵の無い messages_en.pl で、代わりの表示を確かめる。
file(WRITE ${CMAKE_SOURCE_DIR}/tests/tmp/messages_empty/messages_en.pl ":- module(messages_en, []).\n")
add_test(NAME session_messages_missing COMMAND $<TARGET_FILE:session_test> missing)
set_tests_properties(session_messages_missing PROPERTIES
  ENVIRONMENT "MOSA_LIB=${CMAKE_SOURCE_DIR}/tests/tmp/messages_empty")
add_test(NAME session_english COMMAND $<TARGET_FILE:session_test> english)
set_tests_properties(session_english PROPERTIES ENVIRONMENT "LANG=C")

add_executable(stream_test tests/cpp/stream_test.cpp)
target_include_directories(stream_test PRIVATE tests/cpp)
target_compile_definitions(stream_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(stream_test PRIVATE mosa_lib)
add_test(NAME stream_test COMMAND $<TARGET_FILE:stream_test>)

add_executable(machine_test tests/cpp/machine_test.cpp)
target_include_directories(machine_test PRIVATE tests/cpp)
target_link_libraries(machine_test PRIVATE mosa_lib)
add_test(NAME machine COMMAND $<TARGET_FILE:machine_test>)
add_executable(address_range_test tests/cpp/address_range_test.cpp)
target_include_directories(address_range_test PRIVATE tests/cpp)
target_link_libraries(address_range_test PRIVATE mosa_lib)
add_test(NAME address_range COMMAND $<TARGET_FILE:address_range_test>)
add_executable(evaluable_test tests/cpp/evaluable_test.cpp)
target_include_directories(evaluable_test PRIVATE tests/cpp src)
target_link_libraries(evaluable_test PRIVATE mosa_lib)
add_test(NAME evaluable COMMAND $<TARGET_FILE:evaluable_test>)
add_executable(module_load_test tests/cpp/module_load_test.cpp)
target_include_directories(module_load_test PRIVATE tests/cpp)
target_link_libraries(module_load_test PRIVATE mosa_lib)
target_compile_definitions(module_load_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
add_test(NAME module_load COMMAND $<TARGET_FILE:module_load_test>)
add_executable(library_path_test tests/cpp/library_path_test.cpp)
target_include_directories(library_path_test PRIVATE tests/cpp)
target_link_libraries(library_path_test PRIVATE mosa_lib)
target_compile_definitions(library_path_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
# 同じテストを MOSA_LIB の無い環境と有る環境で流す。テスト側に環境変数を書き換える OS 分岐を置かない。
add_test(NAME library_path COMMAND $<TARGET_FILE:library_path_test>)
set_tests_properties(library_path PROPERTIES ENVIRONMENT_MODIFICATION "MOSA_LIB=unset:")
add_test(NAME library_path_env COMMAND $<TARGET_FILE:library_path_test> env)
set_tests_properties(library_path_env PROPERTIES
  ENVIRONMENT "MOSA_LIB=${CMAKE_SOURCE_DIR}/tests/tmp/library_path/lib_env")
# 探索は偽の PATH（tests/tmp/toolchain）を引数で渡して引く。子プロセスは起こさない。
add_executable(toolchain_test tests/cpp/toolchain_test.cpp)
target_include_directories(toolchain_test PRIVATE tests/cpp)
target_link_libraries(toolchain_test PRIVATE mosa_lib)
target_compile_definitions(toolchain_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
add_test(NAME toolchain COMMAND $<TARGET_FILE:toolchain_test>)

add_executable(include_test tests/cpp/include_test.cpp)
target_include_directories(include_test PRIVATE tests/cpp)
target_compile_definitions(include_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(include_test PRIVATE mosa_lib)
add_test(NAME include_test COMMAND $<TARGET_FILE:include_test>)

add_executable(ensure_test tests/cpp/ensure_test.cpp)
target_include_directories(ensure_test PRIVATE tests/cpp)
target_compile_definitions(ensure_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(ensure_test PRIVATE mosa_lib)
add_test(NAME ensure_test COMMAND $<TARGET_FILE:ensure_test>)

add_executable(suite_assert_test tests/cpp/suite_assert_test.cpp)
target_include_directories(suite_assert_test PRIVATE tests/cpp)
target_compile_definitions(suite_assert_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(suite_assert_test PRIVATE mosa_lib)
add_test(NAME suite_assert_test COMMAND $<TARGET_FILE:suite_assert_test>)

add_executable(suite_if_test tests/cpp/suite_if_test.cpp)
target_include_directories(suite_if_test PRIVATE tests/cpp)
target_compile_definitions(suite_if_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(suite_if_test PRIVATE mosa_lib)
add_test(NAME suite_if_test COMMAND $<TARGET_FILE:suite_if_test>)

add_executable(suite_imports_test tests/cpp/suite_imports_test.cpp)
target_include_directories(suite_imports_test PRIVATE tests/cpp)
target_compile_definitions(suite_imports_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(suite_imports_test PRIVATE mosa_lib)
add_test(NAME suite_imports_test COMMAND $<TARGET_FILE:suite_imports_test>)

add_executable(suite_approx_test tests/cpp/suite_approx_test.cpp)
target_include_directories(suite_approx_test PRIVATE tests/cpp)
target_compile_definitions(suite_approx_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(suite_approx_test PRIVATE mosa_lib)
add_test(NAME suite_approx_test COMMAND $<TARGET_FILE:suite_approx_test>)

add_executable(suite_path_test tests/cpp/suite_path_test.cpp)
target_include_directories(suite_path_test PRIVATE tests/cpp)
target_compile_definitions(suite_path_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(suite_path_test PRIVATE mosa_lib)
add_test(NAME suite_path_test COMMAND $<TARGET_FILE:suite_path_test>)
set_tests_properties(suite_path_test PROPERTIES RESOURCE_LOCK suite_scratch)

add_executable(suite_vars_test tests/cpp/suite_vars_test.cpp)
target_include_directories(suite_vars_test PRIVATE tests/cpp)
target_compile_definitions(suite_vars_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(suite_vars_test PRIVATE mosa_lib)
add_test(NAME suite_vars_test COMMAND $<TARGET_FILE:suite_vars_test>)

add_executable(suite_encoding_test tests/cpp/suite_encoding_test.cpp)
target_include_directories(suite_encoding_test PRIVATE tests/cpp)
target_compile_definitions(suite_encoding_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(suite_encoding_test PRIVATE mosa_lib)
add_test(NAME suite_encoding_test COMMAND $<TARGET_FILE:suite_encoding_test>)

add_executable(files_test tests/cpp/files_test.cpp)
target_include_directories(files_test PRIVATE tests/cpp)
target_compile_definitions(files_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(files_test PRIVATE mosa_lib)
add_test(NAME files_test COMMAND $<TARGET_FILE:files_test>)

add_executable(conformance_report_test tests/cpp/conformance_report_test.cpp)
target_include_directories(conformance_report_test PRIVATE tests/cpp src/machine)
target_compile_definitions(conformance_report_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(conformance_report_test PRIVATE mosa_lib)
add_test(NAME conformance_report_test COMMAND $<TARGET_FILE:conformance_report_test>)

add_executable(conformance_io_test tests/cpp/conformance_io_test.cpp)
target_include_directories(conformance_io_test PRIVATE tests/cpp)
target_compile_definitions(conformance_io_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(conformance_io_test PRIVATE mosa_lib)
# tests/tmp/suite は tests.lgt を流す全てが共有する。空かどうかを見るこの 2 本と diff_conformance は同時に走らせない。
add_test(NAME conformance_io_test COMMAND $<TARGET_FILE:conformance_io_test>)
set_tests_properties(conformance_io_test PROPERTIES RESOURCE_LOCK suite_scratch)

add_executable(conformance_output_test tests/cpp/conformance_output_test.cpp)
target_include_directories(conformance_output_test PRIVATE tests/cpp)
target_compile_definitions(conformance_output_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(conformance_output_test PRIVATE mosa_lib)
add_test(NAME conformance_output_test COMMAND $<TARGET_FILE:conformance_output_test>)

add_executable(conformance_badge_test tests/cpp/conformance_badge_test.cpp)
target_include_directories(conformance_badge_test PRIVATE tests/cpp)
target_compile_definitions(conformance_badge_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(conformance_badge_test PRIVATE mosa_lib)
add_test(NAME conformance_badge_test COMMAND $<TARGET_FILE:conformance_badge_test>)

add_executable(conformance_cwd_test tests/cpp/conformance_cwd_test.cpp)
target_include_directories(conformance_cwd_test PRIVATE tests/cpp)
target_compile_definitions(conformance_cwd_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(conformance_cwd_test PRIVATE mosa_lib)
add_test(NAME conformance_cwd_test COMMAND $<TARGET_FILE:conformance_cwd_test>)

add_executable(suite_args_test tests/cpp/suite_args_test.cpp)
target_include_directories(suite_args_test PRIVATE tests/cpp)
target_compile_definitions(suite_args_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(suite_args_test PRIVATE mosa_lib)
add_test(NAME suite_args_test COMMAND $<TARGET_FILE:suite_args_test>)

add_executable(bench_report_test tests/cpp/bench_report_test.cpp)
target_include_directories(bench_report_test PRIVATE tests/cpp src/machine)
target_compile_definitions(bench_report_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(bench_report_test PRIVATE mosa_lib)
add_test(NAME bench_report_test COMMAND $<TARGET_FILE:bench_report_test>)

# \r と \n は CMake のエスケープで CR と LF にする。\\r のままだと文字 r になり、1 行の版表示に一致しない。
set(version_pass_regex "^mosa 0\\.0\\.1 \\(phase 6\\)\r?\n?$")
add_test(NAME mosa_version COMMAND $<TARGET_FILE:mosa> --version)
add_test(NAME mosac_version COMMAND $<TARGET_FILE:mosac> --version)
add_test(NAME mosatest_version COMMAND $<TARGET_FILE:mosatest> --version)
set_tests_properties(mosa_version mosac_version mosatest_version PROPERTIES
  PASS_REGULAR_EXPRESSION "${version_pass_regex}")

add_executable(console_test tests/cpp/console_test.cpp)
target_include_directories(console_test PRIVATE tests/cpp)
target_link_libraries(console_test PRIVATE mosa_lib)
add_test(NAME console COMMAND $<TARGET_FILE:console_test> plain --flag 42)
add_test(NAME mosa_utf8_pipe COMMAND ${CMAKE_COMMAND} -DMOSA=$<TARGET_FILE:mosa>
  -DWORK=${CMAKE_SOURCE_DIR}/tests/tmp -P ${CMAKE_SOURCE_DIR}/tests/cpp/mosa_utf8_pipe_test.cmake)

add_test(NAME mosa_japanese_arg COMMAND ${CMAKE_COMMAND} -DMOSA=$<TARGET_FILE:mosa>
  -DWORK=${CMAKE_SOURCE_DIR}/tests/tmp -P ${CMAKE_SOURCE_DIR}/tests/cpp/mosa_japanese_arg_test.cmake)

add_test(NAME mosa_stdin COMMAND ${CMAKE_COMMAND} -DMOSA=$<TARGET_FILE:mosa>
  -DWORK=${CMAKE_SOURCE_DIR}/tests/tmp -P ${CMAKE_SOURCE_DIR}/tests/cpp/mosa_stdin_test.cmake)

# 手書きの生成関数は runtime.hpp だけを見る。internal.hpp を見るのは指紋を出す側の native_test.cpp だけ。
add_executable(native_test tests/cpp/native_test.cpp tests/cpp/native_units.cpp)
target_include_directories(native_test PRIVATE tests/cpp src)
target_compile_definitions(native_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(native_test PRIVATE mosa_lib)
add_test(NAME native COMMAND $<TARGET_FILE:native_test>)

add_executable(codegen_test tests/cpp/codegen_test.cpp)
target_include_directories(codegen_test PRIVATE tests/cpp src)
target_compile_definitions(codegen_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(codegen_test PRIVATE mosa_lib)
add_test(NAME codegen COMMAND $<TARGET_FILE:codegen_test>)

# mosac emit の目録を本体と同じ生成器、コンパイラ、構成で作る。libmosa は作り直さず、このビルド木のものを使う。
foreach(emit_case hello broken undefined undefined_quoted duplicate silent newop halt)
  add_test(NAME mosac_emit_${emit_case} COMMAND ${CMAKE_COMMAND}
    -DMOSAC=$<TARGET_FILE:mosac> -DCASE=${emit_case} -DSRC=${CMAKE_SOURCE_DIR}/tests/mosac
    -DWORK=${CMAKE_SOURCE_DIR}/tests/tmp/mosac_${emit_case} "-DGENERATOR=${CMAKE_GENERATOR}"
    "-DPLATFORM=${CMAKE_GENERATOR_PLATFORM}" "-DCXX=${CMAKE_CXX_COMPILER}" "-DBUILD_TYPE=${CMAKE_BUILD_TYPE}"
    "-DCONFIG=$<CONFIG>" "-DCXX_FLAGS=${CMAKE_CXX_FLAGS}" "-DLINKER_FLAGS=${CMAKE_EXE_LINKER_FLAGS}"
    "-DEXE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}" -DMOSA_LIB=${CMAKE_SOURCE_DIR}/lib
    -P ${CMAKE_SOURCE_DIR}/tests/mosac/emit_test.cmake)
  # このリポジトリのテストでは、生成コードが警告なしで通ることも確かめる（MOSA_WARNINGS_AS_ERRORS）。
  set_tests_properties(mosac_emit_${emit_case} PROPERTIES ENVIRONMENT "MOSA_WERROR=1")
endforeach()

# mosac の使い方の誤り、--help、dist、原文を上書きする build の拒否（コンパイラを起こさない）。
add_test(NAME mosac_cli COMMAND ${CMAKE_COMMAND}
  -DMOSAC=$<TARGET_FILE:mosac> -DSRC=${CMAKE_SOURCE_DIR}/tests/mosac -DWORK=${CMAKE_SOURCE_DIR}/tests/tmp/mosac_cli
  "-DEXE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}" -DMOSA_LIB=${CMAKE_SOURCE_DIR}/lib
  -P ${CMAKE_SOURCE_DIR}/tests/mosac/cli_test.cmake)

# mosac build と run（計画 Task 9）。同じ確かめを CMake の道と、MOSA_NO_CMAKE=1 で固めた直接の道（_direct）で流す。
# 作業場所は tests/tmp の下にし、.mosac-cache をリポジトリの根に作らない。偽のコンパイラはここで作る（作り置きを入れない）。
# Windows の既定は vswhere で探す。名指しの cl は vcvarsall を通らない（Task 8 の持ち越し）ので、そこでは渡さない。
set(mosac_build_cxx "")
if(NOT MSVC)
  set(mosac_build_cxx "${CMAKE_CXX_COMPILER}")
endif()
# mosac_fake_cxx はいつも失敗し、mosac_fake_cxx_forward は生成コード以外を本物のコンパイラに渡す（fake_cxx.cpp）。
# 本物のコンパイラのパスはマクロでなく、configure_file で作る constexpr の見出し（目標ごとに別のディレクトリ）で渡す。
foreach(fake mosac_fake_cxx mosac_fake_cxx_forward)
  set(fake_real_cxx "")
  if(fake STREQUAL "mosac_fake_cxx_forward")
    set(fake_real_cxx "${mosac_build_cxx}")
  endif()
  configure_file(tests/mosac/fake_cxx_config.hpp.in ${CMAKE_BINARY_DIR}/generated/${fake}/fake_cxx_config.hpp @ONLY)
  add_executable(${fake} tests/mosac/fake_cxx.cpp)
  target_include_directories(${fake} PRIVATE ${CMAKE_BINARY_DIR}/generated/${fake})
  target_link_libraries(${fake} PRIVATE mosa_warnings)
  # Windows では偽物も cl.exe の名にする。mosac は cl 以外の名を MSVC のビルド木と族が違う（libmosa.a が無い）として
  # 止めるので、偽物が生成コードのコンパイルまで届かない（CI run 40892878978）。名前がぶつからないよう目標ごとの
  # ディレクトリに置く。Windows では mosac_build_cxx が空なので、転送する版もいつも失敗する版と同じに振る舞う。
  if(WIN32)
    set_target_properties(${fake} PROPERTIES OUTPUT_NAME cl RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${fake})
  endif()
endforeach()
get_filename_component(mosac_cmake_dir "${CMAKE_COMMAND}" DIRECTORY)
foreach(build_case build_hello run_exit_code run_args cache_reuse internal_error debug_trace)
  foreach(path_kind cmake direct)
    set(build_test mosac_${build_case})
    if(path_kind STREQUAL "direct")
      set(build_test mosac_${build_case}_direct)
    endif()
    add_test(NAME ${build_test} COMMAND ${CMAKE_COMMAND}
      -DMOSAC=$<TARGET_FILE:mosac> -DCASE=${build_case} -DSRC=${CMAKE_SOURCE_DIR}/tests/mosac
      -DWORK=${CMAKE_SOURCE_DIR}/tests/tmp/${build_test} "-DCXX=${mosac_build_cxx}"
      -DFAKE_CXX=$<TARGET_FILE:mosac_fake_cxx_forward> -DFAILING_CXX=$<TARGET_FILE:mosac_fake_cxx>
      "-DEXE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
      -DMOSA_LIB=${CMAKE_SOURCE_DIR}/lib -DPATH_KIND=${path_kind}
      -P ${CMAKE_SOURCE_DIR}/tests/mosac/build_test.cmake)
    if(path_kind STREQUAL "direct")
      set_tests_properties(${build_test} PROPERTIES ENVIRONMENT "MOSA_NO_CMAKE=1")
    else()
      # mosac は PATH から cmake を探す。このビルドの cmake を先に置き、CMake の道を必ず通す。
      set_tests_properties(${build_test} PROPERTIES ENVIRONMENT "MOSA_NO_CMAKE=0"
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${mosac_cmake_dir}")
    endif()
  endforeach()
endforeach()

# mosatest --bench の出力の形だけを確かめる（計画 Task 6）。N が小さいので時間は見ない。翻訳側の configure は
# 本体と同じ生成器、コンパイラ、旗で行う（emit_test.cmake の -G と -A に相当）。CMake は最初の configure で
# 環境変数 CMAKE_GENERATOR、CMAKE_GENERATOR_PLATFORM、CXX、CXXFLAGS、LDFLAGS を読む。
add_test(NAME mosatest_bench COMMAND $<TARGET_FILE:mosatest> --bench bench/nrev.pl --count 10
  --work ${CMAKE_SOURCE_DIR}/tests/tmp/bench --cmake ${CMAKE_COMMAND}
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set(mosatest_bench_env "CXX=${CMAKE_CXX_COMPILER}" "CXXFLAGS=${CMAKE_CXX_FLAGS}" "LDFLAGS=${CMAKE_EXE_LINKER_FLAGS}"
  "CMAKE_GENERATOR=${CMAKE_GENERATOR}" "MOSA_WERROR=1")
if(CMAKE_GENERATOR_PLATFORM)
  list(APPEND mosatest_bench_env "CMAKE_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}")
endif()
# PASS_REGULAR_EXPRESSION は終了コードを見ない。比の行は両側が成功したときだけ出るので、形が合えば成功である。
set_tests_properties(mosatest_bench PROPERTIES
  ENVIRONMENT "${mosatest_bench_env}"
  PASS_REGULAR_EXPRESSION ": [0-9]+ ms\r?\n[^\n]*: [0-9]+ ms\r?\n[^\n]*: [0-9]+\\.[0-9]\r?\n$")

# 単独の実行ファイル（計画 Task 10、ADR 0009 Review Focus 5）。build した物を lib/ も原文も無いディレクトリで、
# MOSA_LIB を外して動かす。翻訳のとき mosac は自分の隣の lib/ を読むので、build にも MOSA_LIB は要らない
# （plain_use だけは台本が利用者のライブラリのディレクトリを MOSA_LIB で渡す）。
foreach(standalone_case uncaught_message use_lists include plain_use subdir)
  add_test(NAME standalone_${standalone_case} COMMAND ${CMAKE_COMMAND}
    -DMOSAC=$<TARGET_FILE:mosac> -DCASE=${standalone_case} -DSRC=${CMAKE_SOURCE_DIR}/tests/mosac
    -DWORK=${CMAKE_SOURCE_DIR}/tests/tmp/standalone_${standalone_case} "-DCXX=${mosac_build_cxx}"
    "-DEXE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
    -P ${CMAKE_SOURCE_DIR}/tests/mosac/standalone_test.cmake)
  set_tests_properties(standalone_${standalone_case} PROPERTIES ENVIRONMENT "MOSA_WERROR=1"
    ENVIRONMENT_MODIFICATION "MOSA_LIB=unset:;PATH=path_list_prepend:${mosac_cmake_dir}")
endforeach()

# 2 万要素のリストの原文を mosa と mosac run の両方で動かし、出力を比べる（翻訳器の再帰の深さ）。
add_test(NAME mosac_long_list COMMAND ${CMAKE_COMMAND}
  -DMOSA=$<TARGET_FILE:mosa> -DMOSAC=$<TARGET_FILE:mosac> -DWORK=${CMAKE_SOURCE_DIR}/tests/tmp/mosac_long_list
  "-DCXX=${mosac_build_cxx}" -DMOSA_LIB=${CMAKE_SOURCE_DIR}/lib
  -P ${CMAKE_SOURCE_DIR}/tests/mosac/long_list_test.cmake)
set_tests_properties(mosac_long_list PROPERTIES TIMEOUT 1800
  ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${mosac_cmake_dir}")

# ヒープの上限（--heap-max / MOSA_HEAP_MAX）で止まる。mosac run は終了コード 2 と上限の MiB を出す。
# REPL は repl_ref（端末でなくても REPL にする）で、止まったあとの次の問い合わせに答える。
add_test(NAME mosac_heap_limit COMMAND ${CMAKE_COMMAND}
  -DMOSA=$<TARGET_FILE:mosa> -DMOSAC=$<TARGET_FILE:mosac> -DREF=$<TARGET_FILE:repl_ref>
  -DSRC=${CMAKE_SOURCE_DIR}/tests/mosac -DWORK=${CMAKE_SOURCE_DIR}/tests/tmp/mosac_heap_limit
  "-DCXX=${mosac_build_cxx}" -DMOSA_LIB=${CMAKE_SOURCE_DIR}/lib "-DEXE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
  -P ${CMAKE_SOURCE_DIR}/tests/mosac/heap_limit_test.cmake)
set_tests_properties(mosac_heap_limit PROPERTIES TIMEOUT 1800
  ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${mosac_cmake_dir}")

# cmake --install を一時の prefix に行い、SPEC 第6節の形と、インストールした mosac での build を確かめる。
# runtime の部品は Release だけ入れるので、ほかの構成では台本が「skipped: 」を出し、CTest はそれを skip と数える。
add_test(NAME mosac_installed_layout COMMAND ${CMAKE_COMMAND}
  -DBUILD_DIR=${CMAKE_BINARY_DIR} "-DCONFIG=$<CONFIG>" "-DABI=${mosa_abi}" -DSRC=${CMAKE_SOURCE_DIR}/tests/mosac
  -DWORK=${CMAKE_SOURCE_DIR}/tests/tmp/mosac_installed_layout "-DCXX=${mosac_build_cxx}"
  "-DEXE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}" "-DLIB_PREFIX=${CMAKE_STATIC_LIBRARY_PREFIX}"
  "-DLIB_SUFFIX=${CMAKE_STATIC_LIBRARY_SUFFIX}"
  -P ${CMAKE_SOURCE_DIR}/tests/mosac/installed_layout_test.cmake)
set_tests_properties(mosac_installed_layout PROPERTIES ENVIRONMENT "MOSA_WERROR=1"
  SKIP_REGULAR_EXPRESSION "skipped: the runtime component is installed only for Release"
  ENVIRONMENT_MODIFICATION "MOSA_LIB=unset:;PATH=path_list_prepend:${mosac_cmake_dir}")

# mosac build --runtime-source は同梱の源から libmosa を 2 回作る（M1 Max で 35 秒、CI の Linux ではもっと長い）。
# ふだんの ctest には重いので、MOSA_SLOW_TESTS=ON のときだけ登録する（ラベル slow）。2 回目がキャッシュの物を使い、
# 3 回目の --clean が作り直した物で結べることまで見る。
option(MOSA_SLOW_TESTS "Also register the slow tests (a libmosa build for mosac build --runtime-source)" OFF)
if(MOSA_SLOW_TESTS)
  add_test(NAME mosac_runtime_source_cache COMMAND ${CMAKE_COMMAND}
    -DMOSAC=$<TARGET_FILE:mosac> -DSRC=${CMAKE_SOURCE_DIR}/tests/mosac
    -DWORK=${CMAKE_SOURCE_DIR}/tests/tmp/mosac_runtime_source_cache "-DCXX=${mosac_build_cxx}"
    "-DEXE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
    -P ${CMAKE_SOURCE_DIR}/tests/mosac/runtime_source_test.cmake)
  set_tests_properties(mosac_runtime_source_cache PROPERTIES ENVIRONMENT "MOSA_WERROR=1" LABELS slow TIMEOUT 1800
    ENVIRONMENT_MODIFICATION "MOSA_LIB=unset:;PATH=path_list_prepend:${mosac_cmake_dir}")
endif()

# 差分テスト（計画 Task 11、持ち主の決定 1 の B）。ディレクトリごとに 1 つの実行ファイルを mosac emit --units と
# CMake で作り、インタプリタと翻訳の答え、エラー項、標準出力を 1 バイトずつ比べる。環境は mosatest_bench と同じ
# （翻訳側の configure を本体と同じ生成器、コンパイラ、旗で行う）。報告 docs/agreement.{json,md} は 2 つのテストが
# 同じファイルに書くので、RESOURCE_LOCK で同時に走らせない。japanese.pl と native.pl が含まれることと、native.pl に
# 生成関数が 1 つ以上付いたこと（ATTACHED）も確かめる。
# bench/ の 6 本は tests/pl/bench_<名前>.pl が include し、最初の解の確かめと短い bench(N) を両方の側で比べる
# （計画 Task 13）。どれも比べた一覧に出て、生成関数が付いたこと。
set(diff_bench_files bench_nrev.pl bench_queens.pl bench_zebra.pl bench_tak.pl bench_deriv.pl bench_crypt.pl)
add_test(NAME diff_tests_pl COMMAND ${CMAKE_COMMAND}
  -DMOSATEST=$<TARGET_FILE:mosatest> -DDIR=tests/pl -DWORK=${CMAKE_SOURCE_DIR}/tests/tmp/diff_tests_pl
  "-DCMAKE_PATH=${CMAKE_COMMAND}" "-DCONFIG=$<IF:$<BOOL:$<CONFIG>>,$<CONFIG>,Release>"
  "-DREQUIRE=japanese.pl;native.pl;${diff_bench_files}" "-DATTACHED=native.pl;${diff_bench_files}" -DROOT=${CMAKE_SOURCE_DIR}
  -P ${CMAKE_SOURCE_DIR}/tests/mosac/diff_test.cmake)
set_tests_properties(diff_tests_pl PROPERTIES ENVIRONMENT "${mosatest_bench_env}" LABELS diff RESOURCE_LOCK agreement
  TIMEOUT 3000)
# 同梱スイート（tests/conformance）の tests.lgt も同じ形で比べる。1 ファイル 1 単位なので翻訳側のビルドが長く、
# ラベル diff で外せる（ctest -LE diff）。
add_test(NAME diff_conformance COMMAND ${CMAKE_COMMAND}
  -DMOSATEST=$<TARGET_FILE:mosatest> -DDIR=tests/conformance -DWORK=${CMAKE_SOURCE_DIR}/tests/tmp/diff_conformance
  "-DCMAKE_PATH=${CMAKE_COMMAND}" "-DCONFIG=$<IF:$<BOOL:$<CONFIG>>,$<CONFIG>,Release>"
  "-DREQUIRE=" "-DATTACHED=" -DROOT=${CMAKE_SOURCE_DIR}
  -P ${CMAKE_SOURCE_DIR}/tests/mosac/diff_test.cmake)
set_tests_properties(diff_conformance PROPERTIES ENVIRONMENT "${mosatest_bench_env}" LABELS diff RESOURCE_LOCK "agreement;suite_scratch"
  TIMEOUT 3000)

# 日本語の文面を確かめるテストを、流す機械の LANG に左右させない（ADR 0007）。英語の既定は session_english が見る。
get_property(mosa_all_tests DIRECTORY PROPERTY TESTS)
list(REMOVE_ITEM mosa_all_tests session_english)
set_property(TEST ${mosa_all_tests} APPEND PROPERTY ENVIRONMENT_MODIFICATION "LANG=set:ja_JP.UTF-8")

# mosac build --interactive（計画 Task 14）。翻訳した実行ファイルの REPL が、mosa と同じ run_session の出力と一致するか。
# repl_ref は main_mosa.cpp と同じ呼び出しで、標準入力が端末でなくても REPL にする（試験の基準）。
add_executable(repl_ref tests/mosac/repl_ref.cpp)
target_link_libraries(repl_ref PRIVATE mosa_lib)
add_test(NAME mosac_interactive COMMAND ${CMAKE_COMMAND}
  -DMOSAC=$<TARGET_FILE:mosac> -DREF=$<TARGET_FILE:repl_ref> -DSRC=${CMAKE_SOURCE_DIR}/tests/mosac
  -DWORK=${CMAKE_SOURCE_DIR}/tests/tmp/mosac_interactive "-DCXX=${mosac_build_cxx}"
  "-DEXE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
  -P ${CMAKE_SOURCE_DIR}/tests/mosac/interactive_test.cmake)
set_tests_properties(mosac_interactive PROPERTIES ENVIRONMENT "MOSA_WERROR=1"
  ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${mosac_cmake_dir};LANG=set:ja_JP.UTF-8")
