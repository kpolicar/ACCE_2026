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

    double max_spillage_iter = threshold + 1;

    int water_level[NROWS * NCOLS];                              // Level of water on each cell (fixed precision)
    float spillage_flag[NROWS * NCOLS];                          // Indicates which cells are spilling to neighbors
    float spillage_level[NROWS * NCOLS];                         // Maximum level of spillage of each cell
    float spillage_from_neigh[NROWS * NCOLS * CONTIGUOUS_CELLS]; // Spillage from each neighbor

    /* Ground generation and initialization of other structures */
    int row_pos, col_pos, depth_pos;
    for (row_pos = 0; row_pos < NROWS; row_pos++) {
        for (col_pos = 0; col_pos < NCOLS; col_pos++) {
            accessMat(water_level, row_pos, col_pos) = 0;
            accessMat(spillage_flag, row_pos, col_pos) = 0.0;
            accessMat(spillage_level, row_pos, col_pos) = 0.0;
            int depths = CONTIGUOUS_CELLS;
            for (depth_pos = 0; depth_pos < depths; depth_pos++)
                accessMat3D(spillage_from_neigh, row_pos, col_pos, depth_pos) = 0.0;
        }
    }

    /* Flood simulation (time iterations) */
    for (r->minute = 0; r->minute < num_minutes && max_spillage_iter > threshold; r->minute++) {

        int new_row, new_col;
        int cell_pos;

        /* Step 1: Clouds movement */
        for (int cloud = 0; cloud < NCLOUDS; cloud++) {
            // Calculate new position (x are rows, y are columns)
            clouds[cloud].x += clouds[cloud].dx / 60;
            clouds[cloud].y += clouds[cloud].dy / 60;
        }

        /* Rainfall */
        for (int cloud = 0; cloud < NCLOUDS; cloud++) {
            // Compute the bounding box area of the cloud
            float row_start = COORD_SCEN2MAT_Y(MAX(0, clouds[cloud].y - clouds[cloud].radius));
            float row_end = COORD_SCEN2MAT_Y(MIN(clouds[cloud].y + clouds[cloud].radius, SCENARIO_SIZE));
            float col_start = COORD_SCEN2MAT_X(MAX(0, clouds[cloud].x - clouds[cloud].radius));
            float col_end = COORD_SCEN2MAT_X(MIN(clouds[cloud].x + clouds[cloud].radius, SCENARIO_SIZE));
            float distance;

            // Add rain to the ground water level
            float row_pos, col_pos;
            for (row_pos = row_start; row_pos < row_end; row_pos++) {
                for (col_pos = col_start; col_pos < col_end; col_pos++) {
                    float x_pos = COORD_MAT2SCEN_X(col_pos);
                    float y_pos = COORD_MAT2SCEN_Y(row_pos);
                    distance = sqrt(SQR(x_pos - clouds[cloud].x) + SQR(y_pos - clouds[cloud].y));
                    if (distance < clouds[cloud].radius) {
                        float rain = ex_factor * MAX(0, clouds[cloud].intensity -
                                                            distance / clouds[cloud].radius * sqrt(clouds[cloud].intensity));
                        float meters_per_minute = rain / 1000 / 60;
                        accessMat(water_level, row_pos, col_pos) += FIXED(meters_per_minute);
                        r->total_rain += FIXED(meters_per_minute);
                    }
                }
            }
        }

        /* Step 2: Compute water spillage to neighbor cells */
        for (row_pos = 0; row_pos < NROWS; row_pos++) {
            for (col_pos = 0; col_pos < NCOLS; col_pos++) {
                if (accessMat(water_level, row_pos, col_pos) > 0) {
                    float sum_diff = 0;
                    float my_spillage_level = 0;

                    /* Differences between current-cell level and its neighbours  */
                    float current_height =
                        accessMat(ground, row_pos, col_pos) + FLOATING(accessMat(water_level, row_pos, col_pos));

                    // Iterate over the four neighboring cells using the displacement array
                    for (cell_pos = 0; cell_pos < CONTIGUOUS_CELLS; cell_pos++) {
                        new_row = row_pos + displacements[cell_pos][0];
                        new_col = col_pos + displacements[cell_pos][1];

                        float neighbor_height;

                        // Check if the new position is within the matrix boundaries
                        if (new_row < 0 || new_row >= NROWS || new_col < 0 || new_col >= NCOLS)
                            // Out of borders: Same height as the cell with no water
                            neighbor_height = accessMat(ground, row_pos, col_pos);
                        else
                            // Neighbor cell: Ground height + water level
                            neighbor_height = accessMat(ground, new_row, new_col) +
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
                                new_row = row_pos + displacements[cell_pos][0];
                                new_col = col_pos + displacements[cell_pos][1];

                                float neighbor_height;

                                // Check if the new position is within the matrix boundaries
                                if (new_row < 0 || new_row >= NROWS || new_col < 0 || new_col >= NCOLS) {
                                    // Spillage out of the borders: Water loss
                                    neighbor_height = accessMat(ground, row_pos, col_pos);
                                    if (current_height >= neighbor_height) {
                                        r->total_water_loss +=
                                            FIXED(proportion * (current_height - neighbor_height) / 2);
                                    }
                                } else {
                                    // Spillage to a neighbor cell
                                    neighbor_height = accessMat(ground, new_row, new_col) +
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
                // If the cell has spillage
                if (accessMat(spillage_flag, row_pos, col_pos) == 1) {

                    // Eliminate the spillage from the origin cell
                    accessMat(water_level, row_pos, col_pos) -=
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

                // Accumulate spillage from neighbors
                for (cell_pos = 0; cell_pos < CONTIGUOUS_CELLS; cell_pos++) {
                    int depths = CONTIGUOUS_CELLS;
                    accessMat(water_level, row_pos, col_pos) +=
                        FIXED(accessMat3D(spillage_from_neigh, row_pos, col_pos, cell_pos) / SPILLAGE_FACTOR);
                }
            }
        }

        /* Reset ancillary structures */
        for (row_pos = 0; row_pos < NROWS; row_pos++) {
            for (col_pos = 0; col_pos < NCOLS; col_pos++) {
                for (cell_pos = 0; cell_pos < CONTIGUOUS_CELLS; cell_pos++) {
                    int depths = CONTIGUOUS_CELLS;
                    accessMat3D(spillage_from_neigh, row_pos, col_pos, cell_pos) = 0;
                }
                accessMat(spillage_flag, row_pos, col_pos) = 0;
                accessMat(spillage_level, row_pos, col_pos) = 0;
            }
        }
    }

    /* 5. Statistics: Total remaining water and maximum amount of water in a cell */
    r->max_water_scenario = 0.0;
    for (row_pos = 0; row_pos < NROWS; row_pos++) {
        for (col_pos = 0; col_pos < NCOLS; col_pos++) {
            if (FLOATING(accessMat(water_level, row_pos, col_pos)) > r->max_water_scenario)
                r->max_water_scenario = FLOATING(accessMat(water_level, row_pos, col_pos));
            r->total_water += accessMat(water_level, row_pos, col_pos);
        }
    }

    return;
}
