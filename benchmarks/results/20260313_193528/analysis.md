# Aion Benchmark Results - March 13, 2026 Run

This note contains the dated analysis for this run folder:
`benchmarks/results/20260313_193528/`.

One measurement note matters when reading the latency tables: the latency
harness wraps each `process_event()` call with `lfence; rdtsc/rdtscp; lfence`.
So the p50/p95/p99 cycle counts are best interpreted comparatively, as a tail
shape metric, not as a direct conversion of throughput into cycles/event.

## Group S - sequence length

This group separates two different questions:

1. How well does generated scalar code track a hand-written bitset NFA?
2. Where does jitter stop helping and fixed-iteration no-jitter become the
   better tradeoff?

For the generated-vs-hand comparison, the 100% selectivity results are the
cleanest signal. Generated throughput stays within about 9% of the hand
baseline across the whole sweep:

- `S2`: `214.8M` generated vs `221.3M` hand (`-2.9%`)
- `S4`: `182.2M` generated vs `183.4M` hand (`-0.7%`)
- `S8`: `134.8M` generated vs `146.8M` hand (`-8.2%`)
- `S16`: `121.3M` generated vs `118.0M` hand (`+2.7%`)
- `S32`: `88.6M` generated vs `90.3M` hand (`-2.0%`)

That is a good result. For a dense, always-advancing linear pattern, the
generated scalar engine is already very close to code written directly against
the bitset model.

The 50% selectivity case is more mixed. Generated code wins at short lengths,
then loses increasingly badly once the sequence gets long:

- `S2`: `675.8M` generated vs `517.7M` hand (`+30.6%`)
- `S4`: `493.1M` generated vs `469.0M` hand (`+5.1%`)
- `S8`: `247.0M` generated vs `264.9M` hand (`-6.8%`)
- `S16`: `186.4M` generated vs `245.3M` hand (`-24.0%`)
- `S32`: `183.7M` generated vs `268.5M` hand (`-31.6%`)

The most likely explanation is visible in the emitted code: the generated S
engines fully unroll the `M` construction and reevaluate `A`/`B` once per
position, while the hand engine keeps a compact loop. At `S16` and `S32`, that
means much larger hot code in the generated path. Both versions still do the
same logical per-position predicate work, but the generated form gives the CPU
frontend a lot more instruction bytes to chew through once the sequence gets
long. This is an inference from the emitted headers, but it fits the shape of
the curve well.

The jitter-vs-no-jitter comparison is also informative, and it  matches
the original expectation both jitter and no-jitter decay linearly. Some interesting points:

- At `100%`, no-jitter wins across the entire sweep. The gap is already large at
  `S2` (`628.6M` vs `214.8M`) and remains large at `S32` (`161.7M` vs `88.6M`).
- At `50%`, no-jitter wins through `S16`, but `S32` is the first measured
  crossover point where jitter becomes faster: `183.7M` vs `166.3M` (`+10.4%`).

That crossover matters. It says the branchy jitter loop is not universally
worse; it becomes worthwhile once the pattern is long enough and the stream is
sparse enough that skipping dead states saves more work than the `while (S)`
control-flow cost adds. The data therefore supports a conditional story:
no-jitter is better for dense linear sequences and most short/medium cases, but
jitter can retake the lead at the long, sparse end.

## Group K - Kleene occupancy and follow-table density

Group K is the cleanest confirmation of the no-jitter design goal.

In jitter mode, throughput degrades monotonically as the star body widens:

- At `10%`, throughput falls from `206.6M` (`K2`) to `65.0M` (`K32`), a `3.18x`
  slowdown.
- At `50%`, throughput falls from `114.5M` to `47.6M`, a `2.40x` slowdown.
- At `90%`, throughput falls from `225.5M` to `67.8M`, a `3.32x` slowdown.

The latency plots say the same thing from the tail perspective. Jitter p99 grows
with occupancy and width:

- `K2_jitter_50`: `78` cycles p99
- `K16_jitter_50`: `98` cycles p99
- `K32_jitter_50`: `104` cycles p99

No-jitter, by contrast, is almost flat in both throughput and latency:

- Throughput stays around `675M-682M` for `K2` through `K16` across all three
  selectivities.
- Even `K32` stays in the same broad range (`654.5M-674.7M`), with the `50%`
  point affected by the variance note above.
- p99 latency stays in a tight `30-38` cycle band.

The expected throughput crossover never appears in the measured K range. In
fact, no-jitter wins everywhere, and by a large margin:

- At `K2`, no-jitter is already `3.0x-5.9x` faster than jitter depending on
  selectivity.
- At `K32`, the advantage grows to roughly `9.8x-13.7x`.

Another interesting detail is that `50%` selectivity is consistently the worst
point for jitter in this suite. For example, `K32_jitter_50` is slower than
both `K32_jitter_10` and `K32_jitter_90`, and its p99 is also highest
(`104` cycles vs `78` and `90`). The likely reason is that mid selectivity
creates the most mixed control flow: enough body symbols fire to keep occupancy
high, but not so predictably that the branch prediction structure settles into a mostly
dead or mostly-saturated regime.

The practical conclusion is straightforward: occupancy-heavy regexes are where
no-jitter currently has its strongest value proposition. Group K is not a close
contest; it is the regime where fixed work and predictable control flow win
decisively.

## Group P - predicate evaluation cost

Group P is quite an actionable suite because it isolates a backend/codegen
issue, not an automata or language or fundamental architectural issue.

From `simple` through `compound`, generated throughput declines only modestly:

- `simple`: `615.0M`
- `moderate`: `608.8M`
- `medium`: `563.7M`
- `compound`: `559.5M`

So moderate predicate growth is not catastrophic. The engine does lose ground
to the hand baseline as complexity increases, but the gap stays manageable:

- `simple`: generated is `1.2%` slower than hand
- `moderate`: `2.2%` slower
- `medium`: `6.6%` slower
- `compound`: `11.9%` slower

The `very_compound` point is completely different:

- Generated: `131.0M`
- Hand: `459.0M`
- Gap: generated is `71.5%` slower, or only `28.5%` of hand throughput

This is not an NFA topology effect. All five P benchmarks use the same repeated
4-state structure. The difference is entirely in how the predicate result is
produced and reused.

Our current codegen strategy and the generated code directly explains the collapse: the backend evaluates
the same repeated predicate once per position when building `M`. So
`P_very_compound` computes the full boolean expression four times per event.
The hand baseline computes the predicate once, stores the boolean in `match`,
and reuses it for all four positions. That makes the `very_compound` result a
clear codegen deficiency, not a limitation of the Glushkov bitset model.

This is an important optimisation target exposed by the current data.
We need a "compute once, reuse everywhere"
strategy for repeated symbols and composite, complex predicates.

Another direction, perhaps further improving upon the theoretical bdd
minimisation, could be we inline only small or moderate predicates. The function call overhead is real,
especially over a big struct, and small predicates are better inlined. For complex functions,
we should evaluate them once and keep reusing them wherver possible.

In case of many predicates, we can also "roll-back" the predicate loop unrolling and partially rely on
the C++ compiler to unroll it as it deem's fit.

## Group W - wildcard density

Group W is the suite where the measured data clearly contradicts the
original monotonic hypothesis.

Our naive expectation was "more wildcards -> more permanently active states ->
lower throughput". That is only half the story. The other half is predicate
elimination, and it dominates the early part of the curve.

`W0` is actually the slowest or near-slowest point in both modes, because it
evaluates all of `A`, `B`, and `C` every event. As soon as `B` is replaced by a
wildcard, throughput jumps sharply:

- `10%`, jitter: `W0 = 258.6M`, `W1 = 579.7M` (`+124%`)
- `10%`, no-jitter: `W0 = 288.1M`, `W1 = 567.0M` (`+96.8%`)
- `50%`, jitter: `W0 = 130.2M`, `W1 = 264.5M` (`+103%`)
- `50%`, no-jitter: `W0 = 165.9M`, `W1 = 558.0M` (`+236%`)

The generated code explains this cleanly. `W1` through `W32` no longer evaluate
predicate `B`; wildcard positions are emitted as unconditional bit sets. So the
first few wildcard steps are trading one real predicate for one cheap state
bit. That trade is strongly favourable. Perhaps it was even wrong or impure
for our benchmark to include the B state, a more direct A .  C state might have
been purer. Nevertheless, the key insight is that a few wildcards are favoured
over predicate evaluation.

After `W1`, the expected occupancy cost does start to show up:

- In jitter mode at `50%`, throughput falls steadily from `264.5M` (`W1`) to
  `125.9M` (`W32`), a `52.4%` drop.
- In no-jitter mode, throughput stays roughly flat till `W16`
  (`547M-571M`) and then drops at `W32` to `231.4M`.

The latency plots match the throughput story:

- At `50%`, jitter p99 rises from `42` cycles (`W1/W2`) to `64` cycles (`W32`).
- No-jitter stays much tighter, in the `30-38` cycle range.
- At `10%`, jitter and no-jitter are nearly tied from `W1` through `W8`, and
  jitter is slightly faster at `W1` (`579.7M` vs `567.0M`, `+2.2%`).

So the real conclusion is not "wildcards are expensive". It is:
wildcards create a two-term tradeoff. Early on, they remove predicate work and
make the engine faster. Only later, once the wildcard run becomes long enough,
does extra state occupancy outweigh that saved predicate cost.

## Group R - realistic mixed-structure pattern

`R_real` is the summary of what happens once all four architectural
parameters interact in one pattern.

This group now has two direct questions:

1. How does throughput scale with stream size for a realistic mixed pattern?
2. For that pattern, which mode currently wins: jitter or no-jitter?

The per-mode means are:

- `low`, jitter: `140.6M` (`1M`), `134.5M` (`10M`), `127.0M` (`100M`)
- `low`, no-jitter: `96.1M`, `95.5M`, `96.5M`
- `medium`, jitter: `113.0M`, `111.8M`, `110.3M`
- `medium`, no-jitter: `86.3M`, `85.7M`, `85.8M`
- `high`, jitter: `86.7M`, `85.6M`, `87.5M`
- `high`, no-jitter: `70.5M`, `70.0M`, `69.9M`

The first clear result is mode choice if throughput is the goal. For this pattern, jitter wins on
throughput at every selectivity and every stream size measured. However, from the above trends, 
we can conclude that p99 latency would still be better for no-jitter.

That gives the following jitter advantage over no-jitter:

- `low`: `+46.3%` (`1M`), `+40.9%` (`10M`), `+31.7%` (`100M`)
- `medium`: `+30.9%`, `+30.5%`, `+28.6%`
- `high`: `+22.9%`, `+22.3%`, `+25.2%`

That places `R_real` in a very different regime from K. Here, skipping inactive
states is consistently worth more than the fixed-iteration predictability of
the no-jitter loop.

The second clear result is stream-size stability. Throughput is fairly stable
from `1M` to `100M`, and that is true in both modes.

For jitter:

- `low` drops from `140.6M` at `1M` to `127.0M` at `100M` (`-9.7%`)
- `medium` drops from `113.0M` to `110.3M` (`-2.4%`)
- `high` is effectively flat: `86.7M` to `87.5M` (`+0.9%`)

For no-jitter, the curve is even flatter:

- `low`: `96.1M` to `96.5M` (`+0.4%`)
- `medium`: `86.3M` to `85.8M` (`-0.6%`)
- `high`: `70.5M` to `69.9M` (`-0.9%`)

`10M` is already representative. The mode choice and selectivity matter far
more than whether the stream is `1M` or `100M` long.

Selectivity also remains the dominant performance axis inside each mode:

- In jitter mode at `10M`, high selectivity is `36.3%` slower than low
  (`85.6M` vs `134.5M`)
- In no-jitter mode at `10M`, high selectivity is `26.7%` slower than low
  (`70.0M` vs `95.5M`)

That is what the architecture suggests should matter: once the pattern
is realistic and mixed, the dominant variable is how often the stream keeps the
engine "busy" with active states, not how long the input stream is.

## Overall conclusions from the current suite

The run supports three concrete conclusions.

First, the scalar backend is already competitive with handwritten bitset NFAs
on simple structural patterns. The S-100% results are the strongest evidence
for that: generated code is within about 9% of hand across the full sweep.
In fact, for complex patterns, writing such code by hand would be quite hard.

Second, the mode choice is pattern-dependent. No-jitter is still clearly better
for occupancy-heavy synthetic regimes like K, and group W shows the same tail
latency advantage once wildcard runs get long. But Group R now shows that a
realistic mixed pattern can favor jitter on throughput across the board, by
roughly `22%` to `46%`. Thus, it is recommended to carefully benchmark both,
while keeping in mind the much better tail latencies of no-jitter mode.

Third, the main performance gap is most probably not the NFA step. It is predicate
result reuse. Group P, especially `very_compound`, shows that repeated
evaluation of the same composite predicate can erase most of the backend's
otherwise good performance. This is an important backend optimisation target.
