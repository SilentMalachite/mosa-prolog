# mosac と生成コードのテスト（生成関数、emit、build と run、dist、bench、単独の実行ファイル、深い項、ヒープの上限、
# install とパッケージ、差分テスト）。tests/tests.cmake が 800 行を超えないよう分けた（レビュー 2026-11-20 の計画）。
# include なので変数と相対パスは tests/tests.cmake と同じ。後ろの mosac_interactive と mosa_std_binary が
# mosac_build_cxx と mosac_cmake_dir を使い、LANG の一括の設定がここで登録したテストにも効くよう、その前で読む。

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

# 目録と翻訳の読み込みの書き留め。codegen_test.cpp が 800 行の上限に近づいたので分けた。
add_executable(codegen_project_test tests/cpp/codegen_project_test.cpp)
target_include_directories(codegen_project_test PRIVATE tests/cpp src)
target_compile_definitions(codegen_project_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(codegen_project_test PRIVATE mosa_lib)
add_test(NAME codegen_project COMMAND $<TARGET_FILE:codegen_project_test>)

# レビュー 2026-11-20 の H-27（ADR 0030）。単位の名前空間とファイル名は位置から付け、どのモジュール名でも壊れない。
add_executable(unit_names_test tests/cpp/unit_names_test.cpp)
target_include_directories(unit_names_test PRIVATE tests/cpp src)
target_compile_definitions(unit_names_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(unit_names_test PRIVATE mosa_lib)
add_test(NAME unit_names COMMAND $<TARGET_FILE:unit_names_test>)

# レビュー 2026-11-20 の H-28。文面の表を別の機械で読み、利用者のモジュール lists、format、messages_* を置き換えない。
add_executable(catalog_isolation_test tests/cpp/catalog_isolation_test.cpp)
target_include_directories(catalog_isolation_test PRIVATE tests/cpp src)
target_compile_definitions(catalog_isolation_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(catalog_isolation_test PRIVATE mosa_lib)
add_test(NAME catalog_isolation COMMAND $<TARGET_FILE:catalog_isolation_test>)

# mosac emit の目録を本体と同じ生成器、コンパイラ、構成で作る。libmosa は作り直さず、このビルド木のものを使う。
foreach(emit_case hello broken undefined undefined_catch undefined_qualified undefined_quoted undefined_handled refused duplicate silent newop refused_op halt
  trigraph bigint)
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

# mosac 自身のディレクトリの名前に ]==] や改行があっても、生成した CMakeLists.txt の角括弧の引数が破れない
# （レビュー 2026-11-20 の H-29）。configure だけを本体と同じ生成器とコンパイラで流す。
add_test(NAME mosac_bracket_path COMMAND ${CMAKE_COMMAND}
  -DMOSAC=$<TARGET_FILE:mosac> -DSRC=${CMAKE_SOURCE_DIR}/tests/mosac
  -DWORK=${CMAKE_SOURCE_DIR}/tests/tmp/mosac_bracket_path "-DGENERATOR=${CMAKE_GENERATOR}"
  "-DPLATFORM=${CMAKE_GENERATOR_PLATFORM}" "-DCXX=${CMAKE_CXX_COMPILER}" "-DBUILD_TYPE=${CMAKE_BUILD_TYPE}"
  -DMOSA_LIB=${CMAKE_SOURCE_DIR}/lib -P ${CMAKE_SOURCE_DIR}/tests/mosac/bracket_path_test.cmake)

# 同梱の lib/*.pl を訳しても、未定義の呼び出しの警告が出ない（L12 の敵対的レビュー A4。警告はビルドを止めない）。
add_test(NAME mosac_library_clean COMMAND ${CMAKE_COMMAND}
  -DMOSAC=$<TARGET_FILE:mosac> -DMOSA_LIB=${CMAKE_SOURCE_DIR}/lib -DWORK=${CMAKE_SOURCE_DIR}/tests/tmp/mosac_library_clean
  -P ${CMAKE_SOURCE_DIR}/tests/mosac/library_clean_test.cmake)

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
foreach(build_case build_hello run_exit_code run_args cache_reuse internal_error debug_trace module_names
                   catalog_names)
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

# ADR 0023。mosac dist の書庫を cmake -E tar で展開して動かす。名前の <OS>-<arch> は鍵 MOSA_ABI の前の 2 つ。
string(REGEX MATCH "^[^-]+-[^-]+" mosac_dist_os_arch "${MOSA_ABI}")
set(mosac_dist_ext .tar.gz)
if(WIN32)
  set(mosac_dist_ext .zip)
endif()
foreach(dist_case hello default_dir japanese replace out_is_file tmp_link name_clash dot_stem parallel build_fails)
  add_test(NAME mosac_dist_${dist_case} COMMAND ${CMAKE_COMMAND}
    -DMOSAC=$<TARGET_FILE:mosac> -DMOSA=$<TARGET_FILE:mosa> -DCASE=${dist_case} -DSRC=${CMAKE_SOURCE_DIR}/tests/mosac
    -DWORK=${CMAKE_SOURCE_DIR}/tests/tmp/mosac_dist_${dist_case} "-DCXX=${mosac_build_cxx}"
    "-DEXE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}" -DMOSA_LIB=${CMAKE_SOURCE_DIR}/lib -DROOT=${CMAKE_SOURCE_DIR}
    -DOS_ARCH=${mosac_dist_os_arch} -DARCHIVE_EXT=${mosac_dist_ext} -DVERSION=${PROJECT_VERSION}
    -P ${CMAKE_SOURCE_DIR}/tests/mosac/dist_test.cmake)
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
foreach(standalone_case uncaught_message use_lists use_dcg include plain_use subdir goal_use_body goal_use_init
    goal_use_late reexport cond_late_lib cond_build_mod cond_skip_op cond_export
    cond_goal_mod cond_goal_lib cond_build_fail cond_else_build cond_consult_if cond_consult_elif cond_half_main
    cond_half_lib cond_clause_fail cond_clause_late cond_after_apart cond_after_late debugger_state
    takeover_lists takeover_apply catalog_goal_lib)
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

# 64 KiB を超えるアトムの名前を字面にせず、MOSA_WERROR=1 の mosac run が mosa と同じ答えを出す（レビュー H-31）。
add_test(NAME mosac_long_atom COMMAND ${CMAKE_COMMAND}
  -DMOSA=$<TARGET_FILE:mosa> -DMOSAC=$<TARGET_FILE:mosac> -DWORK=${CMAKE_SOURCE_DIR}/tests/tmp/mosac_long_atom
  "-DCXX=${mosac_build_cxx}" -DMOSA_LIB=${CMAKE_SOURCE_DIR}/lib
  -P ${CMAKE_SOURCE_DIR}/tests/mosac/long_atom_test.cmake)
set_tests_properties(mosac_long_atom PROPERTIES TIMEOUT 1800
  ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${mosac_cmake_dir}")

# mosac build で作った実行ファイルが、データのファイルの 100,000 段の項を 1 MiB のスタックで読む（ADR 0011 改訂 4）。
add_test(NAME mosac_deep_file COMMAND ${CMAKE_COMMAND}
  -DMOSAC=$<TARGET_FILE:mosac> -DWORK=${CMAKE_SOURCE_DIR}/tests/tmp/mosac_deep_file
  "-DCXX=${mosac_build_cxx}" -DMOSA_LIB=${CMAKE_SOURCE_DIR}/lib "-DEXE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
  -DSMALL_STACK=${CMAKE_SOURCE_DIR}/tests/stack/small_stack.cmake
  -P ${CMAKE_SOURCE_DIR}/tests/stack/deep_file.cmake)
set_tests_properties(mosac_deep_file PROPERTIES TIMEOUT 900
  ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${mosac_cmake_dir}")

# 節の本体の 100,000 ゴールの連言と 100,000 段の入れ子を、mosac emit が 1 MiB のスタックで訳す。64 段の上限の両側の
# 形はビルドし、1 MiB で動かした出力を mosa と比べる（ADR 0011 改訂 5、レビュー 2026-11-20 の H-30）。
add_test(NAME mosac_deep_body COMMAND ${CMAKE_COMMAND}
  -DMOSA=$<TARGET_FILE:mosa> -DMOSAC=$<TARGET_FILE:mosac> -DWORK=${CMAKE_SOURCE_DIR}/tests/tmp/mosac_deep_body
  "-DCXX=${mosac_build_cxx}" -DMOSA_LIB=${CMAKE_SOURCE_DIR}/lib "-DEXE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
  -DSMALL_STACK=${CMAKE_SOURCE_DIR}/tests/stack/small_stack.cmake
  -P ${CMAKE_SOURCE_DIR}/tests/stack/deep_body.cmake)
set_tests_properties(mosac_deep_body PROPERTIES TIMEOUT 1800
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

# 入れ子の収集 100,000 段を mosac の実行ファイルで 1 MiB のスタックの下で動かす（ADR 0013、レビュー H4）。
add_test(NAME mosac_deep_collect COMMAND ${CMAKE_COMMAND}
  -DMOSAC=$<TARGET_FILE:mosac> -DSRC=${CMAKE_SOURCE_DIR}/tests -DWORK=${CMAKE_SOURCE_DIR}/tests/tmp/mosac_deep_collect
  "-DCXX=${mosac_build_cxx}" -DMOSA_LIB=${CMAKE_SOURCE_DIR}/lib "-DEXE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
  -P ${CMAKE_SOURCE_DIR}/tests/mosac/deep_collect_test.cmake)
set_tests_properties(mosac_deep_collect PROPERTIES TIMEOUT 1800
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

# リリースのパッケージ（リリース 0.1.0 Task 9）。cmake/package.cmake で作って空白と日本語のある場所に展開し、その木の
# mosatest でマニュアルを動かす。Release 以外は台本が「skipped: 」を出す。第 13 章の mosac build は同梱の実行時ライブラリ
# を使うので、鍵の合うこのビルドのコンパイラを MOSA_CXX で渡す（Windows は mosac の既定の vswhere で探す）。
add_test(NAME package_layout COMMAND ${CMAKE_COMMAND}
  -DSRC=${CMAKE_SOURCE_DIR} -DBUILD_DIR=${CMAKE_BINARY_DIR} "-DCONFIG=$<CONFIG>" "-DABI=${mosa_abi}"
  -DVERSION=${PROJECT_VERSION} -DWORK=${CMAKE_SOURCE_DIR}/tests/tmp/package "-DEXE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
  -P ${CMAKE_SOURCE_DIR}/tests/package_test.cmake)
set(package_env "MOSA_WERROR=1")
if(mosac_build_cxx)
  list(APPEND package_env "MOSA_CXX=${mosac_build_cxx}")
endif()
set_tests_properties(package_layout PROPERTIES ENVIRONMENT "${package_env}" TIMEOUT 1800
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
# bench/ の 7 本は tests/pl/bench_<名前>.pl が include し、最初の解の確かめと短い bench(N) を両方の側で比べる
# （計画 Task 13）。どれも比べた一覧に出て、生成関数が付いたこと。
set(diff_bench_files bench_nrev.pl bench_queens.pl bench_zebra.pl bench_tak.pl bench_deriv.pl bench_crypt.pl
  bench_bigint.pl)
add_test(NAME diff_tests_pl COMMAND ${CMAKE_COMMAND}
  -DMOSATEST=$<TARGET_FILE:mosatest> -DDIR=tests/pl -DWORK=${CMAKE_SOURCE_DIR}/tests/tmp/diff_tests_pl
  "-DCMAKE_PATH=${CMAKE_COMMAND}" "-DCONFIG=$<IF:$<BOOL:$<CONFIG>>,$<CONFIG>,Release>"
  "-DREQUIRE=japanese.pl;native.pl;dcg.pl;lib_dcg.pl;modules.pl;${diff_bench_files}" "-DATTACHED=native.pl;${diff_bench_files}" -DROOT=${CMAKE_SOURCE_DIR}
  -P ${CMAKE_SOURCE_DIR}/tests/mosac/diff_test.cmake)
set_tests_properties(diff_tests_pl PROPERTIES ENVIRONMENT "${mosatest_bench_env}" LABELS diff RESOURCE_LOCK "agreement;pl_tmp"
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
