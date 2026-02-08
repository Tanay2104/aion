/* parser.cpp
 * Implementation of the recursive descent parser.
 * Created on 7 Feb 2026 by Tanay Jha
 */

module aion.frontend;

import std;
import aion.core;

namespace  aion::frontend
{
    Token Parser::advance()
    {
        if (is_at_end()) return tokens[current-1];
        return tokens[current++];
    }
    Token Parser::peek() const
    {
        return tokens[current];
    }

    Token Parser::peek_next() const
    {
        return tokens[current+1];
    }

    bool Parser::is_at_end() const
    {
        return (current == tokens.size());
    }

    Token Parser::previous() const
    {
        return tokens[current-1];
    }

    void Parser::synchronize()
    {
        advance();
        while (!is_at_end())
        {
            if (previous().type == TokenType::SEMICOLON)
            {
                // we just passed a semicolon, so safe.
                return;
            }
            switch (peek().type)
            {
                case TokenType::KW_EVENT:
                case TokenType::KW_PRED:
                case TokenType::KW_REGEX:
                    return; // safe to resume parsing.
                default:
                ;
            }
            advance();
        }
    }

    std::unique_ptr<AionFile>  Parser::parse()
    {
        root = std::make_unique<AionFile>();
        current = 0;

        const auto& event_decl = parse_event_decl();
        if (event_decl.has_value())
        {
            root->event = event_decl.value();
        }
        else
        {
            ctxt.log(2, "No valid event found", aion::core::YELLOW);
            synchronize();
        }
        return std::move(root);
    }
};