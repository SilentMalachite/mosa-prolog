#pragma once

// 生成コードが include する唯一のヘッダ（ADR 0009）。Machine::Impl は中身を見せない。
// セルを読む関数と項を書く put_* はここで inline に展開し、libmosa の外の呼び出しは下の宣言だけにする。
// 宣言の形は計画 Task 3 の Interfaces が正で、翻訳器（Task 4）はこの名前をそのまま出す。

#include "mosa/cell.hpp"
#include "mosa/goal_word.hpp"
#include "mosa/machine.hpp"

// array は生成コードが引数とセルの並びに使う。生成コードは標準ヘッダもこのヘッダからだけ得る。
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace mosa::rt {

using NativeProc = void (*)(Machine::Impl& machine, std::uint64_t frame, std::uint32_t state);
struct UnitTag { char unused = 0; };  // 翻訳単位ごとに constinit で 1 つ置く。番地だけを使う
struct UnitState;                     // Machine ごとの単位の表。internal.hpp で定義する
struct NativeEntry { std::string_view module; std::string_view name; std::uint16_t arity;
                     std::uint64_t fingerprint; NativeProc proc; };
struct NativeCallee { std::string_view module; std::string_view name; std::uint16_t arity; };
struct NativeUnit { std::string_view name; std::string_view source; const UnitTag* tag;
                    std::span<const NativeEntry> entries;      // 添字 = interpret_clause の entry
                    std::span<const std::string_view> atoms;    // 添字 = regs.atoms の添字
                    std::span<const NativeCallee> calls; };     // 添字 = call_goal の number
struct Regs { const std::uint64_t* words; const std::uint32_t* atoms; const UnitState* unit; std::size_t cut; };

inline constexpr std::size_t kFrameHead = 4;    // ゴール枠の頭。引数は frame + 4 + i
inline constexpr std::size_t kNativeVars = 5;   // Ctrl::Native の変数は frame + 5 + i
inline constexpr std::uint64_t kNativeBit = std::uint64_t{1} << 63;
inline constexpr std::uint32_t kCall = 0;

// 節は 1 から数える。goal 0 は節の頭部に入る状態なので、状態 0（呼び出し）と重ならない。
[[nodiscard]] constexpr std::uint32_t state_of(std::uint32_t clause, std::uint32_t goal) {
  return (clause << 16) | goal;
}

// inline（libmosa を呼ばない）
[[nodiscard]] inline const std::uint64_t* cell_words(Cell cell) {
  return std::bit_cast<const std::uint64_t*>(cell.raw() & ~std::uint64_t{7});
}

[[nodiscard]] inline Cell deref(Cell cell) {
  while (cell.tag() == Tag::Ref) {
    const Cell next{*cell_words(cell)};
    if (next.raw() == cell.raw()) return cell;
    cell = next;
  }
  return cell;
}

[[nodiscard]] inline bool unbound(Cell cell) { return cell.tag() == Tag::Ref && *cell_words(cell) == cell.raw(); }
[[nodiscard]] inline Cell head(Cell list) { return Cell{cell_words(list)[0]}; }
[[nodiscard]] inline Cell tail(Cell list) { return Cell{cell_words(list)[1]}; }

// pack_functor と比べる。アトムの番号は単位の atoms から引いた値を使う。
[[nodiscard]] inline std::uint64_t functor_word(Cell structure) { return cell_words(structure)[0]; }
[[nodiscard]] inline Cell struct_arg(Cell structure, std::size_t index) {
  return Cell{cell_words(structure)[index + 1]};
}

[[nodiscard]] inline Cell atom_cell(std::uint32_t atom) {
  return Cell{(static_cast<std::uint64_t>(atom) << 3) | static_cast<std::uint64_t>(Tag::Atom)};
}

// 即値の範囲だけ。外は put_int_box。Heap::integer と同じ境界で、翻訳器が値で振り分ける。
[[nodiscard]] inline Cell small_int(std::int64_t value) {
  return Cell{(static_cast<std::uint64_t>(value) << 3) | static_cast<std::uint64_t>(Tag::Int)};
}

[[nodiscard]] inline Cell arg(const Regs& regs, std::uint64_t frame, std::size_t index) {
  return Cell{regs.words[frame + kFrameHead + index]};
}
[[nodiscard]] inline Cell var(const Regs& regs, std::uint64_t frame, std::size_t index) {
  return Cell{regs.words[frame + kNativeVars + index]};
}
[[nodiscard]] inline std::uint64_t frame_cut(const Regs& regs, std::uint64_t frame) { return regs.words[frame + 2]; }
[[nodiscard]] inline std::uint64_t frame_cont(const Regs& regs, std::uint64_t frame) { return regs.words[frame + 3]; }
// 呼び手の文脈モジュール（ADR 0006）。節の入口とやり直しで渡る枠はゴール枠なので、語 1 から読める。
[[nodiscard]] inline std::uint32_t frame_module(const Regs& regs, std::uint64_t frame) {
  return goal_word_module(regs.words[frame + 1]);
}

// small_int の逆。制御構文は選択点の高さを継続枠の変数（整数セル）で運ぶ。符号付きの右シフトは C++20 で算術。
[[nodiscard]] inline std::int64_t small_value(Cell cell) { return static_cast<std::int64_t>(cell.raw()) >> 3; }

// alloc で取った塊に項を書く。番地はヒープの語そのもので、未束縛の変数は自分を指す。
[[nodiscard]] inline Cell put_ref(std::uint64_t* slot) {
  const auto address = std::bit_cast<std::uint64_t>(slot);
  *slot = address;
  return Cell{address};
}
[[nodiscard]] inline Cell put_list(std::uint64_t* slot, Cell first, Cell rest) {
  slot[0] = first.raw();
  slot[1] = rest.raw();
  return Cell{std::bit_cast<std::uint64_t>(slot) | static_cast<std::uint64_t>(Tag::Lis)};
}
[[nodiscard]] inline Cell put_struct(std::uint64_t* slot, std::uint32_t atom, std::span<const Cell> args) {
  slot[0] = pack_functor(atom, static_cast<std::uint16_t>(args.size()));
  for (std::size_t index = 0; index < args.size(); ++index) slot[index + 1] = args[index].raw();
  return Cell{std::bit_cast<std::uint64_t>(slot) | static_cast<std::uint64_t>(Tag::Str)};
}
[[nodiscard]] inline Cell put_int_box(std::uint64_t* slot, std::int64_t value) {
  *slot = std::bit_cast<std::uint64_t>(value);
  return Cell{std::bit_cast<std::uint64_t>(slot) | static_cast<std::uint64_t>(Tag::IntBox)};
}
[[nodiscard]] inline Cell put_float(std::uint64_t* slot, double value) {
  *slot = std::bit_cast<std::uint64_t>(value);
  return Cell{std::bit_cast<std::uint64_t>(slot) | static_cast<std::uint64_t>(Tag::Float)};
}

// libmosa の外の呼び出しはこれだけ
[[nodiscard]] bool install_unit(Machine& machine, const NativeUnit& unit);  // 表を作ってから source を読み、読み終えたら付ける
[[nodiscard]] Regs regs(Machine::Impl& machine, const UnitTag& unit);       // 1 回の起動に 1 回。tag の番地で単位を引く
[[nodiscard]] std::uint64_t* alloc(Machine::Impl& machine, std::size_t count);  // 節ごとに 1 塊
void bind(Machine::Impl& machine, Cell variable, Cell value);                 // 最新の選択点より古ければトレイル
[[nodiscard]] bool unify(Machine::Impl& machine, Cell left, Cell right);     // 変数と同じセルの近道のあと do_unify
void call_goal(Machine::Impl& machine, const Regs& regs, std::uint32_t number, std::span<const Cell> args,
               std::uint64_t cont, std::size_t cut);
[[nodiscard]] std::uint64_t resume_frame(Machine::Impl& machine, NativeProc proc, std::uint32_t state, std::size_t cut,
                                         std::uint64_t cont, std::span<const Cell> vars);
void push_alternative(Machine::Impl& machine, const Regs& regs, std::uint64_t frame, NativeProc proc,
                      std::span<const std::uint32_t> states);  // cut = regs.cut
void cut_to(Machine::Impl& machine, std::size_t barrier);
void proceed(Machine::Impl& machine, std::uint64_t cont);
void fail(Machine::Impl& machine);
void interpret_clause(Machine::Impl& machine, const Regs& regs, std::uint32_t entry, std::uint64_t frame,
                      std::uint32_t clause);

// 制御構文（計画 Task 7）。枠は resume_frame で作った継続枠で、その状態から生成関数を呼び直す。
[[nodiscard]] std::size_t choice_mark(Machine::Impl& machine);  // 選択点の高さ。条件のカット境界と、切り戻す先
void push_branch(Machine::Impl& machine, std::uint64_t frame, std::size_t cut);  // (A ; B) の B。やり直しで枠の状態へ
[[nodiscard]] std::size_t push_soft(Machine::Impl& machine, std::uint64_t frame, std::size_t cut);  // (A *-> B ; C) の C。印の番号を返す
void soft_commit(Machine::Impl& machine, std::size_t soft_id);  // 条件の成功。印の付いた else はやり直しで飛ぶ
void push_negation(Machine::Impl& machine, std::uint64_t cont);  // \+ A の床。A が失敗してここへ戻れば cont へ
void throw_term(Machine::Impl& machine, Cell ball);  // throw/1。捕まえる catch まで巻き戻す（C++ の例外は使わない）
// catch/3。recovery は回復ゴールの継続枠。ボールが pattern と単一化できればそこへ進む
void push_catch(Machine::Impl& machine, Cell pattern, std::uint64_t recovery, std::size_t cut);
void catch_exit(Machine::Impl& machine, std::size_t index);  // ゴールを抜けた。index の Catch を無効にする
// meta_predicate で限定する引数。M: が無ければ context:actual にする（enter_clause の meta_argument）
[[nodiscard]] Cell qualify(Machine::Impl& machine, Cell actual, std::uint32_t context);

// mosac の main.cpp（計画 Task 5）。start で引数を入れ、embed で原文を置き、install_unit で読んで付け、finish で返す。
// 単位の name が embed したパスなら、install_unit はそのファイルとして読む（読み済みなら付けるだけ）。
void start(Machine& machine, int argc, char** argv);  // コンソールを UTF-8 にし、argv[1] 以降を旗 argv にする
void embed(Machine& machine, std::string_view path, std::string_view source);  // include/1 などがファイルより先に引く
[[nodiscard]] int finish(Machine& machine, bool loaded);  // 読めなければ診断を出して 1。main は Task 2 の終了コード
}
