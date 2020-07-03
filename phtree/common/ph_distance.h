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

#ifndef PHTREE_PH_COMMON_DISTANCES_H
#define PHTREE_PH_COMMON_DISTANCES_H

#include "ph_base_types.h"
#include "ph_bits.h"
#include "ph_flat_array_map.h"
#include "ph_flat_sparse_map.h"
#include "ph_preprocessor.h"
#include "ph_tree_stats.h"
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
 * - PhDistanceDoubleEuclidean:     Euclidean distance for PhPointD
 * - PhDistanceDoubleL1:            L1 distance (manhattan distance / taxi distance) for PhPointD
 * - PhDistanceLongEuclidean:       Euclidean distance for PhPoint
 */

template <dimension_t DIM>
struct PhDistanceDoubleEuclidean {
    double operator()(const PhPointD<DIM>& p1, const PhPointD<DIM>& p2) const {
        double sum2 = 0;
        for (dimension_t i = 0; i < DIM; i++) {
            double d2 = p1[i] - p2[i];
            sum2 += d2 * d2;
        }
        return sqrt(sum2);
    };
};

template <dimension_t DIM>
struct PhDistanceDoubleL1 {
    double operator()(const PhPointD<DIM>& v1, const PhPointD<DIM>& v2) const {
        double sum = 0;
        for (dimension_t i = 0; i < DIM; i++) {
            sum += std::abs(v1[i] - v2[i]);
        }
        return sum;
    };
};

template <dimension_t DIM>
struct PhDistanceLongEuclidean {
    double operator()(const PhPoint<DIM>& v1, const PhPoint<DIM>& v2) const {
        // Substraction of large long integers can easily overflow because the distance can be
        // larger than the value range. Such large values are common when using the IEEE
        // double-to-long converter, however, if we use a converter we should use a distance
        // function that processes converted values.
        double sum2 = 0;
        for (dimension_t i = 0; i < DIM; i++) {
            assert(
                (v1[i] >= 0) == (v2[i] >= 0) ||
                double(v1[i]) - double(v2[i]) < double(std::numeric_limits<scalar_t>::max()));
            double d2 = double(v1[i] - v2[i]);
            sum2 += d2 * d2;
        }
        return sqrt(sum2);
    };
};

}  // namespace improbable::phtree

#endif  // PHTREE_PH_COMMON_DISTANCES_H
