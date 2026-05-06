import matplotlib.pyplot as plt
import pandas as pd
from pathlib import Path

script_dir = Path(__file__).resolve().parent
repo_root = script_dir.parent

file_name = "28-04-2026-main.csv"

source_file = repo_root / "results" / "tabulated" / file_name
output_file = repo_root / "results" / "plots" / "headline"

with source_file.open("r") as file:
    df = pd.read_csv(file)

def plot_fixed_capacity(capacity):
    fig, ax = plt.subplots()

    x_MB = df.loc[(df["implementation"] == "MutexBuffer") & (df["capacity_items"] == capacity),
                "payload_bytes"].copy()

    y_MB = df.loc[(df["implementation"] == "MutexBuffer") & (df["capacity_items"] == capacity),
                "items_per_second_median"].copy()

    x_SPSC = df.loc[(df["implementation"] == "SPSCBuffer") & (df["capacity_items"] == capacity),
                "payload_bytes"].copy()

    y_SPSC = df.loc[(df["implementation"] == "SPSCBuffer") & (df["capacity_items"] == capacity),
                "items_per_second_median"].copy()

    ax.plot(x_MB, y_MB, marker='o', color="blue", label="MutexBuffer")
    ax.plot(x_SPSC, y_SPSC, marker='s', color="red", label="SPSCBuffer")

    ax.legend()

    ax.set_xscale("log")
    ax.set_yscale("log")

    ax.set_xlabel("Payload (Bytes)")
    ax.set_ylabel("Median Items/Second")

    fig.suptitle(f"MutexBuffer VS SPSCBuffer (Median Throughput @ {capacity} Capacity)")

    fig.savefig(output_file / "fixed_capacity.svg")

    plt.close(fig)

def plot_fixed_payload(payload_bytes):
    fig, ax = plt.subplots()

    x_MB = df.loc[(df["implementation"] == "MutexBuffer") & (df["payload_bytes"] == payload_bytes),
                "capacity_items"].copy()

    y_MB = df.loc[(df["implementation"] == "MutexBuffer") & (df["payload_bytes"] == payload_bytes),
                "items_per_second_median"].copy()

    x_SPSC = df.loc[(df["implementation"] == "SPSCBuffer") & (df["payload_bytes"] == payload_bytes),
                "capacity_items"].copy()

    y_SPSC = df.loc[(df["implementation"] == "SPSCBuffer") & (df["payload_bytes"] == payload_bytes),
                "items_per_second_median"].copy()

    ax.plot(x_MB, y_MB, marker='o', color="blue", label="MutexBuffer")
    ax.plot(x_SPSC, y_SPSC, marker='s', color="red", label="SPSCBuffer")

    ax.legend()

    ax.set_xscale("log")
    ax.set_yscale("log")

    ax.set_xlabel("Capacity")
    ax.set_ylabel("Median Items/Second")

    fig.suptitle(f"MutexBuffer VS SPSCBuffer (Median Throughput @ {payload_bytes}-Byte Payload)")

    fig.savefig(output_file / "fixed_payload.svg")

    plt.close(fig)

def speedup_heatmap():
    mutex = df[df["implementation"] == "MutexBuffer"].copy()
    spsc = df[df["implementation"] == "SPSCBuffer"].copy()

    merged = pd.merge(spsc,
                      mutex,
                      on=["payload_bytes", "capacity_items"],
                      suffixes=["_spsc", "_mutex"]
                     )

    merged["speedup"] = (
        merged["items_per_second_median_spsc"] /
        merged["items_per_second_median_mutex"]
    )

    grid = merged.pivot(
        index="payload_bytes",
        columns="capacity_items",
        values="speedup"
    )

    fig, ax = plt.subplots()

    image = ax.imshow(grid)

    ax.set_xticks(range(len(grid.columns)))
    ax.set_xticklabels(grid.columns, rotation=90)

    ax.set_yticks(range(len(grid.index)))
    ax.set_yticklabels(grid.index)

    ax.set_xlabel("Capacity (Items)")
    ax.set_ylabel("Payload Size (Bytes)")
    ax.set_title("SPSCBuffer Speedup Over MutexBuffer")

    colorbar = fig.colorbar(image, ax=ax)
    colorbar.set_label("Speedup Factor")

    fig.savefig(output_file / "speedup_heatmap.svg")
    plt.close(fig)

capacity = 4096
payload_size = 8

plot_fixed_capacity(capacity)
plot_fixed_payload(payload_size)
speedup_heatmap()