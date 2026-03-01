/* event_emitter.cpp
 * Code for emitting the event struct in a C header file.
 */

module aion.codegen;

import :eventgen;
import :cemitter;

namespace aion::codegen
{
    void emit_event(const frontend::EventDecl& event_decl, CEmitter& emitter)
    {
        emitter.emit_block_start("struct alignas(32) event");
        for (const frontend::FieldDecl& field_decl : event_decl.fields)
        {
            emitter.emit_line(std::format("{} {};", frontend::type_string[static_cast<std::size_t>(field_decl.type)], field_decl.name));
        }
        emitter.emit_block_end(";");
    }
    // Note: Incase event contains a "string" the output struct will also contain a string.
    // It is the users responsibility to ensure string dtype exists, and that comparisons
    // are supported on those string without any additional functions.
    // Because of these limitations, string is discouraged and may even be removed later
    // unless a better method for handling strings is found.
}