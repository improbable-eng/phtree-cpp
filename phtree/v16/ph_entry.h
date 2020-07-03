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

#ifndef PHTREE_V16_PH_ENTRY_H
#define PHTREE_V16_PH_ENTRY_H

#include "../common/ph_common.h"
#include "node.h"
#include <cassert>
#include <cstdint>
#include <memory>
#include <string>
#include <variant>

namespace improbable::phtree::v16 {

template <dimension_t DIM, typename T>
class Node;

/*
 * Nodes in the PH-Tree contain up to 2^DIM PhEntries, one in each geometric quadrant.
 * PhEntries can contain two types of data:
 * - A key/value pair (value of type T)
 * - A prefix/child-node pair, where prefix is the prefix of the child node and the
 *   child node is contained in a unique_ptr.
 */
template <dimension_t DIM, typename T>
class PhEntry {
    using Value = std::remove_const_t<T>;

  public:
    PhEntry() : kd_key_(), value_{std::in_place_type<Value>, T{}} {}

    /*
     * Construct entry with existing node.
     */
    PhEntry(const PhPoint<DIM>& k, std::unique_ptr<Node<DIM, T>>&& node)
    : kd_key_{k}
    , value_{std::in_place_type<std::unique_ptr<Node<DIM, T>>>,
             std::forward<std::unique_ptr<Node<DIM, T>>>(node)} {}

    /*
     * Construct entry with a new node.
     */
    PhEntry(bit_width_t infix_len, bit_width_t postfix_len)
    : kd_key_()
    , value_{std::in_place_type<std::unique_ptr<Node<DIM, T>>>,
             std::make_unique<Node<DIM, T>>(infix_len, postfix_len)} {}

    /*
     * Construct entry with existing value T.
     */
    PhEntry(const PhPoint<DIM>& k, const T& v) : kd_key_{k}, value_{std::in_place_type<Value>, v} {}

    /*
     * Construct entry with new T value.
     */
    template <typename... _Args>
    explicit PhEntry(const PhPoint<DIM>& k, _Args&&... __args)
    : kd_key_{k}, value_{std::in_place_type<Value>, std::forward<_Args>(__args)...} {}

    [[nodiscard]] const PhPoint<DIM>& GetKey() const {
        return kd_key_;
    }

    [[nodiscard]] bool IsValue() const {
        return std::holds_alternative<Value>(value_);
    }

    [[nodiscard]] bool IsNode() const {
        return std::holds_alternative<std::unique_ptr<Node<DIM, T>>>(value_);
    }

    [[nodiscard]] T& GetValue() const {
        assert(IsValue());
        return const_cast<T&>(std::get<Value>(value_));
    }

    [[nodiscard]] Node<DIM, T>& GetNode() const {
        assert(IsNode());
        return *std::get<std::unique_ptr<Node<DIM, T>>>(value_);
    }

  private:
    PhPoint<DIM> kd_key_;
    std::variant<Value, std::unique_ptr<Node<DIM, T>>> value_;
};
}  // namespace improbable::phtree::v16

#endif  // PHTREE_V16_PH_ENTRY_H
