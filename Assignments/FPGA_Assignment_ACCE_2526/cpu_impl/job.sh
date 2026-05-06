#!/bin/sh
#SBATCH --time=00:15:00
#SBATCH --nodes=1

make flood

./flood $(< test_files/small_dam.in)

./flood $(< test_files/small_mountains.in)

./flood $(< test_files/tiny_dam.in)

./flood $(< test_files/tiny_mountains.in)
