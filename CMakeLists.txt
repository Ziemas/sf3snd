cmake_minimum_required(VERSION 3.31)
project(cps3snd C CXX)

find_package(SDL3 REQUIRED CONFIG REQUIRED COMPONENTS SDL3-shared)
find_package(PkgConfig REQUIRED)
pkg_check_modules(readline readline REQUIRED IMPORTED_TARGET readline)

add_library(cps3snd)
set_property(TARGET cps3snd PROPERTY CXX_STANDARD 23)
target_compile_options(cps3snd PRIVATE -Wall -Wextra)
target_include_directories(cps3snd PRIVATE include/private)
target_sources(cps3snd
  PRIVATE
    src/cps3snd.cc
    src/loader.cc
    src/player.cc
    src/tables.cc

  PUBLIC
    FILE_SET header
    TYPE HEADERS
    BASE_DIRS
        include 
    FILES
        include/cps3snd.h
)

add_executable(cps3play)
set_property(TARGET cps3play PROPERTY CXX_STANDARD 23)
target_compile_options(cps3play PRIVATE -Wall -Wextra)
# target_compile_options(cps3play PRIVATE -fsanitize=address,undefined)
# target_link_options(cps3play PRIVATE -fsanitize=address,undefined)
target_include_directories(cps3play PRIVATE include)
target_sources(
  cps3play
  PRIVATE
  src/main.cc
)
target_link_libraries(cps3play PRIVATE cps3snd SDL3::SDL3 PkgConfig::readline)
