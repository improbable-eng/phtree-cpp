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

#ifndef PHTREE_V16_PH_ITERATOR_SIMPLE_H
#define PHTREE_V16_PH_ITERATOR_SIMPLE_H

#include "../common/ph_common.h"
#include "ph_iterator_base.h"

namespace improbable::phtree::v16 {

template <dimension_t DIM, typename T, typename KEY, PhPostprocessor<DIM, KEY> POST>
class PhTreeV16;

template <dimension_t DIM, typename T>
class PhEntry;

template <dimension_t DIM, typename T, typename KEY, PhPostprocessor<DIM, KEY> POST>
class PhIteratorSimple : public PhIteratorBase<DIM, T, KEY, POST> {
  public:
    PhIteratorSimple() : PhIteratorBase<DIM, T, KEY, POST>() {
        this->SetFinished();
    }

    explicit PhIteratorSimple(const PhEntry<DIM, T>* e) : PhIteratorBase<DIM, T, KEY, POST>() {
        this->SetCurrentResult(e);
    }

    PhIteratorSimple& operator++() {
        FindNextElement();
        return *this;
    }

    PhIteratorSimple operator++(int) {
        PhIteratorSimple iterator(*this);
        ++(*this);
        return iterator;
    }

  protected:
    void FindNextElement() {
        this->SetFinished();
    }
};

template <dimension_t DIM, typename T, typename KEY, PhPostprocessor<DIM, KEY> POST>
using PhIteratorEnd = PhIteratorSimple<DIM, T, KEY, POST>;

}  // namespace improbable::phtree::v16

#endif  // PHTREE_V16_PH_ITERATOR_SIMPLE_H
