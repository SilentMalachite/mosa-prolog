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

void push_else(Impl& machine, GoalRef goal, std::uint64_t cont, std::size_t cut, const EnvRef& env, int soft_id,
               std::uint32_t module) {
  Entry entry;
  entry.pick = Pick::ElseGoal;
  entry.else_goal = std::move(goal);
  entry.env = env;
  entry.cont = cont;
  entry.cut = cut;
  entry.soft_id = soft_id;
  entry.module = module;
  entry.heap_mark = machine.heap.size();
  entry.trail_mark = machine.trail.slots.size();
  push_frame(machine, std::move(entry));
}

// -> と *-> の then を待つ枠。条件が成功するとこの枠に来て then を動かす。
std::uint64_t push_commit(Impl& machine, Ctrl ctrl, GoalRef then, std::uint64_t cont, std::size_t cut,
                          const EnvRef& env, std::uint32_t module) {
  Extra commit;
  commit.ctrl = ctrl;
  commit.then = std::move(then);
  commit.env = env;
  commit.cont = cont;
  commit.cut = cut;
  commit.module = module;
  return push_extra(machine, std::move(commit));
}

}  // namespace

void trim_stack(Impl& machine, std::size_t boundary) {
  if (machine.stack.size() > boundary) {
    machine.stack.resize(boundary);
    prune_catch_log(machine);
  }
}

LaunchNext open_or(Impl& machine, const GoalRef& goal, std::uint64_t cont, std::size_t cut, const EnvRef& env,
                   std::uint32_t module) {
  const IrTerm& left = goal->args[0];
  if (left.tag == IrTag::IfThen) {
    const auto boundary = machine.stack.size();
    push_else(machine, sub_goal(goal, 1), cont, cut, env, 0, module);
    const GoalRef cond_then = sub_goal(goal, 0);
    const auto frame = push_commit(machine, Ctrl::Commit, sub_goal(cond_then, 1), cont, cut, env, module);
    machine.extras.back().trim_to = boundary;
    // 条件の中のカットは else と外側の選択点を捨てない（7.8.7）。
    return LaunchNext{sub_goal(cond_then, 0), frame, machine.stack.size()};
  }
  if (left.tag == IrTag::SoftIf) {
    const int soft_id = machine.next_soft++;
    const auto else_at = machine.stack.size();
    push_else(machine, sub_goal(goal, 1), cont, cut, env, soft_id, module);
    const GoalRef cond_then = sub_goal(goal, 0);
    const auto frame = push_commit(machine, Ctrl::SoftCommit, sub_goal(cond_then, 1), cont, cut, env, module);
    machine.extras.back().soft_id = soft_id;
    machine.extras.back().trim_to = else_at;
    // 条件のカットは *-> の外と else を捨てない。
    return LaunchNext{sub_goal(cond_then, 0), frame, machine.stack.size()};
  }
  push_else(machine, sub_goal(goal, 1), cont, cut, env, 0, module);
  return LaunchNext{sub_goal(goal, 0), cont, cut};
}

LaunchNext open_if_then(Impl& machine, const GoalRef& goal, std::uint64_t cont, std::size_t cut,
                        const EnvRef& env, std::uint32_t module) {
  const auto boundary = machine.stack.size();
  const auto frame = push_commit(machine, Ctrl::Commit, sub_goal(goal, 1), cont, cut, env, module);
  machine.extras.back().trim_to = boundary;
  return LaunchNext{sub_goal(goal, 0), frame, machine.stack.size()};
}

LaunchNext open_soft(Impl& machine, const GoalRef& goal, std::uint64_t cont, std::size_t cut, const EnvRef& env,
                     std::uint32_t module) {
  const auto frame = push_commit(machine, Ctrl::SoftCommit, sub_goal(goal, 1), cont, cut, env, module);
  return LaunchNext{sub_goal(goal, 0), frame, machine.stack.size()};
}

}
