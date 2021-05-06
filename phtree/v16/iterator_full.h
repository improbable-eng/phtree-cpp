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

#ifndef PHTREE_V16_ITERATOR_FULL_H
#define PHTREE_V16_ITERATOR_FULL_H

#include "../common/common.h"
#include "iterator_base.h"

namespace improbable::phtree::v16 {

template <dimension_t DIM, typename T, typename SCALAR>
class Node;

template <typename T, typename CONVERT, typename FILTER>
class IteratorFull : public IteratorBase<T, CONVERT, FILTER> {
    static constexpr dimension_t DIM = CONVERT::DimInternal;
    using SCALAR = typename CONVERT::ScalarInternal;
    using NodeT = Node<DIM, T, SCALAR>;
    using EntryT = typename IteratorBase<T, CONVERT, FILTER>::EntryT;

  public:
    IteratorFull(const EntryT& root, const CONVERT& converter, FILTER filter)
    : IteratorBase<T, CONVERT, FILTER>(converter, filter), stack_{}, stack_size_{0} {
        PrepareAndPush(root.GetNode());
        FindNextElement();
    }

    IteratorFull& operator++() {
        FindNextElement();
        return *this;
    }

    IteratorFull operator++(int) {
        IteratorFull iterator(*this);
        ++(*this);
        return iterator;
    }

  private:
    void FindNextElement() {
        while (!IsEmpty()) {
            auto* p = &Peek();
            while (*p != PeekEnd()) {
                auto& candidate = (*p)->second;
                ++(*p);
                if (this->ApplyFilter(candidate)) {
                    if (candidate.IsNode()) {
                        p = &PrepareAndPush(candidate.GetNode());
                    } else {
                        this->SetCurrentResult(&candidate);
                        return;
                    }
                }
            }
            // return to parent node
            Pop();
        }
        // finished
        this->SetFinished();
    }

    auto& PrepareAndPush(const NodeT& node) {
        assert(stack_size_ < stack_.size() - 1);
        // No '&'  because this is a temp value
        stack_[stack_size_].first = node.Entries().cbegin();
        stack_[stack_size_].second = node.Entries().end();
        ++stack_size_;
        return stack_[stack_size_ - 1].first;
    }

    auto& Peek() {
        assert(stack_size_ > 0);
        return stack_[stack_size_ - 1].first;
    }

    auto& PeekEnd() {
        assert(stack_size_ > 0);
        return stack_[stack_size_ - 1].second;
    }

    auto& Pop() {
        assert(stack_size_ > 0);
        return stack_[--stack_size_].first;
    }

    bool IsEmpty() {
        return stack_size_ == 0;
    }

    std::array<
        std::pair<EntryIteratorC<DIM, EntryT>, EntryIteratorC<DIM, EntryT>>,
        MAX_BIT_WIDTH<SCALAR>>
        stack_;
    size_t stack_size_;
};

}  // namespace improbable::phtree::v16

#endif  // PHTREE_V16_ITERATOR_FULL_H
