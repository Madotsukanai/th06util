#pragma once

#include <Windows.h>

#include "ZunResult.hpp"
#include "diffbuild.hpp"
#include "inttypes.hpp"

namespace th06
{
namespace FileSystem
{
u8 *OpenPath(const char *filepath, int isExternalResource);
int WriteDataToFile(char *path, void *data, size_t size);
} // namespace FileSystem
extern u32 g_LastFileSize;
}; // namespace th06
