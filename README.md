# FFDB

FFDB is a C++20 experimental database engine developed by Lukas Gartman and Andreas Palmqvist as part of a master's thesis at the University of Gothenburg. It reproduces the NGF database architecture, using a persistent memory-mapped structure with copy-on-write transactions to evaluate indexing structures, key storage, and page-aware allocation.

The full thesis is available in the [University of Gothenburg’s research repository](https://gupea.ub.gu.se/items/73450632-9939-475e-b61b-4529a0263b0e).

## Requirements

- Linux (or WSL)
- CMake 3.30 or newer
- A C++20 compiler
- Git

## Build

Initialise only the submodules for the targets you want to build:

```sh
git submodule update --init external/googlebenchmark # for benchmarks
git submodule update --init external/googletest      # for tests
```

Configure the project:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
```

Build the sample executable:

```sh
cmake --build build --target ffdb
```

Build the benchmark executables after initialising Google Benchmark:

```sh
cmake --build build --target btree_bench sbt_bench
```

Build the tests after initialising GoogleTest:

```sh
cmake --build build --target ffdb_tests
```

## Run

Run the sample executable:

```sh
./build/ffdb
```

The current sample in `src/main.cpp` stores a counter in `test.db`, increments it, and prints the value. The counter persists between runs.

If the Google Benchmark submodule is available, run the B-tree and Size-Balanced Tree benchmark suites:

```sh
./build/btree_bench
./build/sbt_bench
```

Each suite runs six workloads at 100K, 1M, and 5M operations, with ten repetitions per case. Benchmark results are printed to the terminal.

If the GoogleTest submodule is available, run the tests with:

```sh
ctest --test-dir build --output-on-failure
```
