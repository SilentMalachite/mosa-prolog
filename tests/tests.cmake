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

add_executable(bigint_test tests/cpp/bigint_test.cpp)
target_include_directories(bigint_test PRIVATE tests/cpp)
target_link_libraries(bigint_test PRIVATE mosa_lib)
add_test(NAME bigint COMMAND $<TARGET_FILE:bigint_test>)

add_executable(bigint_heap_test tests/cpp/bigint_heap_test.cpp)
target_include_directories(bigint_heap_test PRIVATE tests/cpp src)
target_link_libraries(bigint_heap_test PRIVATE mosa_lib)
add_test(NAME bigint_heap COMMAND $<TARGET_FILE:bigint_heap_test>)
set_tests_properties(bigint_heap PROPERTIES ENVIRONMENT "MOSA_LIB=${CMAKE_SOURCE_DIR}/lib")

add_executable(bigint_copy_test tests/cpp/bigint_copy_test.cpp)
target_include_directories(bigint_copy_test PRIVATE tests/cpp src)
target_link_libraries(bigint_copy_test PRIVATE mosa_lib)
add_test(NAME bigint_copy COMMAND $<TARGET_FILE:bigint_copy_test>)
set_tests_properties(bigint_copy PROPERTIES ENVIRONMENT "MOSA_LIB=${CMAKE_SOURCE_DIR}/lib")

add_executable(unify_test tests/cpp/unify_test.cpp)
target_include_directories(unify_test PRIVATE tests/cpp)
target_link_libraries(unify_test PRIVATE mosa_lib)
add_test(NAME unify COMMAND $<TARGET_FILE:unify_test>)

add_executable(reader_test tests/cpp/reader_test.cpp)
target_include_directories(reader_test PRIVATE tests/cpp)
target_link_libraries(reader_test PRIVATE mosa_lib)
add_test(NAME reader COMMAND $<TARGET_FILE:reader_test>)

add_executable(writer_test tests/cpp/writer_test.cpp)
target_include_directories(writer_test PRIVATE tests/cpp src)
target_link_libraries(writer_test PRIVATE mosa_lib)
add_test(NAME writer COMMAND $<TARGET_FILE:writer_test>)

# writer の出力を golden のファイルで固定する。--write-golden を付けて動かすとファイルを書き直す。
add_executable(writer_golden_test tests/cpp/writer_golden_test.cpp)
target_include_directories(writer_golden_test PRIVATE tests/cpp)
target_link_libraries(writer_golden_test PRIVATE mosa_lib)
add_test(NAME writer_golden COMMAND $<TARGET_FILE:writer_golden_test>
  ${CMAKE_SOURCE_DIR}/tests/cpp/data/writer_golden.txt)

# 1 MiB のスタックで、深さ 1,000,000 の項を書く（ADR 0011）。
add_test(NAME stack_deep_write COMMAND ${CMAKE_COMMAND}
  "-DCOMMAND=$<TARGET_FILE:mosa>;${CMAKE_SOURCE_DIR}/tests/stack/deep_write.pl"
  -DEXPECTED=${CMAKE_SOURCE_DIR}/tests/stack/deep_write.expected -DWORK=${CMAKE_SOURCE_DIR}/tests/tmp/stack
  -DSUMMARY=1 -P ${CMAKE_SOURCE_DIR}/tests/stack/small_stack.cmake)
set_tests_properties(stack_deep_write PROPERTIES TIMEOUT 900 ENVIRONMENT "MOSA_LIB=${CMAKE_SOURCE_DIR}/lib")

# 1 MiB のスタックで、深さ 1,000,000 の 7 つの形（関数表記、リスト、前置、xfy、括弧、波括弧、| の尾）を読む（ADR 0011 改訂 4）。
add_test(NAME stack_deep_read COMMAND ${CMAKE_COMMAND}
  "-DCOMMAND=$<TARGET_FILE:mosa>;${CMAKE_SOURCE_DIR}/tests/stack/deep_read.pl"
  -DEXPECTED=${CMAKE_SOURCE_DIR}/tests/stack/deep_read.expected -DWORK=${CMAKE_SOURCE_DIR}/tests/tmp/stack
  -P ${CMAKE_SOURCE_DIR}/tests/stack/small_stack.cmake)
set_tests_properties(stack_deep_read PROPERTIES TIMEOUT 900 ENVIRONMENT "MOSA_LIB=${CMAKE_SOURCE_DIR}/lib")

# 1 MiB のスタックで、深さ 1,000,000 の項に項を歩く組込を使う（ADR 0011、計画 Task 7）。
# macOS のランナーでは 250〜510 秒かかり、ae22e43 の push の 2 回が 600 秒の既定を超えた（別の項目で止まり、機能の失敗は無し）。
# 持ち主の A で、このテストだけ上限を 1200 秒に延ばし、ctest の上限はそれより上に置く。
add_test(NAME stack_deep_builtins COMMAND ${CMAKE_COMMAND}
  "-DCOMMAND=$<TARGET_FILE:mosa>;${CMAKE_SOURCE_DIR}/tests/stack/deep_builtins.pl"
  -DEXPECTED=${CMAKE_SOURCE_DIR}/tests/stack/deep_builtins.expected -DWORK=${CMAKE_SOURCE_DIR}/tests/tmp/stack
  -DTIME_LIMIT=1200 -P ${CMAKE_SOURCE_DIR}/tests/stack/small_stack.cmake)
set_tests_properties(stack_deep_builtins PROPERTIES TIMEOUT 1500 ENVIRONMENT "MOSA_LIB=${CMAKE_SOURCE_DIR}/lib")

# 1 MiB のスタックで、findall/3・findall/4・bagof/3・setof/3 を 100,000 段入れ子にする（ADR 0013、レビュー H4）。
add_test(NAME stack_deep_collect COMMAND ${CMAKE_COMMAND}
  "-DCOMMAND=$<TARGET_FILE:mosa>;${CMAKE_SOURCE_DIR}/tests/stack/deep_collect.pl"
  -DEXPECTED=${CMAKE_SOURCE_DIR}/tests/stack/deep_collect.expected -DWORK=${CMAKE_SOURCE_DIR}/tests/tmp/stack
  -P ${CMAKE_SOURCE_DIR}/tests/stack/small_stack.cmake)
set_tests_properties(stack_deep_collect PROPERTIES TIMEOUT 900 ENVIRONMENT "MOSA_LIB=${CMAKE_SOURCE_DIR}/lib")

# 1 MiB のスタックで、ソースに書いた 200,000 要素のリストを読んで動かす（ADR 0011）。
add_test(NAME long_literal COMMAND ${CMAKE_COMMAND} -DMOSA=$<TARGET_FILE:mosa>
  -DWORK=${CMAKE_SOURCE_DIR}/tests/tmp/long_literal -DSMALL_STACK=${CMAKE_SOURCE_DIR}/tests/stack/small_stack.cmake
  -P ${CMAKE_SOURCE_DIR}/tests/stack/long_literal.cmake)
set_tests_properties(long_literal PROPERTIES TIMEOUT 900 ENVIRONMENT "MOSA_LIB=${CMAKE_SOURCE_DIR}/lib")

# 1 MiB のスタックで、30,000 段に入れ子の :- meta_predicate の頭を集める（レビュー H-10、ADR 0011）。
add_test(NAME stack_deep_meta COMMAND ${CMAKE_COMMAND} -DMOSA=$<TARGET_FILE:mosa>
  -DWORK=${CMAKE_SOURCE_DIR}/tests/tmp/deep_meta -DSMALL_STACK=${CMAKE_SOURCE_DIR}/tests/stack/small_stack.cmake
  -P ${CMAKE_SOURCE_DIR}/tests/stack/deep_meta.cmake)
set_tests_properties(stack_deep_meta PROPERTIES TIMEOUT 900 ENVIRONMENT "MOSA_LIB=${CMAKE_SOURCE_DIR}/lib")

# 差分テストの答えの字面（計画 Task 11）。深い項を既定のスタックで書けることまで見る。
add_executable(answer_text_test tests/cpp/answer_text_test.cpp)
target_include_directories(answer_text_test PRIVATE tests/cpp src/machine)
target_link_libraries(answer_text_test PRIVATE mosa_lib)
add_test(NAME answer_text COMMAND $<TARGET_FILE:answer_text_test>)

add_test(NAME roundtrip COMMAND $<TARGET_FILE:mosatest> --roundtrip ${CMAKE_SOURCE_DIR}/tests/pl/syntax.pl)
# テストのパスはリポジトリの根から書く。build の場所に左右されない（計画 Task 15）。
add_test(NAME prolog_tests COMMAND $<TARGET_FILE:mosatest> ${CMAKE_SOURCE_DIR}/tests/pl
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
# tests/pl の consult と close の試験は tests/tmp の同じパスに書いて読む。open/3 の write は開いた時点で空にするので、
# 差分テスト（diff_tests_pl）と重なると、片方が書き終える前のファイルをもう片方が読みうる（Codex レビュー）。
set_tests_properties(prolog_tests PROPERTIES RESOURCE_LOCK pl_tmp TIMEOUT 1800)

add_executable(heap_growth_test tests/cpp/heap_growth_test.cpp)
target_include_directories(heap_growth_test PRIVATE tests/cpp)
target_link_libraries(heap_growth_test PRIVATE mosa_lib)
add_test(NAME heap_growth COMMAND $<TARGET_FILE:heap_growth_test>)
# レビュー M6（ADR 0010 の改訂 2）。findall/bagof/setof が持つ答えを上限に数える。
add_executable(collect_limit_test tests/cpp/collect_limit_test.cpp)
target_include_directories(collect_limit_test PRIVATE tests/cpp src)
target_link_libraries(collect_limit_test PRIVATE mosa_lib)
add_test(NAME collect_limit COMMAND $<TARGET_FILE:collect_limit_test>)
# GC ストレスと並列のもとでは既定の 600 秒に近づき（前回の CI で 313 秒）、Debug + ASan では 1792 秒かかった。
set_tests_properties(collect_limit PROPERTIES TIMEOUT 3600)
# レビュー M17（ADR 0010 の改訂 3）。選択点のスタック、extras、env、実行時に訳したゴールを上限に数える。
add_executable(state_limit_test tests/cpp/state_limit_test.cpp)
target_include_directories(state_limit_test PRIVATE tests/cpp)
target_link_libraries(state_limit_test PRIVATE mosa_lib)
add_test(NAME state_limit COMMAND $<TARGET_FILE:state_limit_test>)
# Debug + ASan では Release の何倍もかかる（CI の linux-asan で 300 秒を超え、795 秒で通った次も 1200 秒を超えた）。
# 長い CPU ループなので、並列の取り合いで壁時計が膨らまないよう、ひとりで走らせて上限も上げる。
set_tests_properties(state_limit PROPERTIES RUN_SERIAL TRUE TIMEOUT 2400)
# フェーズ10（ADR 0025）。Big の算術は結果の大きさを先に見積もり、ヒープの上限で止まる。
add_executable(bigint_limit_test tests/cpp/bigint_limit_test.cpp)
target_include_directories(bigint_limit_test PRIVATE tests/cpp)
target_link_libraries(bigint_limit_test PRIVATE mosa_lib)
add_test(NAME bigint_limit COMMAND $<TARGET_FILE:bigint_limit_test>)
# 300 万桁の解析など長い CPU ループ。並列の macos ジョブで 137 秒で通った次に 300 秒を超えたので、
# ひとりで走らせて上限も上げる（state_limit と同じ）。
set_tests_properties(bigint_limit PROPERTIES RUN_SERIAL TRUE TIMEOUT 900)

# mark-and-compact の GC（ADR 0010）。ヒープ・extras・トレイルの頭打ちと、生きた項が保たれること。
add_executable(gc_test tests/cpp/gc_test.cpp tests/cpp/gc_policy_test.cpp)
target_include_directories(gc_test PRIVATE tests/cpp src)
target_compile_definitions(gc_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(gc_test PRIVATE mosa_lib)
add_test(NAME gc COMMAND $<TARGET_FILE:gc_test>)

# レビュー H6。ヒープの上限で with_output_to/2 と portray/1 から抜けても現在出力が戻り、捕獲の流れが閉じる。
add_executable(output_stop_test tests/cpp/output_stop_test.cpp)
target_include_directories(output_stop_test PRIVATE tests/cpp src)
target_compile_definitions(output_stop_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(output_stop_test PRIVATE mosa_lib)
add_test(NAME output_stop COMMAND $<TARGET_FILE:output_stop_test>)
add_executable(file_flush_test tests/cpp/file_flush_test.cpp)
target_include_directories(file_flush_test PRIVATE tests/cpp)
target_compile_definitions(file_flush_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(file_flush_test PRIVATE mosa_lib)
add_test(NAME file_flush COMMAND $<TARGET_FILE:file_flush_test>)
add_executable(portray_halt_test tests/cpp/portray_halt_test.cpp)
target_include_directories(portray_halt_test PRIVATE tests/cpp)
target_compile_definitions(portray_halt_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(portray_halt_test PRIVATE mosa_lib)
add_test(NAME portray_halt COMMAND $<TARGET_FILE:portray_halt_test>)
add_executable(answer_prompt_test tests/cpp/answer_prompt_test.cpp)
target_include_directories(answer_prompt_test PRIVATE tests/cpp)
target_link_libraries(answer_prompt_test PRIVATE mosa_lib)
add_test(NAME answer_prompt COMMAND $<TARGET_FILE:answer_prompt_test>)

# ADR 0012。決定的に抜けた catch/3 と *-> の Entry を外し、ループで選択点スタックとヒープが頭打ちになる。
add_executable(choice_exit_test tests/cpp/choice_exit_test.cpp)
target_include_directories(choice_exit_test PRIVATE tests/cpp src)
target_link_libraries(choice_exit_test PRIVATE mosa_lib)
add_test(NAME choice_exit COMMAND $<TARGET_FILE:choice_exit_test>)

# レビュー M3（ADR 0015）。assertz で積んだ p/2 の節を 4 倍にしても、ゴールの時間が節の数に比例する範囲に収まる。
add_executable(index_scale_test tests/cpp/index_scale_test.cpp)
target_include_directories(index_scale_test PRIVATE tests/cpp)
target_link_libraries(index_scale_test PRIVATE mosa_lib)
add_test(NAME index_scale COMMAND $<TARGET_FILE:index_scale_test>)
# 時間の比を測るので、並列で CPU を取り合うと比が崩れる。ほかの scale と同じくひとりで走らせる（全体レビュー推奨5）。
set_tests_properties(index_scale PROPERTIES RUN_SERIAL TRUE)
# レビュー M7。リストを書く時間が要素の数に比例する範囲に収まる（背骨の巡回の先読みが祖先で止まる）。
add_executable(write_scale_test tests/cpp/write_scale_test.cpp)
target_include_directories(write_scale_test PRIVATE tests/cpp)
target_link_libraries(write_scale_test PRIVATE mosa_lib)
add_test(NAME write_scale COMMAND $<TARGET_FILE:write_scale_test>)
set_tests_properties(write_scale PROPERTIES RUN_SERIAL TRUE)
# レビュー M32。sub_atom/5 の 1 つの答えの時間が原子の長さに比例する範囲に収まる（束縛された引数で候補を絞る）。
add_executable(sub_atom_scale_test tests/cpp/sub_atom_scale_test.cpp)
target_include_directories(sub_atom_scale_test PRIVATE tests/cpp)
target_link_libraries(sub_atom_scale_test PRIVATE mosa_lib)
add_test(NAME sub_atom_scale COMMAND $<TARGET_FILE:sub_atom_scale_test>)
# 時間の比を測るので、並列の ctest -j でほかのテストと CPU を取り合うと落ちうる。ひとりで走らせる。
set_tests_properties(sub_atom_scale PROPERTIES RUN_SERIAL TRUE)
# 全体レビュー推奨4。=../2 のリスト読みと読み書きの変数表が要素数に比例する範囲に収まる。
add_executable(solutions_scale_test tests/cpp/solutions_scale_test.cpp)
target_include_directories(solutions_scale_test PRIVATE tests/cpp)
target_link_libraries(solutions_scale_test PRIVATE mosa_lib)
add_test(NAME solutions_scale COMMAND $<TARGET_FILE:solutions_scale_test>)
# 時間の比を測るので、ほかの *_scale と同じく 1 本だけで流す。並べて流すと CI の windows で比が 12〜24 に揺れた。
set_tests_properties(solutions_scale PROPERTIES RUN_SERIAL TRUE)
add_executable(term_scale_test tests/cpp/term_scale_test.cpp)
target_include_directories(term_scale_test PRIVATE tests/cpp)
target_link_libraries(term_scale_test PRIVATE mosa_lib)
add_test(NAME term_scale COMMAND $<TARGET_FILE:term_scale_test>)
set_tests_properties(term_scale PROPERTIES RUN_SERIAL TRUE)
# レビュー M3（ADR 0015）。未束縛の第1引数の呼び出し・retract・clause/2 の答えごとの more と、
# 閉じた走査が残した節の消去。
add_executable(clause_scan_test tests/cpp/clause_scan_test.cpp)
target_include_directories(clause_scan_test PRIVATE tests/cpp src)
target_link_libraries(clause_scan_test PRIVATE mosa_lib)
target_compile_definitions(clause_scan_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
add_test(NAME clause_scan COMMAND $<TARGET_FILE:clause_scan_test>)
# レビュー L11（ADR 0020）。読み直しは、ファイルがもう定義しない述語を消す。
add_executable(reconsult_test tests/cpp/reconsult_test.cpp)
target_include_directories(reconsult_test PRIVATE tests/cpp src)
target_link_libraries(reconsult_test PRIVATE mosa_lib)
target_compile_definitions(reconsult_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
add_test(NAME reconsult COMMAND $<TARGET_FILE:reconsult_test>)
# Muse のレビューの中。壊れた docs/agreement.json を捨てて書き直す。
add_executable(agreement_json_test tests/cpp/agreement_json_test.cpp)
target_include_directories(agreement_json_test PRIVATE tests/cpp src)
target_compile_definitions(agreement_json_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(agreement_json_test PRIVATE mosa_lib)
add_test(NAME agreement_json COMMAND $<TARGET_FILE:agreement_json_test>)
# レビュー M21（ADR 0015 の改訂 2）。節の列の大きさ・順・写し。
add_executable(clause_list_test tests/cpp/clause_list_test.cpp)
target_include_directories(clause_list_test PRIVATE tests/cpp src)
target_link_libraries(clause_list_test PRIVATE mosa_lib)
add_test(NAME clause_list COMMAND $<TARGET_FILE:clause_list_test>)
# レビュー M25（ADR 0018）。述語表の索引と読み込みの写しの鎖、述語の数を 4 倍にしたときの呼び出しと読み込みの時間。
add_executable(pred_table_test tests/cpp/pred_table_test.cpp)
target_include_directories(pred_table_test PRIVATE tests/cpp src)
target_link_libraries(pred_table_test PRIVATE mosa_lib)
add_test(NAME pred_table COMMAND $<TARGET_FILE:pred_table_test>)
add_executable(pred_scale_test tests/cpp/pred_scale_test.cpp)
target_include_directories(pred_scale_test PRIVATE tests/cpp)
target_link_libraries(pred_scale_test PRIVATE mosa_lib)
add_test(NAME pred_scale COMMAND $<TARGET_FILE:pred_scale_test>)
# 時間の比を測るので、並列の ctest -j でほかのテストと CPU を取り合うと落ちうる。ひとりで走らせる。
set_tests_properties(pred_scale PROPERTIES RUN_SERIAL TRUE)
# 百万要素のリストや loop を GC 付きで回す。ストレス（MOSA_GC_STRESS）では既定の 1500 秒を超えうる。
set_tests_properties(gc heap_growth PROPERTIES TIMEOUT 3600)

add_executable(full_stop_test tests/cpp/full_stop_test.cpp)
target_include_directories(full_stop_test PRIVATE tests/cpp)
target_compile_definitions(full_stop_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(full_stop_test PRIVATE mosa_lib)
add_test(NAME full_stop COMMAND $<TARGET_FILE:full_stop_test>)

add_executable(message_ops_test tests/cpp/message_ops_test.cpp)
target_include_directories(message_ops_test PRIVATE tests/cpp)
target_link_libraries(message_ops_test PRIVATE mosa_lib)
add_test(NAME message_ops COMMAND $<TARGET_FILE:message_ops_test>)
# 読み込みの指示が GC を起こすカタログ。文面の引数は、カタログを読む前に字面にしておく（L19 の Codex レビュー）。
# 入力のファイルは tests/cpp/data に置いて追跡する。前は configure のときに git の無視する tests/tmp に書いたので、
# tests/tmp を空にすると configure し直すまで試験が落ちるか、黙って代わりの表示で通った（レビュー H-35）。
add_test(NAME message_ops_gc COMMAND $<TARGET_FILE:message_ops_test> gc)
set_tests_properties(message_ops_gc PROPERTIES ENVIRONMENT "MOSA_LIB=${CMAKE_SOURCE_DIR}/tests/cpp/data/messages_gc")

# ADR 0006 の改訂 1。library(...) のファイルは ISO の核と自分のモジュールの演算子で読む。plain.pl はパスで読む。
# 入力のファイルは tests/cpp/data/library_ops にある（レビュー H-35）。nestops.pl のゴールの consult は相対パスを
# 作業ディレクトリから開くので、リポジトリの根で動かす。
add_executable(library_ops_test tests/cpp/library_ops_test.cpp)
target_include_directories(library_ops_test PRIVATE tests/cpp)
target_compile_definitions(library_ops_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(library_ops_test PRIVATE mosa_lib)
add_test(NAME library_ops COMMAND $<TARGET_FILE:library_ops_test>)
set_tests_properties(library_ops PROPERTIES ENVIRONMENT "MOSA_LIB=${CMAKE_SOURCE_DIR}/tests/cpp/data/library_ops"
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# レビュー L21 の Codex 敵対的レビュー。共有の多い要素を持つリストを、8 MiB の上限で format/2 と
# atomic_list_concat/2,3 に渡す。背骨の検査が \+ でリストを写すと、上限で止まる。
add_executable(lib_shared_test tests/cpp/lib_shared_test.cpp)
target_include_directories(lib_shared_test PRIVATE tests/cpp)
target_compile_definitions(lib_shared_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(lib_shared_test PRIVATE mosa_lib)
add_test(NAME lib_shared COMMAND $<TARGET_FILE:lib_shared_test>)
set_tests_properties(lib_shared PROPERTIES ENVIRONMENT "MOSA_LIB=${CMAKE_SOURCE_DIR}/lib")
# 0.1.1。format/1,2,3 は成功して選択点を残さない（対話が促しを出さない）。
add_executable(format_det_test tests/cpp/format_det_test.cpp)
target_include_directories(format_det_test PRIVATE tests/cpp)
target_link_libraries(format_det_test PRIVATE mosa_lib)
add_test(NAME format_det COMMAND $<TARGET_FILE:format_det_test>)
set_tests_properties(format_det PROPERTIES ENVIRONMENT "MOSA_LIB=${CMAKE_SOURCE_DIR}/lib")
# ADR 0023。mosac dist の書庫（tar.gz と zip）をバイトで確かめる。
add_executable(archive_test tests/cpp/archive_test.cpp)
target_include_directories(archive_test PRIVATE tests/cpp src)
target_link_libraries(archive_test PRIVATE mosa_lib)
add_test(NAME archive COMMAND $<TARGET_FILE:archive_test>)
# レビュー L23。initialization(G, main) のゴールは、後の op/3 の下でも指令の項のまま動く。
add_executable(main_goal_test tests/cpp/main_goal_test.cpp)
target_include_directories(main_goal_test PRIVATE tests/cpp)
target_compile_definitions(main_goal_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(main_goal_test PRIVATE mosa_lib)
add_test(NAME main_goal COMMAND $<TARGET_FILE:main_goal_test>)
# レビュー M29。~e と ~f の桁数はプロセスを落とさず、上限の語数を超える有限の数の桁はヒープの上限で止まる。
add_executable(format_digits_test tests/cpp/format_digits_test.cpp)
target_include_directories(format_digits_test PRIVATE tests/cpp)
target_link_libraries(format_digits_test PRIVATE mosa_lib)
add_test(NAME format_digits COMMAND $<TARGET_FILE:format_digits_test>)
# レビュー M30。assert は共有の深い項を木に広げず、共有しない形はヒープの上限で止まる。
add_executable(assert_shared_test tests/cpp/assert_shared_test.cpp)
target_include_directories(assert_shared_test PRIVATE tests/cpp)
target_compile_definitions(assert_shared_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(assert_shared_test PRIVATE mosa_lib)
add_test(NAME assert_shared COMMAND $<TARGET_FILE:assert_shared_test>)
# head_first と chain_ratio は cputime の比で見るので、scale と同じくひとりで走らせる
# （並列の readme-windows で比が崩れて落ちた）。
set_tests_properties(assert_shared PROPERTIES RUN_SERIAL TRUE)
# レビュー M28。実行時のゴールの共有の多い引数は写さずに指し、すべて写す最後の段はヒープの上限で止まる。
add_executable(goal_shared_test tests/cpp/goal_shared_test.cpp)
target_include_directories(goal_shared_test PRIVATE tests/cpp)
target_link_libraries(goal_shared_test PRIVATE mosa_lib)
add_test(NAME goal_shared COMMAND $<TARGET_FILE:goal_shared_test>)
# フェーズ12 の Muse レビュー（Major 1）。読み込みの途中でヒープの上限の例外が抜けても、読み込みの状態を残さない。
add_executable(load_throw_test tests/cpp/load_throw_test.cpp)
target_include_directories(load_throw_test PRIVATE tests/cpp)
target_compile_definitions(load_throw_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(load_throw_test PRIVATE mosa_lib)
add_test(NAME load_throw COMMAND $<TARGET_FILE:load_throw_test>)
# フェーズ13 Task 3。条件付きコンパイルの診断（対の誤りの字面、条件の誤りの Formal、halt の終了コード）。期待の
# 字面は message_text で作るので、machine/internal.hpp を読む src も足す。
add_executable(cond_compile_test tests/cpp/cond_compile_test.cpp)
target_include_directories(cond_compile_test PRIVATE tests/cpp src)
target_compile_definitions(cond_compile_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(cond_compile_test PRIVATE mosa_lib)
add_test(NAME cond_compile COMMAND $<TARGET_FILE:cond_compile_test>)

add_executable(session_test tests/cpp/session_test.cpp tests/cpp/session_goals_test.cpp)
target_include_directories(session_test PRIVATE tests/cpp src)
target_compile_definitions(session_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(session_test PRIVATE mosa_lib)
add_test(NAME session COMMAND $<TARGET_FILE:session_test>)
# 今ある日本語の文面の確認を、流す機械の LANG に左右させない（ADR 0007）。
set_tests_properties(session PROPERTIES ENVIRONMENT "LANG=ja_JP.UTF-8")
# MOSA_LIB は実行ファイルの lib/ より先に探す。鍵の無い messages_en.pl（tests/cpp/data/messages_empty）で、
# 代わりの表示を確かめる。
add_test(NAME session_messages_missing COMMAND $<TARGET_FILE:session_test> missing)
set_tests_properties(session_messages_missing PROPERTIES
  ENVIRONMENT "MOSA_LIB=${CMAKE_SOURCE_DIR}/tests/cpp/data/messages_empty")
add_test(NAME session_english COMMAND $<TARGET_FILE:session_test> english)
set_tests_properties(session_english PROPERTIES ENVIRONMENT "LANG=C")

add_executable(stream_test tests/cpp/stream_test.cpp tests/cpp/stream_read_test.cpp)
target_include_directories(stream_test PRIVATE tests/cpp)
target_compile_definitions(stream_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(stream_test PRIVATE mosa_lib)
add_test(NAME stream_test COMMAND $<TARGET_FILE:stream_test>)
add_executable(stream_budget_test tests/cpp/stream_budget_test.cpp)
target_include_directories(stream_budget_test PRIVATE tests/cpp)
target_compile_definitions(stream_budget_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(stream_budget_test PRIVATE mosa_lib)
add_test(NAME stream_budget COMMAND $<TARGET_FILE:stream_budget_test>)
add_executable(stdin_read_test tests/cpp/stdin_read_test.cpp)
target_include_directories(stdin_read_test PRIVATE tests/cpp)
target_link_libraries(stdin_read_test PRIVATE mosa_lib)
add_test(NAME stdin_read COMMAND $<TARGET_FILE:stdin_read_test>)
add_executable(standard_traffic_test tests/cpp/standard_traffic_test.cpp)
target_include_directories(standard_traffic_test PRIVATE tests/cpp src)
target_compile_definitions(standard_traffic_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(standard_traffic_test PRIVATE mosa_lib)
add_test(NAME standard_traffic COMMAND $<TARGET_FILE:standard_traffic_test>)
add_executable(stdin_scale_test tests/cpp/stdin_scale_test.cpp)
target_include_directories(stdin_scale_test PRIVATE tests/cpp)
target_link_libraries(stdin_scale_test PRIVATE mosa_lib)
add_test(NAME stdin_scale COMMAND $<TARGET_FILE:stdin_scale_test>)
# 時間の比を測るので、ほかの試験と並べて走らせない（pred_scale と同じ）。
set_tests_properties(stdin_scale PROPERTIES RUN_SERIAL TRUE)

add_executable(machine_test tests/cpp/machine_test.cpp tests/cpp/machine_load_test.cpp)
target_include_directories(machine_test PRIVATE tests/cpp)
target_link_libraries(machine_test PRIVATE mosa_lib)
add_test(NAME machine COMMAND $<TARGET_FILE:machine_test>)
add_executable(machine_deep_test tests/cpp/machine_deep_test.cpp)
target_include_directories(machine_deep_test PRIVATE tests/cpp)
target_link_libraries(machine_deep_test PRIVATE mosa_lib)
add_test(NAME machine_deep COMMAND $<TARGET_FILE:machine_deep_test>)
add_executable(ir_release_test tests/cpp/ir_release_test.cpp)
target_include_directories(ir_release_test PRIVATE tests/cpp)
target_link_libraries(ir_release_test PRIVATE mosa_lib)
add_test(NAME ir_release COMMAND $<TARGET_FILE:ir_release_test>)
add_executable(declare_neck_test tests/cpp/declare_neck_test.cpp)
target_include_directories(declare_neck_test PRIVATE tests/cpp)
target_link_libraries(declare_neck_test PRIVATE mosa_lib)
add_test(NAME declare_neck COMMAND $<TARGET_FILE:declare_neck_test>)
add_executable(atom_alloc_test tests/cpp/atom_alloc_test.cpp)
target_include_directories(atom_alloc_test PRIVATE tests/cpp)
target_link_libraries(atom_alloc_test PRIVATE mosa_lib)
add_test(NAME atom_alloc COMMAND $<TARGET_FILE:atom_alloc_test>)
add_executable(pair_limit_test tests/cpp/pair_limit_test.cpp)
target_include_directories(pair_limit_test PRIVATE tests/cpp)
target_link_libraries(pair_limit_test PRIVATE mosa_lib)
add_test(NAME pair_limit COMMAND $<TARGET_FILE:pair_limit_test>)
add_executable(global_limit_test tests/cpp/global_limit_test.cpp)
target_include_directories(global_limit_test PRIVATE tests/cpp)
target_link_libraries(global_limit_test PRIVATE mosa_lib)
add_test(NAME global_limit COMMAND $<TARGET_FILE:global_limit_test>)
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

add_executable(dcg_test tests/cpp/dcg_test.cpp)
target_include_directories(dcg_test PRIVATE tests/cpp)
target_link_libraries(dcg_test PRIVATE mosa_lib)
add_test(NAME dcg_test COMMAND $<TARGET_FILE:dcg_test>)

add_executable(ensure_test tests/cpp/ensure_test.cpp)
target_include_directories(ensure_test PRIVATE tests/cpp)
target_compile_definitions(ensure_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(ensure_test PRIVATE mosa_lib)
add_test(NAME ensure_test COMMAND $<TARGET_FILE:ensure_test>)

include(${CMAKE_CURRENT_LIST_DIR}/conformance_tests.cmake)

add_executable(bench_report_test tests/cpp/bench_report_test.cpp)
target_include_directories(bench_report_test PRIVATE tests/cpp src/machine)
target_compile_definitions(bench_report_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(bench_report_test PRIVATE mosa_lib)
add_test(NAME bench_report_test COMMAND $<TARGET_FILE:bench_report_test>)

# 版表示は終了コードと 1 行の出力の両方を見る。PASS_REGULAR_EXPRESSION だと CTest は終了コードを見ず、版も手で
# 書くことになった（レビュー L-46）。version_check_self は、その検査が 1 で終わる版表示を落とすことを確かめる。
foreach(version_program mosa mosac mosatest)
  add_test(NAME ${version_program}_version COMMAND ${CMAKE_COMMAND}
    "-DCOMMAND=$<TARGET_FILE:${version_program}>;--version" -DVERSION=${PROJECT_VERSION}
    -P ${CMAKE_SOURCE_DIR}/tests/cpp/version_check.cmake)
endforeach()
add_test(NAME version_check_self COMMAND ${CMAKE_COMMAND}
  -DCHECK=${CMAKE_SOURCE_DIR}/tests/cpp/version_check.cmake -DVERSION=${PROJECT_VERSION}
  -P ${CMAKE_SOURCE_DIR}/tests/cpp/version_check_test.cmake)

add_executable(console_test tests/cpp/console_test.cpp)
target_include_directories(console_test PRIVATE tests/cpp)
target_link_libraries(console_test PRIVATE mosa_lib)
add_test(NAME console COMMAND $<TARGET_FILE:console_test> plain --flag 42)

add_executable(message_language_test tests/cpp/message_language_test.cpp)
target_include_directories(message_language_test PRIVATE tests/cpp)
target_link_libraries(message_language_test PRIVATE mosa_lib)
add_test(NAME message_language COMMAND $<TARGET_FILE:message_language_test>)
add_test(NAME mosa_utf8_pipe COMMAND ${CMAKE_COMMAND} -DMOSA=$<TARGET_FILE:mosa>
  -DWORK=${CMAKE_SOURCE_DIR}/tests/tmp -P ${CMAKE_SOURCE_DIR}/tests/cpp/mosa_utf8_pipe_test.cmake)

add_test(NAME mosa_japanese_arg COMMAND ${CMAKE_COMMAND} -DMOSA=$<TARGET_FILE:mosa>
  -DWORK=${CMAKE_SOURCE_DIR}/tests/tmp -P ${CMAKE_SOURCE_DIR}/tests/cpp/mosa_japanese_arg_test.cmake)

add_test(NAME mosa_stdin COMMAND ${CMAKE_COMMAND} -DMOSA=$<TARGET_FILE:mosa> -DVERSION=${PROJECT_VERSION}
  -DWORK=${CMAKE_SOURCE_DIR}/tests/tmp -P ${CMAKE_SOURCE_DIR}/tests/cpp/mosa_stdin_test.cmake)

# レビュー M-33。ありえない位置への set_stream_position/2 は流れを読み足さずに断る（1 MiB のヒープで止まらない）。
add_test(NAME stream_position COMMAND ${CMAKE_COMMAND} -DMOSA=$<TARGET_FILE:mosa>
  -DWORK=${CMAKE_SOURCE_DIR}/tests/tmp/stream_position -P ${CMAKE_SOURCE_DIR}/tests/cpp/stream_position_test.cmake)

# レビュー M-16。呼べない部分を含む問い合わせ、指令、初期化のゴールは、どの部分も動かす前に止まる。
add_test(NAME callable_goal COMMAND ${CMAKE_COMMAND} -DMOSA=$<TARGET_FILE:mosa>
  -DWORK=${CMAKE_SOURCE_DIR}/tests/tmp/callable_goal -P ${CMAKE_SOURCE_DIR}/tests/cpp/callable_goal_test.cmake)

# レビュー M-12。REPL の続きの行と構文の誤りは、読んだ項をヒープに残さない（1 MiB のヒープで止まらない）。
add_test(NAME repl_heap COMMAND ${CMAKE_COMMAND} -DMOSA=$<TARGET_FILE:mosa>
  -DWORK=${CMAKE_SOURCE_DIR}/tests/tmp -P ${CMAKE_SOURCE_DIR}/tests/cpp/repl_heap_test.cmake)

# MOSA_TRANSCRIPT=1 の管の出力が端末の表示と同じになる（リリース 0.1.0 Task 3、マニュアルの検査の土台）。
foreach(transcript_case query more read order ja)
  set(transcript_lang en_US.UTF-8)
  if(transcript_case STREQUAL "ja")
    set(transcript_lang ja_JP.UTF-8)
  endif()
  add_test(NAME transcript_${transcript_case} COMMAND ${CMAKE_COMMAND} -DMOSA=$<TARGET_FILE:mosa>
    -DDATA=${CMAKE_SOURCE_DIR}/tests/cpp/data/transcript -DCASE=transcript_${transcript_case}
    -DLANG_VALUE=${transcript_lang} -P ${CMAKE_SOURCE_DIR}/tests/cpp/transcript_test.cmake)
endforeach()

include(${CMAKE_CURRENT_LIST_DIR}/debugger_tests.cmake)

include(${CMAKE_CURRENT_LIST_DIR}/mosac_tests.cmake)

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

# レビュー L8。標準入力の CRLF と 0x1A をバイトのとおりに読み、標準出力と標準エラーに LF だけを書く（mosa と実行ファイル）。
add_test(NAME mosa_std_binary COMMAND ${CMAKE_COMMAND}
  -DMOSA=$<TARGET_FILE:mosa> -DMOSAC=$<TARGET_FILE:mosac> -DSRC=${CMAKE_SOURCE_DIR}/tests/mosac
  -DWORK=${CMAKE_SOURCE_DIR}/tests/tmp/mosa_std_binary "-DCXX=${mosac_build_cxx}"
  "-DEXE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}" -DMOSA_LIB=${CMAKE_SOURCE_DIR}/lib
  -P ${CMAKE_SOURCE_DIR}/tests/mosac/std_binary_test.cmake)
set_tests_properties(mosa_std_binary PROPERTIES TIMEOUT 1800
  ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${mosac_cmake_dir}")

# マニュアルの検査（リリース 0.1.0 Task 4〜6）は tests/manual_tests.cmake にある（Task 9 で 800 行を超えないよう分けた）。
# 見本ごとに LANG を決めるので、上の LANG=ja_JP.UTF-8 の一括の設定より後ろで読む。
include(${CMAKE_SOURCE_DIR}/tests/manual_tests.cmake)
