/* parser.cpp
 * Implementation of the recursive descent parser.
 * Created on 7 Feb 2026 by Tanay Jha
 * NOTE: Mostly if we encounter an error we exit(1)
 * This is unideal and needs to be improved later.
 */

module aion.frontend;

import std;
import aion.core;

namespace  aion::frontend
{
    Token Parser::advance()
    {
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

    std::unique_ptr<AionFile>  Parser::parse()
    {
        root = std::make_unique<AionFile>();
        current = 0;
        root->event = parse_event_decl();
        return std::move(root);
    }
};