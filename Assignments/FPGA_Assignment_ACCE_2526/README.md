# VU ACCE Course - FPGA Group Project


## Assignment Description

The problem to implement for this assignment is a grid-based, iterative simulation of rainwater flood over a 30x30 KM terrain.
It features a cloud front moving across a region of land characterized by a heightmap. 
The algorithm logic remains the same as in the previous assignment. We restate the key details below (section "The Algorithm") for completeness.

We want you to implement this algorithm using Vitis HLS for an FPGA. The assignment is split into two parts:
1. Base Implementation: you have to port the original CPU-based algorithm to Vitis HLS, verify correctness, and analyze the synthesis results.
2. Optimized Implementation (optional, but required for higher grades): improves the performance of the base implementation using performance optimization techniques (e.g., unrolling or pipelining loops not automatically optimized by the compiler, partitioning memory, etc.).


### Base implementation
In this first part, you are asked to port the original CPU-based implementation to Vitis HLS. 
Your implementation must be written in the `flood_HLS_base.cpp`, `test_FLOOD_base.cpp`, `run_FLOOD_HLS_base.tcl`, and `FLOOD.h` files. 

To complete this part, you are required to:
- Remove unsupported C/C++ constructs (e.g., `malloc` is not synthesizable in Vitis HLS, while `memset` is), and avoid using pointers in top-level function struct arguments.
- Properly define the interfaces of the top-level function.
- Make sure that the corresponding implementation is correct in both C Simulation and RTL Co-Simulation:
  -  For this, you have to explicitly set the grid size and cloud count as constants. You can conveniently define them in the TCL script. **Be aware:** this must be adjusted according to the considered scenario.
  - For Co-Simulation, we suggest using a small simulation grid with fewer clouds and fewer iterations. This keeps RTL co-simulation time reasonable (a few minutes for `tiny_dam7c.in` on a 50x50 grid for 10 simulated minutes, if loop pipelining is enabled) 
- Analyze the performance by inspecting the produced synthesis report and the log file (with the default configuration provided with this assignment, you will find the latter under `FLOOD_HLS_base/solution_FLOOD_HLS_base/solution_FLOOD_HLS_base.log `). To make sense of it, you must:
  - Provide the synthesizer with hints about the number of iterations of loops. For this, you can consider using `#pragma HLS loop_tripcount` where needed.
  - Reason about the performance of the various loops in the top function. We suggest you properly label them to easily identify them in the report. Aspects to discuss include (but are not limited to): *Are the loops pipelined? What is the achieved Initiation Interval (II)? Why is the II not equal to 1, if that's the case?*
  - **Note**: by default, the provided TCL script disables automatic loop pipelining (check the `config_compile` flag in the script). For the sake of the analysis, we ask you to run synthesis and report your findings **without and with** automatic pipelining. 


You will find more details on the files and directory organization under the [Structure ](#structure) section.

### Optimized Implementation

After you complete the base implementation, you should have gained knowledge of some of the issues or limitations of the base implementation.
In this optional (but recommended) part, you have the freedom to optimize your implementation, using some of the techniques we discussed in class, or other you can find in the Vitis documentation.

Your implementation must be written in the `flood_HLS_optimized.cpp`, `test_FLOOD_optimized.cpp`, and `run_FLOOD_HLS_optimized.tcl` files. By default, they have the same content of the `_base` files.

As in the previous step, we require you to:
- Ensure functional correctness.
- Analyze performance improvements and describe what changes you made, why they improved performance, and  which techniques were applied (e.g., loop unrolling, array partitioning, dataflow pipelining, etc.).


### The Algorithm

Note: this section is identical to the corresponding one of the GPU assignment.

The problem for this assignment is grid-based, iterative simulation of rainwater flood over a 30x30 KM terrain.
It features a cloud front moving across a region of land characterized by a heightmap.
The input parameters include the grid size (finer grid size will increase the resolution and the level of details of the simulation), ground configuration (one of four presets), termination conditions and cloud configuration.

The algorithm works as follows:

1. Move all clouds across the grid based on their direction and velocity and add their rain to the water level.
2. The water is redistributed among neighbors. Given a cell we:
   1. Compute the potential water flow to adjacent cells in the grid.
   2. Distribute the water across the adjacent cells, proportionally to the relative differences of their total height (ground + water level).
3. Update the water array with the computed spillage.

The algorithm repeats steps 1-3 until one of the following conditions is satisfied:
- None of the grid cells have a water flow exceeding a threshold.
- A maximum number of iterations (minutes) is reached.

Step 2 lets the water flow the from higher regions to lower ones, gradually reducing height differences and tending towards equalizing levels. Out-of-bounds cells are treated as dry cells with the same height as their in-bounds neighbor.

**Note:** Further details can be found in the Assignment description provided as PDF.

We want you to implement this algorithm with Vitis HLS.
We suggest you not change this algorithm in your code. There might be floating point errors if you parallelize the algorithm in different ways, but the workflow should remain (mathematically) the same.



## Structure
The main FPGA implementation template is in the main folder, including a triplet of files for both the base and optimized implementations, along with some header files. 
The `test_files` folder contains a set of test files intended to verify the FPGA implementation, as described below.
The `cpu_impl` folder contains the CPU sequential reference implementation. This implementation can be compiled using `make flood` (from within the folder) and then tested using `sbatch job.sh` while within the folder.

In the top folder of this assignment you will find:
- `flood_HLS_base.cpp` is the HLS code for the base implementation. Initially, it is, to a large extent, a copy of the cpu implementation. The `do_compute` is assumed to be the top function.
- the testbench file `test_FLOOD_base.cpp`, which includes functionalities to generate clouds, initialize the scenario, and invoke the top function.
- the TCL script `run_FLOOD_HLS_base.tcl`, that contains the commands to setup a project, running C-Simulation, Synthesis and Co-Simulation.

There is an identical triplet of files for the optimized version of the code (second step of the assignment), whose names end with an `_optimized` suffix. By default, they have the same content of the `_base` files.

Finally, the `FLOOD.h` contains data type definitions and utility macros.

### Input

For the provided FPGA template, the input consists of a series of command line arguments. Most of them match the previous assignment, but there are some notable differences:
1. `<output_file>`: path to the file that will contain the output produced by the program execution.
2. `<ground scenario (M|V|D|d)>`: The heights of the terrain cells are stored in an array.
  The program includes four predefined terrain scenarios, which are selected using a single-character code:
  - `M`: Mountain lakes.
  - `V`: Valley.
  - `D`: Valley with a dam at slightly higher elevations.
  - `d`: Valley with a dam at lower elevations.
3. `<threshold>` The simulation stops when the highest amount of water discharged from one cell to another falls below this threshold.
4. `<num minutes>` The simulation stops after this number of minutes has been simulated.
5. `<exaggeration factor>` Multiplier for the rainfall discharge intensity.
  It allows for a less realistic but faster simulation.
  For example, a value of 60 means that in one minute, the amount of rainwater corresponding to one hour is discharged.
6. `<front distance>` Distance between the center of the simulation domain and the center of the cloud front.
7. `<front width>` Width of the cloud front.
8. `<front depth>` Depth of the cloud front.
  These two parameters (width and depth) define the dimensions of the rectangular region within which random clouds are generated.
  This region is rotated and translated based on the other parameters in order to correctly position the front, so that its direction leads the clouds into the domain.
9. `<front direction (degrees)>` Direction of the cloud front in degrees.
10. `<num random clouds>` Number of clouds generated in the front.
11. `<cloud max radius (km)>` Maximum radius of the clouds.
  For each cloud, a radius is generated randomly between this value and its half.
12. `<cloud max intensity (mm/h)>` Maximum rainfall intensity.
  For each cloud, an intensity is generated randomly between this value and its half.
  Rainfall intensity is considered:
  - `Normal`: up to 15 mm/h,
  - `Heavy`: 15–30 mm/h,
  - `Very Heavy`: 30–60 mm/h,
  - `Torrential`: above 60 mm/h.  Torrential rainfall exceeding 120–140 mm/h is rare but realistic.
13. `<cloud max speed (km/h)>` Maximum speed.
  Each cloud is assigned a speed randomly between this value and its half.
14. `<cloud max angle aperture (degrees)>` Maximum aperture angle.
  Each cloud’s movement direction is randomly selected within the range defined by the front direction plus or minus half of this angle.
15. `<clouds rnd seed>` Random seed used to reproduce the cloud generation in the front.

The differences with respect to the previous assignment concern the presence of an output file path and the absence of the number of rows and columns in the grid, which are instead defined as constants (via TCL script).


The `test_files` folder contains a few provided testing arguments. 
Each input file contains a sequence of command line arguments, which specify the various simulation parameters.
While input files can be tested for any grid size (as configured via TCL script), they include a fixed number of clouds (as seen in the file names).
For example, `test_files/tiny_mountains6c.in` is an input configuration that runs for 10 iterations (as defined inside the file) and contains 6 clouds (as also stated in the filename). The `test_files` folder under `cpu_impl` contains the corresponding input files (containing also the grid size) to be used with the cpu implementation.

For synthesis, the cloud value must match the value defined in the TCL script. The runtime, as reported by the synthesis tool or as experienced in RTL Co-simulation, depends on the grid size, the number of clouds, and the number of simulated minutes.

To run the program, you must execute the TCL script through the `vitis_hls` runtime:
```bash
vitis_hls -f run_FLOOD_HLS_base.tcl
```
Doing this properly (on a compute node) can, as always, be done with a simple slurm script:
```bash
$ sbatch job_base.sh
```

While you are allowed to generate your own input files for expanded experiments, we do expect to see a few reference configurations as described in the [Evaluation](#evaluation) section, which details information (e.g., default grid sizes, number of minutes) for the provided scenarios.

### Output 
At the end of execution, the program outputs a set of aggregated statistics that helps verify the correctness of the results to a file.

The name of this file corresponds with the first string in the input file, and, for C-simulation, it is stored at `FLOOD_HLS_base/solution_FLOOD_HLS_base/csim/build/tiny_mountains6c.out` for the `tiny_mountains6c` input file and base implementation.

The output file is structured similarly to the output of the last assignment, so it includes:
1. the total number of iterations executed
2. the iteration at which the highest amount of water was transferred between two cells
3. the maximum amount of water transferred in a single step
4. the highest water level reached in any cell
5. the total amount of rainwater discharged by the clouds
6. the total amount of water remaining on the ground at the end of the simulation
7. the amount of water lost through the boundaries of the terrain

### Verify Correctness

To verify correctness, the results of the parallel implementation are compared against those of the provided sequential reference implementation (ground truth):
- the first statistic must match exactly
- statistics [3-7] may exhibit small numerical differences due to floating-point rounding effects that can accumulate during the execution. For these values, a small relative error is tolerated.

The reported "Check precision loss" summarizes the deviation in the last 3 statistics, and is acceptable if those 3 statistics are within the described margin.

To automate correctness checking, you can use the provided `test_files/check_correctness.py` verification script. The scripts expects the path to two input files:
1. the output of the sequential (reference) program
2. the output of your FPGA implementation.

A typical workflow is the following
```bash
# Run the sequential version
$ cd cpu_impl
$ make flood
$ sbatch job.sh

# Run the FPGA version
$ sbatch job_base.sh

# Wait for the job completion ...

# Evaluate the correctness of the C-Simulation 
$ python3 test_files/check_correctness.py cpu_impl/tiny_mountains.out FLOOD_HLS_base/solution_FLOOD_HLS_base/csim/build/tiny_mountains.out

# Evaluate the correctness of the RTL Co-simulation 
$ python3 test_files/check_correctness.py cpu_impl/tiny_mountains.out FLOOD_HLS_base/solution_FLOOD_HLS_base/sim/wrapc_pc/tiny_mountains.out 
```

The script verifies that results are within the allowed tolerance and, if your implementation is correct, it will report:
```bash
Your output matches the reference.
```



## Evaluation
Below you can find some information on how we will evaluate your implementation, together with some suggestion on how to approach this assignment.


### Correctness of your implementation

We will check the correctness of your implementation with the following 4 scenarios.
We expect you to test (at least) the following configurations for your report:
1. tiny_mountains on a 40x40 grid, with 6 clouds, running for 10 minutes (iterations) 
2. tiny_dam on a 50x50 grid, with 7 clouds, running for 10 minutes (iterations)
3. small_mountains on a 60x80 grid, with 9 clouds, running for 100 minutes (iterations)
4. small_dam on a 90x90 grid, with 9 clouds, running for 120 minutes (iterations)

For all of them, we will check the result of the C-simulation, and, for the first two (the smaller ones), also the results of the Co-Simulation.
In both cases, we will use the approach described in the [Verify Correctness](#verify-correctness) section. 


### Performance
Provided that your code is correct (as defined above), the quality of your solution is evaluated by the latency, resource consumption, and synthesis frequency (Fmax) reported after the synthesis step (`csynth`). You can find this information in the report generated by the synthesis process (e.g., under `FLOOD_HLS_base/solution_FLOOD_HLS_base/syn/report`).

For the optimized version, we expect a good implementation to reduce latency by 5x in the tiny scenarios compared to the base, pipelined implementation.


### Suggestions on how to approach this assignment

- Inspect the content of the TCL script. By default, it executes the full flow: C Simulation, Synthesis, and RTL Co-Simulation.
- If you wish to change the input dataset or skip specific steps, you must **manually edit the TCL script accordingly**.
- We suggest you start by verifying the C Simulation results, as this step is faster and helps catch early functional errors before proceeding to RTL Co-Simulation.
- Synthesis is a stochastic process: its results may vary depending on the compute node used. To ensure reproducibility, always run synthesis on the same compute node and **mention it in your report**.
- We removed from this assignment the `DEBUG` prints, as these do not match well with the file organization imposed by Vitis HLS. If you want to print debug information directly from the FPGA code, you can do that in C-simulation. Vitis HLS defines the macro `__SYNTHESIS__` when synthesis is performed. This allows the
`__SYNTHESIS__` macro to exclude non-synthesizable code from the design. For instance, `printf` (if used) must be guarded by an `#ifndef __SYNTHESIS__` preprocessor directive.
- If you get a "so-far unseen" warnings from Vitis HLS (so, not about II violations, unsupported constructs, and loop-carried dependencies to mention some "seen" ones), in most of the cases these can be ignored as they might relate to possible stages of the hardware design that we will not touch. Make sure this is still nothing that prevents the correctness of your implementation. If in doubt, you can contact us via email or, better, via the support forum (so that others can benefit). We will do our best to get back to you as soon as possible. 
- By default, the RTL Co-simulation step prints a lot of debug information. For this reason, we suppress some of them by default directly in the provided slurm job script (`job_base.sh`). If you want to inspect the full output, remove the `grep` commands from the script, but be aware that this could create large slurm output files. Please consider deleting them if they are not useful to you, as this will impact your quota.
- Similarly, note that the folder containing the reports and simulation output (by default `FLOOD_HLS_base`), might grow to some hundreds of megabytes. Consider deleting it if not needed.