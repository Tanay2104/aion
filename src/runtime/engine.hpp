/* engine.hpp
 * The core engine for executing the glushkov Automaton
 * This file requires concepts.hpp and bitsets.hpp to be previously
 * included to work correctly.
 */

#pragma once
#include <array>
#include <bit>
namespace aion::runtime
{
    template<BitSet bitset>
    bitset::type route_bit(uint16_t bit_idx , typename bitset::type S_topo,typename  bitset::type S_curr,  std::array<typename bitset::type, bitset::num_count>& follow)
    {
        typename bitset::type valid = bitset::bitwise_and(bitset::shift_right(S_curr, bit_idx), 1);
        typename bitset::type mask = bitset::zero() - valid;
        return bitset::bitwise_or(S_topo, bitset::bitwise_and(mask, follow[bit_idx]));
    };


    template <BitSet bitset>
    bitset::type route_active_states(typename bitset::type current_states, const std::array<typename bitset::type, bitset::bit_count>& follow) {
        typename bitset::type topo = bitset::zero();
        while (current_states != bitset::zero()) {
            int bit_idx = std::countr_zero(current_states);
            topo = bitset::bitwise_or(topo, follow[static_cast<size_t>(bit_idx)]);
            current_states = bitset::bitwise_and(current_states, current_states - 1);
        }
        return topo;
    }

};