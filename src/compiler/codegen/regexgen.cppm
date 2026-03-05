/* regexgen.cppm
 * Function declarations for emitting code
 * regarding the core engine output.
 */
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

}
