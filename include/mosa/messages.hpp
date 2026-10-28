#pragma once

// 利用者向けの文面（ADR 0007）。文面は lib/messages_ja.pl と lib/messages_en.pl の事実に置く。
#include "mosa/cell.hpp"

#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace mosa {

class Machine;

enum class MessageLanguage { Ja, En };

// 旗 message_language の既定。LANG が ja で始まれば日本語、それ以外と未設定は英語。
[[nodiscard]] MessageLanguage language_from_lang(std::optional<std::string_view> lang);
// 環境変数 LANG から決めた既定。
[[nodiscard]] MessageLanguage default_message_language();
// 旗の言語のカタログで Key を書式にする。改行は付けない。machine のヒープと束縛は捨てる。
[[nodiscard]] std::string render_message(Machine& machine, std::string_view key, std::span<const Cell> args);
// 使えるマシンが無いときの表示。新しいマシンを作ってカタログを読む。引数は原子として ~w に渡す。
[[nodiscard]] std::string render_message_fresh(MessageLanguage language, std::string_view key,
                                               std::span<const std::string> args = {});

}
