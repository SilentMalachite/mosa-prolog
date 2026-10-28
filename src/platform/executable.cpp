#include "executable.hpp"

#include <string>
#include <system_error>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>

#include <cstdint>
#else
#include <unistd.h>
#endif

namespace mosa {
namespace {

#ifdef _WIN32

std::filesystem::path executable_path_os() {
  std::wstring buffer(260, L'\0');
  for (;;) {
    const auto size = static_cast<DWORD>(buffer.size());
    const DWORD wrote = GetModuleFileNameW(nullptr, buffer.data(), size);
    if (wrote == 0) return {};
    if (wrote < size) {
      buffer.resize(wrote);
      return std::filesystem::path{buffer};
    }
    buffer.resize(buffer.size() * 2);
  }
}

#elif defined(__APPLE__)

std::filesystem::path executable_path_os() {
  std::uint32_t size = 1024;
  std::string buffer(size, '\0');
  if (_NSGetExecutablePath(buffer.data(), &size) != 0) {
    buffer.assign(size, '\0');
    if (_NSGetExecutablePath(buffer.data(), &size) != 0) return {};
  }
  return std::filesystem::path{buffer.c_str()};
}

#else

std::filesystem::path executable_path_os() {
  std::string buffer(256, '\0');
  for (;;) {
    const auto wrote = ::readlink("/proc/self/exe", buffer.data(), buffer.size());
    if (wrote < 0) return {};
    if (static_cast<std::size_t>(wrote) < buffer.size()) {
      buffer.resize(static_cast<std::size_t>(wrote));
      return std::filesystem::path{buffer};
    }
    buffer.resize(buffer.size() * 2);
  }
}

#endif

}  // namespace

// macOS の _NSGetExecutablePath はシンボリックリンクのパスを返す。lib/ は実体の隣にあるので解決する。
std::filesystem::path executable_path() {
  const auto path = executable_path_os();
  if (path.empty()) return path;
  std::error_code error;
  auto resolved = std::filesystem::weakly_canonical(path, error);
  if (error) return path;
  return resolved;
}

}  // namespace mosa
