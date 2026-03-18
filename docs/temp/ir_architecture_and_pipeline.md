# Aion IR Architecture and Compiler Pipeline Redesign

Created on March 17, 2026.

This document proposes a rigorous, long-term compiler architecture for Aion, centered around multi-level IRs and clean phase boundaries. The goal is to preserve performance while preventing repeated cross-cutting rewrites as features, backends, and optimizations grow.

This revision also hardens the execution strategy: keep the long-term architecture as the destination, but sequence implementation with a minimal high-ROI subset first (`TIR`, `PIR`, `AIR`, `BIR`) and only extract additional backend layers (`KIR`, `LIR`) when concrete duplication pressure appears.

The intended audience is maintainers working on Aion's compiler internals.

---

## Table of Contents

1. [Motivation and Problem Statement](#1-motivation-and-problem-statement)
2. [Current Pipeline and Coupling Points](#2-current-pipeline-and-coupling-points)
3. [Design Goals and Non-Goals](#3-design-goals-and-non-goals)
4. [High-Level Target Architecture](#4-high-level-target-architecture)
5. [IR Levels: Purpose, Syntax, and Semantics](#5-ir-levels-purpose-syntax-and-semantics)
6. [Symbol Table Role in the New Model](#6-symbol-table-role-in-the-new-model)
7. [Data Structures and Ownership Model](#7-data-structures-and-ownership-model)
8. [Mutation Policy and Lifetime Rules](#8-mutation-policy-and-lifetime-rules)
9. [Pass Interfaces and Call Signatures](#9-pass-interfaces-and-call-signatures)
10. [Concrete End-to-End Example Transformation](#10-concrete-end-to-end-example-transformation)
11. [Parallelism and Independent Transformations](#11-parallelism-and-independent-transformations)
12. [Optimization Strategy: Predicate and Kernel](#12-optimization-strategy-predicate-and-kernel)
13. [What Changes, What Is Eliminated, What Stays](#13-what-changes-what-is-eliminated-what-stays)
14. [Migration Plan (Incremental, No Big-Bang Rewrite)](#14-migration-plan-incremental-no-big-bang-rewrite)
15. [Testing and Verification Strategy](#15-testing-and-verification-strategy)
16. [Diagnostics, Dumps, and Developer UX](#16-diagnostics-dumps-and-developer-ux)
17. [Risk Register and Tradeoffs](#17-risk-register-and-tradeoffs)
18. [Open Questions and Decision Checklist](#18-open-questions-and-decision-checklist)
19. [Appendix A: Reference to Current Code Paths](#19-appendix-a-reference-to-current-code-paths)
20. [Appendix B: Minimal Initial Type Definitions](#20-appendix-b-minimal-initial-type-definitions)
21. [Beginner Primer: How To Read This Compiler Design](#21-beginner-primer-how-to-read-this-compiler-design)
22. [Deep Worked Example: Full Pipeline Trace](#22-deep-worked-example-full-pipeline-trace)
23. [Alternative Designs Per IR Layer](#23-alternative-designs-per-ir-layer)
24. [C++ API and Pass Signature Alternatives](#24-c-api-and-pass-signature-alternatives)
25. [Detailed Ownership, Lifetimes, and Mutability](#25-detailed-ownership-lifetimes-and-mutability)
26. [Diagnostics and Source Mapping Deep Dive](#26-diagnostics-and-source-mapping-deep-dive)
27. [Feature Walkthrough: Adding Arithmetic End-to-End](#27-feature-walkthrough-adding-arithmetic-end-to-end)
28. [Feature Walkthrough: Adding History Buffers](#28-feature-walkthrough-adding-history-buffers)
29. [Feature Walkthrough: Multiple Event Streams](#29-feature-walkthrough-multiple-event-streams)
30. [Design FAQ and Decision Rationale](#30-design-faq-and-decision-rationale)
31. [Implementation Checklist by Milestone](#31-implementation-checklist-by-milestone)

---

## 1. Motivation and Problem Statement

Aion currently has a clean frontend/analysis/automata split, but codegen remains a large coupling sink. Backend generation currently performs semantic decisions late, often directly from AST/symbol-table strings. This worked for initial velocity, but it becomes expensive as:

1. Backends diverge (`SCALAR64`, `AVX2`, future `AVX512`, `NEON`).
2. Predicate logic grows (CSE/minimization needs).
3. Feature set expands (arithmetic, history windows, multi-stream).
4. Performance tuning requires structured optimization passes.

The roadmap already identifies this pressure ("need of an IR" and optimization over generated code).

Core issue: many decisions that should be fixed once in semantic/mid-end phases are being re-decided in codegen.

The redesign objective is to:

1. Decide semantics early.
2. Canonicalize once.
3. Lower to backend-specific forms late.
4. Keep phase contracts explicit and testable.

---

## 2. Current Pipeline and Coupling Points

Current top-level pipeline in `src/compiler/main.cpp`:

1. source load
2. lexing
3. parsing (AST)
4. symbol table generation
5. position-id filling for regex symbols
6. Glushkov NFA construction
7. direct code emission (event, predicates, engines)

### 2.1 Strong parts in current design

1. Frontend and automata are already separated.
2. NFA generation is conceptually clean and independently testable.
3. Backend strategy abstraction exists (`CodeGenStrategy`).

### 2.2 Coupling pain points

1. Late identifier disambiguation in predicate codegen (`PredRefExpr` resolved while emitting C++).
2. AST-shape dependence in multiple downstream places.
3. Position-level mask emission repeats predicate evaluations in scalar path.
4. Shared emitter state limits parallelism and modular optimization.

### 2.3 Terminology collision to resolve

`core::IR` currently means dump targets (`TOKENS`, `AST`, `NFA`), not optimization/codegen IR levels. That is fine but should be documented as "debug dump stage", not "compiler mid-end IR".

---

## 3. Design Goals and Non-Goals

### 3.1 Goals

1. Single-source-of-truth semantics before codegen.
2. Minimal repeated logic across backends.
3. Stable IDs and immutable snapshots between phases.
4. Explicit contracts and invariants per pass.
5. Incremental adoption without stopping development.
6. Maintain Aion performance priorities (zero-alloc hot path, deterministic generated kernels).

### 3.2 Non-goals

1. Replacing parser/frontend grammar in one go.
2. Building a generic compiler framework for unrelated DSLs.
3. Introducing heavy formal methods before basic pass contracts are in place.

---

## 4. High-Level Target Architecture

### 4.1 Long-term destination architecture

```text
Source
  -> Tokens
  -> AST
  -> SymbolTable (frontend workspace)
  -> TIR (typed, resolved semantic IR)
  -> PIR (predicate canonical DAG IR)
  -> AIR (automata IR)
  -> KIR (kernel semantic IR)
  -> LIR (backend-legalized IR)
  -> Printed C/C++ header
```

### 4.2 Immediate execution architecture (recommended now)

```text
Source
  -> Tokens
  -> AST
  -> SymbolTable
  -> TIR
  -> PIR
  -> AIR
  -> BIR (single backend IR)
  -> Printed C/C++ header via CEmitter
```

This document intentionally separates:

1. destination architecture (where the system should end up)
2. implementation architecture (what should be built first)

### 4.3 Design principle

1. Earlier layers encode language meaning.
2. Middle layers encode optimization opportunities.
3. Later layers encode backend legality and syntax.

### 4.4 Sequencing rule

Do not introduce a new IR layer unless at least one of these is true:

1. a repeated bug class is crossing backend boundaries
2. backend duplication exceeds acceptable threshold
3. a new backend cannot be added without semantic leakage into existing layers

---

## 5. IR Levels: Purpose, Syntax, and Semantics

This section is normative.

### 5.1 AST (existing)

Purpose:

1. Preserve user syntax shape.
2. Preserve source-level structure for diagnostics.

Semantics:

1. Not fully resolved.
2. Contains parser artifacts (primaries, grouping wrappers, raw names).

### 5.2 TIR (Typed IR)

Purpose:

1. Freeze name resolution and type validity.
2. Remove syntax-only wrappers.
3. Provide stable IDs for all declared entities.

Semantics guaranteed:

1. Every identifier is resolved to concrete entity kind (`field`, `predicate`, `regex`).
2. Every expression has a final static type.
3. Illegal forms are rejected before TIR completion.

Example TIR node set:

1. `LoadField(FieldId)`
2. `PredRef(PredId)` or inlined predicate body reference policy
3. `ConstBool/ConstInt/ConstFloat/ConstChar`
4. `Cmp(op, ExprId, ExprId)`
5. `BoolAnd/BoolOr/BoolNot`
6. `Arith(op, ExprId, ExprId)` (future-ready)

### 5.3 PIR (Predicate IR)

Purpose:

1. Canonical predicate algebra representation.
2. Global CSE and simplification.

Form:

1. Hash-consed DAG of expression nodes.
2. Predicate definitions are roots into this DAG.

Why DAG:

1. Immediate structural sharing across predicates.
2. Straightforward lowering to imperative code with temporaries.
3. Lower complexity than making decision diagrams the primary representation.

Optional advanced pass:

1. `PIR -> DD -> PIR` only for selected heavy formulas under cost/size guards.

### 5.4 AIR (Automata IR)

Purpose:

1. Encode regex automata semantics independent of textual names.

Core contents:

1. Regex symbol positions mapped to semantic atom IDs (`PredId`, wildcard, constant true/false).
2. `nullable`, `first`, `last`, `follow`.

Note:

1. AIR is conceptually close to current `Generic_NFA` plus richer symbol metadata.

### 5.5 BIR (Backend IR, immediate)

Purpose:

1. Single executable backend-facing IR for initial migration.
2. Replace immediate need for separate KIR/LIR while keeping strict contracts.

Semantics:

1. Represents final kernel operations for codegen.
2. Supports both scalar and AVX2 lowering paths as distinct emitters.
3. Must not embed parser/frontend concerns.

Note:

1. BIR is intentionally a temporary consolidation layer.
2. It should be split into KIR/LIR only when extraction gates are met (Section 18).

### 5.6 KIR (Kernel IR, planned extraction stage)

Purpose:

1. Single backend-neutral representation of runtime kernel semantics.

Semantics:

1. `M` construction from predicate/wildcard masks.
2. `S_topo` transfer from `S` and follow.
3. `S_next = S_topo & M`.
4. `accept = ((S_next & last) != 0)`.

KIR ops example:

1. `EvalPredRoot(PredRootId)`
2. `SelectMask(bool_val, mask_true, mask_false)`
3. `OrMask`, `AndMask`
4. `TransferFollow(mode, S, follow, first)`
5. `AcceptTest`

### 5.7 LIR (Lowered IR, planned extraction stage)

Purpose:

1. Encode only operations legal for target backend/capabilities.

Example legalizations:

1. Scalar64: `u64` shifts/masks/loop forms.
2. AVX2: lane-aware bit propagation and mask materialization constraints.
3. Future NEON/AVX512 legalized variants.

### 5.8 Target Syntax IR (deferred)

Status:

1. Explicitly deferred.
2. Not required in current migration scope.

Rationale:

1. CEmitter remains sufficient while BIR contracts are strict.
2. A full target syntax tree is only justified if emission correctness or maintainability becomes a proven bottleneck.

---

## 6. Symbol Table Role in the New Model

Symbol table is still required.

### 6.1 In the new architecture, symbol table is:

1. Frontend binding workspace.
2. Validation engine for duplicates/undefined identifiers.
3. Builder input for TIR construction.

### 6.2 Symbol table is not:

1. A runtime dependency of backend emission phases.
2. A mutable global consulted from every pass.

After TIR is built, downstream passes should use stable IDs, not name lookups.

This removes fragile late resolution patterns.

---

## 7. Data Structures and Ownership Model

Use an explicit compile session with staged modules.

```cpp
struct CompilationSession {
  SourceStore source;
  NameInterner names;
  Diagnostics diagnostics;

  std::optional<AstModule> ast;
  std::optional<SymbolTable> symbols;
  std::optional<TirModule> tir;
  std::optional<PirModule> pir;
  std::optional<AirModule> air;
  std::optional<BirModule> bir;
  // planned extraction stages:
  // std::optional<KirModule> kir;
  // std::optional<LirModule> lir;
};
```

### 7.1 Stable identity

Use compact IDs:

1. `FieldId`, `PredId`, `RegexId`, `ExprId`, `NodeId`, `SpanId`.

Avoid raw pointer identity for cross-pass references.

### 7.2 String ownership

Use interned names (`NameId`) to avoid `std::string_view` lifetime hazards.

### 7.3 Node storage

Prefer arena/vector storage with ID indexing:

1. deterministic traversal
2. cheap serialization/dumps
3. robust hashing/equality

---

## 8. Mutation Policy and Lifetime Rules

Normative phase policy:

1. Parser may mutate only AST being constructed.
2. Binder may mutate only symbol table being constructed.
3. TIR builder creates TIR once; TIR immutable after finalize.
4. PIR/AIR/BIR passes treat inputs as immutable and output new modules.
5. Diagnostics are append-only.

### 8.1 Why immutable snapshots

1. Easy reasoning and debugging.
2. Safe parallel reads.
3. Deterministic pass behavior.

### 8.2 Copying policy

1. No deep copying large trees unless required by pass semantics.
2. Use IDs and references into immutable storage.
3. Return new modules by value with move semantics.

---

## 9. Pass Interfaces and Call Signatures

### 9.1 Immediate implementation signatures

```cpp
AstModule parse_source(const SourceStore&, Diagnostics&);
SymbolTable bind_symbols(const AstModule&, NameInterner&, Diagnostics&);
TirModule lower_ast_to_tir(const AstModule&, const SymbolTable&, NameInterner&, Diagnostics&);

PirModule build_pir(const TirModule&, Diagnostics&);
AirModule build_air(const TirModule&, Diagnostics&);
BirModule build_bir(const TirModule&, const PirModule&, const AirModule&, TargetCaps, OptimLevel, Diagnostics&);

GeneratedUnit emit_cpp_from_bir(const BirModule&, EmitOptions, Diagnostics&);
```

### 9.2 Planned extraction signatures (future)

```cpp
KirModule build_kir(const TirModule&, const PirModule&, const AirModule&, Diagnostics&);
LirModule lower_kir_to_lir(const KirModule&, TargetCaps, OptimLevel, Diagnostics&);
GeneratedUnit emit_cpp(const LirModule&, EmitOptions, Diagnostics&);
```

### 9.3 Structural signature rules

1. Inputs are `const&`.
2. No hidden globals.
3. Outputs are complete modules, not partial mutation.
4. Diagnostics explicit in function boundary.

---

## 10. Concrete End-to-End Example Transformation

Input:

```regex
event { int x; char y; int valid; };
pred P_hi = (x > 100);
pred P_ya = (y == 'a');
pred A    = P_hi && P_ya;
pred B    = P_hi || P_ya;
regex R1  = A . B . A;
regex R2  = (A | _) * . B;
```

### 10.1 AST

Contains:

1. `PredRefExpr("x")`, `PredRefExpr("P_hi")`, etc.
2. `RegexConcat`, `RegexUnion`, `RegexStar`, `RegexWildcard`.
3. Names are text-level strings.

### 10.2 Symbol table

Build:

1. fields: `x`, `y`, `valid`
2. predicates: `P_hi`, `P_ya`, `A`, `B`
3. regexes: `R1`, `R2`

Used for:

1. duplicate detection
2. identifier-kind resolution

### 10.3 TIR

Normalize and resolve:

1. `x > 100` -> `Cmp(GT, LoadField(FieldId(x)), ConstInt(100))`
2. `P_hi && P_ya` -> `And(PredRef(PredId(P_hi)), PredRef(PredId(P_ya)))`
3. regex atom `A` -> `RegexAtom::Predicate(PredId(A))`
4. `_` -> `RegexAtom::Wildcard`

### 10.4 PIR

DAG roots:

1. `n0 = gt(load(x), 100)`
2. `n1 = eq(load(y), 'a')`
3. `n2 = and(n0, n1)` for `A`
4. `n3 = or(n0, n1)` for `B`

### 10.5 AIR

For `R1 = A . B . A`:

1. positions: `1:A`, `2:B`, `3:A`
2. symbol-to-position masks:
3. `mask(A) = (1<<1) | (1<<3)`
4. `mask(B) = (1<<2)`
5. wildcard mask `0`

For `R2 = (A|_)* . B`:

1. positions for `A`, `_`, `B` depending on traversal
2. wildcard mask has the `_` position bit set

`nullable/first/last/follow` computed from regex structure.

### 10.6 BIR (for one regex, immediate)

```text
a = eval(root_of_A)
b = eval(root_of_B)
M = wildcard_mask
M = M | select(a, maskA, 0)
M = M | select(b, maskB, 0)
S_topo = transfer_follow(S, follow, first, mode)
S_next = S_topo & M
accept = (S_next & last) != 0
S = S_next
```

### 10.7 BIR scalar lowering sketch

```cpp
uint64_t M = WILDCARD_MASK;
M |= (0ULL - (uint64_t)a) & MASK_A;
M |= (0ULL - (uint64_t)b) & MASK_B;
```

Then either jitter or no-jitter transfer form, selected by lowering policy.

### 10.8 Planned split view (future)

When extraction conditions are met:

1. BIR semantics become KIR.
2. target-specific legality rewrites become LIR.

### 10.9 Emit

Generate class/struct/function syntax via `CEmitter` directly from BIR (immediate), or from LIR after extraction (future).

---

## 11. Parallelism and Independent Transformations

### 11.1 Safely parallelizable units

1. Per-regex AIR construction after TIR freeze.
2. Per-regex BIR construction.
3. Per-regex `(BIR -> emit)` pipelines using isolated output buffers.
4. Per-predicate local PIR simplification before global DAG merge.

### 11.2 Requires controlled merge

1. Global PIR DAG interning (single merge step or sharded interner).
2. Diagnostics ordering (merge by source position).
3. Final emitted text ordering (deterministic by declaration order).

### 11.3 Not worth parallelizing initially

1. lexer/token stream construction
2. recursive-descent parse
3. small symbol table builds

---

## 12. Optimization Strategy: Predicate and Kernel

### 12.1 Predicate-level (PIR)

1. canonicalization (commutative sort)
2. hash-consing / structural dedup
3. constant folding
4. boolean algebra simplification
5. optional advanced pass (`PIR -> DD -> PIR`) for heavy formulas only
6. cost-aware ordering of conjunction/disjunction (profile-guided later)

### 12.2 Automata/kernel-level (AIR/BIR immediate)

1. precompute symbol masks by predicate id
2. evaluate each predicate root once per event
3. eliminate repeated per-position predicate calls
4. choose transfer implementation (jitter/no-jitter/hybrid) by policy
5. backend-specific details encoded in BIR emitter strategy now

### 12.3 Backend-level (planned KIR/LIR extraction)

1. peephole simplifications legal for target ISA
2. register/use ordering friendly to target compiler
3. capability-guarded paths (e.g., AVX2 lane crossing constraints)

---

## 13. What Changes, What Is Eliminated, What Stays

### 13.1 Will change

1. Add TIR/PIR/AIR/BIR modules and passes first.
2. Codegen inputs switch from AST+SymbolTable to IR modules.
3. Per-regex emission should use isolated buffers then merge.

### 13.2 Should be eliminated over migration

1. Late symbol resolution inside codegen visitors.
2. AST pointer identity as semantic key across long pass chains.
3. Direct string assembly as primary optimization substrate.

### 13.3 Should stay

1. Existing lexer/parser grammar and AST schema (initially).
2. Glushkov construction mathematics (adapted to AIR IDs).
3. `CodeGenStrategy` concept (updated to consume BIR now, KIR/LIR later).

---

## 14. Migration Plan (Incremental, No Big-Bang Rewrite)

### Phase 0: Guardrails

1. Freeze baseline tests/benchmarks.
2. Add regression checks for generated-output semantic parity.

### Phase 1: Introduce TIR

1. Build `AST + SymbolTable -> TIR`.
2. Keep existing codegen path untouched.
3. Add `--dump TIR` support for validation.

### Phase 2: Predicate path switch

1. Add PIR builder from TIR predicate section.
2. Generate predicate helpers from PIR.
3. Keep AST-based predicate codegen behind fallback flag until parity is proven.

### Phase 3: Automata path switch

1. Build AIR from TIR regex section.
2. Adapt Glushkov to consume AIR symbols/IDs.
3. Add `--dump AIR`.

### Phase 4: Backend IR (BIR) switch

1. Build BIR from PIR+AIR.
2. Scalar backend emits from BIR.
3. Ensure output parity with baseline.

### Phase 5: AVX2 direct implementation

1. Implement AVX2 emission from BIR (without forcing KIR/LIR split yet).
2. Keep parity and performance gates active.
3. Capture duplication metrics and backend divergence points.

### Phase 6: Extraction checkpoint

1. Decide whether to extract `KIR` and `LIR` using Section 18 gates.
2. If gates are met, perform `BIR -> KIR/LIR` refactor.
3. If gates are not met, continue with disciplined BIR.

### Phase 7: Cleanup

1. Remove deprecated AST-driven codegen paths.
2. Tighten pass contracts and invariants.

---

## 15. Testing and Verification Strategy

### 15.1 Contract tests per phase

1. `AST -> TIR` type and binding invariants.
2. `TIR -> PIR` canonicalization and node sharing.
3. `TIR -> AIR` position and follow correctness.
4. `PIR + AIR -> BIR` semantic equation checks.
5. backend emitter legality checks from BIR per backend.

### 15.2 Semantic equivalence tests

1. Existing semantic oracle suite remains mandatory.
2. Differential test old pipeline vs new pipeline during migration.

### 15.3 Performance tests

1. keep benchmark groups S/K/P/W for regression tracking
2. track compile-time overhead of new passes

---

## 16. Diagnostics, Dumps, and Developer UX

### 16.1 Separate concepts

1. Debug dump stages (`TOKENS`, `AST`, `NFA`) remain user-facing.
2. Internal optimization IR dumps should be separate flags (`--dump TIR`, `--dump PIR`, `--dump AIR`, `--dump BIR`).

### 16.2 Source mapping

Every TIR/PIR/AIR node should optionally carry source span references to preserve meaningful diagnostics.

### 16.3 Determinism

All dumps and emissions should be deterministic in ordering to make diffs useful.

---

## 17. Risk Register and Tradeoffs

### 17.1 Risks

1. Over-engineering before immediate wins.
2. Compile-time growth from additional passes.
3. Temporary dual-path complexity during migration.
4. Premature backend abstraction before second backend stabilizes.

### 17.2 Mitigations

1. phase-gate each IR with measurable payoff
2. retain old path behind temporary fallback switches
3. remove legacy path quickly after parity validation
4. keep KIR/LIR as explicit future extraction, not mandatory early implementation

### 17.3 Tradeoffs accepted

1. More compiler-internal code in exchange for cleaner extension points.
2. Slightly higher compile-phase complexity in exchange for stable backend evolution.

---

## 18. Open Questions and Decision Checklist

Before implementation, explicitly decide:

1. ID width and allocator strategy for all IR nodes.
2. Name interning API and lifetime ownership.
3. Whether to keep symbol table beyond TIR build for diagnostics only.
4. Which IR dumps are exposed to CLI users vs debug-only.
5. Cost-model inputs for jitter/no-jitter lowering decisions.
6. Policy for optional advanced boolean minimization passes.
7. BIR-to-KIR/LIR extraction trigger thresholds.

Checklist for each new pass:

1. What invariant does it establish?
2. What previous invariant does it rely on?
3. What does it mutate?
4. Is output deterministic?
5. How is it tested?

### 18.1 Extraction gates for KIR/LIR

Extract KIR/LIR only when at least one gate is true:

1. backend code duplication exceeds 20-25% in maintained hot-path emission logic
2. same bug class appears in multiple backend emitters due to missing shared semantic layer
3. third backend implementation starts and BIR can no longer stay clean

### 18.2 Non-goals during migration

1. no multithreaded compiler execution work
2. no target syntax AST work
3. no aggressive DD/BDD integration before PIR+CSE baseline is measured

---

## 19. Appendix A: Reference to Current Code Paths

Current orchestrator:

1. `src/compiler/main.cpp`

Current codegen strategy interfaces:

1. `src/compiler/codegen/codegenstrategy.cppm`
2. `src/compiler/codegen/regexgen.cpp`
3. `src/compiler/codegen/scalar.cpp`
4. `src/compiler/codegen/predicategen.cpp`
5. `src/compiler/codegen/eventgen.cpp`
6. `src/compiler/codegen/emitter.cpp`

Current frontend+analysis+automata anchors:

1. `src/compiler/frontend/ast.cppm`
2. `src/compiler/frontend/parser.cpp`
3. `src/compiler/frontend/symbol_table.cpp`
4. `src/compiler/analysis/alphabet.cpp`
5. `src/compiler/automata/glushkov.cpp`
6. `src/compiler/automata/nfa.cppm`

Roadmap discussion:

1. `docs/roadmap.md`

---

## 20. Appendix B: Minimal Initial Type Definitions

Initial minimal skeleton for bringing in new layers with low churn:

```cpp
// IDs
template <typename Tag>
struct Id {
  uint32_t value{};
  auto operator<=>(const Id&) const = default;
};

struct NameTag {};
struct FieldTag {};
struct PredTag {};
struct RegexTag {};
struct ExprTag {};
struct SpanTag {};

using NameId  = Id<NameTag>;
using FieldId = Id<FieldTag>;
using PredId  = Id<PredTag>;
using RegexId = Id<RegexTag>;
using ExprId  = Id<ExprTag>;
using SpanId  = Id<SpanTag>;

enum class ValueType : uint8_t { Bool, Int32, Float32, Char };
enum class PredOp    : uint8_t { And, Or, Not, Cmp };
enum class CmpOp     : uint8_t { EQ, NEQ, LT, LE, GT, GE };

struct TirExpr {
  ValueType type;
  SpanId span;
  // variant payload with IDs for children
};

struct TirPredicate {
  PredId id;
  NameId name;
  ExprId root;
};

struct TirRegexAtom {
  enum Kind : uint8_t { Predicate, Wildcard, ConstTrue, ConstFalse } kind;
  PredId pred; // valid if kind==Predicate
};

struct PirNode {
  ValueType type;
  // canonical op + children IDs + optional literal payload
};

struct DynamicBitSet {
  std::vector<uint64_t> words;
};

struct AirRegex {
  RegexId id;
  bool nullable;
  std::vector<TirRegexAtom> position_symbol; // 1-based semantic positions
  DynamicBitSet first_bits;
  DynamicBitSet last_bits;
  std::vector<DynamicBitSet> follow_bits;
};

struct BirKernel {
  RegexId regex;
  // backend-facing kernel ops
};

// planned extraction:
// struct KirKernel { ... };
// struct LirKernelScalar64 { ... };
```

This skeleton is intentionally minimal. It is sufficient to bootstrap phased migration without committing to a heavy framework.

---

## 21. Beginner Primer: How To Read This Compiler Design

If you are not deeply familiar with compilers, use this mental model:

1. A compiler pass is just a function that converts one data shape into another.
2. Each data shape exists because it makes one class of work easier.
3. If a pass needs to do two unrelated jobs, your IR boundary is probably wrong.

### 21.1 Why not just AST -> C++ strings forever?

Because the AST is optimized for parsing, not for optimization or backend lowering.

AST answers:

1. what did the user write?
2. how was it grouped syntactically?

Mid-end IR answers:

1. what does it mean semantically?
2. what can be optimized?

Backend IR answers:

1. what operations should the target backend perform?

### 21.2 Why so many layers at all?

Each layer removes one kind of uncertainty:

1. AST: uncertain types and unresolved names.
2. TIR: names and types resolved.
3. PIR: expression algebra canonicalized and shared.
4. AIR: regex logic translated to automata semantics.
5. BIR: executable backend-facing operations.

### 21.3 What makes a "good" IR stage?

A good IR stage:

1. has clear invariants.
2. serves one primary responsibility.
3. is easy to dump and diff.
4. enables optimizations difficult in previous stage.

### 21.4 What makes a "bad" IR stage?

A bad IR stage:

1. duplicates prior stage with no new invariants.
2. exists only for aesthetic purity.
3. forces every new feature to touch many representations without clear payoff.

---

## 22. Deep Worked Example: Full Pipeline Trace

This section traces one example through all immediate pipeline stages.

### 22.1 Source input

```regex
event { int x; char y; int valid; int ts; };

pred P_hi    = (x > 100);
pred P_ya    = (y == 'a');
pred P_valid = (valid == 1);

pred A = P_valid && P_hi && P_ya;
pred B = P_valid && (P_hi || P_ya);

regex R1 = A . B . A;
regex R2 = (A | _) * . B;
```

### 22.2 Tokens (conceptual)

You get a flat token stream:

1. `KW_EVENT`, `{`, `IDENT(x)`, ...
2. `KW_PRED`, `IDENT(P_hi)`, `=`, `(`, ...
3. `KW_REGEX`, `IDENT(R1)`, `=`, ...

At this stage there is no deep semantic meaning yet.

### 22.3 AST snapshot (conceptual)

AST keeps syntactic structure:

1. `PredDecl(A)` contains `AndPredExpr` of three terms.
2. `RegexDecl(R2)` contains `RegexConcat(RegexStar(RegexUnion(A,_)), B)`.

AST is close to grammar shape, not optimized.

### 22.4 Symbol table snapshot

Symbol table binds names:

1. fields: `x`, `y`, `valid`, `ts`
2. predicates: `P_hi`, `P_ya`, `P_valid`, `A`, `B`
3. regexes: `R1`, `R2`

This is where duplicate and undefined name checks happen.

### 22.5 TIR snapshot

TIR removes ambiguity:

```text
Expr#10: Cmp(GT, LoadField(FieldId(x)), ConstInt(100)) -> bool
Expr#11: Cmp(EQ, LoadField(FieldId(y)), ConstChar('a')) -> bool
Expr#12: Cmp(EQ, LoadField(FieldId(valid)), ConstInt(1)) -> bool

Pred#A: And(PredRef(P_valid), PredRef(P_hi), PredRef(P_ya))
Pred#B: And(PredRef(P_valid), Or(PredRef(P_hi), PredRef(P_ya)))
```

### 22.6 PIR snapshot

PIR hash-consing deduplicates shared logic:

```text
n0 = gt(load(x), 100)
n1 = eq(load(y), 'a')
n2 = eq(load(valid), 1)
n3 = and(n2, n0, n1)      // A
n4 = or(n0, n1)
n5 = and(n2, n4)          // B
```

`n0`, `n1`, `n2` are reused. This is the CSE win.

### 22.7 AIR snapshot

For `R1 = A . B . A`:

1. positions: `1:A`, `2:B`, `3:A`
2. `mask(A) = (1<<1) | (1<<3)`
3. `mask(B) = (1<<2)`
4. `wildcard_mask = 0`

For `R2 = (A|_)*.B`, wildcard positions contribute to `wildcard_mask`.

AIR also includes:

1. `first`, `last`, `follow`
2. `nullable`

### 22.8 BIR snapshot

BIR is backend-facing and executable:

```text
tA = EvalPredRoot(root_of_A)
tB = EvalPredRoot(root_of_B)

M  = ConstMask(wildcard_mask)
M  = Or(M, SelectMask(tA, maskA, 0))
M  = Or(M, SelectMask(tB, maskB, 0))

S_topo = TransferFollow(mode, S, follow, first)
S_next = And(S_topo, M)
accept = NonZero(And(S_next, last))
S      = S_next
```

### 22.9 Scalar emission example

```cpp
std::uint64_t M = WILDCARD_MASK;
M |= (0ULL - static_cast<std::uint64_t>(tA)) & MASK_A;
M |= (0ULL - static_cast<std::uint64_t>(tB)) & MASK_B;
```

### 22.10 AVX2 emission example (conceptual)

Same BIR semantics, different lowering:

1. scalar masks may become vector registers
2. transfer may use lane-aware operations
3. legal operations differ by backend, semantics do not

---

## 23. Alternative Designs Per IR Layer

This section compares alternatives and explains why the selected path is currently preferred.

### 23.1 TIR design alternatives

Alternative A: class hierarchy with virtual visitors

1. Pros: familiar OOP style.
2. Cons: pointer-heavy, cache-poor, harder serialization.

Alternative B: `std::variant` node payload + arena IDs

1. Pros: data-oriented, easy hash/equality, compact.
2. Cons: requires careful helper APIs.

Alternative C: table-per-node-kind (SoA)

1. Pros: maximum cache locality and SIMD-friendly scans.
2. Cons: highest engineering complexity.

Recommendation now:

1. Alternative B.

### 23.1.1 Suggested slim BoundPIR schema

```cpp
struct BoundPIR {
  std::vector<ExprNode> exprs;
  std::vector<Predicate> predicates;
  std::vector<RegexDecl> regexes;
};

struct ExprNode {
  ValueType type;
  SpanId span;
  ExprKind kind;
};

using ExprId = Id<ExprTag>;

struct ExprKind {
  std::variant<LoadField, ConstLiteral, Binary, Logical, PredRef> payload;
};

struct LoadField   { FieldId field; };
struct ConstLiteral{ ValueType type; Literal literal; };
struct Binary      { CmpOp op; ExprId lhs; ExprId rhs; };
struct Logical     { LogicalOp op; std::vector<ExprId> terms; };
struct PredRef     { PredId target; };
```

Key notes:

1. `BoundPIR` shares a single arena (`exprs`) for every expression.
2. Hash-consing canonicalizes `ExprKind` before insertion so equivalent nodes dedup automatically.
3. Predicates store the root `ExprId`; regex positions reference those IDs or `Wildcard` atoms separately.
4. Source spans and type info stay on each `ExprNode` for diagnostics.

This merged TIR+PIR delivers binding, typing, and canonical DAG guarantees without extra passes.

### 23.2 PIR alternatives

Alternative A: plain expression tree

1. Pros: simplest to build.
2. Cons: no structural sharing, weak CSE.

Alternative B: hash-consed DAG

1. Pros: high ROI, CSE by construction.
2. Cons: canonicalization complexity.

Alternative C: e-graph

1. Pros: strongest rewrite search power.
2. Cons: heavy complexity and compile-time cost.

Alternative D: BDD/ADD as primary IR

1. Pros: canonical form for some formula classes.
2. Cons: variable ordering sensitivity, integration complexity.

Recommendation now:

1. Alternative B.

### 23.2.1 Suggested slim AIR schema

```cpp
struct AirModule {
  std::unordered_map<RegexId, AirRegex> regexes;
};

struct AirRegex {
  RegexId id;
  BitVector first;
  BitVector last;
  std::vector<BitVector> follow;
  std::vector<Atom> positions;
  bool nullable;
};

struct Atom {
  enum Kind { Predicate, Wildcard, True, False } kind;
  PredId predicate;
};

struct BitVector {
  std::vector<uint64_t> words;
  void set(size_t idx);
};
```

`AirModule` stores follow/first/last bit vectors along with per-position metadata. Lowering later decides actual width.
2. Consider D as optional optimization pass only after baseline measurements.

### 23.3 AIR alternatives

Alternative A: fixed `uint64_t` bitsets

1. Pros: simple and fast.
2. Cons: locks state limits and backend assumptions too early.

Alternative B: dynamic bitset abstraction

1. Pros: backend-neutral, scalable.
2. Cons: more implementation work.

Alternative C: sparse adjacency sets only

1. Pros: memory-friendly for sparse graphs.
2. Cons: slower dense operations.

Recommendation now:

1. Alternative B in AIR.

### 23.3.1 Suggested slim BIR schema

```cpp
struct BirModule {
  std::vector<BirBlock> blocks;
  BirValue next_value;
};

struct BirBlock {
  std::string name;
  std::vector<BirInstr> instrs;
  BirTerminator term;
};

struct BirInstr {
  enum Kind { EvalPred, MaskMerge, TransferFollow, UpdateState, Const, Branch } kind;
  std::vector<BirValue> operands;
};

struct BirTerminator {
  enum Kind { Jump, Branch, Return } kind;
  std::variant<std::monostate, BranchInfo> payload;
};

struct BranchInfo {
  BirValue condition;
  std::string true_block;
  std::string false_block;
};

using BirValue = Id<ExprTag>;
```

`BirModule` now contains explicit basic blocks and terminators, enabling direct C++ control flow emission. Later you can choose per-block policies (`ShortCircuit`, `BranchlessSafe`) to resolve speculative execution vs classical short-circuit semantics.
2. Lower to fixed words only in backend-facing stage.

### 23.4 BIR vs immediate KIR+LIR

Alternative A: BIR only first

1. Pros: faster delivery, less boilerplate.
2. Cons: may mix concerns if not disciplined.

Alternative B: split KIR/LIR immediately

1. Pros: maximal conceptual purity.
2. Cons: high integration overhead before second backend stabilizes.

Recommendation now:

1. Alternative A with explicit extraction gates (Section 18.1).

### 23.5 Target syntax IR alternatives

Alternative A: direct CEmitter from BIR/LIR

1. Pros: simple, fast.
2. Cons: less structural guarantees.

Alternative B: structured target syntax tree

1. Pros: stronger formatting and structural tooling.
2. Cons: large engineering cost for current scope.

Recommendation now:

1. Alternative A.
2. Revisit only if emission maintenance becomes a bottleneck.

---

## 24. C++ API and Pass Signature Alternatives

### 24.1 Model A: pure functional style

```cpp
TirModule lower_to_tir(const AstModule&, const SymbolTable&, Diagnostics&);
PirModule build_pir(const TirModule&, Diagnostics&);
AirModule build_air(const TirModule&, Diagnostics&);
BirModule build_bir(const TirModule&, const PirModule&, const AirModule&, Diagnostics&);
```

Pros:

1. very testable
2. easy replay/diff
3. minimal hidden state

Cons:

1. may pass many parameters repeatedly

### 24.2 Model B: session-context style

```cpp
class CompilerSession {
public:
  void parse();
  void bind();
  void lower_tir();
  void build_pir();
  void build_air();
  void build_bir();
};
```

Pros:

1. ergonomic orchestration
2. less parameter plumbing

Cons:

1. risk of hidden coupling and accidental mutation

### 24.3 Model C: query-based incremental database

```cpp
const TirModule& get_tir(Session&);
const PirModule& get_pir(Session&);
```

Pros:

1. strong incremental compilation model
2. memoization-friendly

Cons:

1. highest design complexity

Recommendation now:

1. Model A for passes.
2. Thin orchestration wrapper around Model A is acceptable.

### 24.4 Signature design rules

1. Inputs are immutable references.
2. Output is explicit and complete.
3. Diagnostics are explicit parameter, not hidden global singleton.
4. Passes should not silently read unrelated stage data.
5. Deterministic ordering required in outputs.

---

## 25. Detailed Ownership, Lifetimes, and Mutability

### 25.1 Lifetime map by stage

1. `SourceStore`: alive for full compilation session.
2. `NameInterner`: alive for full session.
3. `AST`: alive at least until TIR completed and dumps done.
4. `SymbolTable`: alive until TIR done, then optional retention.
5. `TIR/PIR/AIR/BIR`: alive through emission for debugging and diffing.

### 25.2 Why IDs reduce lifetime bugs

Raw pointers to nodes in mutable containers are fragile. IDs plus arena storage means:

1. references remain stable
2. relocation does not break identity
3. easy serialization and logs

### 25.3 Copying policy details

1. Copy scalars and IDs freely.
2. Move large module containers.
3. Avoid deep clone unless pass semantics require forked worlds.

### 25.4 Mutation policy details

1. Construction passes may mutate builders.
2. Finalized modules become immutable views.
3. Any analysis metadata added later should live in side tables, not mutate core nodes.

---

## 26. Diagnostics and Source Mapping Deep Dive

### 26.1 Span propagation strategy

Every meaningful node should carry a `SpanId`:

1. TIR expressions and declarations
2. PIR nodes (with merged-span policy)
3. AIR regex elements
4. BIR operations where diagnostics may arise

### 26.2 PIR dedup and span ambiguity

When hash-consing merges equivalent nodes from multiple source locations:

Option A:

1. keep first span only

Option B:

1. keep vector of spans

Recommendation:

1. keep a compact representative span for fast path
2. optionally keep secondary spans behind debug flag for advanced diagnostics

### 26.3 User-facing error policy

1. Syntax errors stop before TIR.
2. Type/binding errors stop before PIR/AIR/BIR.
3. Backend lowering errors reference nearest semantic span.

---

## 27. Feature Walkthrough: Adding Arithmetic End-to-End

Assume new predicate:

```regex
pred C = (x + ts) > 500;
```

### 27.1 AST impact

1. parser adds arithmetic expression nodes if not already present.

### 27.2 TIR impact

1. add typed arithmetic ops in expression payload.
2. enforce type rules (`int + int`, no invalid mixes unless cast policy exists).

### 27.3 PIR impact

1. hash-cons arithmetic nodes (`add(load(x), load(ts))`).
2. simplification rules (`x + 0 -> x`) if desired.

### 27.4 AIR impact

1. none structurally, still predicate atom references.

### 27.5 BIR impact

1. add arithmetic eval op lowering in predicate evaluation block.

### 27.6 Emitter impact

1. scalar/AVX2 emitters map arithmetic BIR ops to target code forms.

---

## 28. Feature Walkthrough: Adding History Buffers

Assume new predicate concept:

```regex
pred Rising = x > history(x, 1) && history(x, 1) > history(x, 2);
```

### 28.1 TIR extension

Add node:

1. `HistoryRead(field_id, offset)`

### 28.2 PIR extension

History reads become normal leaves with typed outputs.

### 28.3 AIR impact

1. none (regex semantics unchanged).

### 28.4 BIR/backend impact

1. add state buffer declarations in generated engine
2. add load/update operations per event
3. preserve no-heap allocation guarantee via fixed-size ring buffers

### 28.5 Invariant to enforce

If history feature is unused, generated kernel must remain equivalent to old no-history fast path.

---

## 29. Feature Walkthrough: Multiple Event Streams

Assume syntax direction:

```regex
event quote { int px; int sz; };
event trade { int px; int qty; };
pred X = quote.px > trade.px;
```

### 29.1 TIR extension

1. Field IDs become `(StreamId, FieldId)` pairs.
2. predicate leaves include stream source.

### 29.2 PIR impact

1. unchanged structurally; leaves just carry stream-aware load ops.

### 29.3 AIR impact

1. none in automata math.

### 29.4 BIR/backend impact

1. process API shape changes (`process_quote`, `process_trade`, unified dispatcher, or tagged union input).
2. lowering strategy chooses how stream updates affect state transitions.

### 29.5 Design alternatives

Alternative A:

1. single process function with tagged union input

Alternative B:

1. separate entrypoints per stream

Alternative C:

1. templated stream handlers

Recommendation:

1. start with explicit separate entrypoints for clarity and profiling.

---

## 30. Design FAQ and Decision Rationale

### 30.1 Why not implement all layers now?

Because sequencing risk is real. Build what unlocks immediate value, keep future layers as extraction targets.

### 30.2 Why not skip TIR and jump to PIR?

PIR without fully resolved typing and binding invites hard-to-debug semantic errors.

### 30.3 Why keep SymbolTable if TIR exists?

SymbolTable is the binding workspace. TIR is the product of binding.

### 30.4 Why not use BDD immediately?

BDD can be powerful, but it introduces ordering complexity. Start with DAG+CSE, measure, then decide.

### 30.5 Why IDs instead of pointers?

IDs improve cache locality, determinism, and pass-to-pass robustness.

### 30.6 Why immutable passes?

Immutability dramatically reduces hidden coupling and improves reproducibility.

---

## 31. Implementation Checklist by Milestone

### 31.1 Milestone M1 (TIR foundation)

1. strong typed ID wrappers
2. name interner
3. span map
4. TIR builder and dump
5. parity tests still passing

### 31.2 Milestone M2 (PIR foundation)

1. canonical node key definition
2. hash-cons table
3. predicate DAG builder
4. PIR dump
5. CSE validation tests

### 31.3 Milestone M3 (AIR integration)

1. AIR data model finalized with backend-neutral bitsets
2. Glushkov path producing AIR
3. AIR dump
4. automata parity tests

### 31.4 Milestone M4 (BIR + scalar switch)

1. BIR op set finalized for scalar path
2. scalar emission from BIR
3. differential semantic parity against old path
4. benchmark sanity checks

### 31.5 Milestone M5 (AVX2 from BIR)

1. AVX2 emitter from BIR
2. correctness tests
3. benchmark and profiling report
4. duplication metrics collection for KIR/LIR extraction gate

### 31.6 Milestone M6 (checkpoint)

1. evaluate Section 18 extraction gates
2. decide keep-BIR or split-KIR/LIR
3. publish decision note in docs

---

## Summary

The proposed model is not "more abstractions for their own sake." It is a boundary discipline:

1. Symbol table resolves names once.
2. TIR freezes semantics.
3. PIR optimizes predicate algebra.
4. AIR captures regex automata semantics.
5. BIR is the immediate backend IR for migration execution.
6. KIR/LIR remain the planned extraction path when justified by evidence.
7. Emitter prints syntax only.

This design directly addresses roadmap goals while reducing repeated edits across unrelated modules.
