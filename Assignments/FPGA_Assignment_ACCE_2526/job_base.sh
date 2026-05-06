#!/bin/sh
#SBATCH --time=00:45:00         
#SBATCH --nodes=1              

module load vivado/2024.1

# Run the HLS synthesis. We filter out specific warnings and trace information that 
# are output from the RTL-Cosimulation process, which are not relevant. If you need
# to see those messages, you can remove the grep filters.
vitis_hls -f run_FLOOD_HLS_base.tcl | grep -v "OPMODE Input Warning" | grep -v "Time: "

      
