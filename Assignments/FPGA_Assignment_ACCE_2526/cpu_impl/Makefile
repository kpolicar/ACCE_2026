#
# Simulation of rainwater flooding
# 
#
# Parallel computing (Degree in Computer Engineering)
# 2024/2025
#
# (c) 2025 Arturo Gonzalez-Escribano
# Grupo Trasgo, Universidad de Valladolid (Spain)
#

# Compilers
CC=gcc
CUDACC=nvcc

# Flags for optimization and libs
FLAGS=-O3 -Wall
LIBS=-lm

# Targets to build
OBJS=flood

all: $(OBJS)

# Rules. By default make sequential
help:
	@echo
	@echo "Simulation of rainwater flooding"
	@echo
	@echo "Group Trasgo, Universidad de Valladolid (Spain)"
	@echo "Modified for ACCE 2026, Vrije Universiteit (Amsterdam)"
	@echo
	@echo "make	flood	Build the sequential program."
	@echo "make debug	Build with debug code enabled"
	@echo "make clean	Remove targets"
	@echo

flood: flood.c
	$(CC) $(FLAGS) $(DEBUG) $< $(LIBS) -o $@

# Remove the target files
clean:
	rm -rf $(OBJS)

# Compile in debug mode
debug:
	make FLAGS="$(FLAGS) -DDEBUG -g" flood

test: flood
	./flood $(< test_files/small_dam.in)
