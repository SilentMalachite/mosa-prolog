// 実行ファイルの場所と環境変数（SPEC 第5節の library(...) の探索）。OS ごとの分岐はここだけに書く。
#include "mosa/exe_path.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <system_error>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

namespace mosa {

namespace {

// シンボリックリンク越しに起動しても、隣の lib/ は実体の隣を見る。
[[maybe_unused]] std::optional<std::filesystem::path> resolved(const std::filesystem::path& path) {
  std::error_code error;
  const auto canonical = std::filesystem::weakly_canonical(path, error);
  if (error) return path;
  return canonical;
}

}  // namespace

#if defined(_WIN32)

std::optional<std::filesystem::path> executable_path() {
  // 長いパスに備え、切り詰められたら倍にして取り直す。
  std::wstring buffer(260, L'\0');
  while (true) {
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0) return std::nullopt;
    if (length < buffer.size()) {
      buffer.resize(length);
      return resolved(std::filesystem::path{buffer});
    }
    buffer.resize(buffer.size() * 2);
  }
}

// 値は UTF-16 で読む。std::getenv は ANSI コードページの文字を返し、日本語のパスを壊す。
std::optional<std::filesystem::path> environment_path(std::string_view name) {
  // 変数名は ASCII だけを使う。1 バイトずつ符号なしで広げる。
  std::wstring wide;
  for (const char letter : name) wide.push_back(static_cast<wchar_t>(static_cast<unsigned char>(letter)));
  const DWORD size = GetEnvironmentVariableW(wide.c_str(), nullptr, 0);
  if (size <= 1) return std::nullopt;
  std::wstring value(size, L'\0');
  const DWORD length = GetEnvironmentVariableW(wide.c_str(), value.data(), size);
  if (length == 0 || length >= size) return std::nullopt;
  value.resize(length);
  return std::filesystem::path{value};
}

#else

#if defined(__APPLE__)
std::optional<std::filesystem::path> executable_path() {
  std::uint32_t size = 0;
  _NSGetExecutablePath(nullptr, &size);
  std::string buffer(size, '\0');
  if (_NSGetExecutablePath(buffer.data(), &size) != 0) return std::nullopt;
  buffer.resize(std::strlen(buffer.c_str()));
  return resolved(std::filesystem::path{buffer});
}
#elif defined(__linux__)
std::optional<std::filesystem::path> executable_path() {
  std::error_code error;
  const auto path = std::filesystem::read_symlink("/proc/self/exe", error);
  if (error) return std::nullopt;
  return resolved(path);
}
#else
std::optional<std::filesystem::path> executable_path() { return std::nullopt; }
#endif

std::optional<std::filesystem::path> environment_path(std::string_view name) {
  const char* value = std::getenv(std::string{name}.c_str());
  if (value == nullptr || *value == '\0') return std::nullopt;
  return std::filesystem::path{std::string{value}};
}

#endif

}
