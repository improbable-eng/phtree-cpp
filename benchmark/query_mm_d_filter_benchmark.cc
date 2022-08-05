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
#include "benchmark/benchmark_util.h"
#include "benchmark/logging.h"
#include "phtree/phtree.h"
#include "phtree/phtree_multimap.h"
#include <benchmark/benchmark.h>
#include <random>

using namespace improbable;
using namespace improbable::phtree;
using namespace improbable::phtree::phbenchmark;

/*
 * Benchmark for querying entries in multi-map implementations.
 * This benchmarks uses a SPHERE shaped query!
 */
namespace {

const double GLOBAL_MAX = 10000;

enum Scenario { SPHERE_WQ, SPHERE, WQ, SPHERE_IT_WQ, LEGACY_WQ };

using TestPoint = PhPointD<3>;
using QueryBox = PhBoxD<3>;
using payload_t = TestPoint;
using BucketType = std::set<payload_t>;

struct Query {
    QueryBox box{};
    TestPoint center{};
    double radius{};
};

template <dimension_t DIM>
using CONVERTER = ConverterIEEE<DIM>;

template <dimension_t DIM>
using DistanceFn = DistanceEuclidean<DIM>;

template <dimension_t DIM>
using TestMap = PhTreeMultiMapD<DIM, payload_t, CONVERTER<DIM>>;

template <
    typename CONVERTER = ConverterIEEE<3>,
    typename DISTANCE = DistanceEuclidean<CONVERTER::DimInternal>>
class FilterSphereLegacy {
    using KeyExternal = typename CONVERTER::KeyExternal;
    using KeyInternal = typename CONVERTER::KeyInternal;
    using ScalarInternal = typename CONVERTER::ScalarInternal;
    using ScalarExternal = typename CONVERTER::ScalarExternal;

    static constexpr auto DIM = CONVERTER::DimInternal;

  public:
    FilterSphereLegacy(
        const KeyExternal& center,
        const ScalarExternal& radius,
        CONVERTER converter = CONVERTER(),
        DISTANCE distance_function = DISTANCE())
    : center_external_{center}
    , center_internal_{converter.pre(center)}
    , radius_{radius}
    , converter_{converter}
    , distance_function_{distance_function} {};

    template <typename BucketT>
    [[nodiscard]] bool IsEntryValid(const KeyInternal&, const BucketT&) const {
        // We simulate a legacy filter by returning 'true' for all buckets
        return true;
    }

    template <typename T>
    [[nodiscard]] bool IsBucketEntryValid(const KeyInternal& key, const T&) const {
        KeyExternal point = converter_.post(key);
        return distance_function_(center_external_, point) <= radius_;
    }

    /*
     * Calculate whether AABB encompassing all possible points in the node intersects with the
     * sphere.
     */
    [[nodiscard]] bool IsNodeValid(const KeyInternal& prefix, std::uint32_t bits_to_ignore) const {
        // we always want to traverse the root node (bits_to_ignore == 64)

        if (bits_to_ignore >= (MAX_BIT_WIDTH<ScalarInternal> - 1)) {
            return true;
        }

        ScalarInternal node_min_bits = MAX_MASK<ScalarInternal> << bits_to_ignore;
        ScalarInternal node_max_bits = ~node_min_bits;

        KeyInternal closest_in_bounds;
        for (dimension_t i = 0; i < DIM; ++i) {
            // calculate lower and upper bound for dimension for given node
            ScalarInternal lo = prefix[i] & node_min_bits;
            ScalarInternal hi = prefix[i] | node_max_bits;

            // choose value closest to center for dimension
            closest_in_bounds[i] = std::clamp(center_internal_[i], lo, hi);
        }

        KeyExternal closest_point = converter_.post(closest_in_bounds);
        return distance_function_(center_external_, closest_point) <= radius_;
    }

  private:
    const KeyExternal center_external_;
    const KeyInternal center_internal_;
    const ScalarExternal radius_;
    const CONVERTER converter_;
    const DISTANCE distance_function_;
};

template <dimension_t DIM, Scenario SCENARIO>
class IndexBenchmark {
  public:
    IndexBenchmark(benchmark::State& state, double avg_query_result_size_);

    void Benchmark(benchmark::State& state);

  private:
    void SetupWorld(benchmark::State& state);
    void QueryWorld(benchmark::State& state, const Query& query);
    void CreateQuery(Query& query);

    const TestGenerator data_type_;
    const size_t num_entities_;
    const double avg_query_result_size_;

    constexpr double query_endge_length() {
        return GLOBAL_MAX * pow(avg_query_result_size_ / (double)num_entities_, 1. / (double)DIM);
    };

    TestMap<DIM> tree_;
    std::default_random_engine random_engine_;
    std::uniform_real_distribution<> cube_distribution_;
    std::vector<PhPointD<DIM>> points_;
};

template <dimension_t DIM, Scenario SCENARIO>
IndexBenchmark<DIM, SCENARIO>::IndexBenchmark(benchmark::State& state, double avg_query_result_size)
: data_type_{static_cast<TestGenerator>(state.range(1))}
, num_entities_(state.range(0))
, avg_query_result_size_(avg_query_result_size)
, tree_{}
, random_engine_{1}
, cube_distribution_{0, GLOBAL_MAX}
, points_(num_entities_) {
    logging::SetupDefaultLogging();
    SetupWorld(state);
}

template <dimension_t DIM, Scenario SCENARIO>
void IndexBenchmark<DIM, SCENARIO>::Benchmark(benchmark::State& state) {
    Query query{};
    for (auto _ : state) {
        state.PauseTiming();
        CreateQuery(query);
        state.ResumeTiming();

        QueryWorld(state, query);
    }
}

template <dimension_t DIM>
void InsertEntry(TestMap<DIM>& tree, const PhPointD<DIM>& point, const payload_t& data) {
    tree.emplace(point, data);
}

bool CheckPosition(const payload_t& entity, const TestPoint& center, double radius) {
    const auto& point = entity;
    double dx = center[0] - point[0];
    double dy = center[1] - point[1];
    double dz = center[2] - point[2];
    return dx * dx + dy * dy + dz * dz <= radius * radius;
}

struct CounterCheckPosition {
    template <typename T>
    void operator()(const PhPointD<3>& p, const T&) {
        n_ += CheckPosition(p, center_, radius_);
    }
    const TestPoint& center_;
    double radius_;
    size_t n_;
};

struct Counter {
    void operator()(const PhPointD<3>&, const payload_t&) {
        ++n_;
    }
    size_t n_;
};

template <dimension_t DIM, Scenario SCENARIO>
typename std::enable_if<SCENARIO == Scenario::SPHERE_WQ, int>::type CountEntries(
    TestMap<DIM>& tree, const Query& query) {
    FilterMultiMapSphere filter{query.center, query.radius, tree.converter(), DistanceFn<DIM>()};
    Counter counter{0};
    tree.for_each(query.box, counter, filter);
    return counter.n_;
}

template <dimension_t DIM, Scenario SCENARIO>
typename std::enable_if<SCENARIO == Scenario::SPHERE, int>::type CountEntries(
    TestMap<DIM>& tree, const Query& query) {
    FilterMultiMapSphere filter{query.center, query.radius, tree.converter(), DistanceFn<DIM>()};
    Counter counter{0};
    tree.for_each(counter, filter);
    return counter.n_;
}

template <dimension_t DIM, Scenario SCENARIO>
typename std::enable_if<SCENARIO == Scenario::WQ, int>::type CountEntries(
    TestMap<DIM>& tree, const Query& query) {
    CounterCheckPosition counter{query.center, query.radius, 0};
    tree.for_each(query.box, counter);
    return counter.n_;
}

template <dimension_t DIM, Scenario SCENARIO>
typename std::enable_if<SCENARIO == Scenario::SPHERE_IT_WQ, int>::type CountEntries(
    TestMap<DIM>& tree, const Query& query) {
    FilterMultiMapSphere filter{query.center, query.radius, tree.converter(), DistanceFn<DIM>()};
    Counter counter{0};
    for (auto it = tree.begin_query(query.box, filter); it != tree.end(); ++it) {
        ++counter.n_;
    }
    return counter.n_;
}

template <dimension_t DIM, Scenario SCENARIO>
typename std::enable_if<SCENARIO == Scenario::LEGACY_WQ, int>::type CountEntries(
    TestMap<DIM>& tree, const Query& query) {
    // Legacy: use non-multi-map filter
    FilterSphereLegacy filter{query.center, query.radius, tree.converter(), DistanceFn<DIM>()};
    Counter counter{0};
    tree.for_each(query.box, counter, filter);
    return counter.n_;
}

template <dimension_t DIM, Scenario SCENARIO>
void IndexBenchmark<DIM, SCENARIO>::SetupWorld(benchmark::State& state) {
    logging::info("Setting up world with {} entities and {} dimensions.", num_entities_, DIM);
    // create data with about 10% duplicate coordinates
    CreatePointData<DIM>(points_, data_type_, num_entities_, 0, GLOBAL_MAX, 0.8);
    for (size_t i = 0; i < num_entities_; ++i) {
        InsertEntry(tree_, points_[i], points_[i]);
    }

    state.counters["query_rate"] = benchmark::Counter(0, benchmark::Counter::kIsRate);
    state.counters["result_rate"] = benchmark::Counter(0, benchmark::Counter::kIsRate);
    state.counters["avg_result_count"] = benchmark::Counter(0, benchmark::Counter::kAvgIterations);
    logging::info("World setup complete.");
}

template <dimension_t DIM, Scenario SCENARIO>
void IndexBenchmark<DIM, SCENARIO>::QueryWorld(benchmark::State& state, const Query& query) {
    int n = CountEntries<DIM, SCENARIO>(tree_, query);

    state.counters["query_rate"] += 1;
    state.counters["result_rate"] += n;
    state.counters["avg_result_count"] += n;
}

template <dimension_t DIM, Scenario SCENARIO>
void IndexBenchmark<DIM, SCENARIO>::CreateQuery(Query& query) {
    double radius = query_endge_length() * 0.5;
    for (dimension_t d = 0; d < DIM; ++d) {
        auto x = cube_distribution_(random_engine_);
        query.box.min()[d] = x - radius;
        query.box.max()[d] = x + radius;
        query.center[d] = x;
    }
    query.radius = radius;
}

}  // namespace

template <typename... Arguments>
void PhTree3DSphereWQ(benchmark::State& state, Arguments&&... arguments) {
    IndexBenchmark<3, Scenario::SPHERE_WQ> benchmark{state, arguments...};
    benchmark.Benchmark(state);
}

template <typename... Arguments>
void PhTree3DSphere(benchmark::State& state, Arguments&&... arguments) {
    IndexBenchmark<3, Scenario::SPHERE> benchmark{state, arguments...};
    benchmark.Benchmark(state);
}

template <typename... Arguments>
void PhTree3DWQ(benchmark::State& state, Arguments&&... arguments) {
    IndexBenchmark<3, Scenario::WQ> benchmark{state, arguments...};
    benchmark.Benchmark(state);
}

template <typename... Arguments>
void PhTree3DSphereITWQ(benchmark::State& state, Arguments&&... arguments) {
    IndexBenchmark<3, Scenario::SPHERE_IT_WQ> benchmark{state, arguments...};
    benchmark.Benchmark(state);
}

template <typename... Arguments>
void PhTree3DLegacyWQ(benchmark::State& state, Arguments&&... arguments) {
    IndexBenchmark<3, Scenario::LEGACY_WQ> benchmark{state, arguments...};
    benchmark.Benchmark(state);
}

// index type, scenario name, data_type, num_entities, avg_query_result_size
BENCHMARK_CAPTURE(PhTree3DSphereWQ, _100, 100.0)
    ->RangeMultiplier(10)
    ->Ranges({{1000, 1000 * 1000}, {TestGenerator::CUBE, TestGenerator::CLUSTER}})
    ->Unit(benchmark::kMillisecond);

BENCHMARK_CAPTURE(PhTree3DSphere, _100, 100.0)
    ->RangeMultiplier(10)
    ->Ranges({{1000, 1000 * 1000}, {TestGenerator::CUBE, TestGenerator::CLUSTER}})
    ->Unit(benchmark::kMillisecond);

BENCHMARK_CAPTURE(PhTree3DWQ, _100, 100.0)
    ->RangeMultiplier(10)
    ->Ranges({{1000, 1000 * 1000}, {TestGenerator::CUBE, TestGenerator::CLUSTER}})
    ->Unit(benchmark::kMillisecond);

BENCHMARK_CAPTURE(PhTree3DSphereITWQ, _100, 100.0)
    ->RangeMultiplier(10)
    ->Ranges({{1000, 1000 * 1000}, {TestGenerator::CUBE, TestGenerator::CLUSTER}})
    ->Unit(benchmark::kMillisecond);

BENCHMARK_CAPTURE(PhTree3DLegacyWQ, _100, 100.0)
    ->RangeMultiplier(10)
    ->Ranges({{1000, 1000 * 1000}, {TestGenerator::CUBE, TestGenerator::CLUSTER}})
    ->Unit(benchmark::kMillisecond);

BENCHMARK_MAIN();