#!/bin/bash
#SBATCH --job-name=flood_cuda
#SBATCH --output=flood_output_%j.txt
#SBATCH --error=flood_error_%j.txt
#SBATCH --time=00:30:00
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --partition=defq
#SBATCH --constraint="gpunode,TitanX"
#SBATCH --gres=gpu:1

## This is an example of a SLURM job script to run the program on a GPU node
module load cuda12.6/toolkit

for input in debug small_mountains custom_clouds medium_lower_dam medium_higher_dam large_mountains; do
    echo "=== ${input} Sequential ==="
    ./flood_seq $(< test_files/${input}.in)
    echo ""
    echo "=== ${input} CUDA ==="
    nvprof ./flood_cuda $(< test_files/${input}.in)
    echo ""
done
