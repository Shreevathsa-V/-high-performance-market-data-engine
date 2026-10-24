import csv
import statistics
from pathlib import Path

RESULTS_DIR = Path("benchmark_results")


def load_results(filename):
    path = RESULTS_DIR / filename

    with path.open(newline="", encoding="utf-8") as f:
        rows = list(csv.DictReader(f))

    measured = [
        row for row in rows
        if row["run_type"] == "measured"
    ]

    if len(measured) != 10:
        raise RuntimeError(
            f"{filename}: expected 10 measured runs, "
            f"found {len(measured)}"
        )

    for row in measured:
        if row["correctness_verified"].lower() != "true":
            raise RuntimeError(
                f"{filename}: correctness verification failed"
            )

        if int(row["messages_processed"]) != 20_000:
            raise RuntimeError(
                f"{filename}: unexpected processed message count"
            )

        if int(row["bytes_received"]) != 400_000:
            raise RuntimeError(
                f"{filename}: unexpected byte count"
            )

        if int(row["messages_rejected"]) != 0:
            raise RuntimeError(
                f"{filename}: rejected messages detected"
            )

        if int(row["verified_clients"]) != 4:
            raise RuntimeError(
                f"{filename}: expected four verified clients"
            )

    return [
        float(row["throughput_messages_per_second"])
        for row in measured
    ]


lt = load_results("epoll_lt.csv")
et = load_results("epoll_et.csv")

lt_median = statistics.median(lt)
et_median = statistics.median(et)

lt_mean = statistics.mean(lt)
et_mean = statistics.mean(et)

print("EPOLL PERFORMANCE COMPARISON")
print("=" * 55)
print("All measured runs passed correctness validation.")
print()

print(f"{'Metric':<25}{'Level-triggered':>15}{'Edge-triggered':>15}")
print("-" * 55)

print(
    f"{'Median (msg/s)':<25}"
    f"{lt_median:>15,.0f}"
    f"{et_median:>15,.0f}"
)

print(
    f"{'Mean (msg/s)':<25}"
    f"{lt_mean:>15,.0f}"
    f"{et_mean:>15,.0f}"
)

print(
    f"{'Minimum (msg/s)':<25}"
    f"{min(lt):>15,.0f}"
    f"{min(et):>15,.0f}"
)

print(
    f"{'Maximum (msg/s)':<25}"
    f"{max(lt):>15,.0f}"
    f"{max(et):>15,.0f}"
)

print(
    f"{'Std deviation (msg/s)':<25}"
    f"{statistics.stdev(lt):>15,.0f}"
    f"{statistics.stdev(et):>15,.0f}"
)

print()

if lt_median > et_median:
    difference = (lt_median / et_median - 1) * 100
    print(
        f"Level-triggered has {difference:.2f}% higher "
        "median throughput in these runs."
    )
elif et_median > lt_median:
    difference = (et_median / lt_median - 1) * 100
    print(
        f"Edge-triggered has {difference:.2f}% higher "
        "median throughput in these runs."
    )
else:
    print("Both configurations have equal median throughput.")

print()
print("Interpretation: these are benchmark observations,")
print("not proof of a universal performance advantage.")