/* parser_event.cpp
* Implementation of the parsing of the event.
 * Created on 7 Feb 2026 by Tanay Jha
 */

module aion.frontend;

import std;
import aion.core;

namespace aion::frontend
{
 std::optional<FieldDecl> Parser::parse_field_decl()
    {
        FieldDecl field;
        // check for identifier.
        if (peek().type != TokenType::IDENTIFIER)
        {
            ctxt.diagnostics.report_error(peek().location,"expected identifier for data type");
            return std::nullopt;
        }
        // see the dtype.
        std::string_view type_text = peek().text;
        if (type_text == "int")
        {
            field.type = core::Type::INT;
        }
        else if (type_text == "float")
        {
            field.type = core::Type::FLOAT;
        }
        else if (type_text == "char")
        {
            field.type = core::Type::CHAR;
        }
        else if (type_text == "string")
        {
            field.type = core::Type::STRING;
        }
        else if (type_text == "bool")
        {
            field.type = core::Type::BOOL;
        }
        else
        {
            ctxt.diagnostics.report_error(peek().location,"unknown data type");
            return std::nullopt;
        }

        advance();
        // now check for identifier, which is the name.
        if (peek().type != TokenType::IDENTIFIER)
        {
            ctxt.diagnostics.report_error(peek().location,"expected identifier for variable name");
            return std::nullopt;
        }

        field.name = peek().text;
        advance();

        // check for semicolon.
        if (peek().type != TokenType::SEMICOLON)
        {
            ctxt.diagnostics.report_error(peek().location,"expected ';'");
            return std::nullopt;
        }
        advance();
     return field;
    }
   std::optional<EventDecl> Parser::parse_event_decl()
    {
        EventDecl event_decl;
        if (peek().type != TokenType::KW_EVENT)
        {
            ctxt.diagnostics.report_error(peek().location,"expected keyword 'event'");
            return std::nullopt;
        }
        advance();
        if (peek().type != TokenType::LBRACE)
        {
            ctxt.diagnostics.report_error(peek().location,"expected left brace '{'");
            return std::nullopt;
        }
        advance();
        while (peek().type != TokenType::RBRACE)
        {
            const auto& field_decl = parse_field_decl();
            if (field_decl.has_value())
            {
                event_decl.fields.push_back(field_decl.value());
                ctxt.log(3, std::format("Found field declaration: varname {} dtype {}", event_decl.fields.back().name,  type_string[static_cast<std::uint8_t>(event_decl.fields.back().type)]));
            }
            else
            {
                synchronize();
            }
        }
        advance();
        if (peek().type != TokenType::SEMICOLON)
        {
            ctxt.diagnostics.report_error(peek().location,"expected ';'");
            return std::nullopt;
        }
        advance();
        ctxt.log(3, "Found complete event declaration");
        return event_decl;
    }
}
