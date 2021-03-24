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
#include "logging.h"
#include "phtree/benchmark/benchmark_util.h"
#include "phtree/phtree.h"
#include <benchmark/benchmark.h>

using namespace improbable;
using namespace improbable::phtree;
using namespace improbable::phtree::phbenchmark;

namespace {

constexpr size_t UPDATES_PER_ROUND = 1000;
std::vector<double> MOVE_DISTANCE = {0, 1.0, 10};

const double GLOBAL_MAX = 10000;

enum UpdateType { ERASE_BY_KEY, ERASE_BY_ITER, EMPLACE_HINT };

template <dimension_t DIM>
using PointType = PhPointD<DIM>;

template <dimension_t DIM>
using TreeType = PhTreeD<DIM, size_t>;

template <dimension_t DIM>
struct UpdateOp {
    size_t id_;
    PointType<DIM> old_;
    PointType<DIM> new_;
};

/*
 * Benchmark for updating the position of entries.
 */
template <dimension_t DIM, UpdateType UPDATE_TYPE>
class IndexBenchmark {
  public:
    IndexBenchmark(
        benchmark::State& state,
        TestGenerator data_type,
        int num_entities,
        int updates_per_round = UPDATES_PER_ROUND,
        std::vector<double> move_distance = MOVE_DISTANCE);

    void Benchmark(benchmark::State& state);

  private:
    void SetupWorld(benchmark::State& state);
    void BuildUpdates();
    void UpdateWorld(benchmark::State& state);

    const TestGenerator data_type_;
    const size_t num_entities_;
    const size_t updates_per_round_;
    const std::vector<double> move_distance_;

    TreeType<DIM> tree_;
    std::vector<PointType<DIM>> points_;
    std::vector<UpdateOp<DIM>> updates_;
    std::default_random_engine random_engine_;
    std::uniform_int_distribution<> entity_id_distribution_;
};

template <dimension_t DIM, UpdateType UPDATE_TYPE>
IndexBenchmark<DIM, UPDATE_TYPE>::IndexBenchmark(
    benchmark::State& state,
    TestGenerator data_type,
    int num_entities,
    int updates_per_round,
    std::vector<double> move_distance)
: data_type_{data_type}
, num_entities_(num_entities)
, updates_per_round_(updates_per_round)
, move_distance_(std::move(move_distance))
, points_(num_entities)
, updates_(updates_per_round)
, random_engine_{0}
, entity_id_distribution_{0, num_entities - 1} {
    logging::SetupDefaultLogging();
    SetupWorld(state);
}

template <dimension_t DIM, UpdateType UPDATE_TYPE>
void IndexBenchmark<DIM, UPDATE_TYPE>::Benchmark(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        BuildUpdates();
        state.ResumeTiming();

        UpdateWorld(state);
    }
}

template <dimension_t DIM, UpdateType UPDATE_TYPE>
void IndexBenchmark<DIM, UPDATE_TYPE>::SetupWorld(benchmark::State& state) {
    logging::info("Setting up world with {} entities and {} dimensions.", num_entities_, DIM);
    CreatePointData<DIM>(points_, data_type_, num_entities_, 0, GLOBAL_MAX);
    for (size_t i = 0; i < num_entities_; ++i) {
        tree_.emplace(points_[i], i);
    }

    state.counters["total_upd_count"] = benchmark::Counter(0);
    state.counters["update_rate"] = benchmark::Counter(0, benchmark::Counter::kIsRate);
    logging::info("World setup complete.");
}

template <dimension_t DIM, UpdateType UPDATE_TYPE>
void IndexBenchmark<DIM, UPDATE_TYPE>::BuildUpdates() {
    size_t move_id = 0;
    for (auto& update : updates_) {
        int point_id = entity_id_distribution_(random_engine_);
        update.id_ = point_id;
        update.old_ = points_[point_id];
        for (dimension_t d = 0; d < DIM; ++d) {
            update.new_[d] = update.old_[d] + move_distance_[move_id];
        }
        // update reference data
        points_[point_id] = update.new_;

        move_id = (move_id + 1) % move_distance_.size();
    }
}

template <dimension_t DIM>
size_t UpdateByKey(TreeType<DIM>& tree, std::vector<UpdateOp<DIM>>& updates) {
    size_t n = 0;
    for (auto& update : updates) {
        // naive erase + emplace
        size_t result_erase = tree.erase(update.old_);
        auto result_emplace = tree.emplace(update.new_, update.id_);
        n += result_erase == 1 && result_emplace.second;
    }
    return n;
}

template <dimension_t DIM>
size_t UpdateByIter(TreeType<DIM>& tree, std::vector<UpdateOp<DIM>>& updates) {
    size_t n = 0;
    for (auto& update : updates) {
        // find + erase + emplace
        // This is not immediately useful, but demonstrates that find + erase is as fast
        // as erase(key).
        auto iter = tree.find(update.old_);
        size_t result_erase = tree.erase(iter);
        auto result_emplace = tree.emplace(update.new_, update.id_);
        n += result_erase == 1 && result_emplace.second;
    }
    return n;
}

template <dimension_t DIM>
size_t UpdateByIterHint(TreeType<DIM>& tree, std::vector<UpdateOp<DIM>>& updates) {
    size_t n = 0;
    for (auto& update : updates) {
        // find + erase + emplace_hint
        auto iter = tree.find(update.old_);
        size_t result_erase = tree.erase(iter);
        auto result_emplace = tree.emplace_hint(iter, update.new_, update.id_);
        n += result_erase == 1 && result_emplace.second;
    }
    return n;
}

template <dimension_t DIM, UpdateType UPDATE_TYPE>
void IndexBenchmark<DIM, UPDATE_TYPE>::UpdateWorld(benchmark::State& state) {
    size_t initial_tree_size = tree_.size();
    size_t n = 0;
    switch (UPDATE_TYPE) {
    case UpdateType::ERASE_BY_KEY:
        n = UpdateByKey(tree_, updates_);
        break;
    case UpdateType::ERASE_BY_ITER:
        n = UpdateByIter(tree_, updates_);
        break;
    case UpdateType::EMPLACE_HINT:
        n = UpdateByIterHint(tree_, updates_);
        break;
    }

    if (n != updates_.size()) {
        logging::error("Invalid update count: {}/{}", updates_.size(), n);
    }

    // For normal indexes we expect num_entities==size(), but the PhTree<Map<...>> index has
    // size() as low as (num_entities-duplicates).
    if (tree_.size() > num_entities_ || tree_.size() + updates_per_round_ < initial_tree_size) {
        logging::error("Invalid index size after update: {}/{}", tree_.size(), num_entities_);
    }

    state.counters["total_upd_count"] += updates_per_round_;
    state.counters["update_rate"] += updates_per_round_;
}

}  // namespace

template <typename... Arguments>
void PhTreeEraseKey3D(benchmark::State& state, Arguments&&... arguments) {
    IndexBenchmark<3, UpdateType::ERASE_BY_KEY> benchmark{state, arguments...};
    benchmark.Benchmark(state);
}

template <typename... Arguments>
void PhTreeEraseIter3D(benchmark::State& state, Arguments&&... arguments) {
    IndexBenchmark<3, UpdateType::ERASE_BY_ITER> benchmark{state, arguments...};
    benchmark.Benchmark(state);
}

template <typename... Arguments>
void PhTreeEmplaceHint3D(benchmark::State& state, Arguments&&... arguments) {
    IndexBenchmark<3, UpdateType::EMPLACE_HINT> benchmark{state, arguments...};
    benchmark.Benchmark(state);
}

// index type, scenario name, data_type, num_entities, updates_per_round, move_distance
// PhTree3D CUBE
BENCHMARK_CAPTURE(PhTreeEraseKey3D, UPDATE_CU_100_of_1K, TestGenerator::CUBE, 1000)
    ->Unit(benchmark::kMillisecond);

BENCHMARK_CAPTURE(PhTreeEraseKey3D, UPDATE_CU_100_of_10K, TestGenerator::CUBE, 10000)
    ->Unit(benchmark::kMillisecond);

BENCHMARK_CAPTURE(PhTreeEraseKey3D, UPDATE_CU_100_of_100K, TestGenerator::CUBE, 100000)
    ->Unit(benchmark::kMillisecond);

BENCHMARK_CAPTURE(PhTreeEraseKey3D, UPDATE_CU_100_of_1M, TestGenerator::CUBE, 1000000)
    ->Unit(benchmark::kMillisecond);

// PhTree3D CLUSTER
BENCHMARK_CAPTURE(PhTreeEraseKey3D, UPDATE_CL_100_of_1K, TestGenerator::CLUSTER, 1000)
    ->Unit(benchmark::kMillisecond);

BENCHMARK_CAPTURE(PhTreeEraseKey3D, UPDATE_CL_100_of_10K, TestGenerator::CLUSTER, 10000)
    ->Unit(benchmark::kMillisecond);

BENCHMARK_CAPTURE(PhTreeEraseKey3D, UPDATE_CL_100_of_100K, TestGenerator::CLUSTER, 100000)
    ->Unit(benchmark::kMillisecond);

BENCHMARK_CAPTURE(PhTreeEraseKey3D, UPDATE_CL_100_of_1M, TestGenerator::CLUSTER, 1000000)
    ->Unit(benchmark::kMillisecond);

// index type, scenario name, data_type, num_entities, updates_per_round, move_distance
// PhTree3D CUBE
BENCHMARK_CAPTURE(PhTreeEraseIter3D, UPDATE_CU_100_of_1K, TestGenerator::CUBE, 1000)
    ->Unit(benchmark::kMillisecond);

BENCHMARK_CAPTURE(PhTreeEraseIter3D, UPDATE_CU_100_of_10K, TestGenerator::CUBE, 10000)
    ->Unit(benchmark::kMillisecond);

BENCHMARK_CAPTURE(PhTreeEraseIter3D, UPDATE_CU_100_of_100K, TestGenerator::CUBE, 100000)
    ->Unit(benchmark::kMillisecond);

BENCHMARK_CAPTURE(PhTreeEraseIter3D, UPDATE_CU_100_of_1M, TestGenerator::CUBE, 1000000)
    ->Unit(benchmark::kMillisecond);

// PhTree3D CLUSTER
BENCHMARK_CAPTURE(PhTreeEraseIter3D, UPDATE_CL_100_of_1K, TestGenerator::CLUSTER, 1000)
    ->Unit(benchmark::kMillisecond);

BENCHMARK_CAPTURE(PhTreeEraseIter3D, UPDATE_CL_100_of_10K, TestGenerator::CLUSTER, 10000)
    ->Unit(benchmark::kMillisecond);

BENCHMARK_CAPTURE(PhTreeEraseIter3D, UPDATE_CL_100_of_100K, TestGenerator::CLUSTER, 100000)
    ->Unit(benchmark::kMillisecond);

BENCHMARK_CAPTURE(PhTreeEraseIter3D, UPDATE_CL_100_of_1M, TestGenerator::CLUSTER, 1000000)
    ->Unit(benchmark::kMillisecond);

// index type, scenario name, data_type, num_entities, updates_per_round, move_distance
// PhTree3D CUBE
BENCHMARK_CAPTURE(PhTreeEmplaceHint3D, UPDATE_CU_100_of_1K, TestGenerator::CUBE, 1000)
    ->Unit(benchmark::kMillisecond);

BENCHMARK_CAPTURE(PhTreeEmplaceHint3D, UPDATE_CU_100_of_10K, TestGenerator::CUBE, 10000)
    ->Unit(benchmark::kMillisecond);

BENCHMARK_CAPTURE(PhTreeEmplaceHint3D, UPDATE_CU_100_of_100K, TestGenerator::CUBE, 100000)
    ->Unit(benchmark::kMillisecond);

BENCHMARK_CAPTURE(PhTreeEmplaceHint3D, UPDATE_CU_100_of_1M, TestGenerator::CUBE, 1000000)
    ->Unit(benchmark::kMillisecond);

// PhTree3D CLUSTER
BENCHMARK_CAPTURE(PhTreeEmplaceHint3D, UPDATE_CL_100_of_1K, TestGenerator::CLUSTER, 1000)
    ->Unit(benchmark::kMillisecond);

BENCHMARK_CAPTURE(PhTreeEmplaceHint3D, UPDATE_CL_100_of_10K, TestGenerator::CLUSTER, 10000)
    ->Unit(benchmark::kMillisecond);

BENCHMARK_CAPTURE(PhTreeEmplaceHint3D, UPDATE_CL_100_of_100K, TestGenerator::CLUSTER, 100000)
    ->Unit(benchmark::kMillisecond);

BENCHMARK_CAPTURE(PhTreeEmplaceHint3D, UPDATE_CL_100_of_1M, TestGenerator::CLUSTER, 1000000)
    ->Unit(benchmark::kMillisecond);
BENCHMARK_MAIN();
