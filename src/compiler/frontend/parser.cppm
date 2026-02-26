/* parser.cppm
 * Simple recursive descent parser
 * Checks the syntax and builds the AST
 * Functions like parse_event, parse_regex etc
 */
export module aion.frontend:parser;
import std;
import :lexer;
import :ast;
export namespace aion::frontend
{
    class Parser
    {
        private:
        std::size_t current{};
        std::vector<Token>& tokens;
        std::unique_ptr<AionFile> root;
        core::CompilationContext &ctxt;
        // Hopefully the compiler will optimise the construction of the returned
        // objects to prevent copying.

        // advances current position by 1 and return the (prev) current token.
        Token advance();
        // Returns the current token without incrementing current.
        [[nodiscard]] Token peek() const;
        // Returns the next token without incrementing current.
        [[nodiscard]] Token peek_next() const;
        // have we reached the end??
        [[nodiscard]] bool is_at_end() const;
        // get the previous token
        [[nodiscard]] Token previous() const;
        // advance till the next semicolon, for errors.
        void synchronize();


        // Parsing helper functions. One function for each rule.

        std::optional<EventDecl> parse_event_decl();
        std::optional<FieldDecl> parse_field_decl();

        std::optional<PredDecl> parse_pred_decl();
        std::unique_ptr<PredExpr> parse_predexpr();
        std::unique_ptr<PredExpr> parse_predexpr_or();
        std::unique_ptr<PredExpr> parse_predexpr_and();
        std::unique_ptr<PredExpr> parse_predexpr_not();
        std::unique_ptr<PredExpr> parse_predexpr_comp();
        std::unique_ptr<PrimaryPredExpr> parse_predexpr_primary();


        std::optional<RegexDecl> parse_regex_decl();
        std::unique_ptr<RegexExpr> parse_regex();
        std::unique_ptr<RegexExpr> parse_regex_alt();
        std::unique_ptr<RegexExpr> parse_regex_concat();
        std::unique_ptr<RegexExpr> parse_regex_unary();
        std::unique_ptr<RegexPrimary> parse_regex_primary();


        public:
        explicit Parser(std::vector<Token>& tokens, core::CompilationContext& ctxt) : tokens(tokens), ctxt(ctxt)
        {
        }

        // Returns a unique pointer AST of the file. Note that the return
        // is by move, not copy.
        std::unique_ptr<AionFile> parse();
        };
};