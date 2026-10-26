# Mosa Prolog

ISO/IEC 13211 に沿った Prolog 処理系です。

## ビルド

```
cmake --preset macos-clang
cmake --build --preset macos-clang
ctest --preset macos-clang
```

## 日本語の変数

`親(太郎, 一郎).` の `太郎` はアトムです。日本語名の変数は `_太郎` のように下線で始めます。詳細は `docs/SPEC.md` 第4節です。

## 準拠率

未計測（フェーズ3で `docs/conformance.md` を自動生成する）

## ライセンス

Apache-2.0 です。本文は `LICENSE`、著作権表示は `NOTICE` にあります。`tests/conformance/logtalk/` も同じライセンスで、Logtalk の著作権表示はそこの `NOTICE.txt` にあります。
