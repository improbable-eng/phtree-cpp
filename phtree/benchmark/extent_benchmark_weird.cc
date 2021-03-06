/*
 * Copyright 2020 Improbable Worlds Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "phtree/benchmark/benchmark_util.h"
#include "phtree/phtree.h"
#include <benchmark/benchmark.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/ansicolor_sink.h>
#include <random>

using namespace improbable;
using namespace improbable::phtree;
using namespace improbable::phtree::phbenchmark;

namespace {

const int GLOBAL_MAX = 10000;

/*
 * Benchmark for iterating over all entries while using a filter.
 *
 * TODO
 * This benchmarks shows some weird behaviour, see below.
 * It should probably be removed at some point.
 */

template <dimension_t DIM>
class IndexBenchmark {
  public:
    IndexBenchmark(
        benchmark::State& state, TestGenerator data_type, int num_entities, int type = 0);

    void Benchmark(benchmark::State& state);

  private:
    void SetupWorld(benchmark::State& state);
    void QueryWorld(benchmark::State& state);

    const TestGenerator data_type_;
    const int num_entities_;

    PhTree<DIM, int> tree_;
    std::default_random_engine random_engine_;
    std::uniform_int_distribution<> cube_distribution_;
    std::vector<PhPoint<DIM>> points_;
    int type_;
};

template <dimension_t DIM>
IndexBenchmark<DIM>::IndexBenchmark(
    benchmark::State& state, TestGenerator data_type, int num_entities, int type)
: data_type_{data_type}
, num_entities_(num_entities)
, random_engine_{1}
, cube_distribution_{0, GLOBAL_MAX}
, points_(num_entities)
, type_{type} {
    auto console_sink = std::make_shared<spdlog::sinks::ansicolor_stdout_sink_mt>();
    spdlog::set_default_logger(
        std::make_shared<spdlog::logger>("", spdlog::sinks_init_list({console_sink})));
    spdlog::set_level(spdlog::level::warn);

    SetupWorld(state);
}

template <dimension_t DIM>
void IndexBenchmark<DIM>::Benchmark(benchmark::State& state) {
    for (auto _ : state) {
        QueryWorld(state);
    }
}

template <dimension_t DIM>
void IndexBenchmark<DIM>::SetupWorld(benchmark::State& state) {
    spdlog::info("Setting up world with {} entities and {} dimensions.", num_entities_, DIM);
    CreatePointData<DIM>(points_, data_type_, num_entities_, 0, GLOBAL_MAX);
    for (int i = 0; i < num_entities_; ++i) {
        tree_.emplace(points_[i], i);
    }

    state.counters["total_result_count"] = benchmark::Counter(0);
    state.counters["query_rate"] = benchmark::Counter(0, benchmark::Counter::kIsRate);
    state.counters["result_rate"] = benchmark::Counter(0, benchmark::Counter::kIsRate);
    state.counters["avg_result_count"] = benchmark::Counter(0, benchmark::Counter::kAvgIterations);

    spdlog::info("World setup complete.");
}

template <
    dimension_t DIM,
    typename KEY = PhPoint<DIM>,
    PhPreprocessor<DIM, KEY> PRE = PrePostNoOp<DIM>>
class PhFilterBoxIntersection {
  public:
    PhFilterBoxIntersection(const PhPoint<DIM>& minInclude, const PhPoint<DIM>& maxInclude)
    : minIncludeBits{minInclude}, maxIncludeBits{maxInclude} {};

    void set(const PhPointD<DIM>& minExclude, const PhPointD<DIM>& maxExclude) {
        minIncludeBits = PRE(minExclude);
        maxIncludeBits = PRE(maxExclude);
    }

    [[nodiscard]] bool IsEntryValid(const PhPoint<DIM>& key, const int& value) const {
        for (int i = 0; i < DIM; ++i) {
            if (key[i] < minIncludeBits[i] || key[i] > maxIncludeBits[i]) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] bool IsNodeValid(const PhPoint<DIM>& prefix, int bits_to_ignore) const {
        // skip this for root node (bitsToIgnore == 64)
        if (bits_to_ignore >= (MAX_BIT_WIDTH - 1)) {
            return true;
        }
        bit_mask_t maskMin = MAX_MASK << bits_to_ignore;
        bit_mask_t maskMax = ~maskMin;

        for (size_t i = 0; i < prefix.size(); ++i) {
            scalar_t minBits = prefix[i] & maskMin;
            scalar_t maxBits = prefix[i] | maskMax;
            if (maxBits < minIncludeBits[i] || minBits > maxIncludeBits[i]) {
                return false;
            }
        }
        return true;
    }

  private:
    const PhPoint<DIM> minIncludeBits;
    const PhPoint<DIM> maxIncludeBits;
};

template <
    dimension_t DIM,
    typename KEY = PhPoint<DIM>,
    PhPreprocessor<DIM, KEY> PRE = PrePostNoOp<DIM>>
class PhFilterTrue {
  public:
    PhFilterTrue(const PhPoint<DIM>& minInclude, const PhPoint<DIM>& maxInclude)
    : minIncludeBits{minInclude}, maxIncludeBits{maxInclude} {};

    void set(const PhPointD<DIM>& minExclude, const PhPointD<DIM>& maxExclude) {
        minIncludeBits = PRE(minExclude);
        maxIncludeBits = PRE(maxExclude);
    }

    [[nodiscard]] bool IsEntryValid(const PhPoint<DIM>& key, const int& value) const {
        return true;
    }

    [[nodiscard]] bool IsNodeValid(const PhPoint<DIM>& prefix, int bits_to_ignore) const {
        return true;
    }

  private:
    const PhPoint<DIM> minIncludeBits;
    const PhPoint<DIM> maxIncludeBits;
};

template <
    dimension_t DIM,
    typename KEY = PhPoint<DIM>,
    PhPreprocessor<DIM, KEY> PRE = PrePostNoOp<DIM>>
class PhFilterTrue2 {
  public:
    PhFilterTrue2() : minIncludeBits{}, maxIncludeBits{} {};

    [[nodiscard]] bool IsEntryValid(const PhPoint<DIM>& key, const int& value) const {
        return true;
    }

    [[nodiscard]] bool IsNodeValid(const PhPoint<DIM>& prefix, int bits_to_ignore) const {
        return true;
    }

  private:
    const PhPoint<DIM> minIncludeBits;
    const PhPoint<DIM> maxIncludeBits;
};

template <dimension_t DIM, typename T>
struct PhFilterTrue3 {
    [[nodiscard]] constexpr bool IsEntryValid(const PhPoint<DIM>& key, const T& value) const {
        return true;
    }

    [[nodiscard]] constexpr bool IsNodeValid(const PhPoint<DIM>& prefix, int bits_to_ignore) const {
        return true;
    }
};

template <dimension_t DIM>
void IndexBenchmark<DIM>::QueryWorld(benchmark::State& state) {
    int n = 0;
    // TODO This is all really weird.
    //   reenabling one of the following filters has the follwing effects:
    //   1) Some of the filters in the first branch will affect performance of the
    //      second branch (?!?!?!)
    //   2) Performance is often different from the second branch if the the filters are
    //      logically the same.
    //   Differences are usually between 5% and 15%, but confidence is pretty high
    //   if the tests at thye end of the file are enabled (notice the somewhat irregular
    //   pattern of the tests that will find itself clearly in the results:
    //   Order: 0 1 0 0 1 1 0 1
    //
    //  Some observations:
    //  - Whichever test if in the 'if' part is hardly slowed down, but the 'else'
    //    part is clearly slowed down.
    //  - Compiling with  -falign-functions=32 or -falign-functions=64 did not help
    if (type_ == 0) {
        // PhPoint<DIM> min{-GLOBAL_MAX, -GLOBAL_MAX, -GLOBAL_MAX};
        // PhPoint<DIM> max{GLOBAL_MAX, GLOBAL_MAX, GLOBAL_MAX};
        // PhFilterAABB<DIM, int> filter(min, max);
        // PhFilterBoxIntersection<DIM> filter(min, max);
        // PhFilterNoOp<DIM, int> filter;
        // PhFilterTrue<DIM> filter(min, max);
        // PhFilterTrue2<DIM> filter;
        PhFilterTrue3<DIM, int> filter;
        auto q = tree_.begin(filter);

        //       auto q = tree_.begin();
        while (q != tree_.end()) {
            // just read the entry
            ++q;
            ++n;
        }
    } else {
        auto q = tree_.begin();
        while (q != tree_.end()) {
            // just read the entry
            ++q;
            ++n;
        }
    }

    state.counters["total_result_count"] += n;
    state.counters["query_rate"] += 1;
    state.counters["result_rate"] += n;
    state.counters["avg_result_count"] += n;
}

}  // namespace

template <typename... Arguments>
void PhTree3D(benchmark::State& state, Arguments&&... arguments) {
    IndexBenchmark<3> benchmark{state, arguments...};
    benchmark.Benchmark(state);
}

// index type, scenario name, data_generator, num_entities, filter selector
// PhTree 3D CUBE
BENCHMARK_CAPTURE(PhTree3D, EXT_CU_1K, TestGenerator::CUBE, 1000, 0)->Unit(benchmark::kMillisecond);

BENCHMARK_CAPTURE(PhTree3D, EXT_CU_1K, TestGenerator::CUBE, 1000, 1)->Unit(benchmark::kMillisecond);

BENCHMARK_CAPTURE(PhTree3D, EXT_CU_1K, TestGenerator::CUBE, 1000, 0)->Unit(benchmark::kMillisecond);

BENCHMARK_CAPTURE(PhTree3D, EXT_CU_1K, TestGenerator::CUBE, 1000, 0)->Unit(benchmark::kMillisecond);

BENCHMARK_CAPTURE(PhTree3D, EXT_CU_1K, TestGenerator::CUBE, 1000, 1)->Unit(benchmark::kMillisecond);

BENCHMARK_CAPTURE(PhTree3D, EXT_CU_1K, TestGenerator::CUBE, 1000, 1)->Unit(benchmark::kMillisecond);

BENCHMARK_CAPTURE(PhTree3D, EXT_CU_1K, TestGenerator::CUBE, 1000, 0)->Unit(benchmark::kMillisecond);

BENCHMARK_CAPTURE(PhTree3D, EXT_CU_1K, TestGenerator::CUBE, 1000, 1)->Unit(benchmark::kMillisecond);

BENCHMARK_MAIN();
