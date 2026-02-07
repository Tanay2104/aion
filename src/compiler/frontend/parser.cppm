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

        // Parsing helper functions. One function for each rule.

        EventDecl parse_event_decl();
        FieldDecl parse_field_decl();

        void parse_pred_decl();
        void parse_predexpr();
        void parse_predexpr_or();
        void parse_predexpr_and();
        void parse_predexpr_not();
        void parse_predexpr_comp();
        void parse_predexpr_primary();


        void parse_regex_decl();
        void parse_regex();
        void parse_regex_alt();
        void parse_regex_concat();
        void parse_regex_unary();
        void parse_regex_primary();


        public:
        explicit Parser(std::vector<Token>& tokens, core::CompilationContext& ctxt) : tokens(tokens), ctxt(ctxt)
        {
        }

        // Returns a unique pointer AST of the file. Note that the return
        // is by move, not copy.
        std::unique_ptr<AionFile> parse();
        };
};