/* ast.cppm
 * Abstract Syntax Tree
 * Data structures representing the grammar of the regex.
 * This is the intermediate representation between parser and semantic analysis
 */
export module aion.frontend:ast;
import std;
export namespace aion::frontend {
    enum class Type : std::uint8_t
    {
        INT,
        CHAR,
        FLOAT,
        STRING
    };
    constexpr std::array<std::string, 4> type_string =
        { "INT", "CHAR", "FLOAT", "STRING" };

    struct FieldDecl
    {
        Type type{};
        std::string name{};
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
        std::string name;
    };
    struct Literal
    {
        Type type{};
        std::variant<int, float, char, std::string> value{};
    };
    struct PredDecl
    {
        std::string name{};
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
    struct RegexPredRef : RegexExpr
    {
        PredRefExpr pred_ref_expr;
    };
    struct RegexWildcard : RegexExpr {};
    struct RegexDecl
    {
        std::string name{};
        std::unique_ptr<RegexExpr> expr{};
    };
    struct AionFile {
        EventDecl event{};
        std::vector<PredDecl> predicates{};
        std::vector<RegexDecl> regexes{};
    };

    void dump_ast(const AionFile& ast);
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