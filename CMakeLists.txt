cmake_minimum_required(VERSION 3.28)
project(VThreadPrac)

# GCC 14.1 bug workaround: https://gitlab.kitware.com/cmake/cmake/-/issues/25974
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS FALSE)

include(CTest)
enable_testing()

set(vtp_compile_options
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /Zc:preprocessor /Zc:__cplusplus /source-charset:utf-8>
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Wpedantic>
)

add_subdirectory(01_spinlock_mutex)
add_subdirectory(02_mutex)
add_subdirectory(03_petersons_algorithm)
