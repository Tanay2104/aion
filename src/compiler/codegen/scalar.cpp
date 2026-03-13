/* scalar.cpp
 * Implementation of scalar.cppm
 */

module aion.codegen;

import :codegenstrategy;
import aion.automata;
import :cemitter;
import std;

namespace aion::runtime
{
    void ScalarStrategy::convert_to_hw_nfa()
    {
        automata::NFA_64 nfa_64;
        nfa_64.nullable = generic_nfa.nullable;
        nfa_64.num_states = generic_nfa.num_states;
        nfa_64.first = 0ULL;
        for (const std::uint16_t first : generic_nfa.first)
        {
            nfa_64.first = (nfa_64.first | (1ULL << first));
        }

        nfa_64.last = 0;
        for (const std::uint16_t last : generic_nfa.last)
        {
            nfa_64.last = (nfa_64.last | (1ULL << last));
        }

        for (auto& [start, end_set] : generic_nfa.follow)
        {
            nfa_64.follow[start] = 0;
            for (const std::uint16_t end: end_set)
            {
                nfa_64.follow[start] |=  (1ULL << end);
            }
        }

        this->nfa = nfa_64;
    }

    void ScalarStrategy::emit_preamble()
    {
        // This will be executed first.
        convert_to_hw_nfa();
        emitter.emit_block_start(std::format("class Engine_{}", regex_decl.name));

    }

    void ScalarStrategy::emit_state_decl()
    {
        emitter.emit_line(std::format("static constexpr std::uint64_t first = {:#x};", nfa.first));
        emitter.emit_line(std::format("static constexpr std::uint64_t last = {:#x};", nfa.last));
        emitter.emit_block_start("static constexpr std::array<std::uint64_t, 64> follow = ");
        for (auto& entry: nfa.follow)
        {
            emitter.emit_line(std::format("{:#x},", entry));
        }
        emitter.emit_block_end(";");
        emitter.emit_line("\n");
        emitter.emit_line("std::uint64_t S = 0ULL;\n");
    }
    void ScalarStrategy::emit_transition_kernel()
    {
        emitter.emit_line("public:");
        emitter.emit_block_start("bool process_event(const Event& event)");

        emitter.emit_line("std::uint64_t M = 0ULL;");
        emitter.emit_line("\n");
        // Now we need to loop over events and emit the predicate evaluation M stuff.
        // however we don't have direct access to the maps so we use a two map approach.
        const frontend::RegexMetadata& meta = std::get<frontend::RegexMetadata>(symbol_table.resolve(regex_decl.name)->details);
        for (std::uint16_t pos_id = 1; pos_id <= nfa.num_states; ++pos_id)
        {
            const auto it = meta.pos_ids_to_names.find(pos_id);
            if (it == meta.pos_ids_to_names.end())
            {
                continue;
            }
            std::string_view name = it->second;
            if (name == "_")
            {
                emitter.emit_line(std::format("M |= ((0ULL - true) & (1ULL << {}));", pos_id));
            }
            else
            {
                /* We can also try this:
                 emitter.emit_line(std::format("M = B::bitwise_or(B::shift_left(static_cast<B::type>({}{}{}), {}), M);",
                  function_prefix, name, function_call_suffix, pos_id));
                 */
                emitter.emit_line(std::format("M |= ((0ULL - static_cast<std::uint64_t>({}{}{})) & (1ULL << {}));", codegen::function_prefix,name,codegen::function_call_suffix,pos_id));
            }
        }

        emitter.emit_line("std::uint64_t S_topo = first;");
        if (ctxt.options.jitter == true)
        {
            emitter.emit_block_start("while (S != 0ULL)");
            emitter.emit_line("int bit_idx = std::countr_zero(S);");
            emitter.emit_line("S_topo |= follow[static_cast<size_t>(bit_idx)];");
            emitter.emit_line("S &= (S - 1);");
            emitter.emit_block_end(";");
        }
        else
        {
            emitter.emit_block_start(std::format("for (size_t i = 1; i <= {}; ++i)", nfa.num_states));
            emitter.emit_line("std::uint64_t mask = 0ULL - ((S >> i) & 1ULL);");
            emitter.emit_line("S_topo |= (follow[i] & mask);");
            emitter.emit_block_end("");
        }
        emitter.emit_line("S = S_topo & M;");
    }
    void ScalarStrategy::emit_acceptance_check()
    {
        emitter.emit_line("return ((S & last) != 0ULL);");

        emitter.emit_block_end(";"); // for function
    }
    void ScalarStrategy::emit_reset()
    {
        emitter.emit_line("void reset() { S = 0ULL; }");
        emitter.emit_block_end(";"); // for class
    }
}
