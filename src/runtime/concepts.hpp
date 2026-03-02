/* concepts.hpp
 * Contains the concept definitions of types like BitSet
 */

#pragma once
#include <concepts>

namespace aion::runtime {
    template<typename T>
    concept BitSet = requires(typename T::type a, typename T::type b, const unsigned short pos) {
        typename T::type; // Must define an underlying storage type
        { T::bit_count } -> std::convertible_to<std::size_t>; // Should also specify num bits needed
        { T::zero() } -> std::same_as<typename T::type>;
        { T::from_position(pos) } -> std::same_as<typename T::type>;
        { T::bitwise_or(a, b) } -> std::same_as<typename T::type>;
        { T::bitwise_and(a, b) } -> std::same_as<typename T::type>;
        { T::shift_right(a, pos)} -> std::same_as<typename T::type>;
        { T::shift_left(a, pos)} -> std::same_as<typename T::type>;
    };
}