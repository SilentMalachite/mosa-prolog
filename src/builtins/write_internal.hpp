#pragma once
// write_pred.cpp、write_capture.cpp、write_portray.cpp が分け合う内部の宣言。with_output_to/2 を別のファイルへ出した
// （800 行の上限と、フェーズ 14 の追加のため）ので、portray/1 の道（run_goal、OutputScope）と流れの検査を両方から
// 使えるようにする。portray/1 の道と run_goal の本体は write_portray.cpp、ほかの本体は write_pred.cpp にある。
// 移しただけで、振る舞いは変えない。

#include "../machine/internal.hpp"

#include "mosa/writer.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
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
// 成功したゴールの束縛を残すか。with_output_to/2 は残し、portray/1 は字面を取った後に戻す（print/1 は @term）。
enum class Bindings : std::uint8_t { Keep, Undo };
Ran run_goal(Impl& machine, const GoalRef& goal, const EnvRef& env, std::uint32_t module, Bindings bindings);
// 流れの番号へ書き、位置と字数を進める。標準の流れは OS にも出す（write_pred.cpp の emit_text）。
void emit_to_stream(Impl& machine, std::size_t index, std::string_view text);

// print/1 が writer の掛け金 apply_portray に渡す状態。emit_written（write_pred.cpp）が作り、apply_portray
// （write_portray.cpp）が書き換える。
struct PortrayHook {
  Impl* machine = nullptr;
  std::uint32_t name = 0;
  std::size_t scratch = 0;
  bool have_scratch = false;
  bool stop = false;
  bool halted = false;
  // portray が下書きの流れ（print の間の現在出力）を閉じた。print は書かずに誤りで止まる（レビュー M-31）。
  bool scratch_closed = false;

  PortrayHook() = default;
  PortrayHook(const PortrayHook&) = delete;
  PortrayHook& operator=(const PortrayHook&) = delete;

  ~PortrayHook() {
    if (have_scratch && machine != nullptr) {
      machine->streams[scratch].closed = true;
      std::string{}.swap(machine->streams[scratch].bytes);
    }
  }
};

// user から見える portray/1 に節があれば、その名を name に置いて真（write_portray.cpp）。
bool portray_ready(Impl& machine, std::uint32_t& name);
// writer の掛け金。raw は PortrayHook（write_portray.cpp）。
WriteCut apply_portray(void* raw, Cell term, std::string& out, WriteHeld& held);

}  // namespace mosa::write_internal
