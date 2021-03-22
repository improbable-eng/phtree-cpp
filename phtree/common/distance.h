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

#ifndef PHTREE_COMMON_DISTANCES_H
#define PHTREE_COMMON_DISTANCES_H

#include "base_types.h"
#include "bits.h"
#include "converter.h"
#include "flat_array_map.h"
#include "flat_sparse_map.h"
#include "tree_stats.h"
#include <cassert>
#include <climits>
#include <cmath>
#include <functional>
#include <sstream>

namespace improbable::phtree {

/*
 * The PH-Tree supports different distance functions. These can be used
 * by the kNN (k nearest neighbor) query facility.
 *
 * The implementations in this file are:
 * - DistanceEuclidean:     Euclidean distance for PhPoint & PhPointD
 * - DistanceL1:            L1 distance (Manhattan distance / taxi distance) for PhPoint & PhPointD
 */

template <dimension_t DIM>
struct DistanceEuclidean {
    double operator()(const PhPoint<DIM>& v1, const PhPoint<DIM>& v2) const {
        double sum2 = 0;
        for (dimension_t i = 0; i < DIM; ++i) {
            assert(
                (v1[i] >= 0) != (v2[i] >= 0) ||
                double(v1[i]) - double(v2[i]) <
                    double(std::numeric_limits<decltype(v1[i] - v2[i])>::max()));
            double d2 = double(v1[i] - v2[i]);
            sum2 += d2 * d2;
        }
        return sqrt(sum2);
    };

    double operator()(const PhPointD<DIM>& p1, const PhPointD<DIM>& p2) const {
        double sum2 = 0;
        for (dimension_t i = 0; i < DIM; ++i) {
            double d2 = p1[i] - p2[i];
            sum2 += d2 * d2;
        }
        return sqrt(sum2);
    };
};

template <dimension_t DIM>
struct DistanceL1 {
    double operator()(const PhPoint<DIM>& v1, const PhPoint<DIM>& v2) const {
        double sum = 0;
        for (dimension_t i = 0; i < DIM; ++i) {
            assert(
                (v1[i] >= 0) != (v2[i] >= 0) ||
                double(v1[i]) - double(v2[i]) <
                    double(std::numeric_limits<decltype(v1[i] - v2[i])>::max()));
            sum += std::abs(double(v1[i] - v2[i]));
        }
        return sum;
    };

    double operator()(const PhPointD<DIM>& v1, const PhPointD<DIM>& v2) const {
        double sum = 0;
        for (dimension_t i = 0; i < DIM; ++i) {
            sum += std::abs(v1[i] - v2[i]);
        }
        return sum;
    };
};

}  // namespace improbable::phtree

#endif  // PHTREE_COMMON_DISTANCES_H
