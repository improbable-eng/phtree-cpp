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

#include "common.h"
#include <gtest/gtest.h>
#include <random>

using namespace improbable::phtree;

TEST(PhTreeFilterTest, FilterParaboloidTest) {
    FilterParaboloid<ConverterNoOp<2, scalar_64_t>, DistanceEuclidean<2>> filter{1, {5, 3}, 2};
    // root is always valid
    ASSERT_TRUE(filter.IsNodeValid({0, 0}, 63));
    // valid because node encompasses vertex
    ASSERT_TRUE(filter.IsNodeValid({1, 1}, 10));
    // valid because node cuts the parabola
    ASSERT_TRUE(filter.IsNodeValid({4, 4}, 2));
    // valid because parabola encompasses the node
    ASSERT_TRUE(filter.IsNodeValid({6, 20}, 1));
    // valid because parabola touches the corner of the node
    ASSERT_TRUE(filter.IsNodeValid({6, 4}, 1));
    // invalid because node is beside the parabola
    ASSERT_FALSE(filter.IsNodeValid({2, 8}, 1));
    // invalid because node is below the parabola
    ASSERT_FALSE(filter.IsNodeValid({4, 0}, 1));

    // valid because point is the vertex
    ASSERT_TRUE(filter.IsEntryValid({5, 3}, nullptr));
    // valid because point is within parabola on axis
    ASSERT_TRUE(filter.IsEntryValid({5, 4}, nullptr));
    // valid because point is within parabola
    ASSERT_TRUE(filter.IsEntryValid({6, 7}, nullptr));
    // valid because point on parabola
    ASSERT_TRUE(filter.IsEntryValid({6, 5}, nullptr));
    // invalid because point is below parabola
    ASSERT_FALSE(filter.IsEntryValid({4, 2}, nullptr));
    // invalid because point is outside parabola
    ASSERT_FALSE(filter.IsEntryValid({2, 6}, nullptr));

    FilterParaboloid<ConverterNoOp<2, scalar_64_t>, DistanceEuclidean<2>> filter2{0, {8, 10}, -1};
    // valid because parabola encompasses the node
    ASSERT_TRUE(filter2.IsNodeValid({0, 8}, 2));
    // valid because node cuts the parabola and contains the vertex
    ASSERT_TRUE(filter2.IsNodeValid({6, 10}, 2));
    // invalid because node is outside parabola
    ASSERT_FALSE(filter2.IsNodeValid({6, 4}, 2));
    // invalid because node is beyond parabola
    ASSERT_FALSE(filter2.IsNodeValid({16, 8}, 3));

    // valid because point is the vertex
    ASSERT_TRUE(filter2.IsEntryValid({8, 10}, nullptr));
    // valid because point is within parabola
    ASSERT_TRUE(filter2.IsEntryValid({2, 8}, nullptr));
    // invalid because point is outside parabola
    ASSERT_FALSE(filter2.IsEntryValid({4, 6}, nullptr));
    // invalid because point is beyond parabola
    ASSERT_FALSE(filter2.IsEntryValid({10, 8}, nullptr));
}

TEST(PhTreeFilterTest, BoxFilterTest) {
    FilterAABB<ConverterNoOp<2, scalar_64_t>> filter{{3, 3}, {7, 7}};
    // root is always valid
    ASSERT_TRUE(filter.IsNodeValid({0, 0}, 63));
    // valid because node encompasses the AABB
    ASSERT_TRUE(filter.IsNodeValid({1, 1}, 10));
    // valid
    ASSERT_TRUE(filter.IsNodeValid({7, 7}, 1));
    // invalid
    ASSERT_FALSE(filter.IsNodeValid({88, 5}, 1));

    ASSERT_TRUE(filter.IsEntryValid({3, 7}, nullptr));
    ASSERT_FALSE(filter.IsEntryValid({2, 8}, nullptr));
}

TEST(PhTreeFilterTest, FilterNoOpSmokeTest) {
    auto filter = FilterNoOp();
    ASSERT_TRUE(filter.IsNodeValid<PhPoint<3>>({3, 7, 2}, 10));
    ASSERT_TRUE(filter.IsEntryValid<PhPoint<3>>({3, 7, 2}, 10));
}