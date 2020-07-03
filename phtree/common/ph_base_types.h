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

#ifndef PHTREE_COMMON_BASE_TYPES_H
#define PHTREE_COMMON_BASE_TYPES_H

#include <algorithm>
#include <array>
#include <cassert>
#include <climits>
#include <cstdint>
#include <sstream>

/*
 * PLEASE do not include this file directly, it is included via ph_common.h.
 *
 * This file contains specifications for various types used in the PH-Tree, including
 * PhPoint, PhPointD and PhPointBox.
 */
namespace improbable::phtree {

// ************************************************************************
// Constants and base types
// ************************************************************************

using scalar_t = int64_t;
// Bits in a coordinate (usually a double or long has 64 bits, so uint_8 suffices)
using bit_width_t = uint16_t;
// Number of bit for 'scalar_t'. Note that 'digits' does _not_ include sign bit, so e.g. int64_t has
// 63 `digits`, however we need all bits, i.e. 64.
static constexpr bit_width_t MAX_BIT_WIDTH =
    std::numeric_limits<scalar_t>::digits + std::numeric_limits<char>::is_signed;
using node_size_t = int32_t;  // Node sizes
using bit_mask_t = uint64_t;  // Bit mask
static constexpr bit_mask_t MAX_MASK = std::numeric_limits<bit_mask_t>::max();
using dimension_t = size_t;  // Number of dimensions
using hc_pos_t = uint64_t;

template <dimension_t DIM>
static constexpr hc_pos_t END_POS = (hc_pos_t(1) << DIM);  // Max hypercube address + 1

// double
using scalar_d_t = double;
static constexpr scalar_d_t D_INFINITY = std::numeric_limits<scalar_d_t>::infinity();
static constexpr scalar_d_t D_NEG_INFINITY = -std::numeric_limits<scalar_d_t>::infinity();

// ************************************************************************
// Basic structs and classes
// ************************************************************************

template <dimension_t DIM>
class PhBoxD;

struct HashPhBoxD;

template <dimension_t DIM>
using PhPoint = std::array<scalar_t, DIM>;

template <dimension_t DIM>
using PhPointD = std::array<scalar_d_t, DIM>;

template <dimension_t DIM>
class PhBoxD {
    friend HashPhBoxD;

  public:
    explicit PhBoxD() = default;

    PhBoxD(const PhBoxD<DIM>& orig) = default;

    PhBoxD(const std::array<double, DIM>& min, const std::array<double, DIM>& max)
    : min_{min}, max_{max} {}

    [[nodiscard]] PhPointD<DIM> min() const {
        return min_;
    }

    [[nodiscard]] PhPointD<DIM> max() const {
        return max_;
    }

    [[nodiscard]] PhPointD<DIM>& min() {
        return min_;
    }

    [[nodiscard]] PhPointD<DIM>& max() {
        return max_;
    }

    void min(const std::array<double, DIM>& new_min) {
        min_ = new_min;
    }

    void max(const std::array<double, DIM>& new_max) {
        max_ = new_max;
    }

    auto operator==(const PhBoxD<DIM>& other) const -> bool {
        return min_ == other.min_ && max_ == other.max_;
    }

  private:
    PhPointD<DIM> min_;
    PhPointD<DIM> max_;
};

struct HashPhBoxD {
    template <dimension_t DIM>
    std::size_t operator()(const PhBoxD<DIM>& x) const {
        std::size_t hash_val = 0;
        for (dimension_t i = 0; i < DIM; i++) {
            hash_val = std::hash<scalar_t>{}(x.min_[i]) ^ (hash_val * 31);
            hash_val = std::hash<scalar_t>{}(x.max_[i]) ^ (hash_val * 31);
        }
        return hash_val;
    }
};

template <dimension_t DIM>
std::ostream& operator<<(std::ostream& os, const PhPoint<DIM>& data) {
    assert(DIM >= 1);
    os << "[";
    for (dimension_t i = 0; i < DIM - 1; i++) {
        os << data[i] << ",";
    }
    os << data[DIM - 1] << "]";
    return os;
}

template <dimension_t DIM>
std::ostream& operator<<(std::ostream& os, const PhPointD<DIM>& data) {
    assert(DIM >= 1);
    os << "[";
    for (dimension_t i = 0; i < DIM - 1; i++) {
        os << data[i] << ",";
    }
    os << data[DIM - 1] << "]";
    return os;
}

template <dimension_t DIM>
std::ostream& operator<<(std::ostream& os, const PhBoxD<DIM>& data) {
    assert(DIM >= 1);
    os << "[";
    for (dimension_t i = 0; i < DIM - 1; i++) {
        os << data[i] << ",";
    }
    os << data[DIM - 1] << "]:[";
    for (dimension_t i = 0; i < DIM - 1; i++) {
        os << data[DIM + i] << ",";
    }
    os << data[2 * DIM - 1] << "]";
    return os;
}

}  // namespace improbable::phtree

#endif  // PHTREE_COMMON_BASE_TYPES_H
