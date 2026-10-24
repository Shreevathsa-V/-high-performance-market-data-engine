import csv
import statistics
from pathlib import Path

RESULTS_DIR = Path("benchmark_results")


def load_results(filename):
    path = RESULTS_DIR / filename

    with path.open(newline="", encoding="utf-8") as file:
        rows = list(csv.DictReader(file))

    measured = [
        row for row in rows
        if row["run_type"] == "measured"
    ]

    if len(measured) != 10:
        raise ValueError(
            f"{filename}: expected 10 measured runs, "
            f"found {len(measured)}"
        )

    for row in measured:
        if row["correctness_verified"].lower() != "true":
            raise ValueError(
                f"{filename}: correctness validation failed"
            )

        if int(row["messages_processed"]) != 20_000:
            raise ValueError(
                f"{filename}: unexpected message count"
            )

        if int(row["bytes_received"]) != 400_000:
            raise ValueError(
                f"{filename}: unexpected byte count"
            )

        if int(row["messages_rejected"]) != 0:
            raise ValueError(
                f"{filename}: rejected messages detected"
            )

        if int(row["verified_clients"]) != 4:
            raise ValueError(
                f"{filename}: expected four verified clients"
            )

    return [
        float(row["throughput_messages_per_second"])
        for row in measured
    ]


def summarize(values):
    return {
        "median": statistics.median(values),
        "mean": statistics.mean(values),
        "min": min(values),
        "max": max(values),
        "stdev": statistics.stdev(values),
    }


results = {
    "LT 4 KiB": summarize(load_results("epoll_lt_4k.csv")),
    "LT 16 KiB": summarize(load_results("epoll_lt_16k.csv")),
    "ET 4 KiB": summarize(load_results("epoll_et_4k.csv")),
    "ET 16 KiB": summarize(load_results("epoll_et_16k.csv")),
}

print("RECEIVE BUFFER COMPARISON")
print("=" * 78)
print("All four datasets passed correctness validation.\n")

print(
    f"{'Configuration':<16}"
    f"{'Median (msg/s)':>17}"
    f"{'Mean (msg/s)':>17}"
    f"{'Min (msg/s)':>14}"
    f"{'Max (msg/s)':>14}"
)

print("-" * 78)

for name, metrics in results.items():
    print(
        f"{name:<16}"
        f"{metrics['median']:>17,.0f}"
        f"{metrics['mean']:>17,.0f}"
        f"{metrics['min']:>14,.0f}"
        f"{metrics['max']:>14,.0f}"
    )

print("\nMEDIAN THROUGHPUT CHANGE")

for mode in ("LT", "ET"):
    baseline = results[f"{mode} 4 KiB"]["median"]
    candidate = results[f"{mode} 16 KiB"]["median"]

    change = (candidate / baseline - 1) * 100

    print(
        f"{mode}: {change:+.2f}% "
        f"when increasing the buffer from 4 KiB to 16 KiB"
    )

print(
    "\nNote: these are measurements from separate sessions. "
    "Differences do not establish causation."
)