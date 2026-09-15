# Speedrun module

The speedrun client is the clean native port plus a self-contained speedrun subsystem. This document covers the subsystem only.

## Shape

The subsystem is a pure state machine with no engine, platform or SDL dependency. The integration layer decodes one engine frame into a `NativeSpeedrunFrame` and reads back the clocks, events and a fixed-layout surface block. That single seam is what the host tests exercise.

Files:

- `include/platform/native_speedrun.h` defines the frame record, the route, the events and the surface block.
- `platform/native_speedrun.c` implements the clocks, the run window, the load predicate, the split engine, the route parser and the surface writer.
- `tools/test-native-speedrun.c` is the host harness.

## Timing

- Loadless accumulates the engine's per-frame time (`gGT->elapsedTimeMS`) only on non-load frames while a run is active. A frame is a load when `sdata->Loading.stage != LOAD_IDLE`.
- RTA is wall clock, supplied in the frame record.
- The engine freezes its own clock while `PAUSE_ALL` is set, so pause time is recovered from the wall clock. Pause counts, because pausing is used to skip cutscenes.
- A run starts when control is gained in the starting hub (N. Sanity Beach). A run ends on P1's `ACTION_RACE_FINISHED` during the final race in the route.

The full predicate, its edge cases and the source justification are recorded in the project vault at `11-Dev/CTR Speedrun/Wayfinder/research/21-loadless-predicate.md`.

## Route config

A line-based text format, one split per line:

```
<levelID> <kind> <name>
```

`kind` is `normal` or `boss`. Blank lines and `#` comments are ignored. Names may contain spaces. The Any% NMG route is seeded from the community splits.

## Event log

Each emitted event is one text line, for example:

```
seq=1 type=split level=3 mode=524288 segment=0 pos=1 seg_ms=128 total_ms=128 loadless_ms=128 rta_ms=128
```

The line format is stable enough to tail and parse, and it is the record a verifier would inspect.

## Build and test

The host tests do not need SDL or the game. Configure with the game target off:

```
cmake -S . -B build-host -DCMAKE_BUILD_TYPE=Release -DCTR_BUILD_GAME=OFF -DCTR_SPEEDRUN_TESTS=ON
cmake --build build-host --target test_native_speedrun
./build-host/test_native_speedrun
```

The harness prints a checks and failures line and returns non-zero on failure.

## Integration

Build the feature into the game with `-DCTR_SPEEDRUN=ON`. The integration lives in `platform/native_speedrun_runtime.c` and is gated everywhere, so the default build is unchanged.

At start it loads `speedrun-any-nmg.cfg` from the working directory if present, otherwise it runs the clocks with no splits. Each gameplay frame it decodes the engine state, refreshes the surface and appends any event to `speedrun-events.log`.

The surface lives in its own named section:

- Section name `.ctrsr`
- First word is the magic `0x43545253`
- Second word is the ABI version, currently `1`

A consumer locates the section and rejects a mismatched ABI version. The section is compile-verified on the host and on the Linux game build; runtime behaviour still needs a Steam session.

## Not yet done

- The companion timer bridge.
- Build identity stamping and the signed release pipeline.
- The Any% NMG route config content, which needs the boss-race level identities.
