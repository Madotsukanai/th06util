#pragma once

#ifdef __GNUC__

#ifndef _WINSOCKAPI_
#include <winsock2.h>
#include <ws2tcpip.h>
#endif
#include <windows.h>

#ifndef C_ASSERT
#define C_ASSERT(expr) static_assert(expr, #expr)
#endif

#ifndef __forceinline
#define __forceinline __attribute__((always_inline)) inline
#endif

#ifndef __int64
#define __int64 long long
#endif
#ifndef __int32
#define __int32 int
#endif

#ifndef __in
#define __in
#endif
#ifndef __out
#define __out
#endif
#ifndef __inout
#define __inout
#endif

#endif
