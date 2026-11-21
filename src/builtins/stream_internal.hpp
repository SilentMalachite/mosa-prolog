#pragma once

// streams.cpp と stream_close.cpp だけが共有する。ほかの組込からは使わない。
#include "../machine/internal.hpp"

#include <string_view>

namespace mosa::streams {

// 誤りを投げて真を返す。呼び手が `return raise_inst(...)` の 1 行で引数の検査を抜けるため。
bool raise_inst(Impl& machine, const Goal& goal);
bool raise_type(Impl& machine, const Goal& goal, std::string_view type, Cell culprit);
bool raise_domain(Impl& machine, const Goal& goal, std::string_view domain, Cell culprit);
[[nodiscard]] std::string_view atom_text(const Impl& machine, Cell cell);
[[nodiscard]] bool item_unbound(const Impl& machine, const WalkedList& walked);
// close/1,2 と flush_output/0,1（stream_close.cpp）。
void run_close(Impl& machine, const Goal& goal);
void run_flush(Impl& machine, const Goal& goal);

}  // namespace mosa::streams
