#include "time/bclock.h"

#include "platform/platform.h"

void bclock_update(bclock* clock)
{
    if (clock->start_time != 0)
        clock->elapsed = platform_get_absolute_time() - clock->start_time;
}

void bclock_start(bclock* clock)
{
    clock->start_time = platform_get_absolute_time();
    clock->elapsed = 0;
}

void bclock_stop(bclock* clock)
{
    clock->start_time = 0;
}
