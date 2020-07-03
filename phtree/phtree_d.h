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

#ifndef PHTREE_PHTREE_D_H
#define PHTREE_PHTREE_D_H

#include "common/ph_common.h"
#include "v16/phtree_v16.h"

namespace improbable::phtree {

/*
 * Floating-point `double` version of the PH-Tree.
 * This version of the tree accepts multi-dimensional keys with floatring point (`double`)
 * coordinates.
 *
 * The default implementation uses a direct lossless (in terms of numeric precision) mapping from
 * 64bit double to 64bit long integer. The mapping is defined in the Preprocessor/PostProcessor
 * functions. Other, lossy, mapping have been shown to provide somewhat better performance (due to
 * better tree structure), but this default mapping has been chosen because it is lossless.
 *
 * For more information please refer to the README of this project.
 */
template <
    dimension_t DIM,
    typename T,
    typename KEY = PhPointD<DIM>,
    PhPreprocessor<DIM, KEY> PRE = PreprocessIEEE<DIM>,
    PhPostprocessor<DIM, KEY> POST = PostprocessIEEE<DIM>>
class PhTreeD {
    friend PhTreeDebugHelper;

  public:
    PhTreeD() : tree_{} {}

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
    std::pair<T&, bool> emplace(const PhPointD<DIM>& key, _Args&&... __args) {
        return tree_.emplace(PRE(key), std::forward<_Args>(__args)...);
    }

    /*
     * See std::map::insert().
     *
     * @return a pair consisting of the inserted element (or to the element that prevented the
     * insertion) and a bool denoting whether the insertion took place.
     */
    std::pair<T&, bool> insert(const PhPointD<DIM>& key, const T& value) {
        return tree_.insert(PRE(key), value);
    }

    /*
     * @return the value stored at position 'key'. If no such value exists, one is added to the tree
     * and returned.
     */
    T& operator[](const PhPointD<DIM>& key) {
        return tree_[PRE(key)];
    }

    /*
     * Analogous to map:count().
     *
     * @return '1', if a value is associated with the provided key, otherwise '0'.
     */
    size_t count(const PhPointD<DIM>& key) const {
        return tree_.count(PRE(key));
    }

    /*
     * Analogous to map:find().
     *
     * Get an entry associated with a k dimensional key.
     * @param key the key to look up
     * @return an iterator that points either to the associated value or to {@code end()} if the key
     * was found
     */
    auto find(const PhPointD<DIM>& key) const {
        return tree_.find(PRE(key));
    }

    /*
     * See std::map::erase(). Removes any value associated with the provided key.
     *
     * @return '1' if a value was found, otherwise '0'.
     */
    size_t erase(const PhPointD<DIM>& key) {
        return tree_.erase(PRE(key));
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
        return tree_.begin(filter);
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
        const PhPointD<DIM>& min, const PhPointD<DIM>& max, FILTER filter = FILTER()) const {
        return tree_.begin_query(PRE(min), PRE(max), filter);
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
        typename DISTANCE = PhDistanceDoubleEuclidean<DIM>,
        typename FILTER = PhFilterNoOp<DIM, T>>
    auto begin_knn_query(
        size_t min_results,
        const PhPointD<DIM>& center,
        DISTANCE distance_function = DISTANCE(),
        FILTER filter = FILTER()) const {
        return tree_.begin_knn_query(min_results, PRE(center), distance_function, filter);
    }

    /*
     * @return An iterator representing the tree's 'end'.
     */
    const auto& end() const {
        return tree_.end();
    }

    /*
     * Remove all entries from the tree.
     */
    void clear() {
        tree_.clear();
    }

    /*
     * @return the number of entries (key/value pairs) in the tree.
     */
    [[nodiscard]] size_t size() const {
        return tree_.size();
    }

    /*
     * @return 'true' if the tree is empty, otherwise 'false'.
     */
    [[nodiscard]] bool empty() const {
        return tree_.empty();
    }

  private:
    // This is used by PhTreeDebugHelper
    const auto& GetInternalTree() const {
        return tree_;
    }

    v16::PhTreeV16<DIM, T, KEY, POST> tree_;
};

}  // namespace improbable::phtree

#endif  // PHTREE_PHTREE_D_H
