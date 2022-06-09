/*
 * Copyright 2022 Tilmann Zäschke
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

#include "b_plus_tree_map.h"
#include <gtest/gtest.h>
#include <random>

using namespace improbable::phtree;

TEST(PhTreeBptMapTest, SmokeTest) {
    const int max_size = 200;
    std::default_random_engine random_engine{0};
    std::uniform_int_distribution<> cube_distribution(0, max_size - 1);

    for (int i = 0; i < 10; i++) {
        b_plus_tree_map<size_t, max_size> test_map;
        std::map<size_t, size_t> reference_map;
        for (int j = 0; j < 2 * max_size; j++) {
            size_t val = cube_distribution(random_engine);
            bool hasVal = test_map.find(val) != test_map.end();
            bool hasValRef = reference_map.find(val) != reference_map.end();
            ASSERT_EQ(hasVal, hasValRef);
            if (!hasVal) {
                reference_map.emplace(val, val);
                test_map.emplace(val, val);
                test_map._check();
            }
            ASSERT_EQ(test_map.size(), reference_map.size());
            for (auto it : reference_map) {
                size_t vRef = it.first;
                size_t vMap = test_map.find(vRef)->second;
                ASSERT_EQ(vMap, vRef);
            }
            for (auto it : test_map) {
                size_t v = it.first;
                size_t vRef = reference_map.find(v)->second;
                size_t vMap = test_map.find(v)->second;
                ASSERT_EQ(vMap, vRef);
            }
        }
    }
}

TEST(PhTreeBptMapTest, SmokeTestWithTryEmplace) {
    const int max_size = 200;
    std::default_random_engine random_engine{0};
    std::uniform_int_distribution<> cube_distribution(0, max_size - 1);

    for (int i = 0; i < 10; i++) {
        b_plus_tree_map<size_t, max_size> test_map;
        std::map<size_t, size_t> reference_map;
        for (int j = 0; j < 2 * max_size; j++) {
            size_t val = cube_distribution(random_engine);
            bool hasVal = test_map.find(val) != test_map.end();
            bool hasValRef = reference_map.find(val) != reference_map.end();
            ASSERT_EQ(hasVal, hasValRef);
            if (!hasVal) {
                reference_map.emplace(val, val);
                test_map.try_emplace(val, val);
            }
            ASSERT_EQ(test_map.size(), reference_map.size());
            for (auto it : reference_map) {
                size_t vRef = it.first;
                size_t vMap = test_map.find(vRef)->second;
                ASSERT_EQ(vMap, vRef);
            }
            for (auto it : test_map) {
                size_t v = it.first;
                size_t vRef = reference_map.find(v)->second;
                size_t vMap = test_map.find(v)->second;
                ASSERT_EQ(vMap, vRef);
            }
        }
    }
}

TEST(PhTreeBptMapTest, SmokeTestWithErase) {
    const int max_size = 200;
    std::default_random_engine random_engine{0};
    std::uniform_int_distribution<> cube_distribution(0, max_size - 1);

    for (int i = 0; i < 10; i++) {
        b_plus_tree_map<size_t, max_size> test_map{};
        std::unordered_map<size_t, size_t> reference_map{};
        std::vector<size_t> key_list{};
        for (int j = 0; j < 2 * max_size; j++) {
            size_t val = cube_distribution(random_engine);
            bool hasVal = test_map.find(val) != test_map.end();
            bool hasValRef = reference_map.find(val) != reference_map.end();
            ASSERT_EQ(hasVal, hasValRef);
            if (!hasVal) {
                reference_map.emplace(val, val);
                test_map.try_emplace(val, val);
                key_list.emplace_back(val);
            }
        }

        std::shuffle(key_list.begin(), key_list.end(), random_engine);
        for (auto key : key_list) {
            if (key % 2 == 0) {
                test_map.erase(key);
            } else {
                auto it = test_map.find(key);
                ASSERT_NE(it, test_map.end());
                ASSERT_EQ(it->second, key);
                test_map.erase(it);
            }
            test_map._check();
            reference_map.erase(key);
            for (auto it : reference_map) {
                size_t vRef = it.first;
                size_t vMap = test_map.find(vRef)->second;
                ASSERT_EQ(vMap, vRef);
            }
            for (auto it : test_map) {
                size_t v = it.first;
                size_t vRef = reference_map.find(v)->second;
                size_t vMap = test_map.find(v)->second;
                ASSERT_EQ(vMap, vRef);
            }
            ASSERT_EQ(test_map.size(), reference_map.size());
        }
    }
}

TEST(PhTreeBptMapTest, SmokeTestLowerBound) {
    const int max_size = 200;
    std::default_random_engine random_engine{0};
    std::uniform_int_distribution<> cube_distribution(0, max_size - 1);

    for (int i = 0; i < 10; i++) {
        b_plus_tree_map<size_t, max_size> test_map;
        std::map<size_t, size_t> reference_map;
        for (int j = 0; j < 2 * max_size; j++) {
            size_t val = cube_distribution(random_engine);
            bool hasVal = test_map.find(val) != test_map.end();
            bool hasValRef = reference_map.find(val) != reference_map.end();
            ASSERT_EQ(hasVal, hasValRef);
            if (!hasVal) {
                reference_map.emplace(val, val);
                test_map.try_emplace(val, val);
            }
            ASSERT_EQ(test_map.size(), reference_map.size());
            for (auto it : reference_map) {
                size_t vRef = it.first;
                size_t vMap = test_map.lower_bound(vRef)->second;
                ASSERT_EQ(vMap, vRef);
            }
            for (auto it : test_map) {
                size_t v = it.first;
                size_t vRef = reference_map.find(v)->second;
                size_t vMap = test_map.lower_bound(v)->second;
                ASSERT_EQ(vMap, vRef);
            }
            for (size_t v = 0; v < max_size + 5; ++v) {
                auto itRef = reference_map.lower_bound(v);
                auto itMap = test_map.lower_bound(v);
                if (itRef == reference_map.end()) {
                    ASSERT_EQ(itMap, test_map.end());
                } else {
                    ASSERT_NE(itMap, test_map.end());
                    // ASSERT_EQ(v, itRef->second);
                    ASSERT_EQ(itRef->second, itMap->second);
                }
            }
        }
    }
}
