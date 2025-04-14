cmake_minimum_required(VERSION 3.30)
project(ffdb)

set(CMAKE_CXX_STANDARD 20)

add_executable(ffdb src/main.cpp
        src/FFDB.cpp
        src/FFDB.h
        src/buddy_allocator.cpp
        src/buddy_allocator.h
        src/typedefs.cpp
        src/typedefs.h
        src/allocator.cpp
        src/allocator.h
        src/BTree.h
        src/ref_list.h
)

# Check if the googletest submodule exists
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/external/googletest/CMakeLists.txt")
    message(WARNING "GoogleTest submodule is missing! Run:\ngit submodule update --init --recursive external/googletest")
else()
    add_subdirectory(external/googletest)
    enable_testing()

    add_executable(ffdb_tests
            tests/test_main.cpp
            tests/test_example.cpp
            tests/test_allocator.cpp
            tests/test_ref_list.cpp
            tests/test_btree.cpp
            src/typedefs.cpp
            src/typedefs.h
            src/allocator.cpp
            src/allocator.h
            src/BTree.h
            src/ref_list.h
    )
    target_link_libraries(ffdb_tests gtest gtest_main)
endif()