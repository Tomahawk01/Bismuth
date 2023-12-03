#pragma once

#include "defines.h"

typedef struct bclock
{
    f64 start_time;
    f64 elapsed;
} bclock;

// Updates the provided clock. Should be called just before checking elapsed time
// Has no effect on non-started clocks
BAPI void bclock_update(bclock* clock);

// Starts the provided clock. Resets elapsed time
BAPI void bclock_start(bclock* clock);

// Stops the provided clock. Does not reset elapsed time
BAPI void bclock_stop(bclock* clock);
