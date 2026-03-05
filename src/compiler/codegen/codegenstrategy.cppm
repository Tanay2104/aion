/* codegenstrategy.cppm
 * Defines the base class for emitting the object
 */

export module aion.codegen:codegenstrategy;

import :cemitter;
import aion.core;
import aion.frontend;
import aion.automata;

import std;

namespace aion::runtime
{
    export class CodeGenStrategy {

    public:

        virtual ~CodeGenStrategy() = default;
        // 1. Setup (Headers, Class Members)
        virtual void convert_to_hw_nfa() = 0;
        virtual void emit_preamble() = 0;
        virtual void emit_state_decl() = 0;

        // 2. The Core Kernel
        // This generates the entire logic block inside process_event()
        // transforming 'current state' + 'event inputs' -> 'next state'
        virtual void emit_transition_kernel() = 0;

        // 3. The Result
        virtual void emit_acceptance_check() = 0;
    };
};