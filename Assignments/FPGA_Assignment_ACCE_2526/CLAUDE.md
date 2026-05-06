# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What This Is

VU ACCE course FPGA assignment: a grid-based iterative simulation of rainwater flooding, implemented in Vitis HLS for FPGA. The simulation models a cloud front moving over a heightmap, computing per-minute water redistribution across a 2D grid.

There are two implementations:
- **Base**: direct port of the CPU sequential algorithm to Vitis HLS (`flood_HLS_base.cpp`)
- **Optimized**: performance-improved variant (`flood_HLS_optimized.cpp`) using HLS pragmas (loop unrolling, array partitioning, pipelining, etc.)

## Key Constraints

`NROWS`, `NCOLS`, and `NCLOUDS` are **compile-time constants** defined via the TCL script (not runtime parameters). They must be set in the TCL script to match the test scenario before running. The `FLOOD.h` header enforces this with `#error` directives.

`malloc`/`free` are **not synthesizable** — static arrays must be used in the HLS implementation. The `__SYNTHESIS__` macro excludes non-synthesizable code (like `printf`) from the RTL build.

## Build & Run Commands

### CPU Reference (from `cpu_impl/`)
```bash
make flood              # build
sbatch job.sh           # run all 4 test scenarios via SLURM
make test               # quick local test (small_dam)
make debug              # build with -DDEBUG -g
```

### FPGA HLS (from repo root)
```bash
sbatch job_base.sh               # run full HLS flow: C-sim + synthesis + RTL co-sim
sbatch job_optimized.sh          # same for optimized version (create analogously)
vitis_hls -f run_FLOOD_HLS_base.tcl      # run directly (requires vivado/2024.1 module)
vitis_hls -f run_FLOOD_HLS_optimized.tcl
```

The SLURM job loads `module load vivado/2024.1`. Always run synthesis on the same compute node for reproducibility.

### Verify Correctness
```bash
# C-simulation output
python3 test_files/check_correctness.py \
  cpu_impl/tiny_mountains.out \
  FLOOD_HLS_base/solution_FLOOD_HLS_base/csim/build/tiny_mountains.out

# RTL co-simulation output
python3 test_files/check_correctness.py \
  cpu_impl/tiny_mountains.out \
  FLOOD_HLS_base/solution_FLOOD_HLS_base/sim/wrapc_pc/tiny_mountains.out
```

The checker tolerates small floating-point differences; the iteration count must match exactly.

## File Organization

| File | Purpose |
|------|---------|
| `FLOOD.h` | Shared types (`Cloud_t`, `parameters`, `results`), macros (`FIXED`, `FLOATING`, `accessMat`, `accessMat3D`), and `do_compute` declaration |
| `flood_HLS_base.cpp` | HLS top function — edit this for the base implementation |
| `flood_HLS_optimized.cpp` | HLS top function — edit this for the optimized implementation |
| `test_FLOOD_base.cpp` | Testbench: reads input file, initializes scenario, calls `do_compute` |
| `run_FLOOD_HLS_base.tcl` | TCL script: sets `NROWS`/`NCOLS`/`NCLOUDS`, runs csim/csynth/cosim |
| `rng.h` / `rng.cpp` | Shared RNG used in testbench and simulation |
| `cpu_impl/flood.c` | Sequential CPU reference (ground truth for correctness) |
| `test_files/*.in` | Input argument files for each scenario |
| `test_files/check_correctness.py` | Correctness checker script |

## Algorithm Structure (`do_compute`)

Each simulated minute:
1. **Cloud movement** — advance each cloud's `(x, y)` by `(dx, dy) / 60`
2. **Rainfall** — for each cloud's bounding box, add `FIXED(rain_m_per_min)` to `water_level`
3. **Spillage computation** — for each cell with water, compute proportional flow to lower neighbors into `spillage_from_neigh[NROWS][NCOLS][4]`
4. **Spillage propagation** — apply `spillage_from_neigh` to update `water_level`, track `max_spillage_iter`
5. **Reset** — zero `spillage_flag`, `spillage_level`, `spillage_from_neigh` for next iteration

Water levels use fixed-point arithmetic via `FIXED()`/`FLOATING()` macros (multiplier: 1,000,000) to ensure reproducible results across parallelization strategies.

## TCL Script Configuration

To switch scenarios, edit the TCL script:
```tcl
set defs "-DNROWS=60 -DNCOLS=80 -DNCLOUDS=9"
set fp [open "test_files/small_mountains9c.in" r]
```

To skip co-simulation (much faster for development), comment out the `cosim_design` line.
To enable automatic loop pipelining, remove or comment out `config_compile -pipeline_loops 0`.

## Required Test Scenarios (Evaluation)

| Input file | Grid | Clouds | Minutes | C-sim | Co-sim |
|-----------|------|--------|---------|-------|--------|
| `tiny_mountains6c.in` | 40×40 | 6 | 10 | yes | yes |
| `tiny_dam7c.in` | 50×50 | 7 | 10 | yes | yes |
| `small_mountains9c.in` | 60×80 | 9 | 100 | yes | no |
| `small_dam9c.in` | 90×90 | 9 | 120 | yes | no |

The optimized implementation should achieve ≥5× latency reduction vs. the pipelined base for tiny scenarios.

## HLS Output Locations

- Synthesis report: `FLOOD_HLS_base/solution_FLOOD_HLS_base/syn/report/`
- C-sim build output: `FLOOD_HLS_base/solution_FLOOD_HLS_base/csim/build/`
- Co-sim output: `FLOOD_HLS_base/solution_FLOOD_HLS_base/sim/wrapc_pc/`
- Log file: `FLOOD_HLS_base/solution_FLOOD_HLS_base/solution_FLOOD_HLS_base.log`

These directories can grow to hundreds of MB — delete when not needed.
