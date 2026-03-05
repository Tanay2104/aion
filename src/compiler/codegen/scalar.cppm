/* scalar.cppm
 * Contains the derived class for scalar code emitter
 */


export module aion.codegen:scalar;

import :codegenstrategy;
import :cemitter;
import aion.core;
import aion.automata;
import std;

namespace aion::runtime
{
    export class ScalarStrategy : public CodeGenStrategy {

        codegen::CEmitter& emitter;
        const frontend::RegexDecl& regex_decl;
        const automata::Generic_NFA& generic_nfa;
        const frontend::SymbolTable& symbol_table;
        const core::CompilationContext& ctxt;
        automata::NFA_64 nfa;

    public:
        ScalarStrategy(codegen::CEmitter& _emitter,
                        const automata::Generic_NFA& _generic_nfa,
                        const  frontend::SymbolTable& _symbol_table,
                        const frontend::RegexDecl& _regex_decl,
                        const core::CompilationContext& _ctxt
                        ) : emitter(_emitter), regex_decl(_regex_decl),
                        generic_nfa(_generic_nfa), symbol_table(_symbol_table),
                        ctxt(_ctxt) {}
        void convert_to_hw_nfa() override;
        void emit_preamble() override;
        void emit_state_decl() override;
        void emit_transition_kernel() override;
        void emit_acceptance_check() override;
    };
};