/* symbol_table.cppm
* Maps identifiers(such as x, P1, or R1) to their metadata
 * For variables, this can be the type and location in struct.
 * For predicates, it can be the C++ expression and bit-index
 * For regexes, this can be the root of the NFA graph, list of final states.
 */
export module aion.core:symbol_table;

import std;

export namespace aion::core {

struct FieldInfo {
  std::string type{}; // "int"
  int order{}; // Order in struct(can regain offset via this).
  FieldInfo(const std::string& type, const int order) : type(type), order(order) {}
};

struct PredicateInfo {
  std::string expression{}; // "(x > 100)"
  int alphabet_id{};        // 0, 1, 2... (The bit index)
  PredicateInfo(const std::string& expr, const int alph_id) : expression(expr), alphabet_id(alph_id) {}
};

class SymbolTable {
private:
  // Maps names to their info
  std::unordered_map<std::string, FieldInfo> fields{};
  std::unordered_map<std::string, PredicateInfo> predicates{};

  int next_alphabet_id{};

public:
  // Called by Parser when it sees 'int x;' inside event{}
  void add_field(const std::string& name, const std::string& type,
                 const int order) {
    fields.insert({name, FieldInfo(type, order)});
  }

  // Called by Parser when it sees 'pred P1 = ...'
  void add_predicate(const std::string& name, const std::string& expr) {
    predicates.insert({name, PredicateInfo(expr, next_alphabet_id)});
  }

  // Called by Glushkov Builder when it sees P1 in a regex
  [[nodiscard]] const PredicateInfo& lookup_predicate(const std::string& name) const {
    return predicates.at(name);
  }

  // Returns the total number of bits needed (the alphabet size)
  [[nodiscard]] int get_alphabet_size() const { return next_alphabet_id; }
};
}
