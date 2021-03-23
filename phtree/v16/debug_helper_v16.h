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

#ifndef PHTREE_V16_DEBUG_HELPER_H
#define PHTREE_V16_DEBUG_HELPER_H

#include "node.h"
#include "../common/common.h"
#include "../common/debug_helper.h"
#include "phtree_v16.h"
#include <string>

namespace improbable::phtree::v16 {

template <dimension_t DIM, typename T, typename CONVERT>
class PhTreeV16;

template <dimension_t DIM, typename T, typename SCALAR>
class DebugHelperV16 : public PhTreeDebugHelper::DebugHelper {
    using Key = PhPoint<DIM, SCALAR>;
    using Node = Node<DIM, T, SCALAR>;
    using Entry = Entry<DIM, T, SCALAR>;

  public:
    DebugHelperV16(const Node& root, size_t size) : root_{root}, size_{size} {}

    /*
     * Depending on the detail parameter this returns:
     * - "name"    : a string that identifies the tree implementation type.
     * - "entries" : a string that lists all elements in the tree.
     * - "tree"    : a string that lists all elements in the tree, pretty formatted to indicate tree
     * structure.
     *
     * @return a string that identifies the tree implementation type.
     */
    [[nodiscard]] std::string ToString(
        const PhTreeDebugHelper::PrintDetail& detail) const override {
        using Enum = PhTreeDebugHelper::PrintDetail;
        std::ostringstream os;
        switch (detail) {
        case Enum::name:
            os << "PH-TreeV16-C++";
            break;
        case Enum::entries:
            ToStringPlain(os, root_);
            break;
        case Enum::tree:
            ToStringTree(os, 0, root_, Key(), true);
            break;
        }
        return os.str();
    }

    /*
     * Collects some statistics about the tree, such as number of nodes, average depth, ...
     *
     * @return some statistics about the tree.
     */
    [[nodiscard]] PhTreeStats GetStats() const override {
        PhTreeStats stats;
        root_.GetStats(stats);
        return stats;
    }

    /*
     * Checks the consistency of the tree. This function requires assertions to be enabled.
     */
    void CheckConsistency() const override {
        assert(size_ == root_.CheckConsistency());
    }

  private:
    void ToStringPlain(std::ostringstream& os, const Node& node) const {
        for (auto& it : node.Entries()) {
            const Entry& o = it.second;
            // inner node?
            if (o.IsNode()) {
                ToStringPlain(os, o.GetNode());
            } else {
                os << o.GetKey();
                os << "  v=" << (o.IsValue() ? "T" : "null") << std::endl;
            }
        }
    }

    void ToStringTree(
        std::ostringstream& sb,
        bit_width_t current_depth,
        const Node& node,
        const Key& prefix,
        bool printValue) const {
        std::string ind = "*";
        for (bit_width_t i = 0; i < current_depth; ++i) {
            ind += "-";
        }
        sb << ind << "il=" << node.GetInfixLen() << " pl=" << node.GetPostfixLen()
           << " ec=" << node.GetEntryCount() << " inf=[";

        // for a leaf node, the existence of a sub just indicates that the value exists.
        if (node.GetInfixLen() > 0) {
            bit_mask_t<SCALAR> mask = MAX_MASK<SCALAR> << node.GetInfixLen();
            mask = ~mask;
            mask <<= node.GetPostfixLen() + 1;
            for (dimension_t i = 0; i < DIM; ++i) {
                sb << ToBinary<SCALAR>(prefix[i] & mask) << ",";
            }
        }
        current_depth += node.GetInfixLen();
        sb << "]  "
           << "Node___il=" << node.GetInfixLen() << ";pl=" << node.GetPostfixLen()
           << ";size=" << node.Entries().size() << std::endl;

        // To clean previous postfixes.
        for (auto& it : node.Entries()) {
            const auto& o = it.second;
            hc_pos_t hcPos = it.first;
            if (o.IsNode()) {
                sb << ind << "# " << hcPos << "  Node: " << std::endl;
                ToStringTree(sb, current_depth + 1, o.GetNode(), o.GetKey(), printValue);
            } else {
                // post-fix
                sb << ind << ToBinary(o.GetKey());
                sb << "  hcPos=" << hcPos;
                if (printValue) {
                    sb << "  v=" << (o.IsValue() ? "T" : "null");
                }
                sb << std::endl;
            }
        }
    }

    const Node& root_;
    const size_t size_;
};
}  // namespace improbable::phtree::v16

#endif  // PHTREE_V16_DEBUG_HELPER_H
