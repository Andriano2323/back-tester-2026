# HW4 Group A Interface Contract

Status: proposed contract frozen by Task A0. No HW4 runtime behavior is implemented by this document.

The A0R branch is based directly on `hw3-solution` at
`74a88a1cc839d111bf83bdb90c6b1a080fa3a6f2`.
That branch is five commits ahead of `main` and zero commits behind it: `main` is
its direct ancestor. The historical replay, LOB, private engine overlay, fill
simulation, and HW3 tests are part of the checked-out baseline and compile on this
branch. No manual source copy or merge is required before A1.

The Side, BookUpdate, latency, cross-engine ordering, and equal-timestamp rules in
this document are approved A0R decisions, not unresolved alternatives.

## 1. Purpose and scope

Group A connects historical replay, `HistoricalLOB`, `HistoricalLobStore`,
`EngineView`, `SimulatedLOB`, fill simulation, the order gateway, and trading-engine
state into one deterministic causal event loop.

This contract fixes the intended interfaces, ownership boundaries, time semantics,
sequencing, and determinism rules before production code changes. Task A0 does not
implement `VirtualClock`, `ReadySignal`, `MarketDataConsumer`, `OrderManager`,
`PositionKeeper`, order-latency scheduling, or any other later Group A stage.

## 2. Existing components to reuse

The following components are present in the current `hw3-solution`-based feature
branch and are the concrete HW3 reuse surface for Group A.

| Component | Location | Responsibility and reuse boundary |
| --- | --- | --- |
| Basic domain aliases | `src/common/BasicTypes.hpp` | Current HW1 identifiers, `Price`, `Quantity`, and order-side definitions. It is a compatibility source, not the HW4 canonical source. |
| Historical event | `src/domain/MarketDataEvent.hpp` and `.cpp` | Carries source timestamps, historical order data, instrument, and stable merge metadata. `eventComesBefore` orders by timestamp, source file, then source sequence. |
| Replay runners | `src/runners/StandardRunner.cpp`, `FlatMergeRunner.cpp`, `HierarchicalMergeRunner.cpp`, and `HardRunnerSupport.cpp` | Read and deterministically merge historical input. Flat and hierarchy modes use producer/merge threads and one dispatcher thread. |
| Processor interface | `src/processing/IMarketDataEventProcessor.hpp` | Existing synchronous-looking `processMarketDataEvent` entry point. It is too small for the final callback contract but is a useful adapter boundary. |
| Historical L3 book | `src/lob/HistoricalLOB.hpp` and `.cpp` | Reconstructs one instrument from Add, Modify, Cancel, and Clear; exposes aggregated levels and top-N snapshots. |
| Shared historical store | `src/lob/HistoricalLobStore.hpp` and `.cpp` | Owns one `HistoricalLOB` per instrument, protects it with `std::shared_mutex`, and produces a stable sorted digest. |
| Replay-to-LOB adapter | `src/lob/HistoricalLobProcessor.hpp` and `.cpp` | Applies replay events to a `HistoricalLobStore` through `IMarketDataEventProcessor`. |
| Private engine overlay | `src/lob/EngineView.hpp` and `.cpp` | Stores one engine's synthetic orders and privately consumed historical liquidity behind a per-view mutex. |
| Per-engine visible book | `src/lob/SimulatedLOB.hpp` and `.cpp` | Computes `historical - privately consumed + own synthetic` without exposing another engine's overlay. |
| Fill calculation | `src/lob/FillSimulator.hpp` and `.cpp` | Implements immediate fill-at-touch against visible historical liquidity and rests the remainder in `EngineView`. It remains the calculation component, not lifecycle or position state. |
| Isolation and LOB tests | `tests/lob/` and `tests/runners/HardLobIntegrationTest.cpp` | Cover reconstruction, top-N state, private overlays, immediate fills, thread safety, stable historical digests, and equal-timestamp historical merge order. |

`src/messaging/`, `src/backtest/`, `src/gateway/`, Python bindings, and the
`python/backtester/` package do not yet exist in the HW3 baseline.

## 3. Canonical domain types

The proposed canonical source for Group A is `src/domain/Types.hpp`. That file does
not yet exist; A1 must create it. All new Group A interfaces must include it rather
than independently declaring aliases.

| Type | Proposed representation and semantics |
| --- | --- |
| `InstrumentId` | `std::uint64_t`; zero is invalid/unset. |
| `OrderId` | `std::uint64_t`; unique within a trading engine. The pair `(TradingEngineId, OrderId)` is globally unambiguous. |
| `TradingEngineId` | `std::uint64_t`; zero is invalid/unset. |
| `TimestampNs` | `std::int64_t`; non-negative virtual nanoseconds. Input conversion and latency addition must detect overflow. |
| `Price` | `std::int64_t`; fixed-point price in the existing market-data 1e-9 unit. Floating-point prices are not allowed in the causal path. |
| `Quantity` | `std::uint64_t`; absolute non-negative integral quantity. Signed position is a separate accounting representation. |
| `SeqNo` | `std::uint64_t`; market/source sequence metadata, scoped per instrument or provider stream as declared by the adapter. It is not a dispatch barrier sequence. |
| `Side` | The canonical C++ enum is the existing `Side::None`, `Side::Bid`, and `Side::Ask`. In order semantics, Bid means Buy and Ask means Sell. Group A must not add a second C++ `Buy/Sell` enum. |
| `OrderStatus` | Strong enum containing `PendingNew`, `Active`, `PartiallyFilled`, `Filled`, `CancelPending`, `Cancelled`, and `Rejected`. |

The current `src/common/BasicTypes.hpp` conflicts with this proposal:

- `Price` and `Quantity` are `double`, while the HW3 market event and LOB types
  use `std::int64_t` price and `std::uint64_t` quantity.
- `cmf::Side` is `None/Buy/Sell` with signed numeric values, while
  `src/domain/MarketDataEvent.hpp` defines the approved runtime `md::Side` as
  `None/Bid/Ask` with character wire values.
- `SecurityId` is 16-bit, while HW3 `InstrumentId` is 64-bit.
- `cmf::OrderId`, HW3 `HistoricalOrderId`, and HW3 `SyntheticOrderId` are separate
  aliases with no explicit conversion or scope policy.
- `NanoTime` is signed while the HW3 historical event timestamps are unsigned.

`src/lob/LobTypes.hpp` is closer to the proposed numeric
representations, but it is another duplicate type source and must not become a
second canonical header. A1 must add checked adapters and compatibility aliases;
A0R intentionally does not refactor any type. Python aliases such as Buy/Sell may
be added later by Group B without changing the C++ enum.

The HW3 `source_sequence` is a per-input-file stable line sequence used with
`source_file_id` as a merge tie-breaker. It is not the proposed per-instrument
`SeqNo`, and neither is the global `DispatchSeq` defined below.

## 4. Market-data callback contract

The approved callback behavior is:

1. An accepted Add, Modify, Cancel, or Clear mutates the shared
   `HistoricalLobStore` first.
2. The engine's local market view is refreshed from that post-event state.
3. Add and Cancel produce the applicable post-event `BookUpdate`. A Modify that
   changes price or side produces two ordered updates in the same dispatch: the
   old level after removal, followed by the new level after insertion. A Modify
   that remains on one level produces one post-event update.
4. Clear produces no fictitious price-level update. It is represented by the
   post-clear `BookSnapshot` for the affected instrument.
5. `on_trade` is invoked for Trade and historical Fill events. Synthetic order
   fills travel through the order-event path instead and must not be reported as
   historical trades.
6. Every `BookSnapshot` passed to a strategy is a post-event top-N snapshot.
7. All messages and snapshots caused by one historical event carry the same
   `DispatchSeq` and complete before that dispatch is acknowledged.

For Cancel and for the old part of a moving Modify, side and price are resolved
from the stored order before mutation. `BookUpdate.size` is always the absolute
aggregate quantity remaining at the affected price level after the event; it is
never the raw input delta. A removed level is represented by `size == 0`.

Invalid or unresolved no-op events are diagnostics, not accepted updates. All
callbacks and all immediate work caused by a dispatch complete before the
dispatcher can release the next historical dispatch.

The HW3 baseline has no `BookUpdate`, strategy `BookSnapshot` callback,
`MarketDataPublisher`, or `MarketDataSubscriber`. `HistoricalLOB::apply` accepts a
raw `MarketDataEvent`, returns no accepted/no-op result, and exposes aggregate
snapshots only on a separate read. `MarketDataEvent::size` remains raw event size.
Building the ordered post-event callback payload is therefore a later-task gap.

## 5. Virtual-time contract

The two causal timestamps are:

```text
engine_receive_time_ns(engine) =
    historical_event_timestamp_ns + market_data_latency_ns(engine)

order_activation_time_ns(engine) =
    strategy_submit_time_ns(engine) + order_latency_ns(engine)
```

Each trading engine has its own fixed `market_data_latency_ns` and
`order_latency_ns`. Both values are non-negative integer nanoseconds and remain
constant for that engine for the entire run. A single-engine run has one such
configuration; a multi-engine run may use different fixed values per engine.
Configuration parsing must reject negative values, and addition must reject
overflow. For one engine's scheduling and equal-timestamp rule,
`market_event_time` means that engine's `engine_receive_time_ns`, not the raw
historical timestamp.

Each engine's virtual time never moves backwards. Out-of-order input after
deterministic merge, or a computed time earlier than that engine clock's current
value, is a deterministic contract violation; the implementation must not silently
use wall time or clamp an event without reporting it. Wall-clock time may be
measured for benchmarks but must not influence ordering, fills, state, IDs, or
timestamps.

Strategy callbacks observe `engine_receive_time_ns`. A submission made inside a
callback uses that visible virtual time as `strategy_submit_time_ns`. Synthetic
fill timestamps are the order's activation time, including all partial fills
calculated during that activation.

The current HW3 `FillSimulator` fills synchronously at method call time and copies
the caller-provided request timestamp to every fill. It has no clock, market-data
latency, order latency, or pending scheduler.

## 6. Dispatch sequencing and ready-signal contract

Group A introduces:

```cpp
using DispatchSeq = std::uint64_t;
```

`DispatchSeq` starts at 1, is global for the fully merged replay stream, and is
strictly monotonically increasing. It is independent of per-instrument or
provider `SeqNo`, and independent of the HW3 `(source_file_id, source_sequence)`
tie-break metadata.

Each configured trading engine owns one atomic `processed_seq`, initialized to 0.
Only that engine's consumer publishes its acknowledgment. For dispatch `N`, it
updates `processed_seq` only after all causal work triggered by `N` is complete:

- the local market view has been updated;
- the strategy callback has returned;
- generated order requests have been captured;
- immediately available order events have been processed;
- `OrderManager` has been updated;
- `PositionKeeper` has been updated.

The consumer publishes completion with a release operation, for example
`processed_seq.store(N, std::memory_order_release)`, followed by notification.
The dispatcher observes each acknowledgment with an acquire load/wait. The
release/acquire pair makes all state and order effects before acknowledgment
visible to the dispatcher and subsequent dispatch processing. Relaxed ordering is
not sufficient, and acknowledgment must never be published before callback work.

The dispatcher may parse and prefetch future input. It must not apply dispatch
`N + 1` to the shared `HistoricalLobStore` until every required engine has
acknowledged at least `N`. Engine failure must abort the run deterministically
rather than silently remove the engine from the barrier.

The HW3 sharded processor's worker counters use release/acquire to drain worker
queues, but they are not per-trading-engine acknowledgments. In particular,
`ShardedLobMarketDataEventProcessor::processMarketDataEvent` can return before a
worker has applied the event, so it cannot directly satisfy this barrier.

## 7. Equal-timestamp ordering

For each next historical event, compare activation time with each engine's computed
receive time. The deterministic rule is:

1. Process pending synthetic orders with
   `activation_time < market_event_time` in configured engine order.
2. Apply the historical event to the shared historical store.
3. Deliver that historical event to trading engines in configured engine order.
4. Complete the ready-signal barrier for the historical dispatch.
5. Process synthetic orders with
   `activation_time == market_event_time` in configured engine order.

Each pending-order scheduler assigns a monotonically increasing local sequence at
submission capture. Orders with the same activation timestamp are ordered by that
stable local sequence. If outputs from multiple private engine schedulers must be
combined, configured engine order precedes the per-engine local sequence; no
unordered-container iteration may decide the result.

This deliberately gives the historical event precedence over an order activating
at the same engine-local virtual timestamp. It is an approved contract decision,
not an implementation accident, and future tests must enforce it. Equal-timestamp
historical events keep their deterministic merged input order before receiving
consecutive `DispatchSeq` values.

## 8. Order-lifecycle contract

The required states are:

- `PendingNew`
- `Active`
- `PartiallyFilled`
- `Filled`
- `CancelPending`
- `Cancelled`
- `Rejected`

Submission capture creates `PendingNew`. At activation, invalid orders become
`Rejected`; accepted orders become `Active`, `PartiallyFilled`, or `Filled`
according to immediate fills and remaining quantity. A cancel of live quantity
enters `CancelPending` and becomes `Cancelled` when applied. A cancel request for
an unknown or terminal order produces a deterministic rejection event without
rewriting the existing terminal state.

Responsibilities remain separate:

- `OrderManager` owns client-order lifecycle state, original quantity, cumulative
  filled quantity, remaining quantity, status transitions, and terminal-state
  validation.
- `EngineView` owns the private synthetic LOB overlay and privately consumed
  historical-liquidity state.
- `FillSimulator` owns fill calculation only.
- `PositionKeeper` owns signed positions and accounting.

These responsibilities must not be merged into one class. In particular,
`FillSimulator` must return deterministic execution results to the lifecycle layer
instead of mutating order status or positions itself.

## 9. Position contract

- A `Side::Bid` (Buy) fill increases signed position by its quantity.
- A `Side::Ask` (Sell) fill decreases signed position by its quantity.
- Positions are maintained independently per instrument and per trading engine.
- Every fill has a stable execution identity. Re-delivery of the same fill must be
  detected and must not change position or accounting twice.
- Cumulative fill quantity greater than original order quantity is an overfill and
  must be detected before position mutation. Arithmetic overflow is also a
  deterministic error.
- Basic signed position is required first. Realized/unrealized PnL, fees, and cash
  accounting may be added later, but the interface must accept enough execution
  context to add them without replacing the position lifecycle.

The basic position representation must be signed and wide enough for canonical
`Quantity`; conversion and subtraction require checked arithmetic.

## 10. Multi-engine contract

- There is one shared `HistoricalLobStore` for the replay.
- Every configured engine has a unique, nonzero `TradingEngineId`.
- There is one private `EngineView` per `TradingEngineId`.
- There is one `processed_seq` acknowledgment per trading engine.
- An engine never observes another engine's synthetic orders or privately consumed
  historical liquidity.
- Synthetic fills never mutate the shared historical book.
- The dispatcher waits for every configured engine before releasing the next
  historical dispatch.
- Engine construction and callback order come from stable configuration order,
  not map/hash iteration order.

The current HW3 `EngineView`/`SimulatedLOB`/`FillSimulator` design already
demonstrates the intended private-overlay isolation. It does not provide the
runtime, acknowledgments, lifecycle, positions, or event-loop integration.

## 11. Determinism requirements

Two runs with identical input events, engine configuration, latency configuration,
and strategy decisions must produce identical:

- dispatch order and `DispatchSeq` assignment;
- fills and fill timestamps;
- order states and transition order;
- positions;
- final `HistoricalLobStore` digest;
- per-engine visible state.

Stable source and local order tie-breakers must be explicit. Digests and serialized
outputs must sort unordered data. Randomness, if later introduced, requires a
configured seed and defined consumption order. Wall-clock measurements, thread
scheduling, addresses, and unordered-container iteration must not affect any
causal output.

## 12. Implementation map

The paths are proposals on top of the approved `hw3-solution` ancestry. A1 may
adjust directory ownership without weakening this contract.

| Task | Proposed files |
| --- | --- |
| A1 — domain type cleanup and latency configuration | `src/domain/Types.hpp`, `src/backtest/LatencyConfig.hpp`, compatibility adapters in `src/common/BasicTypes.hpp` |
| A2 — `DispatchSeq` and market-data envelope | `src/messaging/MarketDataMessage.hpp`, `src/backtest/MarketDataEventAdapter.hpp` and `.cpp` |
| A3 — `VirtualClock` | `src/backtest/VirtualClock.hpp` and `.cpp` |
| A4 — `ReadySignal` | `src/concurrency/ReadySignal.hpp` and `tests/concurrency/ReadySignalTest.cpp` |
| A5 — `MarketDataConsumer` | `src/messaging/MarketDataConsumer.hpp` and `.cpp` |
| A6 — `PendingOrderScheduler` | `src/backtest/PendingOrderScheduler.hpp` and `.cpp` |
| A7 — `OrderManager` | `src/gateway/OrderManager.hpp` and `.cpp` |
| A8 — `PositionKeeper` | `src/trading/PositionKeeper.hpp` and `.cpp` |
| A9 — gateway / `FillSimulator` integration | `src/backtest/OrderExecutionBridge.hpp` and `.cpp`, `src/lob/FillSimulator.*`, `src/gateway/` |
| A10 — `TradingEngineRuntime` | `src/trading/TradingEngineRuntime.hpp` and `.cpp` |
| A11 — Backtest Engine to Trading Engine integration | `src/backtest/IntegratedBacktestEngine.hpp` and `.cpp`, `src/backtest/MarketDataEventAdapter.*` |
| A12 — synthetic end-to-end harness | `bindings/python/backtester_module.cpp`, `python/backtester/`, `tests/backtest/SyntheticEndToEndTest.cpp` |
| A13 — multi-engine and thread-safety tests | `tests/backtest/MultiEngineTest.cpp`, `tests/lob/ThreadSafetyTest.cpp` |
| A14 — ready-signal benchmark | `benchmarks/ReadySignalBenchmark.cpp` and benchmark CMake registration |

## 13. Known gaps in the current repository

| Gap | Concrete evidence and impact |
| --- | --- |
| Canonical types are absent | `src/domain/Types.hpp` does not exist. New Group A APIs have no canonical `InstrumentId`, `TradingEngineId`, `SeqNo`, or `OrderStatus`. |
| Existing types conflict | `src/common/BasicTypes.hpp`, `src/domain/MarketDataEvent.hpp`, and `src/lob/LobTypes.hpp` disagree on price, quantity, identifiers, timestamps, and side representations. A1 must retain `Side::Bid/Ask/None` and add checked adapters without introducing another runtime Side enum. |
| No market-data envelope or publisher/subscriber | `src/messaging/MarketDataMessage.hpp`, `MarketDataPublisher.hpp`, and `MarketDataSubscriber.hpp` are absent. There is no global `DispatchSeq`, callback fan-out, or completion barrier. |
| No accepted-event or post-event update payload | `HistoricalLOB::apply` returns `void`; `IMarketDataEventProcessor` accepts only a raw event. Unknown/invalid no-ops cannot be distinguished from accepted mutations, and no ordered absolute post-level `BookUpdate` batch is published. |
| Sequence semantics are incomplete | `MarketDataEvent::source_sequence` is per input file and only a merge tie-breaker. There is no per-instrument market-data `SeqNo` and no global dispatch sequence. |
| Fill timing is immediate | `FillSimulator::submitLimitOrder` calculates fills synchronously and copies `request.timestamp_ns`; it has no activation queue, equal-time rule, virtual clock, or per-engine latency. |
| Lifecycle handling is only implicit overlay state | `EngineView` stores live synthetic orders and silently ignores unknown cancels. It has no `PendingNew`, partial/terminal status model, cumulative fill validation, rejection events, or idempotent execution processing. |
| No position/accounting owner | No `PositionKeeper` or equivalent exists. Duplicate fills and overfills are not guarded at an accounting boundary. |
| No order gateway or integration engine | `src/gateway/`, `OrderExecutionBridge`, `IntegratedBacktestEngine`, and `MarketDataEventAdapter` are absent. Historical replay, fills, lifecycle, and strategies are not connected. |
| No virtual clock or pending scheduler | No `VirtualClock`, per-engine latency configuration, or `PendingOrderScheduler` exists. Historical timestamps order input only. |
| Current thread ownership is not a causal barrier | Hard runners have producer/merger threads and one dispatcher, which is reusable. `ShardedLobMarketDataEventProcessor::processMarketDataEvent` returns after enqueue and drains only for snapshots/final reads; it can expose event N+1 before N is applied unless wrapped by the Group A ready barrier. |
| Multi-engine runtime is absent | HW3 LOB tests prove private overlay isolation, but there is no trading-engine market-data consumer, per-engine `processed_seq`, strategy callback, or wait-for-all dispatcher. |
| Python integration is absent | `bindings/python/backtester_module.cpp` and `python/backtester/` do not exist. A12 must bind or call the C++ causal engine rather than create a second Python matcher. |
| HW4 behavioral tests are absent | The HW3 suite covers replay, LOB state, isolation, immediate fills, concurrency, and historical determinism, but not virtual time, DispatchSeq, ready signaling, lifecycle, positions, or the integrated causal loop. |
| Ready-signal benchmark is absent | There is no `benchmarks/ReadySignalBenchmark.cpp` or equivalent measurement of the proposed acknowledgment path. |
| Baseline pre-commit is not clean | Full pre-commit reformats 86 tracked files: Ruff Format changes two Python scripts and clang-format changes 84 C++ runtime/test files. A0R restores those unrelated edits; the exhaustive list is in `HW4_BASELINE.md` and belongs to the separate A0.1 format-only task. |

Runtime ancestry, canonical Side, moving-Modify/Clear callbacks, per-engine fixed
latency, configured engine ordering, and historical priority at equal timestamps
are approved and require no further A1 design approval.

## 14. Acceptance criteria for Group A

- [ ] Group A remains based on `hw3-solution`, and every reused HW3 component and
  test continues to build and pass.
- [ ] `src/domain/Types.hpp` is the only canonical Group A type source, with checked
  adapters for legacy/wire types and `Side::Bid/Ask/None` as the sole C++ Side.
- [ ] Historical events receive global `DispatchSeq` values while source `SeqNo`
  retains its independent scope.
- [ ] Add/Modify/Cancel callbacks publish absolute post-event level size, a moving
  Modify emits old then new updates, Clear publishes a post-clear snapshot, and
  Trade/Fill use `on_trade`.
- [ ] Per-engine virtual time and both fixed non-negative per-engine latencies
  control every causal timestamp without wall-clock influence.
- [ ] Equal-timestamp historical/synthetic ordering matches section 7 in focused
  tests.
- [ ] Every engine acknowledges dispatch only after callbacks, immediate order
  events, lifecycle state, and positions are complete.
- [ ] `OrderManager`, `EngineView`, `FillSimulator`, and `PositionKeeper` retain
  separate responsibilities and enforce valid lifecycle transitions.
- [ ] Duplicate fills are idempotent, overfills are rejected, and signed positions
  are correct per engine and instrument.
- [ ] Multiple engines share historical state but never observe each other's
  synthetic or consumed-liquidity state.
- [ ] Two identical runs produce identical dispatches, fills, order states,
  positions, historical digest, and per-engine state.
- [ ] End-to-end, multi-engine, thread-safety, and ready-signal tests pass, and the
  ready-signal benchmark is recorded without changing semantics.
