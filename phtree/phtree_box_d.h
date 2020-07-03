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

#ifndef PHTREE_PHTREE_BOX_D_H
#define PHTREE_PHTREE_BOX_D_H

#include "common/ph_common.h"
#include "v16/phtree_v16.h"

namespace improbable::phtree {

/*
 * Floating-point `double` Box version of the PH-Tree.
 * This wrapper accepts axis aligned boxes as key. The boxes are defined by their minimum and
 * maximum coordinates in each dimension.
 *
 * Encoding boxes as points
 * ========================
 * The native PH-Tree can only handle points, not boxes. This PhTreeBoxD class solves this by
 * encoding the boxes into points by concatenating the minimum and maximum coordinates (with DIM
 * dimensions) of each box to a single point with 2*DIM dimensions. For example, a 2D box
 * (1,3)/(9,8) becomes (1,3,9,8).
 *
 * Querying boxes
 * ==============
 * Executing window queries on these encoded boxes requires some transformation of the query
 * constraints.
 *
 * The transformation has two steps: one steps is to transform the requested query min_req/max_req
 * points into useful internal 4D min_int/max_int points, the other step is to transform floating
 * point coordinates into integer coordinates. The second step is equivalent to the transformation
 * in normal floating-point point trees, so it is not discussed further here. Also note that the two
 * steps can be swapped.
 *
 * The default window query works as 'intersection' query, i.e. it returns all boxes that intersect
 * or lie completely inside the query window. The solution is to fill the lower half of the internal
 * min_int point with -infinity and the upper half with the requested min_req coordinate. For the
 * internal max_int point we fill the lower half with the requested max_req value and the upper half
 * with +infinity.
 *
 * For example, since the internal tree is 4D, a 2D window query with min_req=(2,4)/max_req=(12,10)
 * is transformed to min_int=(-infinity,-infinity,2,4) / max_int=(12,10,+infinity,+infinity). The
 * internal query of the PH-Tree simply returns any 4D point (= encoded box) that is strictly larger
 * than min_int and strictly smaller than max_int. The result is that it returns all boxes that
 * somehow intersect with, or lie inside of, the requested query window.
 *
 * For more information please refer to the README of this project.
 */
template <
    dimension_t DIM,
    typename T,
    typename KEY = PhBoxD<DIM>,
    PhPreprocessorBoxD<DIM> PRE = PreprocessBoxIEEE<DIM>,
    PhPostprocessorBoxD<DIM> POST = PostprocessBoxIEEE<DIM>,
    PhPreprocessorD<2 * DIM> PRE_QUERY = PreprocessIEEE<2 * DIM>>
class PhTreeBoxD {
    friend PhTreeDebugHelper;
    static const dimension_t TREE_DIM = 2 * DIM;

  public:
    PhTreeBoxD() : tree_() {}

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
    std::pair<T&, bool> emplace(const PhBoxD<DIM>& key, _Args&&... __args) {
        return tree_.emplace(PRE(key), std::forward<_Args>(__args)...);
    }

    /*
     * See std::map::insert().
     *
     * @return a pair consisting of the inserted element (or to the element that prevented the
     * insertion) and a bool denoting whether the insertion took place.
     */
    std::pair<T&, bool> insert(const PhBoxD<DIM>& key, const T& value) {
        return tree_.insert(PRE(key), value);
    }

    /*
     * @return the value stored at position 'key'. If no such value exists, one is added to the tree
     * and returned.
     */
    T& operator[](const PhBoxD<DIM>& key) {
        return tree_[PRE(key)];
    }

    /*
     * Analogous to map:count().
     *
     * @return '1', if a value is associated with the provided key, otherwise '0'.
     */
    size_t count(const PhBoxD<DIM>& key) const {
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
    auto find(const PhBoxD<DIM>& key) const {
        return tree_.find(PRE(key));
    }

    /*
     * See std::map::erase(). Removes any value associated with the provided key.
     *
     * @return '1' if a value was found, otherwise '0'.
     */
    size_t erase(const PhBoxD<DIM>& key) {
        return tree_.erase(PRE(key));
    }

    /*
     * Iterates over all entries in the tree. The optional filter allows filtering entries and nodes
     * (=sub-trees) before returning / traversing them. By default all entries are returned. Filter
     * functions must implement the same signature as the default 'PhFilterNoOp'.
     *
     * @return an iterator over all (filtered) entries in the tree,
     */
    template <typename FILTER = PhFilterNoOp<TREE_DIM, T>>
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
    template <typename FILTER = PhFilterNoOp<TREE_DIM, T>>
    auto begin_query(
        const PhPointD<DIM>& min, const PhPointD<DIM>& max, FILTER filter = FILTER()) const {
        PhPointD<TREE_DIM> min_2_DIM;
        PhPointD<TREE_DIM> max_2_DIM;
        for (dimension_t i = 0; i < DIM; i++) {
            min_2_DIM[i] = D_NEG_INFINITY;
            max_2_DIM[i] = max[i];
        }
        for (dimension_t i = DIM; i < 2 * DIM; i++) {
            min_2_DIM[i] = min[i - DIM];
            max_2_DIM[i] = D_INFINITY;
        }
        return tree_.begin_query(PRE_QUERY(min_2_DIM), PRE_QUERY(max_2_DIM), filter);
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

    v16::PhTreeV16<TREE_DIM, T, KEY, POST> tree_;
};

}  // namespace improbable::phtree

#endif  // PHTREE_PHTREE_BOX_D_H
