import matplotlib.pyplot as plt
import pandas as pd
from pathlib import Path

script_dir = Path(__file__).resolve().parent
repo_root = script_dir.parent

file_name = "28-04-2026-staged.csv"

source_file = repo_root / "results" / "tabulated" / file_name
output_file = repo_root / "results" / "plots" / "staged"

with source_file.open("r") as file:
    df = pd.read_csv(file)

def plot_all(capacity):
    x_all = df["payload_bytes"].unique()
    
    fig, ax = plt.subplots()

    implementations = ["SPSCBufferBaseline", 
                       "SPSCBufferStage1", 
                       "SPSCBufferStage2", 
                       "SPSCBufferStage3",
                       "SPSCBufferStage4",
                       "SPSCBufferStage5",
                       "SPSCBufferStage6"
                      ]
    
    shapes = ["x", "o", "|", "^", "s", "p", "h"]

    colors = ["gray", "brown", "purple", "blue", "green", "orange", "red"]

    for implementation, shape, color in zip(implementations, shapes, colors):
        y_impl = df.loc[(df["implementation"] == implementation) & 
                        (df["capacity_items"] == capacity), 
                        ["items_per_second_median"]
                       ]
        
        ax.plot(x_all, y_impl, marker=shape, color=color, label=implementation)

    ax.legend()

    ax.set_xscale("log")
    ax.set_yscale("log")

    ax.set_xlabel("Payload (Bytes)")
    ax.set_ylabel("Median Items/Second")

    fig.suptitle(f"SPSCBuffer Throughput With Staged Optimizations")

    fig.savefig(output_file / "staged_throughput.svg")

    plt.close(fig)

def plot_speedups(capacity):
    x_all = df["payload_bytes"].unique()
    
    y_baseline = df.loc[(df["implementation"] == "SPSCBufferBaseline") & 
                        (df["capacity_items"] == capacity), 
                        ["items_per_second_median"]
                       ]
    
    y_baseline = y_baseline.reset_index(drop=True)

    fig, ax = plt.subplots()

    implementations = ["SPSCBufferStage1", 
                       "SPSCBufferStage2", 
                       "SPSCBufferStage3",
                       "SPSCBufferStage4",
                       "SPSCBufferStage5",
                       "SPSCBufferStage6"
                      ]
    
    shapes = ["o", "|", "^", "s", "p", "h"]

    colors = ["brown", "purple", "blue", "green", "orange", "red"]

    for implementation, shape, color in zip(implementations, shapes, colors):
        y_impl = df.loc[(df["implementation"] == implementation) & 
                        (df["capacity_items"] == capacity), 
                        ["items_per_second_median"]
                       ]
        
        y_impl = y_impl.reset_index(drop=True)
        
        y_speedup = y_impl.div(y_baseline) 

        ax.plot(x_all, y_speedup, marker=shape, color=color, label=implementation)

    ax.legend()

    ax.set_xscale("log")
    #ax.set_yscale("log")

    ax.set_xlabel("Payload (Bytes)")
    ax.set_ylabel("Speedup")

    fig.suptitle(f"SPSCBuffer With Staged Optimizations Speedup VS Baseline")

    fig.savefig(output_file / "staged_speedups.svg")

    plt.close(fig)

capacity = 4096

plot_all(capacity)
plot_speedups(capacity)