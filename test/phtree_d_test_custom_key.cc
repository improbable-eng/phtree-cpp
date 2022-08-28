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

#include "phtree/phtree.h"
#include <include/gtest/gtest.h>
#include <random>

using namespace improbable::phtree;

static const double MY_MULTIPLIER = 1000000.;

/*
 * A custom key class.
 * This allows using custom classes directly as keys without having to convert them to
 * PhPoint or PhBox first.
 * However, the general converter still needs to convert them.
 *
 * Allowing custom keys may not give a huge advantage in terms of performance or convenience,
 * but we get it almost for free. This test also serves as an example how to implement it.
 */
struct MyPoint {
    // Required for testing, not by the PH-Tree
    bool operator==(const MyPoint& other) const {
        return x_ == other.x_ && y_ == other.y_ && z_ == other.z_;
    }

    // Required for testing, not by the PH-Tree
    bool operator!=(const MyPoint& other) const {
        return x_ != other.x_ || y_ != other.y_ || z_ == other.z_;
    }

    // Required for testing, not by the PH-Tree
    bool operator<(const MyPoint& other) const {
        // A very simple ordering
        return x_ < other.x_;
    }

    double x_;
    double y_;
    double z_;
};

using MyBox = std::pair<MyPoint, MyPoint>;

template <dimension_t DIM>
using TestPoint = MyPoint;

class MyConverterMultiply : public ConverterBase<3, 3, double, scalar_64_t, MyPoint, MyBox> {
    using BASE = ConverterPointBase<3, double, scalar_64_t>;
    using PointInternal = typename BASE::KeyInternal;
    using QueryBoxInternal = typename BASE::QueryBoxInternal;

  public:
    explicit MyConverterMultiply(double multiplier)
    : multiplier_{multiplier}, divider_{1. / multiplier} {}

    [[nodiscard]] PointInternal pre(const MyPoint& point) const {
        return {
            static_cast<long>(point.x_ * multiplier_),
            static_cast<long>(point.y_ * multiplier_),
            static_cast<long>(point.z_ * multiplier_)};
    }

    [[nodiscard]] MyPoint post(const PointInternal& in) const {
        return {in[0] * divider_, in[1] * divider_, in[2] * divider_};
    }

    [[nodiscard]] QueryBoxInternal pre_query(const MyBox& box) const {
        return {pre(box.first), pre(box.second)};
    }

  private:
    const double multiplier_;
    const double divider_;
};

template <dimension_t DIM, typename T>
using TestTree = PhTreeD<DIM, T, MyConverterMultiply>;

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

    explicit Id(const size_t i) : _i{static_cast<int>(i)} {}

    bool operator==(const Id& rhs) const {
        return _i == rhs._i;
    }

    Id& operator=(Id const& rhs) = default;

    int _i;
};

template <dimension_t DIM>
void generateCube(std::vector<TestPoint<DIM>>& points, size_t N) {
    DoubleRng rng(-1000, 1000);
    auto refTree = std::map<TestPoint<DIM>, size_t>();

    points.reserve(N);
    for (size_t i = 0; i < N; i++) {
        auto point = TestPoint<DIM>{rng.next(), rng.next(), rng.next()};
        if (refTree.count(point) != 0) {
            i--;
            continue;
        }

        refTree.emplace(point, i);
        points.push_back(point);
    }
    assert(refTree.size() == N);
    assert(points.size() == N);
}

template <dimension_t DIM>
void SmokeTestBasicOps() {
    MyConverterMultiply tm{MY_MULTIPLIER};
    TestTree<DIM, Id> tree(tm);
    size_t N = 10000;

    std::vector<TestPoint<DIM>> points;
    generateCube<DIM>(points, N);

    ASSERT_EQ(0, tree.size());
    ASSERT_TRUE(tree.empty());
    PhTreeDebugHelper::CheckConsistency(tree);

    for (size_t i = 0; i < N; i++) {
        TestPoint<DIM>& p = points.at(i);
        ASSERT_EQ(tree.count(p), 0);
        ASSERT_EQ(tree.end(), tree.find(p));

        Id id(i);
        if (i % 2 == 0) {
            ASSERT_TRUE(tree.emplace(p, id).second);
        } else {
            ASSERT_TRUE(tree.insert(p, id).second);
        }
        ASSERT_EQ(tree.count(p), 1);
        ASSERT_NE(tree.end(), tree.find(p));
        ASSERT_EQ(id._i, tree.find(p)->_i);
        ASSERT_EQ(i + 1, tree.size());

        // try add again
        ASSERT_FALSE(tree.insert(p, id).second);
        ASSERT_FALSE(tree.emplace(p, id).second);
        ASSERT_EQ(tree.count(p), 1);
        ASSERT_NE(tree.end(), tree.find(p));
        ASSERT_EQ(id._i, tree.find(p)->_i);
        ASSERT_EQ(i + 1, tree.size());
        ASSERT_FALSE(tree.empty());
    }

    for (size_t i = 0; i < N; i++) {
        TestPoint<DIM>& p = points.at(i);
        auto q = tree.begin_query({p, p});
        ASSERT_NE(q, tree.end());
        ASSERT_EQ(i, (*q)._i);
        q++;
        ASSERT_EQ(q, tree.end());
    }

    PhTreeDebugHelper::CheckConsistency(tree);

    for (size_t i = 0; i < N; i++) {
        TestPoint<DIM>& p = points.at(i);
        ASSERT_NE(tree.find(p), tree.end());
        ASSERT_EQ(tree.count(p), 1);
        ASSERT_EQ(i, tree.find(p)->_i);
        ASSERT_EQ(1, tree.erase(p));

        ASSERT_EQ(tree.count(p), 0);
        ASSERT_EQ(tree.end(), tree.find(p));
        ASSERT_EQ(N - i - 1, tree.size());

        // try remove again
        ASSERT_EQ(0, tree.erase(p));
        ASSERT_EQ(tree.count(p), 0);
        ASSERT_EQ(tree.end(), tree.find(p));
        ASSERT_EQ(N - i - 1, tree.size());
        if (i < N - 1) {
            ASSERT_FALSE(tree.empty());
        }
    }
    ASSERT_EQ(0, tree.size());
    ASSERT_TRUE(tree.empty());
    PhTreeDebugHelper::CheckConsistency(tree);
}

TEST(PhTreeDTestCustomKey, SmokeTestBasicOps) {
    SmokeTestBasicOps<3>();
}
