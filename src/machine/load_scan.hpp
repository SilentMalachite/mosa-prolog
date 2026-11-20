#pragma once

// load.cpp と load_use.cpp だけが共有する。テキスト 1 回分の走査の状態と、ファイルを開く指示とゴールの入口。
#include "load_internal.hpp"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mosa {

struct LoadScan {
  LoadSnapshots& snapshots;
  std::vector<Apart>& apart;
  std::vector<Cell>& inits;
  std::optional<PredKey>& last;
  std::vector<std::string>& opening;
  // open_prolog_file が開けたファイルの数。ゴールの読み込みの診断が、開けなかったのが根のファイルかを見分ける。
  std::size_t opened = 0;
};

// テキストの読み方。import_all は、モジュールのファイルを読み終えたとき公開述語を全部取り込むこと（use_module は
// 自分で取り込む）。owns は、テキストの述語をこのファイルのものにすること（ADR 0020）。include したテキストは、
// それを読んだファイルの述語を定義する。
struct TextRole {
  bool nested = true;
  bool import_all = true;
  bool owns = true;
};

// ゴールの読み込み 1 回（ADR 0026）。body は呼び手が GC の根に登録した use_module(...) か ensure_loaded(...)。file は
// 開いたパス。prefix_file は、失敗の診断に「file: 」を付けること（計画の「設計との違い」6）。
struct GoalLoad {
  Cell& body;
  std::filesystem::path file;
  bool prefix_file = true;
};

// 定義は load.cpp。テキスト 1 つを scan へ読む。include したテキストも use_module したファイルもここを通る。
bool read_terms(Impl& machine, std::string_view source, std::string_view directory, std::string_view origin,
                LoadScan& scan, TextRole role);

// 以下の定義は load_use.cpp。指示の入口は、その指示でなければ 0、成功なら 1、失敗なら -1 を返す。
int fail_formal(Impl& machine, Cell formal);
int apply_use(Impl& machine, Cell term, std::string_view directory, LoadScan& scan);
int apply_file(Impl& machine, Cell term, std::string_view directory, LoadScan& scan, std::string_view functor,
               bool once);
bool goal_use(Impl& machine, GoalLoad& load, LoadScan& scan);
bool goal_ensure(Impl& machine, GoalLoad& load, LoadScan& scan);

}  // namespace mosa
