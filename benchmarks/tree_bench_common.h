#pragma once

#include <benchmark/benchmark.h>
#include <fcntl.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <chrono>
#include <algorithm>
#include <cstdint>
#include <random>
#include <variant>
#include <cstring>
#include "../src/FFDB.h"
#include "../src/FFDBKey.h"

// Mixed workload operation distribution
static constexpr int MIXED_INSERT_PCT = 20;
static constexpr int MIXED_REMOVE_PCT = 10;
// MIXED_LOOKUP_PCT = 100 - MIXED_INSERT_PCT - MIXED_REMOVE_PCT = 70%

std::vector<std::string> read_words_file(const std::string &filename);

static std::vector<std::string> load_words() {
    try { return read_words_file("words.txt"); }
    catch (...) { return read_words_file("benchmarks/words.txt"); }
}

static Size initial_size_for(Size n) {
    if (n <= 1'000'000)  return 512ull * 1024 * 1024;
    if (n <= 5'000'000)  return   2ull * 1024 * 1024 * 1024;
    if (n <= 20'000'000) return   8ull * 1024 * 1024 * 1024;
    if (n <= 80'000'000) return  32ull * 1024 * 1024 * 1024;
    return 48ull * 1024 * 1024 * 1024;
}

static void build_dataset(std::vector<std::string> &out, const std::vector<std::string> &base, Size n) {
    out.clear();
    out.reserve(n);
    Size m = base.size();
    for (Size i = 0; i < n; ++i) out.push_back(base[i % m]);
}

static void build_unique_dataset(std::vector<std::string> &out, const std::vector<std::string> &base, Size n, const std::string &prefix, Size start = 0) {
    out.clear();
    out.reserve(n);
    Size m = base.size();
    for (Size i = 0; i < n; ++i) out.push_back(prefix + base[i % m] + ":" + std::to_string(start + i));
}

static void compute_quantiles(const std::vector<double> &l, double &p50, double &p95, double &p99) {
    if (l.empty()) { p50 = p95 = p99 = 0; return; }
    std::vector<double> c = l;
    auto pick = [&](int pct) {
        Size i = c.size() * pct / 100;
        std::nth_element(c.begin(), c.begin() + i, c.end());
        return c[i];
    };
    p50 = pick(50);
    p95 = pick(95);
    p99 = pick(99);
}

template <typename TreeType>
static void create_empty_db(const std::string &dbfile, Size N) {
    FFDB db(dbfile, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR, initial_size_for(N));
    db.start_txn(DB_WRITE_TXN);
    auto tree = TreeType::empty();
    db.commit_txn(tree.as_offset());
}

static FFDB open_db(const std::string &dbfile) {
    FFDB db(dbfile, O_RDWR, 0, 0);
    return db;
}

template <typename TreeType, typename TreeRef>
static void run_wordcount_iteration(benchmark::State &state, Size N, const std::vector<std::string> &words, const std::string &dbfile) {
    for (auto _ : state) {
        ::unlink(dbfile.c_str());

        create_empty_db<TreeType>(dbfile, N);

        std::vector<double> latencies;
        latencies.reserve(N);
        auto total_start = std::chrono::steady_clock::now();

        FFDB db = open_db(dbfile);
        TreeRef tree = db.start_txn(DB_WRITE_TXN);

        Size halfway_mark = N / 2;
        for (Size i = 0; i < N; ++i) {
            if (i == halfway_mark) {
                db.commit_txn(tree.as_offset());
                tree = db.start_txn(DB_WRITE_TXN);
            }

            auto op_start = std::chrono::steady_clock::now();
            tree = TreeType::upsert(tree, words[i], 1, [](uint64_t v){ return v + 1; });
            auto op_end = std::chrono::steady_clock::now();
            double ns = (double) std::chrono::duration_cast<std::chrono::nanoseconds>(op_end - op_start).count();
            latencies.push_back(ns);
        }

        auto total_end = std::chrono::steady_clock::now();
        double total_ns = (double) std::chrono::duration_cast<std::chrono::nanoseconds>(total_end - total_start).count();
        db.commit_txn(tree.as_offset());

        double p50, p95, p99;
        compute_quantiles(latencies, p50, p95, p99);
        state.counters["throughput_ops_ms"] = (double) N / (total_ns / 1e6);
        state.counters["latency_p50_ns"] = p50;
        state.counters["latency_p95_ns"] = p95;
        state.counters["latency_p99_ns"] = p99;
    }
}

static void build_random_queries(const std::vector<std::string> &words, Size N, std::vector<std::string> &queries, int miss_pct = 10, uint64_t seed = 133769420ULL) {
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<Size> pick(0, words.size() - 1);
    queries.clear();
    queries.reserve(N);
    Size miss_every = miss_pct > 0 ? 100 / miss_pct : 0;
    for (Size i = 0; i < N; ++i) {
        if (miss_pct > 0 && miss_every && (i % miss_every == 0)) {
            queries.push_back("_guaranteed_unique_key" + std::to_string(i));
        } else {
            queries.push_back(words[pick(rng)]);
        }
    }
}

template <typename TreeType, typename TreeRef>
static void run_random_lookup_iteration(benchmark::State &state, Size N, const std::vector<std::string> &words, const std::vector<std::string> &queries, const std::string &dbfile) {
    for (auto _ : state) {
        ::unlink(dbfile.c_str());
        {
            create_empty_db<TreeType>(dbfile, N);

            FFDB db = open_db(dbfile);
            TreeRef tree = db.start_txn(DB_WRITE_TXN);

            Size halfway_mark = N / 2;
            for (Size i = 0; i < N; ++i) {
                if (i == halfway_mark) {
                    db.commit_txn(tree.as_offset());
                    tree = db.start_txn(DB_WRITE_TXN);
                }

                tree = TreeType::upsert(tree, words[i], 1, [](uint64_t v){ return v + 1; });
            }

            db.commit_txn(tree.as_offset());
        }

        FFDB db = open_db(dbfile);
        TreeRef tree = db.start_txn(DB_READ_TXN);

        std::vector<double> latencies;
        latencies.reserve(queries.size());
        Size hits = 0;

        auto total_start = std::chrono::steady_clock::now();
        for (const auto &q : queries) {
            auto op_start = std::chrono::steady_clock::now();
            auto val = TreeType::get_value(tree, q);
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

template <typename TreeType, typename TreeRef>
static void run_sequential_lookup_iteration(benchmark::State &state, Size N, const std::vector<std::string> &words, const std::string &dbfile) {
    for (auto _ : state) {
        ::unlink(dbfile.c_str());
        {
            create_empty_db<TreeType>(dbfile, N);

            FFDB db = open_db(dbfile);
            TreeRef tree = db.start_txn(DB_WRITE_TXN);

            Size halfway_mark = N / 2;
            for (Size i = 0; i < N; ++i) {
                if (i == halfway_mark) {
                    db.commit_txn(tree.as_offset());
                    tree = db.start_txn(DB_WRITE_TXN);
                }

                tree = TreeType::upsert(tree, words[i], 1, [](uint64_t v){ return v + 1; });
            }

            db.commit_txn(tree.as_offset());
        }

        FFDB db = open_db(dbfile);
        TreeRef tree = db.start_txn(DB_READ_TXN);

        // Get keys in their actual storage order via in-order traversal for testing cache locality
        std::vector<std::string> storage_order_keys;
        storage_order_keys.reserve(TreeType::get_size(tree));
        TreeType::traverse_inorder(tree, [&storage_order_keys](const auto &kv) {
            std::string key_str;
            if (std::holds_alternative<char>(kv.key.key_storage)) {
                key_str = std::string(1, std::get<char>(kv.key.key_storage));
            } else {
                const auto& lk = std::get<typename std::decay_t<decltype(kv.key)>::LargeKey>(kv.key.key_storage);
                key_str.resize(kv.key.size);
                if constexpr (PREFIX_SIZE > 0) {
                    std::memcpy(key_str.data(), lk.key_prefix, std::min(static_cast<size_t>(PREFIX_SIZE), kv.key.size));
                }
                if (kv.key.size > PREFIX_SIZE) {
                    std::memcpy(key_str.data() + PREFIX_SIZE, lk.key_ref, kv.key.size - PREFIX_SIZE);
                }
            }
            storage_order_keys.push_back(key_str);
        });

        std::vector<double> latencies;
        latencies.reserve(storage_order_keys.size());
        Size hits = 0;

        auto total_start = std::chrono::steady_clock::now();
        for (const auto &key : storage_order_keys) {
            auto op_start = std::chrono::steady_clock::now();
            auto val = TreeType::get_value(tree, key);
            auto op_end = std::chrono::steady_clock::now();
            if (val.has_value()) hits++;
            double ns = (double) std::chrono::duration_cast<std::chrono::nanoseconds>(op_end - op_start).count();
            latencies.push_back(ns);
        }
        auto total_end = std::chrono::steady_clock::now();
        double total_ns = (double) std::chrono::duration_cast<std::chrono::nanoseconds>(total_end - total_start).count();

        double p50, p95, p99;
        compute_quantiles(latencies, p50, p95, p99);
        state.counters["throughput_ops_ms"] = (double) storage_order_keys.size() / (total_ns / 1e6);
        state.counters["latency_p50_ns"] = p50;
        state.counters["latency_p95_ns"] = p95;
        state.counters["latency_p99_ns"] = p99;
        state.counters["hit_rate_pct"] = storage_order_keys.empty() ? 0.0 : (100.0 * hits / storage_order_keys.size());

        db.commit_txn();
    }
}

template <typename TreeType, typename TreeRef>
static void run_insert_iteration(benchmark::State &state, Size N, const std::vector<std::string> &words_all, const std::string &dbfile) {
    for (auto _ : state) {
        ::unlink(dbfile.c_str());

        create_empty_db<TreeType>(dbfile, N);

        FFDB db = open_db(dbfile);
        TreeRef tree = db.start_txn(DB_WRITE_TXN);

        std::vector<std::string> ins_keys;
        build_unique_dataset(ins_keys, words_all, N, "ins:");

        std::vector<double> latencies;
        latencies.reserve(N);
        auto total_start = std::chrono::steady_clock::now();

        Size halfway_mark = N / 2;
        for (Size i = 0; i < N; ++i) {
            if (i == halfway_mark) {
                db.commit_txn(tree.as_offset());
                tree = db.start_txn(DB_WRITE_TXN);
            }

            auto op_start = std::chrono::steady_clock::now();
            tree = TreeType::insert(tree, ins_keys[i], 1);
            auto op_end = std::chrono::steady_clock::now();
            double ns = (double) std::chrono::duration_cast<std::chrono::nanoseconds>(op_end - op_start).count();
            latencies.push_back(ns);
        }

        auto total_end = std::chrono::steady_clock::now();
        double total_ns = (double) std::chrono::duration_cast<std::chrono::nanoseconds>(total_end - total_start).count();
        db.commit_txn(tree.as_offset());

        double p50, p95, p99;
        compute_quantiles(latencies, p50, p95, p99);
        state.counters["throughput_ops_ms"] = (double) N / (total_ns / 1e6);
        state.counters["latency_p50_ns"] = p50;
        state.counters["latency_p95_ns"] = p95;
        state.counters["latency_p99_ns"] = p99;
    }
}

template <typename TreeType, typename TreeRef>
static void run_remove_iteration(benchmark::State &state, Size N, const std::vector<std::string> &words_all, const std::string &dbfile) {
    for (auto _ : state) {
        ::unlink(dbfile.c_str());

        create_empty_db<TreeType>(dbfile, N);

        FFDB db = open_db(dbfile);
        TreeRef tree = db.start_txn(DB_WRITE_TXN);

        std::vector<std::string> keys;
        build_unique_dataset(keys, words_all, N, "rem:");

        Size halfway_mark = N / 2;
        for (Size i = 0; i < N; ++i) {
            if (i == halfway_mark) {
                db.commit_txn(tree.as_offset());
                tree = db.start_txn(DB_WRITE_TXN);
            }

            tree = TreeType::insert(tree, keys[i], 1);
        }

        std::vector<std::string> rem_keys;
        build_unique_dataset(rem_keys, words_all, N, "rem:");

        std::vector<double> latencies;
        latencies.reserve(N);
        Size removed_cnt = 0;

        auto total_start = std::chrono::steady_clock::now();
        for (Size i = 0; i < N; ++i) {
            if (i == halfway_mark) {
                db.commit_txn(tree.as_offset());
                tree = db.start_txn(DB_WRITE_TXN);
            }

            auto op_start = std::chrono::steady_clock::now();
            auto [t2, removed] = TreeType::remove(tree, rem_keys[i]);
            if (removed) {
                tree = t2;
                removed_cnt++;
            }

            auto op_end = std::chrono::steady_clock::now();
            double ns = (double) std::chrono::duration_cast<std::chrono::nanoseconds>(op_end - op_start).count();
            latencies.push_back(ns);
        }

        auto total_end = std::chrono::steady_clock::now();
        double total_ns = (double) std::chrono::duration_cast<std::chrono::nanoseconds>(total_end - total_start).count();

        db.commit_txn(tree.as_offset());

        double p50, p95, p99;
        compute_quantiles(latencies, p50, p95, p99);
        state.counters["throughput_ops_ms"] = (double) N / (total_ns / 1e6);
        state.counters["latency_p50_ns"] = p50;
        state.counters["latency_p95_ns"] = p95;
        state.counters["latency_p99_ns"] = p99;
        state.counters["removed_pct"] = N == 0 ? 0.0 : 100.0 * (double)removed_cnt / (double)N;
    }
}

enum class Op { LOOKUP, INSERT, REMOVE };

static void build_mixed_ops(std::vector<Op> &ops, Size total, uint64_t seed = 420691337ULL) {
    Size inserts = (total * MIXED_INSERT_PCT) / 100;
    Size removes = (total * MIXED_REMOVE_PCT) / 100;
    Size lookups = total - inserts - removes;
    ops.clear();
    ops.reserve(total);
    ops.insert(ops.end(), lookups, Op::LOOKUP);
    ops.insert(ops.end(), inserts, Op::INSERT);
    ops.insert(ops.end(), removes, Op::REMOVE);
    std::mt19937_64 rng(seed);
    std::shuffle(ops.begin(), ops.end(), rng);
}

template <typename TreeType, typename TreeRef>
static void run_mixed_iteration(benchmark::State &state, Size N, const std::vector<std::string> &words_all, const std::string &dbfile) {
    for (auto _ : state) {
        ::unlink(dbfile.c_str());

        create_empty_db<TreeType>(dbfile, N);

        FFDB db = open_db(dbfile);
        TreeRef tree = db.start_txn(DB_WRITE_TXN);

        std::vector<std::string> base_keys;
        build_unique_dataset(base_keys, words_all, N, "mix:");

        for (Size i = 0; i < base_keys.size(); ++i) {
            tree = TreeType::insert(tree, base_keys[i], 1);
        }

        Size inserts = (N * MIXED_INSERT_PCT) / 100;
        Size removes = (N * MIXED_REMOVE_PCT) / 100;
        Size lookups = N - inserts - removes;

        std::vector<std::string> ins_keys;
        build_unique_dataset(ins_keys, words_all, inserts, "mix:ins:");

        std::vector<std::string> rem_keys;
        rem_keys.reserve(removes);
        for (Size i = 0; i < removes; ++i) rem_keys.push_back(base_keys[i]);

        std::vector<std::string> queries;
        build_random_queries(base_keys, lookups, queries, 10);

        std::vector<Op> ops;
        build_mixed_ops(ops, N);

        std::vector<double> latencies;
        latencies.reserve(N);

        Size look_i = 0, ins_i = 0, rem_i = 0;
        Size lookup_hits = 0, lookup_cnt = 0, removed_cnt = 0;

        auto total_start = std::chrono::steady_clock::now();
        Size halfway_mark = N / 2;
        for (Size i = 0; i < N; ++i) {
            if (i == halfway_mark) {
                db.commit_txn(tree.as_offset());
                tree = db.start_txn(DB_WRITE_TXN);
            }

            auto op_start = std::chrono::steady_clock::now();
            Op op = ops[i];
            if (op == Op::LOOKUP) {
                if (look_i < queries.size()) {
                    auto val = TreeType::get_value(tree, queries[look_i++]);
                    if (val.has_value()) lookup_hits++;
                    lookup_cnt++;
                } else {
                    auto val = TreeType::get_value(tree, base_keys[0]);
                    (void)val;
                }
            } else if (op == Op::INSERT) {
                if (ins_i < ins_keys.size()) {
                    tree = TreeType::insert(tree, ins_keys[ins_i++], 1);
                } else {
                    auto val = TreeType::get_value(tree, base_keys[0]);
                    (void)val;
                }
            } else {
                if (rem_i < rem_keys.size()) {
                    auto [t2, removed] = TreeType::remove(tree, rem_keys[rem_i++]);
                    if (removed) { tree = t2; removed_cnt++; }
                } else {
                    auto val = TreeType::get_value(tree, base_keys[0]);
                    (void)val;
                }
            }

            auto op_end = std::chrono::steady_clock::now();
            double ns = (double) std::chrono::duration_cast<std::chrono::nanoseconds>(op_end - op_start).count();
            latencies.push_back(ns);
        }

        auto total_end = std::chrono::steady_clock::now();
        double total_ns = (double) std::chrono::duration_cast<std::chrono::nanoseconds>(total_end - total_start).count();

        db.commit_txn(tree.as_offset());

        double p50, p95, p99;
        compute_quantiles(latencies, p50, p95, p99);
        state.counters["throughput_ops_ms"] = (double) N / (total_ns / 1e6);
        state.counters["latency_p50_ns"] = p50;
        state.counters["latency_p95_ns"] = p95;
        state.counters["latency_p99_ns"] = p99;
        state.counters["lookup_hit_rate_pct"] = lookup_cnt == 0 ? 0.0 : (100.0 * (double)lookup_hits / (double)lookup_cnt);
        state.counters["removed_pct"] = removes == 0 ? 0.0 : (100.0 * (double)removed_cnt / (double)removes);
    }
}

