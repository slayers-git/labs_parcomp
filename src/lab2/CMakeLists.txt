cmake_minimum_required (VERSION 3.12)
project (PC_Lab2)

set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

set (SRC
    server.cpp
    lab2.cpp)

add_executable (lab2 ${SRC})
target_link_libraries (lab2 parcomp)
