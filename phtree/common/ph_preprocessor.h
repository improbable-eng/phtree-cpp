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

#ifndef PHTREE_COMMON_PH_PREPROCESSOR_H
#define PHTREE_COMMON_PH_PREPROCESSOR_H

#include "ph_base_types.h"
#include <cstdint>
#include <functional>

/*
 * PLEASE do not include this file directly, it is included via ph_common.h.
 *
 * This file contains conversion/tranmsformation functions for converting user coordinates and
 * shapes, such as PhPointD and PhBoxD, into PH-Tree native coordinates (PhPoint).
 */
namespace improbable::phtree {

template <dimension_t DIM, typename KEY>
using PhPreprocessor = PhPoint<DIM> (*)(const KEY& point);

template <dimension_t DIM, typename KEY>
using PhPostprocessor = KEY (*)(const PhPoint<DIM>& point);

template <dimension_t DIM>
using PhPreprocessorD = PhPreprocessor<DIM, PhPointD<DIM>>;

template <dimension_t DIM>
using PhPostprocessorD = PhPostprocessor<DIM, PhPointD<DIM>>;

template <dimension_t DIM>
using PhPreprocessorBoxD = PhPoint<2 * DIM> (*)(const PhBoxD<DIM>& point);

template <dimension_t DIM>
using PhPostprocessorBoxD = PhBoxD<DIM> (*)(const PhPoint<2 * DIM>& point);

class Preprocessors {
  public:
    static std::int64_t ToSortableLong(double value) {
        // To create a sortable long, we convert the double to a long using the IEEE-754 standard,
        // which stores floats in the form <sign><exponent-127><mantissa> .
        // This result is properly ordered longs for all positive doubles. Negative values have
        // inverse ordering. For negative doubles, we therefore simply invert them to make them
        // sortable, however the sign must be inverted again to stay negative.
        std::int64_t r = reinterpret_cast<std::int64_t&>(value);
        return r >= 0 ? r : r ^ 0x7FFFFFFFFFFFFFFFL;
    }

    static double ToDouble(scalar_t value) {
        auto v = value >= 0.0 ? value : value ^ 0x7FFFFFFFFFFFFFFFL;
        return reinterpret_cast<double&>(v);
    }
};

// These are the IEEE and no-op conversion functions for KEY/PRE/POST

template <dimension_t DIM>
PhPoint<DIM> PrePostNoOp(const PhPoint<DIM>& in) {
    return in;
}

template <dimension_t DIM>
PhPoint<DIM> PreprocessIEEE(const PhPointD<DIM>& point) {
    PhPoint<DIM> out;
    for (dimension_t i = 0; i < DIM; ++i) {
        out[i] = Preprocessors::ToSortableLong(point[i]);
    }
    return out;
}

template <dimension_t DIM>
PhPointD<DIM> PostprocessIEEE(const PhPoint<DIM>& in) {
    PhPointD<DIM> out;
    for (dimension_t i = 0; i < DIM; ++i) {
        out[i] = Preprocessors::ToDouble(in[i]);
    }
    return out;
}

template <dimension_t DIM>
PhPoint<2 * DIM> PreprocessBoxIEEE(const PhBoxD<DIM>& box) {
    PhPoint<2 * DIM> out;
    for (dimension_t i = 0; i < DIM; ++i) {
        out[i] = Preprocessors::ToSortableLong(box.min()[i]);
        out[i + DIM] = Preprocessors::ToSortableLong(box.max()[i]);
    }
    return out;
}

template <dimension_t DIM>
PhBoxD<DIM> PostprocessBoxIEEE(const PhPoint<2 * DIM>& in) {
    PhBoxD<DIM> out;
    for (dimension_t i = 0; i < DIM; ++i) {
        out.min()[i] = Preprocessors::ToDouble(in[i]);
        out.max()[i] = Preprocessors::ToDouble(in[i + DIM]);
    }
    return out;
}

}  // namespace improbable::phtree

#endif  // PHTREE_COMMON_PH_PREPROCESSOR_H
