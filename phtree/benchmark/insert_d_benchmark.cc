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
#include "phtree/phtree_d.h"
#include <benchmark/benchmark.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/ansicolor_sink.h>

using namespace improbable;
using namespace improbable::phtree;
using namespace improbable::phtree::phbenchmark;

namespace {

const double GLOBAL_MAX = 10000;

/*
 * Benchmark for adding entries to the index.
 */
template <dimension_t DIM>
class IndexBenchmark {
  public:
    IndexBenchmark(benchmark::State& state, TestGenerator data_type, int num_entities);

    void Benchmark(benchmark::State& state);

  private:
    void SetupWorld(benchmark::State& state);

    void Insert(benchmark::State& state, PhTreeD<DIM, int>& tree);

    const TestGenerator data_type_;
    const int num_entities_;
    std::vector<PhPointD<DIM>> points_;
};

template <dimension_t DIM>
IndexBenchmark<DIM>::IndexBenchmark(
    benchmark::State& state, TestGenerator data_type, int num_entities)
: data_type_{data_type}, num_entities_(num_entities), points_(num_entities) {
    auto console_sink = std::make_shared<spdlog::sinks::ansicolor_stdout_sink_mt>();
    spdlog::set_default_logger(
        std::make_shared<spdlog::logger>("", spdlog::sinks_init_list({console_sink})));
    spdlog::set_level(spdlog::level::warn);

    SetupWorld(state);
}

template <dimension_t DIM>
void IndexBenchmark<DIM>::Benchmark(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        auto* tree = new PhTreeD<DIM, int>();
        state.ResumeTiming();

        Insert(state, *tree);

        // we do this top avoid measuring deallocation
        state.PauseTiming();
        delete tree;
        state.ResumeTiming();
    }
}

template <dimension_t DIM>
void IndexBenchmark<DIM>::SetupWorld(benchmark::State& state) {
    spdlog::info("Setting up world with {} entities and {} dimensions.", num_entities_, DIM);
    CreatePointData<DIM>(points_, data_type_, num_entities_, 0, GLOBAL_MAX);

    state.counters["total_put_count"] = benchmark::Counter(0);
    state.counters["put_rate"] = benchmark::Counter(0, benchmark::Counter::kIsRate);

    spdlog::info("World setup complete.");
}

template <dimension_t DIM>
void IndexBenchmark<DIM>::Insert(benchmark::State& state, PhTreeD<DIM, int>& tree) {
    for (int i = 0; i < num_entities_; ++i) {
        PhPointD<DIM>& p = points_[i];
        tree.emplace(p, i);
    }

    state.counters["total_put_count"] += num_entities_;
    state.counters["put_rate"] += num_entities_;
}

}  // namespace

template <typename... Arguments>
void PhTree3D(benchmark::State& state, Arguments&&... arguments) {
    IndexBenchmark<3> benchmark{state, arguments...};
    benchmark.Benchmark(state);
}

template <typename... Arguments>
void PhTree6D(benchmark::State& state, Arguments&&... arguments) {
    IndexBenchmark<6> benchmark{state, arguments...};
    benchmark.Benchmark(state);
}

template <typename... Arguments>
void PhTree10D(benchmark::State& state, Arguments&&... arguments) {
    IndexBenchmark<10> benchmark{state, arguments...};
    benchmark.Benchmark(state);
}

template <typename... Arguments>
void PhTree20D(benchmark::State& state, Arguments&&... arguments) {
    IndexBenchmark<20> benchmark{state, arguments...};
    benchmark.Benchmark(state);
}

// index type, scenario name, data_generator, num_entities
// PhTree 3D CUBE
BENCHMARK_CAPTURE(PhTree3D, INS_CU_1K, TestGenerator::CUBE, 1000)->Unit(benchmark::kMillisecond);

BENCHMARK_CAPTURE(PhTree3D, INS_CU_10K, TestGenerator::CUBE, 10000)->Unit(benchmark::kMillisecond);

BENCHMARK_CAPTURE(PhTree3D, INS_CU_100K, TestGenerator::CUBE, 100000)
    ->Unit(benchmark::kMillisecond);

BENCHMARK_CAPTURE(PhTree3D, INS_CU_1M, TestGenerator::CUBE, 1000000)->Unit(benchmark::kMillisecond);

BENCHMARK_CAPTURE(PhTree3D, INS_CU_10M, TestGenerator::CUBE, 10000000)
    ->Unit(benchmark::kMillisecond);

BENCHMARK_CAPTURE(PhTree3D, INS_CL_1K, TestGenerator::CLUSTER, 1000)->Unit(benchmark::kMillisecond);

BENCHMARK_CAPTURE(PhTree3D, INS_CL_10K, TestGenerator::CLUSTER, 10000)
    ->Unit(benchmark::kMillisecond);

BENCHMARK_CAPTURE(PhTree3D, INS_CL_100K, TestGenerator::CLUSTER, 100000)
    ->Unit(benchmark::kMillisecond);

BENCHMARK_CAPTURE(PhTree3D, INS_CL_1M, TestGenerator::CLUSTER, 1000000)
    ->Unit(benchmark::kMillisecond);

BENCHMARK_CAPTURE(PhTree3D, INS_CL_10M, TestGenerator::CLUSTER, 10000000)
    ->Unit(benchmark::kMillisecond);

BENCHMARK_CAPTURE(PhTree6D, INS_CL_100K, TestGenerator::CLUSTER, 100000)
    ->Unit(benchmark::kMillisecond);

BENCHMARK_CAPTURE(PhTree6D, INS_CL_1M, TestGenerator::CLUSTER, 1000000)
    ->Unit(benchmark::kMillisecond);

BENCHMARK_CAPTURE(PhTree10D, INS_CL_100K, TestGenerator::CLUSTER, 100000)
    ->Unit(benchmark::kMillisecond);

BENCHMARK_CAPTURE(PhTree10D, INS_CL_1M, TestGenerator::CLUSTER, 1000000)
    ->Unit(benchmark::kMillisecond);

BENCHMARK_CAPTURE(PhTree20D, INS_CL_100K, TestGenerator::CLUSTER, 100000)
    ->Unit(benchmark::kMillisecond);

BENCHMARK_CAPTURE(PhTree20D, INS_CL_1M, TestGenerator::CLUSTER, 1000000)
    ->Unit(benchmark::kMillisecond);

BENCHMARK_MAIN();
