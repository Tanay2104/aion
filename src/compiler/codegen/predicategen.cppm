/* event_emitter.cppm
* Class declaration for event codegen.
 */

export module aion.codegen:predicategen;

import std;
import aion.frontend;
import :cemitter;
import aion.core;

namespace aion::codegen
{

 export void emit_predicates(const frontend::AionFile& ast, const frontend::SymbolTable& symbol_table, CEmitter& emitter, const core::CompilationContext& ctxt);

 class PredicateGenVisitor : public frontend::PredicateVisitor
 {
 private:
  std::stack<std::string> stack;

  const core::CompilationContext& ctxt;
  const frontend::SymbolTable& symbol_table;

 public:
  explicit PredicateGenVisitor(const core::CompilationContext& _ctxt,
                              const frontend::SymbolTable& _symbol_table);

  void visit(const frontend::AndPredExpr&) override;
  void visit(const frontend::OrPredExpr&) override;
  void visit(const frontend::NotExpr&) override;
  void visit(const frontend::CompPredExpr&) override;
  void visit(const frontend::PrimaryPredExpr&) override;
  void visit(const frontend::PredRefExpr&) override;

  std::string get_pred_string();
 };
};