#pragma once

#include "inttypes.hpp"

namespace th06
{
struct Supervisor;

namespace QuickSettings
{
bool IsOpen();
bool IsVisible();
bool IsWaitingKey();
bool Update(Supervisor *s);
void Draw();
} // namespace QuickSettings
} // namespace th06
