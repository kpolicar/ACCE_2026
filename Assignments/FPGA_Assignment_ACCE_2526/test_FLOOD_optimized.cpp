#include <float.h>
#include <stdio.h>

#include "FLOOD.h"
#include "rng.h"
#include <cassert>

/*
 * Function: Generate ground height for a given position
 * 	This function can be changed and/or optimized by the students
 */
float get_height(char scenario, int row, int col, int rows, int columns) {
    // Choose scenario limits
    float x_min, x_max, y_min, y_max;
    if (scenario == 'M') { // Mountains scenario
        x_min = -3.3;
        x_max = 5.1;
        y_min = -0.5;
        y_max = 8.8;
    } else { // Valley scenarios
        x_min = -5.5;
        x_max = -3;
        y_min = -0.1;
        y_max = 4.2;
    }

    // Compute scenario coordinates of the cell position
    float x = x_min + ((x_max - x_min) / columns) * col;
    float y = y_min + ((y_max - y_min) / rows) * row;

    // Compute function height
    float height = -1 / (x * x + 1) + 2 / (y * y + 1) + 0.5 * sin(5 * sqrt(x * x + y * y)) / sqrt(x * x + y * y) +
                   (x + y) / 3 + sin(x) * cos(y) + 0.4 * sin(3 * x + y) + 0.25 * cos(4 * y + x);

// Substitute by the dam height in the proper scenarios
#define LOW_DAM_HEIGHT -1.0
#define HIGH_DAM_HEIGHT -0.4
    if (scenario == 'D' && x <= -4.96 && x >= -5.0) {
        if (height < HIGH_DAM_HEIGHT) {
            height = HIGH_DAM_HEIGHT;
        }
    } else if (scenario == 'd' && x <= -5.3 && x >= -5.34) {
        if (height < LOW_DAM_HEIGHT) {
            height = LOW_DAM_HEIGHT;
        }
    }

    // Transform to meters
    if (scenario == 'M')
        return height * 30 + 400;
    else
        return height * 20 + 100;
}

/*
 * Function: Initialize cloud with random values
 * 	This function can be changed and/or optimized by the students
 */
Cloud_t cloud_init(Cloud_t cloud_model, float front_distance, float front_width, float front_depth,
                   float front_direction, int rows, int cols, rng_t *rnd_state) {
    Cloud_t cloud;

    // Random position around the front center
    cloud.x = (float)rng_next_between(rnd_state, 0, front_width) - front_width / 2;
    cloud.y = (float)rng_next_between(rnd_state, 0, front_depth) - front_depth / 2;

    // Rotate
    float opposite = front_direction + 180;
    float tmp_x = cloud.x;
    float tmp_y = cloud.y;
    cloud.x = tmp_x * cos(opposite * M_PI / 180.0) - tmp_y * sin(opposite * M_PI / 180.0);
    cloud.y = tmp_x * sin(opposite * M_PI / 180.0) + tmp_y * cos(opposite * M_PI / 180.0);

    // Move center
    float x_center = front_distance * cos(opposite * M_PI / 180.0) + SCENARIO_SIZE / 2;
    float y_center = front_distance * sin(opposite * M_PI / 180.0) + SCENARIO_SIZE / 2;
    cloud.x += x_center;
    cloud.y += y_center;

    // Cloud random parameters
    cloud.radius = (float)rng_next_between(rnd_state, cloud_model.radius / 2, cloud_model.radius);
    cloud.intensity = (float)rng_next_between(rnd_state, cloud_model.intensity / 2, cloud_model.intensity);
    float speed = (float)rng_next_between(rnd_state, cloud_model.dx / 2, cloud_model.dx);
    float angle = front_direction + (float)rng_next_between(rnd_state, 0, cloud_model.dy) - cloud_model.dy / 2;

    cloud.dx = speed * cos(angle * M_PI / 180.0);
    cloud.dy = speed * sin(angle * M_PI / 180.0);
    cloud.active = 1;
    return cloud;
}

void writeResult(struct results *r, const char *filename) {
    FILE *fp = fopen(filename, "wt");

    if (fp != NULL) {
        /* Results: Statistics */
        fprintf(fp, "Result: %d, %d, %10.6lf, %10.6lf, %10.6lf, %10.6lf, %10.6f\n\n", r->minute, r->max_spillage_minute,
                r->max_spillage_scenario, r->max_water_scenario, FLOATING(r->total_rain), FLOATING(r->total_water),
                FLOATING(r->total_water_loss));
        fprintf(fp, "Check precision loss: %10.6f\n\n", FLOATING(r->total_rain - r->total_water - r->total_water_loss));
        fclose(fp);
    } else {
        fprintf(stderr, "Error writing file: %s.\n", filename); // No file found
        fflush(stderr);
        exit(-3);
    }
}

/*
 * Function: Print the program usage line in stderr
 */
void show_usage(char *program_name) {
    fprintf(stderr, "\nFlood Simulation - Simulate rain and flooding in %d x %d km^2\n", SCENARIO_SIZE, SCENARIO_SIZE);
    fprintf(stderr, "----------------------------------------------------------------\n");
    fprintf(stderr, "Usage: %s ", program_name);
    fprintf(
        stderr,
        "<output file> <ground_scenario(M|V|D|d)> <threshold> <num_minutes> <exaggeration_factor> <front_distance> "
        "<front_width> <front_depth> <front_direction(grad.)> <num_random_clouds> <cloud_max_radius(km)> "
        "<cloud_max_intensity(mm/h)> <cloud_max_speed(km/h)> <cloud_max_angle_aperture(grad.)> <clouds_rnd_seed>\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "\tOptional arguments for special clouds: <cloud_start_x(km)> <cloud_start_y(km)> "
                    "<cloud_radius(km)> <cloud_intensity(mm/h)> <cloud_speed(km/h)> <cloud_angle(grad.)> ...\n");
    fprintf(stderr, "\n");
    fprintf(stderr,
            "\tGround models: 'M' mountain lakes, 'V' valley, 'D' valley with high dam, 'd' valley with low dam\n");
    fprintf(stderr, "\tIntensity of rain (mm/h): Strong (15-30), Very strong (30-60), Torrential: Above 60\n");
    fprintf(stderr, "\n");
}

/*
 * MAIN PROGRAM
 */
int main(int argc, char *argv[]) {

#define NUM_FIXED_ARGS 16

    parameters p;

    /* 1. Read simulation arguments */
    /* 1.1. Check minimum number of arguments */
    if (argc < NUM_FIXED_ARGS) {
        fprintf(stderr, "-- Error: Not enough arguments when reading configuration from the command line\n\n");
        show_usage(argv[0]);
        exit(EXIT_FAILURE);
    }
    if (argc > NUM_FIXED_ARGS) {
        if ((argc - NUM_FIXED_ARGS) % 6 != 0) {
            fprintf(stderr,
                    "-- Error: Wrong number of arguments, there should be %d compulsory arguments + groups of 6 "
                    "optional arguments\n",
                    NUM_FIXED_ARGS);
            exit(EXIT_FAILURE);
        }
    }

    /* 1.2. Read ground sizes and selection of ground scenario */
    char ground_scenario = argv[2][0];
    if (ground_scenario != 'M' && ground_scenario != 'V' && ground_scenario != 'D' && ground_scenario != 'd') {
        fprintf(stderr, "-- Error: Wrong ground scenario\n\n");
        show_usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    /* 1.3. Read termination conditions */
    p.threshold = atof(argv[3]);
    p.num_minutes = atoi(argv[4]);

    /* 1.4. Read clouds data */
    p.ex_factor = atoi(argv[5]);
    float front_distance = atof(argv[6]);
    float front_width = atof(argv[7]);
    float front_depth = atof(argv[8]);
    float front_direction = atof(argv[9]);
    int arg_clouds = atoi(argv[10]);
    Cloud_t cloud_model;
    cloud_model.x = cloud_model.y = 0;
    cloud_model.radius = atof(argv[11]);
    cloud_model.intensity = atof(argv[12]);
    cloud_model.dx = atof(argv[13]); // speed
    cloud_model.dy = atof(argv[14]); // angle
    cloud_model.active = 0;
    unsigned int seed_clouds = (unsigned int)atol(argv[15]);
    // Initialize random sequence
    rng_t rnd_state = rng_new(seed_clouds);

    /* 1.5. Read the non-random clouds information */
    int num_clouds_arg = (argc - NUM_FIXED_ARGS) / 6;
    Cloud_t clouds_arg[num_clouds_arg];
    int idx;
    for (idx = NUM_FIXED_ARGS; idx < argc; idx += 6) {
        int pos = (idx - NUM_FIXED_ARGS) / 6;
        clouds_arg[pos].x = atof(argv[idx]);
        clouds_arg[pos].y = atof(argv[idx + 1]);
        clouds_arg[pos].radius = atof(argv[idx + 2]);
        clouds_arg[pos].intensity = atof(argv[idx + 3]);
        float speed = atof(argv[idx + 4]);
        float angle = atof(argv[idx + 5]);
        clouds_arg[pos].dx = speed * cos(angle * M_PI / 180.0);
        clouds_arg[pos].dy = speed * sin(angle * M_PI / 180.0);
    }

    /*
     *
     * START HERE: DO NOT CHANGE THE CODE ABOVE THIS POINT
     *
     */

    float *ground;   // Ground height
    Cloud_t *clouds; // Clouds

    /* Initialization */
    /* Memory allocation */
    ground = (float *)malloc(sizeof(float) * (size_t)NROWS * (size_t)NCOLS);
    clouds = (Cloud_t *)malloc(sizeof(Cloud_t) * (NCLOUDS));

    if (ground == NULL) {
        fprintf(stderr, "-- Error allocating ground and rain structures for size: %d x %d \n", NROWS, NCOLS);
        exit(EXIT_FAILURE);
    }
    if (clouds == NULL) {
        fprintf(stderr, "-- Error allocating clouds structures for size: %d\n", NCLOUDS);
        exit(EXIT_FAILURE);
    }

    /* Ground generation and initialization of other structures */
    int row_pos, col_pos, depth_pos;
    for (row_pos = 0; row_pos < NROWS; row_pos++) {
        for (col_pos = 0; col_pos < NCOLS; col_pos++) {
            int columns = NCOLS;
            accessMat(ground, row_pos, col_pos) = get_height(ground_scenario, row_pos, col_pos, NROWS, NCOLS);
        }
    }

    /* Clouds initialization */
    /* Random clouds generation */
    int cloud;
    for (cloud = 0; cloud < arg_clouds; cloud++) {
        clouds[cloud] = cloud_init(cloud_model, front_distance, front_width, front_depth, front_direction, NROWS, NCOLS,
                                   &rnd_state);
    }
    /* Copy optional argument clouds */
    for (cloud = 0; cloud < num_clouds_arg; cloud++)
        clouds[arg_clouds + cloud] = clouds_arg[cloud];
    arg_clouds += num_clouds_arg;
    assert(arg_clouds == NCLOUDS);

    // Set input parameters
    p.ground = ground;
    p.clouds = clouds;

    struct results r = {.minute = 0,
                        .max_water_scenario = 0.0,
                        .max_spillage_scenario = 0.0,
                        .max_spillage_minute = 0,
                        .total_water = 0,
                        .total_water_loss = 0,
                        .total_rain = 0};

    do_compute(&p, &r);

    /* Free resources */
    free(ground);
    free(clouds);

    /* Write results to file*/
    writeResult(&r, argv[1]);

    /* 9. End */
    return 0;
}
