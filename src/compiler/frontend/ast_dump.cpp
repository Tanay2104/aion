/* ast_dump.cpp
 * Visitor functions for printing the AST
 * Created by ChatGPT on 15 Feb 2026
 */

module aion.frontend;

import :ast;
import std;

namespace aion::frontend
{
    static constexpr std::array<std::string_view, 6> compop_string{
        "==", "!=", "<", "<=", ">", ">="
    };

    struct AstPrinter
    {
        std::vector<bool> branches;
        std::ostream& out;
        /* ───────────────────────────── */
        AstPrinter(std::ostream& _out) : out(_out) {}
        void prefix() const
        {
            for (std::size_t i = 0; i + 1 < branches.size(); ++i)
            {
                if (branches[i]) std::print(out,"│  ");
                else             std::print(out, "   ");
            }

            if (!branches.empty())
            {
                if (branches.back()) std::print(out,"├─ ");
                else                 std::print(out,"└─ ");
            }
        }

        void push(bool more) { branches.push_back(more); }
        void pop() { branches.pop_back(); }

        /* ───────────────────────────── */

        void visit(const AionFile& file)
        {
            std::println(out, "AionFile");

            push(true);
            visit(file.event);
            pop();

            for (std::size_t i = 0; i < file.predicates.size(); ++i)
            {
                bool more = (i + 1 < file.predicates.size()) ||
                            !file.regexes.empty();
                push(more);
                visit(file.predicates[i]);
                pop();
            }

            for (std::size_t i = 0; i < file.regexes.size(); ++i)
            {
                push(i + 1 < file.regexes.size());
                visit(file.regexes[i]);
                pop();
            }
        }

        /* ───────────────────────────── */
        /* Event */

        void visit(const EventDecl& e)
        {
            prefix();
            std::println(out,"EventDecl");

            for (std::size_t i = 0; i < e.fields.size(); ++i)
            {
                push(i + 1 < e.fields.size());
                visit(e.fields[i]);
                pop();
            }
        }

        void visit(const FieldDecl& f)
        {
            prefix();
            std::println(out,"FieldDecl");

            push(true);
            prefix();
            std::println(out, "type: {}",
                type_string[static_cast<std::size_t>(f.type)]);
            pop();

            push(false);
            prefix();
            std::println(out, "name: {}", f.name);
            pop();
        }

        /* ───────────────────────────── */
        /* PredDecl */

        void visit(const PredDecl& d)
        {
            prefix();
            std::println(out, "PredDecl \"{}\"", d.name);

            push(false);
            visit(*d.expr);
            pop();
        }

        /* ───────────────────────────── */
        /* PredExpr */

        void visit(const PredExpr& expr)
        {
            if (auto* andp = dynamic_cast<const AndPredExpr*>(&expr))
                visit(*andp);
            else if (auto* orp = dynamic_cast<const OrPredExpr*>(&expr))
                visit(*orp);
            else if (auto* notp = dynamic_cast<const NotExpr*>(&expr))
                visit(*notp);
            else if (auto* comp = dynamic_cast<const CompPredExpr*>(&expr))
                visit(*comp);
            else if (auto* prim = dynamic_cast<const PrimaryPredExpr*>(&expr))
                visit(*prim);
        }

        void visit(const AndPredExpr& e)
        {
            prefix();
            std::println(out ,"AndPredExpr");

            for (std::size_t i = 0; i < e.terms.size(); ++i)
            {
                push(i + 1 < e.terms.size());
                visit(*e.terms[i]);
                pop();
            }
        }

        void visit(const OrPredExpr& e)
        {
            prefix();
            std::println(out, "OrPredExpr");

            for (std::size_t i = 0; i < e.terms.size(); ++i)
            {
                push(i + 1 < e.terms.size());
                visit(*e.terms[i]);
                pop();
            }
        }

        void visit(const NotExpr& e)
        {
            prefix();
            std::println(out, "NotExpr");

            push(false);
            visit(*e.inner);
            pop();
        }

        void visit(const CompPredExpr& e)
        {
            prefix();
            std::println(out, "CompPredExpr ({})",
                compop_string[static_cast<std::size_t>(e.op)]);

            push(true);
            visit(*e.lhs);
            pop();

            push(false);
            visit(*e.rhs);
            pop();
        }

        void visit(const PrimaryPredExpr& e)
        {
            std::visit([this](auto const& val)
            {
                using T = std::decay_t<decltype(val)>;

                if constexpr (std::is_same_v<T, PredRefExpr>)
                {
                    prefix();
                    std::println(out, "PredRefExpr \"{}\"", val.name);
                }
                else if constexpr (std::is_same_v<T,
                         std::unique_ptr<PredExpr>>)
                {
                    visit(*val);
                }
                else if constexpr (std::is_same_v<T, Literal>)
                {
                    prefix();
                    std::print(out, "Literal {}(",
                        type_string[
                            static_cast<std::size_t>(val.type)
                        ]);

                    std::visit([this](auto const& v)
                    {
                        using V = std::decay_t<decltype(v)>;

                        if constexpr (std::is_same_v<V, char>)
                            std::print(out, "'{}'", v);
                        else if constexpr (std::is_same_v<V, std::string_view>)
                            std::print(out,"\"{}\"", v);
                        else
                            std::print(out, "{}", v);

                    }, val.value);

                    std::println(out, ")");
                }

            }, e.expr);
        }

        /* ───────────────────────────── */
        /* RegexDecl */

        void visit(const RegexDecl& d)
        {
            prefix();
            std::println(out, "RegexDecl \"{}\"", d.name);

            push(false);
            visit(*d.expr);
            pop();
        }

        /* ───────────────────────────── */
        /* RegexExpr */

        void visit(const RegexExpr& expr)
        {
            if (auto* uni = dynamic_cast<const RegexUnion*>(&expr))
                visit(*uni);
            else if (auto* con = dynamic_cast<const RegexConcat*>(&expr))
                visit(*con);
            else if (auto* star = dynamic_cast<const RegexStar*>(&expr))
                visit(*star);
            else if (auto* ref = dynamic_cast<const RegexRefExpr*>(&expr))
                visit(*ref);
            else if (auto* prim = dynamic_cast<const RegexPrimary*>(&expr))
                visit(*prim);
            else if (dynamic_cast<const RegexWildcard*>(&expr))
            {
                prefix();
                std::println(out, "RegexWildcard");
            }
        }

        void visit(const RegexUnion& e)
        {
            prefix();
            std::println(out, "RegexUnion");

            for (std::size_t i = 0; i < e.options.size(); ++i)
            {
                push(i + 1 < e.options.size());
                visit(*e.options[i]);
                pop();
            }
        }

        void visit(const RegexConcat& e)
        {
            prefix();
            std::println(out, "RegexConcat");

            for (std::size_t i = 0; i < e.sequence.size(); ++i)
            {
                push(i + 1 < e.sequence.size());
                visit(*e.sequence[i]);
                pop();
            }
        }

        void visit(const RegexStar& e)
        {
            prefix();
            std::println(out, "RegexStar");

            push(false);
            visit(*e.inner);
            pop();
        }

        void visit(const RegexRefExpr& e)
        {
            prefix();
            std::println(out, "RegexRefExpr \"{}\"", e.regex_ref_expr);
        }
        void visit(const RegexPrimary& e)
        {
            std::visit([this](auto const& val)
            {
                using T = std::decay_t<decltype(val)>;

                if constexpr (std::is_same_v<T, std::string_view>)
                {
                    prefix();
                    std::println(out, "RegexPrimary PredicateRef \"{}\"", val);
                }
                else if constexpr (std::is_same_v<T, RegexWildcard>)
                {
                    prefix();
                    std::println(out, "RegexPrimary");
                    push(false);
                    prefix();
                    std::println(out, "RegexWildcard");
                    pop();
                }
                else if constexpr (std::is_same_v<T,
                             std::unique_ptr<RegexExpr>>)
                {
                    prefix();
                    std::println(out, "RegexPrimary (group)");
                    push(false);
                    visit(*val);
                    pop();
                }

            }, e.expr);
        }
    };

    /* ───────────────────────────── */

    void dump_ast(const AionFile& ast, const core::CompilationContext& ctx)
    {
        if (ctx.options.output_filename != core::DEFAULT_NAME)
        {
            std::ofstream output_file;
            output_file.open(ctx.options.output_filename);
            AstPrinter p(output_file);
            p.visit(ast);
            output_file.close();
        }
        else
        {
            AstPrinter p(std::cout);
            p.visit(ast);
        }
    }
}
