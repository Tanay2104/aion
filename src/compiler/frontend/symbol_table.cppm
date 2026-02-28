/* symbol_table.cppm
 * Maps identifiers(such as x, P1, or R1) to their metadata
 * For variables, this can be the type and location in struct.
 * For predicates, it can be the C++ expression and bit-index
 * For regexes, this can be the root of the NFA graph, list of final states.
 */
export module aion.frontend:symbol_table;

import std;
import aion.core;
import :ast;

// Perhaps keeping symbol table in frontend might not be the best
// philosophically pure. However, it preserves our flow of information
// which is core -> frontend -> analysis -> automata -> codegen
// and doing so otherwise results in circular dependeny conflicts.

export namespace aion::frontend {

  enum class SymbolKind : std::uint8_t
  {
    EVENT_FIELD,
    PREDICATE,
    REGEX,
  };
  struct FieldMetadata
  {
    core::Type type{};
    std::uint32_t offset{}; // byte offset in struct.
    std::uint8_t size_in_bytes{}; // size in bytes.
  };
  struct PredicateMetadata
  {
    const PredDecl* ast_node{}; // non-owning const ptr to node.
    std::uint8_t alphabet_id{}; // currently only support upto 256 alphabets
  };
  struct RegexMetadata
  {
    const RegexDecl* ast_node{}; //  non-owning const ptr to node.
    std::uint8_t root_alphabet_id{}; // maybe don't need cause assumption we start always at 0?
    std::unordered_map<const RegexExpr*, std::uint16_t> node_to_pos_ids{}; // map from each predicate/reghex to it's pos id.
    // Note that we are mapping address instead of name since each predicate can occur multiple times in the
    // regex, and they require different id's in the glushkov construction. An alternative can be to directly
    // make an "annotated" ast wherein we keep a field in the base struct.
    std::unordered_map< std::uint16_t, std::string> pos_ids_to_names{}; // map from each predicate/regex to it's pos id.
    // Also we use uint16_t so that later we can extend, and provide warning in case regex is too large.
  };

  struct Symbol
  {
    std::string_view name{};
    SymbolKind kind{};
    std::variant<FieldMetadata, PredicateMetadata, RegexMetadata> details{};
  };

  class SymbolTable
  {
    private:
    // the core hashmap
    std::unordered_map<std::string_view, Symbol> symbols{};
    public:
    // insert new symbol. returns false if already inserted.
    [[nodiscard]] bool define(const Symbol& sym);

    // lookup a symbol by name. return nullptr if not found.
    [[nodiscard]] const Symbol* resolve(std::string_view name) const;

    [[nodiscard]] Symbol* mod_resolve(std::string_view name);
  };

  SymbolTable generate_symbol_table(const AionFile& ast, core::CompilationContext &ctxt);

}
