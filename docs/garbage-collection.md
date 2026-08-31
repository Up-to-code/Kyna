# Automatic garbage collection

`ManagedHeap` is the public allocation boundary. It owns objects, arrays, VM closures, and VM capture cells; `RuntimeValue` stores typed non-owning references to those nodes. Collection occurs only at interpreter allocation safepoints or through `collectGarbage()`.

Roots include globals, module environments, active lexical environments, VM registers, closures, capture cells, bound receivers, class state, and module namespaces. Marking uses an iterative worklist, so deep object/array/closure graphs cannot overflow the C++ stack. Because managed object edges are non-owning, unreachable cycles—including recursive closures—are reclaimed.

The growth policy uses live heap size rather than lifetime allocation count. Closure allocation is an explicit VM safepoint: active registers, boxed registers, and frame captures form a typed root set before collection. C++ `HeapStats` reports live, allocated, reclaimed, collection count, peak live size, the next threshold, and live counts for objects, arrays, capture cells, closures, bound methods, and errors. Bytecode execution returns the same statistics for embedding and tests. `kyna run --heap-stats` prints the complete snapshot. Kyna's existing `gcStats()` string and `collectGarbage()` behavior remain source-compatible while exposing the richer class counts.

The runtime suite retains and then releases a 8,001-node cyclic object/array graph. This exercises iterative marking, verifies exact per-class counts while rooted, and proves every node is reclaimed after the temporary root scope ends.
