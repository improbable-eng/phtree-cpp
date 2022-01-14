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

#ifndef PHTREE_V16_FOR_EACH_HC_H
#define PHTREE_V16_FOR_EACH_HC_H

#include "../common/common.h"
#include "iterator_simple.h"

namespace improbable::phtree::v16 {

/*
 * The HC (hyper cube) iterator uses `hypercube navigation`, ie. filtering of quadrants by their
 * binary hypercube address. In effect it compares the node's volume (box) with the query volume
 * (box) to calculate two bit masks, mask_lower_ and mask_upper_. These can be used as the number of
 * the lowest and highest quadrant that overlaps with the query box. They can also be used to tell
 * for any quadrant whether it overlaps with the query, simply by comparing the quadrant's ID with
 * the two masks, see IsPosValid().
 *
 * For details see  "Efficient Z-Ordered Traversal of Hypercube Indexes" by T. Zäschke, M.C. Norrie,
 * 2017.
 */
template <typename T, typename CONVERT, typename CALLBACK_FN, typename FILTER>
class ForEachHC {
    static constexpr dimension_t DIM = CONVERT::DimInternal;
    using KeyExternal = typename CONVERT::KeyExternal;
    using KeyInternal = typename CONVERT::KeyInternal;
    using SCALAR = typename CONVERT::ScalarInternal;
    using EntryT = Entry<DIM, T, SCALAR>;
    using NodeT = Node<DIM, T, SCALAR>;

  public:
    ForEachHC(
        const KeyInternal& range_min,
        const KeyInternal& range_max,
        const CONVERT& converter,
        CALLBACK_FN& callback,
        FILTER filter)
    : range_min_{range_min}
    , range_max_{range_max}
    , converter_{converter}
    , callback_{callback}
    , filter_(std::move(filter)) {}

    void run(const EntryT& root) {
        assert(root.IsNode());
        TraverseNode(root.GetKey(), root.GetNode());
    }

  private:
    void TraverseNode(const KeyInternal& key, const NodeT& node) {
        hc_pos_t mask_lower = 0;
        hc_pos_t mask_upper = 0;
        CalcLimits(node.GetPostfixLen(), key, mask_lower, mask_upper);
        auto iter = node.Entries().lower_bound(mask_lower);
        auto end = node.Entries().end();
        for (; iter != end && iter->first <= mask_upper; ++iter) {
            auto child_hc_pos = iter->first;
            // Use bit-mask magic to check whether we are in a valid quadrant.
            // -> See paper referenced in class description.
            if (((child_hc_pos | mask_lower) & mask_upper) == child_hc_pos) {
                const auto& child = iter->second;
                const auto& child_key = child.GetKey();
                if (child.IsNode()) {
                    const auto& child_node = child.GetNode();
                    if (CheckNode(child_key, child_node)) {
                        TraverseNode(child_key, child_node);
                    }
                } else {
                    T& value = child.GetValue();
                    if (IsInRange(child_key, range_min_, range_max_) &&
                        ApplyFilter(child_key, value)) {
                        callback_(converter_.post(child_key), value);
                    }
                }
            }
        }
    }

    bool CheckNode(const KeyInternal& key, const NodeT& node) const {
        // Check if the node overlaps with the query box.
        // An infix with len=0 implies that at least part of the child node overlaps with the query,
        // otherwise the bit mask checking would have returned 'false'.
        if (node.GetInfixLen() > 0) {
            // Mask for comparing the prefix with the query boundaries.
            assert(node.GetPostfixLen() + 1 < MAX_BIT_WIDTH<SCALAR>);
            SCALAR comparison_mask = MAX_MASK<SCALAR> << (node.GetPostfixLen() + 1);
            for (dimension_t dim = 0; dim < DIM; ++dim) {
                SCALAR prefix = key[dim] & comparison_mask;
                if (prefix > range_max_[dim] || prefix < (range_min_[dim] & comparison_mask)) {
                    return false;
                }
            }
        }
        return ApplyFilter(key, node);
    }

    [[nodiscard]] bool ApplyFilter(const KeyInternal& key, const NodeT& node) const {
        return filter_.IsNodeValid(key, node.GetPostfixLen() + 1);
    }

    [[nodiscard]] bool ApplyFilter(const KeyInternal& key, const T& value) const {
        return filter_.IsEntryValid(key, value);
    }

    void CalcLimits(
        bit_width_t postfix_len,
        const KeyInternal& prefix,
        hc_pos_t& lower_limit,
        hc_pos_t& upper_limit) {
        // create limits for the local node. there is a lower and an upper limit. Each limit
        // consists of a series of DIM bit, one for each dimension.
        // For the lower limit, a '1' indicates that the 'lower' half of this dimension does
        // not need to be queried.
        // For the upper limit, a '0' indicates that the 'higher' half does not need to be
        // queried.
        //
        //              ||  lower_limit=0 || lower_limit=1 || upper_limit = 0 || upper_limit = 1
        // =============||======================================================================
        // query lower  ||     YES              NO
        // ============ || =====================================================================
        // query higher ||                                       NO                  YES
        //
        assert(postfix_len < MAX_BIT_WIDTH<SCALAR>);
        bit_mask_t<SCALAR> maskHcBit = bit_mask_t<SCALAR>(1) << postfix_len;
        bit_mask_t<SCALAR> maskVT = MAX_MASK<SCALAR> << postfix_len;
        constexpr hc_pos_t ONE = 1;
        // to prevent problems with signed long when using 64 bit
        if (postfix_len < MAX_BIT_WIDTH<SCALAR> - 1) {
            for (dimension_t i = 0; i < DIM; ++i) {
                lower_limit <<= 1;
                upper_limit <<= 1;
                SCALAR nodeBisection = (prefix[i] | maskHcBit) & maskVT;
                if (range_min_[i] >= nodeBisection) {
                    //==> set to 1 if lower value should not be queried
                    lower_limit |= ONE;
                }
                if (range_max_[i] >= nodeBisection) {
                    // Leave 0 if higher value should not be queried.
                    upper_limit |= ONE;
                }
            }
        } else {
            // special treatment for signed longs
            // The problem (difference) here is that a '1' at the leading bit does indicate a
            // LOWER value, opposed to indicating a HIGHER value as in the remaining 63 bits.
            // The hypercube assumes that a leading '0' indicates a lower value.
            // Solution: We leave HC as it is.
            for (dimension_t i = 0; i < DIM; ++i) {
                lower_limit <<= 1;
                upper_limit <<= 1;
                if (range_min_[i] < 0) {
                    // If minimum is positive, we don't need the search negative values
                    //==> set upper_limit to 0, prevent searching values starting with '1'.
                    upper_limit |= ONE;
                }
                if (range_max_[i] < 0) {
                    // Leave 0 if higher value should not be queried
                    // If maximum is negative, we do not need to search positive values
                    //(starting with '0').
                    //--> lower_limit = '1'
                    lower_limit |= ONE;
                }
            }
        }
    }

    const KeyInternal range_min_;
    const KeyInternal range_max_;
    CONVERT converter_;
    CALLBACK_FN& callback_;
    FILTER filter_;
};
}  // namespace improbable::phtree::v16

#endif  // PHTREE_V16_FOR_EACH_HC_H
