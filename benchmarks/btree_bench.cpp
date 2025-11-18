#include "tree_bench_common.h"
#include "../src/BTreeFunctional.h"

std::vector<std::string> read_words_file(const std::string &filename);

template <int DEGREE>
static void WordCount(benchmark::State &state) {
    using BT = BTreeFunctional<char, uint64_t, DEGREE>;
    using BTRef = BTreeRef<char, uint64_t, DEGREE>;

    auto N = (Size) state.range(0);
    std::string dbfile = "wordcount_" + std::to_string(N) + ".db";

    auto words_all = load_words();
    std::vector<std::string> words;
    build_dataset(words, words_all, N);

    run_wordcount_iteration<BT, BTRef>(state, N, words, dbfile);
}

template <int DEGREE>
static void RandomLookup(benchmark::State &state) {
    using BT = BTreeFunctional<char, uint64_t, DEGREE>;
    using BTRef = BTreeRef<char, uint64_t, DEGREE>;

    auto N = (Size) state.range(0);
    std::string dbfile = "random_lookup_" + std::to_string(N) + ".db";

    auto words_all = load_words();
    std::vector<std::string> words;
    build_dataset(words, words_all, N);

    std::vector<std::string> queries;
    build_random_queries(words, N, queries, 10);

    run_random_lookup_iteration<BT, BTRef>(state, N, words, queries, dbfile);
}

template <int DEGREE>
static void Insert(benchmark::State &state) {
    using BT = BTreeFunctional<char, uint64_t, DEGREE>;
    using BTRef = BTreeRef<char, uint64_t, DEGREE>;

    auto N = (Size) state.range(0);
    std::string dbfile = "insert_" + std::to_string(N) + ".db";
    auto words_all = load_words();
    run_insert_iteration<BT, BTRef>(state, N, words_all, dbfile);
}

template <int DEGREE>
static void Remove(benchmark::State &state) {
    using BT = BTreeFunctional<char, uint64_t, DEGREE>;
    using BTRef = BTreeRef<char, uint64_t, DEGREE>;

    auto N = (Size) state.range(0);
    std::string dbfile = "remove_" + std::to_string(N) + ".db";
    auto words_all = load_words();
    run_remove_iteration<BT, BTRef>(state, N, words_all, dbfile);
}

template <int DEGREE>
static void Mixed(benchmark::State &state) {
    using BT = BTreeFunctional<char, uint64_t, DEGREE>;
    using BTRef = BTreeRef<char, uint64_t, DEGREE>;

    auto N = (Size) state.range(0);
    std::string dbfile = "mixed_" + std::to_string(N) + ".db";

    auto words_all = load_words();
    run_mixed_iteration<BT, BTRef>(state, N, words_all, dbfile);
}

// static constexpr int DEGREE = find_t<char, int>(192);
static constexpr int DEGREE = 5;

BENCHMARK_TEMPLATE(WordCount,    DEGREE)->Arg(100'000)->Arg(1'000'000)->Arg(5'000'000)->Repetitions(10)->Unit(benchmark::kMillisecond);
BENCHMARK_TEMPLATE(RandomLookup, DEGREE)->Arg(100'000)->Arg(1'000'000)->Arg(5'000'000)->Repetitions(10)->Unit(benchmark::kMillisecond);
BENCHMARK_TEMPLATE(Insert,       DEGREE)->Arg(100'000)->Arg(1'000'000)->Arg(5'000'000)->Repetitions(10)->Unit(benchmark::kMillisecond);
BENCHMARK_TEMPLATE(Remove,       DEGREE)->Arg(100'000)->Arg(1'000'000)->Arg(5'000'000)->Repetitions(10)->Unit(benchmark::kMillisecond);
BENCHMARK_TEMPLATE(Mixed,        DEGREE)->Arg(100'000)->Arg(1'000'000)->Arg(5'000'000)->Repetitions(10)->Unit(benchmark::kMillisecond);
