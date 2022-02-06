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

#ifndef PHTREE_COMMON_FILTERS_H
#define PHTREE_COMMON_FILTERS_H

#include "base_types.h"
#include "bits.h"
#include "converter.h"
#include "flat_array_map.h"
#include "flat_sparse_map.h"
#include "tree_stats.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <functional>
#include <limits>
#include <sstream>

namespace improbable::phtree {

/*
 * Any iterator that has a filter defined will traverse nodes or return values if and only if the
 * filter function returns 'true'. The filter functions are called for every node and every entry
 * (note: internally, nodes are also stored in entries, but these entries will be passed to the
 * filter for nodes) that the iterator encounters. By implication, it will never call the filter
 * function for nodes of entries if their respective parent node has already been rejected.
 *
 * There are separate filter functions for nodes and for key/value entries.
 *
 * Every filter needs to provide two functions:
 * - bool IsEntryValid(const PhPoint<DIM>& key, const T& value);
 *   This function is called for every key/value pair that the query encounters. The function
 *   should return 'true' iff the key/value should be added to the query result.
 *   The parameters are the key and value of the key/value pair.
 * - bool IsNodeValid(const PhPoint<DIM>& prefix, int bits_to_ignore);
 *   This function is called for every node that the query encounters. The function should
 *   return 'true' if the node should be traversed and searched for potential results.
 *   The parameters are the prefix of the node and the number of least significant bits of the
 *   prefix that can (and should) be ignored. The bits of the prefix that should be ignored can
 *   have any value.
 */

/*
 * The no-op filter is the default filter for the PH-Tree. It always returns 'true'.
 */
struct FilterNoOp {
    /*
     * @param key The key/coordinate of the entry.
     * @param value The value of the entry.
     * @returns This default implementation always returns `true`.
     */
    template <typename KEY, typename T>
    constexpr bool IsEntryValid(const KEY& /*key*/, const T& /*value*/) const {
        return true;
    }

    /*
     * @param prefix The prefix of node. Any coordinate in the nodes shares this prefix.
     * @param bits_to_ignore The number of bits of the prefix that should be ignored because they
     * are NOT the same for all coordinates in the node. For example, assuming 64bit values, if the
     * node represents coordinates that all share the first 10 bits of the prefix, then the value of
     * bits_to_ignore is 64-10=54.
     * @returns This default implementation always returns `true`.
     */
    template <typename KEY>
    constexpr bool IsNodeValid(const KEY& /*prefix*/, int /*bits_to_ignore*/) const {
        return true;
    }
};

/*
 * The AABB filter can be used to query a point tree for an axis aligned bounding box (AABB).
 * The result is equivalent to that of the 'begin_query(...)' function.
 */
template <typename CONVERTER = ConverterIEEE<3>>
class FilterAABB {
    using KeyExternal = typename CONVERTER::KeyExternal;
    using KeyInternal = typename CONVERTER::KeyInternal;
    using ScalarInternal = typename CONVERTER::ScalarInternal;

    static constexpr auto DIM = CONVERTER::DimInternal;

  public:
    FilterAABB(
        const KeyExternal& min_include,
        const KeyExternal& max_include,
        CONVERTER converter = CONVERTER())
    : min_external_{min_include}
    , max_external_{max_include}
    , min_internal_{converter.pre(min_include)}
    , max_internal_{converter.pre(max_include)}
    , converter_{converter} {};

    /*
     * This function allows resizing/shifting the AABB while iterating over the tree.
     */
    void set(const KeyExternal& min_include, const KeyExternal& max_include) {
        min_external_ = min_include;
        max_external_ = max_include;
        min_internal_ = converter_.pre(min_include);
        max_internal_ = converter_.pre(max_include);
    }

    template <typename T>
    [[nodiscard]] bool IsEntryValid(const KeyInternal& key, const T& /*value*/) const {
        auto point = converter_.post(key);
        for (dimension_t i = 0; i < DIM; ++i) {
            if (point[i] < min_external_[i] || point[i] > max_external_[i]) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] bool IsNodeValid(const KeyInternal& prefix, int bits_to_ignore) const {
        // Let's assume that we always want to traverse the root node (bits_to_ignore == 64)
        if (bits_to_ignore >= (MAX_BIT_WIDTH<ScalarInternal> - 1)) {
            return true;
        }
        ScalarInternal node_min_bits = MAX_MASK<ScalarInternal> << bits_to_ignore;
        ScalarInternal node_max_bits = ~node_min_bits;

        for (dimension_t i = 0; i < DIM; ++i) {
            if ((prefix[i] | node_max_bits) < min_internal_[i] ||
                (prefix[i] & node_min_bits) > max_internal_[i]) {
                return false;
            }
        }
        return true;
    }

  private:
    const KeyExternal min_external_;
    const KeyExternal max_external_;
    const KeyInternal min_internal_;
    const KeyInternal max_internal_;
    const CONVERTER converter_;
};

/*
 * The sphere filter can be used to query a point tree for a sphere.
 */
template <
    typename CONVERTER = ConverterIEEE<3>,
    typename DISTANCE = DistanceEuclidean<CONVERTER::DimInternal>>
class FilterSphere {
    using KeyExternal = typename CONVERTER::KeyExternal;
    using KeyInternal = typename CONVERTER::KeyInternal;
    using ScalarInternal = typename CONVERTER::ScalarInternal;
    using ScalarExternal = typename CONVERTER::ScalarExternal;

    static constexpr auto DIM = CONVERTER::DimInternal;

  public:
    FilterSphere(
        const KeyExternal& center,
        const ScalarExternal& radius,
        CONVERTER converter = CONVERTER(),
        DISTANCE distance_function = DISTANCE())
    : center_external_{center}
    , center_internal_{converter.pre(center)}
    , radius_{radius}
    , converter_{converter}
    , distance_function_{distance_function} {};

    template <typename T>
    [[nodiscard]] bool IsEntryValid(const KeyInternal& key, const T&) const {
        KeyExternal point = converter_.post(key);
        return distance_function_(center_external_, point) <= radius_;
    }

    /*
     * Calculate whether AABB encompassing all possible points in the node intersects with the
     * sphere.
     */
    [[nodiscard]] bool IsNodeValid(const KeyInternal& prefix, int bits_to_ignore) const {
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
    const KeyExternal center_internal_;
    const ScalarExternal radius_;
    const CONVERTER converter_;
    const DISTANCE distance_function_;
};

}  // namespace improbable::phtree

#endif  // PHTREE_COMMON_FILTERS_H
