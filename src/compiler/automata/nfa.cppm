/* nfa.cppm
 * The data structures for the machine
 */
module;
export module aion.automata:nfa;

import std;

namespace aion::automata
{
    export struct Generic_NFA
    {
        std::uint16_t num_states{};
        bool nullable{};
        std::unordered_set<std::uint16_t> first{}; // List of PosIDs
        std::unordered_set<std::uint16_t> last{};  // List of PosIDs
        std::unordered_map<std::uint16_t, std::unordered_set<std::uint16_t>> follow;
    };

    export struct NFA_64
    {
        std::uint16_t num_states{};
        bool nullable{};
        std::uint64_t first{};
        std::uint64_t last{};
        std::array<std::uint64_t , 64> follow{};
    };

    /*HardwareNFA<bitset> convert_to_nfa_64(const Generic_NFA& nfa) {
        HardwareNFA<bitset> nfa_64;
        nfa_64.nullable = nfa.nullable;

        nfa_64.first = 0;
        for (const std::uint16_t first : nfa.first)
        {
            nfa_64.first = bitset::bitwise_or(nfa_64.first,bitset::from_position(first));
        }

        nfa_64.last = 0;
        for (const std::uint16_t last : nfa.last)
        {
            nfa_64.last = bitset::bitwise_or(bitset::bitwise_or(nfa_64.last,bitset::from_position(last)), nfa_64.last);
        }

        for (auto& [start, end_set] : nfa.follow)
        {
            nfa_64.follow[start] = 0;
            for (const std::uint16_t end: end_set)
            {
                nfa_64.follow[start] = bitset::bitwise_or(bitset::bitwise_or(nfa_64.follow[start],bitset::from_position(end)), nfa_64.follow[end]);
            }
        }

        return nfa_64;
    }*/
};