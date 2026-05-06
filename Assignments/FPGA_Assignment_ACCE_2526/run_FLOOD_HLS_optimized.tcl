# Create a new Vitis HLS project.
# NOTE: this will reset the project if it already exists, 
# so make sure to save any important files before running this script
open_project -reset FLOOD_HLS_optimized
set_top do_compute

# Define preprocessor macros for the number of rows, columns, and clouds
# Note: these must be changed based on the considered scenario
# The default values can be used with the tiny_mountains6c scenario
set defs "-DNROWS=40 -DNCOLS=40 -DNCLOUDS=6" 

# Add files and testbed
add_files FLOOD.h -cflags $defs
add_files rng.cpp
add_files flood_HLS_optimized.cpp -cflags $defs
add_files -tb test_FLOOD_optimized.cpp -cflags $defs

# Read the input arguments from the file (change the path as needed)
set fp [open "test_files/tiny_mountains6c.in" r]
set arg_string [read $fp]
close $fp

# Create a solution
open_solution -reset "solution_FLOOD_HLS_optimized"

# Set the target FPGA part (modify as needed)
set_part {virtexuplusHBM}

# Set clock
create_clock -period 250MHz

# Run C simulation
csim_design -argv "$arg_string"

# Disable automatic pipelining (what does it change if you remove/comment this command?)
config_compile -pipeline_loops 0

# Run High-Level Synthesis (HLS)
csynth_design

# Run co-simulation (Attention, this might require a long time, you may want to comment it out for development purposes)
cosim_design -argv "$arg_string" -trace_level none -enable_binary_tv 

exit

