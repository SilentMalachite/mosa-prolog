#pragma once

// ゴール枠は項ではない。選択点と catch は同じスタックに積む。

#include "mosa/atom.hpp"
#include "mosa/cell.hpp"
#include "mosa/heap.hpp"
#include "mosa/operator_table.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace mosa {

// Abort はデバッガの a で止まった問い合わせ（ADR 0028）。halt と同じく catch/3 を通らない。halt と違い、止まる前に
// setup_call_cleanup/3 の後始末を、例外のときと同じ束縛で動かす（2026-11-18 の持ち主の決定 A と B）。
enum class StopKind { Success, Failure, Exception, Halt, Abort };

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

  [[nodiscard]] static std::unique_ptr<Machine> create(std::size_t heap_limit = kDefaultHeapMax);
  ~Machine();

  Machine(const Machine&) = delete;
  Machine& operator=(const Machine&) = delete;

  [[nodiscard]] AtomTable& atoms();
  [[nodiscard]] const AtomTable& atoms() const;
  [[nodiscard]] Heap& heap();
  [[nodiscard]] const Heap& heap() const;
  [[nodiscard]] OperatorTable& operators();
  [[nodiscard]] const OperatorTable& operators() const;
  [[nodiscard]] LoadStatus load_program(std::string_view source);
  // 返した Stop のセル（bindings・exception）は、この Machine の次の呼び出しまでしか有効でない。
  // GC（ADR 0010）はヒープを選択点より下まで詰めるので、次の next()・query()・load_program()・consult() の後は
  // 古い番地になる。続けて使う値は、次の呼び出しの前に字面か C++ の値へ写しておく。
  [[nodiscard]] Stop query(std::string_view goal_text);
  [[nodiscard]] Stop next();
  void halt(int code);
  [[nodiscard]] std::size_t choice_depth() const;
  [[nodiscard]] std::string_view diagnostic() const;
  // halt/0,1 で止まった読み込みは偽を返し、diagnostic() は空にする（失敗ではないので理由が無い）。load_program も同じ。
  // ヒープの上限で ResourceLimit を投げて抜けるときも、diagnostic() は空にする（理由は例外が持つ）。
  [[nodiscard]] bool consult(std::string_view path);

  explicit Machine(std::size_t heap_limit);

 private:
  friend Impl& engine_impl(Machine& machine);
  friend const Impl& engine_impl(const Machine& machine);

  std::unique_ptr<Impl> impl_;
};

[[nodiscard]] std::string format_answer(const Machine& machine, const Stop& stop);

}
