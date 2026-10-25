#include "internal.hpp"

#include <fstream>
#include <sstream>
#include <string>

namespace mosa {

bool Machine::consult(std::string_view path) {
  std::ifstream input{std::string{path}, std::ios::binary};
  Impl& impl = engine_impl(*this);
  if (!input) {
    impl.diagnostic = std::string{path} + ": ファイルを開けません";
    impl.syntax_diag = false;
    return false;
  }
  std::ostringstream buffer;
  buffer << input.rdbuf();
  const LoadStatus status = load_source(impl, buffer.str());
  if (status.ok) {
    impl.diagnostic.clear();
    return true;
  }
  if (impl.syntax_diag) {
    impl.diagnostic = std::string{path} + ":" + std::to_string(impl.diag_line) + ":" +
                      std::to_string(impl.diag_column) + ": " + impl.diagnostic;
  } else {
    impl.diagnostic = std::string{path} + ": " + impl.diagnostic;
  }
  return false;
}

bool run_consult(Impl& machine, const Goal& goal) {
  const Cell path = machine.heap.deref(goal.arg(machine, 0));
  if (machine.heap.unbound(path)) {
    raise_formal(machine, formal_instantiation(machine), goal.name, goal.arity);
    return true;
  }
  if (path.tag() != Tag::Atom) {
    raise_formal(machine, formal_type(machine, "atom", path), goal.name, goal.arity);
    return true;
  }
  const auto text = std::string{machine.atoms.name(static_cast<std::uint32_t>(path.raw() >> 3))};
  std::ifstream input{text, std::ios::binary};
  if (!input) {
    machine.diagnostic = text + ": ファイルを開けません";
    machine.failed = true;
    return true;
  }
  std::ostringstream buffer;
  buffer << input.rdbuf();
  const LoadStatus status = load_source(machine, buffer.str());
  if (!status.ok) {
    if (machine.syntax_diag) {
      machine.diagnostic = text + ":" + std::to_string(machine.diag_line) + ":" +
                           std::to_string(machine.diag_column) + ": " + machine.diagnostic;
    } else {
      machine.diagnostic = text + ": " + machine.diagnostic;
    }
    machine.failed = true;
    return true;
  }
  machine.diagnostic.clear();
  machine.ip = goal.cont;
  return true;
}

}
