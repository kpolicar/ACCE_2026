/*
 * Simple random generator
 * LCG (Linear Congruential Generator)
 *
 * Computacion Paralela, Grado en Informatica (Universidad de Valladolid)
 * 2024/2025
 *
 * v1.3
 *
 * (c) 2025, Arturo Gonzalez-Escribano
 */
#ifndef _RNG_H_
#define _RNG_H_

#include<stdint.h>
#include<math.h>

/*
 * Constants
 */
#define RNG_MULTIPLIER 6364136223846793005ULL
#define RNG_INCREMENT  1442695040888963407ULL

/*
 * Type for random sequences state
 */
typedef uint64_t	rng_t;

/*
 * Constructor: Create a new state from a seed
 */
rng_t rng_new(uint64_t seed);

/*
 * Next: Advance state and return a double number uniformely distributed
 * Adapted from the implementation on PCG (https://www.pcg-random.org/)
 */
double rng_next(rng_t *seq);

/*
 * Next: Advance state and return a double number uniformely distributed between limits
 */
double rng_next_between(rng_t *seq, double min, double max);

/*
 * Next Normal: Advance state and return a double number distributed with a normal(mu,sigma)
 */
double rng_next_normal( rng_t *seq, double mu, double sigma);

/*
 * Skip ahead: Advance state with an arbitrary jump in log time
 * Adapted from the implementation on PCG (https://www.pcg-random.org/)
 */
void rng_skip( rng_t *seq, uint64_t steps );

#endif
