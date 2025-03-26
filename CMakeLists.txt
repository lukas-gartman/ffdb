cmake_minimum_required(VERSION 3.30)
project(ffdb)

set(CMAKE_CXX_STANDARD 20)

# Check if the googletest submodule exists
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/external/googletest/CMakeLists.txt")
    message(WARNING "GoogleTest submodule is missing! Run:\ngit submodule update --init --recursive external/googletest")
else()
    add_subdirectory(external/googletest)
    enable_testing()

    add_executable(ffdb_tests
            tests/test_main.cpp
            tests/test_example.cpp
            typedefs.cpp
            typedefs.h
    )
    target_link_libraries(ffdb_tests gtest gtest_main)
endif()

add_executable(ffdb main.cpp
        FFDB.cpp
        FFDB.h
        buddy_allocator.cpp
        buddy_allocator.h
        typedefs.cpp
        typedefs.h
        BTree.cpp
        BTree.h
        # Allocator.cpp
        # Allocator.h
)
