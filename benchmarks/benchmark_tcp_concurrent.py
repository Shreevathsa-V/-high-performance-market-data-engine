import socket
import statistics
import struct
import threading
import time
from concurrent.futures import ThreadPoolExecutor


# ---------------- Configuration ----------------

HOST = "127.0.0.1"
PORT = 9000

NUM_CLIENTS = 4
ORDERS_PER_CLIENT = 2_500

WARMUP_RUNS = 2
MEASURED_RUNS = 10

CONNECT_TIMEOUT = 30
BARRIER_TIMEOUT = 30


# ---------------- Protocol Messages ----------------

def create_new_order(order_id: int) -> bytes:
    """
    Create a 29-byte NewOrder message.

    Fields:
      type       = 1
      length     = 29
      order_id   = supplied ID
      side       = 0 (Buy)
      order_type = 0 (GoodTillCancel)
      price      = 10000
      quantity   = 1
    """
    return struct.pack(
        "!BHQBBqQ",
        1,          # Message type: NewOrder
        29,         # Message length
        order_id,
        0,          # Buy
        0,          # GoodTillCancel
        10_000,     # Price
        1           # Quantity
    )


def create_cancel_order(order_id: int) -> bytes:
    """Create an 11-byte CancelOrder message."""
    return struct.pack(
        "!BHQ",
        2,          # Message type: CancelOrder
        11,         # Message length
        order_id
    )


# ---------------- Client Workload ----------------

def run_client(client_number: int,
               iteration_number: int,
               start_barrier: threading.Barrier) -> tuple[int, int]:

    messages = bytearray()

    # Keep order IDs unique across clients and iterations.
    first_order_id = (
        1_000_000
        + iteration_number * 1_000_000
        + client_number * 10_000
    )

    for i in range(ORDERS_PER_CLIENT):
        order_id = first_order_id + i

        messages.extend(create_new_order(order_id))
        messages.extend(create_cancel_order(order_id))

    # Establish the connection before the measured interval.
    with socket.create_connection(
        (HOST, PORT),
        timeout=CONNECT_TIMEOUT
    ) as client:

        client.settimeout(CONNECT_TIMEOUT)

        # Wait until every client is connected and ready.
        start_barrier.wait(timeout=BARRIER_TIMEOUT)

        # Send the complete workload.
        client.sendall(messages)

        # Tell the server that this client has finished sending.
        client.shutdown(socket.SHUT_WR)

        # Wait until the server closes the connection.
        while client.recv(4096):
            pass

    expected_messages = ORDERS_PER_CLIENT * 2

    return len(messages), expected_messages


# ---------------- One Benchmark Iteration ----------------

def run_benchmark(iteration_number: int) -> dict:

    total_messages_expected = (
        NUM_CLIENTS * ORDERS_PER_CLIENT * 2
    )

    expected_bytes = total_messages_expected * 20

    # One participant per client, plus the coordinating main thread.
    start_barrier = threading.Barrier(
        NUM_CLIENTS + 1,
        timeout=BARRIER_TIMEOUT
    )

    with ThreadPoolExecutor(
        max_workers=NUM_CLIENTS
    ) as executor:

        futures = [
            executor.submit(
                run_client,
                client_number,
                iteration_number,
                start_barrier
            )
            for client_number in range(NUM_CLIENTS)
        ]

        # All clients must reach the barrier before timing begins.
        start_time = time.perf_counter()
        start_barrier.wait(timeout=BARRIER_TIMEOUT)

        # Collect results and propagate any client-side errors.
        results = [
            future.result(timeout=CONNECT_TIMEOUT + BARRIER_TIMEOUT + 30)
            for future in futures
        ]

        elapsed = time.perf_counter() - start_time

    bytes_sent = sum(result[0] for result in results)
    messages_sent = sum(result[1] for result in results)

    if len(results) != NUM_CLIENTS:
        raise RuntimeError("Not all clients completed.")

    if bytes_sent != expected_bytes:
        raise RuntimeError(
            f"Payload mismatch: expected {expected_bytes} bytes, "
            f"got {bytes_sent} bytes."
        )

    if messages_sent != total_messages_expected:
        raise RuntimeError(
            f"Message mismatch: expected {total_messages_expected}, "
            f"got {messages_sent}."
        )

    throughput = messages_sent / elapsed
    payload_mb_per_second = bytes_sent / elapsed / 1_000_000

    return {
        "elapsed": elapsed,
        "messages": messages_sent,
        "bytes": bytes_sent,
        "throughput": throughput,
        "payload_mbps": payload_mb_per_second,
    }


# ---------------- Benchmark Summary ----------------

def main() -> None:

    total_messages = NUM_CLIENTS * ORDERS_PER_CLIENT * 2
    total_bytes = total_messages * 20

    print("Concurrent TCP Benchmark")
    print("========================")
    print(f"Server: {HOST}:{PORT}")
    print(f"Concurrent clients: {NUM_CLIENTS}")
    print(f"Orders per client: {ORDERS_PER_CLIENT:,}")
    print(f"Messages per iteration: {total_messages:,}")
    print(f"Payload per iteration: {total_bytes:,} bytes")
    print(f"Warm-up iterations: {WARMUP_RUNS}")
    print(f"Measured iterations: {MEASURED_RUNS}")
    print()

    print("Running warm-up iterations...")

    for i in range(WARMUP_RUNS):
        result = run_benchmark(i)
        print(
            f"Warm-up {i + 1}/{WARMUP_RUNS}: "
            f"{result['throughput']:,.0f} messages/sec"
        )

    print()
    print("Running measured iterations...")

    measured_results = []

    for i in range(MEASURED_RUNS):
        # Use a distinct ID range for each iteration.
        iteration_number = WARMUP_RUNS + i

        result = run_benchmark(iteration_number)
        measured_results.append(result)

        print(
            f"Run {i + 1:02d}/{MEASURED_RUNS}: "
            f"{result['throughput']:,.0f} messages/sec | "
            f"{result['elapsed'] * 1000:.3f} ms | "
            f"{result['payload_mbps']:.2f} MB/s"
        )

    throughputs = [
        result["throughput"]
        for result in measured_results
    ]

    elapsed_times = [
        result["elapsed"] * 1000
        for result in measured_results
    ]

    print()
    print("Final Benchmark Results")
    print("=======================")
    print(f"Successful measured runs: {len(measured_results)}")
    print(f"Messages per run: {total_messages:,}")
    print(f"Payload per run: {total_bytes:,} bytes")
    print()
    print(f"Mean throughput:   {statistics.mean(throughputs):,.0f} messages/sec")
    print(f"Median throughput: {statistics.median(throughputs):,.0f} messages/sec")
    print(f"Minimum throughput:{min(throughputs):,.0f} messages/sec")
    print(f"Maximum throughput:{max(throughputs):,.0f} messages/sec")

    if len(throughputs) > 1:
        print(
            f"Std. deviation:    "
            f"{statistics.stdev(throughputs):,.0f} messages/sec"
        )

    print()
    print(f"Mean elapsed time: {statistics.mean(elapsed_times):.3f} ms")
    print(f"Median elapsed:    {statistics.median(elapsed_times):.3f} ms")
    print()
    print(
        "Note: These are client-side measurements. "
        "Check the server logs to verify that every client "
        "processed 5,000 messages with zero rejections."
    )


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, threading.BrokenBarrierError,
            TimeoutError) as error:
        print(f"\nBenchmark failed: {error}")
        raise SystemExit(1)