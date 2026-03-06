/* regexgen.cpp
 * Implementation of the regexgen.cppm functions.
 */
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
            std::unique_ptr<runtime::CodeGenStrategy> engine;
            if (ctxt.options.arch == core::Arch::SCALAR64)
            {
                engine = std::make_unique<runtime::ScalarStrategy>(runtime::ScalarStrategy(emitter, nfas[regex_decl.name], symbol_table, regex_decl, ctxt));
            }
            else if (ctxt.options.arch == core::Arch::AVX2)
            {
                ctxt.diagnostics.report_internal_error("Target arch not implemented yet");
                // do stuff as needed.
            }
            engine->emit_preamble();
            engine->emit_state_decl();
            engine->emit_transition_kernel();
            engine->emit_acceptance_check();
            engine->emit_reset();
            emitter.emit_line("\n\n");
        }
    }
};