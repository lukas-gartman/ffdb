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
            tests/test_allocator.cpp
            tests/test_btree.cpp
            src/FFDB2.cpp
            src/FFDB2.h
            src/Typedefs.cpp
            src/Typedefs.h
            src/Synch.h
            tests/test_binary_tree.cpp
    )
    target_link_libraries(ffdb_tests gtest gtest_main)
endif()

add_executable(ffdb src/main.cpp
        src/FFDB2.cpp
        src/FFDB2.h
        src/Typedefs.cpp
        src/Typedefs.h
        src/BTree.h
        src/Synch.h
        src/BinaryTree.h
)
