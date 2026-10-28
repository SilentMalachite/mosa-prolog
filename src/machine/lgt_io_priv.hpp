#pragma once

// lgt_io と lgt_output が分け合う ^^ の下回り。テストランナーの外には出さない。

#include "lgt_io.hpp"

#include "internal.hpp"
#include "mosa/path_text.hpp"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mosa::lgtio {

struct IoOpt {
  bool reposition = true;
  Stream::Data data = Stream::Data::Text;
  Stream::EofAction eof = Stream::EofAction::EofCode;
};

[[nodiscard]] std::string_view atom_text(const Heap& heap, Cell cell);
// UTF-8 の 1 文字のバイト数。壊れた並びは 1 として先へ進める。
[[nodiscard]] std::size_t scalar_width(std::string_view bytes, std::size_t index);
[[nodiscard]] bool text_contents(const Heap& heap, Cell cell, std::string& out);
[[nodiscard]] bool byte_contents(const Heap& heap, Cell cell, std::string& out);
[[nodiscard]] bool read_options(const Heap& heap, Cell list, IoOpt& opt);
[[nodiscard]] std::filesystem::path locate_file(const LgtIo& io, std::string_view name);
[[nodiscard]] std::optional<std::size_t> open_alias(const Impl& machine, std::string_view alias);
void close_path(Impl& machine, std::string_view path);
void drop_file(Impl& machine, const std::filesystem::path& path);
[[nodiscard]] LgtSuper bound_ok();
[[nodiscard]] LgtSuper bound_no();
[[nodiscard]] std::string var_name(const Heap& heap, Cell cell, const std::vector<VarName>& names);
[[nodiscard]] LgtSuper bind_slot(Machine& machine, Cell slot, Cell value, const std::vector<VarName>& names);
[[nodiscard]] bool named_goal(const Heap& heap, Cell cell, std::string_view name, int& arity);
// 読み書きした中身の表明。扱わない名前は Unknown を返す。lgt_output.cpp。
[[nodiscard]] LgtSuper observe_contents(Machine& machine, const LgtIo& io, Cell term,
                                        const std::vector<VarName>& names);

}  // namespace mosa::lgtio
