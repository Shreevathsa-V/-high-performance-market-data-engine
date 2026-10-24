import argparse
import csv
import json
import statistics
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import socket
import struct
import threading


HOST = "127.0.0.1"
PORT = 9000

NUM_CLIENTS = 4
ORDERS_PER_CLIENT = 2_500

WARMUP_RUNS = 2
MEASURED_RUNS = 10

CONNECT_TIMEOUT = 30
BARRIER_TIMEOUT = 30

SERVER_LOG = Path("server_benchmark.log")
RESULTS_DIR = Path("benchmark_results")


def create_new_order(order_id: int) -> bytes:
    return struct.pack(
        "!BHQBBqQ",
        1,
        29,
        order_id,
        0,
        0,
        10_000,
        1,
    )


def create_cancel_order(order_id: int) -> bytes:
    return struct.pack(
        "!BHQ",
        2,
        11,
        order_id,
    )


def run_client(
    client_number: int,
    iteration_number: int,
    start_barrier: threading.Barrier,
) -> tuple[int, int]:

    messages = bytearray()

    first_order_id = (
        1_000_000
        + iteration_number * 1_000_000
        + client_number * 10_000
    )

    for i in range(ORDERS_PER_CLIENT):
        order_id = first_order_id + i
        messages.extend(create_new_order(order_id))
        messages.extend(create_cancel_order(order_id))

    with socket.create_connection(
        (HOST, PORT),
        timeout=CONNECT_TIMEOUT,
    ) as client:

        client.settimeout(CONNECT_TIMEOUT)
        start_barrier.wait(timeout=BARRIER_TIMEOUT)

        client.sendall(messages)
        client.shutdown(socket.SHUT_WR)

        while client.recv(4096):
            pass

    return len(messages), ORDERS_PER_CLIENT * 2


def read_server_results() -> list[dict]:
    if not SERVER_LOG.exists():
        raise RuntimeError(
            f"Server log not found: {SERVER_LOG}. "
            "Start the server with output redirected to this file."
        )

    records = []

    with SERVER_LOG.open("r", encoding="utf-8") as log_file:
        for line in log_file:
            marker = "BENCHMARK_RESULT "

            if marker not in line:
                continue

            payload = line.split(marker, 1)[1].strip()

            try:
                records.append(json.loads(payload))
            except json.JSONDecodeError:
                continue

    return records


def wait_for_server_results(
    previous_count: int,
    expected_clients: int,
) -> list[dict]:

    deadline = time.monotonic() + 10

    while time.monotonic() < deadline:
        records = read_server_results()

        if len(records) >= previous_count + expected_clients:
            return records[
                previous_count:previous_count + expected_clients
            ]

        time.sleep(0.05)

    records = read_server_results()
    actual = max(0, len(records) - previous_count)

    raise RuntimeError(
        f"Expected {expected_clients} server result records, "
        f"but received {actual} within 10 seconds."
    )


def run_benchmark(iteration_number: int) -> dict:

    expected_messages = NUM_CLIENTS * ORDERS_PER_CLIENT * 2
    expected_bytes = expected_messages * 20

    barrier = threading.Barrier(
        NUM_CLIENTS + 1,
        timeout=BARRIER_TIMEOUT,
    )

    previous_results = len(read_server_results())

    with ThreadPoolExecutor(max_workers=NUM_CLIENTS) as executor:
        futures = [
            executor.submit(
                run_client,
                client_number,
                iteration_number,
                barrier,
            )
            for client_number in range(NUM_CLIENTS)
        ]

        start_time = time.perf_counter()
        barrier.wait(timeout=BARRIER_TIMEOUT)

        client_results = [
            future.result(
                timeout=CONNECT_TIMEOUT + BARRIER_TIMEOUT + 30
            )
            for future in futures
        ]

        elapsed = time.perf_counter() - start_time

    bytes_sent = sum(result[0] for result in client_results)
    client_messages = sum(result[1] for result in client_results)

    if bytes_sent != expected_bytes:
        raise RuntimeError(
            f"Expected {expected_bytes} bytes, got {bytes_sent}."
        )

    if client_messages != expected_messages:
        raise RuntimeError(
            f"Expected {expected_messages} client messages, "
            f"got {client_messages}."
        )

    server_results = wait_for_server_results(
        previous_results,
        NUM_CLIENTS,
    )

    processed = sum(
        result["messages_processed"]
        for result in server_results
    )

    rejected = sum(
        result["messages_rejected"]
        for result in server_results
    )

    received_bytes = sum(
        result["bytes_received"]
        for result in server_results
    )

    valid_clients = all(
        result["messages_processed"] == ORDERS_PER_CLIENT * 2
        and result["messages_rejected"] == 0
        and result["bytes_received"] == ORDERS_PER_CLIENT * 40
        for result in server_results
    )

    if (
        len(server_results) != NUM_CLIENTS
        or processed != expected_messages
        or rejected != 0
        or received_bytes != expected_bytes
        or not valid_clients
    ):
        raise RuntimeError(
            "Server verification failed: "
            f"clients={len(server_results)}, "
            f"processed={processed}/{expected_messages}, "
            f"rejected={rejected}, "
            f"bytes={received_bytes}/{expected_bytes}. "
            "Inspect server_benchmark.log."
        )

    return {
        "elapsed_seconds": elapsed,
        "messages": processed,
        "bytes": received_bytes,
        "throughput": processed / elapsed,
        "payload_MB_s": received_bytes / elapsed / 1_000_000,
        "verified_clients": len(server_results),
        "rejected": rejected,
    }


def main() -> None:

    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--mode",
        choices=["lt", "et"],
        required=True,
        help="Server mode: lt (level-triggered) or et (edge-triggered)",
    )
    args = parser.parse_args()

    if not SERVER_LOG.exists():
        raise RuntimeError(
            "server_benchmark.log does not exist. "
            "Start the Docker server with output redirected to it."
        )

    RESULTS_DIR.mkdir(exist_ok=True)

    csv_path = RESULTS_DIR / f"epoll_{args.mode}.csv"

    total_messages = NUM_CLIENTS * ORDERS_PER_CLIENT * 2

    print("Verified Concurrent TCP Benchmark")
    print("==================================")
    print(f"Mode: {args.mode}")
    print(f"Clients: {NUM_CLIENTS}")
    print(f"Messages per iteration: {total_messages:,}")
    print(f"Warm-up iterations: {WARMUP_RUNS}")
    print(f"Measured iterations: {MEASURED_RUNS}")
    print(f"CSV output: {csv_path}")
    print()

    measured = []

    with csv_path.open(
        "w",
        newline="",
        encoding="utf-8",
    ) as csv_file:

        fieldnames = [
            "mode",
            "run_type",
            "iteration",
            "elapsed_seconds",
            "messages_processed",
            "bytes_received",
            "throughput_messages_per_second",
            "payload_MB_per_second",
            "verified_clients",
            "messages_rejected",
            "correctness_verified",
        ]

        writer = csv.DictWriter(
            csv_file,
            fieldnames=fieldnames,
        )
        writer.writeheader()

        total_runs = WARMUP_RUNS + MEASURED_RUNS

        for iteration in range(total_runs):
            run_type = (
                "warmup"
                if iteration < WARMUP_RUNS
                else "measured"
            )

            result = run_benchmark(iteration)

            row = {
                "mode": args.mode,
                "run_type": run_type,
                "iteration": iteration + 1,
                "elapsed_seconds": result["elapsed_seconds"],
                "messages_processed": result["messages"],
                "bytes_received": result["bytes"],
                "throughput_messages_per_second": result["throughput"],
                "payload_MB_per_second": result["payload_MB_s"],
                "verified_clients": result["verified_clients"],
                "messages_rejected": result["rejected"],
                "correctness_verified": True,
            }

            writer.writerow(row)
            csv_file.flush()

            print(
                f"{run_type:8s} {iteration + 1:02d}/{total_runs}: "
                f"{result['throughput']:,.0f} messages/sec | "
                f"{result['elapsed_seconds'] * 1000:.3f} ms | "
                f"verified clients={result['verified_clients']} | "
                f"rejected={result['rejected']}"
            )

            if run_type == "measured":
                measured.append(result["throughput"])

    print()
    print("Final Results")
    print("=============")
    print(f"CSV: {csv_path}")
    print(f"Mean throughput: {statistics.mean(measured):,.0f} messages/sec")
    print(f"Median throughput: {statistics.median(measured):,.0f} messages/sec")
    print(f"Minimum throughput: {min(measured):,.0f} messages/sec")
    print(f"Maximum throughput: {max(measured):,.0f} messages/sec")

    if len(measured) > 1:
        print(
            "Standard deviation: "
            f"{statistics.stdev(measured):,.0f} messages/sec"
        )

    print("\nAll measured iterations passed server-side verification.")


if __name__ == "__main__":
    main()