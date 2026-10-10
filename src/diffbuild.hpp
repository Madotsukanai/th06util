#pragma once

#include "mingw_compat.hpp"

#if defined(BINARYMATCHBUILD) || defined(DIFFBUILD) || defined(DLLBUILD)
#define ZUN_ASSERT_SIZE(type, size) C_ASSERT(sizeof(type) == size);
#else
#define ZUN_ASSERT_SIZE(type, size) C_ASSERT(true);
#endif
