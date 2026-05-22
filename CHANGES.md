# Changes

Refactor + optimization pass on the C++20 NDJSON market-data ingestion pipeline.

## What changed

### Domain / parsing
- Removed dead diagnostic counters (`empty_lines`, `invalid_json_lines`, `invalid_messages`) and the `ParseStatus` enum. The contract is "input is always correct"; failure paths were unreachable.
- `parseMarketDataEventLine` now returns `MarketDataEvent` directly (was `std::optional<MarketDataEvent>`).
- Rewrote `src/parsing/JsonParser.cpp` against simdjson (vendored at `third_party/simdjson`). Highlights:
  - `thread_local simdjson::ondemand::parser` + `thread_local std::vector<char>` padded buffer (simdjson requires `SIMDJSON_PADDING` trailing bytes).
  - Price parsing keeps Databento's fixed-point `1e-9` contract for quoted decimal strings, numeric decimals, and `null` (`INT64_MAX`).
  - Howard-Hinnant `daysFromCivil` + manual time math replaced with `std::chrono::sys_days` + `year/month/day`.
  - File now ~140 lines (was 273).

### I/O
- `StandardRunner` and the producer threads in `HardRunnerSupport` read NDJSON sequentially with buffered `std::ifstream` / `std::getline`.
- Each reader path reuses one `std::string` line buffer and parses the line immediately as a `std::string_view`.
- An mmap line reader was benchmarked but not kept: it helped one Standard-mode single-file measurement, but it was not a clear throughput win for hard-mode flat/hierarchy folder runs.

### Build
- `CMakeLists.txt`: added `third_party/simdjson/simdjson.cpp` to `ingest_core`. simdjson sources are built with `-Wno-error -w` (we don't own its warnings).
- `target_include_directories(ingest_core PRIVATE third_party/simdjson)`.

### Merge tuning
- Replaced the bounded queue stage with `NonBlockingQueue`, an unbounded batched SPSC queue. Producers publish batches without waiting for free ring slots; consumers wait for the next batch with `std::atomic::wait/notify_one`, not mutexes or condition variables.
- `event_queue_batch_size = 256` in `src/runners/HardRunnerSupport.hpp`.
- `mergeInputQueues` kept as a binary min-heap (`std::priority_queue<HeapNode>`). Considered a flat linear scan over `k` heads — for `k ≈ 20` the asymptotic difference (`log k` vs `k`) is small and the heap reads cleanly; left as-is.

### Profiling
- `build_profiling.sh` — release+debug build (`-O3 -g -fno-omit-frame-pointer -DNDEBUG`).
- `profile_standard.sh` — runs the binary under macOS `/usr/bin/sample` and `samply` (Firefox-Profiler flamegraph).
- `scripts/benchmark.sh` — benchmark driver.

## Performance

Hard-mode folder throughput is driven primarily by merge topology. The hierarchical merge remains the main optimization because it spreads merge work across more cores than the flat single-merger pipeline.

Reader follow-up: mmap was benchmarked against buffered stream line reading. It was about 21.7% faster in one Standard-mode single-file run, but stream reading was about 32.2% faster in flat mode and about 16.7% faster in hierarchy mode. Since Task 1 only needs sequential line/window reading, the simpler buffered stream implementation is kept.

## How to use

### Build
```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### Run
Long-form:
```sh
./build/ingest --mode standard     --input <file.jsonl>   [--verbose] [--print-events N]
./build/ingest --mode flat         --input <folder>       [--verbose] [--print-events N]
./build/ingest --mode hierarchy    --input <folder>       [--verbose] [--print-events N]
./build/ingest --benchmark <folder>
```

Shortcuts (mode as positional, then path):
```sh
./build/ingest <file.jsonl>                 # implicit standard
./build/ingest standard <file.jsonl>
./build/ingest flat <folder>
./build/ingest hierarchy <folder>
./build/ingest benchmark <folder>
```

`--print-events N` caps how many parsed events the dispatcher prints (default 10). `--benchmark` forces it to 0 so timing isn't bound by stdout.

### Modes
- **standard** — single file, single thread, buffered stream read -> parse -> dispatch.
- **flat** — folder of files; one producer thread per file feeds a batched SPSC queue, one merger thread runs a k-way min-heap merge across all queues, one dispatcher consumes the merged stream.
- **hierarchy** — folder of files; 4-way tree of mergers (parallelizable), useful when the merger itself is the bottleneck.
- **benchmark** — runs the hard-task pipeline silently for timing.

### Tests
```sh
cmake --build build -j
ctest --test-dir build --output-on-failure
```

### Profiling (macOS)
```sh
./build_profiling.sh                  # produces build-prof/ingest with -O3 -g
./profile_standard.sh <file.jsonl>    # writes a /usr/bin/sample text report and a samply .json.gz flamegraph
```
Open the `.json.gz` at <https://profiler.firefox.com> for the flamegraph.

## Layout
```
src/
  app/            CLI parsing
  domain/         MarketDataEvent + comparator
  io/             File-reading helpers
  parsing/        JsonParser (simdjson)
  processing/     IMarketDataEventProcessor + LoggingMarketDataEventProcessor
  runners/        StandardRunner, FlatMergeRunner, HierarchicalMergeRunner, HardRunnerSupport, ResultPrinter
  concurrency/    NonBlockingQueue
tests/            test_main.cpp (CTest target ingest_tests)
third_party/
  simdjson/       vendored simdjson.{h,cpp}
docs/             requirements
```
