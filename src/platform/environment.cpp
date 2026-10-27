#include "environment.hpp"

#include <string>

#ifdef _WIN32
#include <stdlib.h>
#else
#include <cstdlib>
#endif

namespace mosa {

std::string environment_value(const char* name) {
#ifdef _WIN32
  std::size_t length = 0;
  getenv_s(&length, nullptr, 0, name);
  if (length <= 1) return {};
  std::string buffer(length, '\0');
  if (getenv_s(&length, buffer.data(), buffer.size(), name) != 0) return {};
  return std::string{buffer.c_str()};
#else
  const char* value = std::getenv(name);
  if (value == nullptr) return {};
  return value;
#endif
}

bool set_environment(const char* name, const char* value) {
#ifdef _WIN32
  return _putenv_s(name, value) == 0;
#else
  return setenv(name, value, 1) == 0;
#endif
}

void unset_environment(const char* name) {
#ifdef _WIN32
  _putenv_s(name, "");
#else
  unsetenv(name);
#endif
}

}  // namespace mosa
