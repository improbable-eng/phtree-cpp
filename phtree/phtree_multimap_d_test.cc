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

#include "phtree/phtree_multimap.h"
#include <gtest/gtest.h>
#include <random>

using namespace improbable::phtree;

// Number of entries that have the same coordinate
static const size_t NUM_DUPL = 4;
static const double WORLD_MIN = -1000;
static const double WORLD_MAX = 1000;

template <dimension_t DIM>
using TestPoint = PhPointD<DIM>;

template <dimension_t DIM, typename T>
using TestTree = PhTreeMultiMap<DIM, T, ConverterIEEE<DIM>>;

class DoubleRng {
  public:
    DoubleRng(double minIncl, double maxExcl) : eng(), rnd{minIncl, maxExcl} {}

    double next() {
        return rnd(eng);
    }

  private:
    std::default_random_engine eng;
    std::uniform_real_distribution<double> rnd;
};

struct Id {
    Id() = default;

    explicit Id(const int i) : _i(i), data_{0} {};

    bool operator==(const Id& rhs) const {
        return _i == rhs._i;
    }

    Id& operator=(Id const& rhs) = default;

    int _i;
    int data_;
};

namespace std {
template <>
struct hash<Id> {
    size_t operator()(const Id& x) const {
        return std::hash<int>{}(x._i);
    }
};
};  // namespace std

struct IdHash {
    template <class T1, class T2>
    std::size_t operator()(std::pair<T1, T2> const& v) const {
        return std::hash<T1>()(v.size());
    }
};

struct PointDistance {
    PointDistance(double distance, size_t id) : _distance(distance), _id(id) {}

    double _distance;
    int _id;
};

bool comparePointDistanceAndId(PointDistance& i1, PointDistance& i2) {
    return (i1._distance != i2._distance) ? (i1._distance < i2._distance) : (i1._id < i2._id);
}

template <dimension_t DIM>
double distance(const TestPoint<DIM>& p1, const TestPoint<DIM>& p2) {
    double sum2 = 0;
    for (dimension_t i = 0; i < DIM; i++) {
        double d = p1[i] - p2[i];
        sum2 += d * d;
    }
    return sqrt(sum2);
}

template <dimension_t DIM>
double distanceL1(const TestPoint<DIM>& p1, const TestPoint<DIM>& p2) {
    double sum = 0;
    for (dimension_t i = 0; i < DIM; i++) {
        sum += std::abs(p1[i] - p2[i]);
    }
    return sum;
}

template <dimension_t DIM>
void generateCube(std::vector<TestPoint<DIM>>& points, size_t N) {
    assert(N % NUM_DUPL == 0);
    DoubleRng rng(WORLD_MIN, WORLD_MAX);
    auto reference_set = std::unordered_map<TestPoint<DIM>, size_t>();

    points.reserve(N);
    for (size_t i = 0; i < N / NUM_DUPL; i++) {
        // create duplicates, ie. entries with the same coordinates. However, avoid unintentional
        // duplicates.
        TestPoint<DIM> key{};
        for (dimension_t d = 0; d < DIM; ++d) {
            key[d] = rng.next();
        }
        if (reference_set.count(key) != 0) {
            i--;
            continue;
        }
        reference_set.emplace(key, i);
        for (size_t dupl = 0; dupl < NUM_DUPL; dupl++) {
            auto point = TestPoint<DIM>(key);
            points.push_back(point);
        }
    }
    ASSERT_EQ(reference_set.size(), N / NUM_DUPL);
    ASSERT_EQ(points.size(), N);
}

template <dimension_t DIM>
void SmokeTestBasicOps(size_t N) {
    TestTree<DIM, Id> tree;

    std::vector<TestPoint<DIM>> points;
    generateCube(points, N);

    ASSERT_EQ(0, tree.size());
    ASSERT_TRUE(tree.empty());
    PhTreeDebugHelper::CheckConsistency(tree);

    for (size_t i = 0; i < N; i++) {
        TestPoint<DIM>& p = points.at(i);
        ASSERT_LE(tree.count(p), i % NUM_DUPL);
        if (i % NUM_DUPL == 0) {
            ASSERT_EQ(tree.end(), tree.find(p));
        }

        Id id(i);
        if (i % 2 == 0) {
            ASSERT_TRUE(tree.emplace(p, id).second);
        } else {
            ASSERT_TRUE(tree.insert(p, id).second);
        }
        ASSERT_EQ(tree.count(p), i % NUM_DUPL + 1);
        ASSERT_NE(tree.end(), tree.find(p));
        ASSERT_EQ(id._i, tree.find(p, id)->_i);
        ASSERT_EQ(i + 1, tree.size());

        // try add again
        ASSERT_FALSE(tree.insert(p, id).second);
        ASSERT_FALSE(tree.emplace(p, id).second);
        ASSERT_EQ(tree.count(p), i % NUM_DUPL + 1);
        ASSERT_NE(tree.end(), tree.find(p));
        ASSERT_EQ(id._i, tree.find(p, id)->_i);
        ASSERT_EQ(i + 1, tree.size());
        ASSERT_FALSE(tree.empty());
    }

    for (size_t i = 0; i < N; i++) {
        TestPoint<DIM>& p = points.at(i);
        auto q = tree.begin_query({p, p});
        ASSERT_NE(q, tree.end());
        for (size_t j = 0; j < NUM_DUPL; j++) {
            ASSERT_EQ(i / NUM_DUPL, (*q)._i / NUM_DUPL);
            q++;
        }
        ASSERT_EQ(q, tree.end());
    }

    PhTreeDebugHelper::CheckConsistency(tree);

    for (size_t i = 0; i < N; i++) {
        TestPoint<DIM>& p = points.at(i);
        Id id(i);
        ASSERT_NE(tree.find(p), tree.end());
        size_t expected_remaining = (N - i - 1) % NUM_DUPL + 1;
        ASSERT_EQ(tree.count(p), expected_remaining);
        ASSERT_EQ(i, tree.find(p, id)->_i);
        if (i % 3 == 0) {
            ASSERT_EQ(1, tree.erase(p, id));
        } else {
            auto iter = tree.find(p, id);
            ASSERT_EQ(1, tree.erase(iter));
        }

        ASSERT_EQ(tree.count(p), expected_remaining - 1);
        if (expected_remaining - 1 == 0) {
            ASSERT_EQ(tree.end(), tree.find(p));
        }
        ASSERT_EQ(N - i - 1, tree.size());

        // try remove again
        ASSERT_EQ(0, tree.erase(p, id));
        ASSERT_EQ(tree.count(p), expected_remaining - 1);
        if (expected_remaining - 1 == 0) {
            ASSERT_EQ(tree.end(), tree.find(p));
        }
        ASSERT_EQ(N - i - 1, tree.size());
        if (i < N - 1) {
            ASSERT_FALSE(tree.empty());
        }
    }
    ASSERT_EQ(0, tree.size());
    ASSERT_TRUE(tree.empty());
    PhTreeDebugHelper::CheckConsistency(tree);
}

TEST(PhTreeMMDTest, SmokeTestBasicOps) {
    SmokeTestBasicOps<1>(10000);
    SmokeTestBasicOps<3>(10000);
    SmokeTestBasicOps<6>(10000);
    SmokeTestBasicOps<10>(10000);
    SmokeTestBasicOps<20>(1000);
    SmokeTestBasicOps<63>(100);
}

TEST(PhTreeMMDTest, TestDebug) {
    const dimension_t dim = 3;
    TestTree<dim, Id> tree;
    size_t N = 1000;

    std::vector<TestPoint<dim>> points;
    generateCube(points, N);

    using Debug = PhTreeDebugHelper;
    ASSERT_LE(10, Debug::ToString(tree, Debug::PrintDetail::name).length());
    ASSERT_GE(10, Debug::ToString(tree, Debug::PrintDetail::entries).length());
    ASSERT_GE(100, Debug::ToString(tree, Debug::PrintDetail::tree).length());
    ASSERT_EQ(0, Debug::GetStats(tree).size_);
    Debug::CheckConsistency(tree);

    for (size_t i = 0; i < N; i++) {
        TestPoint<dim>& p = points.at(i);
        Id id(i);
        ASSERT_TRUE(tree.insert(p, id).second);
    }

    ASSERT_LE(10, Debug::ToString(tree, Debug::PrintDetail::name).length());
    ASSERT_LE(N * 10, Debug::ToString(tree, Debug::PrintDetail::entries).length());
    ASSERT_LE(N * 10, Debug::ToString(tree, Debug::PrintDetail::tree).length());
    ASSERT_EQ(N / NUM_DUPL, Debug::GetStats(tree).size_);
    Debug::CheckConsistency(tree);

    tree.clear();

    ASSERT_LE(10, Debug::ToString(tree, Debug::PrintDetail::name).length());
    ASSERT_GE(10, Debug::ToString(tree, Debug::PrintDetail::entries).length());
    ASSERT_GE(100, Debug::ToString(tree, Debug::PrintDetail::tree).length());
    ASSERT_EQ(0, Debug::GetStats(tree).size_);
    Debug::CheckConsistency(tree);
}

TEST(PhTreeMMDTest, TestInsert) {
    const dimension_t dim = 3;
    TestTree<dim, Id> tree;
    size_t N = 1000;

    std::vector<TestPoint<dim>> points;
    generateCube(points, N);

    for (size_t i = 0; i < N; i++) {
        TestPoint<dim>& p = points.at(i);
        Id id(i);
        ASSERT_EQ(true, tree.insert(p, id).second);
        ASSERT_EQ(tree.count(p), i % NUM_DUPL + 1);
        ASSERT_EQ(id._i, tree.find(p, id)->_i);

        // try add again
        ASSERT_EQ(false, tree.insert(p, id).second);
        ASSERT_EQ(i, tree.insert(p, id).first._i);
        ASSERT_EQ(tree.count(p), i % NUM_DUPL + 1);
        ASSERT_EQ(id._i, tree.find(p, id)->_i);
    }
    ASSERT_EQ(N, tree.size());

    for (size_t i = 0; i < N; i++) {
        TestPoint<dim>& p = points.at(i);
        auto q = tree.begin_query({p, p});
        ASSERT_NE(q, tree.end());
        for (size_t j = 0; j < NUM_DUPL; j++) {
            ASSERT_EQ(i / NUM_DUPL, (*q)._i / NUM_DUPL);
            q++;
        }
        ASSERT_EQ(q, tree.end());
    }

    for (size_t i = 0; i < N; i++) {
        TestPoint<dim>& p = points.at(i);
        ASSERT_EQ(tree.count(p), NUM_DUPL);
        Id id(i);
        ASSERT_EQ(i, tree.find(p, id)->_i);
        ASSERT_EQ(i / NUM_DUPL, tree.find(p)->_i / NUM_DUPL);
    }
}

TEST(PhTreeMMDTest, TestEmplace) {
    const dimension_t dim = 3;
    TestTree<dim, Id> tree;
    size_t N = 1000;

    std::vector<TestPoint<dim>> points;
    generateCube(points, N);

    for (int i = 0; i < (int)N; i++) {
        TestPoint<dim>& p = points.at(i);
        Id id(i);
        ASSERT_EQ(true, tree.emplace(p, id).second);
        ASSERT_EQ(tree.count(p), i % NUM_DUPL + 1);
        ASSERT_EQ(id._i, tree.find(p, id)->_i);
        ASSERT_EQ(i + 1, tree.size());

        // try add again (same `identity`), this should NOT replace the existing value
        Id id2(i);
        id2.data_ = 42;
        ASSERT_EQ(false, tree.emplace(p, id2).second);
        ASSERT_EQ(i, tree.find(p, id2)->_i);
        ASSERT_EQ(0, tree.find(p, id2)->data_);
        ASSERT_EQ(i, tree.emplace(p, id).first._i);
        ASSERT_EQ(tree.count(p), i % NUM_DUPL + 1);

        // Check that the returned value is a reference
        tree.emplace(p, id2).first.data_++;
        ASSERT_EQ(1, tree.find(p, id)->data_);
        tree.emplace(p, id2).first.data_ = 0;
        ASSERT_EQ(0, tree.emplace(p, id).first.data_);
    }
    ASSERT_EQ(N, tree.size());

    for (size_t i = 0; i < N; i++) {
        TestPoint<dim>& p = points.at(i);
        auto q = tree.begin_query({p, p});
        ASSERT_NE(q, tree.end());
        for (size_t j = 0; j < NUM_DUPL; j++) {
            ASSERT_EQ(i / NUM_DUPL, (*q)._i / NUM_DUPL);
            q++;
        }
        ASSERT_EQ(q, tree.end());
    }

    for (size_t i = 0; i < N; i++) {
        TestPoint<dim>& p = points.at(i);
        Id id(i);
        ASSERT_EQ(tree.count(p), NUM_DUPL);
        ASSERT_EQ(i, tree.find(p, id)->_i);
    }
}

template <dimension_t DIM>
void populate(TestTree<DIM, size_t>& tree, std::vector<TestPoint<DIM>>& points, size_t N) {
    generateCube(points, N);
    for (size_t i = 0; i < N; i++) {
        ASSERT_TRUE(tree.insert(points[i], i).second);
    }
    ASSERT_EQ(N, tree.size());
}

template <dimension_t DIM>
void populate(TestTree<DIM, Id>& tree, std::vector<TestPoint<DIM>>& points, size_t N) {
    generateCube(points, N);
    for (size_t i = 0; i < N; i++) {
        ASSERT_TRUE(tree.emplace(points[i], (int)i).second);
    }
    ASSERT_EQ(N, tree.size());
}

TEST(PhTreeMMDTest, TestClear) {
    const dimension_t dim = 3;
    TestTree<dim, Id> tree;
    size_t N = 100;
    std::vector<TestPoint<dim>> points;

    ASSERT_TRUE(tree.empty());
    tree.clear();
    ASSERT_TRUE(tree.empty());

    populate(tree, points, N);

    ASSERT_FALSE(tree.empty());
    tree.clear();
    ASSERT_TRUE(tree.empty());
    points.clear();

    // try again
    populate(tree, points, N);

    ASSERT_FALSE(tree.empty());
    tree.clear();
    ASSERT_TRUE(tree.empty());
    points.clear();
}

TEST(PhTreeMMDTest, TestFind) {
    const dimension_t dim = 3;
    TestTree<dim, Id> tree;
    size_t N = 10000;
    std::vector<TestPoint<dim>> points;
    populate(tree, points, N);

    size_t i = 0;
    for (auto& p : points) {
        // test commutativity
        Id id(i);
        ASSERT_NE(tree.find(p), tree.end());
        ASSERT_NE(tree.end(), tree.find(p));
        ASSERT_NE(tree.find(p, id), tree.end());
        ASSERT_NE(tree.end(), tree.find(p, id));
        ASSERT_EQ(tree.find(p, id)->_i, i);
        i++;
    }

    TestPoint<dim> p{1, 1, 10000000};
    auto result = tree.find(p);
    ASSERT_EQ(result, tree.end());
    ASSERT_EQ(tree.end(), result);

    auto iter1 = tree.find(points[0]);
    auto iter2 = tree.find(points[0]);
    ASSERT_EQ(iter1, iter2);
    ASSERT_NE(tree.end(), iter1);
}

TEST(PhTreeMMDTest, TestUpdateWithEmplace) {
    const dimension_t dim = 3;
    TestTree<dim, Id> tree;
    size_t N = 10000;
    double delta = 20;
    std::vector<TestPoint<dim>> points;
    populate(tree, points, N);

    size_t i = 0;
    for (auto& p : points) {
        auto pOld = p;
        TestPoint<dim> pNew{pOld[0] + delta, pOld[1] + delta, pOld[2] + delta};
        size_t count_new = tree.count(pNew);
        size_t count_old = tree.count(pOld);
        size_t n = tree.erase(pOld, Id(i));
        ASSERT_EQ(1U, n);
        tree.emplace(pNew, Id(i));
        ASSERT_EQ(count_new + 1, tree.count(pNew));
        ASSERT_EQ(count_old - 1, tree.count(pOld));
        p = pNew;
        ++i;
    }

    ASSERT_EQ(N, tree.size());
    tree.clear();
}

TEST(PhTreeMMDTest, TestUpdateWithEmplaceHint) {
    const dimension_t dim = 3;
    TestTree<dim, Id> tree;
    size_t N = 10000;
    std::array<double, 4> deltas{0, 0.1, 1, 10};
    std::vector<TestPoint<dim>> points;
    populate(tree, points, N);

    int i = 0;
    size_t d_n = 0;
    for (auto& p : points) {
        auto pOld = p;
        d_n = (d_n + 1) % deltas.size();
        double delta = deltas[d_n];
        TestPoint<dim> pNew{pOld[0] + delta, pOld[1] + delta, pOld[2] + delta};
        auto iter = tree.find(pOld, Id(i));
        size_t n = tree.erase(iter);
        ASSERT_EQ(1U, n);
        ASSERT_TRUE(tree.emplace_hint(iter, pNew, Id(i)).second);
        ASSERT_EQ(Id(i), *tree.find(pNew, Id(i)));
        auto iterNew = tree.find(pNew, Id(i));
        ASSERT_FALSE(tree.emplace_hint(iterNew, pNew, Id(i)).second);
        p = pNew;
        ++i;
    }

    ASSERT_EQ(N, tree.size());
    tree.clear();
}

TEST(PhTreeMMDTest, TestUpdateWithRelocate) {
    const dimension_t dim = 3;
    TestTree<dim, Id> tree;
    size_t N = 10000;
    std::array<double, 4> deltas{0, 0.1, 1, 10};
    std::vector<TestPoint<dim>> points;
    populate(tree, points, N);

    size_t i = 0;
    size_t d_n = 0;
    for (auto& p : points) {
        auto pOld = p;
        d_n = (d_n + 1) % deltas.size();
        double delta = deltas[d_n];
        TestPoint<dim> pNew{pOld[0] + delta, pOld[1] + delta, pOld[2] + delta};
        ASSERT_EQ(1, tree.relocate(pOld, pNew, Id(i)));
        if (delta > 0.0) {
            // second time fails because value has already been moved
            ASSERT_EQ(0, tree.relocate(pOld, pNew, Id(i)));
        }
        ASSERT_EQ(Id(i), *tree.find(pNew, Id(i)));
        p = pNew;
        ++i;
    }

    ASSERT_EQ(N, tree.size());
    tree.clear();
}

TEST(PhTreeMMDTest, TestEraseByIterator) {
    const dimension_t dim = 3;
    TestTree<dim, Id> tree;
    size_t N = 10000;
    std::vector<TestPoint<dim>> points;
    populate(tree, points, N);

    size_t i = 0;
    for (auto& p : points) {
        ASSERT_NE(tree.end(), tree.find(p));
        auto iter = tree.find(p, Id(i));
        ASSERT_NE(tree.end(), iter);
        size_t count = tree.erase(iter);
        ASSERT_EQ(1U, count);
        ASSERT_EQ(tree.end(), tree.find(p, Id(i)));
        if (tree.size() % NUM_DUPL == 0) {
            ASSERT_EQ(tree.end(), tree.find(p));
        }
        i++;
    }

    ASSERT_EQ(0, tree.erase(tree.end()));
}

TEST(PhTreeMMDTest, TestEraseByIteratorQuery) {
    const dimension_t dim = 3;
    TestTree<dim, Id> tree;
    size_t N = 10000;
    std::vector<TestPoint<dim>> points;
    populate(tree, points, N);

    for (size_t i = 0; i < N; ++i) {
        auto iter = tree.begin();
        ASSERT_NE(tree.end(), iter);
        size_t count = tree.erase(iter);
        ASSERT_EQ(1U, count);
    }

    ASSERT_EQ(0, tree.erase(tree.end()));
}

TEST(PhTreeMMDTest, TestExtent) {
    const dimension_t dim = 3;
    TestTree<dim, Id> tree;
    size_t N = 10000;
    std::vector<TestPoint<dim>> points;
    populate(tree, points, N);

    int num_e = 0;
    auto qE = tree.begin();
    while (qE != tree.end()) {
        qE++;
        num_e++;
    }
    ASSERT_EQ(N, num_e);

    auto iter1 = tree.begin();
    auto iter2 = tree.begin();
    ASSERT_EQ(iter1, iter2);
    ASSERT_NE(tree.end(), iter1);
}

template <dimension_t DIM, typename T>
struct FilterEvenId {
    [[nodiscard]] constexpr bool IsEntryValid(const PhPoint<DIM>& key, const T& value) const {
        return value._i % 2 == 0;
    }
    [[nodiscard]] constexpr bool IsNodeValid(const PhPoint<DIM>& prefix, int bits_to_ignore) const {
        return true;
    }
};

TEST(PhTreeMMDTest, TestExtentFilter) {
    const dimension_t dim = 3;
    TestTree<dim, Id> tree;
    size_t N = 10000;
    std::vector<TestPoint<dim>> points;
    populate(tree, points, N);

    int num_e = 0;
    auto qE = tree.begin(FilterEvenId<dim, Id>());
    while (qE != tree.end()) {
        ASSERT_TRUE(qE->_i % 2 == 0);
        qE++;
        num_e++;
    }
    ASSERT_EQ(N, num_e * 2);
}

TEST(PhTreeMMDTest, TestExtentForEachFilter) {
    const dimension_t dim = 3;
    TestTree<dim, Id> tree;
    size_t N = 10000;
    std::vector<TestPoint<dim>> points;
    populate(tree, points, N);

    struct Counter {
        void operator()(const TestPoint<dim> key, const Id& t) {
            ++n_;
            ASSERT_EQ(points_[t._i], key);
            ASSERT_TRUE(t._i % 2 == 0);
        }
        std::vector<TestPoint<dim>>& points_;
        size_t n_ = 0;
    };
    Counter callback{points, 0};
    tree.for_each(callback, FilterEvenId<dim, Id>());
    ASSERT_EQ(N, callback.n_ * 2);
}

TEST(PhTreeMMDTest, TestRangeBasedForLoop) {
    const dimension_t dim = 3;
    TestTree<dim, Id> tree;
    size_t N = 10000;
    std::vector<TestPoint<dim>> points;
    populate(tree, points, N);

    size_t num_e1 = 0;
    for (auto& x : tree) {
        x.data_ = 42;
        num_e1++;
    }
    ASSERT_EQ(N, num_e1);

    // Check that we really had references and that data_ was changed
    size_t num_e2 = 0;
    for (auto& x : tree) {
        ASSERT_EQ(42, x.data_);
        num_e2++;
    }
    ASSERT_EQ(N, num_e2);
}

TEST(PhTreeMMDTest, TestEstimateCountIntersect) {
    const dimension_t dim = 3;
    TestTree<dim, Id> tree;
    size_t N = 1000;
    std::vector<TestPoint<dim>> points;
    populate(tree, points, N);

    // Test small
    for (auto& p : points) {
        size_t n = tree.estimate_count({p, p});
        ASSERT_LE(NUM_DUPL, n);
        // arbitrary upper limit: 10*NUM_DUPL
        ASSERT_GE(10, NUM_DUPL);
    }

    // Test medium (1/8 of volume), allow variation of 20% 0.8 / 1.2
    double min_2 = WORLD_MIN / 2;
    double max_2 = WORLD_MAX / 2;
    size_t n_medium = tree.estimate_count({{min_2, min_2, min_2}, {max_2, max_2, max_2}});
    ASSERT_LE(N / 8. * 0.8, n_medium);
    ASSERT_GE(N / 8. * 1.2, n_medium);

    // Test all
    size_t n_all =
        tree.estimate_count({{WORLD_MIN, WORLD_MIN, WORLD_MIN}, {WORLD_MAX, WORLD_MAX, WORLD_MAX}});
    ASSERT_EQ(N, n_all);
}

template <dimension_t DIM>
void referenceQuery(
    std::vector<TestPoint<DIM>>& points,
    TestPoint<DIM>& min,
    TestPoint<DIM>& max,
    std::set<size_t>& result) {
    for (size_t i = 0; i < points.size(); i++) {
        auto& p = points[i];
        bool match = true;
        for (dimension_t d = 0; d < DIM; d++) {
            match &= p[d] >= min[d] && p[d] <= max[d];
        }
        if (match) {
            result.insert(i);
        }
    }
}

// We use 'int&' because gtest does not compile with assertions in non-void functions.
template <dimension_t DIM>
void testQuery(TestPoint<DIM>& min, TestPoint<DIM>& max, size_t N, int& result) {
    TestTree<DIM, Id> tree;
    std::vector<TestPoint<DIM>> points;
    populate(tree, points, N);

    std::set<size_t> referenceResult;
    referenceQuery(points, min, max, referenceResult);

    result = 0;
    for (auto it = tree.begin_query({min, max}); it != tree.end(); it++) {
        auto& x = *it;
        ASSERT_GE(x._i, 0);
        ASSERT_EQ(referenceResult.count(x._i), 1);
        result++;
    }
    ASSERT_EQ(referenceResult.size(), result);
}

TEST(PhTreeMMDTest, TestWindowQuery0) {
    const dimension_t dim = 3;
    TestPoint<dim> p{-10000, -10000, -10000};
    int n = 0;
    testQuery<dim>(p, p, 10000, n);
    ASSERT_EQ(0, n);
}

TEST(PhTreeMMDTest, TestWindowQuery1) {
    size_t N = 1000;
    const dimension_t dim = 3;
    TestTree<dim, Id> tree;
    std::vector<TestPoint<dim>> points;
    populate(tree, points, N);

    int n = 0;
    for (size_t i = 0; i < N; i++) {
        TestPoint<dim>& p = points.at(i);
        auto q = tree.begin_query({p, p});
        ASSERT_NE(q, tree.end());
        auto& x = *q;
        for (size_t j = 0; j < NUM_DUPL; j++) {
            ASSERT_EQ(i / NUM_DUPL, x._i / NUM_DUPL);
            q++;
        }
        ASSERT_EQ(q, tree.end());
        n++;
    }
    ASSERT_EQ(N, n);
}

TEST(PhTreeMMDTest, TestWindowQueryMany) {
    const dimension_t dim = 3;
    TestPoint<dim> min{-100, -100, -100};
    TestPoint<dim> max{100, 100, 100};
    int n = 0;
    testQuery<dim>(min, max, 10000, n);
    ASSERT_LE(3, n);
    ASSERT_GE(100, n);
}

TEST(PhTreeMMDTest, TestWindowQueryAll) {
    const dimension_t dim = 3;
    const size_t N = 10000;
    TestPoint<dim> min{-10000, -10000, -10000};
    TestPoint<dim> max{10000, 10000, 10000};
    int n = 0;
    testQuery<dim>(min, max, N, n);
    ASSERT_EQ(N, n);
}

TEST(PhTreeMMDTest, TestWindowQueryManyMoving) {
    size_t N = 10000;
    const dimension_t dim = 3;
    TestTree<dim, Id> tree;
    std::vector<TestPoint<dim>> points;
    populate(tree, points, N);

    double query_length = 200;
    size_t nn = 0;
    for (int i = -120; i < 120; i++) {
        TestPoint<dim> min{i * 10., i * 9., i * 11.};
        TestPoint<dim> max{i * 10 + query_length, i * 9 + query_length, i * 11 + query_length};
        std::set<size_t> referenceResult;
        referenceQuery(points, min, max, referenceResult);

        size_t n = 0;
        for (auto it = tree.begin_query({min, max}); it != tree.end(); it++) {
            auto& x = *it;
            ASSERT_EQ(referenceResult.count(x._i), 1);
            n++;
            nn++;
        }
        ASSERT_EQ(referenceResult.size(), n);

        // basic check to ensure healthy queries
        ASSERT_GE(100, n);
    }
    ASSERT_LE(500, nn);
    ASSERT_GE(5000, nn);
}

TEST(PhTreeMMDTest, TestWindowForEachQueryManyMoving) {
    size_t N = 10000;
    const dimension_t dim = 3;
    TestTree<dim, Id> tree;
    std::vector<TestPoint<dim>> points;
    populate(tree, points, N);

    double query_length = 200;
    size_t nn = 0;
    for (int i = -120; i < 120; i++) {
        TestPoint<dim> min{i * 10., i * 9., i * 11.};
        TestPoint<dim> max{i * 10 + query_length, i * 9 + query_length, i * 11 + query_length};
        std::set<size_t> referenceResult;
        referenceQuery(points, min, max, referenceResult);

        struct Counter {
            void operator()(const TestPoint<dim>& key, const Id& t) {
                ++n_;
                ASSERT_EQ(referenceResult.count(t._i), 1);
            }
            std::set<size_t>& referenceResult;
            size_t n_ = 0;
        };

        size_t n = 0;
        Counter callback{referenceResult, 0};
        tree.for_each({min, max}, callback);
        n += callback.n_;
        nn += callback.n_;
        ASSERT_EQ(referenceResult.size(), n);

        // basic check to ensure healthy queries
        ASSERT_GE(100, n);
    }
    ASSERT_LE(500, nn);
    ASSERT_GE(5000, nn);
}

TEST(PhTreeMMDTest, TestWindowQueryIterators) {
    size_t N = 1000;
    const dimension_t dim = 3;
    TestTree<dim, Id> tree;
    std::vector<TestPoint<dim>> points;
    populate(tree, points, N);

    int n = 0;
    for (size_t i = 0; i < N; i++) {
        TestPoint<dim>& p = points.at(i);
        auto q1 = tree.begin_query({p, p});
        auto q2 = tree.begin_query({p, p});
        ASSERT_NE(q1, tree.end());
        ASSERT_NE(q2, tree.end());
        ASSERT_EQ(q1, q2);
        q1++;
        ASSERT_NE(q1, q2);
        q2++;
        n++;
    }
    ASSERT_EQ(N, n);
}

TEST(PhTreeMMDTest, TestWindowQueryFilter) {
    const dimension_t dim = 3;
    TestTree<dim, Id> tree;
    size_t N = 10000;
    std::vector<TestPoint<dim>> points;
    populate(tree, points, N);

    int num_e = 0;
    TestPoint<dim> min{-100, -100, -100};
    TestPoint<dim> max{100, 100, 100};
    auto qE = tree.begin_query({min, max}, FilterEvenId<dim, Id>());
    while (qE != tree.end()) {
        ASSERT_TRUE(qE->_i > -1);
        ASSERT_TRUE(qE->_i % 2 == 0);
        qE++;
        num_e++;
    }
    ASSERT_LE(2, num_e);
    ASSERT_GE(50, num_e);
}

TEST(PhTreeMMDTest, TestKnnQuery) {
    // deliberately allowing outside of main points range
    DoubleRng rng(-1500, 1500);
    const dimension_t dim = 3;
    const size_t N = 1000;
    const size_t Nq = 10;

    TestTree<dim, Id> tree;
    std::vector<TestPoint<dim>> points;
    populate(tree, points, N);

    for (size_t round = 0; round < 100; round++) {
        TestPoint<dim> center{rng.next(), rng.next(), rng.next()};

        // sort points manually
        std::vector<PointDistance> sorted_data;
        for (size_t i = 0; i < points.size(); i++) {
            double dist = distance(center, points[i]);
            sorted_data.emplace_back(dist, i);
        }
        std::sort(sorted_data.begin(), sorted_data.end(), comparePointDistanceAndId);

        size_t n = 0;
        double prevDist = -1;
        auto q = tree.begin_knn_query(Nq, center, DistanceEuclidean<3>());
        while (q != tree.end()) {
            // just read the entry
            auto& e = *q;
            ASSERT_EQ(sorted_data[n]._distance, q.distance());
            ASSERT_EQ(sorted_data[n]._id / NUM_DUPL, e._i / NUM_DUPL);
            ASSERT_EQ(points[sorted_data[n]._id], q.first());
            ASSERT_EQ(sorted_data[n]._id / NUM_DUPL, q->_i / NUM_DUPL);
            ASSERT_GE(q.distance(), prevDist);
            prevDist = q.distance();
            q++;
            n++;
        }
        ASSERT_EQ(Nq * NUM_DUPL, n);
    }
}

template <dimension_t DIM>
struct PhDistanceLongL1 {
    double operator()(const TestPoint<DIM>& v1, const TestPoint<DIM>& v2) const {
        double sum = 0;
        for (dimension_t i = 0; i < DIM; i++) {
            sum += std::abs(v1[i] - v2[i]);
        }
        return sum;
    };
};

TEST(PhTreeMMDTest, TestKnnQueryFilterAndDistanceL1) {
    // deliberately allowing outside of main points range
    DoubleRng rng(-1500, 1500);
    const dimension_t dim = 3;
    const size_t N = 100;
    const size_t Nq = 10;

    TestTree<dim, Id> tree;
    std::vector<TestPoint<dim>> points;
    populate(tree, points, N);

    for (size_t round = 0; round < 100; round++) {
        TestPoint<dim> center{rng.next(), rng.next(), rng.next()};

        // sort points manually by L1; skip every 2nd point
        std::vector<PointDistance> sorted_data;
        for (size_t i = 0; i < points.size(); i += 2) {
            double dist = distanceL1(center, points[i]);
            sorted_data.emplace_back(dist, i);
        }
        std::sort(sorted_data.begin(), sorted_data.end(), comparePointDistanceAndId);

        std::vector<PointDistance> sorted_results;
        size_t n = 0;
        double prevDist = -1;
        auto q = tree.begin_knn_query(Nq, center, PhDistanceLongL1<dim>(), FilterEvenId<dim, Id>());
        while (q != tree.end()) {
            // just read the entry
            auto& e = *q;
            sorted_results.emplace_back(q.distance(), e._i);
            if (sorted_data[n]._id == e._i) {
                ASSERT_EQ(points[sorted_data[n]._id], q.first());
                ASSERT_EQ(sorted_data[n]._id, q->_i);
            }

            ASSERT_GE(q.distance(), prevDist);
            prevDist = q.distance();
            q++;
            n++;
        }
        std::sort(sorted_results.begin(), sorted_results.end(), comparePointDistanceAndId);

        for (size_t i = 0; i < n; ++i) {
            auto& r = sorted_results[i];
            ASSERT_EQ(sorted_data[i]._distance, r._distance);
            ASSERT_EQ(sorted_data[i]._id, r._id);
        }
        ASSERT_EQ(Nq * NUM_DUPL / 2, n);
    }
}

TEST(PhTreeMMDTest, TestKnnQueryIterator) {
    // deliberately allowing outside of main points range
    DoubleRng rng(-1500, 1500);
    const dimension_t dim = 3;
    const size_t N = 1000;
    const size_t Nq = 10;

    TestTree<dim, Id> tree;
    std::vector<TestPoint<dim>> points;
    populate(tree, points, N);

    TestPoint<dim> center{rng.next(), rng.next(), rng.next()};
    size_t n = 0;
    auto q1 = tree.begin_knn_query(Nq, center, DistanceEuclidean<3>());
    auto q2 = tree.begin_knn_query(Nq, center, DistanceEuclidean<3>());
    while (q1 != tree.end()) {
        ASSERT_NE(q1, tree.end());
        ASSERT_NE(q2, tree.end());
        ASSERT_EQ(q1, q2);
        q1++;
        ASSERT_NE(q1, q2);
        q2++;
        n++;
    }
    ASSERT_EQ(Nq * NUM_DUPL, n);
}

TEST(PhTreeMMDTest, SmokeTestPoint0) {
    // Test edge case: empty tree
    TestPoint<3> p{1, 2, 3};
    TestTree<3, Id> tree;
    ASSERT_EQ(tree.size(), 0);
    ASSERT_EQ(tree.find(p), tree.end());

    auto q_window = tree.begin_query({p, p});
    ASSERT_EQ(q_window, tree.end());

    auto q_extent = tree.begin();
    ASSERT_EQ(q_extent, tree.end());

    auto q_knn = tree.begin_knn_query(10, p, DistanceEuclidean<3>());
    ASSERT_EQ(q_knn, tree.end());

    ASSERT_EQ(0, tree.erase(p, Id(-1)));
    ASSERT_EQ(0, tree.size());
    ASSERT_TRUE(tree.empty());
}

TEST(PhTreeMMDTest, SmokeTestPointInfinity) {
    // Test inifnity.
    double positive_infinity = std::numeric_limits<double>::infinity();
    double negative_infinity = -positive_infinity;
    PhPointD<3> p_pos{positive_infinity, positive_infinity, positive_infinity};
    PhPointD<3> p_neg{negative_infinity, negative_infinity, negative_infinity};
    PhPointD<3> p{1, 2, 3};
    TestTree<3, Id> tree;
    tree.emplace(p, Id{1});
    tree.emplace(p_pos, Id{10});
    tree.emplace(p_neg, Id{-10});
    ASSERT_EQ(tree.size(), 3);
    ASSERT_EQ(tree.find(p_neg, Id(-10))->_i, -10);
    ASSERT_EQ(tree.find(p, Id(1))->_i, 1);
    ASSERT_EQ(tree.find(p_pos, Id(10))->_i, 10);

    ASSERT_EQ(positive_infinity, positive_infinity);
    ASSERT_EQ(negative_infinity, negative_infinity);
    ASSERT_GT(positive_infinity, negative_infinity);

    // Note that the tree returns result in z-order, however, since the z-order is based on
    // the (unsigned) bit representation, negative values come _after_ positive values.
    auto q_window = tree.begin_query({p_neg, p_pos});
    ASSERT_EQ(1, q_window->_i);
    ++q_window;
    ASSERT_EQ(10, q_window->_i);
    ++q_window;
    ASSERT_EQ(-10, q_window->_i);
    ++q_window;
    ASSERT_EQ(q_window, tree.end());

    auto q_extent = tree.begin();
    ASSERT_EQ(1, q_extent->_i);
    ++q_extent;
    ASSERT_EQ(10, q_extent->_i);
    ++q_extent;
    ASSERT_EQ(-10, q_extent->_i);
    ++q_extent;
    ASSERT_EQ(q_extent, tree.end());

    auto q_knn = tree.begin_knn_query(10, p, DistanceEuclidean<3>());
    ASSERT_EQ(1, q_knn->_i);
    ++q_knn;
    ASSERT_NE(q_knn, tree.end());
    ++q_knn;
    ASSERT_NE(q_knn, tree.end());
    ++q_knn;
    ASSERT_EQ(q_knn, tree.end());

    ASSERT_EQ(1, tree.erase(p_neg, Id(-10)));
    ASSERT_EQ(1, tree.erase(p, Id(1)));
    ASSERT_EQ(1, tree.erase(p_pos, Id(10)));
    ASSERT_EQ(0, tree.size());
    ASSERT_EQ(0, tree.erase(p_neg, Id(-10)));
    ASSERT_EQ(0, tree.erase(p_pos, Id(10)));
    ASSERT_EQ(0, tree.size());
    ASSERT_TRUE(tree.empty());
}

TEST(PhTreeMMDTest, SmokeTestTreeAPI) {
    std::map<int, Id*> mapPtr;
    PhTreeMultiMapD<3, Id*> treePtr;
    Id* idPtr = new Id(1);
    treePtr.emplace(PhPointD<3>{1, 2, 3}, idPtr);
    treePtr.clear();
    delete idPtr;
}
