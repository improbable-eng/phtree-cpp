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

#ifndef PHTREE_V16_PH_ITERATOR_FULL_H
#define PHTREE_V16_PH_ITERATOR_FULL_H

#include "../common/ph_common.h"
#include "ph_iterator_base.h"

namespace improbable::phtree::v16 {

template <dimension_t DIM, typename T>
class Node;

template <dimension_t DIM, typename T>
class PhEntry;

template <
    dimension_t DIM,
    typename T,
    typename KEY,
    PhPostprocessor<DIM, KEY> POST,
    typename FILTER>
class PhIteratorFull : public PhIteratorBase<DIM, T, KEY, POST, FILTER> {
  public:
    PhIteratorFull(const PhEntry<DIM, T>& root, FILTER filter)
    : PhIteratorBase<DIM, T, KEY, POST, FILTER>(filter), stack_{}, stack_size_{0} {
        PrepareAndPush(root.GetNode());
        FindNextElement();
    }

    PhIteratorFull& operator++() {
        FindNextElement();
        return *this;
    }

    PhIteratorFull operator++(int) {
        PhIteratorFull iterator(*this);
        ++(*this);
        return iterator;
    }

  private:
    void FindNextElement() {
        while (!IsEmpty()) {
            EntryIteratorC<DIM, T>* p = &Peek();
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

    EntryIteratorC<DIM, T>& PrepareAndPush(const Node<DIM, T>& node) {
        assert(stack_size_ < stack_.size() - 1);
        // No '&'  because this is a temp value
        stack_[stack_size_].first = node.Entries().cbegin();
        stack_[stack_size_].second = node.Entries().end();
        ++stack_size_;
        return stack_[stack_size_ - 1].first;
    }

    EntryIteratorC<DIM, T>& Peek() {
        assert(stack_size_ > 0);
        return stack_[stack_size_ - 1].first;
    }

    EntryIteratorC<DIM, T>& PeekEnd() {
        assert(stack_size_ > 0);
        return stack_[stack_size_ - 1].second;
    }

    EntryIteratorC<DIM, T>& Pop() {
        assert(stack_size_ > 0);
        return stack_[--stack_size_].first;
    }

    bool IsEmpty() {
        return stack_size_ == 0;
    }

    std::array<std::pair<EntryIteratorC<DIM, T>, EntryIteratorC<DIM, T>>, MAX_BIT_WIDTH> stack_;
    size_t stack_size_;
};

}  // namespace improbable::phtree::v16

#endif  // PHTREE_V16_PH_ITERATOR_FULL_H
