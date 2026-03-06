# Aion Benchmarking

> **No benchmarks are currently implemented.** This document describes the
> benchmarking approach that will be used once the codegen backend is
> sufficiently complete. It is published now so that the methodology is visible
> and open to scrutiny before any numbers exist.

---

## What Is Being Measured

Aion's hot path is the generated `process_event(const Event&)` function. On
every call it does four things:

1. Evaluates all predicates and packs the results into a bitmask `M`
2. Propagates the current state bitset `S` through the precomputed follow table
   to produce `S_topo`
3. Masks `S_topo` with `M` to produce the next state `S`
4. Returns whether any accepting state is set in `S`

That is the complete runtime. Everything except the state bitset (a single
`uint64_t`) is `static constexpr`. The benchmark measures the throughput of
this function under a range of conditions.

---

## Comparison Points

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

The goal of these comparisons is honesty, not winning. If RE2 is faster on a
given task, that will be reported and explained. If Hyperscan is faster, the
same. The purpose of the benchmark suite is to quantify the tradeoffs, not to
make Aion look good.

---

## The Four Parameters

Every benchmark varies along four axes:

**1. NFA size (number of symbol positions)**
The Glushkov construction produces exactly `|re| + 1` states, where `|re|` is
the number of symbol occurrences in the regex. All states fit in a `uint64_t`,
so the current maximum is 63 positions. The benchmarks will cover small (≤8),
medium (16–32), and large (48–63) position counts to characterise how
throughput scales with NFA size.

**2. Number of predicates**
More predicates means more field reads and comparisons per event before the
NFA step. The benchmarks separate predicate evaluation cost from follow-table
propagation cost, since these scale differently with regex complexity.

**3. Event struct size**
A larger struct means more data to touch per event. Benchmarks will run with
small structs (a few integer fields) and larger structs (including string
fields) to characterise cache effects.

**4. Codegen strategy (scalar vs. AVX2)**
The scalar backend uses portable `uint64_t` bitwise operations. The AVX2
backend vectorises predicate evaluation for regexes with many predicates.
Benchmarks will measure the crossover point where AVX2 becomes beneficial.

---

## Event Generation

Benchmarks use a synthetic event stream rather than captured production data.
This is deliberate: it makes results reproducible without requiring proprietary
data, and it allows precise control over the match rate (how often the pattern
fires).

The event generator produces streams with configurable:

- **Match rate** — fraction of positions where the pattern accepts. A match
  rate of 0% (the engine never accepts) and 100% (the engine always accepts)
  are both tested, along with realistic intermediate rates.
- **Field distributions** — uniform random, skewed, or constant-field streams
  to stress different parts of the predicate evaluation path.
- **Stream length** — short bursts (thousands of events) and long streams
  (millions of events) to distinguish startup costs from sustained throughput.

---

## Measurement Methodology

All benchmarks use [Google Benchmark](https://github.com/google/benchmark).

**What is timed:** only the `process_event` call loop. Event generation,
engine construction, and result collection are outside the timed region.

**What is not timed:** compilation (Aion's compilation happens offline; the
generated header is compiled once into the benchmark binary). Engine
construction is `constexpr`-initialised, so it has no runtime cost.

**Preventing dead-code elimination:** the return value of `process_event` is
accumulated into a `volatile` sink to prevent the compiler from eliding the
computation.

**CPU frequency stability:** benchmarks are run with frequency scaling
disabled (`cpupower frequency-set -g performance` on Linux) and with
`BENCHMARK_MIN_TIME` set high enough to amortise any warm-up variation.

**Reported metrics:**

| Metric | What it tells you |
|---|---|
| Throughput (events/sec) | Sustained processing rate |
| Latency per event (ns) | Per-call cost |
| σ / CV | Measurement stability |

Results will be reported with mean, standard deviation, and the exact hardware,
compiler, and flag configuration used.

---

## Reproducing Results

Once benchmarks are implemented, the full reproduction steps will be:

```bash
# Configure with the release preset (O3 + LTO)
cmake --preset release
cmake --build --preset release --target aion_benchmarks

# Disable frequency scaling (requires root; Linux only)
sudo cpupower frequency-set -g performance

# Run
./build/release/bin/aion_benchmarks --benchmark_out=results.json \
                                    --benchmark_out_format=json

# Restore
sudo cpupower frequency-set -g powersave
```

The exact benchmark binary name and available filter flags will be documented
here when the suite is added.

---

## What the Benchmarks Will Not Show

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

## Future Work

The initial benchmark suite covers the scalar codegen backend. Additional
benchmarks are planned as features land:

- **AVX2 backend** — vectorised predicate evaluation and follow-table
  propagation; measuring at what NFA size and predicate count AVX2 wins over
  scalar.
- **BDD-based predicate minimisation** — if two predicates share field reads,
  the minimiser can fold them. Benchmarks will quantify the win from predicate
  reduction.
- **State partitioning** — the partitioner groups NFA states with identical
  follow sets; benchmarks will measure how much this reduces work in the
  follow-table propagation step.