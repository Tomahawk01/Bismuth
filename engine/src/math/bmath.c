#include "bmath.h"
#include "platform/platform.h"

#include <math.h>
#include <stdlib.h>

static b8 rand_seeded = FALSE;

f32 bsin(f32 x)
{
    return sinf(x);
}

f32 bcos(f32 x)
{
    return cosf(x);
}

f32 btan(f32 x)
{
    return tanf(x);
}

f32 bacos(f32 x)
{
    return acosf(x);
}

f32 bsqrt(f32 x)
{
    return sqrtf(x);
}

f32 babs(f32 x)
{
    return fabsf(x);
}

i32 brandom()
{
    if (!rand_seeded)
    {
        srand((u32)platform_get_absolute_time());
        rand_seeded = TRUE;
    }
    return rand();
}

i32 brandom_in_range(i32 min, i32 max)
{
    if (!rand_seeded)
    {
        srand((u32)platform_get_absolute_time());
        rand_seeded = TRUE;
    }
    return (rand() % (max - min + 1)) + min;
}

f32 fbrandom()
{
    return (float)brandom() / (f32)RAND_MAX;
}

f32 fbrandom_in_range(f32 min, f32 max)
{
    return min + ((float)brandom() / ((f32)RAND_MAX / (max - min)));
}
