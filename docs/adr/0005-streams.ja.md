[English](0005-streams.md) | 日本語

状態: 承認済み（2026-10-25）

## 背景

入出力の述語は、ファイルと標準入出力とメモリで同じカーソルを進める。
置き場を先に決めないと、open と get と write が別の形を持つ。
仕様第6節は抽象クラスと書く。公開ヘッダ mosa.hpp はフェーズ7まで増やさない。

## 決定

Stream は Impl の値のベクタである。抽象クラスにはしない。種別は File、Standard、Memory。データは Text か Binary。モードは Read、Write、Append。
バイト列とカーソルと閉じた印と reposition と別名を持つ。encoding の既定は utf8。型の既定は text。operators の直後に置く。
述語名は open_string/2 と with_output_to/2。テキストの改行は、ファイルもメモリも、どの OS でも U+000A 1 個。起動時の別名は user_input、user_output、user_error。

## 捨てた案

open_memory/2 という別名。仕様第8節の名前と揃えない。
C++ のテキストモードに改行を任せる案。Windows では CR LF になり、テストのバイトが OS で割れる。
mosa.hpp にストリーム操作を足す案。公開 API は create、consult、query、next、halt のままにする。

## 影響

この ADR の承認後に、ストリームのコードを書いてよい。
ファイルはバイナリで開く。consult は既に ios::binary である。nl は 0x0A を 1 バイト書く。
with_output_to/2 のゴールは今の文脈で走る。モジュールの包みはフェーズ4まで足さない。
