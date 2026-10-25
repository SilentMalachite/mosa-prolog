#include "internal.hpp"

#include <utility>

namespace mosa {
namespace {

std::uint64_t push_extra(Impl& machine, Extra extra) {
  machine.extras.push_back(std::move(extra));
  const auto at = machine.heap.allocate_words(kFrameHead);
  machine.heap.set_word(at, 0);
  machine.heap.set_word(at + 1, machine.extras.size() - 1);
  machine.heap.set_word(at + 2, 0);
  machine.heap.set_word(at + 3, kNoFrame);
  return at;
}

void push_else(Impl& machine, const IrTerm& goal, std::uint64_t cont, std::size_t cut,
               const std::vector<Cell>& env, int soft_id) {
  Entry entry;
  entry.pick = Pick::ElseGoal;
  entry.else_goal = goal;
  entry.env = env;
  entry.cont = cont;
  entry.cut = cut;
  entry.soft_id = soft_id;
  entry.heap_mark = machine.heap.size();
  entry.trail_mark = machine.trail.slots.size();
  push_frame(machine, std::move(entry));
}

}  // namespace

void trim_stack(Impl& machine, std::size_t boundary) {
  if (machine.stack.size() > boundary) machine.stack.resize(boundary);
}

void launch_or(Impl& machine, const IrTerm& goal, std::uint64_t cont, std::size_t cut,
               const std::vector<Cell>& env) {
  const IrTerm& left = goal.args[0];
  if (left.tag == IrTag::IfThen) {
    const auto boundary = machine.stack.size();
    push_else(machine, goal.args[1], cont, cut, env, 0);
    Extra commit;
    commit.ctrl = Ctrl::Commit;
    commit.then = left.args[1];
    commit.env = env;
    commit.cont = cont;
    commit.cut = cut;
    commit.trim_to = boundary;
    const auto frame = push_extra(machine, std::move(commit));
    // 条件の中のカットは else と外側の選択点を捨てない（7.8.7）。
    launch(machine, left.args[0], frame, machine.stack.size(), env);
    return;
  }
  if (left.tag == IrTag::SoftIf) {
    const int soft_id = machine.next_soft++;
    push_else(machine, goal.args[1], cont, cut, env, soft_id);
    Extra commit;
    commit.ctrl = Ctrl::SoftCommit;
    commit.then = left.args[1];
    commit.env = env;
    commit.cont = cont;
    commit.cut = cut;
    commit.soft_id = soft_id;
    const auto frame = push_extra(machine, std::move(commit));
    // 条件のカットは *-> の外と else を捨てない。
    launch(machine, left.args[0], frame, machine.stack.size(), env);
    return;
  }
  push_else(machine, goal.args[1], cont, cut, env, 0);
  launch(machine, left, cont, cut, env);
}

void launch_if_then(Impl& machine, const IrTerm& goal, std::uint64_t cont, std::size_t cut,
                    const std::vector<Cell>& env) {
  Extra commit;
  commit.ctrl = Ctrl::Commit;
  commit.then = goal.args[1];
  commit.env = env;
  commit.cont = cont;
  commit.cut = cut;
  commit.trim_to = machine.stack.size();
  const auto frame = push_extra(machine, std::move(commit));
  launch(machine, goal.args[0], frame, machine.stack.size(), env);
}

void launch_soft(Impl& machine, const IrTerm& goal, std::uint64_t cont, std::size_t cut,
                 const std::vector<Cell>& env) {
  Extra commit;
  commit.ctrl = Ctrl::SoftCommit;
  commit.then = goal.args[1];
  commit.env = env;
  commit.cont = cont;
  commit.cut = cut;
  const auto frame = push_extra(machine, std::move(commit));
  launch(machine, goal.args[0], frame, machine.stack.size(), env);
}

}
