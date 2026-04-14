/*
 * NOTE: READ CAREFULLY
 * Here the function `do_compute` is just a copy of the CPU sequential version.
 * Implement your GPU code with CUDA here. Check the README for further instructions.
 * You can modify everything in this file, as long as we can compile the executable using
 * this source code, and Makefile.
 *
 * Simulation of rainwater flooding
 * CUDA version (Implement your parallel version here)
 * Version note: v3 uses per-cloud bbox rainfall kernels to reduce wasted work.
 *
 * Adapted for ACCE at the VU, Period 5 2025-2026 from the original version by
 * Based on the EduHPC 2025: Peachy assignment, Computacion Paralela, Grado en Informatica (Universidad de Valladolid)
 * 2024/2025
 */

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

/* Headers for the CUDA assignment versions */
#include <cuda.h>

/* Example of macros for error checking in CUDA */
#define CUDA_CHECK_FUNCTION(call)                                                                                      \
    {                                                                                                                  \
        cudaError_t check = call;                                                                                      \
        if (check != cudaSuccess)                                                                                      \
            fprintf(stderr, "CUDA Error in line: %d, %s\n", __LINE__, cudaGetErrorString(check));                      \
    }
#define CUDA_CHECK_KERNEL()                                                                                            \
    {                                                                                                                  \
        cudaError_t check = cudaGetLastError();                                                                        \
        if (check != cudaSuccess)                                                                                      \
            fprintf(stderr, "CUDA Kernel Error in line: %d, %s\n", __LINE__, cudaGetErrorString(check));               \
    }

/*
 * Utils: Random generator
 */
#include "rng.c"

/*
 * Header file: Contains constants and definitions
 */
#include "flood.h"

extern "C" double get_time();

__host__ __device__ static inline int idx2d(int row, int col, int columns) {
    return row * columns + col;
}

static inline float bits_to_float(unsigned int bits) {
    union {
        unsigned int u;
        float f;
    } cvt;
    cvt.u = bits;
    return cvt.f;
}

__global__ void compute_spillage_kernel(int rows, int columns, const float *ground, const int *water_level,
                                        float *spillage_flag, float *spillage_level, float *spillage_from_neigh,
                                        unsigned long long *total_water_loss) {
    int row = blockIdx.y * blockDim.y + threadIdx.y;
    int col = blockIdx.x * blockDim.x + threadIdx.x;

    if (row >= rows || col >= columns) {
        return;
    }

    int idx = idx2d(row, col, columns);
    spillage_flag[idx] = 0.0f;
    spillage_level[idx] = 0.0f;

    int water_i = water_level[idx];
    if (water_i <= 0) {
        return;
    }

    float water_f = (float)water_i / (float)PRECISION;
    float current_height = ground[idx] + water_f;

    const int dr[CONTIGUOUS_CELLS] = {-1, 1, 0, 0};
    const int dc[CONTIGUOUS_CELLS] = {0, 0, -1, 1};

    float diff_to_neigh[CONTIGUOUS_CELLS];
    float sum_diff = 0.0f;
    float my_spillage_level = 0.0f;

#pragma unroll
    for (int dir = 0; dir < CONTIGUOUS_CELLS; dir++) {
        int new_row = row + dr[dir];
        int new_col = col + dc[dir];

        float neighbor_height;
        if (new_row < 0 || new_row >= rows || new_col < 0 || new_col >= columns) {
            neighbor_height = ground[idx];
        } else {
            int nidx = idx2d(new_row, new_col, columns);
            neighbor_height = ground[nidx] + (float)water_level[nidx] / (float)PRECISION;
        }

        float height_diff = 0.0f;
        if (current_height >= neighbor_height) {
            height_diff = current_height - neighbor_height;
            sum_diff += height_diff;
            my_spillage_level = fmaxf(my_spillage_level, height_diff);
        }
        diff_to_neigh[dir] = height_diff;
    }

    my_spillage_level = fminf(water_f, my_spillage_level);

    if (sum_diff > 0.0f) {
        float proportion = my_spillage_level / sum_diff;
        if (proportion > 1e-8f) {
            spillage_flag[idx] = 1.0f;
            spillage_level[idx] = my_spillage_level;

#pragma unroll
            for (int dir = 0; dir < CONTIGUOUS_CELLS; dir++) {
                float flow = proportion * diff_to_neigh[dir];
                if (flow <= 0.0f) {
                    continue;
                }

                int new_row = row + dr[dir];
                int new_col = col + dc[dir];

                if (new_row < 0 || new_row >= rows || new_col < 0 || new_col >= columns) {
                    unsigned long long loss_fixed = (unsigned long long)((int)(flow * ((float)PRECISION / SPILLAGE_FACTOR)));
                    if (loss_fixed > 0ULL) {
                        atomicAdd(total_water_loss, loss_fixed);
                    }
                } else {
                    int nidx = idx2d(new_row, new_col, columns);
                    spillage_from_neigh[nidx * CONTIGUOUS_CELLS + dir] = flow;
                }
            }
        }
    }
}

__global__ void rainfall_per_cloud_kernel(int rows, int columns,
                                          float cx, float cy, float cradius, float cintensity,
                                          float row_start, float row_end,
                                          float col_start, float col_end,
                                          float ex_factor,
                                          int *water_level,
                                          unsigned long long *total_rain) {
    int j_row = blockIdx.y * blockDim.y + threadIdx.y;
    int j_col = blockIdx.x * blockDim.x + threadIdx.x;

    float row_pos = row_start + (float)j_row;
    float col_pos = col_start + (float)j_col;
    if (row_pos >= row_end || col_pos >= col_end) return;

    int r_i = (int)row_pos;
    int c_i = (int)col_pos;
    if (r_i < 0 || r_i >= rows || c_i < 0 || c_i >= columns) return;

    float x_pos = col_pos * (float)SCENARIO_SIZE / (float)columns;
    float y_pos = row_pos * (float)SCENARIO_SIZE / (float)rows;

    float dx = x_pos - cx;
    float dy = y_pos - cy;
    double distance = sqrt((double)(dx * dx + dy * dy));
    if (distance < (double)cradius) {
        double rain_d = (double)ex_factor *
            fmax(0.0, (double)cintensity - distance / (double)cradius * sqrt((double)cintensity));
        float rain = (float)rain_d;
        float meters_per_minute = rain / 1000.0f / 60.0f;
        int fixed_add = (int)(meters_per_minute * (float)PRECISION);
        if (fixed_add != 0) {
            water_level[r_i * columns + c_i] += fixed_add;
            atomicAdd(total_rain, (unsigned long long)fixed_add);
        }
    }
}

__global__ void apply_spillage_kernel(int rows, int columns, int *water_level, float *spillage_flag,
                                      float *spillage_level, float *spillage_from_neigh,
                                      unsigned int *max_spillage_bits) {
    int row = blockIdx.y * blockDim.y + threadIdx.y;
    int col = blockIdx.x * blockDim.x + threadIdx.x;

    if (row >= rows || col >= columns) {
        return;
    }

    int idx = idx2d(row, col, columns);
    int water = water_level[idx];

    if (spillage_flag[idx] == 1.0f) {
        float spill = spillage_level[idx] / SPILLAGE_FACTOR;
        water -= FIXED(spill);
        atomicMax(max_spillage_bits, __float_as_uint(spill));
    }

    int base = idx * CONTIGUOUS_CELLS;
#pragma unroll
    for (int dir = 0; dir < CONTIGUOUS_CELLS; dir++) {
        water += FIXED(spillage_from_neigh[base + dir] / SPILLAGE_FACTOR);
        spillage_from_neigh[base + dir] = 0.0f;
    }

    spillage_flag[idx] = 0.0f;
    spillage_level[idx] = 0.0f;
    water_level[idx] = water;
}

/*
 * Main compute function
 */
extern "C" void do_compute(struct parameters *p, struct results *r) {
    int rows = p->rows, columns = p->columns;
    int *minute = &r->minute;
    size_t cells = (size_t)rows * (size_t)columns;

    /* 2. Start global timer */
    CUDA_CHECK_FUNCTION(cudaSetDevice(0));
    CUDA_CHECK_FUNCTION(cudaDeviceSynchronize());

    /*
     *
     * Allocate memory and call kernels in this function.
     * Ensure all debug and animation code works in your final version.
     *
     */

    /* Memory allocation */

    int *water_level;           // Level of water on each cell (fixed precision)
    float *ground;              // Ground height
    float *spillage_flag;       // Indicates which cells are spilling to neighbors
    float *spillage_level;      // Maximum level of spillage of each cell
    float *spillage_from_neigh; // Spillage from each neighbor

    float *d_ground = NULL;
    int *d_water_level = NULL;
    float *d_spillage_flag = NULL;
    float *d_spillage_level = NULL;
    float *d_spillage_from_neigh = NULL;
    unsigned long long *d_total_water_loss = NULL;
    unsigned long long *d_total_rain = NULL;
    unsigned int *d_max_spillage_bits = NULL;

    ground = p->ground;
    water_level = (int *)malloc(sizeof(int) * (size_t)rows * (size_t)columns);
    spillage_flag = (float *)malloc(sizeof(float) * (size_t)rows * (size_t)columns);
    spillage_level = (float *)malloc(sizeof(float) * (size_t)rows * (size_t)columns);
    spillage_from_neigh = (float *)malloc(sizeof(float) * (size_t)rows * (size_t)columns * (size_t)CONTIGUOUS_CELLS);

    if (water_level == NULL || spillage_flag == NULL || spillage_level == NULL || spillage_from_neigh == NULL) {
        fprintf(stderr, "-- Error allocating ground and rain structures for size: %d x %d \n", rows, columns);
        exit(EXIT_FAILURE);
    }

    /* Ground generation and initialization of other structures */
    int row_pos, col_pos, depth_pos;
    for (row_pos = 0; row_pos < rows; row_pos++) {
        for (col_pos = 0; col_pos < columns; col_pos++) {
            accessMat(water_level, row_pos, col_pos) = 0;
            accessMat(spillage_flag, row_pos, col_pos) = 0.0;
            accessMat(spillage_level, row_pos, col_pos) = 0.0;
            int depths = CONTIGUOUS_CELLS;
            for (depth_pos = 0; depth_pos < depths; depth_pos++)
                accessMat3D(spillage_from_neigh, row_pos, col_pos, depth_pos) = 0.0;
        }
    }

    CUDA_CHECK_FUNCTION(cudaMalloc((void **)&d_ground, sizeof(float) * cells));
    CUDA_CHECK_FUNCTION(cudaMalloc((void **)&d_water_level, sizeof(int) * cells));
    CUDA_CHECK_FUNCTION(cudaMalloc((void **)&d_spillage_flag, sizeof(float) * cells));
    CUDA_CHECK_FUNCTION(cudaMalloc((void **)&d_spillage_level, sizeof(float) * cells));
    CUDA_CHECK_FUNCTION(cudaMalloc((void **)&d_spillage_from_neigh, sizeof(float) * cells * CONTIGUOUS_CELLS));
    CUDA_CHECK_FUNCTION(cudaMalloc((void **)&d_total_water_loss, sizeof(unsigned long long)));
    CUDA_CHECK_FUNCTION(cudaMalloc((void **)&d_total_rain, sizeof(unsigned long long)));
    CUDA_CHECK_FUNCTION(cudaMalloc((void **)&d_max_spillage_bits, sizeof(unsigned int)));

    CUDA_CHECK_FUNCTION(cudaMemcpy(d_ground, ground, sizeof(float) * cells, cudaMemcpyHostToDevice));
    CUDA_CHECK_FUNCTION(cudaMemset(d_water_level, 0, sizeof(int) * cells));
    CUDA_CHECK_FUNCTION(cudaMemset(d_spillage_flag, 0, sizeof(float) * cells));
    CUDA_CHECK_FUNCTION(cudaMemset(d_spillage_level, 0, sizeof(float) * cells));
    CUDA_CHECK_FUNCTION(cudaMemset(d_spillage_from_neigh, 0, sizeof(float) * cells * CONTIGUOUS_CELLS));
    CUDA_CHECK_FUNCTION(cudaMemset(d_total_water_loss, 0, sizeof(unsigned long long)));
    CUDA_CHECK_FUNCTION(cudaMemset(d_total_rain, 0, sizeof(unsigned long long)));

    dim3 block(16, 16);
    dim3 grid((columns + block.x - 1) / block.x, (rows + block.y - 1) / block.y);

#ifdef DEBUG
    print_matrix(PRECISION_FLOAT, rows, columns, ground, "Ground heights");
#ifndef ANIMATION
    print_clouds(p->num_clouds, p->clouds);
#endif
#endif

    double max_spillage_iter = DBL_MAX;

    /* Prepare to measure runtime */
    r->runtime = get_time();

    /* Flood simulation */
    for (*minute = 0; *minute < p->num_minutes && max_spillage_iter > p->threshold; (*minute)++) {

        /* Step 1.1: Clouds movement */
        for (int cloud = 0; cloud < p->num_clouds; cloud++) {
            // Calculate new position (x are rows, y are columns)
            Cloud_t *c_cloud = &p->clouds[cloud];
            float km_minute = c_cloud->speed / 60;
            c_cloud->x += km_minute * cos(c_cloud->angle * M_PI / 180.0);
            c_cloud->y += km_minute * sin(c_cloud->angle * M_PI / 180.0);
        }

#ifdef DEBUG
#ifndef ANIMATION
        print_clouds(p->num_clouds, p->clouds);
#endif
#endif

        /* Step 1.2: Rainfall (GPU) — one kernel per cloud, bbox-sized grid, matches seq's row_start+j iteration */
        for (int cloud = 0; cloud < p->num_clouds; cloud++) {
            Cloud_t c_cloud = p->clouds[cloud];
            float rs = COORD_SCEN2MAT_Y(MAX(0, c_cloud.y - c_cloud.radius));
            float re = COORD_SCEN2MAT_Y(MIN(c_cloud.y + c_cloud.radius, SCENARIO_SIZE));
            float cs = COORD_SCEN2MAT_X(MAX(0, c_cloud.x - c_cloud.radius));
            float ce = COORD_SCEN2MAT_X(MIN(c_cloud.x + c_cloud.radius, SCENARIO_SIZE));
            int bbox_rows = (int)ceilf(re - rs);
            int bbox_cols = (int)ceilf(ce - cs);
            if (bbox_rows <= 0 || bbox_cols <= 0) continue;
            dim3 rain_block(16, 16);
            dim3 rain_grid((bbox_cols + rain_block.x - 1) / rain_block.x,
                           (bbox_rows + rain_block.y - 1) / rain_block.y);
            rainfall_per_cloud_kernel<<<rain_grid, rain_block>>>(
                rows, columns, c_cloud.x, c_cloud.y, c_cloud.radius, c_cloud.intensity,
                rs, re, cs, ce, p->ex_factor, d_water_level, d_total_rain);
            CUDA_CHECK_KERNEL();
        }

#ifdef DEBUG
        CUDA_CHECK_FUNCTION(cudaMemcpy(water_level, d_water_level, sizeof(int) * cells, cudaMemcpyDeviceToHost));
        print_matrix(PRECISION_FIXED, rows, columns, water_level, "Water after rain");
#endif

        /* Step 2 + Step 3 (GPU): Compute and apply spillage */
        CUDA_CHECK_FUNCTION(cudaMemset(d_max_spillage_bits, 0, sizeof(unsigned int)));

        compute_spillage_kernel<<<grid, block>>>(rows, columns, d_ground, d_water_level, d_spillage_flag,
                                                 d_spillage_level, d_spillage_from_neigh, d_total_water_loss);
        CUDA_CHECK_KERNEL();

        apply_spillage_kernel<<<grid, block>>>(rows, columns, d_water_level, d_spillage_flag, d_spillage_level,
                                               d_spillage_from_neigh, d_max_spillage_bits);
        CUDA_CHECK_KERNEL();
        CUDA_CHECK_FUNCTION(cudaDeviceSynchronize());

        unsigned int max_spillage_bits = 0;
        CUDA_CHECK_FUNCTION(
            cudaMemcpy(&max_spillage_bits, d_max_spillage_bits, sizeof(unsigned int), cudaMemcpyDeviceToHost));
        max_spillage_iter = bits_to_float(max_spillage_bits);

        if (max_spillage_iter > r->max_spillage_scenario) {
            r->max_spillage_scenario = max_spillage_iter;
            r->max_spillage_minute = *minute;
        }

#ifdef DEBUG
#ifndef ANIMATION
        CUDA_CHECK_FUNCTION(cudaMemcpy(water_level, d_water_level, sizeof(int) * cells, cudaMemcpyDeviceToHost));
        print_matrix(PRECISION_FIXED, rows, columns, water_level, "Water after spillage");
#endif
#endif

    }

    cudaDeviceSynchronize();

    CUDA_CHECK_FUNCTION(cudaMemcpy(water_level, d_water_level, sizeof(int) * cells, cudaMemcpyDeviceToHost));

    r->runtime = get_time() - r->runtime;

    if (p->final_matrix) {
        print_matrix(PRECISION_FIXED, rows, columns, water_level, "Water after spillage");
    }

    /* Statistics: Total remaining water and maximum amount of water in a cell */
    r->max_water_scenario = 0.0;
    for (row_pos = 0; row_pos < rows; row_pos++) {
        for (col_pos = 0; col_pos < columns; col_pos++) {
            if (FLOATING(accessMat(water_level, row_pos, col_pos)) > r->max_water_scenario)
                r->max_water_scenario = FLOATING(accessMat(water_level, row_pos, col_pos));
            r->total_water += accessMat(water_level, row_pos, col_pos);
        }
    }

    unsigned long long total_water_loss = 0;
    CUDA_CHECK_FUNCTION(cudaMemcpy(&total_water_loss, d_total_water_loss, sizeof(unsigned long long),
                                   cudaMemcpyDeviceToHost));
    r->total_water_loss = (long)total_water_loss;

    unsigned long long total_rain_fixed = 0;
    CUDA_CHECK_FUNCTION(cudaMemcpy(&total_rain_fixed, d_total_rain, sizeof(unsigned long long),
                                   cudaMemcpyDeviceToHost));
    r->total_rain = (long)total_rain_fixed;

    /* Free resources */
    CUDA_CHECK_FUNCTION(cudaFree(d_ground));
    CUDA_CHECK_FUNCTION(cudaFree(d_water_level));
    CUDA_CHECK_FUNCTION(cudaFree(d_spillage_flag));
    CUDA_CHECK_FUNCTION(cudaFree(d_spillage_level));
    CUDA_CHECK_FUNCTION(cudaFree(d_spillage_from_neigh));
    CUDA_CHECK_FUNCTION(cudaFree(d_total_water_loss));
    CUDA_CHECK_FUNCTION(cudaFree(d_total_rain));
    CUDA_CHECK_FUNCTION(cudaFree(d_max_spillage_bits));

    free(ground);
    free(water_level);
    free(spillage_flag);
    free(spillage_level);
    free(spillage_from_neigh);

    CUDA_CHECK_FUNCTION(cudaDeviceSynchronize());
}
