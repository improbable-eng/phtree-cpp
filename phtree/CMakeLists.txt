cmake_minimum_required(VERSION 3.14)
project(phtree)

add_library(phtree STATIC "")
add_subdirectory(common)
add_subdirectory(v16)

set_target_properties(phtree PROPERTIES LINKER_LANGUAGE CXX)

