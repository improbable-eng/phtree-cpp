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
#include <memory>
#include <optional>

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
    /*
     * Construct entry with existing node.
     */
    Entry(const KeyT& k, std::unique_ptr<NodeT>&& node_ptr)
    : kd_key_{k}, node_{std::move(node_ptr)}, value_{std::nullopt} {}

    /*
     * Construct entry with a new node.
     */
    Entry(bit_width_t infix_len, bit_width_t postfix_len)
    : kd_key_(), node_{std::make_unique<NodeT>(infix_len, postfix_len)}, value_{std::nullopt} {}

    /*
     * Construct entry with existing T.
     */
    Entry(const KeyT& k, std::optional<ValueT>&& value)
    : kd_key_{k}, node_{nullptr}, value_{std::move(value)} {}

    /*
     * Construct entry with new T or moved T.
     */
    template <typename... Args>
    explicit Entry(const KeyT& k, Args&&... args)
    : kd_key_{k}, node_{nullptr}, value_{std::in_place, std::forward<Args>(args)...} {}

    [[nodiscard]] const KeyT& GetKey() const {
        return kd_key_;
    }

    [[nodiscard]] bool IsValue() const {
        return value_.has_value();
    }

    [[nodiscard]] bool IsNode() const {
        return node_.get() != nullptr;
    }

    [[nodiscard]] T& GetValue() const {
        assert(IsValue());
        return const_cast<T&>(*value_);
    }

    [[nodiscard]] NodeT& GetNode() const {
        assert(IsNode());
        return *node_;
    }

    void SetNode(std::unique_ptr<NodeT>&& node) {
        assert(!IsNode());
        node_ = std::move(node);
        value_.reset();
    }

    [[nodiscard]] std::optional<ValueT>&& ExtractValue() {
        assert(IsValue());
        return std::move(value_);
    }

    [[nodiscard]] std::unique_ptr<NodeT>&& ExtractNode() {
        assert(IsNode());
        return std::move(node_);
    }

    void ReplaceNodeWithDataFromEntry(Entry&& other) {
        assert(IsNode());
        kd_key_ = other.GetKey();

        if (other.IsNode()) {
            node_ = std::move(other.node_);
        } else {
            value_ = std::move(other.value_);
            node_.reset();
        }
    }

  private:
    KeyT kd_key_;
    std::unique_ptr<NodeT> node_;
    std::optional<ValueT> value_;
};
}  // namespace improbable::phtree::v16

#endif  // PHTREE_V16_ENTRY_H
