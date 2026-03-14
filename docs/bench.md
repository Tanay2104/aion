# Aion Benchmarking

> **Status (March 13, 2026): Synthetic benchmarks are implemented.** The
> current suite covers generated scalar engines (jitter/no-jitter where
> applicable), hand-written baselines for selected groups, throughput harnesses,
> and latency harnesses. RE2/Hyperscan comparisons are deferred for later.

---
## Table of Contents 
1. [What Is Being Measured](#1-what-is-being-measured)
2. [Comparison Points](#2-comparison-points)
3. [The Four Parameters](#3-the-four-parameters)
4. [Benchmark Suite](#4-benchmark-suite) 
5. [The Event Generator](#5-the-event-generator)
6. [Reproducing Results](#6-reproducing-results)
7. [What the Benchmarks Will Not Show](#7-what-the-benchmarks-will-not-show)
8. [Latest Analysis](#8-latest-analysis)
## 1. What Is Being Measured

Aion's hot path is the generated `process_event(const Event&)` function. On
every call it does four things:

1. Evaluates all predicates and packs the results into a bitmask `M`
2. Propagates the current state bitset `S` through the precomputed follow table
   to produce `S_topo`
3. Masks `S_topo` with `M` to produce the next state `S`
4. Returns whether any accepting state is set in `S`

That is the complete runtime. Everything except the state bitset (a single
`uint64_t`) is `static constexpr`. The benchmark measures the throughput and latency of
this function under a range of conditions.

---

## 2. Comparison Points

Aion is **not** a general-purpose string regex engine, so comparisons with
RE2, Hyperscan, or PCRE require care. Those engines operate on byte streams;
Aion operates on typed C++ structs. A fair comparison requires an equivalent
task to be constructed for each engine.

The planned comparison targets are:

**RE2** — used as a baseline for NFA-based matching throughput. The comparison
task is: encode each event as a fixed-width string, run RE2 on it, and measure
end-to-end latency including serialisation. This isolates how much Aion saves
by eliminating the serialisation step and working directly on the struct.

**Hyperscan** — used as a ceiling for what a heavily SIMD-optimised,
production-hardened regex engine can achieve. Hyperscan's compilation model
(offline compile, online scan) is the closest architectural analogue to Aion's
approach of doing all NFA work ahead of time.

**Hand-written state machine** — the most relevant comparison. A skilled
engineer writing a purpose-built boolean state machine for the same pattern
should produce code of similar quality to what Aion generates. If Aion's
output is significantly slower, that is a codegen bug, not an inherent
limitation of the approach.

If RE2 is faster on a given task, that will be reported and explained. If
Hyperscan is faster, the same. The purpose of the benchmark suite is to
quantify the tradeoffs.

---

## 3. The Four Parameters

Every benchmark varies along four axes:

**1. NFA size (number of symbol positions)**
The Glushkov construction produces exactly `|re| + 1` states, where `|re|` is
the number of symbol occurrences in the regex. All states fit in a `uint64_t`,
so the current maximum is 63 positions. The benchmarks will cover small (≤6),
medium (4-24), and large (24+) position counts to characterise how
throughput scales with NFA size.

**2. Complexity of predicates and their evaluation cost**
Complex predicates means more field reads and comparisons per event before the
NFA step. The goal is to understand how much of the processing cost 
is due to the predicate evaluation only, and how does that scale with increasing complexity.
There relationships point to various code optimisation strategies, such as minimisation,
temporary variable allocation, fully inlined approaches, etc.
The benchmarks separate predicate evaluation cost from follow-table
propagation cost, since these scale differently with regex complexity.

**3. Follow table density**
This refers to the number of non-zero entries in `follow[]`. Note that by non-zero entries
we do not mean the number of non-zero bitsets in the array `follow`, but rather the number
of non-zero bits in an average bitset. A sparse follow table, such as  a simple linear sequence
`A . B . C . D` has O(N) non-zero entries, since each alphabet can only lead to the next one,
and hence it's bitset contains only one active bit. A complex pattern such as `(A | B | C | D)*`
contains O(N<sup>2</sup>) active bits in a bitset, since each alphabet can be followed by any other 
alphabet. Density affects how many bits get set in `S_topo` per event. Because of that, density also 
measures how quickly the engine moves towards an accepting state.
---

**4. State Occupancy**
The number of bits active in `S` at runtime. In the jitter mode, the while-loop iterates
over `S` O(|active bits|) number of times(in fact, it is exactly active bits). In no-jitter
mode, it iterates over `S` for all states, which is a fixed compile time constant. The maximum 
active bits in S only depend on the automaton structure, which can dictate the max number of 
concurrent states we can be in. For example,

- A simple sequence `A . B . C` has at most 1 bit set in `S` at any time (the engine
  is always in exactly one state).
- A Kleene with multiple alternatives `(A | B | C)* . D` can have all three of
  positions A, B, C simultaneously active after the first event.
- A union of parallel alternatives `(A . B) | (A . C) | (A . D)` can have positions
  B, C, D all active simultaneously after seeing A.

However, the number of active states over a time period very much
depends on the selectivity(the number of true predicates). Selectivity controls how often 
partial matches are started and how often full matches are completed. In practice, selectivity
has been found to play an important role in state occupancy.

## 4. Benchmark Suite

### 4.1 Shared Event and Predicate Declarations

All benchmarks use the following event schema and predicates for consistency.
This is `benchmarks/regex/shared_predicates.regex`:

```
event { int x; char y; int ts; int valid; };

// Atomic predicates
pred P_hi    = (x > 100);
pred P_lo    = (x < 30);
pred P_mid   = (x >= 30) && (x <= 100);
pred P_ya    = (y == 'a');
pred P_yb    = (y == 'b');
pred P_yc    = (y == 'c');
pred P_valid = (valid == 1);
pred P_early = (ts < 50);
pred P_late  = (ts > 150);

// Composite predicates
pred A = P_valid && P_hi  && P_ya;
pred B = P_valid && P_mid && P_yb;
pred C = P_lo   && (P_yb || P_yc);
pred D = P_early && P_mid;
pred E = P_late  && P_hi;
```

---

### 4.2 Group S — Sequence Length

**File:** `benchmarks/regex/group_s.regex`

**Purpose:** Probe how throughput scales with number of NFA states. Isolates
state count with everything else held constant.

```
regex S2  = A . B;
regex S4  = A . B . A . B;
regex S8  = A . B . A . B . A . B . A . B;
regex S16 = A . B . A . B . A . B . A . B . A . B . A . B . A . B . A . B;
regex S32 = A . B . A . B . A . B . A . B . A . B . A . B . A . B . A . B . A . B . A . B . A . B . A . B . A . B . A . B . A . B . A . B;
```

**What is varying** Selectivity at `{50%, 100%}`. At 100%, every event satisfies either
`A` or `B` (alternating). Thus, multiple matches are started and the number of active bits 
of `S` is high. At 50%, half the events satisfy neither predicate, testing the non-matching fast path.

**Expected output:** Since the number of states increases for both the jitter mode and the no-jitter
mode, we expect that output with decay linearly with size of regex.

The interesting question is where can we identify the crossing over point, where amortized speed of jitter
mode is overpowered by the branch mispredictions because of the `while` loop and the C++ compiler optimisations
of the `for` loop.
---

### 4.3 Group K — Kleene and State Occupancy

**File:** `benchmarks/regex/group_k.regex`

**Purpose:** Probe the impact of follow table density. Kleene stars, especially
when combined with unions, degrade expected follow table bits to O(N<sup>2</sup>)

```
// Exponential occupancy sweep: widen alternatives in the star body.
regex K_2  = (T01 | T02)* . P_late;
regex K_4  = (T01 | T02 | T03 | T04)* . P_late;
regex K_8  = (T01 | T02 | T03 | T04 | T05 | T06 | T07 | T08)* . P_late;
regex K_16 = (T01 | ... | T16)* . P_late;
regex K_32 = (T01 | ... | T32)* . P_late;
```

**Varying parameters:** Pattern structure (K_2, K_4, K_8, K_16, K_32) controls maximum active bits in `S`.
Selectivity varying across `{10%, 50%, 90%}` controls how often the kleene body fires, and basically contributes to 
achieving the maximum active bits of `S` dictated by the follow table.

**Expectations:** In jitter mode, throughput should degrade as you move from K_2 to
K_4 to K_8 to K_16 to K_32, because the while-loop iterates more. In no-jitter mode, throughput should
remain more or less the same and p99 latency should be flat. 

The crossover — where no-jitter mode becomes faster than jitter mode in terms of average throughput would 
be an interesting point.
---

### 4.4 Group P — Predicate Evaluation Cost

**File:** `benchmarks/regex/group_p.regex`

**Purpose:** Determine whether the engine is predicate-bound or NFA-bound. Isolates
predicate cost with other things constant and a simple regex structure.

```
// All five use the same 4-state repeated-symbol structure.
// Predicate complexity is the variable being stressed.

pred SIMPLE        = (x > 100);
pred MODERATE      = (x > 100) && (y == 'a');
pred MEDIUM        = (x > 100) && (y == 'a') && (valid == 1);
pred COMPOUND      = (x > 95) && (x < 170) && (valid == 1) && ((y == 'a') || (y == 'b')) && (ts < 120)
pred VERY_COMPOUND = ((x > 90) && (x < 170) && (valid == 1) && ((y == 'a') || (y == 'b')) && (ts < 120)) ||
                     ((x > 140) && (valid == 1) && (ts < 50) && !(y == 'c')) ||
                     ((x > 100) && (valid == 1) && (y == 'a'));

regex P_simple   = SIMPLE   . SIMPLE   . SIMPLE   . SIMPLE;
regex P_moderate = MODERATE . MODERATE . MODERATE . MODERATE;
regex P_medium   = MEDIUM   . MEDIUM   . MEDIUM   . MEDIUM;
regex P_compound = COMPOUND . COMPOUND . COMPOUND . COMPOUND;
regex P_very_compound = VERY_COMPOUND . VERY_COMPOUND . VERY_COMPOUND . VERY_COMPOUND;

```

**Varying parameters:** Selectivity at `{50%}` only. The goal is to see predicate cost
in isolation, not to vary match frequency.

**Expectations:** All five compile to an identical 4-state repeated-symbol NFA, so
differences isolate predicate complexity directly. VERY_COMPOUND adds an extra
stress point for heavy boolean logic. If throughput
is flat across SIMPLE → MODERATE → MEDIUM, the engine is NFA-bound (follow-table
propagation dominates). If throughput drops, the engine is predicate-bound (the `M`
computation dominates). COMPOUND and VERY_COMPOUND should widen that gap if predicate work is the
bottleneck.

**Comparison and Question:** Hand-written bitset NFA. We want to understand that if we are sure of a simple
regex structure but complex predicates and write a custom bitset based engine, how well does
the C++ compiler optimise our specific code wrt Aion's generated code?

---

### 4.5 Group W — Wildcard Density

**File:** `benchmarks/regex/group_w.regex`

**Purpose:** Wildcards (`_`) emit `M |= ((0ULL - true) & (1ULL << pos))` — that bit
is unconditionally set in `M` every event. States corresponding to wildcards are always
in `S_topo`. More wildcards means more permanently active states, regardless of input.
This probes Parameter 4 (state occupancy).

```
regex W0  = A . B . C;
regex W1  = A . _ . C;
regex W2  = A . _ . _ . C;
regex W4  = A . _ . _ . _ . _ . C;
regex W8  = A . _ . _ . _ . _ . _ . _ . _ . _ . C;
regex W16 = A . _ . _ . _ . _ . _ . _ . _ . _ . _ . _ . _ . _ . _ . _ . _ . _ . C;
regex W32 = A . _ . _ . _ . _ . _ . _ . _ . _ . _ . _ . _ . _ . _ . _ . _ . _ . _ . _ . _ . _ . _ . _ . _ . _ . _ . _ . _ . _ . _ . _ . _ . _ . C;
```

*Varying parameter:** Selectivity on A and C at `{10%, 50%}`. The wildcard states are
always active, but A and C selectivity controls when match attempts start and end.

**What to expect:** Jitter mode should degrade from W0 to W32 because more bits
are permanently set in `S`, so the while-loop iterates more often on average, and also has higher mispredictions. No-jitter mode
should degrade more gently (the extra states add fixed iterations).
Moreover, the degradation of jitter mode with increase in selectivity should be quite high 
since the number of active matches increases more in proportion to closed matches.
---

### 4.6 Group R — Real Pattern

**File:** `benchmarks/regex/group_r.regex`

**Purpose:** End-to-end characterisation of a somewhat realistic pattern that exercises all four
parameters simultaneously. This is one of the headline number benchmarks.

```
regex R_real = (D . (A | I) . (A | B | F)*) . ((G | _) . (B | C))* . ((I | B) . (F | C))* . (C . C . (F | B)) . (H | E) . (J . (E | J));
```

This pattern models a somewhat realistic monitoring pipeline:
1. Early pre-check activity starts (`D`).
2. High-value or suspicious-valid behavior accumulates (`A`, `I`, `F`, `B`).
3. Optional noisy interleavings occur (`G | _`).
4. Two low-value confirmations are required (`C . C` plus a follow-up gate).
5. Late-stage confirmation closes the sequence (`H`, `E`, `J`).


**Varying parameters:** Selectivity combinations:
- Low: 5% for D, 5% for A, 5% for B, 5% for C, 5% for E (rare matches)
- Medium: 20% for all predicates
- High: 50% for all predicates

Stream sizes: 1M, 10M, 100M events. The 100M run establishes steady-state throughput
without startup effects. The 1M run is mainly for quick iteration.
---

### 4.7 Complete Benchmark Matrix

| ID | Pattern | Selectivity | Stream Size | Comparison |
|---|---|---|---|---|
| S2, S4, S8, S16, S32 | Sequences | 50%, 100% | 10M | Hand-NFA (RE2/Hyperscan deferred) |
| K_2, K_4, K_8, K_16, K_32 | Kleene + union | 10%, 50%, 90% | 10M | Jitter vs no-jitter |
| P_simple, P_moderate, P_medium, P_compound, P_very_compound | Predicate-cost sweep | 50% | 10M | Hand-NFA |
| W0, W1, W2, W4, W8, W16, W32 | Wildcards | 10%, 50% | 10M | Jitter vs no-jitter |
| R_real (low / med / high) | Full real pattern | 5%, 20%, 50% | 1M, 10M, 100M | Jitter vs no-jitter |

Total configurations: 10 + 15 + 5 + 14 + 18 = **62 configurations**, each run 5 times.

See benchmarks/run_synthetic.sh to run the full bench matrix, or change bench repetitions.
---

## 5. The Event Generator

All benchmarks use synthetic generated event streams. We do not use external datasets because:

1. Synthetic streams with controlled selectivity let us vary exactly one variable at a
   time. Real datasets have fixed statistical properties that we cannot control.
2. Reproducibility is exact — anyone can regenerate the identical input stream from
   the seed and parameters, with no external download required.
3. Aion's correctness does not depend on the semantic content of events, only on which
   predicates fire. A sensor reading of 150 and a stock price of 150 are identical
   from the engine's perspective.

### Generator Specification

```cpp
struct GenParams {
    double sel_hi;    // P(x > 100)  — controls P_hi, affects A, E, F
    double sel_lo;    // P(x < 30)   — controls P_lo, affects C
    double sel_mid;   // P(30<=x<=100) — controls P_mid, affects B, D
    double sel_ya;    // P(y == 'a') — affects A
    double sel_yb;    // P(y == 'b') — affects B, C
    double sel_valid; // P(valid==1) — affects A, B
    double sel_early; // P(ts < 50)  — affects D
    uint64_t seed;    // fixed for reproducibility
};

std::vector<Event> generate(size_t n, GenParams p) {
    std::mt19937_64 rng(p.seed);
    std::vector<Event> events;
    events.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        Event e;
        auto coin = [&](double prob) -> bool {
            return std::bernoulli_distribution(prob)(rng);
        };
        // x: mutually exclusive ranges, sel_hi takes priority
        if      (coin(p.sel_hi))  e.x = 150;
        else if (coin(p.sel_lo))  e.x = 15;
        else                      e.x = 65;
        // y
        if      (coin(p.sel_ya))  e.y = 'a';
        else if (coin(p.sel_yb))  e.y = 'b';
        else                      e.y = 'c';
        e.valid = coin(p.sel_valid) ? 1 : 0;
        e.ts    = coin(p.sel_early) ? 25 : 175;
        events.push_back(e);
    }
    return events;
}
```

**Fixed seed:** `0xdeadbeef'600dcafe` (64-bit).
We pre-generate all streams into `std::vector<Event>` before the benchmark loop starts.

## 6. Reproducing Results

Reproduction steps:

```bash
# Configure with benchmarks enabled
cmake --preset release -DAION_BUILD_BENCHMARKS=ON
cmake --build --preset release --target \
    aion_bench_throughput_jitter aion_bench_throughput_nojitter \
    aion_bench_latency_jitter aion_bench_latency_nojitter

# Disable frequency scaling (requires root; Linux only)
sudo cpupower frequency-set -g performance

# Run full synthetic matrix + summary
# To pin core, set AION_BENCH_CPU_CORE env variable
benchmarks/scripts/run_synthetic.sh release

# Restore
sudo cpupower frequency-set -g powersave
```

Results are emitted under `benchmarks/results/<timestamp>/`.
The summarizer now also emits ready-to-use PNG plots under
`benchmarks/results/<timestamp>/plots/` (when `matplotlib` is installed).

---

## 7. What the Benchmarks Will Not Show

It is worth being explicit about the limits of this benchmark suite.

**They do not measure compilation time.** Aion's compilation (the `.regex` →
C++ step) happens offline. Compile time matters for developer iteration speed
but not for runtime performance. It will be tracked separately.

**They do not measure match quality.** A benchmark that fires an accept on
every event is not representative of a useful pattern. Match rate is a
controlled parameter, not an afterthought.

**They do not measure worst-case latency.** Mean throughput benchmarks
characterise average behaviour. If you need hard latency bounds (e.g., for a
real-time system), you need a separate tail-latency analysis. That is planned
but not part of the initial suite.

**They do not cover patterns larger than 63 symbol positions.** This is a
current hard limit of the bitset NFA representation. If your pattern exceeds
this, Aion will refuse to compile it.
---


## 8. Latest Analysis

The dated analysis for the latest fully plotted run now lives in
[`benchmarks/results/20260313_193528/analysis.md`](../benchmarks/results/20260313_193528/analysis.md).

That note analyses `benchmarks/results/20260313_193528/` and the generated
plots under `benchmarks/results/20260313_193528/plots/`.
