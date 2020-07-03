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

#ifndef PHTREE_COMMON_FLAT_ARRAY_MAP_H
#define PHTREE_COMMON_FLAT_ARRAY_MAP_H

#include "ph_bits.h"
#include <cassert>
#include <tuple>

/*
 * PLEASE do not include this file directly, it is included via ph_common.h.
 *
 * This file contains the array_map implementation, which is used in low-dimensional nodes in the
 * PH-Tree.
 */
namespace improbable::phtree {

namespace {
template <typename T, std::size_t SIZE>
class PhFlatMapIterator;

template <typename T>
using PhFlatMapPair = std::pair<size_t, T>;

using bit_string_t = std::uint64_t;
constexpr bit_string_t U64_ONE = bit_string_t(1);
}  // namespace

/*
 * The array_map is a flat map implementation that uses an array of SIZE=2^DIM. The key is
 * effectively the position in the array.
 *
 * It has O(1) insertion/removal time complexity, but O(2^DIM) space complexity, so it is best used
 * when DIM is low and/or the map is known to have a high fill ratio.
 */
template <typename T, std::size_t SIZE>
class array_map {
    friend PhFlatMapIterator<T, SIZE>;
    static_assert(SIZE <= 64);  // or else we need to adapt 'occupancy'
    static_assert(SIZE > 0);

  public:
    explicit array_map() : occupancy{0}, size_{0} {};

    [[nodiscard]] auto find(size_t index) const {
        return occupied(index) ? PhFlatMapIterator<T, SIZE>{index, *this} : end();
    }

    [[nodiscard]] auto lower_bound(size_t index) const {
        size_t index2 = lower_bound_index(index);
        if (index2 < SIZE) {
            return PhFlatMapIterator<T, SIZE>{index2, *this};
        }
        return end();
    }

    [[nodiscard]] auto begin() const {
        size_t index = CountTrailingZeros(occupancy);
        // Assert index points to a valid position or outside the map if the map is empty
        assert((size_ == 0 && index >= SIZE) || occupied(index));
        return PhFlatMapIterator<T, SIZE>{index < SIZE ? index : SIZE, *this};
    }

    [[nodiscard]] auto cbegin() const {
        size_t index = CountTrailingZeros(occupancy);
        // Assert index points to a valid position or outside the map if the map is empty
        assert((size_ == 0 && index >= SIZE) || occupied(index));
        return PhFlatMapIterator<T, SIZE>{index < SIZE ? index : SIZE, *this};
    }

    [[nodiscard]] auto end() const {
        return PhFlatMapIterator<T, SIZE>{SIZE, *this};
    }

    auto emplace(size_t index, T&& value) {
        return emplace_base(index, std::forward<T>(value));
    }

    template <typename... _Args>
    auto emplace(_Args&&... __args) {
        return emplace_base(std::forward<_Args>(__args)...);
    }

    bool erase(size_t index) {
        if (occupied(index)) {
            occupied(index, false);
            --size_;
            data_[index].second.~T();
            return true;
        }
        return false;
    }

    bool erase(PhFlatMapIterator<T, SIZE>& iterator) {
        size_t index = iterator.first;
        if (occupied(index)) {
            occupied(index, false);
            --size_;
            data_[index].second.~T();
            return true;
        }
        return false;
    }

    [[nodiscard]] size_t size() const {
        return size_;
    }

  private:
    std::pair<PhFlatMapPair<T>*, bool> emplace_base(size_t index, T&& value) {
        if (!occupied(index)) {
            data_[index].first = index;
            data_[index].second = std::forward<T>(value);
            ++size_;
            occupied(index, true);
            return {&data_[index], true};
        }
        return {&data_[index], false};
    }

    [[nodiscard]] size_t lower_bound_index(size_t index) const {
        assert(index < SIZE);
        size_t num_zeros = CountTrailingZeros(occupancy >> index);
        // num_zeros may be equal to SIZE if no bits remain
        return std::min(SIZE, index + num_zeros);
    }

    void occupied(size_t index, bool flag) {
        assert(index < SIZE);
        assert(occupied(index) != flag);
        // flip the bit
        occupancy ^= (U64_ONE << index);
        assert(occupied(index) == flag);
    }

    [[nodiscard]] bool occupied(size_t index) const {
        return (occupancy >> index) & U64_ONE;
    }

    bit_string_t occupancy;
    std::uint32_t size_;
    PhFlatMapPair<T> data_[SIZE];
};

namespace {
template <typename T, std::size_t SIZE>
class PhFlatMapIterator {
    friend array_map<T, SIZE>;

  public:
    PhFlatMapIterator() : first{0}, map_{nullptr} {};

    explicit PhFlatMapIterator(size_t index, const array_map<T, SIZE>& map)
    : first{index}, map_{&map} {
        assert(index >= 0);
        assert(index <= SIZE);
    }

    auto& operator*() const {
        assert(first < SIZE && map_->occupied(first));
        return const_cast<PhFlatMapPair<T>&>(map_->data_[first]);
    }

    auto* operator-> () const {
        assert(first < SIZE && map_->occupied(first));
        return const_cast<PhFlatMapPair<T>*>(&map_->data_[first]);
    }

    auto& operator++() {
        first = (first + 1) >= SIZE ? SIZE : first = map_->lower_bound_index(first + 1);
        return *this;
    }

    auto operator++(int) {
        PhFlatMapIterator iterator(first, *map_);
        ++(*this);
        return iterator;
    }

    friend bool operator==(
        const PhFlatMapIterator<T, SIZE>& left, const PhFlatMapIterator<T, SIZE>& right) {
        return left.first == right.first;
    }

    friend bool operator!=(
        const PhFlatMapIterator<T, SIZE>& left, const PhFlatMapIterator<T, SIZE>& right) {
        return !(left == right);
    }

  private:
    size_t first;
    const array_map<T, SIZE>* map_;
};

}  // namespace
}  // namespace improbable::phtree

#endif  // PHTREE_COMMON_FLAT_ARRAY_MAP_H
