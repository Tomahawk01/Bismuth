/**
 * @brief Implementation of Mersenne Twister algorithm
 * Based on: https://github.com/ESultanik/mtwister
 */

#include "defines.h"

#define STATE_VECTOR_LENGTH 624
#define STATE_VECTOR_M 397  // NOTE: Changes to STATE_VECTOR_LENGTH also requires a change to this

typedef struct mtrand_state
{
    u64 mt[STATE_VECTOR_LENGTH];
    i32 index;
} mtrand_state;

/**
 * @brief Creates a new Mersenne Twister random number generator using the provided seed
 * @param seed Seed to use for the RNG
 * @returns State for the MT RNG
 */
BAPI mtrand_state mtrand_create(u64 seed);

/**
 * @brief Generates a new random 64-bit unsigned integer from given generator
 * @param generator Pointer to the random number generator to be used
 * @returns Newly generated 64-bit unsigned integer
 */
BAPI u64 mtrand_generate(mtrand_state* generator);

/**
 * @brief Generates a new random 64-bit floating-point number from given generator
 * @param generator Pointer to the random number generator to be used
 * @returns Newly generated 64-bit floating-point number
 */
BAPI f64 mtrand_generate_d(mtrand_state* generator);
