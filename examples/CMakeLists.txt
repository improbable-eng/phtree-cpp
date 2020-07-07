cmake_minimum_required(VERSION 3.12)
project(Example)

set(SOURCE_FILES example.cc)
add_executable(Example ${SOURCE_FILES})
target_link_libraries(Example phtree)
