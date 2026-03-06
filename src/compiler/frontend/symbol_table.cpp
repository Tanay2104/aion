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
        std::size_t inserted_event_fields = 0;
        std::size_t duplicate_event_fields = 0;
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
                ctxt.diagnostics.report_warning(std::format("Ignoring redeclaration of field \"{}\"", field.name));
                ++duplicate_event_fields;
            }
            else
            {
                ++inserted_event_fields;
                ctxt.log(3, std::format("[Symbols] event field '{}' type={} offset={} size={}",
                                        field.name,
                                        type_string[static_cast<std::uint8_t>(field.type)],
                                        details.offset,
                                        details.size_in_bytes));
            }
        }

        ctxt.log(2, std::format("[Symbols] Event fields: {} (inserted={}, duplicates={}, total_size={} bytes)",
                                ast.event.fields.size(), inserted_event_fields, duplicate_event_fields, offset));

        // now we start filling symbol table of predicates.
        std::size_t inserted_predicates = 0;
        std::size_t duplicate_predicates = 0;
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
                ctxt.diagnostics.report_warning(std::format("Ignoring redeclaration of predicate \"{}\"", pred.name));
                ++duplicate_predicates;
            }
            else
            {
                ++inserted_predicates;
                ctxt.log(3, std::format("[Symbols] predicate '{}'", pred.name));
            }
        }
        ctxt.log(2, std::format("[Symbols] Predicates: {} (inserted={}, duplicates={})",
                                ast.predicates.size(), inserted_predicates, duplicate_predicates));

        // now fill regex.
        std::size_t inserted_regexes = 0;
        std::size_t duplicate_regexes = 0;
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
                ctxt.diagnostics.report_warning(std::format("Ignoring redeclaration of regex \"{}\"", regex.name));
                ++duplicate_regexes;
            }
            else
            {
                ++inserted_regexes;
                ctxt.log(3, std::format("[Symbols] regex '{}'", regex.name));
            }
        }

        ctxt.log(2, std::format("[Symbols] Regexes: {} (inserted={}, duplicates={})",
                                ast.regexes.size(), inserted_regexes, duplicate_regexes));
        ctxt.log(2, std::format("[Symbols] Total inserted symbols: {}",
                                inserted_event_fields + inserted_predicates + inserted_regexes));

        return symbol_table;
    }
};
