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

#ifndef PHTREE_V16_PHTREEV16_H
#define PHTREE_V16_PHTREEV16_H

#include "debug_helper.h"
#include "node.h"
#include "ph_iterator_full.h"
#include "ph_iterator_hc.h"
#include "ph_iterator_knn_hs.h"
#include "ph_iterator_simple.h"

namespace improbable::phtree::v16 {

/*
 * The PH-Tree is an ordered index on an n-dimensional space (quad-/oct-/2^n-tree) where each
 * dimension is (by default) indexed by a 64 bit integer. The index ordered follows z-order / Morton
 * order. The index is effectively a 'map', i.e. each key is associated with at most one value.
 *
 * Keys are points in n-dimensional space.
 *
 * This API behaves similar to std::map, see function descriptions for details.
 *
 * Loosely based on PH-Tree Java, V16, see http://www.phtree.org
 *
 * See also :
 * - T. Zaeschke, C. Zimmerli, M.C. Norrie:
 *   "The PH-Tree -- A Space-Efficient Storage Structure and Multi-Dimensional Index", (SIGMOD 2014)
 * - T. Zaeschke: "The PH-Tree Revisited", (2015)
 * - T. Zaeschke, M.C. Norrie: "Efficient Z-Ordered Traversal of Hypercube Indexes" (BTW 2017).
 *
 * @tparam T   Value type.
 * @tparam DIM Dimensionality. This is the number of dimensions of the space to index.
 */
template <
    dimension_t DIM,
    typename T,
    typename KEY = PhPoint<DIM>,
    PhPostprocessor<DIM, KEY> POST = PrePostNoOp<DIM>>
class PhTreeV16 {
    friend PhTreeDebugHelper;

  public:
    static_assert(!std::is_reference<T>::value, "Reference type value are not supported.");

    PhTreeV16() : num_entries_{0}, root_{0, MAX_BIT_WIDTH - 1} {}

    /*
     *  Attempts to build and insert a key and a value into the tree.
     *
     *  @param key The key for the new entry.
     *
     *  @param __args  Arguments used to generate a new value.
     *
     *  @return  A pair, whose first element points  to the possibly inserted pair,
     *           and whose second element is a bool that is true if the pair was actually inserted.
     *
     * This function attempts to build and insert a (key, value) pair into the tree. The PH-Tree is
     * effectively a map, so if an entry with the same key was already in the tree, returns that
     * entry instead of inserting a new one.
     */
    template <typename... _Args>
    std::pair<T&, bool> emplace(const PhPoint<DIM>& key, _Args&&... __args) {
        auto* current_entry = &root_;
        bool is_inserted = false;
        while (current_entry->IsNode()) {
            current_entry =
                current_entry->GetNode().Emplace(is_inserted, key, std::forward<_Args>(__args)...);
        }
        num_entries_ += is_inserted;
        return {current_entry->GetValue(), is_inserted};
    }

    /*
     * See std::map::insert().
     *
     * @return a pair consisting of the inserted element (or to the element that prevented the
     * insertion) and a bool denoting whether the insertion took place.
     */
    std::pair<T&, bool> insert(const PhPoint<DIM>& key, const T& value) {
        return emplace(key, value);
    }

    /*
     * @return the value stored at position 'key'. If no such value exists, one is added to the tree
     * and returned.
     */
    T& operator[](const PhPoint<DIM>& key) {
        return emplace(key).first;
    }

    /*
     * Analogous to map:count().
     *
     * @return '1', if a value is associated with the provided key, otherwise '0'.
     */
    size_t count(const PhPoint<DIM>& key) const {
        if (empty()) {
            return 0;
        }
        auto* current_entry = &root_;
        while (current_entry && current_entry->IsNode()) {
            current_entry = current_entry->GetNode().Find(key);
        }
        return current_entry ? 1 : 0;
    }

    /*
     * Analogous to map:find().
     *
     * Get an entry associated with a k dimensional key.
     * @param key the key to look up
     * @return an iterator that points either to the associated value or to {@code end()} if the key
     * was found
     */
    PhIteratorSimple<DIM, T, KEY, POST> find(const PhPoint<DIM>& key) const {
        if (empty()) {
            return {};
        }

        auto* current_entry = &root_;
        while (current_entry && current_entry->IsNode()) {
            current_entry = current_entry->GetNode().Find(key);
        }

        if (current_entry) {
            return PhIteratorSimple<DIM, T, KEY, POST>(current_entry);
        }
        return {};
    }

    /*
     * See std::map::erase(). Removes any value associated with the provided key.
     *
     * @return '1' if a value was found, otherwise '0'.
     */
    size_t erase(const PhPoint<DIM>& key) {
        auto* current_node = &root_.GetNode();
        Node<DIM, T>* parent_node = nullptr;
        bool found = false;
        while (current_node) {
            auto* child_node = current_node->Erase(key, parent_node, found);
            parent_node = current_node;
            current_node = child_node;
        }
        num_entries_ -= found;
        return found;
    }

    /*
     * Iterates over all entries in the tree. The optional filter allows filtering entries and nodes
     * (=sub-trees) before returning / traversing them. By default all entries are returned. Filter
     * functions must implement the same signature as the default 'PhFilterNoOp'.
     *
     * @return an iterator over all (filtered) entries in the tree,
     */
    template <typename FILTER = PhFilterNoOp<DIM, T>>
    auto begin(FILTER filter = FILTER()) const {
        return PhIteratorFull<DIM, T, KEY, POST, FILTER>(root_, filter);
    }

    /*
     * Performs a rectangular window query. The parameters are the min and max keys which
     * contain the minimum respectively the maximum keys in every dimension.
     * @param min Minimum values
     * @param max Maximum values
     * @param filter An optional filter function. The filter function allows filtering entries and
     * sub-nodes before they are returned or traversed. Any filter function must follow the
     * signature of the default 'PhFilterNoOp`.
     * @return Result iterator.
     */
    template <typename FILTER = PhFilterNoOp<DIM, T>>
    auto begin_query(
        const PhPoint<DIM>& min, const PhPoint<DIM>& max, FILTER filter = FILTER()) const {
        return PhIteratorHC<DIM, T, KEY, POST, FILTER>(root_, min, max, filter);
    }

    /*
     * Locate nearest neighbors for a given point in space.
     * @param min_results number of entries to be returned. More entries may or may not be returned
     * when several entries have the same distance.
     * @param center center point
     * @param distance_function optional distance function, defaults to euclidean distance
     * @param filter optional filter predicate that excludes nodes/entries before their distance is
     * calculated.
     * @return Result iterator.
     */
    template <
        typename DISTANCE = PhDistanceLongEuclidean<DIM>,
        typename FILTER = PhFilterNoOp<DIM, T>>
    auto begin_knn_query(
        size_t min_results,
        const PhPoint<DIM>& center,
        DISTANCE distance_function = DISTANCE(),
        FILTER filter = FILTER()) const {
        return PhIteratorKnnHS<DIM, T, KEY, POST, DISTANCE, FILTER>(
            root_, min_results, center, distance_function, filter);
    }

    /*
     * @return An iterator representing the tree's 'end'.
     */
    const auto& end() const {
        return the_end_;
    }

    /*
     * Remove all entries from the tree.
     */
    void clear() {
        num_entries_ = 0;
        root_ = PhEntry<DIM, T>(0, MAX_BIT_WIDTH - 1);
    }

    /*
     * @return the number of entries (key/value pairs) in the tree.
     */
    [[nodiscard]] size_t size() const {
        return num_entries_;
    }

    /*
     * @return 'true' if the tree is empty, otherwise 'false'.
     */
    [[nodiscard]] bool empty() const {
        return num_entries_ == 0;
    }

  private:
    /*
     * This function is only for debugging.
     */
    auto GetDebugHelper() const {
        return DebugHelperV16(root_.GetNode(), num_entries_);
    }

  private:
    size_t num_entries_;
    // Contract: root_ contains a Node with 0 or more entries (the root node is the only Node
    // that is allowed to have less than two entries.
    PhEntry<DIM, T> root_;
    PhIteratorEnd<DIM, T, KEY, POST> the_end_;
};

}  // namespace improbable::phtree::v16

#endif  // PHTREE_V16_PHTREEV16_H
