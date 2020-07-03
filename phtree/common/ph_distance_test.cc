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

#include "ph_common.h"
#include <gtest/gtest.h>
#include <random>

using namespace improbable::phtree;

TEST(PhTreeDistanceTest, DoubleEuclidean) {
    auto distance = PhDistanceDoubleEuclidean<2>();
    ASSERT_DOUBLE_EQ(5, distance({-1, -1}, {2, 3}));
}

TEST(PhTreeDistanceTest, DoubleL1) {
    auto distance = PhDistanceDoubleL1<2>();
    ASSERT_DOUBLE_EQ(7, distance({-1, -1}, {2, 3}));
}

TEST(PhTreeDistanceTest, LongEuclidean) {
    auto distance = PhDistanceLongEuclidean<2>();
    ASSERT_DOUBLE_EQ(5, distance({-1, -1}, {2, 3}));
}
