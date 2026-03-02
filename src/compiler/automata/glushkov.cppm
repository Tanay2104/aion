/* glushkov.cppm
 * Implements the glushkov algorithm
 * Takes in the AST and produces the NFA object
 */
export module aion.automata:glushkov;

import std;
import aion.frontend;
import aion.core;
import :nfa;

namespace aion::automata
{
    export std::unordered_map<std::string_view, Generic_NFA> convert_to_generic_nfa(const frontend::AionFile& ast, const frontend::SymbolTable& table, const core::CompilationContext& ctxt);

    class GlushkovVisitor : public frontend::RegexVisitor {
    private:
        struct GlushkovFragment {
            bool nullable{};
            std::unordered_set<std::uint16_t> first{}; // List of PosIDs
            std::unordered_set<std::uint16_t> last{};  // List of PosIDs
        };
        std::stack<GlushkovFragment> stack;

        // Access to the regex info.
        const frontend::RegexMetadata& meta;
        const core::CompilationContext& ctxt;
        std::uint16_t num_states;
        // Our global follow set.
        std::unordered_map<std::uint16_t, std::unordered_set<std::uint16_t>> follow;

        void add_character(const frontend::RegexExpr* node);
    public:
        explicit GlushkovVisitor(const frontend::RegexMetadata& _meta, const core::CompilationContext& _ctxt);

        void visit(const frontend::RegexPrimary& node) override;
        void visit(const frontend::RegexUnion& node) override;
        void visit(const frontend::RegexWildcard& node) override;
        void visit(const frontend::RegexStar& node) override;
        void visit(const frontend::RegexRefExpr& node) override;
        void visit(const frontend::RegexConcat& node) override;

        Generic_NFA get_generic_NFA();
    };
};