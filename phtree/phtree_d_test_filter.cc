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
#include <gtest/gtest.h>
#include <random>
#include <unordered_set>

using namespace improbable::phtree;

template <dimension_t DIM>
using TestPoint = PhPointD<DIM>;

template <dimension_t DIM, typename T>
using TestTree = PhTreeD<DIM, T>;

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

    explicit Id(const int i) : _i(i){};

    bool operator==(const Id& rhs) const {
        return _i == rhs._i;
    }

    Id(Id const& rhs) = default;
    Id(Id&& rhs) = default;
    Id& operator=(Id const& rhs) = default;
    Id& operator=(Id&& rhs) = default;

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
    ASSERT_EQ(refTree.size(), N);
    ASSERT_EQ(points.size(), N);
}

template <dimension_t DIM>
void populate(TestTree<DIM, size_t>& tree, std::vector<TestPoint<DIM>>& points, size_t N) {
    generateCube(points, N);
    for (size_t i = 0; i < N; i++) {
        ASSERT_TRUE(tree.insert(points[i], i).second);
    }
    ASSERT_EQ(N, tree.size());
}

static int f_default_construct_ = 0;
static int f_construct_ = 0;
static int f_copy_construct_ = 0;
static int f_move_construct_ = 0;
static int f_copy_assign_ = 0;
static int f_move_assign_ = 0;
static int f_destruct_ = 0;

static void f_reset_id_counters() {
    f_default_construct_ = 0;
    f_construct_ = 0;
    f_copy_construct_ = 0;
    f_move_construct_ = 0;
    f_copy_assign_ = 0;
    f_move_assign_ = 0;
    f_destruct_ = 0;
}

template <dimension_t DIM, typename T>
struct FilterCount {
    FilterCount() : last_known{} {
        ++f_default_construct_;
    }

    explicit FilterCount(const T i) : last_known{i} {
        ++f_construct_;
    }

    FilterCount(const FilterCount& other) {
        ++f_copy_construct_;
        last_known = other.last_known;
    }

    FilterCount(FilterCount&& other) noexcept {
        ++f_move_construct_;
        last_known = other.last_known;
    }

    FilterCount& operator=(const FilterCount& other) noexcept {
        ++f_copy_assign_;
        last_known = other.last_known;
        return *this;
    }
    FilterCount& operator=(FilterCount&& other) noexcept {
        ++f_move_assign_;
        last_known = other.last_known;
        return *this;
    }

    ~FilterCount() {
        ++f_destruct_;
    }

    [[nodiscard]] constexpr bool IsEntryValid(const PhPoint<DIM>&, const T& value) {
        last_known = const_cast<T&>(value);
        return true;
    }
    [[nodiscard]] constexpr bool IsNodeValid(const PhPoint<DIM>&, int) {
        return true;
    }

    T last_known;
};

template <dimension_t DIM>
struct DistanceCount {
    DistanceCount() {
        ++f_default_construct_;
    }

    DistanceCount(const DistanceCount&) {
        ++f_copy_construct_;
    }

    DistanceCount(DistanceCount&&) noexcept {
        ++f_move_construct_;
    }

    DistanceCount& operator=(const DistanceCount&) noexcept {
        ++f_copy_assign_;
        return *this;
    }
    DistanceCount& operator=(DistanceCount&&) noexcept {
        ++f_move_assign_;
        return *this;
    }

    ~DistanceCount() {
        ++f_destruct_;
    }

    double operator()(const PhPointD<DIM>& p1, const PhPointD<DIM>& p2) const {
        double sum2 = 0;
        for (dimension_t i = 0; i < DIM; ++i) {
            double d2 = p1[i] - p2[i];
            sum2 += d2 * d2;
        }
        return sqrt(sum2);
    };
};

static size_t static_id = 0;

template <dimension_t DIM>
struct CallbackCount {
    CallbackCount() {
        static_id = 0;
        ++f_default_construct_;
    }

    CallbackCount(const CallbackCount&) {
        ++f_copy_construct_;
    }

    CallbackCount(CallbackCount&&) noexcept {
        ++f_move_construct_;
    }

    CallbackCount& operator=(const CallbackCount&) noexcept {
        ++f_copy_assign_;
        return *this;
    }
    CallbackCount& operator=(CallbackCount&&) noexcept {
        ++f_move_assign_;
        return *this;
    }

    ~CallbackCount() {
        ++f_destruct_;
    }

    void operator()(TestPoint<DIM>, Id& t) {
        static_id = t._i;
    }
};

template <dimension_t DIM, typename T>
struct FilterConst {
    [[nodiscard]] constexpr bool IsEntryValid(const PhPoint<DIM>&, const T& value) const {
        assert(value._i == 1);
        return true;
    }
    [[nodiscard]] constexpr bool IsNodeValid(const PhPoint<DIM>&, int) const {
        return true;
    }
};

template <dimension_t DIM>
struct CallbackConst {
    void operator()(const TestPoint<DIM>, const Id& t) const {
        static_id = t._i;
    }
};

static void print_id_counters() {
    std::cout << "dc=" << f_default_construct_ << " c=" << f_construct_
              << " cc=" << f_copy_construct_ << " mc=" << f_move_construct_
              << " ca=" << f_copy_assign_ << " ma=" << f_move_assign_ << " d=" << f_destruct_
              << std::endl;
}

TEST(PhTreeTest, TestFilterAPI_FOR_EACH) {
    // Test edge case: only one entry in tree
    PhPointD<3> p{1, 2, 3};
    auto tree = TestTree<3, Id>();
    tree.emplace(p, Id{1});

    CallbackCount<3> callback;
    FilterCount<3, Id> filter{};
    // rvalue
    tree.for_each(callback, filter);
    ASSERT_EQ(static_id, 1);
    ASSERT_EQ(2, f_construct_ + f_default_construct_);
    ASSERT_EQ(0, f_copy_construct_ + f_move_construct_ + f_copy_assign_ + f_move_assign_);
    f_reset_id_counters();

    // lvalue
    tree.for_each(CallbackCount<3>(), FilterCount<3, Id>());
    ASSERT_EQ(static_id, 1);
    ASSERT_EQ(2, f_construct_ + f_default_construct_);
    ASSERT_LE(1, f_copy_construct_ + f_move_construct_ + f_copy_assign_ + f_move_assign_);
    f_reset_id_counters();

    // const Tree: just test that it compiles
    const TestTree<3, Id>& treeC = tree;
    // lvalue
    CallbackCount<3> callbackC;
    FilterConst<3, Id> filterC;
    treeC.for_each(callbackC, filterC);
    // rvalue
    treeC.for_each(CallbackConst<3>{}, FilterConst<3, Id>());
    f_reset_id_counters();
}

TEST(PhTreeTest, TestFilterAPI_FOR_EACH_WQ) {
    // Test edge case: only one entry in tree
    PhPointD<3> p{1, 2, 3};
    auto tree = TestTree<3, Id>();
    tree.emplace(p, Id{1});

    TestTree<3, Id>::QueryBox qb{{1, 2, 3}, {4, 5, 6}};
    CallbackCount<3> callback;
    FilterCount<3, Id> filter{};
    // lvalue
    tree.for_each(qb, callback, filter);
    ASSERT_EQ(static_id, 1);
    ASSERT_EQ(2, f_construct_ + f_default_construct_);
    ASSERT_EQ(0, f_copy_construct_ + f_move_construct_ + f_copy_assign_ + f_move_assign_);
    f_reset_id_counters();

    // rvalue
    tree.for_each({{1, 2, 3}, {4, 5, 6}}, CallbackCount<3>{}, FilterCount<3, Id>());
    ASSERT_EQ(static_id, 1);
    ASSERT_EQ(2, f_construct_ + f_default_construct_);
    ASSERT_LE(1, f_copy_construct_ + f_move_construct_ + f_copy_assign_ + f_move_assign_);
    f_reset_id_counters();

    // const Tree: just test that it compiles
    const TestTree<3, Id>& treeC = tree;
    // lvalue
    FilterConst<3, Id> filterC;
    treeC.for_each(qb, callback, filterC);
    // rvalue
    treeC.for_each({{1, 2, 3}, {4, 5, 6}}, CallbackConst<3>(), FilterConst<3, Id>());
    f_reset_id_counters();
}

TEST(PhTreeTest, TestFilterAPI_BEGIN) {
    // Test edge case: only one entry in tree
    PhPointD<3> p{1, 2, 3};
    auto tree = TestTree<3, Id>();
    tree.emplace(p, Id{1});

    FilterCount<3, Id> filter{};
    // lvalue
    ASSERT_EQ(tree.begin(filter)->_i, 1);
    ASSERT_EQ(1, f_construct_ + f_default_construct_);
    ASSERT_EQ(0, f_copy_construct_ + f_move_construct_ + f_copy_assign_ + f_move_assign_);
    f_reset_id_counters();

    // rvalue
    ASSERT_EQ(tree.begin(FilterCount<3, Id>())->_i, 1);
    ASSERT_EQ(1, f_construct_ + f_default_construct_);
    ASSERT_LE(1, f_copy_construct_ + f_move_construct_ + f_copy_assign_ + f_move_assign_);
    f_reset_id_counters();

    // const Tree: just test that it compiles
    const TestTree<3, Id>& treeC = tree;
    // lvalue
    FilterConst<3, Id> filterC;
    ASSERT_EQ(treeC.begin(filterC)->_i, 1);
    // rvalue
    ASSERT_EQ(treeC.begin(FilterConst<3, Id>())->_i, 1);
    f_reset_id_counters();
}

TEST(PhTreeTest, TestFilterAPI_WQ) {
    // Test edge case: only one entry in tree
    PhPointD<3> p{1, 2, 3};
    auto tree = TestTree<3, Id>();
    tree.emplace(p, Id{1});

    TestTree<3, Id>::QueryBox qb{{1, 2, 3}, {4, 5, 6}};
    FilterCount<3, Id> filter{};
    // lvalue
    ASSERT_EQ(tree.begin_query(qb, filter)->_i, 1);
    ASSERT_EQ(1, f_construct_ + f_default_construct_);
    ASSERT_EQ(0, f_copy_construct_ + f_move_construct_ + f_copy_assign_ + f_move_assign_);
    f_reset_id_counters();

    // rvalue
    ASSERT_EQ(tree.begin_query({{1, 2, 3}, {4, 5, 6}}, FilterCount<3, Id>())->_i, 1);
    ASSERT_EQ(1, f_construct_ + f_default_construct_);
    ASSERT_LE(1, f_copy_construct_ + f_move_construct_ + f_copy_assign_ + f_move_assign_);
    f_reset_id_counters();

    // const Tree: just test that it compiles
    const TestTree<3, Id>& treeC = tree;
    // lvalue
    FilterConst<3, Id> filterC;
    ASSERT_EQ(treeC.begin_query(qb, filterC)->_i, 1);
    // rvalue
    ASSERT_EQ(treeC.begin_query(qb, FilterConst<3, Id>())->_i, 1);
    f_reset_id_counters();
}

TEST(PhTreeTest, TestFilterAPI_KNN) {
    // Test edge case: only one entry in tree
    PhPointD<3> p{1, 2, 3};
    auto tree = TestTree<3, Id>();
    tree.emplace(p, Id{1});

    FilterCount<3, Id> filter{};
    DistanceCount<3> dist_fn{};
    // lvalue
    ASSERT_EQ(tree.begin_knn_query(3, {2, 3, 4}, dist_fn, filter)->_i, 1);
    ASSERT_EQ(2, f_construct_ + f_default_construct_);
    ASSERT_EQ(0, f_copy_construct_ + f_move_construct_ + f_copy_assign_ + f_move_assign_);
    f_reset_id_counters();

    // rvalue
    ASSERT_EQ(tree.begin_knn_query(3, {2, 3, 4}, DistanceCount<3>{}, FilterCount<3, Id>())->_i, 1);
    ASSERT_EQ(2, f_construct_ + f_default_construct_);
    ASSERT_LE(0, f_copy_construct_ + f_move_construct_ + f_copy_assign_ + f_move_assign_);
    f_reset_id_counters();

    // rvalue #2
    auto a = tree.begin_knn_query<DistanceCount<3>, FilterCount<3, Id>>(3, {2, 3, 4})->_i;
    ASSERT_EQ(a, 1);
    ASSERT_EQ(2, f_construct_ + f_default_construct_);
    ASSERT_LE(0, f_copy_construct_ + f_move_construct_ + f_copy_assign_ + f_move_assign_);
    f_reset_id_counters();

    // const Tree: just test that it compiles
    const TestTree<3, Id>& treeC = tree;
    // lvalue
    FilterConst<3, Id> filterC;
    ASSERT_EQ(treeC.begin_knn_query(3, {2, 3, 4}, dist_fn, filterC)->_i, 1);
    // rvalue
    ASSERT_EQ(treeC.begin_knn_query(3, {2, 3, 4}, DistanceCount<3>{}, FilterConst<3, Id>())->_i, 1);
    f_reset_id_counters();
}