cmake_minimum_required(VERSION 3.25)

project(mosa VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(mosa_warnings INTERFACE)
target_compile_options(mosa_warnings INTERFACE
  $<$<CXX_COMPILER_ID:MSVC>:/W4;/WX;/utf-8;/permissive-;/Zc:__cplusplus>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall;-Wextra;-Wpedantic;-Werror>
)

add_library(mosa_lib STATIC src/version.cpp)
target_include_directories(mosa_lib PUBLIC include)
set_target_properties(mosa_lib PROPERTIES OUTPUT_NAME mosa)
target_link_libraries(mosa_lib PUBLIC mosa_warnings)

add_executable(mosa src/main_mosa.cpp)
target_link_libraries(mosa PRIVATE mosa_lib)

add_executable(mosac src/main_mosac.cpp)
target_link_libraries(mosac PRIVATE mosa_lib)

add_executable(mosatest src/main_mosatest.cpp)
target_link_libraries(mosatest PRIVATE mosa_lib)
