/* parser_event.cpp
* Implementation of the parsing of the event.
 * Created on 7 Feb 2026 by Tanay Jha
 */

module aion.frontend;

import std;
import aion.core;

namespace aion::frontend
{
 FieldDecl Parser::parse_field_decl()
    {
        FieldDecl field;
        // check for identifier.
        if (peek().type != TokenType::IDENTIFIER)
        {
            ctxt.diagnostics.report_error(peek().location,"expected identifier for data type");
            std::exit(1);
        }
        // see the dtype.
        std::string_view type_text = peek().text;
        if (type_text == "int")
        {
            field.type = Type::INT;
        }
        else if (type_text == "float")
        {
            field.type = Type::FLOAT;
        }
        else if (type_text == "char")
        {
            field.type = Type::CHAR;
        }
        else if (type_text == "string")
        {
            field.type = Type::STRING;
        }
        else
        {
            ctxt.diagnostics.report_error(peek().location,"Unknown data type");
            std::exit(1);
        }

        advance();
        // now check for identifier, which is the name.
        if (peek().type != TokenType::IDENTIFIER)
        {
            ctxt.diagnostics.report_error(peek().location,"expected identifier for variable name");
            std::exit(1);
        }

        field.name = peek().text;
        advance();

        // check for semicolon.
        if (peek().type != TokenType::SEMICOLON)
        {
            ctxt.diagnostics.report_error(peek().location,"expected ';'");
            std::exit(1);
        }
        advance();
     return field;
    }
    EventDecl Parser::parse_event_decl()
    {
        EventDecl event_decl;
        if (peek().type != TokenType::KW_EVENT)
        {
            ctxt.diagnostics.report_error(peek().location,"expected keyword 'event'");
            std::exit(1);
        }
        advance();
        if (peek().type != TokenType::LBRACE)
        {
            ctxt.diagnostics.report_error(peek().location,"expected left brace '{'");
            std::exit(1);
        }
        advance();
        while (peek().type != TokenType::RBRACE)
        {
            event_decl.fields.push_back(parse_field_decl());
            ctxt.log(3, std::format("Found field declaration: varname {} dtype {}", event_decl.fields.back().name,  type_string[static_cast<std::uint8_t>(event_decl.fields.back().type)]));
        }
        advance();
        if (peek().type != TokenType::SEMICOLON)
        {
            ctxt.diagnostics.report_error(peek().location,"expected ';'");
            std::exit(1);
        }
        advance();
        ctxt.log(3, "Found complete event declaration");
        return event_decl;
    }
}
