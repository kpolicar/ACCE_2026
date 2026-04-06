# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

CUDA parallelization of a grid-based rainwater flood simulation for the VU Amsterdam ACCE course (XM_0171). The task is to implement `do_compute()` in `flood_cuda.cu` as a GPU-accelerated version of the sequential reference in `flood_seq.c`. Target: 20x+ speedup over sequential on DAS-5 TitanX GPUs (50x+ is achievable).

## Build Commands

Requires `module load cuda12.6/toolkit` on DAS-5 before compiling.

```bash
make all              # Build both flood_seq and flood_cuda
make flood_seq        # Sequential only
make flood_cuda       # CUDA only
make debug            # Sequential with -DDEBUG (verbose per-step output)
make animation        # Sequential with -DDEBUG -DANIMATION (generates animation data)
make clean            # Remove binaries and .o files
```

## Running

```bash
# Local (sequential only, not for benchmarking)
./flood_seq $(< test_files/debug.in)

# DAS-5 compute node (required for GPU and accurate timing)
prun -np 1 -native '-C gpunode,TitanX' ./flood_cuda $(< test_files/small_mountains.in)

# Via SLURM (see job.sh)
sbatch job.sh
```

## Correctness Verification

```bash
# Generate reference and CUDA outputs, then compare
prun -np 1 -native '-C gpunode' ./flood_seq $(< test_files/small_mountains.in) > res_seq.out
prun -np 1 -native '-C gpunode' ./flood_cuda $(< test_files/small_mountains.in) > res_cuda.out
python3 test_files/check_correctness.py res_seq.out res_cuda.out
```

Tolerances: iteration count must match exactly; other stats allow small relative error (<1% for flow/level, <0.01% for water totals).

## Test Files

All under `test_files/`. Listed small-to-large:
- `debug.in` — tiny grid for debugging
- `small_mountains.in`, `custom_clouds.in` — low resolution, fast
- `medium_lower_dam.in`, `medium_higher_dam.in` — medium resolution
- `large_mountains.in` — high resolution (~3 min sequential)

Correctness is checked against at least the 4 non-debug inputs.

## Architecture

### Simulation Loop (the code to parallelize)

Each timestep in `do_compute()` has three phases:
1. **Cloud movement + rainfall** — clouds advance by velocity, deposit rain onto grid cells within their radius. Updates `water_level` (fixed-point int array).
2. **Spillage computation** — for each cell with water, compute potential flow to 4 neighbors (up/down/left/right) proportional to height differences. Writes into `spillage_from_neigh` (3D array: rows x cols x 4).
3. **Water update** — apply spillage: subtract from source cells, add from neighbors. All flows divided by `SPILLAGE_FACTOR=2`. Compute max spillage for termination check.

Loop terminates when max spillage < threshold or max iterations reached.

### Key Files

- **flood.c** — main(), I/O, ground generation, cloud initialization. Calls `do_compute()`. Can be modified to support optimizations (e.g., different data layouts).
- **flood_seq.c** — sequential reference implementation of `do_compute()`. Do not modify. Use as ground truth.
- **flood_cuda.cu** — CUDA implementation file. Currently contains a copy of the sequential code. This is where GPU kernels go.
- **flood.h** — shared structs (`Cloud_t`, `parameters`, `results`), macros (`FIXED`/`FLOATING` for fixed-point, `accessMat`/`accessMat3D` for array indexing), constants.
- **rng.c/rng.h** — deterministic random number generator for cloud initialization.

### Critical Data Representation

Water levels use **fixed-point integers** (scale factor 1,000,000) via `FIXED()`/`FLOATING()` macros. Ground heights are plain floats. The `accessMat(arr, row, col)` macro does row-major indexing: `arr[row * columns + col]`. Spillage per neighbor uses `accessMat3D` with depth dimension = 4 (one per direction).

### Boundary Conditions

Out-of-bounds neighbors are treated as dry cells with the same ground height as the in-bounds cell. Water flowing out of bounds is tracked in `r->total_water_loss`.

### Statistics Accumulated During Simulation

The `results` struct tracks: iteration count, max spillage (value + minute), max water level, total rain/water/water_loss. These use `long` fixed-point accumulators — race conditions in parallel updates will cause incorrect results.