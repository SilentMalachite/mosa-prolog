# mosatest --manual（リリース 0.1.0 Task 4）。印の読み取りは部品テストで、動かして比べるところは tests/manual の見本で
# 確かめる。見本ごとに --work を分け、マニュアル本体の検査（Task 6 の manual_en_ja、tests/tmp/manual）とも重ねない。
# LANG は manual_fixture_test.cmake が見本ごとに決めるので、tests.cmake は LANG=ja_JP.UTF-8 の一括の設定より後ろでこれを読む。
# 見本の good は --mosa を省き、既定（mosatest の隣の mosa）を通す。展開したパッケージで動かすのと同じ形（Task 9）。
add_executable(manual_parse_test tests/cpp/manual_parse_test.cpp)
target_include_directories(manual_parse_test PRIVATE tests/cpp)
target_link_libraries(manual_parse_test PRIVATE mosa_lib)
add_test(NAME manual_parse COMMAND $<TARGET_FILE:manual_parse_test>)
# 表と比べる処理系の一覧（Task 5 の修正 4 で manual_parse_test から分けた）。
add_executable(manual_lists_test tests/cpp/manual_lists_test.cpp)
target_include_directories(manual_lists_test PRIVATE tests/cpp)
target_link_libraries(manual_lists_test PRIVATE mosa_lib)
target_compile_definitions(manual_lists_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
add_test(NAME manual_lists COMMAND $<TARGET_FILE:manual_lists_test>)
# 生の HTML のタグ（Task 5 の修正 4 の追加で manual_parse_test から分けた）。
add_executable(manual_html_test tests/cpp/manual_html_test.cpp)
target_include_directories(manual_html_test PRIVATE tests/cpp)
target_link_libraries(manual_html_test PRIVATE mosa_lib)
add_test(NAME manual_html COMMAND $<TARGET_FILE:manual_html_test>)
set(manual_fixture_script ${CMAKE_SOURCE_DIR}/tests/cpp/manual_fixture_test.cmake)
set(manual_fixture_dir ${CMAKE_SOURCE_DIR}/tests/manual)
add_test(NAME manual_fixture_good COMMAND ${CMAKE_COMMAND} -DMOSATEST=$<TARGET_FILE:mosatest>
  -DDIR=${manual_fixture_dir}/good -DWORK=${CMAKE_SOURCE_DIR}/tests/tmp/manual_fixture_good -DLANG_VALUE=fr_FR.UTF-8
  -DCODE=0 "-DPATTERN_1=^manual: 10 checked, 2 skipped, 0 failed~$" -P ${manual_fixture_script})
add_test(NAME manual_fixture_bad_output COMMAND ${CMAKE_COMMAND} -DMOSATEST=$<TARGET_FILE:mosatest>
  -DMOSA=$<TARGET_FILE:mosa> -DDIR=${manual_fixture_dir}/bad-output
  -DWORK=${CMAKE_SOURCE_DIR}/tests/tmp/manual_fixture_bad_output -DLANG_VALUE=en_US.UTF-8 -DCODE=1
  "-DPATTERN_1=bad-output/en/01-sample\\.md:13 wrong-answer~expected:~\\?- X = 1\\.~X = 2\\.~actual:~\\?- X = 1\\.~X = 1\\.~"
  # 入力の無い -g だけの mosa は促しを出したまま待つ。読者にも見えるので、除かずに違いとして出す（修正 1）。
  "-DPATTERN_2=bad-output/en/01-sample\\.md:23 waiting-prompt~expected:~x~true\\.~actual:~x~true\\.~\\?- ~\\(no newline at end\\)~"
  "-DPATTERN_3=bad-output/ja/01-sample\\.md:23 waiting-prompt~"
  "-DPATTERN_4=~manual: 6 checked, 0 skipped, 3 failed~$" -P ${manual_fixture_script})
add_test(NAME manual_fixture_bad_ids COMMAND ${CMAKE_COMMAND} -DMOSATEST=$<TARGET_FILE:mosatest>
  -DMOSA=$<TARGET_FILE:mosa> -DDIR=${manual_fixture_dir}/bad-ids
  -DWORK=${CMAKE_SOURCE_DIR}/tests/tmp/manual_fixture_bad_ids -DLANG_VALUE=en_US.UTF-8 -DCODE=1
  "-DPATTERN_1=bad-ids/en/01-sample\\.md and [^~]*bad-ids/ja/01-sample\\.md: example ids differ \\(check:first skip:second / skip:second check:first\\)"
  "-DPATTERN_2=bad-ids/ja/02-only\\.md: missing"
  # id が同じでも、片方の版だけが skip: なら違う（修正 5）。
  "-DPATTERN_3=bad-ids/en/03-kind\\.md and [^~]*bad-ids/ja/03-kind\\.md: example ids differ \\(check:shown / skip:shown\\)" -P ${manual_fixture_script})
# 日本語の文面でも file:line: の形と件数の 1 行は英語と同じ（印の誤りは lib/messages_ja.pl の鍵）。
add_test(NAME manual_fixture_unmarked COMMAND ${CMAKE_COMMAND} -DMOSATEST=$<TARGET_FILE:mosatest>
  -DMOSA=$<TARGET_FILE:mosa> -DDIR=${manual_fixture_dir}/unmarked
  -DWORK=${CMAKE_SOURCE_DIR}/tests/tmp/manual_fixture_unmarked -DLANG_VALUE=ja_JP.UTF-8 -DCODE=1
  "-DPATTERN_1=unmarked/en/01-sample\\.md:5: " "-DPATTERN_2=unmarked/ja/01-sample\\.md:5: "
  "-DPATTERN_3=~manual: 0 checked, 0 skipped, 2 failed~$" -P ${manual_fixture_script})
# 述語の表（リリース 0.1.0 Task 5）。表が処理系に無いものと処理系が表に無いものを両方名指し、system は $ の補助を
# 数えず call/1..8 と halt/0,1 を数え、文面の表は一覧を持たず、片方の版だけの表は版の違いになる。
add_test(NAME manual_fixture_bad_preds COMMAND ${CMAKE_COMMAND} -DMOSATEST=$<TARGET_FILE:mosatest>
  -DMOSA=$<TARGET_FILE:mosa> -DDIR=${manual_fixture_dir}/bad-preds
  -DWORK=${CMAKE_SOURCE_DIR}/tests/tmp/manual_fixture_bad_preds -DLANG_VALUE=en_US.UTF-8 -DCODE=1
  "-DPATTERN_1=bad-preds/en/14-reference\\.md:16: table of between differs from Mosa \\(not in Mosa \\[nope/1\\], not in the table \\[succ/2\\]\\)~"
  "-DPATTERN_2=bad-preds/ja/14-reference\\.md:16: table of between differs from Mosa \\(not in Mosa \\[nope/1\\], not in the table \\[succ/2\\]\\)~"
  "-DPATTERN_3=bad-preds/en/14-reference\\.md:27: table of system differs from Mosa \\(not in Mosa \\[\\$call_cleanup/2 nope/0\\], not in the table \\[[^~]* =\\.\\./2 [^~]* call/1 call/2 call/3 call/4 call/5 call/6 call/7 call/8 halt/0 halt/1\\]\\)~"
  "-DPATTERN_4=bad-preds/en/14-reference\\.md:38: no predicate list for messages_en; a table names system or one of apply, between, dcg, format, japanese, lists, strings~"
  "-DPATTERN_5=bad-preds/en/15-tables\\.md and [^~]*bad-preds/ja/15-tables\\.md: predicate tables differ \\(between / \\)~"
  "-DPATTERN_6=~manual: 2 checked, 0 skipped, 7 failed~$" -P ${manual_fixture_script})
# 表と比べるライブラリは --mosa の隣の lib/ のもので、$MOSA_LIB は見ない。bad-preds/lib/between.pl は表の行と
# ちょうど同じ述語を公開するので、それを読めば between の表は誤って通る（修正 1）。
add_test(NAME manual_fixture_bad_preds_mosa_lib COMMAND ${CMAKE_COMMAND} -DMOSATEST=$<TARGET_FILE:mosatest>
  -DMOSA=$<TARGET_FILE:mosa> -DDIR=${manual_fixture_dir}/bad-preds
  -DMOSA_LIB_VALUE=${manual_fixture_dir}/bad-preds/lib
  -DWORK=${CMAKE_SOURCE_DIR}/tests/tmp/manual_fixture_bad_preds_mosa_lib -DLANG_VALUE=en_US.UTF-8 -DCODE=1
  "-DPATTERN_1=bad-preds/en/14-reference\\.md:16: table of between differs from Mosa \\(not in Mosa \\[nope/1\\], not in the table \\[succ/2\\]\\)~"
  "-DPATTERN_2=bad-preds/ja/14-reference\\.md:16: table of between differs from Mosa \\(not in Mosa \\[nope/1\\], not in the table \\[succ/2\\]\\)~"
  "-DPATTERN_3=~manual: 2 checked, 0 skipped, 7 failed~$" -P ${manual_fixture_script})
# シンボリックリンクの --mosa は実体に解いてから隣の lib/ を取る。リンクの隣の lib/ は bad-preds/lib の偽の between
# だけで、それと比べれば between の表は通り、知らない名前の一覧も between だけになる（修正 2）。Windows のリンクは
# 特権か開発者モードが要り、試験が走らせる環境の設定しだいになるので登録しない（解き方は executable_path と同じ）。
if(NOT WIN32)
  add_test(NAME manual_fixture_bad_preds_link COMMAND ${CMAKE_COMMAND} -DMOSATEST=$<TARGET_FILE:mosatest>
    -DMOSA=$<TARGET_FILE:mosa> -DDIR=${manual_fixture_dir}/bad-preds
    -DLINK=${CMAKE_SOURCE_DIR}/tests/tmp/manual_link -DLINK_LIB=${manual_fixture_dir}/bad-preds/lib
    -DWORK=${CMAKE_SOURCE_DIR}/tests/tmp/manual_fixture_bad_preds_link -DLANG_VALUE=en_US.UTF-8 -DCODE=1
    "-DPATTERN_1=bad-preds/en/14-reference\\.md:16: table of between differs from Mosa \\(not in Mosa \\[nope/1\\], not in the table \\[succ/2\\]\\)~"
    "-DPATTERN_2=bad-preds/en/14-reference\\.md:38: no predicate list for messages_en; a table names system or one of apply, between, dcg, format, japanese, lists, strings~"
    "-DPATTERN_3=~manual: 2 checked, 0 skipped, 7 failed~$" -P ${manual_fixture_script})
endif()
# あるが読めないライブラリは、その名前とパスと読み込みの誤りを出す。置き場は mosa と lib/ に bad-lib/lib を足したもの
# （修正 1 と 5）。
add_test(NAME manual_fixture_bad_lib COMMAND ${CMAKE_COMMAND} -DMOSATEST=$<TARGET_FILE:mosatest>
  -DMOSA=$<TARGET_FILE:mosa> -DDIR=${manual_fixture_dir}/bad-lib
  -DSTAGE=${CMAKE_SOURCE_DIR}/tests/tmp/manual_stage_bad_lib
  -DSTAGE_LIB=${manual_fixture_dir}/bad-lib/lib -DWORK=${CMAKE_SOURCE_DIR}/tests/tmp/manual_fixture_bad_lib
  -DLANG_VALUE=en_US.UTF-8 -DCODE=1
  "-DPATTERN_1=bad-lib/en/14-reference\\.md:17: library\\(broken\\) at [^~]*manual_stage_bad_lib/lib/broken\\.pl cannot be loaded: [^~]*broken\\.pl"
  "-DPATTERN_2=bad-lib/en/14-reference\\.md:26: library\\(nomodule\\) at [^~]*manual_stage_bad_lib/lib/nomodule\\.pl cannot be loaded: [^~]+"
  "-DPATTERN_3=bad-lib/en/14-reference\\.md:35: library\\(renamed\\) at [^~]* cannot be loaded: the file does not define the module renamed~"
  "-DPATTERN_4=bad-lib/ja/14-reference\\.md:16: library\\(broken\\) at "
  "-DPATTERN_5=bad-lib/ja/14-reference\\.md:34: library\\(renamed\\) at "
  "-DPATTERN_6=~manual: 2 checked, 0 skipped, 6 failed~$" -P ${manual_fixture_script})
# --all-tables はどの版にも system と、mosa の隣の lib/ のどのライブラリの表も求める。good には between しか無い
# （修正 3）。
add_test(NAME manual_fixture_all_tables COMMAND ${CMAKE_COMMAND} -DMOSATEST=$<TARGET_FILE:mosatest>
  -DMOSA=$<TARGET_FILE:mosa> -DDIR=${manual_fixture_dir}/good -DOPTIONS=--all-tables
  -DWORK=${CMAKE_SOURCE_DIR}/tests/tmp/manual_fixture_all_tables -DLANG_VALUE=en_US.UTF-8 -DCODE=1
  "-DPATTERN_1=good/en: no predicates table for system apply dcg format japanese lists strings~"
  "-DPATTERN_2=good/ja: no predicates table for system apply dcg format japanese lists strings~"
  "-DPATTERN_3=~manual: 10 checked, 2 skipped, 2 failed~$" -P ${manual_fixture_script})
# 組込の一覧は mosatest の中から取るので、版の違う --mosa とは比べずに 2 で止める。cmake --version は別の 1 行を
# 出す（修正 1）。
add_test(NAME manual_fixture_version COMMAND ${CMAKE_COMMAND} -DMOSATEST=$<TARGET_FILE:mosatest>
  -DMOSA=${CMAKE_COMMAND} -DDIR=${manual_fixture_dir}/good
  -DWORK=${CMAKE_SOURCE_DIR}/tests/tmp/manual_fixture_version -DLANG_VALUE=en_US.UTF-8 -DCODE=2
  "-DPATTERN_1=^mosatest: [^~]* --version printed cmake version [^~]*, not mosa [0-9.]+; give the mosa built with this mosatest~$"
  -P ${manual_fixture_script})
add_test(NAME manual_usage COMMAND ${CMAKE_COMMAND} -DMOSATEST=$<TARGET_FILE:mosatest> -DLANG_VALUE=en_US.UTF-8
  -DCODE=2 "-DPATTERN_1=mosatest --manual <directory>" -P ${manual_fixture_script})
# 章の無いマニュアルは何も確かめていないので、通ったことにせず 2 で止める（修正 8）。
add_test(NAME manual_fixture_empty COMMAND ${CMAKE_COMMAND} -DMOSATEST=$<TARGET_FILE:mosatest>
  -DMOSA=$<TARGET_FILE:mosa> -DDIR=${manual_fixture_dir}/empty
  -DWORK=${CMAKE_SOURCE_DIR}/tests/tmp/manual_fixture_empty -DLANG_VALUE=en_US.UTF-8 -DCODE=2
  "-DPATTERN_1=^mosatest: no check: examples in [^~]*tests/manual/empty~$" -P ${manual_fixture_script})
# マニュアル本体（リリース 0.1.0 Task 6）。読者が打つ例が書いてあるとおりに動くことを、人の目ではなく ctest で保つ。
# 英語版と日本語版の両方を動かす。--work は見本の場所と重ねない。--all-tables は第 14 章の表の全部を求める（Task 9）。
add_test(NAME manual_en_ja COMMAND $<TARGET_FILE:mosatest> --manual ${CMAKE_SOURCE_DIR}/docs/manual
  --mosa $<TARGET_FILE:mosa> --work ${CMAKE_SOURCE_DIR}/tests/tmp/manual --all-tables)
