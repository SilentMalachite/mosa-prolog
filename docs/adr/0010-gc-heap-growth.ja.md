[English](0010-gc-heap-growth.md) | 日本語

状態: 承認済み（2026-10-30）

## 背景

負荷テストは `heap_limit` で止まる。`tak(18,12,6)` はヒープがあふれ、素朴な `loop(N)` は N が 40000 から 45000 の間であふれる。インタプリタの `extras` と trail も、バックトラックでしか縮まないので伸び続ける。
セルは実アドレスを持ち（ADR 0001/0002）、ゴール枠・継続・`redo_goal` はヒープの添字を持ち（ADR 0003/0009）、`Trail::slots` と `Trail::mark` は生ポインタである。ヒープが動くと、このすべてを書き換えることになる。
持ち主の要望は、GC、自動で伸びるヒープ、伸ばせないときの安全な停止、見える統計値である。合格の目安はメモリの絶対値ではなく、SPEC 第9節の完了条件にある頭打ちである。

## 決定

ヒープ: `Heap` は起動時に上限までの仮想アドレス空間を予約し、必要に応じて下から確保する（POSIX は `mmap`+`mprotect`、Windows は `VirtualAlloc` の `MEM_RESERVE`/`MEM_COMMIT`。どちらも `src/platform/`）。確保済みのセルは動かないので、ビルトインはステップの途中でもヒープを伸ばせる。上限は語数で、`Machine::create` の `heap_limit` 引数をそのまま使う（API は環境変数を読まない）。入口（`mosa` の `main` と生成する `main.cpp`）は `--heap-max`（MiB の整数。`mosa` のみ）→ `MOSA_HEAP_MAX`（MiB）→ 既定 4 GiB（2^29 語）の順で決める。Linux の OOM killer はメッセージを出す前にプロセスを殺すことがあるためである。起動時の確保量は min(2^20 語, 上限) とする。
安全点: GC は `run_loop` のステップとステップの間でだけ走り、ビルトインや生成関数の途中では走らない。このため、C++ のローカルが握る Cell は動かない。柔らかい容量 C（`gc_capacity`）は `limit - headroom` を超えず、`headroom = min(max(limit / 4, 65536), limit / 2)` 語とする。これで上限の前に必ず GC の機会がある。ステップの途中の確保は C を超えて上限まで進んでよい。安全点の検査は、閾値 `gc_threshold`（普段は C）との比較 1 回である。GC の後で生きているデータを L とする。L > 0.7·C なら C を 2 倍にする（上限まで）。上限でもなお L > 0.7·C なら `gc_threshold = max(C, L + (limit - L) / 2)` とする。ヒープが埋まりかけても安全点ごとに GC せず、最後は上限で止まる。極端に小さい上限（`Machine::create(8)`）は、今までどおり `heap_limit` で止まる。
回収: 印付けは明示的なスタックで行い（C++ の再帰なし）、1 語 1 ビットの表に付ける。語ごとに 2 ビットで種類（Cell、Raw、FrameIndex、ExtrasIndex）も持つ。Str の関数語、Float と IntBox の語、ゴール枠の語 0〜2 は Raw、`Ctrl::Native` 枠の語 0〜2 と 4 は Raw、語 5 以降は Cell とする（ADR 0009）。詰め直しは sliding で、`rank(i)` は i より下の印の付いた語の数であり、64 語の区画ごとの累計と `popcount` で引く。ヒープを下から 1 回走査して、印の付いた語を `rank(i)` へ写し、Cell のポインタ、枠の添字、extras の添字を書き換える。順序が保たれるので、選択点の `heap_mark` は `rank(mark)` になる。`extras` は、到達できる制御枠から指されている項目だけを順序を保って残す（別のビット表）。trail は、スロットに印が付いた項目だけを残して書き換え、`trail_mark` は旧位置より下に残った項目の数にする。
ルート: `ip`、`ball`、`bindings`、選択点スタックの各 `Entry`、`Trail::mark`、そして入れ子の C++ の持ち物を載せる登録簿 `Impl::gc_roots` である。入れ子の `run_loop` を回す場所（`findall/bagof/setof`、consult 中の指示、`with_output_to`、`lgt_copy` の道具）はすべて `RootScope` を持ち、Cell・ヒープの水位・ヒープの添字（findall の `VarIds`、読み込み器の `heap_base`、`extras_mark` を含む `Saved` と `Snap`）を登録して、抜けるときに外す。例外は portray フック（`src/builtins/write_pred.cpp` の `apply_portray`）だけである。ここは writer の再帰の途中で、C++ の枠と `Printer::assigned_` が生のアドレスを握っている。`GcInhibit` の数で GC を止め、writer を反復に書き直すとき（フェーズ7）に外す。
停止: 上限に当たったとき、確保を OS に断られたとき、`std::bad_alloc` のときは、ヒープを壊さずに今の問い合わせを止め、日英のメッセージを出す（ADR 0007 のカタログ）。REPL は続け、`mosac` の実行ファイルは 2 で終わる。`catch/3` では捕まえられない。今の `heap_limit`（`ResourceLimit`）と同じである。

## 捨てた案

GC のときに 2 倍の配列へ引っ越す案。セルが実アドレスなので全ポインタ・trail のスロット・添字を書き換えることになり、ステップの途中では伸ばせない（ステップを巻き戻す手立てが無い）。
入れ子の `run_loop` の中では GC を止める案。`findall` の中の重い計算がまたあふれ、直したい失敗がそのまま残る。
実行モデルを変えて、ただの連言で `Extra` を作らないようにする案。ADR 0003 に手が入り、GC と別の大きな変更になる。GC が `extras` を回収すれば足りる。

## 影響

承認のときに、SPEC 第6節（英日）の Heap と GC の行を、「`std::vector<Cell>` で上限まで確保する」から、予約したアドレス範囲を必要に応じて確保する形へ改め、`docs/iso-notes.md`（英日）にアトム表の GC は無く、アトムを作り続けるループでは表が伸びることを記す。ヒープ・`extras`・trail は頭打ちになり、選択点スタックは生きている選択点の分だけ伸びる。`statistics/2`、`garbage_collect/0`、ログの旗は後のタスクで入れる。承認の前にこの ADR のコードは書かない。
