/* alphabet.cppm
 * Visitor which assigns unique alphabet id's to each
 * predicate/regex/wildcard identifier in the regex
 */

export module aion.analysis:alphabet;

import aion.core;
import aion.frontend;
import std;

namespace aion::analysis
{
    export void fill_pos_ids(frontend::SymbolTable& table, const frontend::AionFile& ast, core::CompilationContext& ctxt);

    class AlphabetVisitor : public frontend::RegexVisitor
    {
    private:
        std::uint16_t current_pos_id;
        frontend::SymbolTable& table;
        frontend::Symbol* rsymbol;
        core::CompilationContext& ctxt;
        // void visit(const frontend::RegexExpr&) override;
    public:
        explicit AlphabetVisitor(frontend::SymbolTable& _table, std::string_view _regex, core::CompilationContext& _ctxt);

        void register_node(const frontend::RegexExpr*, std::string_view);
        void visit(const frontend::RegexUnion&) override;
        void visit(const frontend::RegexConcat&) override;
        void visit(const frontend::RegexStar&) override;
        void visit(const frontend::RegexRefExpr&) override;
        void visit(const frontend::RegexWildcard&) override;
        void visit(const frontend::RegexPrimary&) override;
    };
};
