% 日本語の文面（ADR 0007）。message(Key, Format) の Format は format/2 の書式原子。
% 何も公開しない。C++ は messages_ja:message/2 と修飾して引くので、user の名前は増えない。
:- module(messages_ja, []).
:- use_module(library(format)).

message(goal_raised, '警告: ゴールは例外を送出しました: ~q').
message(heap_limit, '警告: ヒープの上限を超えました').
message(heap_limit_size, 'ヒープが上限（~w MiB）に達したので、問い合わせを止めました').
message(heap_reserve, 'ヒープ ~w MiB 分の番地空間を予約できませんでした。--heap-max か MOSA_HEAP_MAX で上限を小さくしてください').
message(gc_trace, '% GC ~w 回目: ~w KiB → ~w KiB（容量 ~w KiB、~w ms）').
message(help_japanese_variables, '日本語の名前の変数は _太郎 のように下線で始める。下線の無い 太郎 は原子である。').

% 構文エラー（ADR 0007）。理由の鍵は syntax_error(Key) の Key と同じ。
message(syntax_error, '構文エラー: 行 ~d 列 ~d: ~w ~q').
message(syntax_error_at_end, '構文エラー: 行 ~d 列 ~d: 入力の終わりで~w').
message(invalid_utf8, '不正な UTF-8').
message(illegal_character, '使えない文字').
message(unterminated_quoted, '引用が閉じていない').
message(unterminated_comment, 'コメントが閉じていない').
message(bad_escape, '不正なエスケープ').
message(integer_overflow, '整数が範囲を超えている').
message(unexpected_token, '予期しないトークン').
message(max_arity, '引数が多すぎる').
message(incomplete_term, '項が閉じていません').

% 読み込みと使い方の診断（ADR 0007、Task 12b）。
message(usage, 'mosa: 使い方: mosa [ファイル...] [-g ゴール] [-t halt] [--heap-max MiB] [-- 引数...]').
message(mosatest_usage, 'mosatest: 使い方: mosatest --roundtrip <ファイル>~nmosatest --suite <ディレクトリ> [--report <ファイル>]~nmosatest --bench <ファイル> [--count <N>] [--work <ディレクトリ>] [--cmake <パス>] [--report <ファイル>]~nmosatest --diff <ディレクトリ> [--report <ファイル>] [--work <ディレクトリ>] [--cmake <パス>] [--config <名前>]~n--report を省けるのは、リポジトリ直下で tests/conformance（--diff では tests/pl も）を渡したときだけです').
message(mosac_dist_unavailable, 'mosac: mosac dist はフェーズ7まで使えません').
message(mosac_usage, 'mosac: 使い方: mosac build <ファイル> [-o <ファイル>] [--interactive] [--debug] [--verbose] [--clean] [--cxx <コンパイラ>] [--runtime-source]~nmosac run <ファイル> [--debug] [--verbose] [--keep] [--clean] [--cxx <コンパイラ>] [--runtime-source] [-- <引数>...]~nmosac emit <ファイル> -o <ディレクトリ>~nmosac emit --library <lib/名前.pl> -o <ディレクトリ>~nmosac emit --units <ディレクトリ> -o <ディレクトリ>~nmosac toolchain [--cxx <コンパイラ>]').
message(mosac_cannot_write, 'mosac: 書き出せません: ~w').
message(mosac_output_is_source, 'mosac: 出力 ~w は原文のファイルそのもので、ビルドすると原文を上書きします~n-o で別の出力名を与えてください').
message(mosac_undefined_call, '~w を呼んでいるが、どこにも定義が無い（assertz/1 で実行時に作るなら :- dynamic(~w). と宣言する）').
message(mosac_export_undefined, '~w を export したが定義が無い').

% mosac toolchain（SPEC 第6節、計画 Task 8）。見つからないときの導入手順は OS ごとに 1 つ。
message(toolchain_compiler, 'コンパイラ: ~w').
message(toolchain_version, '版: ~w').
message(toolchain_cmake, 'CMake: ~w').
message(toolchain_cmake_missing, 'CMake: 見つかりません（mosac がコンパイラを直接呼びます）').
message(toolchain_runtime, '実行時ライブラリ: ~w').
message(toolchain_msvc_env, '環境: vcvarsall.bat から ~w 個の変数').
message(toolchain_not_found, 'mosac: C++20 のコンパイラが見つかりません').
message(toolchain_named_not_found, 'mosac: コンパイラ ~w（~w で指定）が見つからないか、起動できません').
message(toolchain_hint_macos, 'Xcode Command Line Tools を入れてください: xcode-select --install').
message(toolchain_hint_linux, 'C++ のコンパイラを入れてください: sudo apt install g++').
message(toolchain_msvc_needs_prompt, 'mosac: cl が Visual Studio の環境無し（INCLUDE が未設定）で動きます。mosac が vcvarsall.bat を呼ぶのはコンパイラを名指ししないときだけです~nVisual Studio の開発者用コマンドプロンプトを開くか、--cxx（と MOSA_CXX、CXX）を外して mosac に vcvarsall.bat を呼ばせてください').
message(toolchain_hint_windows, 'Visual Studio Build Tools を「C++ によるデスクトップ開発」のワークロード付きで入れてください: https://visualstudio.microsoft.com/ja/downloads/').

% mosac build と run（SPEC 第6節、計画 Task 9）。環境の問題は直し方まで書き、生成コードの誤りは内部エラーとして出す。
message(mosac_internal_error, '内部エラー: 生成コードがコンパイルできません。~w を添えて報告してください').
message(mosac_disk_full, 'mosac: ディスクの空きが足りず ~w に書けません~n空きを作ってからもう一度動かしてください。.mosac-cache の古い中間物は mosac build --clean で消せます').
message(mosac_no_permission, 'mosac: ~w に書く権限がありません~n書き込めるディレクトリで動かすか、-o で書き込める場所を指してください').
message(mosac_runtime_missing, 'mosac: 実行時ライブラリが見つかりません: ~w~nmosac toolchain で場所を確かめてください').
message(mosac_cannot_start, 'mosac: 起動できません: ~w').
message(mosac_verbose_run, 'mosac: 実行: ~w').
message(mosac_cache_hit, 'mosac: キャッシュを使います: ~w').
message(mosac_kept, 'mosac: 一時ディレクトリを残しました: ~w').
message(mosac_cmake_configure_failed, 'mosac: CMake の構成に失敗しました（ログ: ~w）~nCMake とコンパイラを確かめるか、MOSA_NO_CMAKE=1 を設定して mosac にコンパイラを直接呼ばせてください').
message(mosac_cache_symlink, 'mosac: ~w が記号リンクなので使いません~n消してからもう一度動かしてください').

% 同梱の実行時ライブラリと翻訳済みの lib/*.pl（SPEC 第6節、計画 Task 10）。
message(mosac_library_not_module, 'mosac: ~w は module/2 で始まっていないので、ライブラリの単位にできません').
message(mosac_catalog_missing, 'mosac: 文面の表 messages_ja と messages_en が読めません（~w）~nmosac の隣の lib/ か、環境変数 MOSA_LIB の場所を確かめてください').
message(mosac_runtime_abi_missing, 'mosac: このコンパイラ用の実行時ライブラリがありません: ~w~nmosac と同じコンパイラを使うか、mosac build --runtime-source で同梱の源から作ってください（結果は .mosac-cache に残ります）').
message(mosac_unit_missing, 'mosac: 翻訳済みのライブラリ ~w が見つかりません~nmosac toolchain で実行時ライブラリの場所を確かめてください').
message(mosac_runtime_source_missing, 'mosac: 同梱の実行時ライブラリの源が見つかりません: ~w').
message(mosac_runtime_source_needs_cmake, 'mosac: --runtime-source には CMake 3.25 以上が要ります（mosac toolchain で確かめてください）').
message(mosac_runtime_building, 'mosac: 実行時ライブラリを同梱の源から作ります: ~w').
message(mosac_runtime_build_failed, 'mosac: 実行時ライブラリを源から作れませんでした（ログ: ~w）~nコンパイラと CMake を確かめてください').
message(mosac_runtime_cached, 'mosac: 源から作った実行時ライブラリを使い回します: ~w').
message(mosac_verbose_units, 'mosac: 結ぶ翻訳済みライブラリ: ~w').
message(mosac_verbose_source_only, 'mosac: 原文だけを同梱するライブラリ（mosac の隣の lib/ と違う版なので IR のまま動く）: ~w').
message(mosac_duplicate_key, 'mosac: ~w と ~w は同じ名前 ~w で埋め込まれます。実行ファイルの中で区別できないので、どちらかのファイル名を変えてください').

message(cannot_open_file, '~w: ファイルを開けません').
message(file_cycle, '~w: ファイルが循環しています').
message(clauses_apart, '述語 ~w の節が離れている').
message(predicate_running, '実行中の述語は置き換えられない').
message(directive_unsupported, 'この指示はフェーズ2では使えない: ~w').
message(directive_failed, 'この指示のゴールは失敗した: ~w').
message(not_predicate_indicator, '述語指示子ではありません').
message(static_to_dynamic, '静的な述語は動的にできない').
message(op_bad_priority, '優先度が範囲外です').
message(op_bad_specifier, '演算子の指定子が不正です').
message(op_modify_comma, 'コンマの演算子は変更できません').
message(op_create_empty, '空リストや空の中括弧は演算子にできません').
message(op_infix_and_postfix, '中置と後置は同時に宣言できません').
message(op_create_bar, 'この縦棒は演算子にできません').
message(op_name_not_atom, '演算子名がアトムではありません').
message(op_failed, '演算子を宣言できません').

% mosatest --bench（計画 Task 6）。時間はミリ秒、比はインタプリタの時間を翻訳の時間で割った値。
message(bench_header, '~w: bench(~w)、~w 回の中央値').
message(bench_interpreter, 'インタプリタ: ~w ms').
message(bench_translated, '翻訳: ~w ms').
message(bench_ratio, '比: ~w').
message(bench_cannot_start, 'mosatest: ~w を起こせません').
message(bench_not_found, 'mosatest: ~w が見つかりません').
message(bench_run_failed, 'mosatest: ~w が終了コード ~w で終わりました').
message(bench_step_failed, 'mosatest: ~w が終了コード ~w で失敗しました（出力は ~w）').
message(bench_cannot_write, 'mosatest: ~w に書けません').
message(diff_usage, '使い方: ~w <ディレクトリ> [--report <ファイル>]').
message(diff_no_files, 'mosatest: ~w に試験のファイルがありません').
message(diff_not_emitted, '~w: この実行ファイルには翻訳が入っていません').
message(diff_cannot_read, '~w: 組み立てられません: ~w').
message(diff_source_changed, '~w: mosac emit --units のあとで原文が変わりました').
message(diff_mismatch, '~w ~w: 出力が違います').
message(diff_interpreter, '  インタプリタ: ~w').
message(diff_translated, '  翻訳:         ~w').
message(diff_file, '~w: 比較 ~w 件、一致 ~w 件、翻訳したゴール ~w 件、付けた述語 ~w/~w').
message(diff_not_attached, '~w: 翻訳した述語のうち付いたのは ~w/~w 個（指紋の不一致）').
message(diff_no_self, 'mosatest: 自分の場所が分かりません').
message(diff_total, '一致 ~w/~w、翻訳した述語 ~w/~w（~w%）、違い ~w 件').
message(mosac_units_skipped, 'mosac: ~w は翻訳しません: ~w').
