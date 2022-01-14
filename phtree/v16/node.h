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

#ifndef PHTREE_V16_NODE_H
#define PHTREE_V16_NODE_H

#include "../common/common.h"
#include "../common/tree_stats.h"
#include "entry.h"
#include "phtree_v16.h"
#include <map>

namespace improbable::phtree::v16 {

/*
 * We provide different implementations of the node's internal entry set:
 * - `array_map` is the fastest, but has O(2^DIM) space complexity. This can be very wasteful
 *   because many nodes may have only 2 entries.
 *   Also, iteration depends on some bit operations and is also O(DIM) per step if the CPU/compiler
 *   does not support CTZ (count trailing bits).
 * - `sparse_map` is slower, but requires only O(n) memory (n = number of entries/children).
 *   However, insertion/deletion is O(n), i.e. O(2^DIM) time complexity in the worst case.
 * - 'std::map` is the least efficient for small node sizes but scales best with larger nodes and
 *   dimensionality. Remember that n_max = 2^DIM.
 */
template <dimension_t DIM, typename Entry>
using EntryMap = typename std::conditional<
    DIM <= 3,
    array_map<Entry, (hc_pos_t(1) << DIM)>,
    typename std::conditional<DIM <= 8, sparse_map<Entry>, std::map<hc_pos_t, Entry>>::type>::type;

template <dimension_t DIM, typename Entry>
using EntryIterator = decltype(EntryMap<DIM, Entry>().begin());
template <dimension_t DIM, typename Entry>
using EntryIteratorC = decltype(EntryMap<DIM, Entry>().cbegin());

namespace {

/*
 * Takes a construct of parent_node -> child_node, ie the child_node is owned by parent_node.
 * This function also assumes that the child_node contains only one entry.
 *
 * This function takes the remaining entry from the child node and inserts it into the parent_node
 * where it replaces (and implicitly deletes) the child_node.
 * @param prefix_of_child_in_parent This specifies the position of child_node inside the
 * parent_node. We only need the relevant bits at the level of the parent_node. This means we can
 * use any key of any node or entry that is, or used to be) inside the child_node, because they all
 * share the same prefix. This includes the key of the child_node itself.
 * @param child_node The node to be removed from the parent node.
 * @param parent_node Current owner of the child node.
 */
template <dimension_t DIM, typename T, typename SCALAR>
void MergeIntoParent(Node<DIM, T, SCALAR>& child_node, Node<DIM, T, SCALAR>& parent) {
    assert(child_node.GetEntryCount() == 1);
    // At this point we have found an entry that needs to be removed. We also know that we need to
    // remove the child node because it contains at most one other entry and it is not the root
    // node.
    auto map_entry = child_node.Entries().begin();
    auto& entry = map_entry->second;

    auto hc_pos_in_parent = CalcPosInArray(entry.GetKey(), parent.GetPostfixLen());
    auto& parent_entry = parent.Entries().find(hc_pos_in_parent)->second;

    if (entry.IsNode()) {
        // connect sub to parent
        auto& sub2 = entry.GetNode();
        bit_width_t new_infix_len = child_node.GetInfixLen() + 1 + sub2.GetInfixLen();
        sub2.SetInfixLen(new_infix_len);
    }

    // Now move the single entry into the parent, the position in the parent is the same as the
    // child_node.
    parent_entry.ReplaceNodeWithDataFromEntry(std::move(entry));
}
}  // namespace

/*
 * A node of the PH-Tree. It contains up to 2^DIM entries, each entry being either a leaf with data
 * of type T or a child node (both are of the variant type Entry).
 *
 * The keys (coordinates) of all entries of a node have the same prefix, where prefix refers to the
 * first 'n' bits of their keys. 'n' is equivalent to "n = w - GetPostLen() - 1", where 'w' is the
 * number of bits of the keys per dimension (usually w = 64 for `int64_t` or 'double').
 *
 * The entries are stored in an EntryMap indexed and ordered by their "hypercube address".
 * The hypercube address is the ID of the quadrant in the node. Nodes are effectively binary
 * hypercubes (= binary Hamming space) on {0,1}^DIM. The hypercube address thus uses one bit per
 * dimension to address all quadrants of the node's binary hypercube. Each bit designates for one
 * dimension which quadrant it refers to, such as 0=left/1=right; 0=down/1=up; 0=front/1=back; ... .
 * The ordering of the quadrants thus represents a z-order curve (please note that this completely
 * unrelated to `z-ordering` used in graphics).
 *
 * A node always has at least two entries, except for the root node which can have fewer entries.
 * None of the functions in this class are recursive, see Emplace().
 */
template <dimension_t DIM, typename T, typename SCALAR>
class Node {
    using KeyT = PhPoint<DIM, SCALAR>;
    using EntryT = Entry<DIM, T, SCALAR>;

  public:
    Node(bit_width_t infix_len, bit_width_t postfix_len)
    : postfix_len_(postfix_len), infix_len_(infix_len), entries_{} {
        assert(infix_len_ < MAX_BIT_WIDTH<SCALAR>);
        assert(infix_len >= 0);
    }

    // Nodes should never be copied!
    Node(const Node&) = delete;
    Node(Node&&) = delete;
    Node& operator=(const Node&) = delete;
    Node& operator=(Node&&) = delete;

    [[nodiscard]] auto GetEntryCount() const {
        return entries_.size();
    }

    [[nodiscard]] bit_width_t GetInfixLen() const {
        return infix_len_;
    }

    [[nodiscard]] bit_width_t GetPostfixLen() const {
        return postfix_len_;
    }

    /*
     * Attempts to emplace an entry in this node.
     * The behavior is analogous to std::map::emplace(), i.e. if there is already a value with the
     * given hypercube address 'hc_pos', that value is returned. This function is also
     * non-recursive, it will return a child node instead of traversing it.
     *
     * The scenarios in detail:
     *
     * If there is no entry at the position of 'hc_pos', a new entry is inserted. The new entry is
     * constructed from a constructor of T that accepts the arguments __args. Also, 'is_inserted' is
     * set top 'true'.
     *
     * If there is a is a entry with a value T at 'hc_pos', that value is returned. The value is
     * _not_ overwritten.
     *
     * If there is a child node at the position of 'hc_pos', the child node's prefix is is analysed.
     * If the prefix indicates that the new value would end up inside the child node or any of its
     * children, then the child node is returned for further traversal.
     * If the child nodes' prefix is different, then a new node is created. The new node contains
     * the child node and a new key/value entry constructed with __args. The new node is inserted in
     * the current node at the position of the former child node. The new value is returned and
     * 'is_inserted' is set to 'true'.
     *
     * @param is_inserted The function will set this to true if a new value was inserted
     * @param key The key for which a new value should be inserted.
     * @param __args Constructor arguments for creating a value T that can be inserted for the key.
     */
    template <typename... _Args>
    EntryT* Emplace(bool& is_inserted, const KeyT& key, _Args&&... __args) {
        hc_pos_t hc_pos = CalcPosInArray(key, GetPostfixLen());
        auto emplace_result = entries_.try_emplace(hc_pos, key, std::forward<_Args>(__args)...);
        auto& entry = emplace_result.first->second;
        // Return if emplace succeed, i.e. there was no entry.
        if (emplace_result.second) {
            is_inserted = true;
            return &entry;
        }
        return HandleCollision(entry, is_inserted, key, std::forward<_Args>(__args)...);
    }

    /*
     * Returns the value (T or Node) if the entry exists and matches the key. Child nodes are
     * _not_ traversed.
     * @param key The key of the entry
     * @param parent parent node
     * @return The sub node or null.
     */
    const EntryT* Find(const KeyT& key) const {
        hc_pos_t hc_pos = CalcPosInArray(key, GetPostfixLen());
        const auto& entry = entries_.find(hc_pos);
        if (entry != entries_.end() && DoesEntryMatch(entry->second, key)) {
            return &entry->second;
        }
        return nullptr;
    }

    /*
     * Attempts to erase a key/value pair.
     * This function is not recursive, if the 'key' leads to a child node, the child node
     * is returned and nothing is removed.
     *
     * @param key The key of the key/value pair to be erased
     * @param parent The parent node of the current node (=nullptr) if this is the root node.
     * @param found This is and output parameter and will be set to 'true' if a value was removed.
     * @return A child node if the provided key leads to a child node.
     */
    Node* Erase(const KeyT& key, Node* parent, bool& found) {
        hc_pos_t hc_pos = CalcPosInArray(key, GetPostfixLen());
        auto it = entries_.find(hc_pos);
        if (it != entries_.end() && DoesEntryMatch(it->second, key)) {
            if (it->second.IsNode()) {
                return &it->second.GetNode();
            }
            entries_.erase(it);

            found = true;
            if (parent && GetEntryCount() == 1) {
                MergeIntoParent(*this, *parent);
                // WARNING: (this) is deleted here, do not refer to it beyond this point.
            }
        }
        return nullptr;
    }

    auto& Entries() {
        return entries_;
    }

    const auto& Entries() const {
        return entries_;
    }

    void GetStats(PhTreeStats& stats, bit_width_t current_depth = 0) const {
        size_t num_children = entries_.size();

        ++stats.n_nodes_;
        ++stats.infix_hist_[GetInfixLen()];
        ++stats.node_depth_hist_[current_depth];
        ++stats.node_size_log_hist_[32 - CountLeadingZeros(std::uint32_t(num_children))];
        stats.n_total_children_ += num_children;

        current_depth += GetInfixLen();
        stats.q_total_depth_ += current_depth;

        for (auto& entry : entries_) {
            auto& child = entry.second;
            if (child.IsNode()) {
                auto& sub = child.GetNode();
                sub.GetStats(stats, current_depth + 1);
            } else {
                ++stats.q_n_post_fix_n_[current_depth];
                ++stats.size_;
            }
        }
    }

    size_t CheckConsistency(bit_width_t current_depth = 0) const {
        // Except for a root node if the tree has <2 entries.
        assert(entries_.size() >= 2 || current_depth == 0);

        current_depth += GetInfixLen();
        size_t num_entries_local = 0;
        size_t num_entries_children = 0;
        for (auto& entry : entries_) {
            auto& child = entry.second;
            if (child.IsNode()) {
                auto& sub = child.GetNode();
                // Check node consistency
                assert(sub.GetInfixLen() + 1 + sub.GetPostfixLen() == GetPostfixLen());
                num_entries_children += sub.CheckConsistency(current_depth + 1);
            } else {
                ++num_entries_local;
            }
        }
        return num_entries_local + num_entries_children;
    }

    void SetInfixLen(bit_width_t newInfLen) {
        assert(newInfLen < MAX_BIT_WIDTH<SCALAR>);
        assert(newInfLen >= 0);
        infix_len_ = newInfLen;
    }

  private:
    template <typename... _Args>
    auto& WriteValue(hc_pos_t hc_pos, const KeyT& new_key, _Args&&... __args) {
        return entries_.try_emplace(hc_pos, new_key, std::forward<_Args>(__args)...).first->second;
    }

    void WriteEntry(hc_pos_t hc_pos, EntryT&& entry) {
        if (entry.IsNode()) {
            auto& node = entry.GetNode();
            bit_width_t new_subnode_infix_len = postfix_len_ - node.postfix_len_ - 1;
            node.SetInfixLen(new_subnode_infix_len);
        }
        entries_.try_emplace(hc_pos, std::move(entry));
    }

    /*
     * Handles the case where we want to insert a new entry into a node but the node already
     * has an entry in that position.
     * @param existing_entry The current entry in the node
     * @param is_inserted Output: This will be set to 'true' by this function if a new entry was
     * inserted by this function.
     * @param new_key The key of the entry to be inserted
     * @param __args The constructor arguments for a new value T of a the new entry to be inserted
     * @return A Entry that may contain a child node, a newly created entry or an existing entry.
     * A child node indicates that no entry was inserted, but the caller should try inserting into
     * the child node. A newly created entry (indicated by is_inserted=true) indicates successful
     * insertion. An existing entry (indicated by is_inserted=false) indicates that there is already
     * an entry with the exact same key as new_key, so insertion has failed.
     */
    template <typename... _Args>
    auto* HandleCollision(
        EntryT& existing_entry, bool& is_inserted, const KeyT& new_key, _Args&&... __args) {
        assert(!is_inserted);
        // We have two entries in the same location (local pos).
        // Now we need to compare the keys.
        // If they are identical, we simply return the entry for further traversal.
        if (existing_entry.IsNode()) {
            auto& sub_node = existing_entry.GetNode();
            if (sub_node.GetInfixLen() > 0) {
                bit_width_t max_conflicting_bits =
                    NumberOfDivergingBits(new_key, existing_entry.GetKey());
                if (max_conflicting_bits > sub_node.GetPostfixLen() + 1) {
                    is_inserted = true;
                    return InsertSplit(
                        existing_entry,
                        new_key,
                        max_conflicting_bits,
                        std::forward<_Args>(__args)...);
                }
            }
            // No infix conflict, just traverse subnode
        } else {
            bit_width_t max_conflicting_bits =
                NumberOfDivergingBits(new_key, existing_entry.GetKey());
            if (max_conflicting_bits > 0) {
                is_inserted = true;
                return InsertSplit(
                    existing_entry, new_key, max_conflicting_bits, std::forward<_Args>(__args)...);
            }
            // perfect match -> return existing
        }
        return &existing_entry;
    }

    template <typename... _Args>
    auto* InsertSplit(
        EntryT& current_entry,
        const KeyT& new_key,
        bit_width_t max_conflicting_bits,
        _Args&&... __args) {
        const auto current_key = current_entry.GetKey();

        // determine length of infix
        bit_width_t new_local_infix_len = GetPostfixLen() - max_conflicting_bits;
        bit_width_t new_postfix_len = max_conflicting_bits - 1;
        auto new_sub_node = std::make_unique<Node>(new_local_infix_len, new_postfix_len);
        hc_pos_t pos_sub_1 = CalcPosInArray(new_key, new_postfix_len);
        hc_pos_t pos_sub_2 = CalcPosInArray(current_key, new_postfix_len);

        // Move key/value into subnode
        new_sub_node->WriteEntry(pos_sub_2, std::move(current_entry));
        auto& new_entry =
            new_sub_node->WriteValue(pos_sub_1, new_key, std::forward<_Args>(__args)...);

        // Insert new node into local node
        // We use new_key because current_key has been moved().
        // TODO avoid reassigning the key here, this is unnecessary.
        current_entry = {new_key, std::move(new_sub_node)};
        return &new_entry;
    }

    /*
     * Checks whether an entry's key matches another key. For Non-node entries this simply means
     * comparing the two keys. For entries that contain nodes, we only compare the prefix.
     * @param entry An entry
     * @param key A key
     * @return 'true' iff the relevant part of the key matches (prefix for nodes, whole key for
     * other entries).
     */
    bool DoesEntryMatch(const EntryT& entry, const KeyT& key) const {
        if (entry.IsNode()) {
            const auto& sub = entry.GetNode();
            if (sub.GetInfixLen() > 0) {
                const bit_mask_t<SCALAR> mask = MAX_MASK<SCALAR> << (sub.GetPostfixLen() + 1);
                return KeyEquals(entry.GetKey(), key, mask);
            }
            return true;
        }
        return entry.GetKey() == key;
    }

    // The length (number of bits) of post fixes (the part of the coordinate that is 'below' the
    // current node). If a variable prefix_len would refer to the number of bits in this node's
    // prefix, and if we assume 64 bit values, the following would always hold:
    // prefix_len + 1 + postfix_len = 64.
    // The '+1' accounts for the 1 bit that is represented by the local node's hypercube,
    // ie. the same bit that is used to create the lookup keys in entries_.
    bit_width_t postfix_len_;
    // The number of bits between this node and the parent node. For 64bit keys possible values
    // range from 0 to 62.
    bit_width_t infix_len_;
    EntryMap<DIM, EntryT> entries_;
};

}  // namespace improbable::phtree::v16
#endif  // PHTREE_V16_NODE_H
