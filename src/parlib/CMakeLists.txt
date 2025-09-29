cmake_minimum_required (VERSION 3.12)
project (ParLib)

set (CMAKE_CXX_STANDARD 11)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

set (SRC
    ${CMAKE_SOURCE_DIR}/include/parlib/perf_clock.hpp
    ${CMAKE_SOURCE_DIR}/include/parlib/perf_timepoint.hpp
    ${CMAKE_SOURCE_DIR}/include/parlib/perf_timeduration.hpp
    ${CMAKE_SOURCE_DIR}/src/parlib/platform/impl_$<IF:$<BOOL:${WIN32}>,win32,posix>_perfclock.cpp)

add_library (parcomp STATIC)
target_sources (parcomp PUBLIC ${SRC})

target_include_directories (parcomp PUBLIC ${CMAKE_SOURCE_DIR}/include/parlib)
