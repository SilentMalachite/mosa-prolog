#pragma once
// write_pred.cpp と write_capture.cpp が分け合う内部の宣言。with_output_to/2 を別のファイルへ出した（800 行の上限と、
// フェーズ 14 の追加のため）ので、portray/1 の道（run_goal、OutputScope）と流れの検査を両方から使えるようにする。
// 本体は write_pred.cpp にある。移しただけで、振る舞いは変えない。

#include "../machine/internal.hpp"

#include <string_view>

namespace mosa::write_internal {

enum class SinkKind { Atom, Codes, Stream };
enum class Ran { Ok, Fail, Threw, Halted };

struct Sink {
  SinkKind kind = SinkKind::Stream;
  std::size_t index = 0;
  Cell slot{};
};

// 現在出力をその間だけ差し替える。ヒープの上限は C++ の例外（ResourceLimit）で run_goal を越えて抜けるので、
// 戻すのを後ろに書くと飛ばされる（レビュー H6）。owns なら捕獲用のメモリも閉じる。呼ぶ前の流れが中で閉じられたら、
// close/1 と同じく user_output へ戻す（8.11.6.1）。
class OutputScope {
 public:
  OutputScope(Impl& machine, std::size_t index, bool owns)
      : machine_(machine), saved_(machine.current_out), index_(index), owns_(owns) {
    machine.current_out = index;
  }
  OutputScope(const OutputScope&) = delete;
  OutputScope& operator=(const OutputScope&) = delete;
  ~OutputScope() {
    restore();
    if (!owns_) return;
    // 束縛の後か例外で抜けるときに来る。閉じたスロットは再利用しないので、捕獲した中身をここで手放す。
    machine_.streams[index_].closed = true;
    std::string{}.swap(machine_.streams[index_].bytes);
  }

  void restore() {
    if (restored_) return;
    restored_ = true;
    machine_.current_out = machine_.streams[saved_].closed ? 1 : saved_;
  }

 private:
  Impl& machine_;
  std::size_t saved_;
  std::size_t index_;
  bool owns_;
  bool restored_ = false;
};

bool raise_inst(Impl& machine, const Goal& goal);
bool raise_type(Impl& machine, const Goal& goal, std::string_view type, Cell culprit);
bool resolve_stream(Impl& machine, const Goal& goal, Cell term, std::size_t& index);
bool reject_closed(Impl& machine, const Goal& goal, std::size_t index, Cell culprit);
bool reject_output(Impl& machine, const Goal& goal, const Stream& stream, Cell culprit);
bool compound_named(const Impl& machine, Cell cell, std::string_view& name, Cell& arg);
std::size_t push_memory(Impl& machine);
Ran run_goal(Impl& machine, const GoalRef& goal, const EnvRef& env, std::uint32_t module);
// 流れの番号へ書き、位置と字数を進める。標準の流れは OS にも出す（write_pred.cpp の emit_text）。
void emit_to_stream(Impl& machine, std::size_t index, std::string_view text);

}  // namespace mosa::write_internal
