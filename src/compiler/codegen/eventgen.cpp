/* eventgen.cpp
 * Code for emitting the event struct in a C header file.
 */

module aion.codegen;

import :eventgen;
import :cemitter;

namespace aion::codegen
{
    void emit_event(const frontend::EventDecl& event_decl,  const frontend::SymbolTable& symbol_table, CEmitter& emitter)
    {

        emitter.emit_block_start("struct alignas(32) Event");
        for (const frontend::FieldDecl& field_decl : event_decl.fields)
        {
            if ((symbol_table.resolve(field_decl.name) == nullptr) ||
                (!std::holds_alternative<frontend::FieldMetadata>(symbol_table.resolve(field_decl.name)->details)) ||
                (symbol_table.resolve(field_decl.name)->name != field_decl.name) ||
                (std::get<frontend::FieldMetadata>(symbol_table.resolve(field_decl.name)->details).type != field_decl.type)) {
                continue;
                // Invalid field, was rejected for some reason early on.
            }
            emitter.emit_line(std::format("{} {};", frontend::type_string[static_cast<std::size_t>(field_decl.type)], field_decl.name));
        }
        emitter.emit_block_end(";");
    }
}