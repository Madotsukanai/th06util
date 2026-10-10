

#include "AnmManager.hpp"
#include "AsciiManager.hpp"
#include "Chain.hpp"
#include "detours.h"
#include <fstream>
#include <string.h>

struct Detouring
{
    size_t addressInOriginalBinary;
    char *nameInDllReplacement;
    BOOL stub;

    void *addrToReplace;
    void *replaceWith;
};

#include "detouring.cpp"

#define D3D_WRAPPER 1
#ifdef D3D_WRAPPER
typedef IDirect3D8 *(WINAPI *Direct3DCreate8Proc)(UINT);
extern "C" IDirect3D8 *__stdcall Direct3DCreate8(UINT sdk_version)
{
    char path[MAX_PATH + 1];
    GetSystemDirectoryA(path, MAX_PATH);
    strncat(path, "\\d3d8.dll", MAX_PATH - strlen(path));
    HMODULE d3d8dll = LoadLibraryA(path);
    if (d3d8dll == NULL)
    {
        return NULL;
    }
    Direct3DCreate8Proc realproc = (Direct3DCreate8Proc)GetProcAddress(d3d8dll, "Direct3DCreate8");
    if (!realproc)
    {
        return NULL;
    }

    return realproc(sdk_version);
}
#endif

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD dwReason, LPVOID reserved)
{
    if (DetourIsHelperProcess())
    {
        return TRUE;
    }

    if (dwReason == DLL_PROCESS_ATTACH)
    {
        DetourRestoreAfterWith();

        if (DetourTransactionBegin() != 0)
        {

            return TRUE;
        }
        if (DetourUpdateThread(GetCurrentThread()) != 0)
        {

            DetourTransactionAbort();
            return TRUE;
        }
        for (size_t i = 0; i < sizeof(detours) / sizeof(detours[0]); i++)
        {
            if (!detours[i].stub)
            {
                detours[i].addrToReplace = (void *)detours[i].addressInOriginalBinary;
                detours[i].replaceWith = (void *)GetProcAddress(hinst, detours[i].nameInDllReplacement);
            }
            else
            {
                detours[i].addrToReplace = (void *)GetProcAddress(hinst, detours[i].nameInDllReplacement);
                detours[i].replaceWith = (void *)detours[i].addressInOriginalBinary;
            }
            if (DetourAttach(&detours[i].addrToReplace, detours[i].replaceWith) != 0)
            {

                DetourTransactionAbort();
                return TRUE;
            }
        }
        if (DetourTransactionCommit() != 0)
        {

            return TRUE;
        }
    }
    else if (dwReason == DLL_PROCESS_DETACH)
    {
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        for (size_t i = 0; i < sizeof(detours) / sizeof(detours[0]); i++)
        {
            DetourDetach(&detours[i].addrToReplace, detours[i].replaceWith);
        }
        DetourTransactionCommit();
    }
    return TRUE;
}
