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

#ifndef PHTREE_V16_FOR_EACH_H
#define PHTREE_V16_FOR_EACH_H

#include "iterator_simple.h"
#include "../common/common.h"

namespace improbable::phtree::v16 {

/*
 * Iterates over the whole tree. Entries and child nodes that are rejected by the Filter are not
 * traversed or returned.
 */
template <typename T, typename CONVERT, typename CALLBACK_FN, typename FILTER>
class ForEach {
    static constexpr dimension_t DIM = CONVERT::DimInternal;
    using KeyExternal = typename CONVERT::KeyExternal;
    using KeyInternal = typename CONVERT::KeyInternal;
    using SCALAR = typename CONVERT::ScalarInternal;
    using Entry = Entry<DIM, T, SCALAR>;
    using Node = Node<DIM, T, SCALAR>;

  public:
    ForEach(const CONVERT& converter, CALLBACK_FN& callback, FILTER filter)
    : converter_{converter}, callback_{callback}, filter_(std::move(filter)) {}

    void run(const Entry& root) {
        assert(root.IsNode());
        TraverseNode(root.GetKey(), root.GetNode());
    }

  private:
    void TraverseNode(const KeyInternal& key, const Node& node) {
        auto iter = node.Entries().begin();
        auto end = node.Entries().end();
        for (; iter != end; ++iter) {
            const auto& child = iter->second;
            const auto& child_key = child.GetKey();
            if (child.IsNode()) {
                const auto& child_node = child.GetNode();
                if (filter_.IsNodeValid(key, node.GetPostfixLen() + 1)) {
                    TraverseNode(child_key, child_node);
                }
            } else {
                T& value = child.GetValue();
                if (filter_.IsEntryValid(key, value)) {
                    callback_(converter_.post(child_key), value);
                }
            }
        }
    }

    CONVERT converter_;
    CALLBACK_FN& callback_;
    FILTER filter_;
};
}  // namespace improbable::phtree::v16

#endif  // PHTREE_V16_FOR_EACH_H
