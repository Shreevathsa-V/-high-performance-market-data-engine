# Benchmark Results

## Overview

This document records preliminary performance measurements for the High-Performance Market Data Feed Handler and Matching Engine.

The benchmarks cover:
- Limit order insertion and cancellation
- Matching operation latency
- Binary protocol decoding
- Stream processing under different input conditions

All measurements below were collected from the project's Release build. Results are specific to the test environment and workloads described here.

## Test Environment

| Property | Value |
|---|---|
| Operating system | Ubuntu 24.04-based container environment |
| Kernel | Linux 6.12.76-linuxkit |
| Architecture | ARM64 (`aarch64`) |
| CPU vendor reported by container | Apple |
| CPUs visible to container | 8 |
| Compiler | GCC 13.3.0 |
| Build type | Release |
| Release flags | `-O3 -DNDEBUG` |
| Memory cgroup limit | No limit reported |
| CPU cgroup quota | No quota reported |

The exact processor model, host operating-system configuration, CPU frequency, and physical resource contention were not independently measured. The results should therefore be treated as measurements of this Docker environment, not as hardware-independent guarantees.

## Build and Test Configuration

Configure the project:

```bash
cmake -S . -B build/cmake -DCMAKE_BUILD_TYPE=Release
```

Build selected benchmarks:

```bash
cmake --build build/cmake \
  --target benchmark_add benchmark_cancel \
           benchmark_matching_latency benchmark_decoder \
           benchmark_stream_processor -j2
```

Run the automated test suite:

```bash
ctest --test-dir build/cmake --output-on-failure
```

The clean-clone verification passed all 8 tests. The Release build also passed all 8 tests, with no test failures.

## 1. Order Insertion

Each run inserted 100,000 orders.

| Metric | Run 1 | Run 2 |
|---|---:|---:|
| Orders inserted | 100,000 | 100,000 |
| Average insertion time | 106.22 ns/order | 180.91 ns/order |
| Throughput | 9.41 million orders/sec | 5.53 million orders/sec |
| Final book size | 100,000 | 100,000 |
| Book validation | Passed | Passed |

Insertion throughput varied between runs. Further repeated measurements under controlled host conditions are needed to estimate stable performance.

## 2. Order Cancellation

Each run cancelled 100,000 orders from an initially populated order book.

| Metric | Run 1 | Run 2 |
|---|---:|---:|
| Cancellations | 100,000 | 100,000 |
| Average cancellation time | 37.64 ns/cancel | 40.11 ns/cancel |
| Throughput | 26.57 million cancels/sec | 24.93 million cancels/sec |
| Final book size | 0 | 0 |
| Book validation | Passed | Passed |

These results describe the cancellation benchmark's workload. They do not represent end-to-end cancellation latency over a network.

## 3. Matching Latency

The matching-latency benchmark measures the time required to process incoming orders against an order book containing resting orders.

### Workload and methodology

- Prepared 100,000 resting sell orders before measurement.
- Used 100,000 incoming buy orders, each priced to cross the resting sell orders.
- Executed 10,000 warm-up matches using a separate order book.
- Created a fresh order book for the measured workload.
- Recorded one latency sample per matching operation using `std::chrono::steady_clock`.
- Measured total batch duration separately to estimate throughput.
- Validated the order book before and after the measured workload.

The warm-up operations are excluded from the 100,000 measured latency samples. Each measured interval includes clock-call overhead.

| Metric | Run 1 | Run 2 | Run 3 | Run 4 | Run 5 |
|---|---:|---:|---:|---:|---:|
| Matching operations | 100,000 | 100,000 | 100,000 | 100,000 | 100,000 |
| Average measured latency | 233.55 ns | 171.24 ns | 162.75 ns | 155.16 ns | 157.12 ns |
| p50 latency | 125 ns | 125 ns | 125 ns | 125 ns | 125 ns |
| p90 latency | 333 ns | 208 ns | 167 ns | 167 ns | 167 ns |
| p99 latency | 542 ns | 292 ns | 292 ns | 292 ns | 291 ns |
| p99.9 latency | 1.25 us | 4.875 us | 0.834 us | 0.958 us | 0.792 us |
| Maximum observed latency | 5.00 ms | 1.46 ms | 1.77 ms | 1.33 ms | 1.27 ms |
| Approximate throughput | 3.70 million ops/sec | 4.82 million ops/sec | 5.07 million ops/sec | 5.23 million ops/sec | 5.23 million ops/sec |
| Final book size | 0 | 0 | 0 | 0 | 0 |
| Book validation | Passed | Passed | Passed | Passed | Passed |

These results are preliminary measurements from the documented container environment, not guarantees of performance on other hardware.

### Interpretation

The observed median latency was 125 ns in all five runs. The p99 latency ranged from 291 ns to 542 ns, while approximate throughput ranged from 3.70 million to 5.23 million matching operations per second.

The maximum observed latency ranged from approximately 1.27 ms to 5.00 ms. Such long outliers can disproportionately affect average latency and may reflect operating-system scheduling or other environmental interference. These measurements alone cannot identify the cause.

The results show that the median was stable across these trials, but tail latency and throughput varied. Five runs are insufficient to establish a reliable performance distribution or a production latency guarantee.

### Measurement limitations

- Individual measurements include clock-call overhead, which is significant relative to nanosecond-scale operations.
- The estimated clock overhead is informational only and is not subtracted from samples.
- The workload measures in-process matching operations, not network-to-network latency.
- The reported throughput is measured separately over the batch and should not be inferred from the median latency.
- The benchmark runs in a container without independently verified host CPU frequency or physical resource isolation.
- Results should not be compared directly with other implementations unless hardware, compiler, workload, and measurement methodology are comparable.

## 4. Binary Protocol Decoder

The decoder benchmark warms up each operation with 100,000 calls and then measures 1,000,000 iterations. It checks decoded values and counts successful decodes.

### New-order decoding

| Metric | Run 1 | Run 2 |
|---|---:|---:|
| Iterations | 1,000,000 | 1,000,000 |
| Successful decodes | 1,000,000 | 1,000,000 |
| Average time per decode | 9.51 ns | 11.66 ns |
| Throughput | 105.17 million messages/sec | 85.76 million messages/sec |

### Cancel-order decoding

| Metric | Run 1 | Run 2 |
|---|---:|---:|
| Iterations | 1,000,000 | 1,000,000 |
| Successful decodes | 1,000,000 | 1,000,000 |
| Average time per decode | 5.01 ns | 5.81 ns |
| Throughput | 199.63 million messages/sec | 172.03 million messages/sec |

These are decoder microbenchmark measurements. They exclude socket I/O, network transmission, and end-to-end server processing.

Repeated decoding of the same input under compiler optimization may not represent the full cost of decoding varied production traffic. These results should not be interpreted as TCP server throughput.

## 5. Stream Processor

Each scenario processed 1,000,000 messages, representing 500,000 order/cancel pairs. All scenarios reported zero rejected messages and passed correctness validation.

| Scenario | Run 1 throughput | Run 2 throughput |
|---|---:|---:|
| Complete messages in separate calls | 17.45 million messages/sec | 17.10 million messages/sec |
| Order and cancellation in one buffer | 23.77 million messages/sec | 24.09 million messages/sec |
| Order fragmented across two calls | 21.28 million messages/sec | 21.62 million messages/sec |

| Scenario | Run 1 time/message | Run 2 time/message |
|---|---:|---:|
| Complete messages in separate calls | 57.32 ns | 58.49 ns |
| Order and cancellation in one buffer | 42.07 ns | 41.51 ns |
| Order fragmented across two calls | 46.99 ns | 46.26 ns |

These measurements characterize the stream processor's tested input patterns. They do not include full network round-trip latency.

## Methodology and Limitations

- Measurements were collected from an optimized Release build.
- Two runs are reported for each benchmark; this is not a comprehensive statistical performance study.
- Results can vary due to scheduling, host load, virtualization, CPU frequency management, and other environmental effects.
- The benchmark workloads and data patterns influence the results.
- Per-operation latency measurements include clock-reading overhead.
- Decoder and stream-processor microbenchmarks do not measure complete end-to-end TCP server throughput.
- No comparison against a production trading engine or external reference implementation was performed.
- Results should not be compared directly with figures obtained using different hardware, compilers, workloads, or measurement methods.

For stronger performance claims, repeat the tests under controlled conditions, run multiple trials, report variability, and validate the benchmark methodology independently.

## Reproducing the Benchmarks

Configure an optimized build:

```bash
cmake -S . -B build/cmake -DCMAKE_BUILD_TYPE=Release
```

Build the benchmark executables:

```bash
cmake --build build/cmake \
  --target benchmark_add benchmark_cancel \
           benchmark_matching_latency benchmark_decoder \
           benchmark_stream_processor -j2
```

Run the benchmarks:

```bash
./build/bin/benchmark_add
./build/bin/benchmark_cancel
./build/bin/benchmark_matching_latency
./build/bin/benchmark_decoder
./build/bin/benchmark_stream_processor
```

Run the automated tests:

```bash
ctest --test-dir build/cmake --output-on-failure
```
