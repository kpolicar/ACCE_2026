/*
 * NOTE: READ CAREFULLY
 * Here the function `do_compute` is just a copy of the CPU sequential version.
 * Implement your FPGA code using VITIS HLS. Check the README for further instructions.

 *
 * Simulation of rainwater flooding
 * FPGA version
 *
 * Adapted for ACCE at the VU, Period 5 2025-2026 from the original version by
 * Based on the EduHPC 2025: Peachy assignment, Computacion Paralela, Grado en Informatica (Universidad de Valladolid)
 * 2024/2025
 */

#include "FLOOD.h"

/*
 * Utils: Random generator
 */
#include "rng.h"

void do_compute(float ground[NROWS * NCOLS], Cloud_t clouds[NCLOUDS], float threshold, int num_minutes, float ex_factor,
                struct results *r) {

#pragma HLS INTERFACE m_axi port=ground bundle=gmem0

    float max_spillage_iter = threshold + 1;

    int water_level[NROWS * NCOLS];                              // Level of water on each cell (fixed precision)
    float local_ground[NROWS * NCOLS];
    float spillage_flag[NROWS * NCOLS];                          // Indicates which cells are spilling to neighbors
    float spillage_level[NROWS * NCOLS];                         // Maximum level of spillage of each cell
    float spillage_from_neigh[NROWS * NCOLS * CONTIGUOUS_CELLS]; // Spillage from each neighbor

#pragma HLS ARRAY_PARTITION variable=spillage_from_neigh cyclic factor=8 dim=1
#pragma HLS ARRAY_PARTITION variable=water_level cyclic factor=8 dim=1
#pragma HLS ARRAY_PARTITION variable=local_ground cyclic factor=8 dim=1
#pragma HLS ARRAY_PARTITION variable=spillage_flag cyclic factor=8 dim=1
#pragma HLS ARRAY_PARTITION variable=spillage_level cyclic factor=8 dim=1

    /* Ground generation and initialization of other structures */
    for (int i = 0; i < NROWS * NCOLS; i++) {
#pragma HLS PIPELINE II=1
        local_ground[i] = ground[i];
    }
    int row_pos, col_pos, depth_pos;
    for (row_pos = 0; row_pos < NROWS; row_pos++) {
        for (col_pos = 0; col_pos < NCOLS; col_pos++) {
#pragma HLS PIPELINE II=1
#pragma HLS UNROLL factor=2
            accessMat(water_level, row_pos, col_pos) = 0;
            accessMat(spillage_flag, row_pos, col_pos) = 0.0;
            accessMat(spillage_level, row_pos, col_pos) = 0.0;
            int depths = CONTIGUOUS_CELLS;
            for (depth_pos = 0; depth_pos < depths; depth_pos++) {
#pragma HLS UNROLL
                accessMat3D(spillage_from_neigh, row_pos, col_pos, depth_pos) = 0.0;
            }
        }
    }

    /* Flood simulation (time iterations) */
    for (r->minute = 0; r->minute < num_minutes && max_spillage_iter > threshold; r->minute++) {
#pragma HLS loop_tripcount min=1 max=120 avg=120

        int new_row, new_col;
        int cell_pos;

        /* Step 1: Clouds movement */
        const float inv60 = 1.0f / 60.0f;
        for (int cloud = 0; cloud < NCLOUDS; cloud++) {
#pragma HLS UNROLL
            // Calculate new position (x are rows, y are columns)
            clouds[cloud].x += clouds[cloud].dx * inv60;
            clouds[cloud].y += clouds[cloud].dy * inv60;
        }

        /* Rainfall — map-reduce: each cloud writes to its own slice (no conflicts) */
        const float inv_60000 = 1.0f / 60000.0f;
        
        long rain_total_per_cloud[NCLOUDS];
#pragma HLS ARRAY_PARTITION variable=rain_total_per_cloud complete dim=1
        for (int cloud = 0; cloud < NCLOUDS; cloud++) {
#pragma HLS UNROLL
            rain_total_per_cloud[cloud] = 0;
        }

        // One pass over the grid, checking distance to all clouds in parallel
        for (int rr = 0; rr < NROWS; rr++) {
            for (int cc = 0; cc < NCOLS; cc++) {
#pragma HLS PIPELINE II=1
#pragma HLS UNROLL factor=2
                int cell_sum = 0;
                for (int cloud = 0; cloud < NCLOUDS; cloud++) {
#pragma HLS UNROLL
                    const float c_x = clouds[cloud].x;
                    const float c_y = clouds[cloud].y;
                    const float c_r = clouds[cloud].radius;
                    const float c_int = clouds[cloud].intensity;
                    const float inv_r = 1.0f / c_r;
                    const float sqrt_int = sqrt(c_int);

                    float row_start = COORD_SCEN2MAT_Y(MAX(0, c_y - c_r));
                    float row_end = COORD_SCEN2MAT_Y(MIN(c_y + c_r, SCENARIO_SIZE));
                    float col_start = COORD_SCEN2MAT_X(MAX(0, c_x - c_r));
                    float col_end = COORD_SCEN2MAT_X(MIN(c_x + c_r, SCENARIO_SIZE));

                    const float frac_row = row_start - (int)row_start;
                    const float frac_col = col_start - (int)col_start;

                    float row_logical = (float)rr + frac_row;
                    float col_logical = (float)cc + frac_col;
                    
                    if (row_logical >= row_start && row_logical < row_end &&
                        col_logical >= col_start && col_logical < col_end) {
                        float x_pos = COORD_MAT2SCEN_X(col_logical);
                        float y_pos = COORD_MAT2SCEN_Y(row_logical);
                        float sq_dist = SQR(x_pos - c_x) + SQR(y_pos - c_y);
                        // Math optimization: Test squared distance to skip sqrt unit computations
                        if (sq_dist < c_r * c_r) {
                            float distance = sqrt(sq_dist);
                            float rain = ex_factor * MAX(0, c_int - distance * inv_r * sqrt_int);
                            float meters_per_minute = rain * inv_60000;
                            int rain_fixed = FIXED(meters_per_minute);
                            cell_sum += rain_fixed;
                            rain_total_per_cloud[cloud] += rain_fixed;
                        }
                    }
                }
                accessMat(water_level, rr, cc) += cell_sum;
            }
        }
        for (int cl = 0; cl < NCLOUDS; cl++) {
#pragma HLS UNROLL
            r->total_rain += rain_total_per_cloud[cl];
        }

        /* Step 2: Compute water spillage to neighbor cells */
        for (row_pos = 0; row_pos < NROWS; row_pos++) {
            for (col_pos = 0; col_pos < NCOLS; col_pos++) {
#pragma HLS PIPELINE II=1
#pragma HLS UNROLL factor=2
                if (accessMat(water_level, row_pos, col_pos) > 0) {
                    float sum_diff = 0;
                    float my_spillage_level = 0;

                    /* Differences between current-cell level and its neighbours  */
                    float current_height =
                        accessMat(local_ground, row_pos, col_pos) + FLOATING(accessMat(water_level, row_pos, col_pos));

                    // Iterate over the four neighboring cells using the displacement array
                    for (cell_pos = 0; cell_pos < CONTIGUOUS_CELLS; cell_pos++) {
#pragma HLS UNROLL
                        new_row = row_pos + displacements[cell_pos][0];
                        new_col = col_pos + displacements[cell_pos][1];

                        float neighbor_height;

                        // Check if the new position is within the matrix boundaries
                        if (new_row < 0 || new_row >= NROWS || new_col < 0 || new_col >= NCOLS)
                            // Out of borders: Same height as the cell with no water
                            neighbor_height = accessMat(local_ground, row_pos, col_pos);
                        else
                            // Neighbor cell: Ground height + water level
                            neighbor_height = accessMat(local_ground, new_row, new_col) +
                                              FLOATING(accessMat(water_level, new_row, new_col));

                        // Compute level differences
                        if (current_height >= neighbor_height) {
                            float height_diff = current_height - neighbor_height;
                            sum_diff += height_diff;
                            my_spillage_level = MAX(my_spillage_level, height_diff);
                        }
                    }
                    my_spillage_level = MIN(FLOATING(accessMat(water_level, row_pos, col_pos)), my_spillage_level);

                    // Compute proportion of spillage to each neighbor
                    if (sum_diff > 0.0) {
                        float proportion = my_spillage_level / sum_diff;
                        // If proportion is significative, spillage
                        if (proportion > 1e-8) {
                            accessMat(spillage_flag, row_pos, col_pos) = 1;
                            accessMat(spillage_level, row_pos, col_pos) = my_spillage_level;

                            // Iterate over the four neighboring cells using the displacement array
                            for (cell_pos = 0; cell_pos < 4; cell_pos++) {
#pragma HLS UNROLL
                                new_row = row_pos + displacements[cell_pos][0];
                                new_col = col_pos + displacements[cell_pos][1];

                                float neighbor_height;

                                // Check if the new position is within the matrix boundaries
                                if (new_row < 0 || new_row >= NROWS || new_col < 0 || new_col >= NCOLS) {
                                    // Spillage out of the borders: Water loss
                                    neighbor_height = accessMat(local_ground, row_pos, col_pos);
                                    if (current_height >= neighbor_height) {
                                        r->total_water_loss +=
                                            FIXED(proportion * (current_height - neighbor_height) / 2);
                                    }
                                } else {
                                    // Spillage to a neighbor cell
                                    neighbor_height = accessMat(local_ground, new_row, new_col) +
                                                      FLOATING(accessMat(water_level, new_row, new_col));
                                    if (current_height >= neighbor_height) {
                                        int depths = CONTIGUOUS_CELLS;
                                        accessMat3D(spillage_from_neigh, new_row, new_col, cell_pos) =
                                            proportion * (current_height - neighbor_height);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        /* Step 3: Propagation of previously computer water spillage to/from neighbors */
        max_spillage_iter = 0.0;
        for (row_pos = 0; row_pos < NROWS; row_pos++) {
            for (col_pos = 0; col_pos < NCOLS; col_pos++) {
#pragma HLS PIPELINE II=1
#pragma HLS UNROLL factor=2
                int my_water_level = accessMat(water_level, row_pos, col_pos);
                // If the cell has spillage
                if (accessMat(spillage_flag, row_pos, col_pos) == 1) {

                    // Eliminate the spillage from the origin cell
                    my_water_level -=
                        FIXED(accessMat(spillage_level, row_pos, col_pos) / SPILLAGE_FACTOR);

                    // Compute termination condition: Maximum cell spillage during the iteration
                    if (accessMat(spillage_level, row_pos, col_pos) / SPILLAGE_FACTOR > max_spillage_iter) {
                        max_spillage_iter = accessMat(spillage_level, row_pos, col_pos) / SPILLAGE_FACTOR;
                    }
                    // Statistics: Record maximum cell spillage during the scenario and its time
                    if (accessMat(spillage_level, row_pos, col_pos) / SPILLAGE_FACTOR > r->max_spillage_scenario) {
                        r->max_spillage_scenario = accessMat(spillage_level, row_pos, col_pos) / SPILLAGE_FACTOR;
                        r->max_spillage_minute = r->minute;
                    }
                }

                // Accumulate spillage from neighbors AND reset for next iteration (fused)
                int local_spillage_sum = 0;
                for (cell_pos = 0; cell_pos < CONTIGUOUS_CELLS; cell_pos++) {
#pragma HLS UNROLL
                    int depths = CONTIGUOUS_CELLS;
                    local_spillage_sum += FIXED(accessMat3D(spillage_from_neigh, row_pos, col_pos, cell_pos) / SPILLAGE_FACTOR);
                    accessMat3D(spillage_from_neigh, row_pos, col_pos, cell_pos) = 0;
                }
                my_water_level += local_spillage_sum;
                accessMat(water_level, row_pos, col_pos) = my_water_level;
                accessMat(spillage_flag, row_pos, col_pos) = 0;
                accessMat(spillage_level, row_pos, col_pos) = 0;
            }
        }
    }

    /* 5. Statistics: Total remaining water and maximum amount of water in a cell */
    r->max_water_scenario = 0.0;
    for (row_pos = 0; row_pos < NROWS; row_pos++) {
        for (col_pos = 0; col_pos < NCOLS; col_pos++) {
#pragma HLS PIPELINE II=1
#pragma HLS UNROLL factor=2
            if (FLOATING(accessMat(water_level, row_pos, col_pos)) > r->max_water_scenario)
                r->max_water_scenario = FLOATING(accessMat(water_level, row_pos, col_pos));
            r->total_water += accessMat(water_level, row_pos, col_pos);
        }
    }

    return;
}
