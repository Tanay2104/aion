# Aion Future Roadmap
Created on 14 March 2026.

This document discusses the roadmap for future research and development. It covers a variety 
of topics, ranging from current bugs in the implementation, design oversights, architectural purity vs features
tradeoff, interesting research directions and more.

## Table of Contents
1. [Current Implementation Bugs](#1-current-implementation-bugs)
2. [Codegen Oversights And Need Of An IR](#2-codegen-oversights-and-need-of-an-ir)
3. [Adding Features](#3-adding-features)
4. [Aion as a First Filter](#4-aion-as-a-first-filter)
## 1. Current Implementation Bugs
There are several bugs in the current implementation, which generate invalid code. Perhaps the most notable
is that a reference to another regex isn't actually supported by the code, despite being in the formal grammar.
This is because the current code relies on simple identifier checks for references as identifiers, and both
a predicate identifier and regex identifier are treated the same way. This is an important issue and needs to be 
addressed. 

Another broader issue is namespace collision. Currently, the compiler only checks for repeated predicate names.
However, it does no collision checks against event structs vs predicate names vs regex names. That is, the 
three names can collide with each other. The compiler ignores a predicate declaration with the name colliding with
a field, and a regex declaration colliding with a field or predicate results in an uncaught exception of type 
`std::bad_variant_access: bad_variant_access`. 

The solution to the first problem is not yet identified, several choices are possible depending 
on how much we are willing to change the current code.
Similarly, the second problem can be partially solved by putting namespaces like `event foo {int x};` and then using 
`pred bar = foo.x == 1;`. This would be helpful later too, when we (may or may not) introduce
multiple event structs. Nevertheless, predicate and regex names should not collide, and their should
be passes for ensuring so, as it would be quite cumbersome to write something like `pred.bar` or 
`regex.baz` everytime we want to refer to a predicate or regex.

More than these, I am sure there would be bugs or perhaps untold mutual contracts between 
components. These need to be found out and fixed.

## 2. Codegen Oversights And Need Of An IR
The first codegen pattern used templates and concepts heavily. That pattern
can be found at commit `dd9ef056e468948008e71aad9c8360e5ed762b5a`. I initially thought
that the core matching algorithm will remain quite similar across target architectures,
and hence something like our current std::format based codegen was violating the DRY principle.
Hence, I created an abstract BitSet concept, defined templated algorithms over types
satisifying the BitSet concept, and embedded them into the output header via #embed.
However, further analysis and study revealed that in practice, algorithm's implementation
can differ quite a lot across scalar vs vectors. For example, AVX2 has masking instructions,
however it can be tricky to bitshift AVX2 because of the lane crossing problem(AVX2 is made up 
of 2 128-bit lanes which do not allow bit to shift over each other).

Following this, I changed the codegen pattern to simple text based emitter where each backend 
has an implementation of an abstract `CodeGenStrategy` class, and just emits raw strings using
`CEmitter`, without any IR whatsoever. 

While this works, it is suboptimal, even if we exclude maintainability and feature expansion. 
The biggest weakness is that this method does not allow us to do semantic analysis and optimisations
over the generated code. Yesterday's benchmarks revealed that Clang is not very good at optimizing those 
specific functions(partially it's my fault, due to all predicate evaluation functions being forcefully 
inlined). Thus, we need to do optimisation passes over generated cpp code.


This motivates the need of an IR for the generated code. The design of the IR is a thinking point and 
has not been decided in any way yet.

Another small nitpick in the code generated is that the emitter emits the full follow table, irrespective of number
of states. While this doesn't affect the runtime latency, removing this would save some memory, which is a big
deal in modern day memory crisis . Pun intended, ... or is it? ;).

## 3. Adding Features
In almost any software, there is a tradeoff between features, maintainability and performance. Right now,
Aion completely sacrifices features for maximum performance(and it indeed does quite well!). However, some
analysis and study of real world cases has mostly convinced me that Aion, in its current perform, is simply
too limited.

### Common Expression Elimination and Predicate Minimisation
As explained in the benchmark results, predicate evaluation cost is a big deal. There are two reasons
why predicate evaluation can be slow. The first is simpler to deal with.

We are inlining all predicate function calls in the generated code. This works well for small predicates, which are
quite rarely, but is slow for large predicates. In such a case, we can store the predicate result in a stack allocated
variable and use it as needed. This should not be hard to implement in the backend optimisation pass(
thus further motivating an IR). Note that this is still zero heap allocation.

The second idea is more theoretically interesting. It borrows from the "Binary Decision Diagrams" and
state-of-the-art SAT solvers. Instead of evaluating all predicates multiple times, we can evaluate only 
the atomic predicates and use them to create a tree-like structure, through which we can evaluate compound
predicates. This essentially helps us reduce load across predicates, unlike the previous optimisation, which only
reduced load for a particular predicate.

The third idea is to use AVX2 along with Structure-of-Arrays data stream to enable multiple
predicate evaluations at the same time. This would certainly increase raw throughput, but requires 
the user to send data in the correct format. 

### Small History Buffers
One core issue is that most streams require some form a "history" to check and evaluate predicates
against. Some examples include Spoofing in Algorithmic Trading, and the NASA Turbofan dataset, where we
might need predicates like "sensor value is less than last three sensor values". Current Aion requires
the user to process the history and pass them along in the struct. However, that is suboptimal and leads
to copying data, etc. I think we can implement a small history (via a circular buffer), of size fixed at compile
time. This would, in conjunction with the other ideas, vastly increase the practicality of this tool,
while maintaining the core value proposition: It is still zero allocation and branchless kernels. Moreover,
while it has not been yet benchmarked or developed, I feel the impact should stay smaller than expected.
The reason being :
- Smaller history buffer stays in L1 cache for hot path.
- Amazing prefetching by modern CPUs
- Superscalar OOO execution if no data dependencies

This will still need benchmarking. Nevertheless, it follows "no cost abstraction", meaning users 
who don't use the history buffer feature won't pay for it.


### The State Count Limit
Currently, Aion only supports upto 63 states in the regex. I'll be blunt and honest: I have no idea
how is that for a simple pattern matching engine. However, I do have a desire to remove this limitation.

The first solution is quite simple and stays in the scalar world: simply use `__uint128_t` instead of `std::uint64_t` 
everywhere. Beyond this, we can scale using utilising vector registers and instructions such as AVX2 and AVX512.
Supporting ARM would also be nice, as an increasing number of microprocessors are ARM based, and Aion has an application 
for embedded systems.

### Multiple Event Streams.
Currently, Aion only processes one type of event stream and compares predicates only over them. However, 
in practice, we need multiple streams. Of course, the user can pack them all into one struct and send it
to Aion engine but this costs valuable CPU cycles. Hence, a support for multiple event streams is essential.

The implementation is where it gets tricky. What exactly do we mean by multiple event streams? As in, multiple
types of sequentially ordered streams? Multiple streams in parallel? Some streams in parallel? These lead to various
different designs(such as overloading of single streams vs all streams passed to `process_event` vs pointers passed, 
some can be null, etc). This requires further research to commit to a single direction. Any help on this topic would be 
greatly appreciated!

## 4. Aion as a First Filter

This section discusses an interesting research direction: What if instead of letting Aion be the sole
pattern matcher, we make it a first-filter matcher which processes events in the hot path and only sends
suspicious matches to the heavy core engine, which can be a full CEP engine or ML model.

For example, consider the NASA turbofan dataset. This is a classic ML problem. However, in the real world, we 
may not have sufficient resources to run the full ML model in real time(okay, this example isn't the exact fit, but
I hope you get the general idea). In such a case, the ML engineers can use an interpretable ML model like decision trees,
and "distill" it's core rules down to Aion predicates and patterns. Note that the distillation should result in
very low false negatives, but false positive are acceptable. The core ML model then runs it's inference on the stream
filtered out by Aion, and it has full access to other data it may need to make reliable predictions. I think this is an
underexplored area, and I do plan implement this in examples/ and perhaps make a tools/ directory which would 
contain python scripts for extracting patterns and distilling decision trees into Aion rules.