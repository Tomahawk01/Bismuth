#include <core/logger.h>
#include <core/asserts.h>

int main(void)
{
    BFATAL("Test message: %f", 3.14f);
    BERROR("Test message: %f", 3.14f);
    BWARN("Test message: %f", 3.14f);
    BINFO("Test message: %f", 3.14f);
    BDEBUG("Test message: %f", 3.14f);
    BTRACE("Test message: %f", 3.14f);

    BASSERT(1 == 0);

    return 0;
}