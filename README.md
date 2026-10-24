# High-Performance Market Data Feed Handler & Matching Engine

A C++20 systems-programming project implementing a limit order book, matching engine, binary market-data protocol decoder, TCP stream processor, and Linux `epoll`-based networking.

The project explores price-time priority, order lifecycle management, performance-oriented data structures, binary message processing, and event-driven networking concepts relevant to quantitative trading infrastructure and high-performance backend engineering.

> **Status:** The CMake build succeeds and all 8 registered CTest suites pass in the current development environment.
>
> **Scope:** This is an educational systems-programming project, not a production-ready exchange or trading system.

## Features

### Matching engine

- Limit order book with bid and ask price levels.
- Price-time priority matching.
- Order insertion, cancellation, and modification.
- Partial fills and trade execution.
- Implemented order types include Good-Till-Cancelled, Fill-and-Kill, Fill-or-Kill, Good-for-Day, and Market orders.
- Custom order index for order lookup and removal.
- Preallocated node pool to reduce repeated allocation overhead.
- Execution reports, trade information, and engine events.
- Randomized testing and order-book invariant validation.

### Market data feed processing

- Binary protocol decoding with big-endian field handling.
- Validation of supported message types, lengths, and fields.
- TCP stream reconstruction for fragmented messages and multiple messages in a stream.
- Feed-handler integration for orders and cancellations.
- Nonblocking TCP networking.
- Linux `epoll` server implementations for level-triggered and edge-triggered configurations.
- Per-client message-processing counters.

### Testing and performance analysis

- Unit tests for matching behavior and supported order types.
- Fill-or-Kill and Fill-and-Kill tests.
- Randomized order-book testing.
- Decoder, cancellation, feed-handler, and stream-processor tests.
- TCP integration testing.
- Benchmarks for order-book operations, matching latency, decoding, stream processing, order indexes, and allocation strategies.

## Architecture

```text
                    TCP Client
                        |
                        v
                  Linux TCP Server
                        |
                        v
                    epoll Loop
                   LT / ET Modes
                        |
                        v
                 Stream Processor
             TCP Message Reassembly
                        |
                        v
                 Protocol Decoder
                Validation + Parsing
                        |
                        v
                   Feed Handler
                        |
                        v
                  Matching Engine
                        |
                        v
                  Limit Order Book
                  /      |      \
                 v       v       v
           Price Levels  Index  Node Pool
                        |
                        v
                  Trades and Events
```

### Message-processing flow

1. A client connects to the TCP server.
2. The server receives data through a nonblocking socket.
3. The stream processor reconstructs complete protocol messages.
4. The decoder validates and parses supported messages.
5. The feed handler forwards valid orders and cancellations.
6. The matching engine updates the book and executes compatible orders.
7. The server tracks message-processing statistics.

## Technology stack

| Technology | Purpose |
|---|---|
| C++20 | Matching engine and feed-processing implementation |
| CMake | Build configuration and test integration |
| Linux sockets | TCP communication |
| `epoll` | Event-driven network I/O |
| C++ STL | Order-book and supporting data structures |
| Custom order index | Order lookup and removal |
| Preallocated node pool | Order-book node reuse |
| Python 3 | TCP integration client and supporting scripts |
| Docker | Linux development environment on macOS and other platforms |

## Project structure

```text
.
├── CMakeLists.txt
├── Dockerfile
├── README.md
├── include/
│   ├── core/                 # Matching-engine headers
│   └── feed/                 # Feed-processing headers
├── src/
│   ├── core/                 # Matching-engine implementation
│   ├── feed/                 # Decoder, stream processor, TCP servers
│   └── main.cpp              # Demo entry point
├── tests/
│   ├── unit/                 # Matching-engine unit tests
│   ├── integration/          # TCP integration client
│   └── ...                   # Feed and decoder tests
├── benchmarks/               # Performance benchmark programs
├── docs/                     # Project documentation
└── scripts/                  # Supporting scripts
```

## Requirements

- A C++20-compatible compiler.
- CMake.
- Linux for the `epoll`-based TCP servers.
- Python 3 for the TCP integration client.
- Docker is recommended when developing on macOS.

The `epoll` server uses Linux-specific APIs and must run in a Linux environment.

## Build

Run the following commands from the project root:

```bash
cmake -S . -B build/cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON \
  -DBUILD_BENCHMARKS=ON

cmake --build build/cmake -j2
```

To configure without benchmark targets:

```bash
cmake -S . -B build/cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON \
  -DBUILD_BENCHMARKS=OFF

cmake --build build/cmake -j2
```

## Run the tests

```bash
ctest --test-dir build/cmake --output-on-failure
```

The current configuration registers 8 test suites:

| Test suite | Coverage |
|---|---|
| `test_matching` | Matching-engine behavior |
| `test_order_types` | Supported order-type behavior |
| `test_fok_levels` | Fill-or-Kill behavior across price levels |
| `test_randomized` | Randomized operations and book invariants |
| `test_decoder` | Binary protocol decoding |
| `test_cancel_decoder` | Cancellation-message decoding |
| `test_feed_handler` | Feed-handler order and cancellation paths |
| `test_stream_processor` | TCP stream processing and message reconstruction |

**Latest recorded validation:** 8/8 tests passed.

Rebuild after code changes before rerunning CTest.

## Run the TCP server

Build the project first. Find the generated server executables:

```bash
find build/cmake -type f \
  \( -name 'epoll_feed_server_lt' -o -name 'epoll_feed_server_et' \) \
  -executable -print
```

Start the level-triggered server using the path printed by the command. For example, if it is located under `build/cmake/bin`:

```bash
./build/cmake/bin/epoll_feed_server_lt
```

Or start the edge-triggered server:

```bash
./build/cmake/bin/epoll_feed_server_et
```

Use the actual paths reported by `find` if your CMake configuration places executables elsewhere.

The server listens on TCP port `9000`, as configured by the implementation. Stop it with `Ctrl+C`.

## TCP integration test

With the server running, open a second terminal in the same Linux environment and run:

```bash
python3 tests/integration/test_tcp_client.py
```

The integration scenario sends a new-order message in fragments and then sends a cancellation message.

In the tested scenario, both level-triggered and edge-triggered configurations processed the messages successfully.

Representative observed server counters:

```text
Bytes received:       40
Messages processed:    2
Messages rejected:     0
```

These counters describe the tested scenario, not maximum throughput. The integration client introduces delays between writes.

## Benchmarks

The `benchmarks/` directory contains benchmark programs for areas including:

- Order insertion and cancellation.
- Matching operations and latency.
- Binary message decoding.
- Feed and stream processing.
- Order-index implementations.
- Allocation and node-pool strategies.
- Event logging.

Build benchmark targets with:

```bash
cmake -S . -B build/cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON \
  -DBUILD_BENCHMARKS=ON

cmake --build build/cmake -j2
```

Inspect available benchmark executables:

```bash
find build/cmake -type f -executable -iname '*benchmark*' -print
```

Run a benchmark using the executable path and options supported by that benchmark. Review its source before interpreting results.

### Benchmarking considerations

Measurements depend on hardware, compiler, optimization settings, workload, and measurement methodology. Single-run measurements should be treated as preliminary observations rather than statistically robust performance claims.

For meaningful comparisons, record hardware, compiler version, build flags, workload, number of trials, and latency percentiles where applicable. Compare results only when test conditions are equivalent.

## Design considerations and limitations

This project focuses on matching-engine fundamentals, data structures, protocol decoding, stream processing, and event-driven networking.

It does not claim production readiness or exchange-grade performance.

A production trading system would require additional engineering, including:

- A clearly specified and versioned wire protocol.
- Comprehensive malformed-input and protocol fuzz testing.
- Explicit concurrency and ownership guarantees.
- Persistence, recovery, and replay guarantees.
- Risk controls and operational monitoring.
- More extensive failure-injection and multi-client stress testing.
- Reproducible end-to-end latency and throughput measurements.

## Future improvements

Potential next steps include protocol fuzzing, sanitizer-enabled builds, deterministic message replay, automated benchmark comparisons, and expanded multi-client testing.

## License

No open-source license has been selected yet. Until a license is added, do not assume that the repository is released under a permissive open-source license.
