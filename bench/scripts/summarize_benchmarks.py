import json
import re
import pandas as pd
from pathlib import Path

script_dir = Path(__file__).resolve().parent
repo_root = script_dir.parent

file_name = input("Source: ")

source_file = repo_root / "results" / "raw" / file_name
output_file = repo_root / "results" / "derived" / f"{Path(file_name).stem}.csv"

with source_file.open("r") as file:
    data = json.load(file)

rows = []

PAYLOAD_RE = re.compile(r"BenchPayload<\s*(\d+)\s*>")
CAPACITY_RE = re.compile(r"/(\d+)/")

for benchmark in data["benchmarks"]:
    if benchmark.get("aggregate_name") != "median":
        continue

    if "real_time_median" not in benchmark["name"]:
        continue

    implementation = benchmark["name"][3:].split("<", 1)[0]
    payload_bytes = int(PAYLOAD_RE.search(benchmark["name"]).group(1))
    capacity_items = int(CAPACITY_RE.search(benchmark["name"]).group(1))
    items_per_second_median = int(benchmark["items_per_second"])

    rows.append({
        "implementation": implementation,
        "payload_bytes": payload_bytes,
        "capacity_items": capacity_items,
        "items_per_second_median": items_per_second_median,
        "benchmark_name": benchmark["name"],
        "source_file": str(source_file),
    })

df = pd.DataFrame(rows)

output_file.parent.mkdir(parents=True, exist_ok=True)
df.to_csv(output_file, index=False)

print(f"CSV written to {output_file}")