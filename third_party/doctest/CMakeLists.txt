cmake_minimum_required(VERSION 3.14)
project(doctest LANGUAGES CXX)

add_library(doctest INTERFACE)
add_library(doctest::doctest ALIAS doctest)

target_include_directories(doctest INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
