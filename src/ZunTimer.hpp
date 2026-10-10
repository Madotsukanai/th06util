#pragma once

#include "Supervisor.hpp"
#include "inttypes.hpp"
#include <Windows.h>

namespace th06
{
struct ZunTimer
{
    i32 previous;
    f32 subFrame;
    i32 current;

    ZunTimer()
    {
        this->Initialize();
    }

    ZunBool operator==(i32 time)
    {
        return this->current == time;
    }

    ZunBool operator>=(i32 time)
    {
        return this->current >= time;
    }

    ZunBool operator>(i32 time)
    {
        return this->current > time;
    }

    ZunBool operator<(i32 time)
    {
        return this->current < time;
    }

    ZunBool operator<=(i32 time)
    {
        return this->current <= time;
    }

    void Initialize();
    void Increment(i32 value);
    void Decrement(i32 value);
    i32 NextTick();

    void IncrementInline(i32 value)
    {
        this->Increment(value);
    }

    void InitializeForPopup()
    {
        this->current = 0;
        this->subFrame = 0;
        this->previous = -999;
    }

    void SetCurrent(i32 value)
    {
        this->current = value;
        this->subFrame = 0;
        this->previous = -999;
    }

    void Tick()
    {
        this->previous = this->current;
        g_Supervisor.TickTimer(&this->current, &this->subFrame);
    }

    i32 operator%(i32 divisor)
    {
        return this->current % divisor;
    }

    void operator+=(i32 value)
    {
        this->Increment(value);
    }
    void operator-=(i32 value)
    {
        this->Decrement(value);
    }
    void operator++(int)
    {
        this->Tick();
    }
    void operator--(int)
    {
        this->Decrement(1);
    }
    void operator=(i32 value)
    {
        SetCurrent(value);
    }

    operator f32()
    {
        return this->current + this->subFrame;
    }

    operator i32()
    {
        return this->current;
    }

    f32 AsFramesFloat()
    {
        return this->current + this->subFrame;
    }

    i32 AsFrames()
    {
        return this->current;
    }

    ZunBool HasTicked()
    {
        return this->current != this->previous;
    }
};
ZUN_ASSERT_SIZE(ZunTimer, 0xc);
}; // namespace th06
