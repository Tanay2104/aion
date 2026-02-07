/* ast_dump.cpp
 * Visitor functions for printing the AST
 * Created by Tanay Jha on 7 Feb 2026
 */

module aion.frontend;

import :ast;
import std;

namespace aion::frontend
{
    struct AstPrinter
    {
        std::uint32_t indent=0;

        void pad() const
        {
            for (std::uint32_t i=0;i<indent;++i) std::print(" ");
        }
        void visit(const EventDecl& event_decl)
        {
            pad();
            std::println("EventDecl");
            indent++;
            for (auto& field_decl: event_decl.fields)
            {
                visit(field_decl);
            }
            indent--;
        }
        void visit(const FieldDecl& field_decl)
        {
            pad();
            std::println("FieldDecl");
            indent++;
            pad();
            std::println("type: {}", type_string[static_cast<std::uint8_t>(field_decl.type)]);
            pad();
            std::println("name: {}", field_decl.name);
            indent--;
        }
    };


    void dump_ast(const AionFile& ast)
    {
        AstPrinter printer;
        std::println("AionFile AST");
        printer.pad();
        printer.indent++;
        printer.visit(ast.event);
    }
};


/* An example of how the tree dump can look:
*AionFile
├─ EventDecl
│  ├─ FieldDecl
│  │  ├─ type: INT
│  │  └─ name: x
│  ├─ FieldDecl
│  │  ├─ type: CHAR
│  │  └─ name: y
│  └─ FieldDecl
│     ├─ type: INT
│     └─ name: ts
│
├─ PredDecl  "P_x_big"
│  └─ CompPredExpr  (>)
│     ├─ PredRefExpr  "x"
│     └─ Literal  INT(100)
│
├─ PredDecl  "P_x_small"
│  └─ CompPredExpr  (<)
│     ├─ PredRefExpr  "x"
│     └─ Literal  INT(20)
│
├─ PredDecl  "P_y_a"
│  └─ CompPredExpr  (==)
│     ├─ PredRefExpr  "y"
│     └─ Literal  CHAR('a')
│
├─ PredDecl  "P_y_b"
│  └─ CompPredExpr  (==)
│     ├─ PredRefExpr  "y"
│     └─ Literal  CHAR('b')
│
├─ PredDecl  "P_y_c"
│  └─ CompPredExpr  (==)
│     ├─ PredRefExpr  "y"
│     └─ Literal  CHAR('c')
│
├─ PredDecl  "P1"
│  └─ AndPredExpr
│     ├─ PredRefExpr  "P_x_big"
│     └─ PredRefExpr  "P_y_a"
│
├─ PredDecl  "P2"
│  └─ AndPredExpr
│     ├─ PredRefExpr  "P_x_big"
│     └─ PredRefExpr  "P_y_b"
│
├─ PredDecl  "P3"
│  └─ AndPredExpr
│     ├─ PredRefExpr  "P_x_small"
│     └─ OrPredExpr
│        ├─ PredRefExpr  "P_y_b"
│        └─ PredRefExpr  "P_y_c"
│
└─ RegexDecl  "R1"
   └─ RegexConcat
      ├─ RegexUnion
      │  ├─ RegexPredRef  "P1"
      │  └─ RegexStar
      │     └─ RegexPredRef  "P2"
      ├─ RegexWildcard
      └─ RegexPredRef  "P3"

 */