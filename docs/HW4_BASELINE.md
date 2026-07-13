# HW4 Task A0R Baseline Report

## Baseline identity

| Item | Value |
| --- | --- |
| Repository root | `/home/andre/projects/back-tester-2026` |
| Origin | `https://github.com/Andriano2323/back-tester-2026.git` |
| Feature branch | `feature/hw4-group-a-contract` |
| Direct base branch | `hw3-solution` |
| Base commit | `74a88a1cc839d111bf83bdb90c6b1a080fa3a6f2` (`Change mmap to stream`) |
| Relationship to `main` | `hw3-solution` is 5 commits ahead and 0 behind; `main` is an ancestor |
| Baseline run date | 2026-07-13 (Europe/Moscow) |
| Build environment | Ubuntu 24.04 under WSL2, 12 logical processors |
| CMake | 3.28.3 |
| C/C++ compiler | GCC/G++ 13.3.0 |
| Pre-commit | system `pre-commit 3.6.2` |

The original A0 documentation was moved with a path-scoped temporary stash. The
feature branch was deleted and recreated at the exact `hw3-solution` commit, and
the stash was restored. The README conflict was resolved by retaining the complete
HW3 README and adding only the five-line Homework 4 section. The temporary stash
was dropped after all three restored paths were verified.

The pre-HW4 WIP remains preserved separately on
`backup/import-hw2-code-before-hw4` at
`24c9894bcc7d27fb24a7f25beab5c792c35956e7`. It was not copied into this branch.

## Commands executed

Repository commands were run inside the Ubuntu repository context; the outer
`wsl.exe` transport is omitted.

```bash
pwd
git rev-parse --show-toplevel
git remote get-url origin
git branch --show-current
git status --short

git stash push -u \
  -m "Temporary A0 documentation move to hw3-solution" \
  -- \
  docs/HW4_GROUP_A_CONTRACT.md \
  docs/HW4_BASELINE.md \
  README.md

git switch hw3-solution
git fetch origin
git pull --ff-only origin hw3-solution
git branch -D feature/hw4-group-a-contract
git switch -c feature/hw4-group-a-contract
git stash pop
```

After the expected README conflict, the HW3 version was retained and the small
Homework 4 section was reapplied. After verification, the retained conflict stash
was removed with `git stash drop stash@{0}`.

Toolchain, clean build, and tests:

```bash
cmake --version
c++ --version
rm -rf build
cmake -B build -S . \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTS=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure -j$(nproc)
ctest --test-dir build -N -V
./build/ingest_tests
```

The requested `rm -rf build` was performed through a safety-checked host-side
equivalent targeting exactly the repository's `build/` directory.

Formatting baseline:

```bash
pre-commit run --all-files
```

The generated formatting changes outside A0R scope were inspected and restored
with `git restore -- <exact-auto-modified-paths>`. No blanket reset, checkout, or
clean was used.

## Configure, build, and CTest results

| Check | Result |
| --- | --- |
| CMake configure | Passed in Release mode. |
| Configure warning | `BUILD_TESTS` was manually specified but is not used by the HW3 CMake project. Tests are registered unconditionally, so this did not suppress the suite. |
| Build | Passed; `ingest_core`, `ingest`, and `ingest_tests` all built successfully. |
| Compiler warnings | None emitted. The HW3 targets compile with warning flags including `-Wall`, `-Wextra`, and `-Wpedantic`. |
| CTest | Passed: 5 of 5 tests, 0 failures. |
| Direct aggregate test binary | `./build/ingest_tests` printed `All tests passed`. |

The five CTest registrations were:

1. `ingest_tests`
2. `ingest_smoke_standard`
3. `ingest_smoke_flat`
4. `ingest_smoke_hierarchy`
5. `ingest_smoke_help`

CTest completed in 0.04 seconds on this baseline run. Wall-clock duration is a
diagnostic only and is not a simulation result.

## Existing HW3 implementation

The actual checked-out baseline contains:

- `src/domain/MarketDataEvent.hpp` and `.cpp`: parsed historical event fields,
  timestamps, instrument/order data, `Side::Bid/Ask/None`, action, and deterministic
  `(timestamp, source_file_id, source_sequence)` comparison.
- `src/parsing/JsonParser.*` and `src/io/FeatherEventReader.*`: JSON and optional
  Feather input adapters into the common event shape.
- `src/runners/StandardRunner.*`: chronological single-file ingestion on the
  caller thread.
- `src/runners/FlatMergeRunner.*`, `HierarchicalMergeRunner.*`, and
  `HardRunnerSupport.*`: producer threads, deterministic merge stages, and one
  dispatcher thread calling `IMarketDataEventProcessor`.
- `src/lob/HistoricalLOB.*`: one-instrument historical L3 reconstruction for Add,
  Modify, Cancel, and Clear, with aggregated levels and top-N snapshots.
- `src/lob/HistoricalLobStore.*`: one shared, `std::shared_mutex`-protected store
  of books per instrument plus a stable sorted state digest.
- `src/lob/HistoricalLobProcessor.*`: replay-to-store processor adapter.
- `src/lob/EngineView.*`: private per-engine synthetic orders and privately
  consumed historical liquidity.
- `src/lob/SimulatedLOB.*`: visible state computed as historical state minus that
  engine's consumed liquidity plus that engine's synthetic overlay.
- `src/lob/FillSimulator.*`: synchronous fill-at-touch calculation against visible
  historical liquidity, with an unfilled remainder resting in `EngineView`.
- `src/processing/ShardedLobMarketDataEventProcessor.*`: instrument-sharded worker
  processing with atomic drain counters. It is useful HW3 concurrency code but is
  not the Group A ready barrier.

HW3 synthetic fills do not mutate the shared historical book, and one engine does
not observe another engine's overlay or privately consumed liquidity.

## Existing relevant tests

The aggregate `ingest_tests` executable includes focused HW3 tests for:

- `testHistoricalLobAddModifyCancelClear`
- `testHistoricalLobTopNSnapshot`
- `testHistoricalLobProcessorBuildsBooksPerInstrument`
- `testEngineViewsArePrivate`
- `testSimulatedLobMergesHistoricalAndOwnOverlayOnly`
- `testFillAtTouchConsumesOnlyPrivateLiquidity`
- `testNonCrossingLimitOrderRestsInEngineView`
- `testConcurrentEngineViewsNoCrashesNoLostIsolation`
- `testStandardRunnerCanBuildLobSummary`
- `testFlatAndHierarchyBuildSameLobDigest`

The broader runner suite additionally covers:

- standard, flat, and hierarchy chronological ingestion;
- stable ordering of equal-timestamp historical events by source metadata;
- matching flat/hierarchy historical LOB digests;
- matching sequential and sharded digests;
- synchronous versus asynchronous snapshot output;
- parser, book manager, limit-order-book, queue, and result formatting behavior.

The four smoke registrations exercise standard, flat, hierarchy, and help CLI
paths separately from the aggregate unit/integration executable.

## Current HW4 gaps and risks

The HW3 runtime is real and tested, but Group A still lacks:

- a canonical `src/domain/Types.hpp` and checked reconciliation with
  `src/common/BasicTypes.hpp` and `src/lob/LobTypes.hpp`;
- a global `DispatchSeq` market-data envelope;
- post-event absolute `BookUpdate` batches and trading-engine strategy callbacks;
- a per-engine `VirtualClock` and fixed per-engine market/order latency config;
- one `processed_seq` ready acknowledgment per trading engine;
- a trading-engine `MarketDataConsumer`;
- an order-latency scheduler and approved equal-time activation ordering;
- a complete `OrderManager` lifecycle;
- a `PositionKeeper` with duplicate-fill and overfill protection;
- `OrderExecutionBridge`, `IntegratedBacktestEngine`, and the integrated causal
  event loop;
- end-to-end and multi-engine Group A tests;
- a ready-signal benchmark.

Additional baseline risks:

- `-DBUILD_TESTS=ON` is accepted by the command line but unused by CMake.
- `HistoricalLOB::apply` returns `void`, so accepted mutations cannot yet be
  distinguished from invalid/unresolved no-ops for callback generation.
- `MarketDataEvent::source_sequence` is per file and not a global dispatch or
  per-instrument market-data sequence.
- `FillSimulator` executes immediately and copies the request timestamp; it has no
  latency or activation scheduler.
- The sharded processor can return after enqueue, before a worker applies the
  event. Group A must not treat that return as a ready acknowledgment.
- No Python package or bindings exist. A later harness must call the C++ engine and
  must not duplicate matching behavior in Python.

## Pre-existing formatting drift

`pre-commit run --all-files` fails because formatters modify 86 tracked files.
Task A0R restored all of those automatic changes and does not include formatting
cleanup. A0.1 should address this in a separate format-only commit before A1.

Ruff Format changes exactly these two files:

```text
scripts/benchmark_feather_read.py
scripts/convert_to_feather.py
```

clang-format changes exactly these 84 files:

```text
src/app/AppConfig.hpp
src/app/ArgsParser.cpp
src/app/ArgsParser.hpp
src/book/BookManager.cpp
src/book/BookManager.hpp
src/book/BookSnapshot.hpp
src/book/LimitOrderBook.cpp
src/book/LimitOrderBook.hpp
src/common/BasicTypes.hpp
src/concurrency/NonBlockingQueue.hpp
src/domain/MarketDataEvent.cpp
src/domain/MarketDataEvent.hpp
src/io/FeatherEventReader.cpp
src/io/FeatherEventReader.hpp
src/lob/EngineView.cpp
src/lob/EngineView.hpp
src/lob/FillSimulator.cpp
src/lob/FillSimulator.hpp
src/lob/HistoricalLOB.cpp
src/lob/HistoricalLOB.hpp
src/lob/HistoricalLobProcessor.cpp
src/lob/HistoricalLobProcessor.hpp
src/lob/HistoricalLobStore.cpp
src/lob/HistoricalLobStore.hpp
src/lob/LobTypes.hpp
src/lob/SimulatedLOB.cpp
src/lob/SimulatedLOB.hpp
src/main.cpp
src/main/main.cpp
src/parsing/JsonParser.cpp
src/parsing/JsonParser.hpp
src/processing/AsyncSnapshotWriter.cpp
src/processing/AsyncSnapshotWriter.hpp
src/processing/IMarketDataEventProcessor.hpp
src/processing/LobMarketDataEventProcessor.cpp
src/processing/LobMarketDataEventProcessor.hpp
src/processing/LoggingMarketDataEventProcessor.cpp
src/processing/LoggingMarketDataEventProcessor.hpp
src/processing/ShardedLobMarketDataEventProcessor.cpp
src/processing/ShardedLobMarketDataEventProcessor.hpp
src/runners/BenchmarkRunner.cpp
src/runners/BenchmarkRunner.hpp
src/runners/FeatherHardRunnerSupport.cpp
src/runners/FeatherHardRunnerSupport.hpp
src/runners/FlatMergeRunner.cpp
src/runners/FlatMergeRunner.hpp
src/runners/HardRunnerSupport.cpp
src/runners/HardRunnerSupport.hpp
src/runners/HierarchicalMergeRunner.cpp
src/runners/HierarchicalMergeRunner.hpp
src/runners/InputFileDiscovery.cpp
src/runners/InputFileDiscovery.hpp
src/runners/InputFormat.hpp
src/runners/QueueItem.hpp
src/runners/ResultPrinter.cpp
src/runners/ResultPrinter.hpp
src/runners/RunResult.hpp
src/runners/StandardRunner.cpp
src/runners/StandardRunner.hpp
test/TempFile.hpp
tests/TestSupport.hpp
tests/app/ArgsParserTest.cpp
tests/book/BookManagerTest.cpp
tests/book/LimitOrderBookTest.cpp
tests/concurrency/NonBlockingQueueTest.cpp
tests/domain/MarketDataEventTest.cpp
tests/io/FeatherEventReaderTest.cpp
tests/lob/EngineViewTest.cpp
tests/lob/FillSimulatorTest.cpp
tests/lob/HistoricalLOBTest.cpp
tests/lob/HistoricalLobProcessorTest.cpp
tests/lob/LobSummaryIntegrationTest.cpp
tests/lob/SimulatedLOBTest.cpp
tests/lob/ThreadSafetyTest.cpp
tests/parsing/JsonParserTest.cpp
tests/processing/AsyncSnapshotWriterTest.cpp
tests/processing/LobMarketDataEventProcessorTest.cpp
tests/runners/FlatMergeRunnerTest.cpp
tests/runners/HardLobIntegrationTest.cpp
tests/runners/HierarchicalMergeRunnerTest.cpp
tests/runners/ResultPrinterTest.cpp
tests/runners/StandardRunnerLobIntegrationTest.cpp
tests/runners/StandardRunnerTest.cpp
tests/test_main.cpp
```

## Final A0R validation

- `pre-commit run --all-files` failed because it reproduced the documented
  baseline drift: two Ruff Format files and 84 clang-format files. The exact
  86-file set matched the list above and was restored.
- `pre-commit run --files docs/HW4_GROUP_A_CONTRACT.md docs/HW4_BASELINE.md README.md`
  passed; all configured hooks correctly reported no applicable files.
- `git diff --check` passed for the tracked README diff.
- Equivalent `git diff --no-index --check` validation passed for both new
  documentation files.
- Final scope contains only `README.md`, `docs/HW4_GROUP_A_CONTRACT.md`, and
  `docs/HW4_BASELINE.md`.
- No post-documentation rebuild was required because the clean HW3 build and all
  five CTest registrations passed and no executable/build file changed.

## Runtime-change statement

Task A0R changes only `docs/HW4_GROUP_A_CONTRACT.md`, `docs/HW4_BASELINE.md`, and
`README.md`. It does not change runtime behavior, tests, benchmarks, bindings,
Python modules, CMake, dependency configuration, or CI. No A1 implementation is
included.
