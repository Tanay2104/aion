/* parser_pred.cpp
 * Implementation of the parsing functions for the predicates.
 * Created by Tanay Jha on 14 Feb 2026
 */

module aion.frontend;

import std;
import aion.core;

namespace aion::frontend
{
    std::optional<PredDecl> Parser::parse_pred_decl()
    {
        PredDecl pred_decl;
        if (peek().type != TokenType::KW_PRED)
        {
            ctxt.diagnostics.report_error(peek().location,"expected keyword 'pred'");
            return std::nullopt;
        }
        advance();

        if (peek().type != TokenType::IDENTIFIER)
        {
            ctxt.diagnostics.report_error(peek().location, "expected identifier");
            return std::nullopt;
        }

        pred_decl.name = peek().text;
        advance();

        if (peek().type != TokenType::EQUALS)
        {
            ctxt.diagnostics.report_error(peek().location,"expected left brace '='");
            return std::nullopt;
        }
        advance();


        auto predexpr = parse_predexpr();
        if (predexpr != nullptr)
        {
            pred_decl.expr = std::move(predexpr);
            ctxt.log(3, std::format("Found pred declaration: name {}", pred_decl.name));
        }
        else
        {
            synchronize();
        }

        // advance();
        if (peek().type != TokenType::SEMICOLON)
        {
            ctxt.diagnostics.report_error(peek().location,"expected ';'");
            return std::nullopt;
        }
        advance();

        ctxt.log(3, "Found complete predicate declaration");
        return pred_decl;
    }

    std::unique_ptr<PredExpr> Parser::parse_predexpr()
    {
        return parse_predexpr_or();
    }
    std::unique_ptr<PredExpr> Parser::parse_predexpr_or()
    {
        auto or_predexpr = std::make_unique<OrPredExpr>();
        // note: since we are pre allocating the predexpr, need to delete it in case of syntax error.
        // Oh, no I forgot that in these good days unique ptr manages memory automatically!


        // As per our grammar, the first predicate will be an AND.
        auto and_predexpr = Parser::parse_predexpr_and();

        if (and_predexpr != nullptr)
        {
            or_predexpr->terms.push_back(std::move(and_predexpr));
        }
        else
        {
            ctxt.diagnostics.report_error(peek().location, "expected expression");
            return nullptr;
        }

        while (peek().type == TokenType::LOGICAL_OR)
        {
            advance();

            and_predexpr = Parser::parse_predexpr_and();

            if (and_predexpr != nullptr)
            {
                or_predexpr->terms.push_back(std::move(and_predexpr));
            }
            else
            {
                ctxt.diagnostics.report_error(peek().location, "expected expression");
                return nullptr;
            }
        }
        if (or_predexpr->terms.size() == 1)
        {
            return std::move(or_predexpr->terms[0]);
            // hopefully we don't need to worry about memory issues cause unique ptr.
        }
        // advance();
        return or_predexpr;
    }

    std::unique_ptr<PredExpr> Parser::parse_predexpr_and()
    {
        auto and_predexpr = std::make_unique<AndPredExpr>();

        // As per our grammar, the first predicate will be an AND.
        auto not_predexpr = Parser::parse_predexpr_not();

        if (not_predexpr != nullptr)
        {
            and_predexpr->terms.push_back(std::move(not_predexpr));
        }
        else
        {
            ctxt.diagnostics.report_error(peek().location, "expected expression");
            return nullptr;
        }

        while (peek().type == TokenType::LOGICAL_AND)
        {
            advance();

            not_predexpr = Parser::parse_predexpr_not();

            if (not_predexpr != nullptr)
            {
                and_predexpr->terms.push_back(std::move(not_predexpr));
            }
            else
            {
                ctxt.diagnostics.report_error(peek().location, "expected expression");
                return nullptr;
            }
        }
        if (and_predexpr->terms.size() == 1)
        {
            return std::move(and_predexpr->terms[0]);
        }
        // advance();
        return and_predexpr;
    }

    std::unique_ptr<PredExpr> Parser::parse_predexpr_not()
    {

        if (peek().type == TokenType::LOGICAL_NOT)
        {
            auto not_predexpr = std::make_unique<NotExpr>();
            advance();
            auto nested_not_predexpr = Parser::parse_predexpr_not();
            if (nested_not_predexpr == nullptr)
            {
                ctxt.diagnostics.report_error(peek().location, "expected expression");
                return nullptr;
            }
            not_predexpr->inner = std::move(nested_not_predexpr);
            return not_predexpr;
        }
        else
        {
            return parse_predexpr_comp();
        }
    }

    std::unique_ptr<PredExpr> Parser::parse_predexpr_comp()
    {
        auto lhs = parse_predexpr_primary();
        if (lhs == nullptr)
        {
            ctxt.diagnostics.report_error(peek().location, "expected expression");
            return nullptr;
        }
        // checking for comparison.
        CompOp op;
        if (peek().type == TokenType::DOUBLE_EQUALS)
        {
            op = CompOp::EQ;
        }
        else if (peek().type == TokenType::GREATER)
        {
            op = CompOp::GT;
        }
        else if (peek().type == TokenType::LESS)
        {
            op = CompOp::LT;
        }
        else if (peek().type == TokenType::GREATER_EQUAL)
        {
            op = CompOp::GE;
        }
        else if (peek().type == TokenType::LESS_EQUAL)
        {
            op = CompOp::LE;
        }
        else if (peek().type == TokenType::NOT_EQUALS)
        {
            op = CompOp::NEQ;
        }
        else
        {
            // no operation.
            return lhs;
        }

        auto comp_predexpr = std::make_unique<CompPredExpr>();
        comp_predexpr->lhs = std::move(lhs);
        comp_predexpr->op = op;

        advance();

        auto rhs = parse_predexpr_primary();
        if (rhs == nullptr)
        {
            ctxt.diagnostics.report_error(peek().location, "expected expression");
            return nullptr;
        }

        comp_predexpr->rhs = std::move(rhs);
        // advance();
        return comp_predexpr;
    }

    std::unique_ptr<PrimaryPredExpr> Parser::parse_predexpr_primary()
    {
        auto primary_predexpr = std::make_unique<PrimaryPredExpr>();
        if (peek().type == TokenType::IDENTIFIER)
        {
            PredRefExpr ref_expr;
            ref_expr.name = peek().text;
            primary_predexpr->expr = ref_expr;
        }
        else if (peek().type == TokenType::LIT_CHAR)
        {
            Literal literal;
            literal.type = Type::CHAR;
            literal.value = (peek().text[0]);
            primary_predexpr->expr = std::move(literal);
        }
        else if (peek().type == TokenType::LIT_STRING)
        {
            Literal literal;
            literal.type = Type::STRING;
            literal.value = (peek().text);
            primary_predexpr->expr = std::move(literal);
        }
        else if (peek().type == TokenType::LIT_FLOAT)
        {
            Literal literal;
            literal.type = Type::FLOAT;
            float val;
            std::from_chars(peek().text.data(), peek().text.data() + peek().text.size(), val);
            literal.value = val;
            primary_predexpr->expr = std::move(literal);
        }
        else if (peek().type == TokenType::LIT_INTEGER)
        {
            Literal literal;
            literal.type = Type::INT;
            int val;
            std::from_chars(peek().text.data(), peek().text.data() + peek().text.size(), val, 10);
            literal.value = val;
            primary_predexpr->expr = std::move(literal);
        }
        else if (peek().type == TokenType::LPAREN)
        {
            advance();
            auto predexpr = Parser::parse_predexpr();
            if (predexpr == nullptr)
            {
                ctxt.diagnostics.report_error(peek().location, "expected expression");
                return nullptr;
            }
            // advance();
            if (peek().type != TokenType::RPAREN)
            {
                ctxt.diagnostics.report_error(peek().location, "expected ')'");
                return nullptr;
            }
            // advance();
            primary_predexpr->expr = std::move(predexpr);
        }
        advance();
        return primary_predexpr;
    }

};