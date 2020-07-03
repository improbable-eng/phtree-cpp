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

#ifndef PHTREE_V16_PH_ITERATOR_BASE_H
#define PHTREE_V16_PH_ITERATOR_BASE_H

#include "../common/ph_common.h"
#include "ph_entry.h"

namespace improbable::phtree::v16 {

/*
 * Base class for all PH-Tree iterators.
 */
template <
    dimension_t DIM,
    typename T,
    typename KEY,
    PhPostprocessor<DIM, KEY> POST,
    typename FILTER = PhFilterNoOp<DIM, T>>
class PhIteratorBase {
  public:
    PhIteratorBase() : current_result_{nullptr}, is_finished_{false}, filter_{FILTER()} {}

    explicit PhIteratorBase(FILTER filter)
    : current_result_{nullptr}, is_finished_{false}, filter_(std::move(filter)) {}

    T& operator*() const {
        assert(current_result_);
        return current_result_->GetValue();
    }

    T* operator->() const {
        assert(current_result_);
        return &current_result_->GetValue();
    }

    template <typename FILTER_2>
    friend bool operator==(
        const PhIteratorBase<DIM, T, KEY, POST, FILTER>& left,
        const PhIteratorBase<DIM, T, KEY, POST, FILTER_2>& right) {
        // Note: The following compares pointers to PhEntry objects so it should be
        // a) fast (i.e. not comparing contents of entries)
        // b) return `false` when comparing apparently identical entries from different PH-Trees (as
        // intended)
        return (left.is_finished_ && right.Finished()) ||
            (!left.is_finished_ && !right.Finished() &&
             left.current_result_ == right.GetCurrentResult());
    }

    template <typename FILTER_2>
    friend bool operator!=(
        const PhIteratorBase<DIM, T, KEY, POST, FILTER>& left,
        const PhIteratorBase<DIM, T, KEY, POST, FILTER_2>& right) {
        return !(left == right);
    }

    KEY first() const {
        return POST(current_result_->GetKey());
    }

    T& second() const {
        return current_result_->GetValue();
    }

    [[nodiscard]] bool Finished() const {
        return is_finished_;
    }

    const PhEntry<DIM, T>* GetCurrentResult() const {
        return current_result_;
    }

  protected:
    void SetFinished() {
        is_finished_ = true;
        current_result_ = nullptr;
    }

    [[nodiscard]] bool ApplyFilter(const PhEntry<DIM, T>& entry) const {
        return entry.IsNode()
            ? filter_.IsNodeValid(entry.GetKey(), entry.GetNode().GetPostfixLen() + 1)
            : filter_.IsEntryValid(entry.GetKey(), entry.GetValue());
    }

    void SetCurrentResult(const PhEntry<DIM, T>* current_result) {
        current_result_ = current_result;
    }

  private:
    const PhEntry<DIM, T>* current_result_;
    bool is_finished_;
    FILTER filter_;
};

}  // namespace improbable::phtree::v16

#endif  // PHTREE_V16_PH_ITERATOR_BASE_H
