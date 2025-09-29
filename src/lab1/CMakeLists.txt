cmake_minimum_required (VERSION 3.12)
project (PC_Lab1)

set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

add_executable (lab1 lab1.cpp)
target_link_libraries (lab1 parcomp)
