フェーズ: 3 完了
進行中: なし
完了: Logtalk スイートの分母が 90% 以上。版は mosa 0.0.1 (phase 3)。往復の読み直しは文字列を残す
完了: mosatest --suite が docs/conformance.json（shields.io endpoint）も書く。README のバッジは main のそれを読み、CI の macOS ジョブが未コミットの差分を落とす
完了: 準拠スイートは tests.lgt ごとにカレントをスクラッチへ移す。{open(d1, write, S)} がリポジトリ直下に空ファイルを残さない（conformance_cwd_test）
完了: mosatest --suite は --report を受ける。省けるのはリポジトリ直下の tests/conformance だけ（suite_args_test）
次: フェーズ4のモジュール
未解決: iso-notes に残した解釈
未解決: mosa は引数より先に非端末の標準入力を EOF まで読むので、閉じない入力だと --version でも止まる
計画: docs/superpowers/plans/2026-10-26-phase3-exit.md
