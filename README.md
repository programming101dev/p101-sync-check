# p101-sync-check

`p101-sync-check` analyzes synchronization events emitted by the p101 pthread
wrappers. It finds lock-order cycles, live wait-for cycles, join cycles, and
incomplete event streams. Its v4 `P101RESOURCE` input remains readable text;
output is plain text or the common p101 JSON finding envelope.

## Quick start

    ./change-compiler.sh -c clang
    ./build.sh -q
    ./build-clang/p101-sync-check resource.log
    ./build-clang/p101-sync-check -j resource.log

Exit status is `0` for a clean complete stream, `1` for concurrency findings,
and `2` for malformed/unsupported input, incomplete evidence, usage, or runtime
trouble.

## Contract

Admitted input:

- v4 `P101RESOURCE` records parsed by `lib_tool_event`;
- `pthread-mutex-held` ownership records;
- mutex, condition, rwlock, and join wait records emitted by `lib_posix`;
- one clean completion receipt for every `(pid, context)` producer.

Output:

- `P101-SYNC-001`: a lock-order cycle;
- `P101-SYNC-002`: a live wait-for cycle;
- `P101-SYNC-003`: a join cycle;
- `P101-SYNC-900`: an incomplete or internally inconsistent stream;
- `P101-SYNC-901`: the fixed analysis capacity was exceeded.

## Blind spots

This is a deterministic event-log analyzer, not a replacement for
ThreadSanitizer. It cannot see direct `pthread_*` calls, third-party
synchronization, data races, atomic-ordering mistakes, or resources that did
not emit p101 events. It reports cycles present in the admitted log; it does
not control the scheduler or prove every potential interleaving was explored.
Resource identities may not be stable between separate executions.

## Evidence

The unit corpus constructs lock-order, wait-for, and join cycles directly:

    ./test.sh

The strict repository receipt is:

    ./check.sh
