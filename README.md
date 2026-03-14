# Aion

> **⚠️ Status: Active Development — Pre-Alpha**
> Aion is under heavy construction. The compiler pipeline is functional for the core
> subset of the language, but many features are incomplete, APIs are unstable, and
> the benchmark suite is still synthetic-only. Expect breaking changes.

Aion is a **transpiler** that transforms a concise, domain-specific `.regex` language
into a zero-allocation, branch-minimised C++ event-matching engine. You describe your
event schema, define boolean predicates over its fields, and write regular expressions
over those predicates. Aion emits a self-contained C++ header that you can drop into
any latency-sensitive, allocation-free codebase.

---

## Table of Contents

1. [What Problem Does Aion Solve?](#1-what-problem-does-aion-solve)
2. [What Aion Is Not](#2-what-aion-is-not)
3. [How It Works — The Pipeline](#3-how-it-works--the-pipeline)
4. [The `.regex` Language](#4-the-regex-language)
5. [Generated Code and Runtime Model](#5-generated-code-and-runtime-model)
6. [Project Structure](#6-project-structure)
7. [Building](#7-building)
8. [Running the Compiler](#8-running-the-compiler)
9. [Examples](#9-examples)
10. [Testing](#10-testing)
11. [Benchmarking](#11-benchmarking)
12. [Design Decisions](#12-design-decisions)
13. [Contributing](#13-contributing)
14. [Roadmap](#14-roadmap)

---

## 1. What Problem Does Aion Solve?

Many embedded, real-time, and high-throughput systems need to detect patterns
in streams of typed events — things like:

- "A sensor armed, then triggered, then the alert was not acknowledged within N
  steps"
- "A robot received a move command followed by a stop command with no obstacle
  detected in between"
- "A trade had a specific strategy flag set, then a position was opened, then
  closed, all without an error event"

Writing these detectors by hand produces brittle, hard-to-test state machine
code that couples business logic to infrastructure. Using a general-purpose
string regex engine requires serialising and deserialising events just to get a
string to match against, and brings in a dependency with an allocation model
unsuitable for embedded or real-time use.

Aion solves this by letting you express the pattern in a high-level language
and compiling it down to the smallest possible runtime: a single bitset integer
advanced one step per event, with all tables precomputed and stored as
`static constexpr` data.

---

## 2. What Aion Is Not

It is important to be precise about scope.

**Aion is not a CEP engine.** It does not track multiple simultaneous partial
matches, maintain time windows, aggregate counts, or correlate field values
across events (e.g., `event2.x > event1.x`). Systems like Apache Flink CEP,
SASE, and ZStream solve that problem. Aion does not, by design.

**Aion is not a string regex engine.** It does not consume byte buffers or
character sequences. There is no `std::regex`, PCRE, RE2, or Hyperscan
equivalent here. The "alphabet" is a set of named boolean predicates, not
characters.

**Aion does not run the NFA for you.** It emits C++ code. Your binary embeds
that code and calls it. The generated `process_event` function is the entire
runtime; there is no Aion library to link against.

---

## 3. How It Works — The Pipeline

```
.regex source file
       │
       ▼
  ┌─────────────┐
  │   Frontend  │  Lexer → Parser → AST → Symbol Table
  └──────┬──────┘
         │ typed, resolved AST
         ▼
  ┌─────────────┐
  │   Analysis  │  Alphabet extraction, predicate minimisation
  └──────┬──────┘
         │ alphabet + AST
         ▼
  ┌─────────────┐
  │   Automata  │  Glushkov NFA construction, state partitioning
  └──────┬──────┘
         │ NFA (bitset representation)
         ▼
  ┌─────────────┐
  │   Codegen   │  Scalar / AVX2 strategy selection, C++ emission
  └──────┬──────┘
         │
         ▼
  generated C++ header
```

### Frontend

The lexer tokenises the source, the parser produces a typed AST, and the symbol
table pass resolves all identifiers, assigns byte offsets and sizes to every
event field, and validates declarations (emitting warnings, not errors, for
duplicates — the first declaration wins).

Supported field types and their sizes:

| Type     | Size (bytes)    |
|----------|-----------------|
| `bool`   | 1               |
| `char`   | 1               |
| `int`    | 4               |
| `float`  | 4               |

### Analysis

The alphabet module computes the minimal set of distinct predicate combinations
that the NFA actually needs to distinguish. The minimiser reduces the number of
effective predicate evaluations at match time.

### Automata

Aion uses the **Glushkov NFA** construction (also known as the Berry–Sethi
construction), which builds an NFA directly from the syntax tree without
introducing ε-transitions. This results in an NFA with exactly `|re| + 1`
states (where `|re|` is the number of symbol occurrences in the regex), all of
which are representable as bits in a `uint64_t`. This sets the current hard
limit of 63 symbol positions per regex.

The partitioner groups NFA states into equivalence classes based on their
follow sets, which is the foundation for the codegen optimisation strategies.

### Codegen

Two backend strategies are currently being implemented:

- **Scalar** — standard C++ bitwise operations on `uint64_t`. Fully portable,
  no CPU feature requirements.
- **AVX2** — vectorised path for situations where predicate evaluation becomes
  the bottleneck. Currently a work in progress.

The emitter writes the final C++ header, including the event struct definition,
the precomputed follow table as `static constexpr` data, and the `process_event`
function.

---

## 4. The `.regex` Language

A `.regex` file has three sections that must appear in order: one event
declaration, any number of predicate declarations, and one or more regex
declarations.

### Event Declaration

Declares the C++ struct layout of the events your engine will process.

```
event {
    int     sensor_id;
    float   voltage;
    bool    armed;
    char  label;
};
```

Only one event declaration is permitted per file. Field names must be unique.
Redeclarations generate a warning and are ignored.

### Predicate Declarations

Predicates are named, boolean-valued expressions over event fields. They are
the "alphabet" of the regular language.

```
pred IsArmed       = armed == true;
pred HighVoltage   = voltage > 4.5;
pred KnownSensor   = sensor_id == 42;
pred LabelAlert    = label == 'A'; // for Alert
```

**Supported comparison operators:** `==`, `!=`, `<`, `>`, `<=`, `>=`

Predicates can reference any field of the declared event type. The right-hand
side must be a literal of a compatible type.  Compound boolean expressions are also supported.

### Regex Declarations

Regexes are regular expressions over predicate names.

```
regex ArmThenTrigger = IsArmed . HighVoltage . KnownSensor;
regex AlertLoop      = LabelAlert*;
```

**Operators:**

| Operator | Syntax | Meaning |
|----------|--------|---------|
| Concatenation | `A . B` or `A B` | A followed by B |
| Union | `A \| B` | A or B |
| Kleene star | `A*` | Zero or more repetitions of A |
| Grouping | `(A \| B) . C` | Explicit precedence |

Precedence (highest to lowest): star > concatenation > union. Parentheses
override precedence as usual.

The `true` and `false` literals are also valid regex atoms, matching every
event and no event respectively.

---

## 5. Generated Code and Runtime Model

For a regex named `R` in a file that declares an event struct, Aion emits
approximately the following (simplified for illustration):

```cpp
// --- generated header (do not edit) ---

struct Event {
    int     sensor_id;
    float   voltage;
    bool    armed;
    char    label[MAX_STRING_SIZE];
};

class Engine_R {
public:
    Engine_R() = default;

    bool process_event(const Event& e) {
        // Step 1: evaluate predicates, pack into bitmask
        uint64_t M = 0;
        if (/* IsArmed predicate */)   M |= (1ULL << 0);
        if (/* HighVoltage predicate */) M |= (1ULL << 1);
        // ...
        // Note that in the actual generated code branchless predicate
        // evaluation is used, without if statements.

        // Step 2: propagate state bitset through precomputed follow table
        uint64_t S_next = 0;
        // unrolled / vectorised follow-set propagation using M
        // all table data is static constexpr

        // Step 3: advance state
        state_ = S_next;

        // Step 4: return match
        return (state_ & ACCEPT_MASK) != 0;
    }

    void reset() { state_ = INITIAL_STATE; }

private:
    uint64_t state_ = INITIAL_STATE;
    static constexpr uint64_t INITIAL_STATE = /* ... */;
    static constexpr uint64_t ACCEPT_MASK   = /* ... */;
    // follow tables as static constexpr arrays
};
```

The critical properties of the generated runtime:

- **Zero heap allocation.** The only mutable state is a single `uint64_t`.
- **No virtual dispatch.** `process_event` is a plain non-virtual member.
- **No locks.** Engine instances are not thread-safe by design (one instance
  per thread or per independently tracked stream).
- **No dynamic state construction.** All NFA tables are `static constexpr`,
  computed entirely at compile time by Aion and baked into the application's binary.
- **Constant-time per event.** The cost of `process_event` is bounded by the
  size of the regex (number of symbol positions), not by input length.

---

## 6. Project Structure

```
aion/
├── CMakeLists.txt          # Root build definition
├── CMakePresets.json       # debug / release presets (Clang + libc++)
├── docs/
│   ├── bench.md            # Public benchmark guide
│   ├── bench_results_2026_03_13.md  # Dated benchmark analysis
│   ├── grammar.md          # Formal grammar reference
│   ├── tests.md            # Testing notes
├── examples/
│   ├── main_app.cpp        # Example consumer of a generated header
│   ├── simple.regex        # Minimal example
│   ├── robot.regex         # Robotics command-sequence example
│   └── strategy.regex      # Trading strategy pattern example
├── src/
│   ├── compiler/
│   │   ├── core/           # CompilationContext, diagnostics, types, config
│   │   ├── frontend/       # Lexer, Parser, AST, SymbolTable
│   │   ├── analysis/       # Alphabet extraction, minimisation
│   │   ├── automata/       # Glushkov NFA, partitioner
│   │   ├── codegen/        # Emitter, scalar/AVX2 strategies, event/predicate/regex gen
│   │   └── main.cpp        # Compiler entry point
│   └── utils/              # Argument parser, shared utilities
└── tests/
    ├── unit/               # Lexer, parser, symbol table, alphabet, Glushkov tests
    ├── e2e/                # End-to-end compiler invocation tests
    ├── contract/           # CLI logging, AST dump, and pipeline contract tests
    └── semantic/           # Checking whether the matching output is semantically correct.
```

### Key Module Boundaries

The compiler is structured as C++23 named modules (`.cppm` files). The primary
public interface modules are:

| Module | Responsibility |
|---|---|
| `aion.core` | `CompilationContext`, `Diagnostics`, type system |
| `aion.frontend` | `Lexer`, `Parser`, `AionFile` AST, `SymbolTable` |
| `aion.analysis` | `Alphabet`, `Minimizer` |
| `aion.automata` | `Glushkov`, `NFA`, `Partitioner` |
| `aion.codegen` | `Emitter`, `CodegenStrategy`, scalar/AVX2 backends |
| `aion.utils` | `ArgParser` |

### Documentation

- [`docs/bench.md`](docs/bench.md) - benchmark guide, methodology, and reproduction
- [`docs/bench_results_2026_03_13.md`](docs/bench_results_2026_03_13.md) - latest dated benchmark analysis
- [`docs/grammar.md`](docs/grammar.md) - grammar reference
- [`docs/resources.md`](docs/resources.md) - background reading and references
- [`docs/tests.md`](docs/tests.md) - testing notes

---

## 7. Building

### Prerequisites

| Tool | Minimum Version | Notes |
|------|----------------|-------|
| CMake | 3.28 | Required for C++23 module support |
| Clang / clang++ | 21 | Required. GCC support is not currently configured. |
| libc++ | matching Clang | Set via `-stdlib=libc++` in presets |
| Ninja | any recent | Generator used by all presets |

> **Note:** C++23 standard library module support (`import std;`) is used via
> the experimental `CMAKE_CXX_MODULE_STD` CMake feature. This requires a
> Clang + libc++ combination that ships a pre-built `std` module. Clang 21
> with a matching libc++ is the tested configuration.

### Configure and Build

```bash
# Clone the repository
git clone https://github.com/Tanay2104/aion
cd aion

# Configure (debug build)
cmake --preset debug

# Build
cmake --build --preset debug

# Or for a release build
cmake --preset release
cmake --build --preset release
```

Build artifacts are placed in `build/<preset>/bin/` and `build/<preset>/lib/`.

### Run Tests

```bash
# After building
cd build/debug
ctest --output-on-failure
```

Or with verbose output:

```bash
ctest -V
```

---

## 8. Running the Compiler

After building, the `aion` binary is at `build/<preset>/bin/aion`.

```bash
# Basic usage
./aion <input.regex>

# With explicit output path. 
# The hpp extension is added to output for generated C++ file.
./aion <input.regex> -o <output>

# Verbose / diagnostic output
./aion <input.regex> --verbose

# Dump the AST (useful for debugging)
./aion <input.regex> --dump AST
```

> **Note:** The CLI interface is still being stabilised. Flags and their names
> may change. Consult `--help` for the current list.

---

## 9. Examples

The `examples/` directory contains a few `.regex` files. The
`examples/` subdirectory is not yet wired into the CMake build, so you will need
to invoke the compiler directly.

### `robot.regex`

Models a robotics command-sequence detector: the pattern fires when a specific
sequence of command and sensor events is observed.

### `strategy.regex`

Models a financial strategy pattern over trade event fields.

### `main_app.cpp`

Shows the canonical way to consume a generated header:

```cpp
#include "generated_engine.hpp"   // emitted by Aion

int main() {
    aion::runtime::Engine_MyRegex engine;

    // Feed events one at a time
    for (const Event& ev : event_stream) {
        if (engine.process_event(ev)) {
            // Pattern matched — handle it
        }
    }
}
```

---

## 10. Testing

Tests live under `tests/`. The suite uses **Google Test**.

### Test Categories
To build tests
```bash
cmake --preset release -DAION_BUILD_TESTS=ON
cmake --build --preset release \
  --target aion_unit_tests aion_contract_tests \
           aion_semantic_tests aion_e2e_tests
```

**Unit tests** (`tests/unit/`) test each compiler stage in isolation:

| File | What it tests |
|------|---------------|
| `frontend_lexer_tests.cpp` | Token classification, whitespace handling, literal lexing |
| `frontend_parser_tests.cpp` | AST shape for all grammar productions, error recovery |
| `frontend_symbol_table_tests.cpp` | Field offset/size assignment, duplicate detection, name conflict resolution |
| `analysis_alphabet_tests.cpp` | Alphabet extraction correctness |
| `automata_glushkov_tests.cpp` | NFA state counts, follow-set correctness |

**End-to-end tests** (`tests/e2e/`) invoke the full compiler pipeline on
`.regex` files in `tests/e2e/cases/` and check exit codes, diagnostics, and
(eventually) the shape of the generated output. Cases include:

- `valid_basic.regex` — expected to compile cleanly
- `valid_star.regex` — Kleene star patterns
- `invalid_regex_recovery.regex` — parser error recovery
- `missing_event.regex` — diagnostic for missing event declaration
- `multi_error.regex` — multiple simultaneous errors
- `bool_string_literal.regex` — literal type handling edge cases
- `regression_operator_errors.regex` — operator precedence edge cases

**Contract tests** (`tests/contract/`) verify observable behaviour at the
compiler boundary: CLI flag combinations, AST dump format, and the full
pipeline contract from source text to emitted code.

**Semantic tests** (`tests/semantic/`) verify the semantic validity of generated
matching output. That is, if the engine correctly matched patterns. It uses two 
independent oracles along with truth-table fixtures to verify the semantics.

### Adding Tests

For the frontend, use the helper pattern established in the existing tests:
construct a `CompilationContext` with `Verbosity::NONE`, instantiate `Lexer` and
`Parser` directly, and assert on the resulting AST or diagnostics. This avoids
any I/O and keeps tests fast and hermetic.

---

## 11. Benchmarking

The synthetic benchmark suite is implemented under `benchmarks/`.

To build benchmark binaries:

```bash
cmake --preset release -DAION_BUILD_BENCHMARKS=ON
cmake --build --preset release \
  --target aion_bench_throughput_jitter aion_bench_throughput_nojitter \
           aion_bench_latency_jitter aion_bench_latency_nojitter
```

To run the full synthetic suite and generate summaries:

```bash
# Disable frequency scaling (requires root; Linux only)
sudo cpupower frequency-set -g performance

# Run full synthetic matrix + summary
# To pin core, set AION_BENCH_CPU_CORE env variable
benchmarks/scripts/run_synthetic.sh release
```

Outputs are written to `benchmarks/results/<timestamp>/`:

- `throughput_jitter.json`
- `throughput_nojitter.json`
- `latency_jitter.csv`
- `latency_nojitter.csv`
- `summary.csv`
- `summary.md`
- `plots/`

Cross-engine RE2/Hyperscan comparisons are deferred for now.
Current implementation scope is synthetic Aion-only and handwritten baseline
measurements documented in `docs/bench.md` and
`docs/bench_results_2026_03_13.md`.

---

## 12. Design Decisions

### Why Glushkov and not Thompson?

The Thompson construction introduces ε-transitions, which require either a
closure computation at match time (slow) or an NFA-to-DFA subset construction
(exponential blowup in the worst case). The Glushkov construction produces an
NFA with no ε-transitions and exactly `|re| + 1` states, making it directly
representable as a bitset with no DFA conversion required. At 64 states
(fitting in a `uint64_t`), this is both cache-friendly and branchless.

### Why a compiled header and not a library?

The entire point is zero overhead. A library would require either virtual
dispatch (to handle different regex sizes/structures) or templates with
linkage complexity. A generated header means the compiler sees the full NFA
structure, can inline `process_event` completely, and can verify the generated
code in the context of the calling binary.

### Why C++23 modules?

To enforce clean interface boundaries between compiler stages, reduce
compilation overhead in the test suite, and gain practice with the module
system in a real codebase. The downside is a tighter compiler requirement
(Clang 21 + libc++).

### Why only Clang?

The `import std;` experimental feature and the associated CMake support are most
mature on Clang with libc++ at the time of writing. GCC support is planned once
the CMake/GCC combination stabilises.

### Why use brittle std::string_view everywhere?
Currently Aion processes only one file, and that is opened at Aion runtime(regex compile time).
There is no real harm to using string_view instead of string here, it only saves memory.
If future needs present otherwise, this can always be changed easily.
---

## 13. Contributing

Aion is in pre-alpha. The most useful contributions right now are:

- **Bug reports with minimal `.regex` repros** — especially parser
  crash-or-hang cases, symbol table resolution failures, or incorrect Glushkov
  follow sets.
- **New test cases** — especially for error recovery paths in the parser and
  for edge cases in the alphabet/minimiser analysis.
- **Documentation corrections** — the formal grammar in `docs/grammar.md` and
  the design documents should match the implementation; discrepancies are bugs.

Before opening a pull request, ensure all existing tests pass:

```bash
cmake --build --preset debug && cd build/debug && ctest --output-on-failure
```

The project uses `-Werror` in the Clang preset, so new code must compile
without warnings.

---

## 14. Roadmap

Items are roughly ordered by priority, but the project is single-author and
priorities shift. Nothing here is a commitment. This is a small sketch. A more
detailed discussion can be found at `docs/roadmap.md`.

**Near-term (compiler correctness)**
- [ ] Complete AVX2 codegen backend
- [ ] Wire up the `examples/` directory to the CMake build
- [ ] Support composite predicate expressions (AND / OR / NOT over sub-predicates)

**Medium-term (usability)**
- [ ] Stabilise the CLI flag interface
- [ ] GCC / libstdc++ support
- [ ] LSP-friendly diagnostic output (JSON mode) and syntax highlighting definitions.

**Longer-term (language)**
- [ ] Multi-event files (more than one event struct, for heterogeneous streams)
- [ ] Counted repetition: `A{n}`, `A{n,m}`
- [ ] Named capture / match positions
- [ ] Optional atoms: `A?`

---

## License

This project is under the MIT License.

---

*Aion v0.1.0 — generated header format and `.regex` language are not yet stable.*
