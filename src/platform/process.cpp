// 子プロセスの起動と待ち合わせ（計画 Task 6、mosatest --bench。環境の受け渡しは Task 9）。OS ごとの分岐はここだけに書く。
// POSIX は posix_spawnp、Windows は CreateProcessW。どちらも PATH から探し、終わるまで待って終了コードを返す。
// 時間はここで測らない。呼び手（src/machine/bench.cpp）が起動から戻りまでを測る。
// 標準入力のファイルと親の環境の写し（environment_with）は mosatest --manual のため（リリース 0.1.0 Task 4）。
#include "mosa/process.hpp"

#include "mosa/path_text.hpp"

#include <cstddef>
#include <string>
#include <string_view>
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

namespace {

using Entry = std::pair<std::string, std::string>;

// "NAME=VALUE" を分ける。Windows の隠れた変数 "=C:=C:\x" は名前が = で始まるので、2 文字目から = を探す。
std::optional<Entry> split_entry(std::string_view text) {
  const std::size_t equals = text.find('=', 1);
  if (text.empty() || equals == std::string_view::npos) return std::nullopt;
  return Entry{std::string{text.substr(0, equals)}, std::string{text.substr(equals + 1)}};
}

char folded(char letter, bool fold_case) {
  return fold_case && letter >= 'A' && letter <= 'Z' ? static_cast<char>(letter - 'A' + 'a') : letter;
}

bool same_name(std::string_view left, std::string_view right, bool fold_case) {
  if (left.size() != right.size()) return false;
  for (std::size_t index = 0; index < left.size(); ++index) {
    if (folded(left[index], fold_case) != folded(right[index], fold_case)) return false;
  }
  return true;
}

// 親の並びを保ち、set の名前は値だけを差し替える。親に無い set の名前は後ろに足す。
std::vector<Entry> merged(std::vector<Entry> parent, std::span<const Entry> set, std::span<const std::string> unset,
                          bool fold_case) {
  std::vector<Entry> out;
  std::vector<bool> used(set.size(), false);
  for (Entry& entry : parent) {
    bool removed = false;
    for (const std::string& name : unset) removed = removed || same_name(entry.first, name, fold_case);
    if (removed) continue;
    for (std::size_t index = 0; index < set.size(); ++index) {
      if (!same_name(entry.first, set[index].first, fold_case)) continue;
      entry.second = set[index].second;
      used[index] = true;
    }
    out.push_back(std::move(entry));
  }
  for (std::size_t index = 0; index < set.size(); ++index) {
    if (!used[index]) out.push_back(set[index]);
  }
  return out;
}

}  // namespace

#if defined(_WIN32)

namespace {

constexpr bool kFoldCase = true;

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

// 値は native の形のまま UTF-8 にする。utf8_from_path は区切りを / にそろえるので、PATH の \ を変えてしまう。
std::string utf8_native(const std::wstring& wide) {
  const std::u8string bytes = std::filesystem::path{wide}.u8string();
  std::string text;
  text.reserve(bytes.size());
  for (const char8_t byte : bytes) text.push_back(static_cast<char>(byte));
  return text;
}

// GetEnvironmentStringsW の塊は NAME=VALUE の NUL 終わりの並びで、空の項目で終わる。
std::vector<Entry> parent_environment() {
  std::vector<Entry> entries;
  const LPWCH block = GetEnvironmentStringsW();
  if (block == nullptr) return entries;
  for (const wchar_t* cursor = block; *cursor != L'\0';) {
    const std::wstring entry{cursor};
    cursor += entry.size() + 1;
    if (auto split = split_entry(utf8_native(entry))) entries.push_back(std::move(*split));
  }
  FreeEnvironmentStringsW(block);
  return entries;
}

// 子に継がせる取っ手でファイルを開く。開けなければ INVALID_HANDLE_VALUE。
HANDLE inheritable(const std::filesystem::path& path, DWORD access, DWORD disposition) {
  SECURITY_ATTRIBUTES inherit{};
  inherit.nLength = static_cast<DWORD>(sizeof(inherit));
  inherit.bInheritHandle = TRUE;
  return CreateFileW(path.c_str(), access, FILE_SHARE_READ, &inherit, disposition, FILE_ATTRIBUTE_NORMAL, nullptr);
}

// 子に渡したファイルの取っ手。親の側はどの道で戻っても閉じる。
struct Handles {
  Handles() = default;
  Handles(const Handles&) = delete;
  Handles& operator=(const Handles&) = delete;
  ~Handles() {
    if (output != INVALID_HANDLE_VALUE) CloseHandle(output);
    if (input != INVALID_HANDLE_VALUE) CloseHandle(input);
  }
  HANDLE output = INVALID_HANDLE_VALUE;
  HANDLE input = INVALID_HANDLE_VALUE;
};

// log と input を開いて info に載せる。どちらも空なら何もしない（親の標準の流れを継ぐ）。
bool attach(STARTUPINFOW& info, Handles& handles, const std::filesystem::path& log, const std::filesystem::path& input) {
  if (log.empty() && input.empty()) return true;
  if (!log.empty()) {
    handles.output = inheritable(log, GENERIC_WRITE, CREATE_ALWAYS);
    if (handles.output == INVALID_HANDLE_VALUE) return false;
  }
  if (!input.empty()) {
    handles.input = inheritable(input, GENERIC_READ, OPEN_EXISTING);
    if (handles.input == INVALID_HANDLE_VALUE) return false;
  }
  info.dwFlags = STARTF_USESTDHANDLES;
  info.hStdInput = input.empty() ? GetStdHandle(STD_INPUT_HANDLE) : handles.input;
  info.hStdOutput = log.empty() ? GetStdHandle(STD_OUTPUT_HANDLE) : handles.output;
  info.hStdError = log.empty() ? GetStdHandle(STD_ERROR_HANDLE) : handles.output;
  return true;
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
                               std::span<const std::pair<std::string, std::string>> env,
                               const std::filesystem::path& input) {
  if (argv.empty()) return std::nullopt;
  // 引数は UTF-8 で受ける。path を通すと UTF-16 になる（path_text.hpp と同じ道）。
  std::wstring line;
  for (const std::string& arg : argv) {
    if (!line.empty()) line += L' ';
    line += quoted(path_from_utf8(arg).native());
  }
  STARTUPINFOW info{};
  info.cb = static_cast<DWORD>(sizeof(info));
  Handles handles;
  if (!attach(info, handles, log, input)) return std::nullopt;
  PROCESS_INFORMATION process{};
  std::wstring block = environment_block(env);
  const BOOL started = CreateProcessW(nullptr, line.data(), nullptr, nullptr, TRUE, CREATE_UNICODE_ENVIRONMENT,
                                      env.empty() ? nullptr : block.data(), nullptr, &info, &process);
  if (started == 0) return std::nullopt;
  return wait_for(process);
}

#else

namespace {

constexpr bool kFoldCase = false;

std::vector<Entry> parent_environment() {
  std::vector<Entry> entries;
  for (char** cursor = environ; cursor != nullptr && *cursor != nullptr; ++cursor) {
    if (auto split = split_entry(*cursor)) entries.push_back(std::move(*split));
  }
  return entries;
}

// 標準出力をログに開き、標準エラーもそこへ重ねる。
bool redirect(posix_spawn_file_actions_t& actions, const std::filesystem::path& log) {
  if (posix_spawn_file_actions_addopen(&actions, 1, log.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644) != 0) return false;
  return posix_spawn_file_actions_adddup2(&actions, 1, 2) == 0;
}

// 標準入力をファイルから読ませる。
bool redirect_input(posix_spawn_file_actions_t& actions, const std::filesystem::path& input) {
  return posix_spawn_file_actions_addopen(&actions, 0, input.c_str(), O_RDONLY, 0) == 0;
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
                               std::span<const std::pair<std::string, std::string>> env,
                               const std::filesystem::path& input) {
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
  if ((log.empty() || redirect(actions, log)) && (input.empty() || redirect_input(actions, input))) {
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

std::vector<std::pair<std::string, std::string>> environment_with(std::span<const std::pair<std::string, std::string>> set,
                                                                  std::span<const std::string> unset) {
  return merged(parent_environment(), set, unset, kFoldCase);
}

}
