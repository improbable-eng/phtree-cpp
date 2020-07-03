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
#include "phtree/phtree_box_d.h"
#include <benchmark/benchmark.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/ansicolor_sink.h>
#include <random>

using namespace improbable;
using namespace improbable::phtree;
using namespace improbable::phtree::phbenchmark;

namespace {

const double GLOBAL_MAX = 10000;
const double BOX_LEN = 10;

template <dimension_t DIM>
struct UpdateOp {
    scalar_t id_;
    PhBoxD<DIM> old_;
    PhBoxD<DIM> new_;
};

/*
 * Benchmark for updating the position of entries.
 */
template <dimension_t DIM>
class IndexBenchmark {
  public:
    IndexBenchmark(
        benchmark::State& state,
        TestGenerator data_type,
        int num_entities,
        int updates_per_round,
        double move_distance);

    void Benchmark(benchmark::State& state);

  private:
    void SetupWorld(benchmark::State& state);
    void BuildUpdate(std::vector<UpdateOp<DIM>>& updates);
    void UpdateWorld(benchmark::State& state, std::vector<UpdateOp<DIM>>& updates);

    const TestGenerator data_type_;
    const int num_entities_;
    const int updates_per_round_;
    const double move_distance_;

    PhTreeBoxD<DIM, scalar_t> tree_;
    std::default_random_engine random_engine_;
    std::uniform_real_distribution<> cube_distribution_;
    std::vector<PhBoxD<DIM>> boxes_;
};

template <dimension_t DIM>
IndexBenchmark<DIM>::IndexBenchmark(
    benchmark::State& state,
    TestGenerator data_type,
    int num_entities,
    int updates_per_round,
    double move_distance)
: data_type_{data_type}
, num_entities_(num_entities)
, updates_per_round_(updates_per_round)
, move_distance_(move_distance)
, random_engine_{1}
, cube_distribution_{0, GLOBAL_MAX}
, boxes_(num_entities) {
    auto console_sink = std::make_shared<spdlog::sinks::ansicolor_stdout_sink_mt>();
    spdlog::set_default_logger(
        std::make_shared<spdlog::logger>("", spdlog::sinks_init_list({console_sink})));
    spdlog::set_level(spdlog::level::warn);

    SetupWorld(state);
}

template <dimension_t DIM>
void IndexBenchmark<DIM>::Benchmark(benchmark::State& state) {
    std::vector<UpdateOp<DIM>> updates;
    updates.reserve(updates_per_round_);
    for (auto _ : state) {
        state.PauseTiming();
        BuildUpdate(updates);
        state.ResumeTiming();

        UpdateWorld(state, updates);

        state.PauseTiming();
        for (auto& update : updates) {
            boxes_[update.id_] = update.new_;
        }
        state.ResumeTiming();
    }
}

template <dimension_t DIM>
void IndexBenchmark<DIM>::BuildUpdate(std::vector<UpdateOp<DIM>>& updates) {
    // Use Delta to avoid moving points in insertion order (not that it matters for the PH-Tree, but
    // we may test other trees as well.
    int box_id_increment = num_entities_ / updates_per_round_;  // int division
    int box_id = 0;
    updates.clear();
    for (size_t i = 0; i < updates_per_round_; ++i) {
        assert(box_id >= 0);
        assert(box_id < boxes_.size());
        auto& old_box = boxes_[box_id];
        auto update = UpdateOp<DIM>{box_id, old_box, old_box};
        for (dimension_t d = 0; d < DIM; ++d) {
            update.new_.min()[d] += move_distance_;
            update.new_.max()[d] += move_distance_;
        }
        updates.emplace_back(update);
        box_id += box_id_increment;
    }
}

template <dimension_t DIM>
void IndexBenchmark<DIM>::SetupWorld(benchmark::State& state) {
    spdlog::info("Setting up world with {} entities and {} dimensions.", num_entities_, DIM);
    CreateBoxData<DIM>(boxes_, data_type_, num_entities_, 0, GLOBAL_MAX, BOX_LEN);
    for (int i = 0; i < num_entities_; ++i) {
        tree_.emplace(boxes_[i], i);
    }

    state.counters["total_upd_count"] = benchmark::Counter(0);
    state.counters["update_rate"] = benchmark::Counter(0, benchmark::Counter::kIsRate);
    spdlog::info("World setup complete.");
}

template <dimension_t DIM>
void IndexBenchmark<DIM>::UpdateWorld(
    benchmark::State& state, std::vector<UpdateOp<DIM>>& updates) {
    size_t initial_tree_size = tree_.size();
    for (auto& update : updates) {
        size_t result_erase = tree_.erase(update.old_);
        auto result_emplace = tree_.emplace(update.new_, update.id_);
        assert(result_erase == 1);
        assert(result_emplace.second);
    }

    // For normal indexes we expect num_entities==size(), but the PhTree<Map<...>> index has
    // size() as low as (num_entities-duplicates).
    if (tree_.size() > num_entities_ || tree_.size() < initial_tree_size - updates_per_round_) {
        spdlog::error("Invalid index size after update: {}/{}", tree_.size(), num_entities_);
    }

    state.counters["total_upd_count"] += updates_per_round_;
    state.counters["update_rate"] += updates_per_round_;
}

}  // namespace

template <typename... Arguments>
void PhTree3D(benchmark::State& state, Arguments&&... arguments) {
    IndexBenchmark<4> benchmark{state, arguments...};
    benchmark.Benchmark(state);
}

// index type, scenario name, data_type, num_entities, updates_per_round, move_distance
// PhTree3D CUBE
BENCHMARK_CAPTURE(PhTree3D, UPDATE_CU_100_of_1K, TestGenerator::CUBE, 1000, 100, 10.)
    ->Unit(benchmark::kMillisecond);

BENCHMARK_CAPTURE(PhTree3D, UPDATE_CU_100_of_10K, TestGenerator::CUBE, 10000, 100, 10.)
    ->Unit(benchmark::kMillisecond);

BENCHMARK_CAPTURE(PhTree3D, UPDATE_CU_100_of_100K, TestGenerator::CUBE, 100000, 100, 10.)
    ->Unit(benchmark::kMillisecond);

BENCHMARK_CAPTURE(PhTree3D, UPDATE_CU_100_of_1M, TestGenerator::CUBE, 1000000, 100, 10.)
    ->Unit(benchmark::kMillisecond);

// PhTree3D CLUSTER
BENCHMARK_CAPTURE(PhTree3D, UPDATE_CL_100_of_1K, TestGenerator::CLUSTER, 1000, 100, 10.)
    ->Unit(benchmark::kMillisecond);

BENCHMARK_CAPTURE(PhTree3D, UPDATE_CL_100_of_10K, TestGenerator::CLUSTER, 10000, 100, 10.)
    ->Unit(benchmark::kMillisecond);

BENCHMARK_CAPTURE(PhTree3D, UPDATE_CL_100_of_100K, TestGenerator::CLUSTER, 100000, 100, 10.)
    ->Unit(benchmark::kMillisecond);

BENCHMARK_CAPTURE(PhTree3D, UPDATE_CL_100_of_1M, TestGenerator::CLUSTER, 1000000, 100, 10.)
    ->Unit(benchmark::kMillisecond);

BENCHMARK_MAIN();
