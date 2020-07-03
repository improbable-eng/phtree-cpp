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

#ifndef PHTREE_PH_COMMON_FILTERS_H
#define PHTREE_PH_COMMON_FILTERS_H

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
template <dimension_t DIM, typename T>
struct PhFilterNoOp {
    /*
     * @param key The key/coordinate of the entry.
     * @param value The value of the entry.
     * @returns This default implementation always returns `true`.
     */
    constexpr bool IsEntryValid(const PhPoint<DIM>& key, const T& value) const {
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
    constexpr bool IsNodeValid(const PhPoint<DIM>& prefix, int bits_to_ignore) const {
        return true;
    }
};

/*
 * The AABB filter can be used to query a point tree for an axis aligned bounding box (AABB).
 * The result is equivalent to that of the 'begin_query(...)' function.
 */
template <dimension_t DIM, typename T>
class PhFilterAABB {
  public:
    PhFilterAABB(const PhPoint<DIM>& minInclude, const PhPoint<DIM>& maxInclude)
    : minIncludeBits{minInclude}, maxIncludeBits{maxInclude} {};

    /*
     * This function allows resizing/shifting the AABB while iterating over the tree.
     */
    void set(const PhPoint<DIM>& minExclude, const PhPoint<DIM>& maxExclude) {
        minIncludeBits = minExclude;
        maxIncludeBits = maxExclude;
    }

    [[nodiscard]] bool IsEntryValid(const PhPoint<DIM>& key, const T& value) const {
        for (int i = 0; i < DIM; ++i) {
            if (key[i] < minIncludeBits[i] || key[i] > maxIncludeBits[i]) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] bool IsNodeValid(const PhPoint<DIM>& prefix, int bits_to_ignore) const {
        // Let's assume that we always want to traverse the root node (bits_to_ignore == 64)
        if (bits_to_ignore >= (MAX_BIT_WIDTH - 1)) {
            return true;
        }
        bit_mask_t maskMin = MAX_MASK << bits_to_ignore;
        bit_mask_t maskMax = ~maskMin;

        for (size_t i = 0; i < prefix.size(); ++i) {
            scalar_t minBits = prefix[i] & maskMin;
            scalar_t maxBits = prefix[i] | maskMax;
            if (maxBits < minIncludeBits[i] || minBits > maxIncludeBits[i]) {
                return false;
            }
        }
        return true;
    }

  private:
    const PhPoint<DIM> minIncludeBits;
    const PhPoint<DIM> maxIncludeBits;
};

}  // namespace improbable::phtree

#endif  // PHTREE_PH_COMMON_FILTERS_H
