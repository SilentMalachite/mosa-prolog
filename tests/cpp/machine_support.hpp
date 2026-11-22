#pragma once

// machine_test.cpp と machine_load_test.cpp が共に使う答えの字面。2 つは 1 つの machine_test の実行ファイルに入るので、
// 無名の名前空間に写しを 2 つ持つと、片方が使わない写しが -Werror で落ちる。

#include "mosa/machine.hpp"
#include "mosa/writer.hpp"

#include <string>

namespace machine_support {

inline std::string binding_text(mosa::Machine& machine, const mosa::Stop& stop) {
  return mosa::write_term(machine.heap(), machine.operators(),
                          machine.heap().deref(stop.bindings[0].variable), mosa::WriteOptions{}, {});
}

}  // namespace machine_support
