// MinGW shim: pre-include <cmath> before the DX8 SDK math header to
// prevent the ancient DX8 d3dx8.h from pulling in <math.h> in a context
// where GCC 16 generates template-body diagnostics in stl_pair.h.
#ifndef _D3DX8MATH_H_SHIM_GUARD
#define _D3DX8MATH_H_SHIM_GUARD
// Force cmath (and its transitive deps) in before anything DX8 touches
// <math.h>.  This avoids the "stl_pair.h: move() not found" error from
// GCC 16 that occurs when <cmath> is first included in a template context.
#include <cmath>
#pragma GCC system_header
#include_next <d3dx8math.h>
#endif
