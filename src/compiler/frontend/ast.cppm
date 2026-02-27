/* ast.cppm
 * Abstract Syntax Tree
 * Data structures representing the grammar of the regex.
 * This is the intermediate representation between parser and semantic analysis
 */
export module aion.frontend:ast;

import std;
import aion.core;

export namespace aion::frontend {
    constexpr std::array<std::string, 4> type_string =
        { "INT", "CHAR", "FLOAT", "STRING" };

    struct FieldDecl
    {
        core::Type type{};
        std::string_view name{};
    };
    struct EventDecl
    {
        std::vector<FieldDecl> fields{};
    };
    struct PredExpr
    {
        virtual ~PredExpr() = default;
    };
    struct AndPredExpr : PredExpr
    {
        std::vector<std::unique_ptr<PredExpr>> terms;
    };
    struct OrPredExpr : PredExpr
    {
        std::vector<std::unique_ptr<PredExpr>> terms;
    };
    struct NotExpr : PredExpr
    {
        std::unique_ptr<PredExpr> inner;
    };
    enum class CompOp : std::uint8_t
    {
        EQ, NEQ, LT,
        LE, GT, GE
    };
    struct CompPredExpr : PredExpr
    {
        std::unique_ptr<PredExpr> lhs;
        CompOp op;
        std::unique_ptr<PredExpr> rhs;
    };
    struct PredRefExpr : PredExpr
    {
        std::string_view name;
    };
    struct Literal
    {
        core::Type type{};
        std::variant<int, float, char, std::string_view, bool> value{};
    };
    struct PrimaryPredExpr : PredExpr
    {
        std::variant<PredRefExpr, Literal, std::unique_ptr<PredExpr>> expr;
    };
    struct PredDecl
    {
        std::string_view name{};
        std::unique_ptr<PredExpr> expr{};
    };
    struct RegexExpr
    {
        virtual ~RegexExpr() = default;
    };
    struct RegexUnion : RegexExpr
    {
        std::vector<std::unique_ptr<RegexExpr>> options;

    };
    struct RegexConcat : RegexExpr
    {
        std::vector<std::unique_ptr<RegexExpr>> sequence;
    };
    struct RegexStar : RegexExpr
    {
        std::unique_ptr<RegexExpr> inner;
    };
    struct RegexRefExpr : RegexExpr
    {
        std::string_view regex_ref_expr; // reger to another regex
    };
    struct RegexWildcard : RegexExpr {};
    struct RegexPrimary : RegexExpr
    {
        // we can refer to a predicate, a regex ref, a wildcard or a regex
        std::variant<std::string_view, RegexWildcard, std::unique_ptr<RegexExpr>> expr;
    };
    struct RegexDecl
    {
        std::string_view name{};
        std::unique_ptr<RegexExpr> expr{};
    };
    struct AionFile {
        EventDecl event{};
        std::vector<PredDecl> predicates{};
        std::vector<RegexDecl> regexes{};
    };

    void dump_ast(const AionFile& ast, const core::CompilationContext& ctx);
/* The visitors would look something like this:
    struct PredicateVisitor {
    virtual void visit(const AndExpr&) = 0;
    virtual void visit(const OrExpr&) = 0;
    virtual void visit(const NotExpr&) = 0;
    virtual void visit(const CompareExpr&) = 0;
    virtual void visit(const VarExpr&) = 0;
    virtual void visit(const LiteralExpr&) = 0;
    };
*/
};