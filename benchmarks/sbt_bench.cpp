#include "tree_bench_common.h"
#include "../src/SBTFunctional.h"

std::vector<std::string> read_words_file(const std::string &filename);

static void WordCount(benchmark::State &state) {
    using SBT = SBTFunctional<char, uint64_t>;
    using SBTRef = SBTRef<char, uint64_t>;

    auto N = (Size) state.range(0);
    std::string dbfile = "wordcount_" + std::to_string(N) + ".db";

    auto words_all = load_words();
    std::vector<std::string> words;
    build_dataset(words, words_all, N);

    run_wordcount_iteration<SBT, SBTRef>(state, N, words, dbfile);
}

static void RandomLookup(benchmark::State &state) {
    using SBT = SBTFunctional<char, uint64_t>;
    using SBTRef = SBTRef<char, uint64_t>;

    auto N = (Size) state.range(0);
    std::string dbfile = "random_lookup_" + std::to_string(N) + ".db";

    auto words_all = load_words();
    std::vector<std::string> words;
    build_dataset(words, words_all, N);

    std::vector<std::string> queries;
    build_random_queries(words, N, queries, 10);

    run_random_lookup_iteration<SBT, SBTRef>(state, N, words, queries, dbfile);
}

static void SequentialLookup(benchmark::State &state) {
    using SBT = SBTFunctional<char, uint64_t>;
    using SBTRef = SBTRef<char, uint64_t>;

    auto N = (Size) state.range(0);
    std::string dbfile = "sequential_lookup_" + std::to_string(N) + ".db";

    auto words_all = load_words();
    std::vector<std::string> words;
    build_dataset(words, words_all, N);

    run_sequential_lookup_iteration<SBT, SBTRef>(state, N, words, dbfile);
}

static void Insert(benchmark::State &state) {
    using SBT = SBTFunctional<char, uint64_t>;
    using SBTRef = SBTRef<char, uint64_t>;

    auto N = (Size) state.range(0);
    std::string dbfile = "insert_" + std::to_string(N) + ".db";
    auto words_all = load_words();
    run_insert_iteration<SBT, SBTRef>(state, N, words_all, dbfile);
}

static void Remove(benchmark::State &state) {
    using SBT = SBTFunctional<char, uint64_t>;
    using SBTRef = SBTRef<char, uint64_t>;

    auto N = (Size) state.range(0);
    std::string dbfile = "remove_" + std::to_string(N) + ".db";
    auto words_all = load_words();
    run_remove_iteration<SBT, SBTRef>(state, N, words_all, dbfile);
}

static void Mixed(benchmark::State &state) {
    using SBT = SBTFunctional<char, uint64_t>;
    using SBTRef = SBTRef<char, uint64_t>;

    auto N = (Size) state.range(0);
    std::string dbfile = "mixed_" + std::to_string(N) + ".db";

    auto words_all = load_words();
    run_mixed_iteration<SBT, SBTRef>(state, N, words_all, dbfile);
}

static int setup_context = [] {
    benchmark::AddCustomContext("tree_type", "SBT");
    benchmark::AddCustomContext("logical_page_size", std::to_string(FFDB_LOGICAL_PAGE_SIZE));
    benchmark::AddCustomContext("physical_page_size", std::to_string(FFDB_PHYSICAL_PAGE_SIZE));
    benchmark::AddCustomContext("alignment_bits", std::to_string(FFDB_ALIGN));
    benchmark::AddCustomContext("key_prefix_size", std::to_string(FFDB_KEY_PREFIX_SIZE));
    return 0;
}();

BENCHMARK(WordCount)       ->Arg(100'000)->Arg(1'000'000)->Arg(5'000'000)->Repetitions(10)->MinWarmUpTime(0.5)->Unit(benchmark::kMillisecond);
BENCHMARK(RandomLookup)    ->Arg(100'000)->Arg(1'000'000)->Arg(5'000'000)->Repetitions(10)->MinWarmUpTime(0.5)->Unit(benchmark::kMillisecond);
BENCHMARK(SequentialLookup)->Arg(100'000)->Arg(1'000'000)->Arg(5'000'000)->Repetitions(10)->MinWarmUpTime(0.5)->Unit(benchmark::kMillisecond);
BENCHMARK(Insert)          ->Arg(100'000)->Arg(1'000'000)->Arg(5'000'000)->Repetitions(10)->MinWarmUpTime(0.5)->Unit(benchmark::kMillisecond);
BENCHMARK(Remove)          ->Arg(100'000)->Arg(1'000'000)->Arg(5'000'000)->Repetitions(10)->MinWarmUpTime(0.5)->Unit(benchmark::kMillisecond);
BENCHMARK(Mixed)           ->Arg(100'000)->Arg(1'000'000)->Arg(5'000'000)->Repetitions(10)->MinWarmUpTime(0.5)->Unit(benchmark::kMillisecond);
