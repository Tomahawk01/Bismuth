#include <core/logger.h>
#include <core/asserts.h>

// TODO: Test
#include <platform/platform.h>

int main(void)
{
    BFATAL("Test message: %f", 3.14f);
    BERROR("Test message: %f", 3.14f);
    BWARN("Test message: %f", 3.14f);
    BINFO("Test message: %f", 3.14f);
    BDEBUG("Test message: %f", 3.14f);
    BTRACE("Test message: %f", 3.14f);

    platform_state state;
    if (platform_startup(&state, "Bismuth Engine Testbed", 100, 100, 1280, 720))
    {
        while (TRUE)
        {
            platform_pump_messages(&state);
        }
    }
    platform_shutdown(&state);

    return 0;
}