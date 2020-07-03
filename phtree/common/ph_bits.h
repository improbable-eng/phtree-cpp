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

#ifndef PHTREE_PH_COMMON_BITS_H
#define PHTREE_PH_COMMON_BITS_H

#include "ph_base_types.h"
#include <cassert>

#if defined(__clang__)
#include <x86intrin.h>
#elif defined(__GNUC__)
#include <x86intrin.h>
#elif defined(_MSC_VER)
// https://docs.microsoft.com/en-us/cpp/intrinsics/x64-amd64-intrinsics-list?view=vs-2019
#include <immintrin.h>
#endif

/*
 * PLEASE do not include this file directly, it is included via ph_common.h.
 *
 * This file defines how certain bit level operations are implemented, such as:
 * - count leading zeroes
 * - count trailing zeros
 */
namespace improbable::phtree {

namespace {
inline bit_width_t NumberOfLeadingZeros(std::uint64_t bit_string) {
    if (bit_string == 0) {
        return 64;
    }
    bit_width_t n = 1;
    std::uint32_t x = (bit_string >> 32);
    if (x == 0) {
        n += 32;
        x = (int)bit_string;
    }
    if (x >> 16 == 0) {
        n += 16;
        x <<= 16;
    }
    if (x >> 24 == 0) {
        n += 8;
        x <<= 8;
    }
    if (x >> 28 == 0) {
        n += 4;
        x <<= 4;
    }
    if (x >> 30 == 0) {
        n += 2;
        x <<= 2;
    }
    n -= x >> 31;
    return n;
}

inline bit_width_t NumberOfLeadingZeros(std::int32_t bit_string) {
    if (bit_string == 0) {
        return 32;
    }
    bit_width_t n = 1;
    if (bit_string >> 16 == 0) {
        n += 16;
        bit_string <<= 16;
    }
    if (bit_string >> 24 == 0) {
        n += 8;
        bit_string <<= 8;
    }
    if (bit_string >> 28 == 0) {
        n += 4;
        bit_string <<= 4;
    }
    if (bit_string >> 30 == 0) {
        n += 2;
        bit_string <<= 2;
    }
    n -= bit_string >> 31;
    return n;
}

inline bit_width_t NumberOfTrailingZeros(std::uint64_t bit_string) {
    if (bit_string == 0) {
        return 64;
    }
    uint32_t x = 0;
    uint32_t y = 0;
    uint16_t n = 63;
    y = (std::uint32_t)bit_string;
    if (y != 0) {
        n = n - 32;
        x = y;
    } else {
        x = (std::uint32_t)(bit_string >> 32);
    }
    y = x << 16;
    if (y != 0) {
        n = n - 16;
        x = y;
    }
    y = x << 8;
    if (y != 0) {
        n = n - 8;
        x = y;
    }
    y = x << 4;
    if (y != 0) {
        n = n - 4;
        x = y;
    }
    y = x << 2;
    if (y != 0) {
        n = n - 2;
        x = y;
    }
    return n - ((x << 1) >> 31);
}
}  // namespace

#if defined(__clang__)
#define CountLeadingZeros(bits) NumberOfLeadingZeros(bits)
//#define CountLeadingZeros(bits) __lzcnt64(bits)
//#define CountTrailingZeros(bits) NumberOfTrailingZeros(bits)
#define CountTrailingZeros(bits) __tzcnt_u64(bits)

#elif defined(__GNUC__)
#define CountLeadingZeros(bits) NumberOfLeadingZeros(bits)
   // TODO this works only on 64 bit arch, otherwise __builtin_clzll (double 'l')
//#define CountLeadingZeros(bits) __builtin_clzl(bits)
#define CountTrailingZeros(bits) NumberOfTrailingZeros(bits)
   // TODO this works only on 64 bit arch, otherwise __builtin_ctzll (double 'l')
//#define CountTrailingZeros(bits) __builtin_ctzl(bits)

#elif defined(_MSC_VER)
// https://docs.microsoft.com/en-us/cpp/intrinsics/x64-amd64-intrinsics-list?view=vs-2019
// static inline size_t CountLeadingZeros(std::uint64_t bits) {
//    // TODO there is alo __lzcnt_u64  (AMD/INTEL)
//// TODO this is MS:   -> #include<intrin.h>
//    #define CountTrailingZeros(bits) __lzcnt64(bits);
#define CountLeadingZeros(bits) NumberOfLeadingZeros(bits)
#define CountTrailingZeros(bits) NumberOfTrailingZeros(bits)
//#define CountTrailingZeros(bits) _tzcnt_u64(bits);

#else
#define CountLeadingZeros(bits) NumberOfLeadingZeros(bits)
#define CountTrailingZeros(bits) NumberOfTrailingZeros(bits)
#endif

}  // namespace improbable::phtree

#endif  // PHTREE_PH_COMMON_BITS_H
