#include <benchmark/benchmark.h>
#include "../src/BTree.h"
#include "../src/FFDB.h"
#include "../src/FFDBKey.h"
#include <fcntl.h>

template <int DEGREE>
static void BTree_Insert(benchmark::State& state) {
    remove("benchmark.db");
    FFDB db("benchmark.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 1024 * 1024 * 1024 * 1.9);
    db.start_txn(DB_WRITE_TXN);

    BTree::BTree<int, float, DEGREE> tree;

    for (auto _ : state) {
        for (int i = 0; i < state.range(0); ++i) {
            tree.insert(FFDBKey<int>(i), i + 0.5f);
        }
    }

    db.commit_txn();
    remove("benchmark.db");

    state.counters["Throughput (inserts/sec)"] = benchmark::Counter(
        static_cast<double>(state.range(0)), benchmark::Counter::kIsRate
    );
}

BENCHMARK_TEMPLATE(BTree_Insert, 3)
    // ->Arg(1'000)
    // ->Arg(10'000)
    // ->Arg(100'000)
    ->Arg(1'000'000)
    ->Unit(benchmark::kMillisecond);
    // ->Repetitions(5)
    // ->ReportAggregatesOnly(true);
BENCHMARK_TEMPLATE(BTree_Insert, 4)
    // ->Arg(1'000)
    // ->Arg(10'000)
    // ->Arg(100'000)
    ->Arg(1'000'000)
    ->Unit(benchmark::kMillisecond);
    // ->Repetitions(5)
    // ->ReportAggregatesOnly(true);
BENCHMARK_TEMPLATE(BTree_Insert, 5)
    // ->Arg(1'000)
    // ->Arg(10'000)
    // ->Arg(100'000)
    ->Arg(1'000'000)
    ->Unit(benchmark::kMillisecond);
    // ->Repetitions(5)
    // ->ReportAggregatesOnly(true);
BENCHMARK_TEMPLATE(BTree_Insert, 6)
    // ->Arg(1'000)
    // ->Arg(10'000)
    // ->Arg(100'000)
    ->Arg(1'000'000)
    ->Unit(benchmark::kMillisecond);
    // ->Repetitions(5)
    // ->ReportAggregatesOnly(true);
BENCHMARK_TEMPLATE(BTree_Insert, 7)
    // ->Arg(1'000)
    // ->Arg(10'000)
    // ->Arg(100'000)
    ->Arg(1'000'000)
    ->Unit(benchmark::kMillisecond);
    // ->Repetitions(5)
    // ->ReportAggregatesOnly(true);
BENCHMARK_TEMPLATE(BTree_Insert, 8)
    // ->Arg(1'000)
    // ->Arg(10'000)
    // ->Arg(100'000)
    ->Arg(1'000'000)
    ->Unit(benchmark::kMillisecond);
    // ->Repetitions(5)
    // ->ReportAggregatesOnly(true);