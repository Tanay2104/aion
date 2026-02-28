/* glushkov.cppm
 * Implements the glushkov algorithm
 * Takes in the AST and produces the NFA object
 */
export module aion.automata:glushkov;

import std;
import aion.frontend;

namespace aion::automata
{
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

        // Our global follow set.
        std::unordered_map<std::uint16_t, std::unordered_set<std::uint16_t>> follow;

        void add_character(const frontend::RegexExpr* node);
    public:
        explicit GlushkovVisitor(frontend::RegexMetadata& _meta, core::CompilationContext& _ctxt);

        void visit(const frontend::RegexPrimary& node) override;
        void visit(const frontend::RegexUnion& node) override;
        void visit(const frontend::RegexWildcard& node) override;
        void visit(const frontend::RegexStar& node) override;
        void visit(const frontend::RegexRefExpr& node) override;
        void visit(const frontend::RegexConcat& node) override;
    };
};