cmake_minimum_required(VERSION 3.30)
project(ffdb)

set(CMAKE_CXX_STANDARD 20)

set(FFDB_SOURCES
    src/FFDB.cpp
    src/FFDB.h
    src/Typedefs.cpp
    src/Typedefs.h
    src/BTreeFunctional.h
    src/SBTFunctional.h
    src/Synch.h
    src/FFDBKey.h
    src/Memutils.h
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
        tests/test_ffdb.cpp
        tests/test_btree_functional.cpp
        tests/test_sbt_functional.cpp
        tests/test_ffdb_key.cpp
        tests/test_memutils.cpp
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

    set(BENCH_COMMON
        benchmarks/main.cpp
        benchmarks/utils.cpp
        ${FFDB_SOURCES}
    )

    add_executable(btree_bench
        benchmarks/btree_bench.cpp
        ${BENCH_COMMON}
    )
    target_link_libraries(btree_bench benchmark::benchmark pthread)
    target_compile_options(btree_bench PRIVATE -march=native -mtune=native -fno-omit-frame-pointer)

    # Allow configuration overrides
    if(DEFINED FFDB_LOGICAL_PAGE_SIZE)
        target_compile_definitions(btree_bench PRIVATE FFDB_LOGICAL_PAGE_SIZE=${FFDB_LOGICAL_PAGE_SIZE})
    endif()
    if(DEFINED FFDB_PAGE_BOUNDARY_AWARE)
        target_compile_definitions(btree_bench PRIVATE FFDB_PAGE_BOUNDARY_AWARE=${FFDB_PAGE_BOUNDARY_AWARE})
    endif()
    if(DEFINED FFDB_KEY_PREFIX_SIZE)
        target_compile_definitions(btree_bench PRIVATE FFDB_KEY_PREFIX_SIZE=${FFDB_KEY_PREFIX_SIZE})
    endif()
    if(DEFINED BENCH_DEGREE)
        target_compile_definitions(btree_bench PRIVATE BENCH_DEGREE=${BENCH_DEGREE})
    endif()

    add_executable(sbt_bench
        benchmarks/sbt_bench.cpp
        ${BENCH_COMMON}
    )
    target_link_libraries(sbt_bench benchmark::benchmark pthread)
    target_compile_options(sbt_bench PRIVATE -march=native -mtune=native -fno-omit-frame-pointer)

    # Allow configuration overrides
    if(DEFINED FFDB_LOGICAL_PAGE_SIZE)
        target_compile_definitions(sbt_bench PRIVATE FFDB_LOGICAL_PAGE_SIZE=${FFDB_LOGICAL_PAGE_SIZE})
    endif()
    if(DEFINED FFDB_KEY_PREFIX_SIZE)
        target_compile_definitions(sbt_bench PRIVATE FFDB_KEY_PREFIX_SIZE=${FFDB_KEY_PREFIX_SIZE})
    endif()
endif()

add_executable(ffdb
    src/main.cpp
    ${FFDB_SOURCES}
)
