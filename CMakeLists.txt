cmake_minimum_required(VERSION 3.14)

# set the project name
project(PH_Tree_Main VERSION 1.1.1
        DESCRIPTION "PH-Tree C++"
        LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -Wall -Werror")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

add_subdirectory(phtree)
add_subdirectory(examples)
