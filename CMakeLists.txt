cmake_minimum_required(VERSION 3.30)
project(ffdb)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(external/googletest)
enable_testing()

add_executable(ffdb main.cpp
        FFDB.cpp
        FFDB.h
        buddy_allocator.cpp
        buddy_allocator.h
        typedefs.cpp
        typedefs.h
        # Allocator.cpp
        # Allocator.h
)

add_executable(ffdb_tests
        tests/test_main.cpp
        tests/test_example.cpp
        typedefs.cpp
        typedefs.h
)

target_link_libraries(ffdb_tests gtest gtest_main)