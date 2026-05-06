# Benchmark Environment

## Hardware

- **CPU:** Intel Core Ultra 7 155H
- **Reported CPU Frequency Range:** 400 MHz–4.50 GHz, from `cpupower frequency-info`
- **CPU Governor:** Performance
- **Background Load:** Nonessential applications closed during benchmark collection

## System / Toolchain

- **OS:** Ubuntu 24.04
- **Compiler:** g++ 13.3.0
- **C++ Standard:** C++20
- **Build Type:** Release
- **Benchmark Framework:** Google Benchmark
- **Benchmark Output Format:** JSON
- **Repository Commit:** `<commit hash>`

## Benchmark Configuration

- **Producer/Consumer Model:** 1 producer, 1 consumer
- **Items Transferred Per Benchmark:** 10,000,000
- **Payload Sizes (Bytes):** 1, 2, 4, 8, 16, 64, 256
- **Capacities:** 1, 4, 64, 256, 1024, 4096, 16384, 32768
- **Timing Mode:** Wall-clock timing via `UseRealTime`
- **Warmup Time:** 0.5s
- **Repetitions:** 20
- **Reported Primary Metric:** Median `items_per_second` across 20 repetitions

## Commands

### Build Commands

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

### Run Commands

```bash
sudo cpupower frequency-set --governor performance
sudo nice -n -20 taskset -c 2,4 ./build/bench/Bench \
  --benchmark_out=results/raw/<output>.json \
  --benchmark_out_format=json
```