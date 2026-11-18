// 規格 13211-1 に無い。with_output_to/2 は ADR 0005。最初の成功だけ取り、現在出力は必ず戻す。
// atom/1 と codes/1 のために作ったメモリは、成功・失敗・例外・停止のあと閉じる。渡された流れは閉じない。
// with_output_to(+sink, :goal)。sink は atom(A)、codes(Cs)、または流れ（別名を含む）。
//   instantiation_error, type_error(callable), type_error(atom), type_error(list),
//   domain_error(stream_or_alias), existence_error(stream),
//   permission_error(output, stream), permission_error(output, binary_stream)
// テスト: tests/pl/write.pl, tests/cpp/stream_test.cpp
// write_pred.cpp から分けた（ファイルの行数の上限のため）。移しただけで、振る舞いは変えない。

#include "write_internal.hpp"

#include "../machine/internal.hpp"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mosa {
using namespace write_internal;
namespace {

bool callable_cell(const Impl& machine, Cell cell) {
  cell = machine.heap.deref(cell);
  return cell.tag() == Tag::Atom || cell.tag() == Tag::Str || cell.tag() == Tag::Lis;
}

bool reject_goal(Impl& machine, const Goal& goal, Cell cell) {
  cell = machine.heap.deref(cell);
  if (machine.heap.unbound(cell)) return raise_inst(machine, goal);
  if (!callable_cell(machine, cell)) return raise_type(machine, goal, "callable", cell);
  const Compiled compiled = compile_goal(machine.heap, cell);
  if (!body_callable(*compiled.term)) return raise_type(machine, goal, "callable", cell);
  return raise_cyclic_goal(machine, compiled, goal.name, goal.arity);
}

int prepare_capture(Impl& machine, const Goal& goal, Cell cell, Sink& sink) {
  std::string_view name;
  Cell arg{};
  if (!compound_named(machine, cell, name, arg)) return 0;
  if (name != "atom" && name != "codes") return 0;
  arg = machine.heap.deref(arg);
  if (name == "atom") {
    if (!machine.heap.unbound(arg) && arg.tag() != Tag::Atom) {
      raise_type(machine, goal, "atom", arg);
      return 1;
    }
    sink.kind = SinkKind::Atom;
    sink.slot = arg;
    return 2;
  }
  if (!machine.heap.unbound(arg)) {
    const WalkedList walked = walk_list(machine, arg);
    if (walked.shape == ListShape::Partial) {
      raise_inst(machine, goal);
      return 1;
    }
    if (walked.shape != ListShape::Proper) {
      raise_type(machine, goal, "list", arg);
      return 1;
    }
  }
  sink.kind = SinkKind::Codes;
  sink.slot = arg;
  return 2;
}

bool prepare_with(Impl& machine, const Goal& goal, Sink& sink) {
  const Cell sink_cell = machine.heap.deref(goal.arg(machine, 0));
  if (machine.heap.unbound(sink_cell)) return raise_inst(machine, goal);
  if (reject_goal(machine, goal, goal.arg(machine, 1))) return true;
  const int captured = prepare_capture(machine, goal, sink_cell, sink);
  if (captured == 1) return true;
  if (captured == 2) return false;
  if (resolve_stream(machine, goal, sink_cell, sink.index)) return true;
  const Cell culprit = goal.arg(machine, 0);
  return reject_closed(machine, goal, sink.index, culprit) ||
         reject_output(machine, goal, machine.streams[sink.index], culprit);
}

void bind_capture(Impl& machine, const Goal& goal, const Sink& sink, std::size_t index) {
  const std::string& bytes = machine.streams[index].bytes;
  Cell value = machine.heap.atom(machine.atoms.intern(bytes));
  if (sink.kind == SinkKind::Codes) {
    const CodeText codes = decode_codes(bytes);
    std::vector<Cell> items;
    items.reserve(codes.codes.size());
    for (const char32_t code : codes.codes) items.push_back(machine.heap.integer(static_cast<std::int64_t>(code)));
    value = list_from_cells(machine, items);
  }
  if (!do_unify(machine, sink.slot, value)) {
    machine.failed = true;
    return;
  }
  machine.ip = goal.cont;
}

void finish_with(Impl& machine, const Goal& goal, const Sink& sink, std::size_t index, Ran ran) {
  if (ran == Ran::Ok) {
    if (sink.kind == SinkKind::Stream) machine.ip = goal.cont;
    else bind_capture(machine, goal, sink, index);
  }
}

void run_with(Impl& machine, const Goal& call) {
  Sink sink;
  if (prepare_with(machine, call, sink)) return;
  std::size_t index = sink.index;
  if (sink.kind != SinkKind::Stream) index = push_memory(machine);
  // 捕獲用のメモリは呼んだ側の流れではない。開いたまま残すと stream_property が拾い、その後も書き込める。
  // scope が抜けるとき（束縛の後、または例外で）閉じ、スロットは再利用しない。
  OutputScope scope(machine, index, sink.kind != SinkKind::Stream);
  const Compiled compiled = compile_goal(machine.heap, machine.heap.deref(call.arg(machine, 1)));
  // 入れ子の GC は枠とセルを動かす。run_goal の後は、登録した写し goal と sink.slot だけを使う。
  Goal goal = call;
  const RootScope roots(machine, [&goal, &sink](GcVisitor& visitor) {
    visitor.frame(goal.at);
    visitor.frame(goal.cont);
    visitor.cell(sink.slot);
  });
  const Ran ran = run_goal(machine, compiled.term, compiled.env, goal.module);
  scope.restore();
  finish_with(machine, goal, sink, index, ran);
}

}  // namespace

bool run_with_output(Impl& machine, const Goal& goal) {
  if (machine.atoms.name(goal.name) != "with_output_to" || goal.arity != 2) return false;
  run_with(machine, goal);
  return true;
}

}  // namespace mosa
