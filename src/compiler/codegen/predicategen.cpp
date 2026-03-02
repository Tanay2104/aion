/* predicategen.cpp
 * Implementation of predicate code generation visitor.
 */

module aion.codegen;

import std;

namespace aion::codegen
{
    void emit_predicates(const frontend::AionFile& ast, const frontend::SymbolTable& symbol_table, CEmitter& emitter, const core::CompilationContext& ctxt)
    {
        for (auto& pred_decl : ast.predicates)
        {
            emitter.emit_block_start("inline bool " + function_prefix + std::string(pred_decl.name) + function_suffix);
            PredicateGenVisitor visitor(ctxt, symbol_table);
            pred_decl.expr->accept(visitor);
            std::string expression = visitor.get_pred_string();
            emitter.emit_line("return " + expression + ";");
            emitter.emit_block_end();
        }
    }

    PredicateGenVisitor::PredicateGenVisitor(const core::CompilationContext& _ctxt,
                           const frontend::SymbolTable& _symbol_table) : ctxt(_ctxt), symbol_table(_symbol_table) {}




    std::string PredicateGenVisitor::get_pred_string()
    {
        if (stack.size() != 1)
        {
            ctxt.diagnostics.report_internal_error("PredicateGenVisitor stacks size != 1");
        }
        return stack.top();
    }

    void PredicateGenVisitor::visit(const frontend::AndPredExpr& and_pred_expr)
    {
        std::size_t num_terms = and_pred_expr.terms.size();
        for (std::size_t i = num_terms; i > 0; --i)
        {

            and_pred_expr.terms[i-1]->accept(*this);
        }
        // Now the stack will contain num_term predicates
        // We need to && them.

        std::string expr_start = stack.top();
        stack.pop();

        std::string and_expr = "(" + expr_start;

        for (std::size_t i = 1; i < num_terms; ++i)
        {
            std::string expr = stack.top();
            stack.pop();
            and_expr += (" && " + expr);
        }
        and_expr += ")";

        stack.push(and_expr);
    }


    void PredicateGenVisitor::visit(const frontend::OrPredExpr& or_pred_expr)
    {
        std::size_t num_terms = or_pred_expr.terms.size();
        for (std::size_t i = num_terms; i > 0; --i)
        {
            or_pred_expr.terms[i-1]->accept(*this);
        }
        // Now the stack will contain num_term predicates
        // We need to "||" them.

        const std::string expr_start = stack.top();
        stack.pop();

        std::string or_expr = "(" + expr_start;

        for (std::size_t i = 1; i < num_terms; ++i)
        {
            std::string expr = stack.top();
            stack.pop();
            or_expr += (" || " + expr);
        }
        or_expr += ")";

        stack.push(or_expr);
    }

    void PredicateGenVisitor::visit(const frontend::NotExpr& not_pred_expr)
    {
        not_pred_expr.inner->accept(*this);

        const std::string inner =  stack.top();
        stack.pop();

        const std::string not_expr = "(!" + inner + ")";
        stack.push(not_expr);
    }

    void PredicateGenVisitor::visit(const frontend::CompPredExpr& comp_pred_expr)
    {
        comp_pred_expr.lhs->accept(*this);
        std::string lhs = stack.top();
        stack.pop();

        comp_pred_expr.rhs->accept(*this);
        std::string rhs = stack.top();
        stack.pop();

        std::string comp_expr = "(" + lhs;

        switch (comp_pred_expr.op)
        {
            case frontend::CompOp::EQ:
                comp_expr += ("==" + rhs);
                break;
            case frontend::CompOp::GT:
                comp_expr += (">" + rhs);
                break;
            case frontend::CompOp::LT:
                comp_expr += ("<" + rhs);
                break;
            case frontend::CompOp::GE:
                comp_expr += (">=" + rhs);
                break;
            case frontend::CompOp::LE:
                comp_expr += ("<=" + rhs);
                break;
            case frontend::CompOp::NEQ:
                comp_expr += ("!=" + rhs);
                break;
        }
        comp_expr += ")";
        stack.push(comp_expr);
    }


    void PredicateGenVisitor::visit(const frontend::PrimaryPredExpr& primary_pred_expr)
    {
        std::string primary_expr = "(";
        std::visit([this, &primary_expr](const auto& arg) {

            // get the raw type of the argument.
            using T = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<T, frontend::Literal>)
            {
                switch (arg.type)
                {
                    case core::Type::BOOL:
                        if (std::get<bool>(arg.value) == true)
                        {
                            primary_expr += "true";
                        }
                        else
                        {
                            primary_expr += "false";
                        }
                        break;

                    case core::Type::INT:
                        primary_expr += std::to_string(std::get<int>(arg.value));
                        break;
                    case core::Type::FLOAT:
                        primary_expr += std::to_string(std::get<float>(arg.value));
                        break;
                    case core::Type::CHAR:
                        primary_expr += "'" + std::string{std::get<char>(arg.value)} + "'";
                        break;
                    case core::Type::STRING:
                        primary_expr += "\"" + std::string(std::get<std::string_view>(arg.value)) + "\"";
                        break;
                }
            }
            else if constexpr (std::is_same_v<T, std::unique_ptr<frontend::PredExpr>>) {
                arg->accept(*this);
                std::string refexpr = stack.top();
                stack.pop();
                primary_expr += "(" + refexpr + ")";
            }
            else if constexpr (std::is_same_v<T, frontend::PredRefExpr>) {
                arg.accept(*this);
                // This pushed something onto a stack.
                std::string refexpr = stack.top();
                stack.pop();
                primary_expr += refexpr;
            }

        }, primary_pred_expr.expr);

        primary_expr += ")";
        stack.push(primary_expr);
    }
    void PredicateGenVisitor::visit(const frontend::PredRefExpr& pred_ref_expr)
    {
        // Critical note:
        // We assume the input is well-formed. Like the predicates will be
        // declared in correct order, and the boolean variable is correct.
        // Instead of using the evaluation function, we can directly use
        // boolean variables.

        // A difficult part is to identify whether the given identifer/name
        // refers to a predicate or member variable of event. We use the symbol
        // table for this. This method is rather brittle. Maybe I should think of something else.

        const frontend::Symbol* symbol = symbol_table.resolve(pred_ref_expr.name);
        if (symbol == nullptr)
        {
            ctxt.diagnostics.report_internal_error("PredRefExpr PredicateGenvisitor");
            return;
        }
        if (symbol->kind == frontend::SymbolKind::PREDICATE)
        {
            std::string refexpr = function_prefix+std::string(pred_ref_expr.name)+function_call_suffix;
            stack.push(refexpr);
        }
        else if (symbol->kind == frontend::SymbolKind::EVENT_FIELD)
        {
            std::string refexpr = event_name + "." + std::string(pred_ref_expr.name);
            stack.push(refexpr);
        }
    }
};