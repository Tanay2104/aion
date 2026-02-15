# Specification of the Symbolic Regex Grammar

## Top level file grammar

```ebnf
aion_file
    → event_decl predicate_decl* regex_decl+
```

## Grammar for event declaration

```ebnf
event_decl
    → "event" "{" field_decl* "}" ";"

field_decl
    → type identifier ";"

type
    → "int"
    | "char"
    | "float"
    | "bool"
```

## Grammar for the predicate language

```ebnf
predicate_decl
    → "pred" identifier "=" predicate_expr ";"

predicate_expr
    → predicate_or

predicate_or
    → predicate_and ( "||" predicate_and )*

predicate_and
    → predicate_not ( "&&" predicate_not )*

predicate_not
    → "!" predicate_not
    | predicate_comparison

predicate_comparison
    → predicate_primary
    | predicate_primary comp_op predicate_primary

predicate_primary
    → identifier
    | literal
    | "(" predicate_expr ")"

literal
    → INT | FLOAT | CHAR | "true" | "false"

comp_op
    → "==" | "!=" | "<=" | "<" | ">" | ">="
```

## Grammar for the regex language

```ebnf
regex_expr
→ regex_alt

regex_alt
→ regex_concat
| regex_alt "|" regex_concat

regex_concat
→ regex_unary
| regex_concat "." regex_unary

regex_unary
→ regex_primary
| regex_primary "*"

regex_primary
→ identifier
| "_"
| "(" regex_expr ")"
```
