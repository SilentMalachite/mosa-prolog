#pragma once

// 読み込み（load_source・consult・同梱ライブラリ）と翻訳済みの単位の付け外しの宣言。internal.hpp が 800 行の上限に
// 達したために分けた。internal.hpp の末尾から読む（Impl と Goal の定義が要る）。

#include "internal.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mosa {

// 公開の入口（Machine::consult、load_program）の読み込みの間だけ、前の問い合わせが halt で止まった印を脇に置く。
// 残すと、この読み込みの誤りを halt と取り違えて診断を消す（run_load）。抜けるとき戻すのは、次の問い合わせが止まった
// 問い合わせの後始末を動かさず（ADR 0016、open_query）、next() がその問い合わせへ戻らないためである。
class HaltSetAside {
 public:
  explicit HaltSetAside(Impl& machine) : machine_(machine), saved_(machine.halt_now) { machine.halt_now = false; }
  ~HaltSetAside() { machine_.halt_now = machine_.halt_now || saved_; }
  HaltSetAside(const HaltSetAside&) = delete;
  HaltSetAside& operator=(const HaltSetAside&) = delete;

 private:
  Impl& machine_;
  bool saved_;
};

// directory が空でなければ、相対の include はそのディレクトリから開く。
// origin は循環の検出に使う。空なら、このテキスト自体はファイルではない。
// shared_opening は、入れ子の consult が親と同じ循環を見るため。
[[nodiscard]] LoadStatus load_source(Impl& machine, std::string_view source, std::string_view directory = {},
                                    std::string_view origin = {},
                                    std::vector<std::string>* shared_opening = nullptr);
// 0 はファイルリストではない。1 は読んだ。-1 は失敗。
[[nodiscard]] int apply_file_list(Impl& machine, Cell term, std::string_view directory,
                                  std::vector<std::string>& opening);
[[nodiscard]] bool run_consult(Impl& machine, const Goal& goal);
// 定義は load.cpp。use_module の Spec の形だけを見る（library(Name) か原子）。指令の locate_use とゴールの
// run_load_goal が呼び、同じ Formal（instantiation_error、type_error(atom, _)）を出す。
[[nodiscard]] std::optional<Cell> check_use_spec(Impl& machine, Cell spec);
// 定義は load.cpp。directive は :-(G) で、G は引数を検査済みの use_module/1,2 か ensure_loaded/1。外側の読み込み 1 回と
// して machine.load_module へ読む（ADR 0026）。相対パスは作業ディレクトリから開き、開いたファイルは根のテキストとして
// 読み、字句の旗はファイルの consult と同じく戻す。失敗の診断には計画の「設計との違い」6 の字面を付ける。
[[nodiscard]] LoadStatus load_goal_term(Impl& machine, Cell directive);
// 定義は goal_libraries.cpp。翻訳の読み込み（machine.translation が在る間）だけ、term の部分項の
// use_module と reexport の (library(N)) と (library(N), _) の N（原子のもの）を TranslationLog::goal_libraries に足す。
void note_goal_libraries(Impl& machine, Cell term);
// 定義は goal_libraries.cpp。goal_libraries のライブラリを user へ取り込みなしで読み、同梱の閉包に入れる（問い 2 の A）。
// 失敗しても止めず、診断も残さない。
void load_goal_libraries(Impl& machine);
// 定義は consult.cpp。use_module/1,2 と ensure_loaded/1 のゴール。引数の誤りは例外にし、ほかの失敗は
// report_goal_load_failure で診断を書いて失敗する。成功なら選択点を残さない。
[[nodiscard]] bool run_load_goal(Impl& machine, const Goal& goal);
// 定義は load_path.cpp。読み込みのファイルの鍵（Module::file、prepared、埋め込み）にする正規化したパス。
[[nodiscard]] std::filesystem::path canonical_key(const std::filesystem::path& file);
// 埋め込みの原文を先に引き、無ければファイルを読む。開けなければ nullopt。
[[nodiscard]] std::optional<std::string> read_source(const Impl& machine, const std::filesystem::path& file);
// 拡張子の無い名前に .pl を足すかを、埋め込みの原文も見て決める。
[[nodiscard]] std::filesystem::path source_path(const Impl& machine, const std::filesystem::path& file);
// 定義は consult.cpp。読み込みの診断にパスと、構文エラーなら行と列を付ける。
void prefix_consult_diagnostic(Impl& machine, std::string_view path);
// 定義は consult.cpp。ゴールの読み込みが失敗するたびに、machine.diagnostic の 1 行を load_report（無ければ std::cerr）へ
// 書き、reported_diagnostic に覚える。;、findall/3、\+ が失敗を吸っても理由が残るように、その場で書く（問い 1 の A）。
void report_goal_load_failure(Impl& machine);
// 定義は native.cpp。単位の name が埋め込んだパスならそのファイルとして、ほかは文字だけで source を読む。
// 差分テスト（計画 Task 11）のインタプリタ側は、表の無い単位でこれを呼んで翻訳側と同じ読み方をする。
[[nodiscard]] bool load_unit(Impl& machine, const rt::NativeUnit& unit);
// 定義は session.cpp。initialization(G, main) を Task 2 の規則で動かし、終了コードを返す。文面は warning に足す。
[[nodiscard]] int run_main_goal(Machine& machine, const ExportedTerm& goal, std::string& warning);
// 定義は native.cpp。翻訳器（Task 4）も同じ関数で指紋を出す。
[[nodiscard]] std::uint64_t clause_fingerprint(const AtomTable& atoms, const Predicate& predicate);
// install_unit が生成関数を付けてよい述語か（指紋は見ない）。翻訳器も同じ関数で出す述語を選ぶ。
[[nodiscard]] bool native_eligible(const Impl& machine, const Predicate& predicate);
// 付けたあとの節か meta への書き込みのたびに呼ぶ。述語と全単位の同じ鍵の calls を一度に空にする。
void detach_native(Impl& machine, Predicate& predicate);
// 単位の表を作って（番号は単位の中だけ）その slot を返す。付けるのは原文を読み終えた後（ADR 0009）。
[[nodiscard]] std::size_t prepare_native_unit(Impl& machine, const rt::NativeUnit& unit);
void attach_native_unit(Impl& machine, std::size_t slot, const rt::NativeUnit& unit);
// 定義は native_library.cpp。同梱ライブラリの擬似パス library/<Name>.pl。mosac（project.cpp）も同じ関数で鍵を作る。
[[nodiscard]] std::filesystem::path bundled_library_path(std::string_view name);
// use_module(library(Name)) が SPEC 第5節の探索より先に引く。同梱にあれば原文を擬似パスで埋め込み、そのパスを返す。
[[nodiscard]] std::optional<std::filesystem::path> locate_bundled_library(Impl& machine, std::string_view name);
// 同梱ライブラリの原文を読み終えた後に、その翻訳済みの単位を付ける（原文だけの同梱なら何もしない）。
void attach_bundled_library(Impl& machine, std::string_view name);
// 同梱ライブラリの中の素のパスの use_module(util) が擬似パス library/util.pl に解決したら、原文を埋め込み（まだなら）、
// file を .pl 付きに直して名前を返す。ほかのパスなら nullopt。
[[nodiscard]] std::optional<std::string> locate_bundled_plain(Impl& machine, std::filesystem::path& file);
// 定義は native_redo.cpp。
void call_native(Impl& machine, rt::NativeProc proc, std::uint64_t frame);
void resume_native(Impl& machine);
[[nodiscard]] bool redo_native(Impl& machine);

}
