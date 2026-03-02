/* symbol_table.cpp
 * Implementations of functions needed to make symbol table along with helpers
 */

module aion.frontend;

import std;
import aion.core;

namespace aion::frontend
{
    [[nodiscard]] bool SymbolTable::define(const Symbol& sym)
    {
        auto [it, inserted] = symbols.insert({sym.name, sym});
        return inserted;
    }

    [[nodiscard]] const Symbol* SymbolTable::resolve(std::string_view name) const
    {
        if (symbols.contains(name))
        {
            return &symbols.at(name);
        }
        return nullptr;
    }

    [[nodiscard]] Symbol* SymbolTable::mod_resolve(std::string_view name)
    {
        if (symbols.contains(name))
        {
            return &symbols.at(name);
        }
        return nullptr;
    }

    SymbolTable generate_symbol_table(const AionFile& ast, core::CompilationContext &ctxt)
    {
        SymbolTable symbol_table;


        std::uint32_t offset = 0; // in bytes
        for (const FieldDecl& field: ast.event.fields)
        {
            FieldMetadata details;
            details.type = field.type;
            details.offset = offset;
            switch (field.type)
            {
                case core::Type::BOOL:
                    details.size_in_bytes = 1;
                    offset += 1;
                    break;
                case core::Type::CHAR:
                    details.size_in_bytes = 1;
                    offset += 1;
                    break;
                case core::Type::INT:
                    details.size_in_bytes = 4;
                    offset += 4;
                    break;
                case core::Type::FLOAT:
                    details.size_in_bytes = 4;
                    offset += 4;
                    break;
                case core::Type::STRING:
                        details.size_in_bytes = core::MAX_STRING_SIZE;
                        offset += core::MAX_STRING_SIZE;
                    break;
                default:
                    // hopefully nothing happens here.
                    // parser would have noticed otherwise.
                    ctxt.diagnostics.report_internal_error("Generating symbol table for unknown type");
                    break;
            }

            bool success = symbol_table.define({
                .name = field.name,
                .kind = SymbolKind::EVENT_FIELD,
                .details = details, // maybe move here? But idk doesn't make much sense for POD.
            });
            if (!success)
            {
                ctxt.diagnostics.report_warning({std::numeric_limits<std::size_t>::max(), std::numeric_limits<std::size_t>::max()},
                    std::format("Ignoring redeclaration of field \"{}\"", field.name));
            }
        }

        ctxt.log(3, "Filled symbol table for event");

        // now we start filling symbol table of predicates.
        for (const PredDecl& pred: ast.predicates)
        {
            PredicateMetadata details;
            details.ast_node = &pred;

            bool success = symbol_table.define({
                .name = pred.name,
                .kind = SymbolKind::PREDICATE,
                .details = details,
            });

            if (!success)
            {
                ctxt.diagnostics.report_warning({std::numeric_limits<std::size_t>::max(), std::numeric_limits<std::size_t>::max()},
                    std::format("Ignoring redeclaration of predicate \"{}\"", pred.name));
            }
        }
        ctxt.log(3, "Filled symbol table for predicates");

        // now fill regex.
        for (const RegexDecl& regex: ast.regexes)
        {
            RegexMetadata details;
            details.ast_node = &regex;

            bool success = symbol_table.define({
            .name = regex.name,
            .kind = SymbolKind::REGEX,
            .details = details,});

            if (!success)
            {
                ctxt.diagnostics.report_warning({std::numeric_limits<std::size_t>::max(), std::numeric_limits<std::size_t>::max()},
                    std::format("Ignoring redeclaration of regex \"{}\"", regex.name));
            }
        }

        ctxt.log(3, "Filled symbol table for all regexes");

        return symbol_table;
    }
};