#pragma once

#include "mosa/machine.hpp"
#include "mosa/operator_table.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mosa {

struct Predicate;

// meta_predicate/1 の引数 1 つ。Goal は 0〜9 で、extra は閉包に足す引数の数。
// None は限定しない引数で、mode に +、-、?、* のアトムを残す。性質 meta_predicate(Head) で書き戻すため。
struct MetaArg {
  enum class Kind { None, Goal, Colon, Caret, Dcg };
  Kind kind = Kind::None;
  std::uint8_t extra = 0;
  std::uint32_t mode = 0;
};

struct PredKey {
  std::uint32_t module = 0;
  std::uint32_t name = 0;
  std::uint16_t arity = 0;
};

// alias は as の別名。別名が無いときは name と同じ値を入れる。
struct Import {
  std::uint32_t name = 0;
  std::uint16_t arity = 0;
  std::uint32_t alias = 0;
  std::uint32_t from = 0;
};

// 手続きがまだ無い名前への meta_predicate/1。宣言は手続きを作らないので、節が来るまでモジュールに置く。
struct PendingMeta {
  std::uint32_t name = 0;
  std::uint16_t arity = 0;
  std::vector<MetaArg> args;
};

struct Module {
  std::uint32_t name = 0;
  std::uint32_t default_import = 0;
  std::vector<PredKey> exports;
  std::vector<Import> imports;
  // 正規化したパス。use_module がファイルからモジュールを引く。ファイルでないテキストは空。
  std::string file;
  std::vector<PendingMeta> pending_meta;
  // 固有層の演算子。優先度 0 は下の層を隠す印。export_ops は公開リストの op(P, T, N) を名前ごとに開いたもの。
  OperatorTable ops;
  std::vector<OpDef> export_ops;
};

// user と system を作る。system の既定の取り込み元は自分自身で、引く連鎖をそこで止める。
void install_modules(Machine::Impl& machine);
[[nodiscard]] const Module* find_module(const Machine::Impl& machine, std::uint32_t name);
[[nodiscard]] Module* find_module(Machine::Impl& machine, std::uint32_t name);
[[nodiscard]] Predicate* find_pred(Machine::Impl& machine, PredKey key);
[[nodiscard]] const Predicate* find_pred(const Machine::Impl& machine, PredKey key);
[[nodiscard]] Predicate* resolve_pred(Machine::Impl& machine, std::uint32_t context, std::uint32_t name,
                                      std::uint16_t arity);

// 読み込みの指令 module/2 と export/1。nullopt は成功で、値は読み込みの診断にする Formal である。
// first が偽なら、引数が正しくても permission_error(create, module, M) にする。
[[nodiscard]] std::optional<Cell> declare_module(Machine::Impl& machine, Cell name, Cell exports, bool first,
                                                 std::string_view file);
[[nodiscard]] std::optional<Cell> add_exports(Machine::Impl& machine, Cell indicators);
[[nodiscard]] std::optional<Cell> read_indicator(Machine::Impl& machine, Cell cell, PredKey& key);

// use_module/2 の第 2 引数。All は use_module/1、Only は [PI, PI as Alias]、Except は except([PI])。
struct ImportSpec {
  enum class Kind { All, Only, Except };
  Kind kind = Kind::All;
  std::vector<Import> only;
  std::vector<PredKey> except;
};

[[nodiscard]] std::optional<Cell> parse_imports(Machine::Impl& machine, Cell imports, ImportSpec& spec);
// from の公開述語を spec のとおり into から見えるようにする。衝突と未公開は読み込みの診断にする Formal を返す。
[[nodiscard]] std::optional<Cell> apply_imports(Machine::Impl& machine, std::uint32_t from, std::uint32_t into,
                                                const ImportSpec& spec);
[[nodiscard]] std::optional<Cell> import_exports(Machine::Impl& machine, std::uint32_t from, std::uint32_t into);
// 局所の述語を作る前に呼ぶ。同じ名前を取り込み済みなら permission_error(import_into, ...) を返す。
[[nodiscard]] std::optional<Cell> check_local(Machine::Impl& machine, std::uint32_t module, std::uint32_t name,
                                              std::uint16_t arity);

// 節操作の M:Term。term と module を内側の値に置き換える。M: が無ければどちらも変えない。
[[nodiscard]] std::optional<Cell> strip_module(Machine::Impl& machine, Cell& term, std::uint32_t& module);
// M:(H :- B) と (M:H :- B) の両方を、修飾の無い節とそのモジュールにする。
[[nodiscard]] std::optional<Cell> strip_clause(Machine::Impl& machine, Cell& term, std::uint32_t& module);
// 検査済みの項の M: を外す。やり直しで引数を読み直すときに使う。
[[nodiscard]] Cell skip_module(const Machine::Impl& machine, Cell term);
// 節操作が触る述語。自分の述語、次に明示の取り込み。既定の取り込み元は見ない。
// 取り込みに当たれば module と name を取り込み元の値にする。nullptr でも、作る先はその値である。
[[nodiscard]] Predicate* owned_pred(Machine::Impl& machine, std::uint32_t& module, std::uint32_t& name,
                                    std::uint16_t arity);
// meta_predicate/1 の宣言 1 つ（M: は外した後）。誤りは読み込みの診断にする Formal を返す。
[[nodiscard]] std::optional<Cell> read_meta_spec(Machine::Impl& machine, Cell head, std::vector<MetaArg>& args);
// 呼び出しの index 番目の引数。限定する引数で M: が無ければ context:actual にする。
[[nodiscard]] Cell meta_argument(Machine::Impl& machine, const Predicate& predicate, std::uint32_t context,
                                 std::size_t index, Cell actual);
// 第1引数を限定するなら真。節の第1引数の索引は、限定の前の値では引けない。
[[nodiscard]] bool wraps_first(const Predicate& predicate);
// 述語を作った直後に呼ぶ。保留の宣言があれば述語へ移す。
void adopt_meta(Machine::Impl& machine, Predicate& predicate);
// 性質 meta_predicate(Head) の Head。宣言の無い述語は nullopt。組込は表から作る。
[[nodiscard]] std::optional<Cell> meta_head(Machine::Impl& machine, const Predicate& predicate);
[[nodiscard]] std::optional<Cell> builtin_meta_head(Machine::Impl& machine, std::uint32_t name, std::uint16_t arity);

// 文脈モジュールから見える演算子。user と無いモジュールは user の表だけ。使う間、モジュール表を動かさないこと。
[[nodiscard]] OperatorView op_view(const Machine::Impl& machine, std::uint32_t module);
// module の層に定義する。user なら全体の表。拒否は表を変えない。
[[nodiscard]] OpResult define_op(Machine::Impl& machine, std::uint32_t module, int priority, Specifier specifier,
                                 std::string name);
// from の公開の演算子を into の層へ入れる。use_module/1、except、consult の取り込みで呼ぶ。
[[nodiscard]] std::optional<Cell> import_ops(Machine::Impl& machine, std::uint32_t from, std::uint32_t into);

// user の述語は N/A、ほかは M:N/A（iso-notes 第5節の決定 A）。
[[nodiscard]] Cell module_indicator(Machine::Impl& machine, std::uint32_t module, std::uint32_t name,
                                    std::uint16_t arity);

}
