/* ast.cppm
 * Abstract Syntax Tree
 * Data structures representing the grammar of the regex.
 * This is the intermediate representation between parser and semantic analysis
 */
export module aion.frontend:ast;

import std;
import aion.core;

export namespace aion::frontend {
    constexpr std::array<std::string, 5> type_string =
        { "INT", "CHAR", "FLOAT", "STRING" , "BOOL"};


    struct FieldDecl
    {
        core::Type type{};
        std::string_view name{};
    };
    struct EventDecl
    {
        std::vector<FieldDecl> fields{};
    };

    class PredicateVisitor;


    struct PredExpr
    {
        virtual ~PredExpr() = default;
        virtual void accept(PredicateVisitor&) const  = 0;

    };
    struct AndPredExpr : PredExpr
    {
        std::vector<std::unique_ptr<PredExpr>> terms;
        void accept(PredicateVisitor& v) const override;
    };
    struct OrPredExpr : PredExpr
    {
        std::vector<std::unique_ptr<PredExpr>> terms;
        void accept(PredicateVisitor& v) const override;
    };
    struct NotExpr : PredExpr
    {
        std::unique_ptr<PredExpr> inner;
        void accept(PredicateVisitor& v) const override;
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
        void accept(PredicateVisitor& v) const override;
    };
    struct PredRefExpr : PredExpr
    {
        std::string_view name;
        void accept(PredicateVisitor& v) const override;
    };
    struct Literal
    {
        core::Type type{};
        std::variant<int, float, char, std::string_view, bool> value{};
    };
    struct PrimaryPredExpr : PredExpr
    {
        std::variant<PredRefExpr, Literal, std::unique_ptr<PredExpr>> expr;
        void accept(PredicateVisitor& v) const override;
    };
    struct PredDecl
    {
        std::string_view name{};
        std::unique_ptr<PredExpr> expr{};
    };

    class RegexVisitor;

    struct RegexExpr
    {
        virtual ~RegexExpr() = default;
        virtual void accept(RegexVisitor&) const  = 0;
    };
    struct RegexUnion : RegexExpr
    {
        std::vector<std::unique_ptr<RegexExpr>> options;
        void accept(RegexVisitor& v) const override;

    };
    struct RegexConcat : RegexExpr
    {
        std::vector<std::unique_ptr<RegexExpr>> sequence;
        void accept(RegexVisitor& v) const override;

    };
    struct RegexStar : RegexExpr
    {
        std::unique_ptr<RegexExpr> inner;
        void accept(RegexVisitor& v) const override;

    };
    struct RegexRefExpr : RegexExpr
    {
        std::string_view regex_ref_expr; // refer to another regex
        void accept(RegexVisitor& v) const override;

    };
    struct RegexWildcard : RegexExpr
    {
        void accept(RegexVisitor& v) const override;
    };
    struct RegexPrimary : RegexExpr
    {
        // we can refer to a predicate, a regex ref, a wildcard or a regex
        std::variant<std::string_view, RegexWildcard, std::unique_ptr<RegexExpr>> expr;
        void accept(RegexVisitor& v) const override;

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

    class RegexVisitor
    {
        public:
        virtual ~RegexVisitor() = default;
        virtual void visit(const RegexUnion&) = 0;
        virtual void visit(const RegexConcat&) = 0;
        virtual void visit(const RegexStar&) = 0;
        virtual void visit(const RegexRefExpr&) = 0;
        virtual void visit(const RegexWildcard&) = 0;
        virtual void visit(const RegexPrimary&) = 0;
    };

    class PredicateVisitor {
        public:
        virtual ~PredicateVisitor() = default;
        virtual void visit(const AndPredExpr&) = 0;
        virtual void visit(const OrPredExpr&) = 0;
        virtual void visit(const NotExpr&) = 0;
        virtual void visit(const CompPredExpr&) = 0;
        virtual void visit(const PrimaryPredExpr&) = 0;
        virtual void visit(const PredRefExpr&) = 0;
    };


    // RegexExpr accept() implementations
    inline void RegexUnion::accept(RegexVisitor& v) const { v.visit(*this); }
    inline void RegexConcat::accept(RegexVisitor& v) const { v.visit(*this); }
    inline void RegexStar::accept(RegexVisitor& v) const { v.visit(*this); }
    inline void RegexRefExpr::accept(RegexVisitor& v) const { v.visit(*this); }
    inline void RegexWildcard::accept(RegexVisitor& v) const { v.visit(*this); }
    inline void RegexPrimary::accept(RegexVisitor& v) const { v.visit(*this); }


    // PredExpr accept() implementations
    inline void AndPredExpr::accept(PredicateVisitor& v) const { v.visit(*this); }
    inline void OrPredExpr::accept(PredicateVisitor& v) const { v.visit(*this); }
    inline void NotExpr::accept(PredicateVisitor& v) const { v.visit(*this); }
    inline void CompPredExpr::accept(PredicateVisitor& v) const { v.visit(*this); }
    inline void PredRefExpr::accept(PredicateVisitor& v) const { v.visit(*this); }
    inline void PrimaryPredExpr::accept(PredicateVisitor& v) const { v.visit(*this); }
};