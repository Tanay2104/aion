/* nfa.cppm
 * The data structures for the machine
 */

export module aion.automata:nfa;

import std;

namespace aion::automata
{
    // The 64 indicates use of uint64
    struct NFA_64
    {
        bool nullable = false;
        std::uint16_t num_states{};
        std::uint64_t first;
        std::uint64_t last;
        std::array<std::uint64_t, 64> mask;
    };
};