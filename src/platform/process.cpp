// 子プロセスの起動と待ち合わせ（計画 Task 6、mosatest --bench。環境の受け渡しは Task 9）。OS ごとの分岐はここだけに書く。
// POSIX は posix_spawnp、Windows は CreateProcessW。どちらも PATH から探し、終わるまで待って終了コードを返す。
// 時間はここで測らない。呼び手（src/machine/bench.cpp）が起動から戻りまでを測る。
#include "mosa/process.hpp"

#include "mosa/path_text.hpp"

#include <string>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
extern "C" char** environ;
#endif

namespace mosa {

#if defined(_WIN32)

namespace {

// CommandLineToArgvW の逆。空白か " を含む引数は " で囲み、" の前と末尾の \ は倍にする。
std::wstring quoted(const std::wstring& arg) {
  if (!arg.empty() && arg.find_first_of(L" \t\"") == std::wstring::npos) return arg;
  std::wstring out = L"\"";
  std::size_t backslashes = 0;
  for (const wchar_t unit : arg) {
    if (unit == L'\\') {
      ++backslashes;
      continue;
    }
    out.append(unit == L'"' ? backslashes * 2 + 1 : backslashes, L'\\');
    backslashes = 0;
    out += unit;
  }
  out.append(backslashes * 2, L'\\');
  return out + L"\"";
}

// CreateProcessW の環境の塊。NAME=VALUE を NUL で区切り、最後にもう 1 つ NUL を置く。
std::wstring environment_block(std::span<const std::pair<std::string, std::string>> env) {
  std::wstring block;
  for (const auto& [name, value] : env) {
    block += path_from_utf8(name + "=" + value).native();
    block += L'\0';
  }
  block += L'\0';
  return block;
}

std::optional<int> wait_for(PROCESS_INFORMATION& process) {
  WaitForSingleObject(process.hProcess, INFINITE);
  DWORD code = 0;
  const BOOL got = GetExitCodeProcess(process.hProcess, &code);
  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
  if (got == 0) return std::nullopt;
  return static_cast<int>(code);
}

}  // namespace

std::optional<int> run_process(std::span<const std::string> argv, const std::filesystem::path& log,
                               std::span<const std::pair<std::string, std::string>> env) {
  if (argv.empty()) return std::nullopt;
  // 引数は UTF-8 で受ける。path を通すと UTF-16 になる（path_text.hpp と同じ道）。
  std::wstring line;
  for (const std::string& arg : argv) {
    if (!line.empty()) line += L' ';
    line += quoted(path_from_utf8(arg).native());
  }
  STARTUPINFOW info{};
  info.cb = static_cast<DWORD>(sizeof(info));
  HANDLE file = INVALID_HANDLE_VALUE;
  if (!log.empty()) {
    SECURITY_ATTRIBUTES inherit{};
    inherit.nLength = static_cast<DWORD>(sizeof(inherit));
    inherit.bInheritHandle = TRUE;
    file = CreateFileW(log.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &inherit, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                       nullptr);
    if (file == INVALID_HANDLE_VALUE) return std::nullopt;
    info.dwFlags = STARTF_USESTDHANDLES;
    info.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    info.hStdOutput = file;
    info.hStdError = file;
  }
  PROCESS_INFORMATION process{};
  std::wstring block = environment_block(env);
  const BOOL started = CreateProcessW(nullptr, line.data(), nullptr, nullptr, TRUE, CREATE_UNICODE_ENVIRONMENT,
                                      env.empty() ? nullptr : block.data(), nullptr, &info, &process);
  if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
  if (started == 0) return std::nullopt;
  return wait_for(process);
}

#else

namespace {

// 標準出力をログに開き、標準エラーもそこへ重ねる。
bool redirect(posix_spawn_file_actions_t& actions, const std::filesystem::path& log) {
  if (posix_spawn_file_actions_addopen(&actions, 1, log.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644) != 0) return false;
  return posix_spawn_file_actions_adddup2(&actions, 1, 2) == 0;
}

// posix_spawnp は書き込める char* の並びを取る。owned の写しを指す。
std::vector<char*> pointers_to(std::vector<std::string>& owned) {
  std::vector<char*> pointers;
  for (std::string& item : owned) pointers.push_back(item.data());
  pointers.push_back(nullptr);
  return pointers;
}

}  // namespace

std::optional<int> run_process(std::span<const std::string> argv, const std::filesystem::path& log,
                               std::span<const std::pair<std::string, std::string>> env) {
  if (argv.empty()) return std::nullopt;
  std::vector<std::string> owned(argv.begin(), argv.end());
  std::vector<char*> pointers = pointers_to(owned);
  std::vector<std::string> entries;
  for (const auto& [name, value] : env) entries.push_back(name + "=" + value);
  std::vector<char*> env_pointers = pointers_to(entries);
  char** child_env = env.empty() ? environ : env_pointers.data();
  posix_spawn_file_actions_t actions;
  if (posix_spawn_file_actions_init(&actions) != 0) return std::nullopt;
  pid_t pid = 0;
  int spawned = -1;
  if (log.empty() || redirect(actions, log)) {
    spawned = posix_spawnp(&pid, pointers.front(), &actions, nullptr, pointers.data(), child_env);
  }
  posix_spawn_file_actions_destroy(&actions);
  if (spawned != 0) return std::nullopt;
  int status = 0;
  while (waitpid(pid, &status, 0) < 0) {
    if (errno != EINTR) return std::nullopt;
  }
  if (WIFEXITED(status)) return WEXITSTATUS(status);
  // シグナルで死んだ子はシェルと同じく 128 + 番号にする。
  if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
  return std::nullopt;
}

#endif

}
