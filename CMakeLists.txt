cmake_minimum_required(VERSION 3.31)
project(sf3snd CXX)

find_package(SDL3 REQUIRED CONFIG REQUIRED COMPONENTS SDL3-shared)
find_package(PkgConfig REQUIRED)

pkg_check_modules(readline readline REQUIRED IMPORTED_TARGET readline)

add_executable(sf3snd)
set_property(TARGET sf3snd PROPERTY CXX_STANDARD 23)
target_compile_options(sf3snd PRIVATE -Wall -Wextra)
# target_compile_options(sf3snd PRIVATE -fsanitize=address,undefined)
# target_link_options(sf3snd PRIVATE -fsanitize=address,undefined)
target_include_directories(sf3snd PRIVATE include)
target_sources(
  sf3snd
  PRIVATE
  src/main.cc
  src/loader.cc
  src/player.cc
)
target_link_libraries(sf3snd PRIVATE SDL3::SDL3 PkgConfig::readline)
