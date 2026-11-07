# リリース 0.1.0 Task 9。cmake/package.cmake でパッケージを作り、空白と日本語を含む場所に展開して、利用者が受け取る
# 形（設計の「リリースパッケージ」、SPEC 第6節）を確かめ、展開した木の mosatest でマニュアルの例をすべて動かす。
# ビルド木ではなく展開した木で動かすのは、mosa が bin/lib を、mosac が lib/mosa/<鍵>/ を自分の場所から見つけ、
# 第 13 章の mosac build が同梱の実行時ライブラリで通ることを、読者と同じ形で確かめるため。
# 使い方: cmake -DSRC=<リポジトリの根> -DBUILD_DIR=<ビルド木> -DCONFIG=<--config の値> -DABI=<OS-arch-compiler>
#   -DVERSION=<0.1.0 など> -DWORK=<tests/tmp/package> -DEXE_SUFFIX=<.exe など> -P package_test.cmake

# runtime の部品は Release だけ入れる。ほかの構成のパッケージには mosac build に要る物が無いので、
# mosac_installed_layout と同じ文で飛ばす（tests.cmake の SKIP_REGULAR_EXPRESSION がこの文を skip と数える）。
if(NOT CONFIG STREQUAL "Release")
  message(STATUS "skipped: the runtime component is installed only for Release (this build's config is '${CONFIG}')")
  return()
endif()

file(REMOVE_RECURSE "${WORK}")
set(out "${WORK}/展開 先")
set(unpacked "${out}/unpacked")
file(MAKE_DIRECTORY "${unpacked}" "${WORK}/work")

execute_process(COMMAND "${CMAKE_COMMAND}" "-DBUILD_DIR=${BUILD_DIR}" "-DCONFIG=${CONFIG}" "-DOUT=${out}"
    -P "${SRC}/cmake/package.cmake"
  OUTPUT_VARIABLE printed ERROR_VARIABLE err RESULT_VARIABLE code TIMEOUT 600)
if(NOT code EQUAL 0)
  message(FATAL_ERROR "package.cmake: code=[${code}] out=[${printed}] err=[${err}]")
endif()

# 名前は mosa-<版>-<OS>-<arch>。<OS>-<arch> は鍵の前の 2 つで、Windows だけ zip にする。最後の行が書庫のパス。
string(REGEX MATCH "^[^-]+-[^-]+" os_arch "${ABI}")
set(name "mosa-${VERSION}-${os_arch}")
set(extension ".tar.gz")
if(os_arch MATCHES "^windows-")
  set(extension ".zip")
endif()
string(REPLACE "\r" "" printed "${printed}")
string(STRIP "${printed}" printed)
string(REGEX REPLACE "^.*\n" "" archive "${printed}")
if(NOT archive STREQUAL "${out}/${name}${extension}" OR NOT EXISTS "${archive}")
  message(FATAL_ERROR "the last line is not the archive ${out}/${name}${extension}: [${printed}]")
endif()
if(NOT IS_DIRECTORY "${out}/${name}")
  message(FATAL_ERROR "the installed tree ${out}/${name} is missing")
endif()

execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xf "${archive}"
  WORKING_DIRECTORY "${unpacked}" OUTPUT_VARIABLE tar_out ERROR_VARIABLE err RESULT_VARIABLE code TIMEOUT 600)
if(NOT code EQUAL 0)
  message(FATAL_ERROR "unpack ${archive}: code=[${code}] out=[${tar_out}] err=[${err}]")
endif()
# 書庫の中はパッケージと同じ名前のフォルダ 1 つ（第 2 章が読者にそう書く）。
file(GLOB top LIST_DIRECTORIES true RELATIVE "${unpacked}" "${unpacked}/*")
if(NOT top STREQUAL name)
  message(FATAL_ERROR "the archive must hold one folder ${name}, not [${top}]")
endif()

set(root "${unpacked}/${name}")
foreach(expected
    "bin/mosa${EXE_SUFFIX}" "bin/mosac${EXE_SUFFIX}" "bin/mosatest${EXE_SUFFIX}" "bin/lib/lists.pl"
    "lib/mosa/${ABI}/include/mosa/runtime.hpp" "lib/mosa/source/CMakeLists.txt"
    "LICENSE" "NOTICE" "README.md" "README.ja.md" "CHANGELOG.md" "CHANGELOG.ja.md"
    "docs/manual/en/README.md" "docs/manual/ja/README.md")
  if(NOT EXISTS "${root}/${expected}")
    file(GLOB_RECURSE listed LIST_DIRECTORIES false RELATIVE "${root}" "${root}/*")
    list(FILTER listed EXCLUDE REGEX "^lib/mosa/source/")
    message(FATAL_ERROR "missing in the package: ${expected}\npackage: ${listed}")
  endif()
endforeach()

# 展開した木の mosatest は、--mosa を省けば隣の mosa を使う。MOSA_LIB は tests.cmake が外す。
execute_process(COMMAND "${root}/bin/mosatest${EXE_SUFFIX}" --manual "${root}/docs/manual"
    --work "${WORK}/work" --all-tables
  OUTPUT_VARIABLE manual_out ERROR_VARIABLE manual_err RESULT_VARIABLE code TIMEOUT 1800)
set(summary "${manual_out}${manual_err}")
if(NOT code EQUAL 0 OR NOT summary MATCHES "manual: [1-9][0-9]* checked, [0-9]+ skipped, 0 failed")
  message(FATAL_ERROR "mosatest --manual in the package: code=[${code}]\n${summary}")
endif()
