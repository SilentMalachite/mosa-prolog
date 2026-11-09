[English](0023-mosac-dist.md) | 日本語

状態: 承認済み（2026-11-10。持ち主が `mosac dist` を質問なしで作るよう頼んだことで承認）。SPEC 第 6 節は `mosac dist app.pl -o dist/` を「build と配布用のパッケージ（実行ファイル、LICENSE、README）」とし、形を zip / tar.gz とする。この ADR は中身と書庫の書き方を決める。

## 背景

利用者の機械に要るのは C++ コンパイラだけで（SPEC 第 6 節。CMake は任意）、Mosa 自身のリリース（`cmake/package.cmake`）のように `cmake -E tar` で固めることはできない。OS の `tar` も OS ごとに違う（GNU tar は zip を書かず、Windows の `tar.exe` は 10 の 1803 からしかない）。
実行ファイルは libmosa と翻訳済みのライブラリの単位を静的にリンクし、それらは Apache-2.0 である。ライセンスの第 4 節は、再頒布するバイナリにライセンスの写しと NOTICE を添えることを求める。
`hello.pl` の実行ファイルは macOS で 1.3 MB で、プログラムの名前は日本語でもよい（`こんにちは.pl`）。

## 決定

`mosac dist app.pl [-o DIR]` は `mosac build` の旗を受け、同じように build して、macOS と Linux では `DIR/<語幹>-<os>-<arch>.tar.gz`、Windows では `DIR/<語幹>-<os>-<arch>.zip` を書く（`DIR` の既定は `dist`、`<os>-<arch>` は実行時ライブラリの鍵と同じ）。書庫はフォルダー `<語幹>-<os>-<arch>/` を 1 つ持ち、その中に実行ファイル、Mosa の `LICENSE` と `NOTICE`（文面は mosac を作るときに mosac に入れる）、`README.txt`（カタログの鍵 `mosac_dist_readme` の英語、続けて日本語。動かし方は、名前にシェルが区切る字や引用符があれば引用し、Windows では PowerShell の `& '…'` にする）を置く。mosac は書庫のパスを標準出力に書く。
mosac は 2 つの形を標準の C++ で自分で書き（`src/codegen/archive.cpp`）、圧縮はしない。tar は ustar の項目で、名前が ASCII でないか 100 バイトの欄に入らなければ、前に PAX の `path` の記録を置き、全体を無圧縮の deflate ブロックの gzip にする。zip は無圧縮の項目に UTF-8 の名前の旗と Unix のモードを付ける。実行ファイルはモード 0755、ほかのファイルは 0644 で、どの項目の時刻も固めた時刻である。
同じ名前の書庫があれば、隣に新しいファイルを書いてから名前を変えて置き換え、書くのに失敗しても古い書庫を残す。前に残った一時ファイルは先に消し、そこにリンクがあってもたどらない。`DIR` が既にあるファイルなら、書けないときのメッセージで止め、実行ファイルの名前が `LICENSE`、`NOTICE`、`README.txt` と（ASCII の大文字小文字を除いて）同じになるプログラムは、展開すると片方が他方を上書きするので build の前に止める。

## 却下した案

`tar`、`cmake -E tar`、PowerShell の `Compress-Archive` を起こす。CMake は任意で、道具は OS と版で違い、Windows で ASCII でないパスを子プロセスに渡すには前にも別の道が要った（7779233）。
deflate で圧縮する。1.3 MB のファイルのために、テストで確かめるべきコードが増える。後の版が同じ形の中で足せる。`LICENSE` と `NOTICE` を実行時に mosac の隣から読む。ビルド木とインストールした木では置き場所が違い、ファイルが無ければ `dist` が止まる。
プログラム自身の `README` と `LICENSE` を書庫に入れる。名前が実行時ライブラリのファイルとぶつかり、どのファイルがプログラムのものかを mosac は知らない。利用者は後から書庫にファイルを足せる。

## 結果

書庫の大きさは中のファイルとほぼ同じになる。同じプログラムを 2 回固めた書庫は、時刻だけが違う。
テスト: `archive_test` が 2 つの形のバイト（CRC-32、tar の検査和、PAX の記録、gzip の末尾、zip のディレクトリ）を確かめ、`mosac_dist` が書庫を `cmake -E tar` で展開して実行ファイルを動かし、3 つの文書を比べる。日本語のファイル名でも確かめ、CI の 3 OS で流す。
メッセージ `mosac_dist_unavailable` は無くなる。SPEC 第 6 節、マニュアル第 13 章、使い方の文、README の状況、0.1.1 の CHANGELOG に `dist` を書く。
