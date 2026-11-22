#pragma once

// stream_test.cpp と stream_read_test.cpp が共に使う道具。2 つは 1 つの stream_test の実行ファイルに入るので、
// 無名の名前空間に写しを 2 つ持つと、片方が使わない写しが -Werror で落ちる。

#include "mosa/machine.hpp"
#include "mosa/writer.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace stream_support {

inline std::string binding_named(mosa::Machine& machine, const mosa::Stop& stop, std::string_view name) {
  for (const mosa::Binding& binding : stop.bindings) {
    if (binding.name != name) continue;
    return mosa::write_term(machine.heap(), machine.operators(), machine.heap().deref(binding.variable),
                            mosa::WriteOptions{}, {});
  }
  return {};
}

inline std::string prolog_atom(std::string_view text) {
  std::string out = "'";
  for (char unit : text) {
    if (unit == '\'') out += "''";
    else out.push_back(unit);
  }
  out.push_back('\'');
  return out;
}

// string() は Windows で \ を含む。引用原子の \a はエスケープなので、UTF-8 の / にする。
inline std::string path_atom(const std::filesystem::path& path) {
  const std::u8string text = path.generic_u8string();
  std::string bytes;
  bytes.reserve(text.size());
  for (char8_t unit : text) bytes.push_back(static_cast<char>(unit));
  return prolog_atom(bytes);
}

inline void write_bytes(const std::filesystem::path& path, std::string_view bytes) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output << bytes;
}

inline std::string formal_text(mosa::Machine& machine, const mosa::Stop& stop) {
  // 例外でない答えの exception は空のセル。辿ると落ち、後の検査の FAIL も出なくなる。
  if (stop.kind != mosa::StopKind::Exception) return "(no exception)";
  const auto ball = machine.heap().deref(stop.exception);
  return mosa::write_term(machine.heap(), machine.operators(), machine.heap().arg(ball, 0), mosa::WriteOptions{}, {});
}

}  // namespace stream_support
