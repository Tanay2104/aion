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

    export void dump_nfa_dot(const std::unordered_map<std::string_view, Generic_NFA>& all_nfa,
                             std::string_view filename)
    {
        auto escape_dot = [](std::string_view text) -> std::string
        {
            std::string escaped;
            escaped.reserve(text.size());
            for (const char c : text)
            {
                if (c == '"' || c == '\\')
                {
                    escaped.push_back('\\');
                }
                escaped.push_back(c);
            }
            return escaped;
        };

        auto sorted_set = [](const std::unordered_set<std::uint16_t>& input) -> std::vector<std::uint16_t>
        {
            std::vector<std::uint16_t> out(input.begin(), input.end());
            std::ranges::sort(out);
            return out;
        };

        auto collect_states = [](const Generic_NFA& nfa) -> std::vector<std::uint16_t>
        {
            std::set<std::uint16_t> states;
            for (const auto id : nfa.first) states.insert(id);
            for (const auto id : nfa.last) states.insert(id);
            for (const auto& [from, to_set] : nfa.follow)
            {
                states.insert(from);
                for (const auto to : to_set)
                {
                    states.insert(to);
                }
            }
            if (states.empty() && nfa.num_states > 0)
            {
                for (std::uint16_t i = 1; i <= nfa.num_states; ++i)
                {
                    states.insert(i);
                }
            }
            return std::vector<std::uint16_t>(states.begin(), states.end());
        };

        std::ofstream out{std::string(filename)};
        if (!out)
        {
            std::println(std::cerr, "Failed to open NFA dump file: {}", filename);
            return;
        }

        std::vector<std::pair<std::string, const Generic_NFA*>> all;
        all.reserve(all_nfa.size());
        for (const auto& [name, nfa] : all_nfa)
        {
            all.emplace_back(std::string(name), &nfa);
        }
        std::ranges::sort(all, [](const auto& lhs, const auto& rhs)
        {
            return lhs.first < rhs.first;
        });

        std::println(out, "digraph AionNFA {{");
        std::println(out, "  rankdir=LR;");
        std::println(out, "  node [shape=circle];");

        std::size_t cluster_idx = 0;
        for (const auto& [regex_name, nfa_ptr] : all)
        {
            const Generic_NFA& nfa = *nfa_ptr;
            const std::string escaped_name = escape_dot(regex_name);
            const std::vector<std::uint16_t> states = collect_states(nfa);
            const std::vector<std::uint16_t> first_sorted = sorted_set(nfa.first);
            const std::vector<std::uint16_t> last_sorted = sorted_set(nfa.last);

            std::println(out, "  subgraph cluster_{} {{", cluster_idx++);
            std::println(out, "    label=\"Regex: {}\";", escaped_name);
            std::println(out, "    color=gray60;");
            std::println(out, "    \"{}_start\" [shape=point, label=\"\"];", escaped_name);

            if (nfa.nullable)
            {
                std::println(out, "    \"{}_eps\" [shape=doublecircle, label=\"ε\"];", escaped_name);
                std::println(out, "    \"{}_start\" -> \"{}_eps\" [label=\"ε\"];", escaped_name, escaped_name);
            }

            for (const auto id : states)
            {
                const bool is_accepting = nfa.last.contains(id);
                if (is_accepting)
                {
                    std::println(out, "    \"{}_s{}\" [shape=doublecircle, label=\"{}\"];", escaped_name, id, id);
                }
                else
                {
                    std::println(out, "    \"{}_s{}\" [label=\"{}\"];", escaped_name, id, id);
                }
            }

            for (const auto id : first_sorted)
            {
                std::println(out, "    \"{}_start\" -> \"{}_s{}\" [label=\"start\"];", escaped_name, escaped_name, id);
            }

            std::vector<std::uint16_t> from_states;
            from_states.reserve(nfa.follow.size());
            for (const auto& [from, to_set] : nfa.follow)
            {
                (void)to_set;
                from_states.push_back(from);
            }
            std::ranges::sort(from_states);

            for (const auto from : from_states)
            {
                const auto to_it = nfa.follow.find(from);
                if (to_it == nfa.follow.end()) continue;
                const std::vector<std::uint16_t> to_sorted = sorted_set(to_it->second);
                for (const auto to : to_sorted)
                {
                    std::println(out, "    \"{}_s{}\" -> \"{}_s{}\";", escaped_name, from, escaped_name, to);
                }
            }

            std::println(out, "    // nullable: {}, first: {}, last: {}", nfa.nullable,
                         first_sorted.size(), last_sorted.size());
            std::println(out, "  }}");
        }

        std::println(out, "}}");
    }

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
