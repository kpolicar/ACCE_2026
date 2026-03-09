# Lab 2: Basic CUDA Programming

**Accelerator-Centric Computing Ecosystems (XM_0171)**  
**Target hardware:** DAS-5 `gpunode,TitanX` (Maxwell, sm_52)

## Getting started

SSH into DAS-5 and copy the Lab2 directory to your home folder. Each task
lives in its own subdirectory with a `.cu` source file and a batch script.

Edit the source file to complete the TODOs, then submit from within the
task directory:

```bash
cd ~/Lab2/Task1
sbatch run_task1.sh
```

The batch script handles module loading, compilation, and execution.
Check the output once your job finishes:

```bash
cat task1_output_[job_id].txt
```

Repeat for tasks 2 through 4.

## Task progression

1. **Task 1 -- Memory management:** Use `cudaMalloc`, `cudaMemcpy`, `cudaFree`.

2. **Task 2 -- Kernel implementation:** Write the matmul dot product.
   With N=16 and a single block, `Test PASSED` should appear.

3. **Task 3 -- Multi-block launch:** Compute `gridDim` with ceiling division to cover
   the full 512x512 matrix. Includes CUDA event timing and speedup reporting.

4. **Task 4 -- Grid-stride loop:** With a fixed small grid (4x4 blocks), each thread
   must process multiple elements via a stride loop.