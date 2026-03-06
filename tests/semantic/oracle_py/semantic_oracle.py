#!/usr/bin/env python3

import argparse
import json
import re
import sys
from dataclasses import dataclass
from typing import Dict, List, Tuple


EMPTY = ("empty",)
EPS = ("eps",)


def parse_declarations(source: str) -> Tuple[List[Tuple[str, str]], List[Tuple[str, str]]]:
    pred_matches = re.finditer(r"\bpred\s+([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(.*?);", source, re.DOTALL)
    regex_matches = re.finditer(r"\bregex\s+([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(.*?);", source, re.DOTALL)
    preds = [(m.group(1), m.group(2).strip()) for m in pred_matches]
    regexes = [(m.group(1), m.group(2).strip()) for m in regex_matches]
    return preds, regexes


def translate_predicate_expr(expr: str) -> str:
    converted = expr
    converted = converted.replace("!=", "__NEQ__")
    converted = converted.replace("&&", " and ")
    converted = converted.replace("||", " or ")
    converted = converted.replace("!", " not ")
    converted = converted.replace("__NEQ__", "!=")
    converted = re.sub(r"\btrue\b", "True", converted)
    converted = re.sub(r"\bfalse\b", "False", converted)
    return converted


def evaluate_predicates(pred_decls: List[Tuple[str, str]], trace: List[Dict[str, object]]) -> Dict[str, List[bool]]:
    compiled = [(name, translate_predicate_expr(expr)) for name, expr in pred_decls]
    result: Dict[str, List[bool]] = {name: [] for name, _ in pred_decls}

    for event in trace:
        env: Dict[str, object] = dict(event)
        for name, expr in compiled:
            value = eval(expr, {"__builtins__": {}}, env)
            env[name] = bool(value)
            result[name].append(bool(value))
    return result


@dataclass
class Token:
    kind: str
    text: str


def tokenize_regex(expr: str) -> List[Token]:
    tokens: List[Token] = []
    i = 0
    while i < len(expr):
        ch = expr[i]
        if ch.isspace():
            i += 1
            continue
        if ch in "|.*()":
            tokens.append(Token(ch, ch))
            i += 1
            continue
        if ch == "_":
            tokens.append(Token("ANY", "_"))
            i += 1
            continue
        if ch.isalpha() or ch == "_":
            start = i
            i += 1
            while i < len(expr) and (expr[i].isalnum() or expr[i] == "_"):
                i += 1
            ident = expr[start:i]
            tokens.append(Token("IDENT", ident))
            continue
        raise ValueError(f"Unexpected regex character: {ch!r}")
    tokens.append(Token("EOF", ""))
    return tokens


class RegexParser:
    def __init__(self, tokens: List[Token]):
        self.tokens = tokens
        self.current = 0

    def peek(self) -> Token:
        return self.tokens[self.current]

    def advance(self) -> Token:
        tok = self.tokens[self.current]
        self.current += 1
        return tok

    def match(self, kind: str) -> bool:
        if self.peek().kind == kind:
            self.advance()
            return True
        return False

    def expect(self, kind: str) -> Token:
        if self.peek().kind != kind:
            raise ValueError(f"Expected token {kind}, found {self.peek().kind}")
        return self.advance()

    def parse(self):
        expr = self.parse_union()
        self.expect("EOF")
        return expr

    def parse_union(self):
        parts = [self.parse_concat()]
        while self.match("|"):
            parts.append(self.parse_concat())
        if len(parts) == 1:
            return parts[0]
        return ("union", parts)

    def parse_concat(self):
        parts = [self.parse_unary()]
        while self.match("."):
            parts.append(self.parse_unary())
        if len(parts) == 1:
            return parts[0]
        return ("concat", parts)

    def parse_unary(self):
        expr = self.parse_primary()
        if self.match("*"):
            return ("star", expr)
        return expr

    def parse_primary(self):
        tok = self.peek()
        if tok.kind == "IDENT":
            self.advance()
            return ("pred", tok.text)
        if tok.kind == "ANY":
            self.advance()
            return ("any",)
        if tok.kind == "(":
            self.advance()
            expr = self.parse_union()
            self.expect(")")
            return expr
        raise ValueError(f"Unexpected token in primary: {tok.kind}")


def parse_regex(expr: str):
    return RegexParser(tokenize_regex(expr)).parse()


def simplify(node):
    kind = node[0]
    if kind in ("empty", "eps", "pred", "any"):
        return node
    if kind == "star":
        inner = simplify(node[1])
        if inner == EMPTY or inner == EPS:
            return EPS
        if inner[0] == "star":
            return inner
        return ("star", inner)
    if kind == "union":
        terms = []
        seen = set()
        for term in node[1]:
            s = simplify(term)
            if s == EMPTY:
                continue
            if s[0] == "union":
                for nested in s[1]:
                    nested_s = simplify(nested)
                    if nested_s != EMPTY and nested_s not in seen:
                        seen.add(nested_s)
                        terms.append(nested_s)
            else:
                if s not in seen:
                    seen.add(s)
                    terms.append(s)
        if not terms:
            return EMPTY
        if len(terms) == 1:
            return terms[0]
        return ("union", tuple(terms))
    if kind == "concat":
        parts = []
        for part in node[1]:
            s = simplify(part)
            if s == EMPTY:
                return EMPTY
            if s == EPS:
                continue
            if s[0] == "concat":
                parts.extend(s[1])
            else:
                parts.append(s)
        if not parts:
            return EPS
        if len(parts) == 1:
            return parts[0]
        return ("concat", tuple(parts))
    raise ValueError(f"Unknown node kind in simplify: {kind}")


def nullable(node) -> bool:
    kind = node[0]
    if kind == "empty":
        return False
    if kind == "eps":
        return True
    if kind in ("pred", "any"):
        return False
    if kind == "union":
        return any(nullable(term) for term in node[1])
    if kind == "concat":
        return all(nullable(part) for part in node[1])
    if kind == "star":
        return True
    raise ValueError(f"Unknown node kind in nullable: {kind}")


def derivative(node, true_preds: Dict[str, bool]):
    kind = node[0]
    if kind in ("empty", "eps"):
        return EMPTY
    if kind == "pred":
        return EPS if true_preds.get(node[1], False) else EMPTY
    if kind == "any":
        return EPS
    if kind == "union":
        return simplify(("union", [derivative(term, true_preds) for term in node[1]]))
    if kind == "concat":
        parts = list(node[1])
        if not parts:
            return EMPTY
        first = parts[0]
        rest = parts[1:]
        first_deriv = derivative(first, true_preds)
        head = simplify(("concat", [first_deriv] + rest))
        if nullable(first):
            if rest:
                tail = derivative(("concat", rest), true_preds)
            else:
                tail = EMPTY
            return simplify(("union", [head, tail]))
        return head
    if kind == "star":
        return simplify(("concat", [derivative(node[1], true_preds), node]))
    raise ValueError(f"Unknown node kind in derivative: {kind}")


def timeline_for_regex(node, pred_timeline: Dict[str, List[bool]], trace_len: int) -> List[bool]:
    node = simplify(node)
    active = EMPTY
    out: List[bool] = []
    for i in range(trace_len):
        true_preds = {name: values[i] for name, values in pred_timeline.items()}
        d_active = derivative(active, true_preds)
        d_start = derivative(node, true_preds)
        active = simplify(("union", [d_active, d_start]))
        matched = nullable(active)
        out.append(bool(matched))
    return out


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", required=True)
    parser.add_argument("--trace", required=True)
    args = parser.parse_args()

    source = open(args.source, "r", encoding="utf-8").read()
    trace = json.load(open(args.trace, "r", encoding="utf-8"))

    pred_decls, regex_decls = parse_declarations(source)
    pred_timeline = evaluate_predicates(pred_decls, trace)

    for regex_name, regex_expr in regex_decls:
        parsed = parse_regex(regex_expr)
        timeline = timeline_for_regex(parsed, pred_timeline, len(trace))
        bits = "".join("1" if v else "0" for v in timeline)
        print(f"{regex_name}:{bits}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
