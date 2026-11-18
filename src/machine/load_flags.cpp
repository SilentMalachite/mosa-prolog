// 読み込みの前の旗の退避と戻し（ADR 0017）。load.cpp が 800 行に近づいたので分けた（敵対的レビュー、2026-11-19。
// skip の箱の番号を旗の組に足す前）。投げる誤りは無い。テスト: tests/pl/debugger.pl（dbg_rollback_*）、
//   tests/cpp/debugger_test.cpp（check_directive_nodebug_rollback）
#include "load_internal.hpp"

namespace mosa {

SavedFlags save_flags(const Impl& machine) {
  return SavedFlags{machine.quotes,
                    machine.unknown_fail,
                    machine.float_overflow_error,
                    machine.float_zero_div_error,
                    machine.float_undefined_error,
                    machine.char_conv.enabled,
                    machine.utf8_replace,
                    machine.debug_on,
                    machine.debug.mode,
                    machine.debug.leashed,
                    machine.japanese_punct,
                    machine.char_conv.map,
                    machine.main_goal};
}

void restore_flags(Impl& machine, const SavedFlags& saved) {
  machine.quotes = saved.quotes;
  machine.unknown_fail = saved.unknown_fail;
  machine.float_overflow_error = saved.float_overflow_error;
  machine.float_zero_div_error = saved.float_zero_div_error;
  machine.float_undefined_error = saved.float_undefined_error;
  machine.char_conv.enabled = saved.char_conv_on;
  machine.utf8_replace = saved.utf8_replace;
  // 入り切りと進み方は組で戻す。片方だけ戻すと、後の spy/1 が creep で動き出す（ADR 0028）。
  // 止まるポートの組も同じ組で戻す（持ち主の決定 B）。読み込みの中のポートで打った n と l もこの組を変えるので、
  // それも戻る（CHANGELOG の既知の制限）。ポートのコマンドは leash を変えないので、止まるポートの組は対話では変わらない。
  machine.debug_on = saved.debug_on;
  machine.debug.mode = saved.debug_mode;
  machine.debug.leashed = saved.debug_leashed;
  machine.japanese_punct = saved.japanese_punct;
  machine.char_conv.map = saved.char_map;
  machine.main_goal = saved.main_goal;
}

// SPEC 第5節。この 2 つは字句の旗なので、ファイルの書き手の選択を読み手のファイルへ持ち出さない。
// 他の旗は実行の旗なので、成功した読み込みの後も残す。
void restore_file_flags(Impl& machine, const SavedFlags& saved) {
  machine.quotes = saved.quotes;
  machine.japanese_punct = saved.japanese_punct;
}


}  // namespace mosa
