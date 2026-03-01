/* eventgen.cppm
 * Function declaration for event codegen.
 */

export module aion.codegen:eventgen;

import std;
import aion.frontend;
import :cemitter;

namespace aion::codegen
{
    export void emit_event(const frontend::EventDecl& event_decl, CEmitter& emitter);
}
