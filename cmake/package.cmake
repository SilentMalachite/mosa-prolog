# リリースのパッケージを作る（リリース 0.1.0 Task 9。設計の「リリースパッケージ」）。ビルド木を cmake --install で
# <OUT>/mosa-<版>-<OS>-<arch>/ に入れ、それを cmake -E tar で固めて <OUT>/mosa-<版>-<OS>-<arch>.tar.gz（Windows は
# .zip）にする。CPack を使わないのは、入れた木そのものを展開して確かめ（tests/package_test.cmake、Task 10 の
# ワークフロー）、名前と中身の形をこの台本だけで決めるため。版と鍵 <OS>-<arch>-<compiler> は BUILD_DIR の
# CMakeCache.txt から読む。最後の行に書庫のパスを標準出力へ書く（ワークフローがそれを拾う）。
# 使い方: cmake -DBUILD_DIR=<ビルド木> -DCONFIG=<Release> -DOUT=<出力のディレクトリ> -P cmake/package.cmake
# テスト: package_layout（tests/tests.cmake、tests/package_test.cmake）。

foreach(required BUILD_DIR CONFIG OUT)
  if(NOT ${required})
    message(FATAL_ERROR "package.cmake: -D${required}=... is required")
  endif()
endforeach()
# 相対パスは呼んだ場所から解く（-P では CMAKE_CURRENT_SOURCE_DIR が今のディレクトリ）。
get_filename_component(build_dir "${BUILD_DIR}" ABSOLUTE)
get_filename_component(out_dir "${OUT}" ABSOLUTE)

# 実行時ライブラリは Release だけ入れる（CMakeLists.txt の CONFIGURATIONS Release）。ほかの構成では mosac build に要る
# lib/mosa/<鍵>/ の無いパッケージができてしまうので止める。
if(NOT CONFIG STREQUAL "Release")
  message(FATAL_ERROR "package.cmake: packages are made from Release only (CONFIG is '${CONFIG}')")
endif()

set(cache "${build_dir}/CMakeCache.txt")
if(NOT EXISTS "${cache}")
  message(FATAL_ERROR "package.cmake: ${cache} does not exist; configure the build first")
endif()
# 1 つの構成の生成器では、--config Release を渡しても Debug の木の物を Release として入れてしまうので、木の構成も見る。
# file(STRINGS) は ; で行を分けるので、CMAKE_CONFIGURATION_TYPES は最初の構成だけになるが、あるかどうかだけを見る。
foreach(entry CMAKE_PROJECT_VERSION MOSA_ABI CMAKE_BUILD_TYPE CMAKE_CONFIGURATION_TYPES)
  set(cached_${entry} "")
  file(STRINGS "${cache}" lines REGEX "^${entry}:[A-Z]+=")
  if(lines)
    list(GET lines 0 line)
    string(REGEX REPLACE "^${entry}:[A-Z]+=" "" cached_${entry} "${line}")
  endif()
endforeach()
if(NOT cached_CMAKE_PROJECT_VERSION OR NOT cached_MOSA_ABI)
  message(FATAL_ERROR "package.cmake: ${cache} has no CMAKE_PROJECT_VERSION or MOSA_ABI; configure it again")
endif()
if(NOT cached_CMAKE_CONFIGURATION_TYPES AND NOT cached_CMAKE_BUILD_TYPE STREQUAL "Release")
  message(FATAL_ERROR "package.cmake: ${build_dir} is configured as '${cached_CMAKE_BUILD_TYPE}', not Release")
endif()

# 名前の <OS>-<arch> は鍵の前の 2 つ（macos-arm64、linux-x86_64、windows-x86_64）。コンパイラは名前に入れない。
string(REGEX MATCH "^[^-]+-[^-]+" os_arch "${cached_MOSA_ABI}")
set(name "mosa-${cached_CMAKE_PROJECT_VERSION}-${os_arch}")
set(stage "${out_dir}/${name}")
if(os_arch MATCHES "^windows-")
  set(archive "${out_dir}/${name}.zip")
  set(tar_args cf "${archive}" --format=zip)
else()
  set(archive "${out_dir}/${name}.tar.gz")
  set(tar_args cfz "${archive}")
endif()

# 前の回の物が残っていると、消したファイルが新しい書庫に紛れ込む。
file(REMOVE_RECURSE "${stage}" "${archive}")
file(MAKE_DIRECTORY "${out_dir}")
# ENCODING UTF-8: 子は UTF-8 で書く。Windows の既定は CMP0176（3.31）が NEW でなければ AUTO で、コンソールのコードページで読み、932 などでは化ける。
execute_process(COMMAND "${CMAKE_COMMAND}" --install "${build_dir}" --prefix "${stage}" --config "${CONFIG}"
  OUTPUT_VARIABLE install_out ERROR_VARIABLE install_err RESULT_VARIABLE code ENCODING UTF-8)
if(NOT code EQUAL 0)
  message(FATAL_ERROR "package.cmake: cmake --install failed (${code})\n${install_out}${install_err}")
endif()

# 書庫の中はパッケージと同じ名前のフォルダ 1 つにする（マニュアル第 2 章が読者にそう書く）。
execute_process(COMMAND "${CMAKE_COMMAND}" -E tar ${tar_args} "${name}"
  WORKING_DIRECTORY "${out_dir}" OUTPUT_VARIABLE tar_out ERROR_VARIABLE tar_err RESULT_VARIABLE code ENCODING UTF-8)
if(NOT code EQUAL 0 OR NOT EXISTS "${archive}")
  message(FATAL_ERROR "package.cmake: cmake -E tar failed (${code})\n${tar_out}${tar_err}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -E echo "${archive}" ENCODING UTF-8)
