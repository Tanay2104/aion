# Aion Testing Guide

Aion uses a four-layer testing model. Each layer has a distinct purpose and
a distinct failure vocabulary. Understanding which layer to reach for — and
why — is the main thing this document conveys.

Tests are treated as part of the product surface. If the compiler is going to
be used in latency-sensitive pipelines, the test suite must reflect that
standard.

---

## The Four Layers

```
Unit       →  did the logic break?
Contract   →  did a behavioral guarantee break?
E2E        →  would a user observe a break?
Semantic   →  does the generated engine produce correct match outcomes?
```

All four are necessary. A bug that passes unit tests but breaks a user-visible
guarantee is a contract failure. A bug that passes contract tests but generates
an engine that accepts the wrong events is a semantic failure. The layers are
not redundant — they catch different classes of problem.

---

## Directory Structure

```
tests/
├── unit/               # Per-module logic tests
├── contract/           # Stage-boundary and API contract tests
├── e2e/
│   └── cases/          # .regex fixture files for E2E tests
├── semantic/
│   └── cases/          # .regex fixture files for semantic tests
└── CMakeLists.txt
```

Four test binaries are produced by the build:

| Binary | Layer |
|---|---|
| `aion_unit_tests` | Unit |
| `aion_contract_tests` | Contract |
| `aion_e2e_tests` | E2E |
| `aion_semantic_tests` | Semantic |

---

## What Each Layer Covers

### Unit (`tests/unit/`)

Verifies the internal logic of individual compiler stages in isolation. These
tests are allowed to inspect internal data structures directly.

Current coverage:

- Lexer tokenisation rules and edge cases
- Parser precedence and error recovery behaviour
- Symbol table: field offset/size assignment, redeclaration handling, name
  conflict resolution
- Alphabet extraction and position-ID mapping
- Glushkov NFA construction invariants (state counts, follow-set correctness)

Characteristics: targeted assertions, no subprocess execution, fast.

### Contract (`tests/contract/`)

Verifies behavioural guarantees at stage boundaries and at the compiler's
public API surface. These tests focus on what a stage promises to the next
stage or to a caller — not on internal implementation details.

Current coverage:

- Pipeline consistency: tokens → AST → symbol table → NFA
- Dump output contracts: presence and structural shape of `--dump` artifacts
- CLI option contracts: `--dump`, `--emit`, verbosity flag behaviour
- Logging gate behaviour across verbosity levels (V0–V3)

Characteristics: stable behavioural assertions with minimal implementation
coupling; still in-process, but focused on observable contracts.

### End-to-End (`tests/e2e/`)

Invokes `aionc` as an external subprocess and validates only what a user can
observe: exit codes, stdout/stderr content, and generated files. E2E tests
never inspect compiler internals.

Current coverage:

- Valid `.regex` inputs compile and produce the expected artifacts
- Invalid inputs produce correct diagnostics
- `--dump` mode generates the expected output files
- Verbosity output appears at the correct tiers
- Output is deterministic across repeated invocations
- Regression guards for previously discovered lexer/parser edge cases

Fixtures live in `tests/e2e/cases/` as `.regex` files. Each E2E test spawns
the compiler in a sandboxed temp directory that is cleaned up automatically.
To preserve the sandbox for debugging a specific failure:

```bash
AION_E2E_KEEP_TMP=1 ./build/debug/bin/aion_e2e_tests
```

### Semantic (`tests/semantic/`)

Verifies that the *generated engine* produces correct match outcomes — that
the right events are accepted and the right events are rejected. This is
distinct from all the above: a compiler can produce syntactically valid C++
that implements the wrong NFA.

Each semantic test case is a fixture consisting of:

- A `.regex` source file
- An event trace (a sequence of typed events)
- An expected boolean timeline (per-event accept/reject ground truth)

Ground truth is established by two independent oracles:

**Oracle A** — a C++ evaluator that walks the AST and applies dynamic
programming directly to the event trace, without invoking any Aion codegen.

**Oracle B** — an independent Python implementation of the same algorithm,
written against the language specification rather than the Aion source.

A semantic test passes only when: Oracle A agrees with Oracle B, and the
generated engine agrees with both. Any divergence is a semantic bug.

Additional coverage:

- `Engine_*::reset()` parity: replaying a trace from scratch after a reset
  must produce identical outcomes
- Metamorphic checks: properties that must hold regardless of input (e.g.,
  union commutativity, Kleene star idempotence)

The Python oracle tests require `python3` to be available in `PATH`. If absent, those specific checks are skipped.---

## Running Tests

### Build

```bash
# Debug
```bash
cmake --preset debug -DAION_BUILD_TESTS=ON
cmake --build --preset debug \
  --target aion_unit_tests aion_contract_tests \
           aion_semantic_tests aion_e2e_tests
```
```

### Run Everything

```bash
ctest --test-dir build/debug --output-on-failure
```

### Run a Specific Layer

```bash
./build/debug/bin/aion_unit_tests
./build/debug/bin/aion_contract_tests
./build/debug/bin/aion_e2e_tests
./build/debug/bin/aion_semantic_tests
```

### Filter by Test Name

```bash
./build/debug/bin/aion_unit_tests      --gtest_filter='Lexer*'
./build/debug/bin/aion_contract_tests  --gtest_filter='*Dump*'
./build/debug/bin/aion_e2e_tests       --gtest_filter='*Verbosity*'
./build/debug/bin/aion_semantic_tests  --gtest_filter='*Oracle*'
```

---

## Adding New Tests

When adding or fixing behaviour, choose the lowest layer that can encode the
guarantee, then add higher-layer coverage only if the behaviour is
user-visible or affects match correctness.

| What changed | Minimum coverage |
|---|---|
| Parsing edge case | Unit; contract if it crosses a stage boundary |
| CLI or dump behaviour | Contract + E2E |
| Semantic behaviour | Semantic; contract/E2E if user-visible |
| Match correctness regression | Semantic at minimum; all layers if the failure was observable |

Test names should be explicit and behaviour-focused, following the pattern
`SubjectConditionExpectedOutcome`. For example:
`StringLiteralTrueRemainsStringNotBoolInTokenDump`,
`NfaDumpAfterRecoveryContainsOnlyValidRegex`.

Quality rules:
- One behavioural contract per test
- Deterministic fixtures — no dependence on time, randomness, or network
- No hidden global state between tests
- Failure output should be immediately actionable without reading source

---

## Design Decisions

### Separate Binaries Per Layer

Unit, contract, E2E, and semantic tests are intentionally separate binaries
rather than a single combined test executable. This makes it possible to run
only the relevant layer during development (unit tests during a refactor,
semantic tests after a codegen change), and allows CI to run faster layers
first and gate on them before running slower ones.

### Stable Assertions Over Snapshots

Tests prefer presence checks, structural invariants, and count assertions over
exact full-output snapshots. Snapshots that encode irrelevant formatting
details break on harmless changes and erode confidence in the suite. Snapshots
are used only where the output format is genuinely stable and the exact
content is the contract being tested.

### E2E Tests Are Strictly Black-Box

E2E tests observe only what a user can observe. They do not import compiler
internals, inspect AST nodes, or assert on intermediate representations. This
makes E2E tests resilient to internal refactors and ensures they remain a
meaningful proxy for user experience.
