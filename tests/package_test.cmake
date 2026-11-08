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
file(MAKE_DIRECTORY "${unpacked}")

# ENCODING UTF-8: Windows のコンソールのコードページが 65001 でないと、印字された「展開 先」が化ける（3.11 から使える）。
execute_process(COMMAND "${CMAKE_COMMAND}" "-DBUILD_DIR=${BUILD_DIR}" "-DCONFIG=${CONFIG}" "-DOUT=${out}"
    -P "${SRC}/cmake/package.cmake"
  OUTPUT_VARIABLE printed ERROR_VARIABLE err RESULT_VARIABLE code TIMEOUT 600 ENCODING UTF-8)
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
string(REGEX REPLACE "^.*\n" "" printed_last "${printed}")
# パス全体ではなく ASCII だけのファイル名で比べ、書庫は印字ではなく自分で組んだパスで探す。デコードが外れても、
# 日本語のディレクトリ名がこの確かめを壊さないように。
set(archive "${out}/${name}${extension}")
get_filename_component(printed_name "${printed_last}" NAME)
if(NOT printed_name STREQUAL "${name}${extension}" OR NOT EXISTS "${archive}")
  message(FATAL_ERROR "the last line is not the archive ${archive}: [${printed}]")
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
  # Linux の CI で書庫が空に展開された（run 42435938912）。原因を絞るため、書庫の大きさと目録、入れた木の数、
  # ロケールを出し、LC_ALL=C.UTF-8 で展開し直した結果も並べる。
  file(SIZE "${archive}" archive_size)
  execute_process(COMMAND "${CMAKE_COMMAND}" -E tar tf "${archive}"
    OUTPUT_VARIABLE listing ERROR_VARIABLE listing_err RESULT_VARIABLE listing_code)
  string(SUBSTRING "${listing}" 0 1500 listing)
  file(GLOB_RECURSE staged LIST_DIRECTORIES false RELATIVE "${out}/${name}" "${out}/${name}/*")
  list(LENGTH staged staged_count)
  file(MAKE_DIRECTORY "${out}/retry")
  execute_process(COMMAND "${CMAKE_COMMAND}" -E env LC_ALL=C.UTF-8 "${CMAKE_COMMAND}" -E tar xf "${archive}"
    WORKING_DIRECTORY "${out}/retry" ERROR_VARIABLE retry_err RESULT_VARIABLE retry_code)
  file(GLOB retry_top LIST_DIRECTORIES true RELATIVE "${out}/retry" "${out}/retry/*")
  message(FATAL_ERROR "the archive must hold one folder ${name}, not [${top}]\n"
    "cmake ${CMAKE_VERSION}; LANG=[$ENV{LANG}] LC_ALL=[$ENV{LC_ALL}]\n"
    "archive: ${archive_size} bytes; staged files: ${staged_count}\n"
    "first extract: out=[${tar_out}] err=[${err}]\n"
    "retry with LC_ALL=C.UTF-8: code=[${retry_code}] err=[${retry_err}] top=[${retry_top}]\n"
    "tar tf: code=[${listing_code}] err=[${listing_err}]\n${listing}")
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

# 展開した木の mosatest は、--mosa を省けば隣の mosa を使う。MOSA_LIB は tests.cmake が外す。2 回流す。
# 1 回目は tests.cmake が PATH の先に置いたビルドの cmake で、mosac の CMake の道を通す。CMake の無い読者
# （Xcode の CLT だけ、g++ だけ）の mosac はコンパイラを直に呼ぶので、2 回目は MOSA_NO_CMAKE=1 でその道を、
# 空白と日本語を含む実行時ライブラリのパスで通す。
foreach(route cmake direct)
  set(no_cmake 0)
  if(route STREQUAL "direct")
    set(no_cmake 1)
  endif()
  file(MAKE_DIRECTORY "${WORK}/work-${route}")
  execute_process(COMMAND "${CMAKE_COMMAND}" -E env "MOSA_NO_CMAKE=${no_cmake}"
      "${root}/bin/mosatest${EXE_SUFFIX}" --manual "${root}/docs/manual" --work "${WORK}/work-${route}" --all-tables
    OUTPUT_VARIABLE manual_out ERROR_VARIABLE manual_err RESULT_VARIABLE code TIMEOUT 1800)
  set(summary "${manual_out}${manual_err}")
  if(NOT code EQUAL 0 OR NOT summary MATCHES "manual: [1-9][0-9]* checked, [0-9]+ skipped, 0 failed")
    message(FATAL_ERROR "mosatest --manual in the package (${route}): code=[${code}]\n${summary}")
  endif()
  # 通ったことだけでは、どちらの道を通ったかは分からない。MOSA_NO_CMAKE が効かずに 2 回とも CMake の道を通っても、
  # cmake が見つからずに 2 回とも直の道を通っても、この確かめは通ってしまう。CMake の道だけが CMakeCache.txt を残す。
  file(GLOB_RECURSE caches LIST_DIRECTORIES false "${WORK}/work-${route}/*CMakeCache.txt")
  list(FILTER caches INCLUDE REGEX "/CMakeCache\\.txt$")
  if(route STREQUAL "cmake" AND NOT caches)
    message(FATAL_ERROR "the CMake route left no CMakeCache.txt under ${WORK}/work-${route}")
  elseif(route STREQUAL "direct" AND caches)
    message(FATAL_ERROR "the MOSA_NO_CMAKE=1 route used CMake: ${caches}")
  endif()
endforeach()
