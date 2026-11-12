[English](0025-bignum-box.md) | 日本語

状態: 提案（2026-11-12。持ち主がフェーズ 10 を設けた。決定 A。コードの前に持ち主の承認を待つ）。SPEC 第 9 節はフェーズ 10 を定める。この ADR は int64 の外の整数のヒープ上の形と、IR、索引、`mosac` がそれを運ぶ方法を決める。

## 背景

Cell は int64 を INT（61 ビットの即値）か IntBox で持ち、タグ 6（BIG）は予約してある（ADR 0001、0002）。フェーズ 10 は任意の大きさの整数を足すので、ヒープ、IR、節の索引、生成する C++ での形を、コードが頼る前に固定する。
GC（ADR 0010）と複製の道（assert、findall、`sol::CopyKind`、`ExportedTerm`）はどれも項を歩くので、最初の語から大きさが分からない形だと、それぞれに新しい規則が要る。
int64 に収まる値は INT か IntBox のままにし、int64 だけを使うプログラムは今と同じ道と費用で動く。

## 決定

箱: タグ 6 の Cell は頭の語 `(n << 4) | (negative << 3)`（下位 3 ビットは 0）を指し、その後に絶対値の 64 ビットの語が n 個（下位が先、最上位の語は 0 でない）続く。値は必ず int64 の外にある。GC は n+1 語を Raw として印を付け、移す。
IR: `IrTag::Big` を足す。`integer` は負なら 1、正なら 0。`args` は絶対値の語を下位から順に `IrTag::Int` の子として持つ（`integer` に `std::bit_cast<std::int64_t>(word)`）。`ExportedTerm::Kind::Big` と `sol::CopyKind::Big` も同じ持ち方にする（子の節点を Int の種類にする）。索引: `KeyKind::Big` を足し、鍵は `big_key(negative, magnitude)`（FNV-1a）で `Clause::key_int` の欄に入れる。`mosac` は定数を `static constexpr std::array<std::uint64_t, N>` と `rt::put_big` で出す。

## 却下した案

`IrTerm` に値の欄を足す: まれな場合のために、すべての節のすべての節点が大きくなる。
節ごとの定数表: 実行時に訳すゴールは節を持たない。
2 の補数の語で持つ: 符号と絶対値に比べて、積と商が書きにくい。

## 結果

`max_integer` と `min_integer` は int64 の境界のまま（ADR 0001 に 1 行足す。iso-notes が「A か B か」を問う）。ADR 0002 には、Big の箱は頭の語を持ち、項として走査しないことを 1 行足す。
実装はフェーズ 10 の計画のタスク 3〜8（受ける側が先、作る側が後）。`bounded` はタスク 6 で算術とともに `false` になる。ADR 0001 の「BIG は予約」は「BIG を使う」になる。
テスト: `tests/pl/bigint.pl`、箱と複製と索引の C++ テスト、`MOSA_GC_STRESS` での実行、`mosatest --diff`。
