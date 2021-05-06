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

#ifndef PHTREE_V16_ITERATOR_HC_H
#define PHTREE_V16_ITERATOR_HC_H

#include "../common/common.h"
#include "iterator_simple.h"

namespace improbable::phtree::v16 {

template <dimension_t DIM, typename T, typename SCALAR>
class Node;

namespace {
template <dimension_t DIM, typename T, typename SCALAR>
class NodeIterator;
}  // namespace

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
template <typename T, typename CONVERT, typename FILTER>
class IteratorHC : public IteratorBase<T, CONVERT, FILTER> {
    static constexpr dimension_t DIM = CONVERT::DimInternal;
    using KeyInternal = typename CONVERT::KeyInternal;
    using SCALAR = typename CONVERT::ScalarInternal;
    using EntryT = typename IteratorBase<T, CONVERT, FILTER>::EntryT;

  public:
    IteratorHC(
        const EntryT& root,
        const KeyInternal& range_min,
        const KeyInternal& range_max,
        const CONVERT& converter,
        FILTER filter)
    : IteratorBase<T, CONVERT, FILTER>(converter, filter)
    , stack_size_{0}
    , range_min_{range_min}
    , range_max_{range_max} {
        PrepareAndPush(root);
        FindNextElement();
    }

    IteratorHC& operator++() {
        FindNextElement();
        return *this;
    }

    IteratorHC operator++(int) {
        IteratorHC iterator(*this);
        ++(*this);
        return iterator;
    }

  private:
    void FindNextElement() {
        assert(!this->Finished());
        while (!IsEmpty()) {
            auto* p = &Peek();
            const EntryT* current_result = nullptr;
            while ((current_result = p->Increment(range_min_, range_max_))) {
                if (this->ApplyFilter(*current_result)) {
                    if (current_result->IsNode()) {
                        p = &PrepareAndPush(*current_result);
                    } else {
                        this->SetCurrentResult(current_result);
                        return;
                    }
                }
            }
            // no matching (more) elements found
            Pop();
        }
        // finished
        this->SetFinished();
    }

    auto& PrepareAndPush(const EntryT& entry) {
        assert(stack_size_ < stack_.size() - 1);
        auto& ni = stack_[stack_size_++];
        ni.init(range_min_, range_max_, entry.GetNode(), entry.GetKey());
        return ni;
    }

    auto& Peek() {
        assert(stack_size_ > 0);
        return stack_[stack_size_ - 1];
    }

    auto& Pop() {
        assert(stack_size_ > 0);
        return stack_[--stack_size_];
    }

    bool IsEmpty() {
        return stack_size_ == 0;
    }

    std::array<NodeIterator<DIM, T, SCALAR>, MAX_BIT_WIDTH<SCALAR>> stack_;
    size_t stack_size_;
    const KeyInternal range_min_;
    const KeyInternal range_max_;
};

namespace {
template <dimension_t DIM, typename T, typename SCALAR>
class NodeIterator {
    using KeyT = PhPoint<DIM, SCALAR>;
    using EntryT = Entry<DIM, T, SCALAR>;
    using NodeT = Node<DIM, T, SCALAR>;

  public:
    NodeIterator() : iter_{}, node_{nullptr}, mask_lower_{0}, mask_upper_(0) {}

    void init(const KeyT& range_min, const KeyT& range_max, const NodeT& node, const KeyT& prefix) {
        node_ = &node;
        CalcLimits(node.GetPostfixLen(), range_min, range_max, prefix);
        iter_ = node.Entries().lower_bound(mask_lower_);
    }

    /*
     * Advances the cursor.
     * @return TRUE iff a matching element was found.
     */
    const EntryT* Increment(const KeyT& range_min, const KeyT& range_max) {
        while (iter_ != node_->Entries().end() && iter_->first <= mask_upper_) {
            if (IsPosValid(iter_->first)) {
                const auto* be = &iter_->second;
                if (CheckEntry(*be, range_min, range_max)) {
                    ++iter_;
                    return be;
                }
            }
            ++iter_;
        }
        return nullptr;
    }

    bool CheckEntry(const EntryT& candidate, const KeyT& range_min, const KeyT& range_max) const {
        if (candidate.IsValue()) {
            return IsInRange(candidate.GetKey(), range_min, range_max);
        }

        auto& node = candidate.GetNode();
        // Check if node-prefix allows sub-node to contain any useful values.
        // An infix with len=0 implies that at least part of the child node overlaps with the query.
        if (node.GetInfixLen() == 0) {
            return true;
        }

        // Mask for comparing the prefix with the query boundaries.
        assert(node.GetPostfixLen() + 1 < MAX_BIT_WIDTH<SCALAR>);
        SCALAR comparison_mask = MAX_MASK<SCALAR> << (node.GetPostfixLen() + 1);
        auto& key = candidate.GetKey();
        for (dimension_t dim = 0; dim < DIM; ++dim) {
            SCALAR in = key[dim] & comparison_mask;
            if (in > range_max[dim] || in < (range_min[dim] & comparison_mask)) {
                return false;
            }
        }
        return true;
    }

  private:
    [[nodiscard]] bool IsPosValid(hc_pos_t key) const {
        return ((key | mask_lower_) & mask_upper_) == key;
    }

    void CalcLimits(
        bit_width_t postfix_len, const KeyT& range_min, const KeyT& range_max, const KeyT& prefix) {
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
        hc_pos_t lower_limit = 0;
        hc_pos_t upper_limit = 0;
        constexpr hc_pos_t ONE = 1;
        // to prevent problems with signed long when using 64 bit
        if (postfix_len < MAX_BIT_WIDTH<SCALAR> - 1) {
            for (dimension_t i = 0; i < DIM; ++i) {
                lower_limit <<= 1;
                upper_limit <<= 1;
                SCALAR nodeBisection = (prefix[i] | maskHcBit) & maskVT;
                if (range_min[i] >= nodeBisection) {
                    //==> set to 1 if lower value should not be queried
                    lower_limit |= ONE;
                }
                if (range_max[i] >= nodeBisection) {
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
                if (range_min[i] < 0) {
                    // If minimum is positive, we don't need the search negative values
                    //==> set upper_limit to 0, prevent searching values starting with '1'.
                    upper_limit |= ONE;
                }
                if (range_max[i] < 0) {
                    // Leave 0 if higher value should not be queried
                    // If maximum is negative, we do not need to search positive values
                    //(starting with '0').
                    //--> lower_limit = '1'
                    lower_limit |= ONE;
                }
            }
        }
        mask_lower_ = lower_limit;
        mask_upper_ = upper_limit;
    }

  private:
    EntryIteratorC<DIM, EntryT> iter_;
    const NodeT* node_;
    hc_pos_t mask_lower_;
    hc_pos_t mask_upper_;
};
}  // namespace
}  // namespace improbable::phtree::v16

#endif  // PHTREE_V16_ITERATOR_HC_H
