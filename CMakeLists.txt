cmake_minimum_required(VERSION 3.30)
project(ffdb)

set(CMAKE_CXX_STANDARD 20)

set(FFDB_SOURCES
        src/FFDB.cpp
        src/FFDB.h
    src/Typedefs.cpp
    src/Typedefs.h
    src/BTree.h
        src/BTreeFunctional.h
    src/Synch.h
    src/SBT.h
    src/FFDBKey.h
)

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
        tests/test_btree_functional.cpp
        tests/test_sbt.cpp
            tests/test_sbt_functional.cpp
        tests/test_ffdb_key.cpp
        ${FFDB_SOURCES}
    )
    target_link_libraries(ffdb_tests gtest gtest_main)
endif()

# Check if the googlebenchmark submodule exists
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/external/googlebenchmark/CMakeLists.txt")
    message(WARNING "GoogleBenchmark submodule is missing! Run:\ngit submodule update --init --recursive external/googlebenchmark")
else()
    set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "Disable Google Benchmark's own tests")
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "Disable Google Benchmark's own tests")
    add_subdirectory(external/googlebenchmark)

    add_executable(ffdb_benchmarks
        benchmarks/main.cpp
        benchmarks/btree_bench.cpp
        #benchmarks/sbt_bench.cpp
        ${FFDB_SOURCES}
    )
    target_link_libraries(ffdb_benchmarks benchmark::benchmark pthread)
endif()

add_executable(ffdb
    src/main.cpp
    ${FFDB_SOURCES}
        src/Memutils.h
)
