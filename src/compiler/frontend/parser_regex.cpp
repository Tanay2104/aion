/* parser_regex.cpp
 * Implementation of the parsing functions for the regular expressions.
 * Created by Tanay Jha on 26 Feb 2026
 */

module aion.frontend;

import std;
import aion.core;

namespace aion::frontend
{
    std::optional<RegexDecl> Parser::parse_regex_decl()
    {
        RegexDecl regex_decl;
        if (peek().type != TokenType::KW_REGEX)
        {
            ctxt.diagnostics.report_error(peek().location,"expected keyword 'regex'");
            return std::nullopt;
        }
        advance();
        if (peek().type != TokenType::IDENTIFIER)
        {
            ctxt.diagnostics.report_error(peek().location,"expected identifier for regex");
            return std::nullopt;
        }
        regex_decl.name = peek().text;
        ctxt.log(3, std::format("[Parse] Parsing regex '{}'", regex_decl.name));
        // std::println("regex name = {}", regex_decl.name);
        advance();

        if (peek().type != TokenType::EQUALS)
        {
            ctxt.diagnostics.report_error(peek().location,"expected token '='");
            return std::nullopt;
        }
        advance();

        auto regexpr = parse_regex();
        if (regexpr != nullptr)
        {
            regex_decl.expr = std::move(regexpr);
            ctxt.log(3, std::format("[Parse] regex AST built for '{}'", regex_decl.name));
        }
        else
        {
            return std::nullopt;
        }
        // no advance here means that we are expecting parse regex to advance
        if (peek().type != TokenType::SEMICOLON)
        {
            ctxt.diagnostics.report_error(peek().location,"expected ';'");
            return std::nullopt;
        }
        advance();

        ctxt.log(3, std::format("[Parse] Regex '{}' complete", regex_decl.name));
        return regex_decl;
    }

    std::unique_ptr<RegexExpr> Parser::parse_regex()
    {
        return parse_regex_alt();
    }

    std::unique_ptr<RegexExpr> Parser::parse_regex_alt()
    {
        auto alt_regexpr = std::make_unique<RegexUnion>();

        // As per our grammar, the first term will be a concat.
        auto concat_regexpr = parse_regex_concat();

        if (concat_regexpr != nullptr)
        {
            alt_regexpr->options.push_back(std::move(concat_regexpr));
        }
        else
        {
            return nullptr;
        }

        while (peek().type == TokenType::UNION)
        {
            advance();

            concat_regexpr = parse_regex_concat();
            if (concat_regexpr != nullptr)
            {
                alt_regexpr->options.push_back(std::move(concat_regexpr));
            }
            else
            {
                return nullptr;
            }
        }

        // if there is only one element, we "shorten" the tree.
        if (alt_regexpr->options.size() == 1)
        {
            return std::move(alt_regexpr->options[0]);
        }
        return alt_regexpr;
    }

    std::unique_ptr<RegexExpr> Parser::parse_regex_concat()
    {
        auto concat_regexpr = std::make_unique<RegexConcat>();

        // as per grammar, the first predicate will be a regex_unary
        auto unary_regexpr  = parse_regex_unary();

        if (unary_regexpr != nullptr)
        {
            concat_regexpr->sequence.push_back(std::move(unary_regexpr));
        }
        else
        {
            return nullptr;
        }

        while (peek().type == TokenType::DOT)
        {
            advance();
            unary_regexpr = parse_regex_unary();

            if (unary_regexpr != nullptr)
            {
                concat_regexpr->sequence.push_back(std::move(unary_regexpr));
            }
            else
            {
                return nullptr;
            }
        }

        if (concat_regexpr->sequence.size() == 1)
        {
            return std::move(concat_regexpr->sequence[0]);
        }
        return concat_regexpr;
    }

    std::unique_ptr<RegexExpr> Parser::parse_regex_unary()
    {
        // regex unary contains regex primary as first.
        auto regex_primary = parse_regex_primary();

        if (regex_primary == nullptr)
        {
            return nullptr;
        }
        // let's say the primary advances before returning.
        // check for kleen star
        if (peek().type == TokenType::STAR)
        {
            auto regex_with_star = std::make_unique<RegexStar>();
            regex_with_star->inner = std::move(regex_primary);
            advance();
            return regex_with_star;
        }
        return regex_primary;
    }

    std::unique_ptr<RegexPrimary> Parser::parse_regex_primary()
    {
        auto primary_regexpr = std::make_unique<RegexPrimary>();
        // Check for wildcard.

        if (peek().type == TokenType::ANY)
        {
            primary_regexpr->expr = std::make_unique<RegexWildcard>();
        }
        // check identifier
        else if (peek().type == TokenType::IDENTIFIER)
        {
            // Critical note: This mixes predicates with RegexRefExpr
            primary_regexpr->expr = peek().text;
        }
        else if (peek().type == TokenType::LPAREN)
        {
            advance();
            auto regexpr = parse_regex();
            if (regexpr == nullptr)
            {
                return nullptr;
            }
            if (peek().type != TokenType::RPAREN)
            {
                ctxt.diagnostics.report_error(peek().location,"expected ')'");
                return nullptr;
            }
            primary_regexpr->expr = std::move(regexpr);
        }
        else
        {
            // ctxt.diagnostics.report_error(peek().location, "unknown expression encountered");
            ctxt.diagnostics.report_error(peek().location,"expected regular expression");
            return nullptr;
        }
        advance();
        return primary_regexpr;

    }
}
