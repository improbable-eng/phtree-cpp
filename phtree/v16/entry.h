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

#ifndef PHTREE_V16_ENTRY_H
#define PHTREE_V16_ENTRY_H

#include "../../phtree/common/common.h"
#include "node.h"
#include <cassert>
#include <cstdint>
#include <memory>
#include <string>
#include <variant>

namespace improbable::phtree::v16 {

template <dimension_t DIM, typename T, typename SCALAR>
class Node;

/*
 * Nodes in the PH-Tree contain up to 2^DIM PhEntries, one in each geometric quadrant.
 * PhEntries can contain two types of data:
 * - A key/value pair (value of type T)
 * - A prefix/child-node pair, where prefix is the prefix of the child node and the
 *   child node is contained in a unique_ptr.
 */
template <dimension_t DIM, typename T, typename SCALAR>
class Entry {
    using KeyT = PhPoint<DIM, SCALAR>;
    using ValueT = std::remove_const_t<T>;
    using NodeT = Node<DIM, T, SCALAR>;

  public:
    Entry() : kd_key_(), value_{std::in_place_type<ValueT>, T{}} {}

    /*
     * Construct entry with existing node.
     */
    Entry(const KeyT& k, std::unique_ptr<NodeT>&& node)
    : kd_key_{k}
    , value_{
          std::in_place_type<std::unique_ptr<NodeT>>, std::forward<std::unique_ptr<NodeT>>(node)} {}

    /*
     * Construct entry with a new node.
     */
    Entry(bit_width_t infix_len, bit_width_t postfix_len)
    : kd_key_()
    , value_{
          std::in_place_type<std::unique_ptr<NodeT>>,
          std::make_unique<NodeT>(infix_len, postfix_len)} {}

    /*
     * Construct entry with new T or moved T.
     */
    template <typename... _Args>
    explicit Entry(const KeyT& k, _Args&&... __args)
    : kd_key_{k}, value_{std::in_place_type<ValueT>, std::forward<_Args>(__args)...} {}

    [[nodiscard]] const KeyT& GetKey() const {
        return kd_key_;
    }

    [[nodiscard]] bool IsValue() const {
        return std::holds_alternative<ValueT>(value_);
    }

    [[nodiscard]] bool IsNode() const {
        return std::holds_alternative<std::unique_ptr<NodeT>>(value_);
    }

    [[nodiscard]] T& GetValue() const {
        assert(IsValue());
        return const_cast<T&>(std::get<ValueT>(value_));
    }

    [[nodiscard]] NodeT& GetNode() const {
        assert(IsNode());
        return *std::get<std::unique_ptr<NodeT>>(value_);
    }

    void ReplaceNodeWithDataFromEntry(Entry&& other) {
        assert(IsNode());
        kd_key_ = other.GetKey();

        // 'value_' points indirectly to 'entry' so we have to remove `entity's` content before
        // assigning anything to `value_` here. Otherwise the assignment would destruct the previous
        // content and, by reachability, `entity's` content.
        auto old_node = std::get<std::unique_ptr<NodeT>>(value_).release();
        value_ = std::move(other.value_);
        delete old_node;
    }

  private:
    KeyT kd_key_;
    std::variant<ValueT, std::unique_ptr<NodeT>> value_;
};
}  // namespace improbable::phtree::v16

#endif  // PHTREE_V16_ENTRY_H
