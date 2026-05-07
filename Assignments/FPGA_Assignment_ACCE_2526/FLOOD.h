#ifndef FLOODH
#define FLOODH
// #pragma once

#include <math.h>
#include <sys/time.h>

/*
 * Water levels are stored with fixed preciseon
 * This avoids result differences when arithmetic operations are reordered
 */
#define PRECISION 1000000
#define FIXED(a) ((int)((a) * PRECISION))
#define FLOATING(a) ((float)(a) / PRECISION)
#define PRECISION_FIXED 1
#define PRECISION_FLOAT 2

/*
 * Scenario size (km x km)
 * and its specification
 */
#define SCENARIO_SIZE 30

/*
 * Run configuration. These values are set in the TCL scripts.
 * Change them to test different scenarios and configurations.
 */
#ifndef NROWS
#error "NROWS must be defined"
#endif
#ifndef NCOLS
#error "NCOLS must be defined"
#endif
#ifndef NCLOUDS
#error "NCLOUDS must be defined"
#endif

/*
 * Spillage factor for equilibrium
 */
#define SPILLAGE_FACTOR 2

/*
 * Utils: Number of contiguous cells to consider for water spillage
 * 	0: up, 1: down, 2: left, 3: right
 * 	Displacements for the contiguous cells
 * 	This data structure can be changed and/or optimized by the students
 */
#define CONTIGUOUS_CELLS 4
static int displacements[CONTIGUOUS_CELLS][2] = {
    {-1, 0}, // Top
    {1, 0},  // Bottom
    {0, -1}, // Left
    {0, 1}   // Right
};

/*
 * Utils: Macro-functions to transform coordinates, from scenario to matrix cells, and back
 * 	These macro-functions can be changed and/or optimized by the students
 */
#define COORD_SCEN2MAT_X(x) (x * NCOLS / SCENARIO_SIZE)
#define COORD_SCEN2MAT_Y(y) (y * NROWS / SCENARIO_SIZE)
#define COORD_MAT2SCEN_X(c) (c * SCENARIO_SIZE / NCOLS)
#define COORD_MAT2SCEN_Y(r) (r * SCENARIO_SIZE / NROWS)

/*
 * Utils: Macro functions for the min and max of two numbers
 * 	These macro-functions can be changed and/or optimized by the students
 */
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define SQR(a) ((a) * (a))

/*
 * Utils: Macro function to simplify accessing data of 2D and 3D matrixes stored in a flattened array
 * 	These macro-functions can be changed and/or optimized by the students
 */
#define accessMat(arr, exp1, exp2) arr[(int)(exp1) * NCOLS + (int)(exp2)]
#define accessMat3D(arr, exp1, exp2, exp3) arr[((int)(exp1) * NCOLS * depths) + ((int)(exp2) * depths) + (int)(exp3)]

/*
 * Structure to represent moving rainy clouds
 * This structure can be changed and/or optimized by the students
 */
typedef struct {
    float x;         // x coordinate of the center
    float y;         // y coordinate of the center
    float radius;    // radius of the cloud (km)
    float intensity; // rainfall intensity (cm/h)
    float dx;        // x component of movement (km/h)
    float dy;        // y component of movement
    int active;      // active cloud
} Cloud_t;

struct parameters {
    float *ground;
    float threshold;
    int num_minutes;
    float ex_factor;
    Cloud_t *clouds;
};

struct results {
    int minute;
    float max_water_scenario;
    double max_spillage_scenario;
    int max_spillage_minute;
    double runtime;
    // Metrics to accumulate fixed point values
    long total_water;
    long total_water_loss;
    long total_rain;
};

void do_compute(float ground[NROWS * NCOLS], Cloud_t clouds[NCLOUDS], float threshold, int num_minutes, float ex_factor,
                struct results *r);

#endif