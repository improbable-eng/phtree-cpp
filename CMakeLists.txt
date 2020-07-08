cmake_minimum_required(VERSION 3.14)

# set the project name
project(PH_Tree_Main VERSION 0.1.0
        DESCRIPTION "PH-Tree C++"
        LANGUAGES CXX)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")

add_subdirectory(phtree)
add_subdirectory(examples)
