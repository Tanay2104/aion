/* alphabet.cpp
* Implementation of alphabet.cppm
*/

module aion.analysis;

import aion.core;
import aion.frontend;
import std;

namespace aion::analysis
{
    AlphabetVisitor::AlphabetVisitor(frontend::SymbolTable& _table, std::string_view _regex, core::CompilationContext& _ctxt) :
        current_pos_id(1), table(_table), rsymbol(table.mod_resolve(_regex)), ctxt(_ctxt)
    {
        rsymbol = table.mod_resolve(_regex);
        if (!rsymbol) {
            // This should theoretically be unreachable if Parser+SymbolTable did their job,
            // but never trust the previous pass in C++ haha.
            ctxt.diagnostics.report_internal_error(std::format("AlphabetVisitor could not resolve regex '{}'", _regex));
        }
    }
    void AlphabetVisitor::register_node(const frontend::RegexExpr* node, std::string_view name) {
        auto& meta = std::get<frontend::RegexMetadata>(rsymbol->details);
        if (current_pos_id >= aion::core::MAX_STATES) {
            if (!max_states_error_given)
            {
                ctxt.diagnostics.report_error( "Regex too long. Does not fit in 64 bits. Ignoring trailing symbols.");
                max_states_error_given = true;
            }
            return;
            // Really not much point in continuing now.
        }
        meta.pos_ids_to_names[current_pos_id] = std::string(name);
        meta.node_to_pos_ids[node] = current_pos_id;
        ctxt.log(3, std::format("Mapped identifier {} address {} to id {}", name, static_cast<const void*>(node), current_pos_id));
        current_pos_id++;
    }

    void AlphabetVisitor::visit(const frontend::RegexUnion& regex_union)
    {
        for (const auto& regexpr : regex_union.options)
        {
            regexpr->accept(*this);
        }
    }
    void AlphabetVisitor::visit(const frontend::RegexConcat& regex_concat)
    {
        for (const auto& regexpr : regex_concat.sequence)
        {
            regexpr->accept(*this);
        }
    }
    void AlphabetVisitor::visit(const frontend::RegexStar& regex_star)
    {
        regex_star.inner->accept(*this);
    }
    void AlphabetVisitor::visit(const frontend::RegexRefExpr& regex_ref_expr)
    {
        register_node(&regex_ref_expr, regex_ref_expr.regex_ref_expr);
    }
    void AlphabetVisitor::visit(const frontend::RegexWildcard& regex_wildcard)
    {
        // Similiar to regex_ref_expr. Though we can optimise for wildcard,
        // I think first we should make a fully working model.
        register_node(&regex_wildcard, "_");
    }
    void AlphabetVisitor::visit(const frontend::RegexPrimary& regex_primary)
    {
            std::visit([this, &regex_primary](const auto& arg) {

                // get the raw type of the argument.
                using T = std::decay_t<decltype(arg)>;

                if constexpr (std::is_same_v<T, std::string_view>) {
                    // It is a predicate_reference.
                    register_node(&regex_primary, arg);
                }
                else if constexpr (std::is_same_v<T, frontend::RegexWildcard>) {
                    arg.accept(*this);
                }
                else if constexpr (std::is_same_v<T, std::unique_ptr<frontend::RegexExpr>>) {
                    arg->accept(*this);
                }

            }, regex_primary.expr);
    }


    void fill_pos_ids(frontend::SymbolTable& table, const frontend::AionFile& ast, core::CompilationContext& ctxt)
    {
        for ( const frontend::RegexDecl& regex_decl : ast.regexes)
        {
            AlphabetVisitor alphabet_visitor(table, regex_decl.name, ctxt);
            regex_decl.expr->accept(alphabet_visitor);
        }
    }

};