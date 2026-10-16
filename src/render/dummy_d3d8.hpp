#pragma once

#ifdef USE_VULKAN

#include <d3d8.h>

namespace th06 {

IDirect3D8* GetDummyD3D();
IDirect3DDevice8* GetDummyD3DDevice();

} // namespace th06

#endif // USE_VULKAN
