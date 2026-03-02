/* bitsets.hpp
 * Contains the implementations of various types of bitsets
 */
#pragma once

#include <cstdint>
#include <immintrin.h>

// Note: each type  must support operator=
namespace aion::runtime {
    struct Scalar64 {
        using type = uint64_t;
        static constexpr size_t bit_count = 256;
        static constexpr type zero() { return 0ULL; }
        static constexpr type from_position(const unsigned short p) { return 1ULL << p; }
        static constexpr type bitwise_and(const type a, const type b) { return a & b; }
        static constexpr type bitwise_or(const type a, const type b) { return a | b; }
        static constexpr type shift_right(const type a, const unsigned short p) { return a >> p; }
        static constexpr type shift_left(const type a, const unsigned short p) { return a << p; }

    };
}