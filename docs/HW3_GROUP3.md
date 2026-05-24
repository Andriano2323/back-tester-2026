# HW3 Group 3: Multi-Engine LOB Simulation

This document describes the HW3 implementation: historical L3 book reconstruction, private per-engine overlays, simulated visible books, fill-at-touch matching, and the thread-safety policy used when many engines read/order against one shared historical store.

## 1. Architecture

The HW3 LOB code lives under `src/lob/`:

- `LobTypes.hpp` defines the shared ID, timestamp, price, and quantity aliases. `Price` is `std::int64_t` fixed precision with the same 1e-9 unit used by the existing market-data parser.
- `HistoricalLOB` reconstructs one historical L3 book for one instrument.
- `HistoricalLobStore` owns `std::unordered_map<InstrumentId, HistoricalLOB>` and protects it with `std::shared_mutex`.
- `HistoricalLobProcessor` implements `IMarketDataEventProcessor`, so existing standard/flat/hierarchy runners can build HW3 books without runner changes.
- `EngineView` stores one engine's private overlay/diff.
- `SimulatedLOB` merges historical state and one `EngineView` on read.
- `FillSimulator` accepts synthetic limit orders and records private consumed historical liquidity in the requesting engine's `EngineView`.

CLI demo mode is available with:

```bash
./build/ingest --mode standard --input tests/data/lob_basic.ndjson --lob-summary --print-events 0
./build/ingest --mode flat --input tests/data/lob_multi --lob-summary --print-events 0
./build/ingest --mode hierarchy --input tests/data/lob_multi --lob-summary --print-events 0
```

`--lob-summary` is intentionally separate from the existing HW2 `--lob` snapshot processor. It uses the HW3 `HistoricalLobProcessor` and prints a compact final book summary plus a stable digest.

## 2. Why Overlay/Diff, Not N Full Copies

The design question for HW3 is whether every engine should own a full copy of the historical book. This implementation does not do that.

There is one shared historical book store. Each engine owns only:

- synthetic orders submitted by that engine,
- aggregated synthetic price levels derived from those orders,
- historical liquidity consumed by that engine's own simulated fills.

This keeps memory growth proportional to engine activity, not to `engine_count * historical_book_size`. It also preserves the HW3 requirement that each engine sees itself as the only additional participant: engine A's synthetic orders and private consumed liquidity do not affect engine B.

## 3. HistoricalLOB Event Semantics

`HistoricalLOB::apply(const MarketDataEvent&)` implements the base historical reconstruction semantics:

- `Add`: insert or replace the resting order and add its size to the aggregated bid/ask price level.
- `Modify`: if the order exists, remove its old level size, then write the new side/price/size and aggregate it.
- `Cancel`: reduce the resting order and level by `event.size`; `size == 0` cancels the full remaining order.
- `Clear`: remove resting orders for the event's `instrument_id`; `instrument_id == 0` clears the whole standalone book.
- `Trade` and `Fill`: no mutation in the historical book model.

`BookLevel` exposes `{ price, size }`. `snapshot(depth)` returns top bid levels in descending price order and ask levels in ascending price order.

## 4. EngineView Private State

`EngineView` is the per-engine diff:

```cpp
std::unordered_map<SyntheticOrderId, SyntheticOrder> own_orders_;
std::unordered_map<InstrumentId, SyntheticBook> synthetic_books_;
std::unordered_map<InstrumentId, ConsumedLiquidityBook> consumed_historical_liquidity_;
```

`addSyntheticOrder(...)` allocates a private synthetic order ID and updates only that engine's synthetic book. `cancelSyntheticOrder(...)` removes only that engine's synthetic order. Reads such as `syntheticBook(instrument_id)` return a snapshot of that engine's synthetic levels.

## 5. SimulatedLOB Merge Rules

`SimulatedLOB` is read-only. It builds the visible book for one engine using:

```text
visible book = historical book
             - historical liquidity consumed by this engine
             + synthetic orders of this engine
```

Other engines' overlays are not included. This is tested by creating two engine views where only engine 1 adds a synthetic order: engine 1 sees it, engine 2 does not, and the historical book remains unchanged.

## 6. Fill-At-Touch Model

`FillSimulator::submitLimitOrder(...)` implements the current matching model:

- Buy limit crosses if `limit_price >= visible historical best ask`; it fills at the best ask.
- Sell limit crosses if `limit_price <= visible historical best bid`; it fills at the best bid.
- Filled size consumes historical liquidity only in the requesting engine's `EngineView`.
- Any unfilled remainder rests as a synthetic order in the same engine's `EngineView`.

The shared `HistoricalLOB` / `HistoricalLobStore` is not mutated by simulated fills. This makes every engine see itself as the only participant added to the historical market.

## 7. Thread-Safety Policy

The HW2 hard-mode ingestion model already has one dispatcher thread calling `processMarketDataEvent`; producer threads only read/parse files. HW3 adds concurrency between historical updates and engine reads/orders.

Locking policy:

- `HistoricalLobStore` uses `std::shared_mutex`.
- Historical updates from the dispatcher take `std::unique_lock`.
- Simulated reads take `std::shared_lock`.
- Each `EngineView` uses its own `std::mutex`.
- Engine order submission, synthetic order reads, and consumed-liquidity reads/writes are protected by that per-engine mutex.

This allows many engine views to operate independently while sharing one historical store.

## 8. Test List And How To Run

Core HW3 tests are registered in `tests/test_main.cpp`:

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

Run the normal suite:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Run the demo commands:

```bash
./build/ingest --mode standard --input tests/data/lob_basic.ndjson --lob-summary --print-events 0
./build/ingest --mode flat --input tests/data/lob_multi --lob-summary --print-events 0
./build/ingest --mode hierarchy --input tests/data/lob_multi --lob-summary --print-events 0
```

Optional TSAN run:

```bash
cmake -S . -B build-tsan \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=thread -g -O1" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread"
cmake --build build-tsan -j
setarch "$(uname -m)" -R ctest --test-dir build-tsan --output-on-failure
```

On this Linux/GCC setup, plain TSAN `ctest` needed `setarch ... -R` because the TSAN runtime otherwise failed before tests with `unexpected memory mapping`. With ASLR disabled for the run, TSAN reported no data races.

## 9. Known Limitations

- The fill simulator is a simple fill-at-touch model, not a full exchange matching engine.
- Synthetic orders do not match against other engines' synthetic orders.
- Queue position, latency, priority, fees, rejects, and partial order lifecycle diagnostics are outside this stage.
- The historical book model treats `Trade` and `Fill` as non-mutating events.
- The demo `lob_digest` is intentionally verbose and suited for deterministic correctness checks, not compact production logging.
- `HistoricalLOB` itself is not internally synchronized; use `HistoricalLobStore` for shared concurrent access.
- HW3 integration currently exposes final summaries and tests correctness. It is not tuned for maximum throughput.

## Real Dataset Acceptance

Acceptance was run on local XEUR data:

```bash
./build/ingest --mode standard --input data/XEUR-20260409-HJTR7RCAKT/xeur-eobi-20260309.mbo.json --lob-summary --print-events 0
./build/ingest --mode flat --input data/XEUR-20260409-HJTR7RCAKT --lob-summary --print-events 0
./build/ingest --mode hierarchy --input data/XEUR-20260409-HJTR7RCAKT --lob-summary --print-events 0
./scripts/benchmark.sh data/XEUR-20260409-HJTR7RCAKT
```

Results:

- standard: 1,305,607 messages, `chronological_violations=0`
- flat: 15,175,617 messages, `chronological_violations=0`
- hierarchy: 15,175,617 messages, `chronological_violations=0`
- flat/hierarchy HW3 digest matched
- benchmark LOB digest matched between flat and hierarchy
- LOB benchmark throughput degradation versus logging-only was about 20.7% for flat and 19.2% for hierarchy

