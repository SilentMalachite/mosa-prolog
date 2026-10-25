#pragma once

// キーは UTF-8 の文字列である。アトム表より先に置くため、アトム番号にはしない。

#include <string>
#include <string_view>
#include <vector>

namespace mosa {

// 宣言順が、同じ優先度の lookup の並びである。
enum class Specifier { Xf, Yf, Xfx, Xfy, Yfx, Fx, Fy };

enum class OpResult {
  Ok, BadPriority, BadSpecifier, ModifyComma, CreateEmpty, InfixAndPostfix, CreateBar
};

struct OpDef {
  int priority = 0;
  Specifier specifier = Specifier::Xfx;
  std::string name;
};

class OperatorTable {
 public:
  [[nodiscard]] static OperatorTable iso_core();

  // 拒否は表を変えない。優先度 0 はその名前と指定子の組だけを消す。
  // 0 以外は、同じ類（前置、中置、後置）の古い指定子を置き換える。
  [[nodiscard]] OpResult define(int priority, Specifier specifier, std::string name);

  // 優先度の昇順、同じ優先度では Specifier の宣言順。
  [[nodiscard]] std::vector<OpDef> lookup(std::string_view name) const;

  // 表に入っている順。名前を決めない current_op/3 はこの順で返す。
  [[nodiscard]] std::vector<OpDef> entries() const;

 private:
  std::vector<OpDef> ops_;
};

}
