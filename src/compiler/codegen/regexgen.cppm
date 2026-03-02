/* regexgen.cppm
 * Function declarations for emitting code
 * regarding the core engine output.
 */
module;
#include "../../runtime/concepts.hpp"
#include "../../runtime/bitsets.hpp"
export module aion.codegen:regexgen;

import std;
import aion.core;
import aion.frontend;
import aion.automata;
import :cemitter;

namespace aion::codegen
{
    export void emit_regex_engines(const frontend::AionFile& ast,
            const frontend::SymbolTable& symbol_table,
            CEmitter& emitter, const core::CompilationContext& ctxt,
            std::unordered_map<std::string_view, automata::Generic_NFA>& nfas);

    class RegexEngineEmitter
    {
        private:
            const frontend::RegexDecl& regex_decl;
            const automata::Generic_NFA& nfa;
            const frontend::SymbolTable& symbol_table;
            const core::CompilationContext& ctxt;
            automata::HardwareNFA<runtime::Scalar64> hw_nfa;
            CEmitter& emitter;

        void emit_glushkov_sets();
        void emit_process_event();

        public:
        RegexEngineEmitter(const frontend::RegexDecl& _regex_decl,
            const automata::Generic_NFA& _nfa,
            const frontend::SymbolTable& _symbol_table,
            const core::CompilationContext& _ctxt,
            CEmitter& _emitter);
        void emit_regex_engine();

    };

}
