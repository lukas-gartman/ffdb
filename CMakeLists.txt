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
        src/FFDB.cpp
        src/FFDB.h
        src/Typedefs.cpp
        src/Typedefs.h
        src/Allocator.cpp
        src/Allocator.h
        src/MemState.h
        src/BTree.h
        src/Synch.h
        src/MemoryMapping.h
        src/MemoryMapping.cpp
        src/Transaction.h
        tests/test_binary_tree.cpp
    )
    target_link_libraries(ffdb_tests gtest gtest_main)
endif()

# Check if the pybind11 submodule exists
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/external/pybind11/CMakeLists.txt")
    message(WARNING "PyBind11 submodule is missing! Run:\ngit submodule update --init --recursive external/pybind11")
else()
    add_subdirectory(external/pybind11)

    pybind11_add_module(ffdbcore src/bindings.cpp
        src/FFDB.cpp
        src/Typedefs.cpp
        src/Allocator.cpp
        src/MemoryMapping.cpp
    )

    set_target_properties(ffdbcore PROPERTIES
        OUTPUT_NAME ffdbcore
        SUFFIX .so
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    )
endif()

set(FFDB_SOURCES
    src/FFDB.cpp
    src/Typedefs.cpp
    src/Allocator.cpp
    src/MemoryMapping.cpp
)

set(FFDB_HEADERS
    src/FFDB.h
    src/Typedefs.h
    src/Allocator.h
    src/MemState.h
    src/BTree.h
    src/Synch.h
    src/MemoryMapping.h
    src/Transaction.h
    src/BinaryTree.h
)

add_executable(ffdb src/main.cpp ${FFDB_SOURCES} ${FFDB_HEADERS})
