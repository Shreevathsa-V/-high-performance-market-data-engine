# High-Performance Market Data Feed Handler & Matching Engine

A C++20 systems-programming project implementing a limit order book, matching engine, binary market-data protocol decoder, TCP stream processor, and Linux `epoll`-based networking.

The project focuses on correctness, low-level systems concepts, performance measurement, and clear separation between the matching engine and network-facing components. It is an educational engineering project—not a production trading system.

## Table of Contents

- [Features](#features)
- [Architecture](#architecture)
- [Technology Stack](#technology-stack)
- [Project Structure](#project-structure)
- [Requirements](#requirements)
- [Build](#build)
- [Run the Tests](#run-the-tests)
- [Run the TCP Server](#run-the-tcp-server)
- [TCP Integration Test](#tcp-integration-test)
- [Benchmarks](#benchmarks)
- [Correctness and Performance Notes](#correctness-and-performance-notes)
- [Limitations](#limitations)
- [Future Improvements](#future-improvements)
- [License](#license)

## Features

### Limit Order Book and Matching Engine

- Maintains bid and ask sides with price levels.
- Supports price-time priority.
- Supports adding, cancelling, and modifying orders.
- Handles matching, partial fills, and trade execution.
- Supports Good-Till-Cancelled (GTC), Fill-and-Kill (FAK), Fill-or-Kill (FOK), Good-for-Day (GFD), and market orders, as implemented by the engine.
- Produces execution reports, trade information, and engine events.
- Includes order-book validation and invariant-oriented tests.
- Uses a custom order index and preallocated node-pool allocation to explore data-structure and allocation trade-offs.

### Market Data Feed Processing

- Decodes binary protocol messages, including big-endian fields.
- Validates incoming message fields before processing.
- Handles new-order and cancel message types.
- Reconstructs messages from TCP byte streams, including fragmented messages.
- Separates protocol decoding and stream processing from matching-engine logic.

### Networking

- Includes nonblocking TCP server implementations for Linux.
- Uses `epoll` for socket multiplexing, with level-triggered and edge-triggered variants.
- Tracks per-client message counters and processing statistics.
- Keeps network input processing separate from the core matching engine.

### Testing and Performance Analysis

- Unit and integration-oriented test coverage for matching, order types, decoders, stream processing, and feed handling.
- Randomized order-book invariant testing.
- Benchmarks for order insertion, cancellation, matching latency, binary decoding, stream processing, indexes, allocation, and event logging.
- A separate benchmark report documents the recorded environment, methodology, results, and limitations: [`BENCHMARKS.md`](BENCHMARKS.md).

## Architecture

The high-level processing flow is:

```text
TCP Client
    |
    v
Nonblocking TCP Server + epoll
    |
    v
TCP Stream Processor
    |  reconstructs complete protocol messages
    v
Binary Protocol Decoder
    |  validates and decodes fields
    v
Feed Handler
    |
    v
Limit Order Book / Matching Engine
    |
    +----> Execution Reports
    |
    +----> Trades
    |
    +----> Engine Events
```

The diagram is conceptual. Exact class and executable names may vary with the current source tree.

## Technology Stack

- **Language:** C++20
- **Build system:** CMake
- **Compiler used for the recorded benchmarks:** GCC 13.3.0
- **Networking:** Linux sockets and `epoll`
- **Testing:** CTest and project test executables
- **Integration test:** Python 3
- **Version control:** Git and GitHub

## Project Structure

The repository is organized around the matching engine, protocol/feed-processing components, network servers, tests, and benchmarks. Key areas include:

```text
.
├── CMakeLists.txt
├── README.md
├── BENCHMARKS.md
├── include / headers
├── source files
├── tests/
│   └── integration/
│       └── test_tcp_client.py
└── benchmark source files
```

This is a high-level guide rather than a complete file listing. Use `find . -maxdepth 3 -type f` to inspect the exact layout in your checkout.

## Requirements

- CMake
- A C++ compiler with C++20 support (GCC or Clang)
- Linux for the `epoll` networking components
- Python 3 for the TCP integration test
- Git

The core matching engine may be buildable on other platforms, but the `epoll` server components are Linux-specific.

## Build

From the repository root, configure a Release build and compile:

```bash
cmake -S . -B build/cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build/cmake -j2
```

If the project defines the `BUILD_TESTING` and `BUILD_BENCHMARKS` CMake options, they can be enabled explicitly:

```bash
cmake -S . -B build/cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON \
  -DBUILD_BENCHMARKS=ON

cmake --build build/cmake -j2
```

If CMake reports that either option is unused, check the option names supported by the current `CMakeLists.txt` rather than assuming those flags take effect.

## Run the Tests

Run the registered tests from the repository root:

```bash
ctest --test-dir build/cmake --output-on-failure
```

In the clean-clone verification and a subsequent Release build, **all 8 registered test suites passed**. Results may differ if the code or build configuration changes.

For a clean verification, clone the repository into a new directory and run the configure, build, and test commands above.

## Run the TCP Server

The Linux TCP servers listen on port **9000**. Build the project first:

```bash
cmake -S . -B build/cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON \
  -DBUILD_BENCHMARKS=ON
cmake --build build/cmake --parallel 2
```

Run one server at a time. The level-triggered and edge-triggered implementations are separate executables:

```bash
./build/bin/epoll_feed_server_lt
```

Or:

```bash
./build/bin/epoll_feed_server_et
```

The ordinary TCP server is available as:

```bash
./build/bin/tcp_feed_server
```

Stop the selected server with Ctrl+C before starting another server on the same port.

## TCP Integration Test

The Python integration client is located at:

```text
tests/integration/test_tcp_client.py
```

Start the TCP server in one terminal, then run the client from the repository root in another terminal:

```bash
python3 tests/integration/test_tcp_client.py
```

The client must be compatible with the server's binary message format. A representative successful run reported:

- Bytes received: `40`
- Messages processed: `2`
- Messages rejected: `0`

These are example results from a test run, not guaranteed output for every client payload or source revision.

## Benchmarks

Benchmarks are useful for comparing implementations and detecting regressions, but the figures below are **preliminary measurements from two Release runs in a containerized Linux ARM64 environment**. They are not production performance guarantees.

### Recorded Results

| Benchmark | Run 1 | Run 2 |
|---|---:|---:|
| Order insertion throughput | 9.41 million orders/sec | 5.53 million orders/sec |
| Cancellation throughput | 26.57 million cancels/sec | 24.93 million cancels/sec |
| Matching latency, p50 | 84 ns | 83 ns |
| Matching latency, p99 | 375 ns | 291 ns |
| New-order decoder throughput | 105.17 million messages/sec | 85.76 million messages/sec |
| Cancel decoder throughput | 199.63 million messages/sec | 172.03 million messages/sec |
| Stream processor: separate calls | 17.45 million messages/sec | 17.10 million messages/sec |
| Stream processor: order and cancel in same buffer | 23.77 million messages/sec | 24.09 million messages/sec |
| Stream processor: fragmented order | 21.28 million messages/sec | 21.62 million messages/sec |

### Benchmark Environment

The recorded runs used:

- Linux kernel `6.12.76-linuxkit`
- ARM64 (`aarch64`) container environment
- GCC `13.3.0`
- CMake Release build with `-O3 -DNDEBUG`
- 100,000 orders for insertion, cancellation, and matching-latency tests
- 1,000,000 iterations/messages for decoder and stream-processor tests

The environment reported an Apple CPU vendor in the container, but the exact host CPU model was not established. The results should therefore be treated as measurements of this specific environment, not as a precise hardware characterization.

### How to Reproduce

Configure and build in Release mode:

```bash
cmake -S . -B build/cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build/cmake -j2
```

Inspect the benchmark executables produced by the build:

```bash
find build -type f -executable
```

Run the relevant benchmark executables from the paths shown by that command. Benchmark target names and output paths depend on the CMake configuration; do not assume a target exists unless it appears in your build.

For the full recorded results, environment details, methodology, and caveats, see [`BENCHMARKS.md`](BENCHMARKS.md).

### How to Interpret These Numbers

- Insertion throughput varied noticeably between the two runs.
- The matching-latency benchmark reported a minimum of `0 ns` in these runs. This is not evidence that matching takes no time; it reflects limitations of measuring very short operations with a nanosecond-resolution timer and should not be presented as a meaningful latency figure.
- The matching benchmark includes timing overhead. The p50 and p99 values are more useful for understanding the recorded distribution than the reported minimum, but they are still environment- and workload-dependent.
- Decoder and stream-processor microbenchmarks measure in-process work. They are **not end-to-end TCP throughput** and do not include the full cost of a live network connection.
- For credible performance comparisons, repeat runs under controlled conditions, record the hardware and compiler settings, and compare equivalent workloads.

## Correctness and Performance Notes

- The project has a clean-clone build and test verification recorded in its development notes.
- The latest recorded CTest result was **8/8 tests passed**.
- Benchmark figures are reported with their workload and environment so they are not mistaken for universal guarantees.
- Passing tests and strong microbenchmark numbers do not establish production readiness for real-money trading.

## Limitations

- This is an educational project and has not been validated for production trading.
- Benchmark results depend on compiler, CPU, operating system, build flags, workload, and runtime conditions.
- The recorded results are preliminary and based on two runs, not a statistically rigorous benchmark campaign.
- The TCP protocol is a project-specific binary protocol and should not be assumed to match any exchange's production protocol.
- Linux `epoll` networking components are platform-specific.
- Further work is needed for robust operational controls, stress testing, fault handling, observability, and production-grade protocol/security review.

## Future Improvements

Potential next steps include:

- Add repeatable benchmark automation and report median and percentile results across many runs.
- Add more adversarial and property-based tests for order-book invariants and malformed protocol messages.
- Add backpressure handling and more extensive TCP disconnect/reconnect tests.
- Improve benchmark reproducibility by documenting the exact CPU model, compiler flags, and runtime conditions.
- Evaluate cache locality and data-structure alternatives using profiling before making optimization claims.
- Add continuous integration for builds and tests on supported platforms.
- Document protocol message layouts and the matching rules in more detail.

## License

No open-source license has been selected for this repository yet. Until a license is added, do not assume that the code is available for unrestricted reuse, modification, or redistribution.
