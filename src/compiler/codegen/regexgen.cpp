/* regexgen.cpp
 * Implementation of the regexgen.cppm functions.
 */
module;
#include "../../runtime/concepts.hpp"
#include "../../runtime/bitsets.hpp"

module aion.codegen;

import std;
import :cemitter;
import aion.automata;
namespace aion::codegen
{
    void emit_regex_engines(const frontend::AionFile& ast,
                const frontend::SymbolTable& symbol_table,
                CEmitter& emitter, const core::CompilationContext& ctxt,
                std::unordered_map<std::string_view, automata::Generic_NFA>& nfas)
    {
        for (const frontend::RegexDecl& regex_decl: ast.regexes)
        {
            RegexEngineEmitter engine(regex_decl, nfas[regex_decl.name], symbol_table, ctxt, emitter);
            engine.emit_regex_engine();
            emitter.emit_line("\n\n");
        }
    }

    RegexEngineEmitter::RegexEngineEmitter(const frontend::RegexDecl& _regex_decl,
    const automata::Generic_NFA& _nfa,
    const frontend::SymbolTable& _symbol_table,
    const core::CompilationContext& _ctxt,
    CEmitter& _emitter)
        : regex_decl(_regex_decl),
          nfa(_nfa), symbol_table(_symbol_table), ctxt(_ctxt), emitter(_emitter)
    {
        this->hw_nfa = automata::convert_to_nfa<runtime::Scalar64>(nfa);

    }
    void RegexEngineEmitter::emit_regex_engine()
    {
        emitter.emit_block_start(std::format("class Engine_{}", regex_decl.name));
        // Right now only Scalar64. We can add other architectures later,
        // and the input arch is in ctxt.options.arch.
        emitter.emit_line("using B = Scalar64;");

        emit_glushkov_sets();

        emitter.emit_line("B::type S = B::zero();\n");
        emitter.emit_line("public:");
        emitter.emit_block_start("bool process_event(const Event& event)");

        emit_process_event();

        emitter.emit_line("const B::type S_topo = B::bitwise_or(first, route_active_states<B>(S, follow));\n");
        emitter.emit_line("S = B::bitwise_and(S_topo, M);\n");
        emitter.emit_line("return (B::bitwise_or(S, last) != B::zero());\n");

        emitter.emit_block_end(";"); // for function
        emitter.emit_block_end(";"); // for class
    }

    void RegexEngineEmitter::emit_glushkov_sets()
    {
        // We will ctxt.options.arch to choose arch.
        if (ctxt.options.arch == core::Arch::SCALAR64)
        {
            emitter.emit_line(std::format("static constexpr B::type first = {:#x};", hw_nfa.first));
            emitter.emit_line(std::format("static constexpr B::type last = {:#x};", hw_nfa.last));
            emitter.emit_block_start("static constexpr std::array<B::type, B::bit_count> follow = ");
            for (auto& entry: hw_nfa.follow)
            {
                emitter.emit_line(std::format("{:#x},", entry));
            }
            emitter.emit_block_end(";");
            emitter.emit_line("\n");
        }
        else
        {
            ctxt.diagnostics.report_internal_error("Arch not supported");
        }
    }

    void RegexEngineEmitter::emit_process_event()
    {
        emitter.emit_line("B::type M = B::zero();\n");

        // Now we need to loop over events and emit the predicate evaluation M stuff.
        // however we don't have direct access to the maps so we use a two map approach.
        const frontend::RegexMetadata& meta = std::get<frontend::RegexMetadata>(symbol_table.resolve(regex_decl.name)->details);
        for (auto& [node_ptr, pos_id]: meta.node_to_pos_ids)
        {
            std::string_view name = meta.pos_ids_to_names.at(pos_id);
            if (name == "_")
            {
                emitter.emit_line(std::format("M = B::bitwise_or(B::bitwise_and(B::zero() - true, B::from_position({})), M);", pos_id));
            }
            else
            {
                emitter.emit_line(std::format("M = B::bitwise_or(B::bitwise_and(B::zero() - {}{}{}, B::from_position({})), M);", function_prefix,name,function_call_suffix, pos_id));
            }
        }


    }


};