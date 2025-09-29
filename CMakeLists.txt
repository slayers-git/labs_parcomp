cmake_minimum_required (VERSION 3.12)
project (ParallelComputing)

set (CMAKE_CXX_STANDARD 11)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory (src/parlib)
include_directories (include/parlib)

add_subdirectory (src/lab1)
