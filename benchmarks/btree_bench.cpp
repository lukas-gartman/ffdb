#include <benchmark/benchmark.h>
#include <fcntl.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <chrono>
#include <algorithm>
#include <cstdint>
#include <random>
#include "../src/BTreeFunctional.h"
#include "../src/FFDB.h"

std::vector<std::string> read_words_file(const std::string &filename);

static std::vector<std::string> load_words() {
    try { return read_words_file("words.txt"); }
    catch (...) { return read_words_file("benchmarks/words.txt"); }
}

static Size initial_size_for(size_t n) {
    if (n <= 1'000'000)  return 512ull * 1024 * 1024;
    if (n <= 5'000'000)  return   2ull * 1024 * 1024 * 1024;
    if (n <= 20'000'000) return   8ull * 1024 * 1024 * 1024;
    if (n <= 80'000'000) return  32ull * 1024 * 1024 * 1024;
    return 48ull * 1024 * 1024 * 1024;
}

static void build_dataset(std::vector<std::string> &out, const std::vector<std::string> &base, size_t n) {
    out.clear();
    out.reserve(n);
    size_t m = base.size();
    for (size_t i = 0; i < n; ++i) out.push_back(base[i % m]);
}

static void build_unique_dataset(std::vector<std::string> &out, const std::vector<std::string> &base, size_t n, const std::string &prefix, size_t start = 0) {
    out.clear();
    out.reserve(n);
    size_t m = base.size();
    for (size_t i = 0; i < n; ++i) out.push_back(prefix + base[i % m] + ":" + std::to_string(start + i));
}

static void compute_quantiles(const std::vector<double> &l, double &p50, double &p95, double &p99) {
    if (l.empty()) { p50 = p95 = p99 = 0; return; }
    std::vector<double> c = l;
    auto pick = [&](int pct) {
        size_t idx = c.size() * pct / 100;
        std::nth_element(c.begin(), c.begin() + idx, c.end());
        return c[idx];
    };
    p50 = pick(50);
    p95 = pick(95);
    p99 = pick(99);
}

template <int DEGREE>
static void run_wordcount_iteration(benchmark::State &state, size_t N, const std::vector<std::string> &words, const std::string &dbfile) {
    for (auto _ : state) {
        ::unlink(dbfile.c_str());

        FFDB db(dbfile, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, initial_size_for(N));
        db.start_txn(DB_WRITE_TXN);

        auto tree = BTreeFunctional<char, uint64_t, DEGREE>::empty();

        std::vector<double> latencies;
        latencies.reserve(N);
        auto total_start = std::chrono::steady_clock::now();

        for (size_t i = 0; i < N; ++i) {
            auto op_start = std::chrono::steady_clock::now();
            auto t2 = BTreeFunctional<char, uint64_t, DEGREE>::update_value(
                tree, words[i], [](uint64_t v){ return v + 1; });
            tree = (t2 == tree)
                ? BTreeFunctional<char, uint64_t, DEGREE>::insert(tree, words[i], 1)
                : t2;
            auto op_end = std::chrono::steady_clock::now();
            double ns = (double) std::chrono::duration_cast<std::chrono::nanoseconds>(op_end - op_start).count();
            latencies.push_back(ns);
        }

        db.set_new_txn_root(tree.as_offset());
        auto total_end = std::chrono::steady_clock::now();
        double total_ns = (double) std::chrono::duration_cast<std::chrono::nanoseconds>(total_end - total_start).count();
        db.commit_txn();

        double p50, p95, p99;
        compute_quantiles(latencies, p50, p95, p99);
        state.counters["throughput_ops_ms"] = (double) N / (total_ns / 1e6);
        state.counters["latency_p50_ns"] = p50;
        state.counters["latency_p95_ns"] = p95;
        state.counters["latency_p99_ns"] = p99;
    }
}

template <int DEGREE>
static void WordCount(benchmark::State &state) {
    auto N = (size_t) state.range(0);
    std::string dbfile = "wordcount_" + std::to_string(N) + ".db";

    auto words_all = load_words();
    std::vector<std::string> words;
    build_dataset(words, words_all, N);

    run_wordcount_iteration<DEGREE>(state, N, words, dbfile);
}

static void build_random_queries(const std::vector<std::string> &words, size_t N, std::vector<std::string> &queries, int miss_pct = 10, uint64_t seed = 123456789ULL) {
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<size_t> pick(0, words.size() - 1);
    queries.clear();
    queries.reserve(N);
    size_t miss_every = miss_pct > 0 ? 100 / miss_pct : 0;
    for (size_t i = 0; i < N; ++i) {
        if (miss_pct > 0 && miss_every && (i % miss_every == 0)) {
            queries.push_back("_guaranteed_unique_key" + std::to_string(i));
        } else {
            queries.push_back(words[pick(rng)]);
        }
    }
}

template <int DEGREE>
static void run_random_lookup_iteration(benchmark::State &state, size_t N, const std::vector<std::string> &words, const std::vector<std::string> &queries, const std::string &dbfile) {
    for (auto _ : state) {
        ::unlink(dbfile.c_str());
        {
            FFDB db_build(dbfile, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, initial_size_for(N));
            db_build.start_txn(DB_WRITE_TXN);
            auto tree_build = BTreeFunctional<char, uint64_t, DEGREE>::empty();
            for (size_t i = 0; i < N; ++i) {
                auto t2 = BTreeFunctional<char, uint64_t, DEGREE>::update_value(tree_build, words[i], [](uint64_t v){ return v + 1; });
                tree_build = t2 == tree_build
                    ? BTreeFunctional<char, uint64_t, DEGREE>::insert(tree_build, words[i], 1)
                    : t2;
            }
            db_build.set_new_txn_root(tree_build.as_offset());
            db_build.commit_txn();
        }

        FFDB db(dbfile, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, initial_size_for(N));
        db.start_txn(DB_READ_TXN);
        auto tree = db.template get_txn_root<BTreeHeader<char, uint64_t, DEGREE>>();

        std::vector<double> latencies;
        latencies.reserve(queries.size());
        size_t hits = 0;

        auto total_start = std::chrono::steady_clock::now();
        for (const auto &q : queries) {
            auto op_start = std::chrono::steady_clock::now();
            auto val = BTreeFunctional<char, uint64_t, DEGREE>::get_value(tree, q);
            auto op_end = std::chrono::steady_clock::now();
            if (val.has_value()) hits++;
            double ns = (double) std::chrono::duration_cast<std::chrono::nanoseconds>(op_end - op_start).count();
            latencies.push_back(ns);
        }
        auto total_end = std::chrono::steady_clock::now();
        double total_ns = (double) std::chrono::duration_cast<std::chrono::nanoseconds>(total_end - total_start).count();

        double p50, p95, p99;
        compute_quantiles(latencies, p50, p95, p99);
        state.counters["throughput_ops_ms"] = (double) queries.size() / (total_ns / 1e6);
        state.counters["latency_p50_ns"] = p50;
        state.counters["latency_p95_ns"] = p95;
        state.counters["latency_p99_ns"] = p99;
        state.counters["hit_rate_pct"] = queries.empty() ? 0.0 : (100.0 * hits / queries.size());

        db.commit_txn();
    }
}

template <int DEGREE>
static void RandomLookup(benchmark::State &state) {
    auto N = (size_t) state.range(0);
    std::string dbfile = "random_lookup_" + std::to_string(N) + ".db";

    auto words_all = load_words();
    std::vector<std::string> words;
    build_dataset(words, words_all, N);

    std::vector<std::string> queries;
    build_random_queries(words, N, queries, 10);

    run_random_lookup_iteration<DEGREE>(state, N, words, queries, dbfile);
}

template <int DEGREE>
static void run_insert_iteration(benchmark::State &state, size_t N, const std::vector<std::string> &words_all, const std::string &dbfile) {
    for (auto _ : state) {
        ::unlink(dbfile.c_str());

        FFDB db(dbfile, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, initial_size_for(N));
        db.start_txn(DB_WRITE_TXN);

        auto tree = BTreeFunctional<char, uint64_t, DEGREE>::empty();

        std::vector<std::string> ins_keys;
        build_unique_dataset(ins_keys, words_all, N, "ins:");

        std::vector<double> latencies;
        latencies.reserve(N);
        auto total_start = std::chrono::steady_clock::now();
        for (size_t i = 0; i < N; ++i) {
            auto op_start = std::chrono::steady_clock::now();
            tree = BTreeFunctional<char, uint64_t, DEGREE>::insert(tree, ins_keys[i], 1);
            auto op_end = std::chrono::steady_clock::now();
            double ns = (double) std::chrono::duration_cast<std::chrono::nanoseconds>(op_end - op_start).count();
            latencies.push_back(ns);
        }
        db.set_new_txn_root(tree.as_offset());
        auto total_end = std::chrono::steady_clock::now();
        double total_ns = (double) std::chrono::duration_cast<std::chrono::nanoseconds>(total_end - total_start).count();
        db.commit_txn();

        double p50, p95, p99;
        compute_quantiles(latencies, p50, p95, p99);
        state.counters["throughput_ops_ms"] = (double) N / (total_ns / 1e6);
        state.counters["latency_p50_ns"] = p50;
        state.counters["latency_p95_ns"] = p95;
        state.counters["latency_p99_ns"] = p99;
    }
}

template <int DEGREE>
static void Insert(benchmark::State &state) {
    auto N = (size_t) state.range(0);
    std::string dbfile = "insert_only_" + std::to_string(N) + ".db";
    auto words_all = load_words();
    run_insert_iteration<DEGREE>(state, N, words_all, dbfile);
}

BENCHMARK_TEMPLATE(WordCount, 5)->Arg(1'000'000)->Arg(5'000'000)->Arg(20'000'000)->Iterations(1)->Repetitions(10)->Unit(benchmark::kMillisecond);
BENCHMARK_TEMPLATE(RandomLookup, 5)->Arg(1'000'000)->Arg(5'000'000)->Arg(20'000'000)->Iterations(1)->Repetitions(10)->Unit(benchmark::kMillisecond);
BENCHMARK_TEMPLATE(Insert, 5)->Arg(100'000)->Arg(1'000'000)->Arg(5'000'000)->Iterations(1)->Repetitions(10)->Unit(benchmark::kMillisecond);
