#!/bin/bash
#SBATCH --job-name=flood_anim
#SBATCH --output=flood_anim_%j.out
#SBATCH --error=flood_anim_%j.err
#SBATCH --time=00:15:00
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --partition=defq
#SBATCH --constraint="gpunode,TitanX"
#SBATCH --gres=gpu:1

cd /home/ppp25016/ACCE_2026/Assignments/Assignment1
./flood_seq $(< test_files/debug.in) > animation_data.txt
