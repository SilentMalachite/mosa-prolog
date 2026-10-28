#pragma once

// ゴール枠は項ではない。選択点と catch は同じスタックに積む。

#include "mosa/atom.hpp"
#include "mosa/cell.hpp"
#include "mosa/heap.hpp"
#include "mosa/operator_table.hpp"
#include "mosa/unify.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace mosa {

enum class StopKind { Success, Failure, Exception, Halt };

struct Binding {
  std::string name;
  Cell variable;
};

struct Stop {
  StopKind kind = StopKind::Failure;
  int halt_code = 0;
  Cell exception{};
  std::vector<Binding> bindings;
  bool more = false;
};

struct LoadStatus {
  bool ok = false;
  std::string message;
};

class Machine {
 public:
  struct Impl;

  [[nodiscard]] static std::unique_ptr<Machine> create(std::size_t heap_limit = std::size_t{1} << 20);
  ~Machine();

  Machine(const Machine&) = delete;
  Machine& operator=(const Machine&) = delete;

  [[nodiscard]] AtomTable& atoms();
  [[nodiscard]] Heap& heap();
  [[nodiscard]] const Heap& heap() const;
  [[nodiscard]] OperatorTable& operators();
  [[nodiscard]] const OperatorTable& operators() const;
  [[nodiscard]] LoadStatus load_program(std::string_view source);
  [[nodiscard]] Stop query(std::string_view goal_text);
  [[nodiscard]] Stop next();
  void halt(int code);
  [[nodiscard]] std::size_t choice_depth() const;
  [[nodiscard]] std::string_view diagnostic() const;
  [[nodiscard]] bool consult(std::string_view path);

  explicit Machine(std::size_t heap_limit);

 private:
  friend Impl& engine_impl(Machine& machine);
  friend const Impl& engine_impl(const Machine& machine);

  std::unique_ptr<Impl> impl_;
};

[[nodiscard]] std::string format_answer(const Machine& machine, const Stop& stop);

}
