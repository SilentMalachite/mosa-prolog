#include "mosa/version.hpp"

#include "mosa/mosa.hpp"
#include "mosa/runtime.hpp"

namespace mosa {
std::string version_line() {
  return "mosa " + std::to_string(kVersionMajor) + "." + std::to_string(kVersionMinor) + "." +
         std::to_string(kVersionPatch) + " (phase 6)";
}
}
