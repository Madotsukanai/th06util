#include <cstdarg>
#include <stdio.h>

#include <windows.h>

#include "ZunMath.hpp"
#include "i18n.hpp"
#include "utils.hpp"

namespace th06
{
HANDLE g_ExclusiveMutex;
namespace utils
{
ZunResult CheckForRunningGameInstance(void)
{

    return ZUN_SUCCESS;
}

void DebugPrint(const char *fmt, ...)
{
#ifdef DEBUG
    char tmpBuffer[512];
    std::va_list args;

    va_start(args, fmt);
    vsprintf(tmpBuffer, fmt, args);
    va_end(args);

    printf("DEBUG2: %s\n", tmpBuffer);
#endif
}

f32 AddNormalizeAngle(f32 a, f32 b)
{
    i32 i;

    i = 0;
    a += b;
    while (a > ZUN_PI)
    {
        a -= ZUN_2PI;
        if (i++ > 16)
            break;
    }
    while (a < -ZUN_PI)
    {
        a += ZUN_2PI;
        if (i++ > 16)
            break;
    }
    return a;
}

void Rotate(D3DXVECTOR3 *outVector, D3DXVECTOR3 *point, f32 angle)
{
    f32 sinOut;
    f32 cosOut;

    sinOut = sinf(angle);
    cosOut = cosf(angle);
    outVector->x = cosOut * point->x + sinOut * point->y;
    outVector->y = cosOut * point->y - sinOut * point->x;
}

void DebugPrint2(const char *fmt, ...)
{
#ifdef DEBUG
    char tmpBuffer[512];
    std::va_list args;

    va_start(args, fmt);
    vsprintf(tmpBuffer, fmt, args);
    va_end(args);

    printf("DEBUG2: %s\n", tmpBuffer);
#endif
}

void LogDebug(const char *fmt, ...)
{
    char tmpBuffer[512];
    std::va_list args;

    va_start(args, fmt);
    vsprintf(tmpBuffer, fmt, args);
    va_end(args);

    FILE *f = fopen("log_spell_practice.txt", "a+");
    if (f != NULL)
    {
        fprintf(f, "%s\n", tmpBuffer);
        fclose(f);
    }
}

void DbgLog(const char *fmt, ...)
{
    char tmpBuffer[512];
    std::va_list args;

    va_start(args, fmt);
    vsprintf(tmpBuffer, fmt, args);
    va_end(args);

    FILE *f = fopen("vkd_debug.log", "a");
    if (f != NULL)
    {
        fputc('\n', f);
        fprintf(f, "%s", tmpBuffer);
        fputc('\n', f);
        fclose(f);
    }
}
}; // namespace utils
}; // namespace th06
