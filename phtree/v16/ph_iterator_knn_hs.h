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

#ifndef PHTREE_V16_PH_QUERY_KNN_HS_H
#define PHTREE_V16_PH_QUERY_KNN_HS_H

#include "../common/ph_common.h"
#include "ph_iterator_base.h"
#include <cfloat>
#include <queue>

namespace improbable::phtree::v16 {

/*
 * kNN query implementation that uses preprocessors and distance functions.
 *
 * Implementation after Hjaltason and Samet (with some deviations: no MinDist or MaxDist used).
 * G. R. Hjaltason and H. Samet., "Distance browsing in spatial databases.", ACM TODS
 * 24(2):265--318. 1999
 */

namespace {
template <dimension_t DIM, typename T>
using PhEntryDist = std::pair<double, const PhEntry<DIM, T>*>;

template <dimension_t DIM, typename T>
struct ComparePhEntryDistByDistance {
    bool operator()(const PhEntryDist<DIM, T>& left, const PhEntryDist<DIM, T>& right) const {
        return left.first > right.first;
    };
};
}  // namespace

template <
    dimension_t DIM,
    typename T,
    typename KEY,
    PhPostprocessor<DIM, KEY> POST,
    typename DISTANCE,
    typename FILTER>
class PhIteratorKnnHS : public PhIteratorBase<DIM, T, KEY, POST, FILTER> {
  public:
    explicit PhIteratorKnnHS(
        const PhEntry<DIM, T>& root,
        size_t min_results,
        const PhPoint<DIM>& center,
        DISTANCE dist,
        FILTER filter)
    : PhIteratorBase<DIM, T, KEY, POST, FILTER>(filter)
    , center_{center}
    , center_post_{POST(center)}
    , current_distance_{DBL_MAX}
    , num_found_results_(0)
    , num_requested_results_(min_results)
    , distance_(std::move(dist)) {
        if (min_results <= 0 || root.GetNode().GetEntryCount() == 0) {
            this->SetFinished();
            return;
        }

        // Initialize queue, use d=0 because every imaginable point lies inside the root Node
        queue_.emplace(0, &root);
        FindNextElement();
    }

    [[nodiscard]] double distance() const {
        return current_distance_;
    }

    PhIteratorKnnHS& operator++() {
        FindNextElement();
        return *this;
    }

    PhIteratorKnnHS operator++(int) {
        PhIteratorKnnHS iterator(*this);
        ++(*this);
        return iterator;
    }

  private:
    void FindNextElement() {
        while (num_found_results_ < num_requested_results_ && !queue_.empty()) {
            auto& candidate = queue_.top();
            auto o = candidate.second;
            if (!o->IsNode()) {
                // data entry
                ++num_found_results_;
                this->SetCurrentResult(o);
                current_distance_ = candidate.first;
                // We need to pop() AFTER we processed the value, otherwise the reference is
                // overwritten.
                queue_.pop();
                return;
            } else {
                // inner node
                auto& node = o->GetNode();
                queue_.pop();
                for (auto& entry : node.Entries()) {
                    auto& e2 = entry.second;
                    if (this->ApplyFilter(e2)) {
                        if (e2.IsNode()) {
                            auto& sub = e2.GetNode();
                            double d = DistanceToNode(e2.GetKey(), sub.GetPostfixLen() + 1);
                            queue_.emplace(d, &e2);
                        } else {
                            double d = distance_(center_post_, POST(e2.GetKey()));
                            queue_.emplace(d, &e2);
                        }
                    }
                }
            }
        }
        this->SetFinished();
        current_distance_ = DBL_MAX;
    }

    double DistanceToNode(const PhPoint<DIM>& prefix, int bits_to_ignore) {
        assert(bits_to_ignore < MAX_BIT_WIDTH);
        scalar_t mask_min = MAX_MASK << bits_to_ignore;
        scalar_t mask_max = ~mask_min;
        PhPoint<DIM> buf;
        // The following calculates the point inside of the node that is closest to center_.
        // If center is inside the node this returns center_, otherwise it finds a point on the
        // node's surface.
        for (dimension_t i = 0; i < DIM; ++i) {
            // if center_[i] is outside the node, return distance to closest edge,
            // otherwise return center_[i] itself (assume possible distance=0)
            scalar_t min = prefix[i] & mask_min;
            scalar_t max = prefix[i] | mask_max;
            buf[i] = min > center_[i] ? min : (max < center_[i] ? max : center_[i]);
        }
        return distance_(center_post_, POST(buf));
    }

  private:
    const PhPoint<DIM> center_;
    // center after post processing == the external representation
    const KEY center_post_;
    double current_distance_;
    std::priority_queue<
        PhEntryDist<DIM, T>,
        std::vector<PhEntryDist<DIM, T>>,
        ComparePhEntryDistByDistance<DIM, T>>
        queue_;
    int num_found_results_;
    int num_requested_results_;
    DISTANCE distance_;
};

}  // namespace improbable::phtree::v16

#endif  // PHTREE_V16_PH_QUERY_KNN_HS_H
