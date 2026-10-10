#include "EnemyManager.hpp"
#include "AnmIdx.hpp"
#include "AnmManager.hpp"
#include "AsciiManager.hpp"
#include "BulletManager.hpp"
#include "Chain.hpp"
#include "ChainPriorities.hpp"
#include "EffectManager.hpp"
#include "GameManager.hpp"
#include "GameWindow.hpp"
#include "Gui.hpp"
#include "Player.hpp"
#include "ReplayManager.hpp"
#include "Rng.hpp"
#include "SpellPracticeDllApi.hpp"
#include "Supervisor.hpp"
#include "diffbuild.hpp"
#include "utils.hpp"
#include <math.h>
#include <stdio.h>
#include <windows.h>

namespace th06
{
using th06::g_SpellPracticeBossSubId;
using th06::g_SpellPracticeWarpTime;
using th06::g_ThpracDlg;
using th06::g_ThpracSection;

#define ECL_WARP(t)                                                                                                    \
    do                                                                                                                 \
    {                                                                                                                  \
        g_EnemyManager.timelineTime.current = (t);                                                                     \
    } while (0)
#define ECL_W32(off, v)                                                                                                \
    do                                                                                                                 \
    {                                                                                                                  \
        *(i32 *)((u8 *)base + (off)) = (i32)(v);                                                                       \
    } while (0)
#define ECL_W16(off, v)                                                                                                \
    do                                                                                                                 \
    {                                                                                                                  \
        *(i16 *)((u8 *)base + (off)) = (i16)(v);                                                                       \
    } while (0)
#define ECL_SETHEALTH(off, ecl_time, life)                                                                             \
    do                                                                                                                 \
    {                                                                                                                  \
        ECL_W32(off, ecl_time);                                                                                        \
        ECL_W32(off + 4, 0x0010006f);                                                                                  \
        ECL_W32(off + 8, 0x00ffff00);                                                                                  \
        ECL_W32(off + 12, life);                                                                                       \
    } while (0)
#define ECL_SETTIME(off, ecl_time, val)                                                                                \
    do                                                                                                                 \
    {                                                                                                                  \
        ECL_W32(off, ecl_time);                                                                                        \
        ECL_W32(off + 4, 0x00100073);                                                                                  \
        ECL_W32(off + 8, 0x00ffff00);                                                                                  \
        ECL_W32(off + 12, val);                                                                                        \
    } while (0)
#define ECL_STALL(off)                                                                                                 \
    do                                                                                                                 \
    {                                                                                                                  \
        ECL_W32(off, 0x99999);                                                                                         \
        ECL_W32(off + 4, 0x000c0000);                                                                                  \
        ECL_W32(off + 8, 0x0000ff00);                                                                                  \
    } while (0)

#define TH06_ST1_MID1 1
#define TH06_ST1_MID2 2
#define TH06_ST1_BOSS1 3
#define TH06_ST1_BOSS2 4
#define TH06_ST1_BOSS3 5
#define TH06_ST1_BOSS4 6
#define TH06_ST2_MID1 7
#define TH06_ST2_BOSS1 8
#define TH06_ST2_BOSS2 9
#define TH06_ST2_BOSS3 10
#define TH06_ST2_BOSS4 11
#define TH06_ST2_BOSS5 12
#define TH06_ST3_MID1 13
#define TH06_ST3_MID2 14
#define TH06_ST3_BOSS1 15
#define TH06_ST3_BOSS2 16
#define TH06_ST3_BOSS3 17
#define TH06_ST3_BOSS4 18
#define TH06_ST3_BOSS5 19
#define TH06_ST3_BOSS6 20
#define TH06_ST3_BOSS7 21
#define TH06_ST4_BOOKS 22
#define TH06_ST4_MID1 23
#define TH06_ST4_BOSS1 24
#define TH06_ST4_BOSS2 25
#define TH06_ST4_BOSS3 26
#define TH06_ST4_BOSS4 27
#define TH06_ST4_BOSS5 28
#define TH06_ST4_BOSS6 29
#define TH06_ST4_BOSS7 30
#define TH06_ST5_MID1 31
#define TH06_ST5_MID2 32
#define TH06_ST5_BOSS1 33
#define TH06_ST5_BOSS2 34
#define TH06_ST5_BOSS3 35
#define TH06_ST5_BOSS4 36
#define TH06_ST5_BOSS5 37
#define TH06_ST5_BOSS6 38
#define TH06_ST6_MID1 39
#define TH06_ST6_MID2 40
#define TH06_ST6_BOSS1 41
#define TH06_ST6_BOSS2 42
#define TH06_ST6_BOSS3 43
#define TH06_ST6_BOSS4 44
#define TH06_ST6_BOSS5 45
#define TH06_ST6_BOSS6 46
#define TH06_ST6_BOSS7 47
#define TH06_ST6_BOSS8 48
#define TH06_ST6_BOSS9 49
#define TH06_ST7_MID1 50
#define TH06_ST7_MID2 51
#define TH06_ST7_MID3 52
#define TH06_ST7_END_NS1 53
#define TH06_ST7_END_S1 54
#define TH06_ST7_END_NS2 55
#define TH06_ST7_END_S2 56
#define TH06_ST7_END_NS3 57
#define TH06_ST7_END_S3 58
#define TH06_ST7_END_NS4 59
#define TH06_ST7_END_S4 60
#define TH06_ST7_END_NS5 61
#define TH06_ST7_END_S5 62
#define TH06_ST7_END_NS6 63
#define TH06_ST7_END_S6 64
#define TH06_ST7_END_NS7 65
#define TH06_ST7_END_S7 66
#define TH06_ST7_END_NS8 67
#define TH06_ST7_END_S8 68
#define TH06_ST7_END_S9 69
#define TH06_ST7_END_S10 70

static void ApplyThpracEclPatch(i32 section, i32 dlg)
{
    void *base;
    i32 shot;

    if (g_EclManager.eclFile == NULL)
        return;
    base = g_EclManager.eclFile;

#define S2B_ND()                                                                                                       \
    ECL_WARP(0x1760);                                                                                                  \
    ECL_W32(0x18fc, 0x0);                                                                                              \
    ECL_W32(0x191c, 0x0);                                                                                              \
    ECL_W32(0x192c, 0x0);                                                                                              \
    ECL_W32(0x194c, 0x0);                                                                                              \
    ECL_W32(0x196c, 0x0);                                                                                              \
    ECL_W32(0x198c, 0x0);                                                                                              \
    ECL_W32(0x19a0, 0x0)

#define S3B_N1()                                                                                                       \
    ECL_W16(0x1274, 0x0);                                                                                              \
    ECL_W16(0x12f0, 0x0);                                                                                              \
    ECL_WARP(0x16d4);                                                                                                  \
    ECL_W16(0x80d6, 0x16d4);                                                                                           \
    ECL_W32(0x1f70, 0x1);                                                                                              \
    ECL_W32(0x1f90, 0x0);                                                                                              \
    ECL_W16(0x80dc, 0x24);                                                                                             \
    ECL_W32(0x20ec, 0x0);                                                                                              \
    ECL_W32(0x210c, 0x0);                                                                                              \
    ECL_W32(0x212c, 0x0);                                                                                              \
    ECL_W32(0x214c, 0x1e);                                                                                             \
    ECL_W32(0x2160, 0x1e);                                                                                             \
    ECL_W32(0x2194, 0x1e);                                                                                             \
    ECL_W32(0x2188, 150)

#define S4B_TIME()                                                                                                     \
    ECL_W32(0x2790, 0x0);                                                                                              \
    ECL_W32(0x27b0, 0x0);                                                                                              \
    ECL_W32(0x27d0, 0x0);                                                                                              \
    ECL_W32(0x27f0, 0x0);                                                                                              \
    ECL_W32(0x2810, 0x0);                                                                                              \
    ECL_W32(0x2824, 0x0);                                                                                              \
    ECL_W32(0x283c, 0x0);                                                                                              \
    ECL_W32(0x2850, 0x0)

#define S7B_CALL()                                                                                                     \
    ECL_W32(0x344e, 0x0);                                                                                              \
    ECL_W32(0x3452, 0x00180023);                                                                                       \
    ECL_W32(0x3456, 0x00ffff00);                                                                                       \
    ECL_W32(0x345a, 0x0);                                                                                              \
    ECL_W32(0x345e, 0x0);                                                                                              \
    ECL_W32(0x3462, 0x0);                                                                                              \
    ECL_STALL(0x3466)

#define S7B_N1()                                                                                                       \
    ECL_WARP(0x2192);                                                                                                  \
    ECL_W32(0x339e, 0x0);                                                                                              \
    ECL_W32(0x33ae, 0x0);                                                                                              \
    ECL_W32(0x33ce, 0x0);                                                                                              \
    ECL_W32(0x33ee, 0x0);                                                                                              \
    ECL_W32(0x340e, 0x0);                                                                                              \
    ECL_W32(0x342e, 0x0)

    switch (section)
    {
    case TH06_ST1_MID1:
        ECL_WARP(0x7d8);
        ECL_W32(0x0ab0, 0x3c);
        ECL_W32(0x0ad0, 0x3c);
        break;
    case TH06_ST1_MID2:
        ECL_WARP(0x7d8);
        ECL_W32(0x0ab0, 0x3c);
        ECL_W32(0x0ad0, 0x3c);
        ECL_SETHEALTH(0x0af0, 0x3c, 0x1f3);
        break;
    case TH06_ST1_BOSS1:
        if (dlg)
            ECL_WARP(0x149e);
        else
        {
            ECL_WARP(0x149f);
            ECL_W32(0x16a6, 0);
            ECL_W32(0x16c6, 0);
            ECL_W32(0x16e6, 0x50);
        }
        break;
    case TH06_ST1_BOSS2:
        ECL_WARP(0x149f);
        ECL_W32(0x16a6, 0);
        ECL_W32(0x16c6, 0);
        ECL_W32(0x16e6, 0x50);
        ECL_SETTIME(0x16e6, 0, 0);
        ECL_STALL(0x16f6);
        break;
    case TH06_ST1_BOSS3:
        ECL_WARP(0x149f);
        ECL_W32(0x16a6, 0);
        ECL_W32(0x16c6, 0);
        ECL_W32(0x16e6, 0x50);
        ECL_W32(0x16f2, 0x10);
        ECL_W32(0x293a, 0);
        ECL_W32(0x294a, 0);
        ECL_W16(0x291e, 0);
        break;
    case TH06_ST1_BOSS4:
        ECL_WARP(0x149f);
        ECL_W32(0x16a6, 0);
        ECL_W32(0x16c6, 0);
        ECL_W32(0x16e6, 0x50);
        ECL_W32(0x16f2, 0x10);
        ECL_W32(0x293a, 0);
        ECL_W32(0x294a, 0);
        ECL_W16(0x291e, 0);
        ECL_SETTIME(0x294a, 0, 0);
        ECL_STALL(0x295a);
        break;
    case TH06_ST2_MID1:
        ECL_WARP(0xa1c);
        break;
    case TH06_ST2_BOSS1:
        if (dlg)
            ECL_WARP(0x175f);
        else
            ECL_WARP(0x1760);
        break;
    case TH06_ST2_BOSS2:
        S2B_ND();
        ECL_SETTIME(0x19a0, 0x0, 0x0);
        ECL_STALL(0x19b0);
        break;
    case TH06_ST2_BOSS3:
        S2B_ND();
        ECL_W32(0x19ac, 0x19);
        ECL_W32(0x2138, 0x0);
        ECL_W32(0x2148, 0x60);
        ECL_W16(0x2110, 0x0);
        break;
    case TH06_ST2_BOSS4:
        S2B_ND();
        ECL_W32(0x19ac, 0x19);
        ECL_W32(0x2138, 0x0);
        ECL_W32(0x2148, 0x60);
        ECL_W16(0x2110, 0x0);
        ECL_SETTIME(0x2148, 0x30, 0x0);
        ECL_STALL(0x2158);
        break;
    case TH06_ST2_BOSS5:
        S2B_ND();
        ECL_W32(0x19ac, 0x19);
        ECL_W32(0x2138, 0x0);
        ECL_W32(0x2148, 0x60);
        ECL_W16(0x2110, 0x0);
        ECL_W32(0x2148, 0x0);
        ECL_W32(0x2154, 0x20);
        ECL_W16(0x33a2, 0x0);
        ECL_W16(0x337a, 0x0);
        ECL_W16(0x3392, 0x0);
        ECL_W32(0x2090, 0x578);
        ECL_W32(0x20b0, 0xffffffff);
        ECL_W32(0x20c0, 0xffffffff);
        ECL_W32(0x20f0, 0x1c);
        break;
    case TH06_ST3_MID1:
        ECL_WARP(0x0edc);
        break;
    case TH06_ST3_MID2:
        ECL_WARP(0x0edc);
        ECL_W16(0x1274, 0x0);
        ECL_W16(0x12f0, 0x0);
        ECL_W32(0x1018, 0x0);
        ECL_SETHEALTH(0x10dc, 0x1e, 0x513);
        ECL_STALL(0x10ec);
        break;
    case TH06_ST3_BOSS1:
        if (dlg)
            ECL_WARP(0x16d4);
        else
            S3B_N1();
        break;
    case TH06_ST3_BOSS2:
        S3B_N1();
        ECL_W32(0x214c, 0x0);
        ECL_SETTIME(0x2160, 0x0, 0x0);
        ECL_STALL(0x2170);
        break;
    case TH06_ST3_BOSS3:
        S3B_N1();
        ECL_W32(0x214c, 0x0);
        ECL_W32(0x2160, 0x0);
        ECL_W32(0x216c, 0x14);
        ECL_W16(0x25f4, 0x0);
        break;
    case TH06_ST3_BOSS4:
        S3B_N1();
        ECL_W32(0x214c, 0x0);
        ECL_W32(0x2160, 0x0);
        ECL_W32(0x216c, 0x14);
        ECL_W16(0x25f4, 0x0);
        ECL_SETTIME(0x267c, 0x0, 0x0);
        ECL_STALL(0x268c);
        break;
    case TH06_ST3_BOSS5:
        S3B_N1();
        ECL_W32(0x214c, 0x0);
        ECL_W32(0x2160, 0x0);
        ECL_W32(0x216c, 0x1a);
        ECL_W16(0x31d0, 0x0);
        break;
    case TH06_ST3_BOSS6:
        S3B_N1();
        ECL_W32(0x214c, 0x0);
        ECL_W32(0x2160, 0x0);
        ECL_W32(0x216c, 0x1a);
        ECL_W16(0x31d0, 0x0);
        ECL_SETTIME(0x3254, 0x0, 0x0);
        ECL_STALL(0x3264);
        break;
    case TH06_ST3_BOSS7:
        S3B_N1();
        ECL_W32(0x214c, 0x0);
        ECL_W32(0x2160, 0x0);
        ECL_W32(0x216c, 0x1a);
        ECL_W16(0x31d0, 0x0);
        ECL_SETTIME(0x3254, 0x0, 0x0);
        ECL_STALL(0x3264);
        ECL_W32(0x3168, 0x7d0);
        ECL_W32(0x31a8, 0x21);
        ECL_W32(0x31b8, 0x21);
        ECL_W16(0x4b64, 0x0);
        ECL_W16(0x4bec, 0x0);
        break;
    case TH06_ST4_BOOKS:
        ECL_WARP(0x0d40);
        break;
    case TH06_ST4_MID1:
        ECL_WARP(0x1024);
        break;
    case TH06_ST4_BOSS1:
        if (dlg)
            ECL_WARP(0x29c6);
        else
            ECL_WARP(0x29c7);
        break;
    case TH06_ST4_BOSS2:
        ECL_WARP(0x29c7);
        S4B_TIME();
        ECL_SETTIME(0x2810, 0x0, 0x0);
        ECL_STALL(0x2820);
        break;
    case TH06_ST4_BOSS3:
        ECL_WARP(0x29c7);
        S4B_TIME();
        ECL_W16(0x2854, 0x23);
        ECL_W32(0x285c, 0x25);
        ECL_W16(0x6da0, 0x0);
        ECL_SETTIME(0x7440, 0x0, 0x0);
        ECL_STALL(0x7450);
        break;
    case TH06_ST4_BOSS4:
        ECL_WARP(0x29c7);
        S4B_TIME();
        ECL_W16(0x2854, 0x23);
        ECL_W32(0x285c, 0x25);
        ECL_W16(0x6da0, 0x0);
        break;
    case TH06_ST4_BOSS5:
        ECL_WARP(0x29c7);
        S4B_TIME();
        ECL_W16(0x2854, 0x23);
        ECL_W32(0x285c, 0x27);
        ECL_W16(0x7950, 0x0);
        ECL_W32(0x7afc, 0x0);
        ECL_W32(0x7b0c, 0x0);
        ECL_W32(0x7b2c, 0x0);
        ECL_W32(0x7b4c, 0x0);
        ECL_W32(0x7b6c, 0x0);
        ECL_W32(0x7b8c, 0x0);
        break;
    case TH06_ST4_BOSS6:
        ECL_WARP(0x29c7);
        S4B_TIME();
        ECL_W16(0x2854, 0x23);
        ECL_W32(0x285c, 0x27);
        ECL_W16(0x7950, 0x0);
        ECL_SETHEALTH(0x7afc, 0, 1699);
        ECL_SETHEALTH(0x7b0c, 0, 3399);
        ECL_STALL(0x7b1c);
        ECL_W16(0x7b04, 0x0200);
        ECL_W16(0x7b14, 0x0c00);
        ECL_W16(0x7bc8, 0);
        ECL_W16(0x7cf4, 0);
        ECL_W16(0x7d0c, 0);
        ECL_W32(0x7d18, 0x0);
        ECL_W32(0x7d28, 0x0);
        ECL_W32(0x7d48, 0x0);
        ECL_W32(0x7d68, 0x0);
        ECL_W32(0x7d88, 0x0);
        ECL_W32(0x7da8, 0x0);
        break;
    case TH06_ST4_BOSS7:
        ECL_WARP(0x29c7);
        S4B_TIME();
        ECL_W16(0x2854, 0x23);
        ECL_W32(0x285c, 0x27);
        ECL_W16(0x7950, 0x0);
        ECL_SETHEALTH(0x7afc, 0, 1699);
        ECL_STALL(0x7b0c);
        ECL_W16(0x7a74, 41);
        ECL_W16(0x7a94, 41);
        ECL_W16(0x7a54, 1700);
        ECL_W16(0x7a64, 1700);
        ECL_W16(0x7de4, 0);
        ECL_W16(0x7ed0, 0);
        ECL_W16(0x7ee8, 0);
        ECL_W32(0x7ef4, 0x0);
        ECL_W32(0x7f04, 0x0);
        ECL_W32(0x7f24, 0x0);
        ECL_W32(0x7f44, 0x0);
        ECL_W32(0x7f64, 0x0);
        ECL_W32(0x7f84, 0x0);
        break;
    case TH06_ST5_MID1:
        ECL_WARP(0x0d2c);
        if (!dlg)
            ECL_W16(0x64a8, 13);
        break;
    case TH06_ST5_MID2:
        ECL_WARP(0x0d2c);
        ECL_W16(0x64a4, 0x0);
        ECL_SETHEALTH(0x14d8, 0x1e, 0x2c5);
        ECL_STALL(0x14e8);
        break;
    case TH06_ST5_BOSS1:
        ECL_WARP(0x1e18);
        if (!dlg)
        {
            ECL_W16(0x767c, 0x0);
            ECL_W32(0x22c8, 0x0);
            ECL_W32(0x22e8, 0x0);
            ECL_W32(0x2308, 0x0);
            ECL_W32(0x2328, 0x0);
            ECL_W32(0x2348, 0x0);
            ECL_W16(0x2218, 0x0);
        }
        break;
    case TH06_ST5_BOSS2:
        ECL_WARP(0x1e18);
        ECL_W16(0x767c, 0x0);
        ECL_W32(0x22c8, 0x0);
        ECL_W32(0x22e8, 0x0);
        ECL_W32(0x2308, 0x0);
        ECL_W32(0x2328, 0x0);
        ECL_W32(0x2348, 0x0);
        ECL_W16(0x2218, 0x0);
        ECL_SETTIME(0x2348, 0x0, 0x0);
        ECL_STALL(0x2358);
        break;
    case TH06_ST5_BOSS3:
        ECL_WARP(0x1e18);
        ECL_W16(0x767c, 0x0);
        ECL_W32(0x22c8, 0x0);
        ECL_W32(0x22e8, 0x0);
        ECL_W32(0x2308, 0x0);
        ECL_W32(0x2328, 0x0);
        ECL_W32(0x2348, 0x0);
        ECL_W16(0x2218, 0x0);
        ECL_W32(0x235c, 0x0);
        ECL_W32(0x2368, 0x22);
        ECL_W16(0x3778, 0x0);
        ECL_W16(0x3828, 0x0);
        break;
    case TH06_ST5_BOSS4:
        ECL_WARP(0x1e18);
        ECL_W16(0x767c, 0x0);
        ECL_W32(0x22c8, 0x0);
        ECL_W32(0x22e8, 0x0);
        ECL_W32(0x2308, 0x0);
        ECL_W32(0x2328, 0x0);
        ECL_W32(0x2348, 0x0);
        ECL_W16(0x2218, 0x0);
        ECL_W32(0x235c, 0x0);
        ECL_W32(0x2368, 0x22);
        ECL_W16(0x3778, 0x0);
        ECL_W16(0x3828, 0x0);
        ECL_SETTIME(0x38b8, 0x0, 0x0);
        ECL_STALL(0x38c8);
        break;
    case TH06_ST5_BOSS5:
        ECL_WARP(0x1e18);
        ECL_W16(0x767c, 0x0);
        ECL_W32(0x22c8, 0x0);
        ECL_W32(0x22e8, 0x0);
        ECL_W32(0x2308, 0x0);
        ECL_W32(0x2328, 0x0);
        ECL_W32(0x2348, 0x0);
        ECL_W16(0x2218, 0x0);
        ECL_W32(0x235c, 0x1e);
        ECL_W32(0x2368, 0x29);
        ECL_W16(0x4638, 0x0);
        ECL_W16(0x46e8, 0x0);
        break;
    case TH06_ST5_BOSS6:
        ECL_WARP(0x1e18);
        ECL_W16(0x767c, 0x0);
        ECL_W32(0x22c8, 0x0);
        ECL_W32(0x22e8, 0x0);
        ECL_W32(0x2308, 0x0);
        ECL_W32(0x2328, 0x0);
        ECL_W32(0x2348, 0x0);
        ECL_W16(0x2218, 0x0);
        ECL_W32(0x235c, 0x1e);
        ECL_W32(0x2368, 0x29);
        ECL_W16(0x4638, 0x0);
        ECL_W16(0x46e8, 0x0);
        ECL_W32(0x235c, 0x0);
        ECL_SETTIME(0x4758, 0x0, 0x0);
        ECL_STALL(0x4768);
        break;
    case TH06_ST6_MID1:
        ECL_WARP(0x0a04);
        if (!dlg)
        {
            ECL_W16(0x77f2, 0x0);
            ECL_W32(0x9e8, 0x1);
        }
        break;
    case TH06_ST6_MID2:
        shot = g_GameManager.character * 2 + g_GameManager.shotType;
        if (shot > 1)
            shot = 1099;
        else if (!shot)
            shot = 749;
        else
            shot = 999;
        ECL_WARP(0x0a04);
        ECL_W16(0x77f2, 0x0);
        ECL_W32(0x0d2c, 0x0);
        ECL_SETHEALTH(0x0d3c, 0x0, shot);
        ECL_STALL(0x0d4c);
        break;
    case TH06_ST6_BOSS1:
        if (dlg)
            ECL_WARP(0x0c5f);
        else
        {
            ECL_WARP(0x0c61);
            ECL_W32(0x1686, 0x0);
            ECL_W32(0x16a6, 0x0);
            ECL_W32(0x16c6, 0x0);
            ECL_W32(0x16e6, 0x0);
            ECL_W16(0x15d6, 0x0);
        }
        break;
    case TH06_ST6_BOSS2:
        ECL_WARP(0x0c61);
        ECL_W32(0x1686, 0x0);
        ECL_W32(0x16a6, 0x0);
        ECL_W32(0x16c6, 0x0);
        ECL_W32(0x16e6, 0x0);
        ECL_W16(0x15d6, 0x0);
        ECL_SETTIME(0x1706, 0x0, 0x0);
        ECL_STALL(0x1716);
        break;
    case TH06_ST6_BOSS3:
        ECL_WARP(0x0c61);
        ECL_W32(0x1686, 0x0);
        ECL_W32(0x16a6, 0x0);
        ECL_W32(0x16c6, 0x0);
        ECL_W32(0x16e6, 0x0);
        ECL_W16(0x15d6, 0x0);
        ECL_W32(0x1706, 0x0);
        ECL_W32(0x171a, 0x0);
        ECL_W32(0x1726, 0x13);
        ECL_W16(0x1b8e, 0x0);
        ECL_W16(0x1c3e, 0x0);
        break;
    case TH06_ST6_BOSS4:
        ECL_WARP(0x0c61);
        ECL_W32(0x1686, 0x0);
        ECL_W32(0x16a6, 0x0);
        ECL_W32(0x16c6, 0x0);
        ECL_W32(0x16e6, 0x0);
        ECL_W16(0x15d6, 0x0);
        ECL_W32(0x1706, 0x0);
        ECL_W32(0x171a, 0x0);
        ECL_W32(0x1726, 0x13);
        ECL_W16(0x1b8e, 0x0);
        ECL_W16(0x1c3e, 0x0);
        ECL_SETTIME(0x1cf2, 0x0, 0x0);
        ECL_STALL(0x1d02);
        break;
    case TH06_ST6_BOSS5:
        ECL_WARP(0x0c61);
        ECL_W32(0x1686, 0x0);
        ECL_W32(0x16a6, 0x0);
        ECL_W32(0x16c6, 0x0);
        ECL_W32(0x16e6, 0x0);
        ECL_W16(0x15d6, 0x0);
        ECL_W32(0x1706, 0x0);
        ECL_W32(0x171a, 0x1e);
        ECL_W32(0x1726, 0x17);
        ECL_W16(0x28e2, 0x0);
        ECL_W16(0x2992, 0x0);
        break;
    case TH06_ST6_BOSS6:
        ECL_WARP(0x0c61);
        ECL_W32(0x1686, 0x0);
        ECL_W32(0x16a6, 0x0);
        ECL_W32(0x16c6, 0x0);
        ECL_W32(0x16e6, 0x0);
        ECL_W16(0x15d6, 0x0);
        ECL_W32(0x1706, 0x0);
        ECL_W32(0x171a, 0x1e);
        ECL_W32(0x1726, 0x17);
        ECL_W16(0x28e2, 0x0);
        ECL_W16(0x2992, 0x0);
        ECL_W32(0x171a, 0x0);
        ECL_SETTIME(0x2a22, 0x0, 0x0);
        ECL_STALL(0x2a32);
        break;
    case TH06_ST6_BOSS7:
        ECL_WARP(0x0c61);
        ECL_W32(0x1686, 0x0);
        ECL_W32(0x16a6, 0x0);
        ECL_W32(0x16c6, 0x0);
        ECL_W32(0x16e6, 0x0);
        ECL_W16(0x15d6, 0x0);
        ECL_W32(0x1706, 0x0);
        ECL_W32(0x171a, 0x0);
        ECL_W32(0x1726, 0x1a);
        ECL_W16(0x2d8e, 0x0);
        ECL_W16(0x2e3e, 0x0);
        break;
    case TH06_ST6_BOSS8:
        ECL_WARP(0x0c61);
        ECL_W32(0x1686, 0x0);
        ECL_W32(0x16a6, 0x0);
        ECL_W32(0x16c6, 0x0);
        ECL_W32(0x16e6, 0x0);
        ECL_W16(0x15d6, 0x0);
        ECL_W32(0x1706, 0x0);
        ECL_W32(0x171a, 0x0);
        ECL_W32(0x1726, 0x1a);
        ECL_W16(0x2d8e, 0x0);
        ECL_W16(0x2e3e, 0x0);
        ECL_SETTIME(0x2ee6, 0x0, 0x0);
        ECL_STALL(0x2ef6);
        break;
    case TH06_ST6_BOSS9:
        ECL_WARP(0x0c61);
        ECL_W32(0x1686, 0x0);
        ECL_W32(0x16a6, 0x0);
        ECL_W32(0x16c6, 0x0);
        ECL_W32(0x16e6, 0x0);
        ECL_W16(0x15d6, 0x0);
        ECL_W32(0x1706, 0x0);
        ECL_W32(0x171a, 0x0);
        ECL_W32(0x1732, 0x0);
        ECL_W32(0x1726, 0x2b);
        ECL_W16(0x1722, 0x0300);
        ECL_W16(0x1736, 0x23);
        ECL_W16(0x173a, 0x0c00);
        ECL_W32(0x173e, 0x2c);
        ECL_W16(0x5c8e, 0x0);
        ECL_W16(0x6290, 0x0);
        ECL_W32(0x1622, 0xffffffff);
        break;
    case TH06_ST7_MID1:
        ECL_WARP(0x1284);
        if (!dlg)
            ECL_W16(0x0d2e2, 0x0);
        break;
    case TH06_ST7_MID2:
        ECL_WARP(0x1284);
        ECL_W16(0x0d2e2, 0x0);
        ECL_W32(0x1b14, 0x12);
        ECL_W16(0x1c2c, 0x0);
        break;
    case TH06_ST7_MID3:
        ECL_WARP(0x1284);
        ECL_W16(0x0d2e2, 0x0);
        ECL_W32(0x1b14, 0x13);
        ECL_W16(0x1d7c, 0x0);
        break;
    case TH06_ST7_END_NS1:
        if (dlg)
            ECL_WARP(0x2191);
        else
            S7B_N1();
        break;
    case TH06_ST7_END_S1:
        S7B_N1();
        ECL_SETTIME(0x344e, 0x0, 0x0);
        break;
    case TH06_ST7_END_NS2:
        S7B_N1();
        S7B_CALL();
        ECL_W32(0x345a, 0x23);
        ECL_W16(0x4210, 0x0);
        break;
    case TH06_ST7_END_S2:
        S7B_N1();
        S7B_CALL();
        ECL_W32(0x345a, 0x23);
        ECL_W16(0x4210, 0x0);
        ECL_W32(0x41fc, 0x0);
        ECL_W32(0x420c, 0x0);
        ECL_SETTIME(0x421c, 0x0, 0x0);
        ECL_STALL(0x422c);
        break;
    case TH06_ST7_END_NS3:
        S7B_N1();
        S7B_CALL();
        ECL_W32(0x345a, 0x26);
        ECL_W16(0x4c62, 0x0);
        break;
    case TH06_ST7_END_S3:
        S7B_N1();
        S7B_CALL();
        ECL_W32(0x345a, 0x26);
        ECL_W16(0x4c62, 0x0);
        ECL_W32(0x4c4e, 0x0);
        ECL_W32(0x4c5e, 0x0);
        ECL_SETTIME(0x4c6e, 0x0, 0x0);
        ECL_STALL(0x4c7e);
        break;
    case TH06_ST7_END_NS4:
        S7B_N1();
        S7B_CALL();
        ECL_W32(0x345a, 0x2b);
        ECL_W16(0x59cc, 0x0);
        break;
    case TH06_ST7_END_S4:
        S7B_N1();
        S7B_CALL();
        ECL_W32(0x345a, 0x2b);
        ECL_W16(0x59cc, 0x0);
        ECL_W32(0x59b8, 0x0);
        ECL_W32(0x59c8, 0x0);
        ECL_SETTIME(0x59d8, 0x0, 0x0);
        ECL_STALL(0x59e8);
        break;
    case TH06_ST7_END_NS5:
        S7B_N1();
        S7B_CALL();
        ECL_W32(0x345a, 0x2f);
        ECL_W16(0x63a2, 0x0);
        break;
    case TH06_ST7_END_S5:
        S7B_N1();
        S7B_CALL();
        ECL_W32(0x345a, 0x2f);
        ECL_W16(0x63a2, 0x0);
        ECL_W32(0x638e, 0x0);
        ECL_W32(0x639e, 0x0);
        ECL_SETTIME(0x63ae, 0x0, 0x0);
        ECL_STALL(0x63be);
        break;
    case TH06_ST7_END_NS6:
        S7B_N1();
        S7B_CALL();
        ECL_W32(0x345a, 0x31);
        ECL_W16(0x6b1c, 0x0);
        break;
    case TH06_ST7_END_S6:
        S7B_N1();
        S7B_CALL();
        ECL_W32(0x345a, 0x31);
        ECL_W16(0x6b1c, 0x0);
        ECL_W32(0x6b08, 0x0);
        ECL_W32(0x6b18, 0x0);
        ECL_SETTIME(0x6b28, 0x0, 0x0);
        ECL_STALL(0x6b38);
        break;
    case TH06_ST7_END_NS7:
        S7B_N1();
        S7B_CALL();
        ECL_W32(0x345a, 0x35);
        ECL_W16(0x78aa, 0x0);
        break;
    case TH06_ST7_END_S7:
        S7B_N1();
        S7B_CALL();
        ECL_W32(0x345a, 0x35);
        ECL_W16(0x78aa, 0x0);
        ECL_W32(0x7896, 0x0);
        ECL_W32(0x78a6, 0x0);
        ECL_SETTIME(0x78b6, 0x0, 0x0);
        ECL_STALL(0x78c6);
        break;
    case TH06_ST7_END_NS8:
        S7B_N1();
        S7B_CALL();
        ECL_W32(0x345a, 0x38);
        ECL_W16(0x8508, 0x0);
        break;
    case TH06_ST7_END_S8:
        S7B_N1();
        S7B_CALL();
        ECL_W32(0x345a, 0x38);
        ECL_W16(0x8508, 0x0);
        ECL_W32(0x84f4, 0x0);
        ECL_W32(0x8504, 0x0);
        ECL_SETTIME(0x8514, 0x0, 0x0);
        ECL_STALL(0x8524);
        break;
    case TH06_ST7_END_S9:
        S7B_N1();
        S7B_CALL();
        ECL_W32(0x345a, 0x3b);
        ECL_W16(0x940a, 0x0);
        ECL_W32(0x93f6, 0x0);
        ECL_W32(0x9406, 0x0);
        ECL_W32(0x9416, 0x0);
        ECL_W32(0x9422, 0x0);
        ECL_W32(0x943a, 0x0);
        ECL_W32(0x9466, 0x0);
        ECL_W32(0x9472, 0x0);
        ECL_W32(0x9482, 0x0);
        break;
    case TH06_ST7_END_S10:
        S7B_N1();
        S7B_CALL();
        ECL_W32(0x345a, 0x43);
        ECL_W16(0x0bea4, 0x0);
        ECL_W32(0x0be90, 0x0);
        ECL_W32(0x0bea0, 0x0);
        ECL_W32(0x0beb0, 0x0);
        ECL_W32(0x0bed0, 0x0);
        ECL_W32(0x0bef0, 0x0);
        ECL_W32(0x0bf10, 0x0);
        ECL_W32(0x0bf30, 0x0);
        ECL_W32(0x0bf50, 0x0);
        ECL_W32(0x0bf5c, 0x0);
        ECL_W32(0x0bf74, 0x0);
        ECL_W32(0x0bfa0, 0x0);
        ECL_W32(0x0bfac, 0x0);
        ECL_W32(0x0bfbc, 0x0);
        break;
    default:
        if (section >= 10000 && section < 20000)
        {
            static const i32 stageWarpTimes[7][9] = {
                {68, 580, 1160, 1540, 2348, 4438, 0, 0, 0},              /* */
                {270, 924, 3528, 4563, 0, 0, 0, 0, 0},                   /* */
                {340, 1050, 1670, 2762, 3807, 4118, 5274, 0, 0},         /* */
                {380, 1454, 2328, 0x0d40, 4872, 5712, 7434, 8354, 9784}, /* */
                {350, 1352, 2292, 3814, 6774, 0, 0, 0, 0},               /* */
                {380, 1484, 0, 0, 0, 0, 0, 0, 0},                        /* */
                {380, 1300, 2600, 3680, 4803, 5933, 7733, 0, 0},         /* */
            };
            i32 stage = (section - 10000) / 100 - 1;   /* */
            i32 portion = (section - 10000) % 100 - 1; /* */
            if (stage >= 0 && stage < 7 && portion >= 0 && portion < 9)
            {
                i32 t = stageWarpTimes[stage][portion];
                if (t > 0)
                    ECL_WARP(t);
            }
        }
        break;
    }

#undef S2B_ND
#undef S3B_N1
#undef S4B_TIME
#undef S7B_CALL
#undef S7B_N1
}

static bool ShouldSkipSpellPracticeDialogue()
{

    if (g_SpellPracticeBossSubId == 0xFFFF || g_SpellPracticeBossSubId == 0xFFFE)
        return false;
    return g_IsSpellPracticeGame > 0;
}

#define ITEM_SPAWNS 3
#define ITEM_TABLES 8

EnemyManager g_EnemyManager;
ChainElem g_EnemyManagerCalcChain;
ChainElem g_EnemyManagerDrawChain;
u8 g_RandomItems[32] = {ITEM_POWER_SMALL, ITEM_POWER_SMALL, ITEM_POINT,       ITEM_POWER_SMALL, ITEM_POINT,
                        ITEM_POWER_SMALL, ITEM_POWER_SMALL, ITEM_POINT,       ITEM_POINT,       ITEM_POINT,
                        ITEM_POWER_SMALL, ITEM_POWER_SMALL, ITEM_POWER_SMALL, ITEM_POINT,       ITEM_POINT,
                        ITEM_POWER_SMALL, ITEM_POINT,       ITEM_POWER_SMALL, ITEM_POINT,       ITEM_POWER_SMALL,
                        ITEM_POINT,       ITEM_POWER_SMALL, ITEM_POINT,       ITEM_POWER_SMALL, ITEM_POINT,
                        ITEM_POWER_SMALL, ITEM_POWER_SMALL, ITEM_POINT,       ITEM_POINT,       ITEM_POINT,
                        ITEM_POWER_SMALL, ITEM_POWER_BIG};

void EnemyManager::Initialize()
{
    i32 i;
    Enemy *enemy;

    enemy = &this->enemies[0];
    memset(this, 0, sizeof(EnemyManager));
    enemy = &this->enemyTemplate;
    memset(enemy, 0, sizeof(Enemy));
    for (i = 0; i < ARRAY_SIZE_SIGNED(this->enemyTemplate.vms); i++)
    {
        enemy->vms[i].anmFileIndex = -1;
    }
    enemy->flags.unk5 = 1;
    enemy->bossTimer.InitializeForPopup();
    enemy->flags.unk6 = 1;
    enemy->flags.unk7 = 1;
    enemy->flags.unk8 = 0;
    enemy->hitboxDimensions = D3DXVECTOR3(12.0f, 12.0f, 12.0f);
    enemy->axisSpeed = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
    enemy->angularVelocity = 0.0f;
    enemy->angle = 0.0f;
    enemy->acceleration = 0.0f;
    enemy->speed = 0.0f;
    enemy->flags.unk1 = 0;
    enemy->flags.unk3 = 0;
    enemy->flags.unk4 = 0;
    enemy->flags.isBoss = 0;
    enemy->stackDepth = 0;
    enemy->life = 1;
    enemy->score = 100;
    enemy->deathAnm1 = 0;
    enemy->deathAnm2 = 0;
    enemy->deathAnm3 = 0;
    enemy->shootInterval = 0;
    enemy->shootIntervalTimer.InitializeForPopup();
    enemy->shootOffset = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
    enemy->anmExLeft = -1;
    enemy->anmExRight = -1;
    enemy->anmExDefaults = -1;
    enemy->flags.unk10 = 1;
    enemy->flags.unk11 = 0;
    enemy->deathCallbackSub = -1;
    enemy->flags.shouldClampPos = 0;
    enemy->effectIdx = 0;
    enemy->runInterrupt = -1;
    enemy->lifeCallbackThreshold = -1;
    enemy->timerCallbackThreshold = -1;
    enemy->laserStore = 0;
    enemy->unk_e41 = 0;
    enemy->flags.unk13 = 0;
    enemy->bulletRankSpeedLow = -0.5f;
    enemy->bulletRankSpeedHigh = 0.5f;
}

EnemyManager::EnemyManager()
{
    this->Initialize();
}

Enemy *EnemyManager::SpawnEnemy(i32 eclSubId, D3DXVECTOR3 *pos, i16 life, i16 itemDrop, i32 score)
{
    Enemy *newEnemy;
    i32 idx;

    newEnemy = this->enemies;
    idx = 0;
    for (; idx < ARRAY_SIZE_SIGNED(this->enemies) - 1; idx++, newEnemy++)
    {
        if (newEnemy->flags.unk5)
            continue;

        *newEnemy = this->enemyTemplate;

        if (life >= 0)
            newEnemy->life = life;

        newEnemy->position = *pos;
        g_EclManager.CallEclSub(&newEnemy->currentContext, eclSubId);
        g_EclManager.RunEcl(newEnemy);
        newEnemy->color = newEnemy->primaryVm.color;
        newEnemy->itemDrop = itemDrop;

        if (life >= 0)
            newEnemy->life = life;

        if (score >= 0)
            newEnemy->score = score;

        newEnemy->maxLife = newEnemy->life;

        if (newEnemy->flags.isBoss && !g_GameManager.isInRetryMenu && !g_GameManager.isInGameMenu &&
            (g_IsSpellPracticeGame == 22 || g_IsSpellPracticeGame == 71 || g_IsSpellPracticeGame == 80 ||
             g_IsSpellPracticeGame == 89 || g_IsSpellPracticeGame == 98))
        {
            g_SpellPracticeResultDelay = 61;
        }

        break;
    }
    return newEnemy;
}

void Enemy::ResetEffectArray(Enemy *enemy)
{
    i32 idx;

    for (idx = 0; idx < enemy->effectIdx; idx++)
    {
        if (!enemy->effectArray[idx])
        {
            continue;
        }
        enemy->effectArray[idx]->unk_17a = 1;
        enemy->effectArray[idx] = NULL;
    }
    enemy->effectIdx = 0;
}

void EnemyManager::RunEclTimeline()
{
    D3DXVECTOR3 pos4;
    D3DXVECTOR3 pos3;
    D3DXVECTOR3 pos2;
    D3DXVECTOR3 pos1;
    EclTimelineInstrArgs *args4;
    EclTimelineInstrArgs *args3;
    EclTimelineInstrArgs *args2;
    EclTimelineInstrArgs *args1;
    i32 subrankIncreaseFrame;
    Enemy *spawnedEnemy;

    if (this->timelineInstr == NULL)
    {
        this->timelineInstr = g_EclManager.timeline;
        if (g_ThpracSection != 0)
        {
            if (g_ThpracSection == TH06_ST6_MID1 && g_EclManager.eclFile != NULL)
            {
                u8 *b = (u8 *)g_EclManager.eclFile;
                u8 *tl_ptr = (u8 *)g_EclManager.eclFile->timelineOffsets[0];
                EclTimelineInstr *instr = (EclTimelineInstr *)tl_ptr;
                i32 count = 0;
                while (instr->time >= 0 && count < 2000)
                {
                    u32 rel = (u32)((u8 *)instr - b);
                    if (instr->time >= 2560 && instr->time <= 2570)
                    {
                        th06::utils::LogDebug("  TL[off=0x%x t=%d op=%d arg0=%d size=%d]", rel, (i32)instr->time,
                                              (i32)instr->opCode, (i32)instr->arg0, (i32)instr->size);
                    }
                    instr = (EclTimelineInstr *)((u8 *)instr + instr->size);
                    count++;
                }
                instr = (EclTimelineInstr *)tl_ptr;
                count = 0;
                while (instr->time >= 0 && count < 2000)
                {
                    u32 rel = (u32)((u8 *)instr - b);
                    if (rel >= 0x77e0 && rel <= 0x7810)
                    {
                        th06::utils::LogDebug("  TL[off=0x%x t=%d op=%d arg0=%d size=%d]", rel, (i32)instr->time,
                                              (i32)instr->opCode, (i32)instr->arg0, (i32)instr->size);
                    }
                    instr = (EclTimelineInstr *)((u8 *)instr + instr->size);
                    count++;
                }
            }
            th06::utils::LogDebug("thprac ECL patch: section=%d", g_ThpracSection);
            ApplyThpracEclPatch(g_ThpracSection, g_ThpracDlg);
            g_ThpracSection = 0;
            if (g_EclManager.eclFile != NULL)
            {
                u8 *b = (u8 *)g_EclManager.eclFile;
                i32 v9e8 = *(i32 *)(b + 0x9e8);
                i16 v77f2 = *(i16 *)(b + 0x77f2);
                i32 tl_current = g_EnemyManager.timelineTime.current;
                th06::utils::LogDebug("post-patch: [0x9e8]=0x%x [0x77f2]=0x%x timelineTime=%d", v9e8, (i32)v77f2,
                                      tl_current);
            }
        }
        else if (g_SpellPracticeWarpTime > 0)
        {
            th06::utils::LogDebug("Warping to %d", g_SpellPracticeWarpTime);
            this->timelineTime.current = g_SpellPracticeWarpTime;
            g_SpellPracticeWarpTime = 0;
        }
    }
    if (g_Gui.HasCurrentMsgIdx() == 0)
    {

        subrankIncreaseFrame = 10 * 4 * 60;
        subrankIncreaseFrame -= g_GameManager.livesRemaining * 4 * 60;
        if (this->timelineTime.HasTicked() && this->timelineTime.AsFrames() % subrankIncreaseFrame == 0)
        {
            g_GameManager.IncreaseSubrank(100);
        }
    }
    while (this->timelineInstr->time >= 0)
    {
        if (this->timelineTime.current == this->timelineInstr->time)
        {
            switch (this->timelineInstr->opCode)
            {
            case 0:
                if (!g_Gui.BossPresent())
                {
                    args1 = &this->timelineInstr->args;
                    this->SpawnEnemy(this->timelineInstr->arg0, args1->Var1AsVec(), args1->ushortVar1,
                                     args1->ushortVar2, args1->uintVar4);
                }
                break;
            case 1:
                if (!g_Gui.BossPresent())
                {
                    this->SpawnEnemy(this->timelineInstr->arg0, this->timelineInstr->args.Var1AsVec(), -1, ITEM_NO_ITEM,
                                     -1);
                }
                break;
            case 2:
                if (!g_Gui.BossPresent())
                {
                    args2 = &this->timelineInstr->args;
                    spawnedEnemy = this->SpawnEnemy(this->timelineInstr->arg0, args2->Var1AsVec(), args2->ushortVar1,
                                                    args2->ushortVar2, args2->uintVar4);
                    spawnedEnemy->flags.unk4 = 1;
                }
                break;
            case 3:
                if (!g_Gui.BossPresent())
                {
                    spawnedEnemy = this->SpawnEnemy(this->timelineInstr->arg0, this->timelineInstr->args.Var1AsVec(),
                                                    -1, ITEM_NO_ITEM, -1);
                    spawnedEnemy->flags.unk4 = 1;
                }
                break;
            case 4:
                if (!g_Gui.BossPresent())
                {
                    args3 = &this->timelineInstr->args;
                    pos1 = *args3->Var1AsVec();
                    if (args3->Var1AsVec()->x <= -990.0f)
                    {
                        pos1.x = g_Rng.GetRandomF32InRange(g_GameManager.playerMovementAreaSize.x);
                    }
                    if (args3->Var1AsVec()->y <= -990.0f)
                    {
                        pos1.y = g_Rng.GetRandomF32InRange(g_GameManager.playerMovementAreaSize.y);
                    }
                    if (args3->Var1AsVec()->z <= -990.0f)
                    {
                        pos1.z = g_Rng.GetRandomF32InRange(800.0f);
                    }
                    this->SpawnEnemy(this->timelineInstr->arg0, &pos1, args3->ushortVar1, args3->ushortVar2,
                                     args3->uintVar4);
                }
                break;
            case 5:
                if (!g_Gui.BossPresent())
                {
                    pos2 = *this->timelineInstr->args.Var1AsVec();
                    if (pos2.x <= -990.0f)
                    {
                        pos2.x = g_Rng.GetRandomF32InRange(g_GameManager.playerMovementAreaSize.x);
                    }
                    if (pos2.y <= -990.0f)
                    {
                        pos2.y = g_Rng.GetRandomF32InRange(g_GameManager.playerMovementAreaSize.y);
                    }
                    if (pos2.z <= -990.0f)
                    {
                        pos2.z = g_Rng.GetRandomF32InRange(800.0f);
                    }
                    this->SpawnEnemy(this->timelineInstr->arg0, &pos2, -1, ITEM_NO_ITEM, -1);
                }
                break;
            case 6:
                if (!g_Gui.BossPresent())
                {
                    args4 = &this->timelineInstr->args;
                    pos3 = *args4->Var1AsVec();
                    if (args4->Var1AsVec()->x <= -990.0f)
                    {
                        pos3.x = g_Rng.GetRandomF32InRange(g_GameManager.playerMovementAreaSize.x);
                    }
                    if (args4->Var1AsVec()->y <= -990.0f)
                    {
                        pos3.y = g_Rng.GetRandomF32InRange(g_GameManager.playerMovementAreaSize.y);
                    }
                    if (args4->Var1AsVec()->z <= -990.0f)
                    {
                        pos3.z = g_Rng.GetRandomF32InRange(800.0f);
                    }
                    spawnedEnemy = this->SpawnEnemy(this->timelineInstr->arg0, &pos3, args4->ushortVar1,
                                                    args4->ushortVar2, args4->uintVar4);
                    spawnedEnemy->flags.unk4 = 1;
                }
                break;
            case 7:
                if (!g_Gui.BossPresent())
                {
                    pos4 = *this->timelineInstr->args.Var1AsVec();
                    if (pos4.x <= -990.0f)
                    {
                        pos4.x = g_Rng.GetRandomF32InRange(g_GameManager.playerMovementAreaSize.x);
                    }
                    if (pos4.y <= -990.0f)
                    {
                        pos4.y = g_Rng.GetRandomF32InRange(g_GameManager.playerMovementAreaSize.y);
                    }
                    if (pos4.z <= -990.0f)
                    {
                        pos4.z = g_Rng.GetRandomF32InRange(800.0f);
                    }
                    spawnedEnemy = this->SpawnEnemy(this->timelineInstr->arg0, &pos4, -1, ITEM_NO_ITEM, -1);
                    spawnedEnemy->flags.unk4 = 1;
                }
                break;
            case 8:
                if (ShouldSkipSpellPracticeDialogue())
                {
                    th06::utils::LogDebug("MsgRead SKIPPED bossSubId=%d", g_SpellPracticeBossSubId);
                    break;
                }
                th06::utils::LogDebug("MsgRead CALLED arg0=%d bossSubId=%d", this->timelineInstr->arg0,
                                      g_SpellPracticeBossSubId);
                if (g_GameManager.difficulty == EASY && g_GameManager.currentStage == 5 &&
                    this->timelineInstr->arg0 == 1)
                {
                    g_Gui.MsgRead(g_GameManager.character * 10 + 3);
                }
                else
                {
                    g_Gui.MsgRead(this->timelineInstr->arg0 + g_GameManager.character * 10);
                }
                break;
            case 9:
                if (ShouldSkipSpellPracticeDialogue())
                {
                    break;
                }
                if (g_Gui.MsgWait())
                {
                    this->timelineTime.Decrement(1);
                    return;
                }
                break;
            case 10:
                this->bosses[this->timelineInstr->args.uintVar1]->runInterrupt = this->timelineInstr->args.uintVar2;
                break;
            case 0xb:
                if (g_IsSpellPracticeGame == 0)
                    g_GameManager.currentPower = this->timelineInstr->arg0;
                break;
            case 0xc:
                if (this->bosses[this->timelineInstr->arg0] != NULL &&
                    this->bosses[this->timelineInstr->arg0]->flags.unk5)
                {
                    this->timelineTime.Decrement(1);
                    return;
                }
            }
        }
        else if (this->timelineTime.current < this->timelineInstr->time)
        {
            break;
        }
        this->timelineInstr = (EclTimelineInstr *)((i32)this->timelineInstr + this->timelineInstr->size);
    }
    if (!g_Gui.HasCurrentMsgIdx())
    {
        g_GameManager.counat++;
    }
    return;
}

ZunBool Enemy::HandleLifeCallback()
{

    i32 i;
    Enemy *curEnemy;

    if (this->life < this->lifeCallbackThreshold)
    {
        this->life = this->lifeCallbackThreshold;
        g_EclManager.CallEclSub(&this->currentContext, this->lifeCallbackSub);
        this->lifeCallbackThreshold = -1;
        this->timerCallbackSub = this->deathCallbackSub;
        this->bulletRankSpeedLow = -0.5f;
        this->bulletRankSpeedHigh = 0.5f;
        this->bulletRankAmount1Low = 0;
        this->bulletRankAmount1High = 0;
        this->bulletRankAmount2Low = 0;
        this->bulletRankAmount2High = 0;
        this->stackDepth = 0;

        curEnemy = g_EnemyManager.enemies;
        for (i = 0; i < ARRAY_SIZE_SIGNED(g_EnemyManager.enemies) - 1; i++, curEnemy++)
        {
            if (!curEnemy->flags.unk5)
            {
                continue;
            }
            if (curEnemy->flags.isBoss)
            {
                continue;
            }
            curEnemy->life = 0;

            if (!curEnemy->flags.unk6 && curEnemy->deathCallbackSub >= 0)
            {
                g_EclManager.CallEclSub(&curEnemy->currentContext, curEnemy->deathCallbackSub);
                curEnemy->deathCallbackSub = -1;
            }
        }
        return true;
    }

    return false;
}

ZunBool Enemy::HandleTimerCallback()
{

    Enemy *curEnemy;
    i32 i;

    if (this->flags.isBoss)
    {
        g_Gui.SetSpellcardSeconds((this->timerCallbackThreshold - this->bossTimer.AsFrames()) / 60);
    }

    if (this->HasBossTimerFinished())
    {
        if (this->lifeCallbackThreshold > 0)
        {
            this->life = this->lifeCallbackThreshold;
            this->lifeCallbackThreshold = -1;
        }
        g_EclManager.CallEclSub(&this->currentContext, this->timerCallbackSub);
        this->timerCallbackThreshold = -1;
        this->timerCallbackSub = this->deathCallbackSub;
        this->bossTimer.InitializeForPopup();
        if (!this->flags.unk16)
        {
            g_EnemyManager.spellcardInfo.isCapturing = false;
            if (g_EnemyManager.spellcardInfo.isActive)
            {
                g_EnemyManager.spellcardInfo.isActive++;
            }
            g_BulletManager.RemoveAllBullets(0);
        }

        curEnemy = g_EnemyManager.enemies;
        for (i = 0; i < ARRAY_SIZE_SIGNED(g_EnemyManager.enemies) - 1; i++, curEnemy++)
        {
            if (!curEnemy->flags.unk5)
            {
                continue;
            }
            if (curEnemy->flags.isBoss)
            {
                continue;
            }
            curEnemy->life = 0;

            if (!curEnemy->flags.unk6 && curEnemy->deathCallbackSub >= 0)
            {
                g_EclManager.CallEclSub(&curEnemy->currentContext, curEnemy->deathCallbackSub);
                curEnemy->deathCallbackSub = -1;
            }
        }
        this->bulletRankSpeedLow = -0.5f;
        this->bulletRankSpeedHigh = 0.5f;
        this->bulletRankAmount1Low = 0;
        this->bulletRankAmount1High = 0;
        this->bulletRankAmount2Low = 0;
        this->bulletRankAmount2High = 0;
        this->stackDepth = 0;
        return true;
    }
    return false;
}

void Enemy::Despawn()
{
    if (!this->flags.unk11)
    {
        this->flags.unk5 = 0;
    }
    else
    {
        this->flags.unk6 = 0;
    }
    if (this->flags.isBoss)
    {
        g_Gui.bossPresent = false;
    }
    if (this->effectIdx != 0)
    {
        this->ResetEffectArray(this);
    }
}

void Enemy::ClampPos()
{
    if (this->flags.shouldClampPos)
    {
        if (this->position.x < this->lowerMoveLimit.x)
        {
            this->position.x = this->lowerMoveLimit.x;
        }
        else if (this->position.x > this->upperMoveLimit.x)
        {
            this->position.x = this->upperMoveLimit.x;
        }

        if (this->position.y < this->lowerMoveLimit.y)
        {
            this->position.y = this->lowerMoveLimit.y;
        }
        else if (this->position.y > this->upperMoveLimit.y)
        {
            this->position.y = this->upperMoveLimit.y;
        }
    }
}

ZunResult EnemyManager::RegisterChain(char *stgEnm1, char *stgEnm2)
{
    EnemyManager *mgr = &g_EnemyManager;
    mgr->Initialize();
    mgr->stgEnmAnmFilename = stgEnm1;
    mgr->stgEnm2AnmFilename = stgEnm2;
    g_EnemyManagerCalcChain.callback = (ChainCallback)mgr->OnUpdate;
    g_EnemyManagerCalcChain.addedCallback = NULL;
    g_EnemyManagerCalcChain.deletedCallback = NULL;
    g_EnemyManagerCalcChain.addedCallback = (ChainAddedCallback)mgr->AddedCallback;
    g_EnemyManagerCalcChain.deletedCallback = (ChainAddedCallback)mgr->DeletedCallback;
    g_EnemyManagerCalcChain.arg = mgr;
    if (g_Chain.AddToCalcChain(&g_EnemyManagerCalcChain, TH_CHAIN_PRIO_CALC_ENEMYMANAGER))
    {
        return ZUN_ERROR;
    }
    g_EnemyManagerDrawChain.callback = (ChainCallback)mgr->OnDraw;
    g_EnemyManagerDrawChain.addedCallback = NULL;
    g_EnemyManagerDrawChain.deletedCallback = NULL;
    g_EnemyManagerDrawChain.arg = mgr;
    if (g_Chain.AddToDrawChain(&g_EnemyManagerDrawChain, TH_CHAIN_PRIO_DRAW_ENEMYMANAGER))
    {
        return ZUN_ERROR;
    }
    return ZUN_SUCCESS;
}

ChainCallbackResult EnemyManager::OnUpdate(EnemyManager *mgr)
{
    Enemy *curEnemy;
    i32 enemyLifeBeforeDmg;
    i32 enemyVmIdx;
    D3DXVECTOR3 enemyHitbox;
    i32 enemyIdx;
    i32 damage;
    i32 local_8;

    local_8 = 0;
    mgr->RunEclTimeline();
    for (curEnemy = &mgr->enemies[0], mgr->enemyCount = 0, enemyIdx = 0; enemyIdx < ARRAY_SIZE_SIGNED(mgr->enemies) - 1;
         enemyIdx++, curEnemy++)
    {
        if (!curEnemy->flags.unk5)
        {
            continue;
        }
        mgr->enemyCount++;
        curEnemy->Move();
        curEnemy->ClampPos();
        if (curEnemy->flags.unk8 == 0 &&
            g_GameManager.IsInBounds(curEnemy->position.x, curEnemy->position.y, curEnemy->primaryVm.sprite->widthPx,
                                     curEnemy->primaryVm.sprite->heightPx))
        {
            curEnemy->flags.unk8 = 1;
        }
        if (curEnemy->flags.unk8 == 1 &&
            !g_GameManager.IsInBounds(curEnemy->position.x, curEnemy->position.y, curEnemy->primaryVm.sprite->widthPx,
                                      curEnemy->primaryVm.sprite->heightPx))
        {
            curEnemy->flags.unk5 = 0;
            curEnemy->Despawn();
            continue;
        }
        if (curEnemy->lifeCallbackThreshold >= 0)
        {
            curEnemy->HandleLifeCallback();
        }
        if (curEnemy->timerCallbackThreshold >= 0)
        {
            curEnemy->HandleTimerCallback();
        }
        if (g_EclManager.RunEcl(curEnemy) == ZUN_ERROR)
        {
            curEnemy->flags.unk5 = 0;
            curEnemy->Despawn();
            continue;
        }
        curEnemy->ClampPos();
        curEnemy->primaryVm.color = curEnemy->color;
        g_AnmManager->ExecuteScript(&curEnemy->primaryVm);
        curEnemy->color = curEnemy->primaryVm.color;
        for (enemyVmIdx = 0; enemyVmIdx < 8; enemyVmIdx++)
        {
            if (curEnemy->vms[enemyVmIdx].anmFileIndex >= 0 && g_AnmManager->ExecuteScript(&curEnemy->vms[enemyVmIdx]))
            {
                curEnemy->vms[enemyVmIdx].anmFileIndex = -1;
            }
        }
        local_8 = 0;
        if (curEnemy->flags.unk8 != 0 && !curEnemy->flags.unk15)
        {
            enemyLifeBeforeDmg = curEnemy->life;
            if (curEnemy->flags.unk7 && curEnemy->flags.unk6)
            {
                enemyHitbox = curEnemy->hitboxDimensions / 1.5;
                if (g_Player.CalcKillBoxCollision(&curEnemy->position, &enemyHitbox) == 1 && curEnemy->flags.unk6 &&
                    !curEnemy->flags.isBoss)
                {
                    curEnemy->life -= 10;
                }
                if (g_IsOnline)
                {
                    if (g_Player2.CalcKillBoxCollision(&curEnemy->position, &enemyHitbox) == 1 &&
                        curEnemy->flags.unk6 && !curEnemy->flags.isBoss)
                    {
                        curEnemy->life -= 10;
                    }
                }
            }
            if (curEnemy->flags.unk6 != 0)
            {
                ZunBool local_8_p2 = false;
                damage = g_Player.CalcDamageToEnemy(&curEnemy->position, &curEnemy->hitboxDimensions, &local_8);
                if (g_IsOnline)
                {
                    i32 damage2 =
                        g_Player2.CalcDamageToEnemy(&curEnemy->position, &curEnemy->hitboxDimensions, &local_8_p2);
                    damage += damage2;
                    if (local_8_p2)
                    {
                        local_8 = true;
                    }
                }
                if (damage >= 70)
                {
                    damage = 70;
                }
                g_GameManager.score = (damage / 5) * 10 + g_GameManager.score;
                if (mgr->spellcardInfo.isActive != 0)
                {
                    if (local_8 == 0)
                    {
                        if (damage > 7)
                        {
                            damage = damage / 7;
                        }
                        else if (damage != 0)
                        {
                            damage = 1;
                        }
                    }
                    else if (mgr->spellcardInfo.usedBomb != 0)
                    {
                        if (damage > 3)
                        {
                            damage = damage / 3;
                        }
                        else if (damage != 0)
                        {
                            damage = 1;
                        }
                    }
                    else
                    {
                        damage = 0;
                    }
                }
                if (curEnemy->flags.unk10 != 0)
                {
                    curEnemy->life -= damage;
                }
                if (g_Player.positionOfLastEnemyHit.y < curEnemy->position.y)
                {
                    g_Player.positionOfLastEnemyHit = curEnemy->position;
                }
                if (g_IsOnline)
                {
                    if (g_Player2.positionOfLastEnemyHit.y < curEnemy->position.y)
                    {
                        g_Player2.positionOfLastEnemyHit = curEnemy->position;
                    }
                }
            }
            if (0 >= curEnemy->life && curEnemy->flags.unk6 != 0)
            {
                curEnemy->lifeCallbackThreshold = -1;
                curEnemy->timerCallbackThreshold = -1;
                switch (curEnemy->flags.unk11)
                {
                case 3:
                    curEnemy->life = 1;
                    curEnemy->flags.unk10 = 0;
                    curEnemy->flags.unk11 = 0;
                    g_Gui.bossPresent = 0;
                    g_EffectManager.SpawnParticles(curEnemy->deathAnm1, &curEnemy->position, 1, COLOR_WHITE);
                    g_EffectManager.SpawnParticles(curEnemy->deathAnm1, &curEnemy->position, 1, COLOR_WHITE);
                    g_EffectManager.SpawnParticles(curEnemy->deathAnm1, &curEnemy->position, 1, COLOR_WHITE);
                    break;
                case 1:
                case 0:
                    g_GameManager.AddScore(curEnemy->score);
                    if (curEnemy->flags.unk11 == 1)
                    {
                        curEnemy->flags.unk6 = 0;
                    }
                    else
                    {
                        curEnemy->flags.unk5 = 0;
                    }
                    if (curEnemy->flags.isBoss)
                    {
                        g_Gui.bossPresent = 0;
                        Enemy::ResetEffectArray(curEnemy);
                    }
                    // Fall through to case 2
                case 2:
                    if (curEnemy->itemDrop >= 0)
                    {
                        g_EffectManager.SpawnParticles(curEnemy->deathAnm2 + 4, &curEnemy->position, 3, 0xffffffff);
                        g_ItemManager.SpawnItem(&curEnemy->position, (ItemType)curEnemy->itemDrop, local_8);
                    }
                    else if (curEnemy->itemDrop == ITEM_NO_ITEM)
                    {
                        if (mgr->randomItemSpawnIndex % 3 == 0)
                        {
                            g_EffectManager.SpawnParticles(curEnemy->deathAnm2 + 4, &curEnemy->position, 6,
                                                           COLOR_WHITE);
                            g_ItemManager.SpawnItem(&curEnemy->position,
                                                    (ItemType)g_RandomItems[mgr->randomItemTableIndex], local_8);
                            mgr->randomItemTableIndex++;
                            if (ARRAY_SIZE_SIGNED(g_RandomItems) <= mgr->randomItemTableIndex)
                            {
                                mgr->randomItemTableIndex = 0;
                            }
                        }
                        mgr->randomItemSpawnIndex++;
                    }
                    if (curEnemy->flags.isBoss && !g_EnemyManager.spellcardInfo.isActive)
                    {
                        g_BulletManager.DespawnBullets(12800, false);
                        if (g_IsSpellPracticeGame > 0 && g_SpellPracticeBossSubId == 0 &&
                            !g_GameManager.isInRetryMenu && !g_GameManager.isInGameMenu)
                        {
                            g_SpellPracticeResultDelay = 61;
                        }
                    }
                    curEnemy->life = 0;
                    break;
                }
                g_SoundPlayer.PlaySoundByIdx((SoundIdx)((enemyIdx % 2) + SOUND_2), 0);
                g_EffectManager.SpawnParticles(curEnemy->deathAnm1, &curEnemy->position, 1, 0xffffffff);
                g_EffectManager.SpawnParticles(curEnemy->deathAnm2 + 4, &curEnemy->position, 4, 0xffffffff);
                if (curEnemy->deathCallbackSub >= 0)
                {
                    curEnemy->bulletRankSpeedLow = -0.5;
                    curEnemy->bulletRankSpeedHigh = 0.5;
                    curEnemy->bulletRankAmount1Low = 0;
                    curEnemy->bulletRankAmount1High = 0;
                    curEnemy->bulletRankAmount2Low = 0;
                    curEnemy->bulletRankAmount2High = 0;
                    curEnemy->stackDepth = 0;
                    g_EclManager.CallEclSub(&curEnemy->currentContext, curEnemy->deathCallbackSub);
                    curEnemy->deathCallbackSub = -1;
                }
            }
            if (curEnemy->flags.isBoss != 0 && !g_Gui.HasCurrentMsgIdx())
            {
                g_Gui.SetBossHealthBar(curEnemy->LifePercent());
            }
            if (curEnemy->unk_e41 != 0)
            {
                curEnemy->unk_e41--;
                curEnemy->primaryVm.flags.colorOp = AnmVmColorOp_Modulate;
            }
            else if (enemyLifeBeforeDmg > curEnemy->life)
            {
                g_SoundPlayer.PlaySoundByIdx(SOUND_TOTAL_BOSS_DEATH, 0);
                curEnemy->primaryVm.flags.colorOp = AnmVmColorOp_Add;

                curEnemy->unk_e41 = 1;
            }
            else
            {
                curEnemy->primaryVm.flags.colorOp = AnmVmColorOp_Modulate;
            }
        }
        Enemy::UpdateEffects(curEnemy);
        if (g_GameManager.isTimeStopped == 0)
        {
            curEnemy->bossTimer.Tick();
        }
    }
    mgr->timelineTime.Tick();
    return CHAIN_CALLBACK_RESULT_CONTINUE;
}

void Enemy::UpdateEffects(Enemy *enemy)
{
    Effect *effect;
    i32 i;

    for (i = 0; i < enemy->effectIdx; i++)
    {
        effect = enemy->effectArray[i];
        if (!effect)
        {
            continue;
        }

        effect->position = enemy->position;
        if (effect->unk_15c < enemy->effectDistance)
        {
            effect->unk_15c += 0.3f;
        }

        effect->angleRelated = utils::AddNormalizeAngle(effect->angleRelated, ZUN_PI / 100);
    }
}

struct HPBarVertex
{
    float x, y, z;
    D3DCOLOR color;
};

static void DrawRing(float r_in, float r_out, float start_angle, float end_angle, D3DCOLOR color)
{
    const int max_segments = 128;
    float angle_diff = end_angle - start_angle;
    int segments = (int)(max_segments * fabsf(angle_diff) / (2.0f * ZUN_PI));
    if (segments < 3)
        segments = 3;
    if (segments > max_segments)
        segments = max_segments;

    HPBarVertex vertices[2 * (max_segments + 1)];

    for (int i = 0; i <= segments; i++)
    {
        float t = (float)i / (float)segments;
        float angle = start_angle + t * angle_diff;
        float cos_a = cosf(angle);
        float sin_a = sinf(angle);

        vertices[2 * i].x = r_in * cos_a;
        vertices[2 * i].y = r_in * sin_a;
        vertices[2 * i].z = 0.0f;
        vertices[2 * i].color = color;

        vertices[2 * i + 1].x = r_out * cos_a;
        vertices[2 * i + 1].y = r_out * sin_a;
        vertices[2 * i + 1].z = 0.0f;
        vertices[2 * i + 1].color = color;
    }

    g_Supervisor.d3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2 * segments, vertices, sizeof(HPBarVertex));
}

static i32 s_SpellcardLifeThresholds[16];
static i32 s_SpellcardLifeThresholdsCount = 0;
static u32 s_LastSpellcardIdx = 0xffffffff;

static void DrawBossEnemyMarker(f32 bossGameX, D3DCOLOR color)
{
    const f32 W = 20.0f;
    const f32 H = 12.0f;
    const f32 sx = GAME_REGION_LEFT + bossGameX;
    f32 sy_base, sy_tip;

    sy_base = 478.0f;
    sy_tip = sy_base - H;

    DWORD oldAlpha, oldSrc, oldDest, oldZWrite;
    DWORD oldColorOp, oldAlphaOp, oldColorArg1, oldAlphaArg1;
    DWORD oldVertexShader;
    IDirect3DBaseTexture8 *oldTex = NULL;
    g_Supervisor.d3dDevice->GetRenderState(D3DRS_ALPHABLENDENABLE, &oldAlpha);
    g_Supervisor.d3dDevice->GetRenderState(D3DRS_SRCBLEND, &oldSrc);
    g_Supervisor.d3dDevice->GetRenderState(D3DRS_DESTBLEND, &oldDest);
    g_Supervisor.d3dDevice->GetRenderState(D3DRS_ZWRITEENABLE, &oldZWrite);
    g_Supervisor.d3dDevice->GetTextureStageState(0, D3DTSS_COLOROP, &oldColorOp);
    g_Supervisor.d3dDevice->GetTextureStageState(0, D3DTSS_ALPHAOP, &oldAlphaOp);
    g_Supervisor.d3dDevice->GetTextureStageState(0, D3DTSS_COLORARG1, &oldColorArg1);
    g_Supervisor.d3dDevice->GetTextureStageState(0, D3DTSS_ALPHAARG1, &oldAlphaArg1);
    g_Supervisor.d3dDevice->GetVertexShader(&oldVertexShader);
    g_Supervisor.d3dDevice->GetTexture(0, &oldTex);

    g_Supervisor.d3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    g_Supervisor.d3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    g_Supervisor.d3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    g_Supervisor.d3dDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    g_Supervisor.d3dDevice->SetTexture(0, NULL);
    g_Supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    g_Supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    g_Supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
    g_Supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
    g_Supervisor.d3dDevice->SetVertexShader(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);

    struct RHW
    {
        f32 x, y, z, rhw;
        D3DCOLOR c;
    };

    const i32 SEGMENTS = 16;
    const f32 rx = W;
    const f32 ry = sy_base - sy_tip;
    RHW v[SEGMENTS + 2];
    v[0].x = sx;
    v[0].y = sy_base;
    v[0].z = 0.f;
    v[0].rhw = 1.f;
    v[0].c = color;
    for (i32 i = 0; i <= SEGMENTS; i++)
    {
        f32 angle = ZUN_PI + ZUN_PI * (f32)i / (f32)SEGMENTS;
        v[i + 1].x = sx + rx * cosf(angle);
        v[i + 1].y = sy_base + ry * sinf(angle);
        v[i + 1].z = 0.f;
        v[i + 1].rhw = 1.f;
        v[i + 1].c = color;
    }
    g_Supervisor.d3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, SEGMENTS, v, sizeof(RHW));

    g_Supervisor.d3dDevice->SetTexture(0, oldTex);
    if (oldTex)
        oldTex->Release();
    g_Supervisor.d3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, oldAlpha);
    g_Supervisor.d3dDevice->SetRenderState(D3DRS_SRCBLEND, oldSrc);
    g_Supervisor.d3dDevice->SetRenderState(D3DRS_DESTBLEND, oldDest);
    g_Supervisor.d3dDevice->SetRenderState(D3DRS_ZWRITEENABLE, oldZWrite);
    g_Supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_COLOROP, oldColorOp);
    g_Supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, oldAlphaOp);
    g_Supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_COLORARG1, oldColorArg1);
    g_Supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, oldAlphaArg1);
    g_Supervisor.d3dDevice->SetVertexShader(oldVertexShader);
    g_AnmManager->SetCurrentVertexShader(0xff);
    g_AnmManager->SetCurrentColorOp(0xff);
    g_AnmManager->SetCurrentBlendMode(0xff);
    g_AnmManager->SetCurrentZWriteDisable(0xff);
}

static void DrawBossHpCircle(Enemy *boss)
{
    D3DXMATRIX origWorld;
    g_Supervisor.d3dDevice->GetTransform(D3DTS_WORLD, &origWorld);

    DWORD oldZWrite, oldZFunc, oldAlphaBlend, oldSrcBlend, oldDestBlend;
    g_Supervisor.d3dDevice->GetRenderState(D3DRS_ZWRITEENABLE, &oldZWrite);
    g_Supervisor.d3dDevice->GetRenderState(D3DRS_ZFUNC, &oldZFunc);
    g_Supervisor.d3dDevice->GetRenderState(D3DRS_ALPHABLENDENABLE, &oldAlphaBlend);
    g_Supervisor.d3dDevice->GetRenderState(D3DRS_SRCBLEND, &oldSrcBlend);
    g_Supervisor.d3dDevice->GetRenderState(D3DRS_DESTBLEND, &oldDestBlend);

    D3DXMATRIX matTranslate;
    D3DXMatrixTranslation(&matTranslate, boss->position.x, -boss->position.y, 0.495f);
    g_Supervisor.d3dDevice->SetTransform(D3DTS_WORLD, &matTranslate);

    g_Supervisor.d3dDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    g_Supervisor.d3dDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);
    g_Supervisor.d3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    g_Supervisor.d3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    g_Supervisor.d3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

    g_Supervisor.d3dDevice->SetTexture(0, NULL);
    g_Supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    g_Supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    g_Supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
    g_Supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);

    g_Supervisor.d3dDevice->SetVertexShader(D3DFVF_XYZ | D3DFVF_DIFFUSE);

    float total_arc = 2.0f * ZUN_PI;
    if (g_EnemyManager.spellcardInfo.isActive)
    {
        float ratio = (float)boss->maxLife / 3000.0f;
        if (ratio < 0.35f)
            ratio = 0.35f;
        if (ratio > 1.0f)
            ratio = 1.0f;
        total_arc = ratio * 2.0f * ZUN_PI;
    }

    D3DCOLOR bgColor = D3DCOLOR_ARGB(0x50, 0x00, 0x00, 0x00);
    DrawRing(55.0f, 61.0f, ZUN_PI / 2.0f, ZUN_PI / 2.0f + total_arc, bgColor);

    float pct = 0.0f;
    if (boss->maxLife > 0)
    {
        pct = (float)boss->life / (float)boss->maxLife;
    }
    if (pct < 0.0f)
        pct = 0.0f;
    if (pct > 1.0f)
        pct = 1.0f;

    if (pct > 0.0f)
    {
        D3DCOLOR borderColor = D3DCOLOR_ARGB(0xE0, 0xFF, 0x30, 0x30);
        D3DCOLOR insideColor = D3DCOLOR_ARGB(0xF0, 0xFF, 0xFF, 0xFF);
        DrawRing(55.0f, 61.0f, ZUN_PI / 2.0f, ZUN_PI / 2.0f + pct * total_arc, borderColor);
        DrawRing(56.5f, 59.5f, ZUN_PI / 2.0f, ZUN_PI / 2.0f + pct * total_arc, insideColor);

        if (g_EnemyManager.spellcardInfo.idx != s_LastSpellcardIdx)
        {
            s_LastSpellcardIdx = g_EnemyManager.spellcardInfo.idx;
            s_SpellcardLifeThresholdsCount = 0;
        }
        if (boss->lifeCallbackThreshold > 0)
        {
            bool found = false;
            for (int i = 0; i < s_SpellcardLifeThresholdsCount; i++)
            {
                if (s_SpellcardLifeThresholds[i] == boss->lifeCallbackThreshold)
                {
                    found = true;
                    break;
                }
            }
            if (!found && s_SpellcardLifeThresholdsCount < ARRAY_SIZE_SIGNED(s_SpellcardLifeThresholds))
            {
                s_SpellcardLifeThresholds[s_SpellcardLifeThresholdsCount++] = boss->lifeCallbackThreshold;
            }
        }
        for (int i = 0; i < s_SpellcardLifeThresholdsCount; i++)
        {
            float T = -1.0f;
            if (s_SpellcardLifeThresholds[i] > 0 && boss->maxLife > 0)
            {
                T = (float)s_SpellcardLifeThresholds[i] / (float)boss->maxLife;
            }
            if (T > 0.0f)
            {
                float theta = ZUN_PI / 2.0f + T * total_arc;
                float epsilon = 0.005f * ZUN_PI;
                D3DCOLOR tickColor = D3DCOLOR_ARGB(0xFF, 0x30, 0xD0, 0xFF);
                DrawRing(54.0f, 62.0f, theta - epsilon, theta + epsilon, tickColor);
            }
        }
    }

    g_Supervisor.d3dDevice->SetTransform(D3DTS_WORLD, &origWorld);
    g_Supervisor.d3dDevice->SetRenderState(D3DRS_ZWRITEENABLE, oldZWrite);
    g_Supervisor.d3dDevice->SetRenderState(D3DRS_ZFUNC, oldZFunc);
    g_Supervisor.d3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, oldAlphaBlend);
    g_Supervisor.d3dDevice->SetRenderState(D3DRS_SRCBLEND, oldSrcBlend);
    g_Supervisor.d3dDevice->SetRenderState(D3DRS_DESTBLEND, oldDestBlend);

    g_Supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    g_Supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    g_Supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    g_Supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);

    g_AnmManager->SetCurrentVertexShader(0xff);
    g_AnmManager->SetCurrentColorOp(0xff);
    g_AnmManager->SetCurrentBlendMode(0xff);
    g_AnmManager->SetCurrentZWriteDisable(0xff);
}

static i32 GetBossMarkerBlinkLevel(Enemy *boss)
{
    i32 level = 0;
    i32 work;

    if (boss->lifeCallbackThreshold >= 0)
    {

        work = boss->life - boss->lifeCallbackThreshold;
        if (g_EnemyManager.spellcardInfo.isActive)
        {
            if (work < 120)
                level = 3;
            else if (work < 200)
                level = 2;
            else if (work < 300)
                level = 1;
        }
        else
        {
            if (work < 500)
                level = 3;
            else if (work < 1500)
                level = 2;
            else if (work < 2200)
                level = 1;
        }
    }
    else
    {

        work = boss->life;
        if (g_EnemyManager.spellcardInfo.isActive)
        {
            if (work < 120)
                level = 3;
            else if (work < 300)
                level = 2;
            else if (work < 400)
                level = 1;
        }
        else
        {
            if (work < 600)
                level = 3;
            else if (work < 1600)
                level = 2;
            else if (work < 2400)
                level = 1;
        }
    }
    return level;
}

void EnemyManager::DrawMarkers()
{

    if (g_ShowEnemyMarker)
    {

        if (((g_Supervisor.cfg.opts >> GCOS_DISPLAY_MINIMUM_GRAPHICS) & 1) == 0 && g_Gui.impl != NULL &&
            g_Gui.impl->vms[8].sprite != NULL)
        {
            AnmVm *borderVm = &g_Gui.impl->vms[8];
            D3DXVECTOR3 savedPos = borderVm->pos;
            for (f32 bx = 32.0f; bx < 416.0f; bx += 32.0f)
            {
                borderVm->pos = D3DXVECTOR3(bx, 464.0f, 0.49f);
                g_AnmManager->DrawNoRotation(borderVm);
            }
            borderVm->pos = savedPos;
        }

        for (i32 bi = 0; bi < 8; bi++)
        {
            Enemy *boss = g_EnemyManager.bosses[bi];
            if (boss == NULL || !boss->flags.unk5 || !boss->flags.isBoss)
                continue;

            if (boss->flags.unk15)
                continue;

            if (boss->position.x < 24.0f || boss->position.x > 360.0f)
                continue;

            f32 dist = fabsf(boss->position.x - g_Player.positionCenter.x);
            i32 alpha = 160;
            if (dist < 64.0f)
            {
                alpha = (i32)(dist * 64.0f / 64.0f + 96.0f);
            }

            i32 blinkLevel = GetBossMarkerBlinkLevel(boss);
            if (blinkLevel > 0)
            {
                static const i32 blinkIntervals[4] = {0, 8, 4, 2};
                if (g_Supervisor.calcCount % blinkIntervals[blinkLevel] == 0)
                {
                    alpha = 255;
                }
            }
            DrawBossEnemyMarker(boss->position.x, D3DCOLOR_ARGB(alpha, 255, 32, 32));
        }
    }

    if (g_VerticalMode && ((g_Supervisor.cfg.opts >> GCOS_DISPLAY_MINIMUM_GRAPHICS) & 1) == 0 && g_Gui.impl != NULL)
    {
        const f32 starStep = 16.0f;
        i32 idx;

        if (g_Gui.impl->vms[17].sprite != NULL)
        {
            AnmVm *bombVm = &g_Gui.impl->vms[17];
            D3DXVECTOR3 savedPos = bombVm->pos;
            u32 savedAnchor = bombVm->flags.anchor;
            bombVm->flags.anchor = AnmVmAnchor_Top;

            for (idx = 0; idx < g_GameManager.bombsRemaining; idx++)
            {
                bombVm->pos = D3DXVECTOR3(GAME_REGION_LEFT + starStep / 2.0f + idx * starStep, 0.0f, 0.49f);
                g_AnmManager->DrawNoRotation(bombVm);
            }

            if (g_IsOnline)
            {
                for (idx = 0; idx < g_P2BombsRemaining; idx++)
                {
                    bombVm->pos = D3DXVECTOR3((f32)GAME_REGION_RIGHT - starStep / 2.0f - idx * starStep, 0.0f, 0.49f);
                    g_AnmManager->DrawNoRotation(bombVm);
                }
            }

            bombVm->pos = savedPos;
            bombVm->flags.anchor = savedAnchor;
        }
    }

    if (g_VerticalMode && ((g_Supervisor.cfg.opts >> GCOS_DISPLAY_MINIMUM_GRAPHICS) & 1) == 0 && g_Gui.impl != NULL)
    {
        const f32 starStep = 16.0f;
        i32 idx;
        char grazeBuf[16];
        sprintf(grazeBuf, "%d", g_GameManager.grazeInStage);

        D3DVIEWPORT8 wideVp;
        wideVp.X = 0;
        wideVp.Y = 0;
        wideVp.Width = GAME_WINDOW_WIDTH;
        wideVp.Height = GAME_WINDOW_HEIGHT + 128;
        wideVp.MinZ = 0.0f;
        wideVp.MaxZ = 1.0f;
        g_Supervisor.d3dDevice->SetViewport(&wideVp);

        if (g_Gui.impl->vms[8].sprite != NULL)
        {
            AnmVm *borderVm = &g_Gui.impl->vms[8];
            D3DXVECTOR3 savedBorderPos = borderVm->pos;
            u32 savedBorderAnchor = borderVm->flags.anchor;
            borderVm->flags.anchor = AnmVmAnchor_TopLeft;
            for (f32 bx = 32.0f; bx < 416.0f; bx += 32.0f)
            {
                borderVm->pos = D3DXVECTOR3(bx, (f32)(GAME_WINDOW_HEIGHT + 0), 0.49f);
                g_AnmManager->DrawNoRotation(borderVm);
            }
            borderVm->pos = savedBorderPos;
            borderVm->flags.anchor = savedBorderAnchor;
        }

        if (g_Gui.impl->vms[8].sprite != NULL)
        {
            AnmVm *borderVm = &g_Gui.impl->vms[8];
            D3DXVECTOR3 savedBorderPos = borderVm->pos;
            u32 savedBorderAnchor = borderVm->flags.anchor;
            borderVm->flags.anchor = AnmVmAnchor_TopLeft;
            for (f32 bx = 32.0f; bx < 416.0f; bx += 32.0f)
            {
                borderVm->pos = D3DXVECTOR3(bx, (f32)(GAME_WINDOW_HEIGHT + 16), 0.49f);
                g_AnmManager->DrawNoRotation(borderVm);
            }
            borderVm->pos = savedBorderPos;
            borderVm->flags.anchor = savedBorderAnchor;
        }

        if (g_Gui.impl->vms[8].sprite != NULL)
        {
            AnmVm *borderVm = &g_Gui.impl->vms[8];
            D3DXVECTOR3 savedBorderPos = borderVm->pos;
            u32 savedBorderAnchor = borderVm->flags.anchor;
            borderVm->flags.anchor = AnmVmAnchor_TopLeft;
            for (f32 bx = 32.0f; bx < 416.0f; bx += 32.0f)
            {
                borderVm->pos = D3DXVECTOR3(bx, (f32)(GAME_WINDOW_HEIGHT + 48), 0.49f);
                g_AnmManager->DrawNoRotation(borderVm);
            }
            borderVm->pos = savedBorderPos;
            borderVm->flags.anchor = savedBorderAnchor;
        }

        if (g_Gui.impl->vms[15].sprite != NULL)
        {
            static const f32 pointLabelW = 16.0f;
            AnmVm *lv = &g_Gui.impl->vms[15];
            D3DXVECTOR3 savedLPos = lv->pos;
            u32 savedLAnch = lv->flags.anchor;
            lv->flags.anchor = AnmVmAnchor_TopLeft;

            lv->pos = D3DXVECTOR3(GAME_REGION_LEFT, (f32)(GAME_WINDOW_HEIGHT + 16), 0.49f);
            g_AnmManager->DrawNoRotation(lv);

            AnmVm *pv = &g_AsciiManager.vm0;
            u32 savedAnchorPv = pv->flags.anchor;
            u32 savedVisiblePv = pv->flags.isVisible;
            D3DXVECTOR3 savedPosPv = pv->pos;
            f32 savedScaleXPv = pv->scaleX;
            f32 savedScaleYPv = pv->scaleY;
            ZunColor savedColorPv = pv->color;

            pv->flags.anchor = AnmVmAnchor_TopLeft;
            pv->flags.isVisible = 1;
            pv->flags.flag1 = 1;
            pv->scaleX = 1.0f;
            pv->scaleY = 1.0f;
            pv->color = 0xFFFFFFFF;

            f32 charW = 14.0f;
            char p1pointBuf[16];
            sprintf(p1pointBuf, "%d", g_GameManager.pointItemsCollectedInStage);

            pv->pos = D3DXVECTOR3(GAME_REGION_LEFT + pointLabelW, (f32)(GAME_WINDOW_HEIGHT + 16), 0.0f);
            for (const char *ch = p1pointBuf; *ch != '\0'; ch++)
            {
                if (*ch != ' ')
                {
                    pv->sprite = &g_AnmManager->sprites[(unsigned char)*ch - 0x15];
                    g_AnmManager->DrawNoRotation(pv);
                }
                pv->pos.x += charW;
            }

            {
                char p2pointBuf[16];
                sprintf(p2pointBuf, "%d", g_IsOnline ? (i32)g_P2PointItems : 0);
                i32 p2nlen = 0;
                for (const char *ch = p2pointBuf; *ch != '\0'; ch++)
                    p2nlen++;
                f32 p2numStartX = (f32)GAME_REGION_RIGHT - pointLabelW - p2nlen * charW;
                lv->pos = D3DXVECTOR3((f32)GAME_REGION_RIGHT - pointLabelW, (f32)(GAME_WINDOW_HEIGHT + 16), 0.49f);
                g_AnmManager->DrawNoRotation(lv);
                pv->pos = D3DXVECTOR3(p2numStartX, (f32)(GAME_WINDOW_HEIGHT + 16), 0.0f);
                for (const char *ch = p2pointBuf; *ch != '\0'; ch++)
                {
                    if (*ch != ' ')
                    {
                        pv->sprite = &g_AnmManager->sprites[(unsigned char)*ch - 0x15];
                        g_AnmManager->DrawNoRotation(pv);
                    }
                    pv->pos.x += charW;
                }
            }

            lv->pos = savedLPos;
            lv->flags.anchor = savedLAnch;
            pv->flags.anchor = savedAnchorPv;
            pv->flags.isVisible = savedVisiblePv;
            pv->pos = savedPosPv;
            pv->scaleX = savedScaleXPv;
            pv->scaleY = savedScaleYPv;
            pv->color = savedColorPv;
        }

        if (g_Gui.impl->vms[8].sprite != NULL)
        {
            AnmVm *borderVm = &g_Gui.impl->vms[8];
            D3DXVECTOR3 savedBorderPos = borderVm->pos;
            u32 savedBorderAnchor = borderVm->flags.anchor;
            borderVm->flags.anchor = AnmVmAnchor_TopLeft;
            for (f32 bx = 32.0f; bx < 416.0f; bx += 32.0f)
            {
                borderVm->pos = D3DXVECTOR3(bx, (f32)(GAME_WINDOW_HEIGHT + 80), 0.49f);
                g_AnmManager->DrawNoRotation(borderVm);
            }
            borderVm->pos = savedBorderPos;
            borderVm->flags.anchor = savedBorderAnchor;
        }

        if (g_Gui.impl->vms[16].sprite != NULL)
        {
            AnmVm *lifeVm = &g_Gui.impl->vms[16];
            D3DXVECTOR3 savedPos = lifeVm->pos;
            u32 savedAnchor = lifeVm->flags.anchor;
            lifeVm->flags.anchor = AnmVmAnchor_Top;

            for (idx = 0; idx < g_GameManager.livesRemaining; idx++)
            {
                lifeVm->pos = D3DXVECTOR3(GAME_REGION_LEFT + starStep / 2.0f + idx * starStep,
                                          (f32)(GAME_WINDOW_HEIGHT + 80), 0.49f);
                g_AnmManager->DrawNoRotation(lifeVm);
            }
            if (g_IsOnline)
            {
                for (idx = 0; idx < g_P2LivesRemaining; idx++)
                {
                    lifeVm->pos = D3DXVECTOR3((f32)GAME_REGION_RIGHT - starStep / 2.0f - idx * starStep,
                                              (f32)(GAME_WINDOW_HEIGHT + 80), 0.49f);
                    g_AnmManager->DrawNoRotation(lifeVm);
                }
            }
            lifeVm->pos = savedPos;
            lifeVm->flags.anchor = savedAnchor;
        }

        if (g_Gui.impl->vms[8].sprite != NULL)
        {
            AnmVm *borderVm = &g_Gui.impl->vms[8];
            D3DXVECTOR3 savedBorderPos = borderVm->pos;
            u32 savedBorderAnchor = borderVm->flags.anchor;
            borderVm->flags.anchor = AnmVmAnchor_TopLeft;
            for (f32 bx = 32.0f; bx < 416.0f; bx += 32.0f)
            {
                borderVm->pos = D3DXVECTOR3(bx, (f32)(GAME_WINDOW_HEIGHT + 96), 0.49f);
                g_AnmManager->DrawNoRotation(borderVm);
            }
            borderVm->pos = savedBorderPos;
            borderVm->flags.anchor = savedBorderAnchor;
        }

        // sprite id=8 (y=208 in texture) is the "得点" label image.
        {
            static const f32 scoreLabelW = 32.0f;
            AnmLoadedSprite *scoreLabelSprite = &g_AnmManager->sprites[8 + ANM_OFFSET_FRONT];

            AnmVm *sv = &g_AsciiManager.vm0;
            u32 savedAnchorSv = sv->flags.anchor;
            u32 savedVisibleSv = sv->flags.isVisible;
            D3DXVECTOR3 savedPosSv = sv->pos;
            f32 savedScaleXSv = sv->scaleX;
            f32 savedScaleYSv = sv->scaleY;
            ZunColor savedColorSv = sv->color;
            AnmLoadedSprite *savedSpriteSv = sv->sprite;

            sv->flags.anchor = AnmVmAnchor_TopLeft;
            sv->flags.isVisible = 1;
            sv->flags.flag1 = 1;
            sv->scaleX = 1.0f;
            sv->scaleY = 1.0f;
            sv->color = 0xFFFFFFFF;

            f32 scoreCharW = 14.0f;
            char p1scoreBuf[16];
            sprintf(p1scoreBuf, "%d", g_GameManager.guiScore);

            sv->sprite = scoreLabelSprite;
            sv->pos = D3DXVECTOR3(GAME_REGION_LEFT, (f32)(GAME_WINDOW_HEIGHT + 96), 0.49f);
            g_AnmManager->DrawNoRotation(sv);
            sv->pos.x += scoreLabelW;
            for (const char *ch = p1scoreBuf; *ch != '\0'; ch++)
            {
                if (*ch != ' ')
                {
                    sv->sprite = &g_AnmManager->sprites[(unsigned char)*ch - 0x15];
                    g_AnmManager->DrawNoRotation(sv);
                }
                sv->pos.x += scoreCharW;
            }

            {
                char p2scoreBuf[16];
                sprintf(p2scoreBuf, "%d", g_IsOnline ? g_P2Score : 0);
                i32 p2slen = 0;
                for (const char *ch = p2scoreBuf; *ch != '\0'; ch++)
                    p2slen++;
                sv->sprite = scoreLabelSprite;
                sv->pos = D3DXVECTOR3((f32)GAME_REGION_RIGHT - scoreLabelW, (f32)(GAME_WINDOW_HEIGHT + 96), 0.49f);
                g_AnmManager->DrawNoRotation(sv);
                sv->pos.x = (f32)GAME_REGION_RIGHT - scoreLabelW - p2slen * scoreCharW;
                for (const char *ch = p2scoreBuf; *ch != '\0'; ch++)
                {
                    if (*ch != ' ')
                    {
                        sv->sprite = &g_AnmManager->sprites[(unsigned char)*ch - 0x15];
                        g_AnmManager->DrawNoRotation(sv);
                    }
                    sv->pos.x += scoreCharW;
                }
            }

            sv->flags.anchor = savedAnchorSv;
            sv->flags.isVisible = savedVisibleSv;
            sv->pos = savedPosSv;
            sv->scaleX = savedScaleXSv;
            sv->scaleY = savedScaleYSv;
            sv->color = savedColorSv;
            sv->sprite = savedSpriteSv;
        }

        {
            static const f32 grazeLabelW = 48.0f;
            AnmLoadedSprite *grazeLabelSprite = &g_AnmManager->sprites[16 + ANM_OFFSET_FRONT];

            AnmVm *av = &g_AsciiManager.vm0;
            u32 savedAnchorAv = av->flags.anchor;
            u32 savedVisibleAv = av->flags.isVisible;
            D3DXVECTOR3 savedPosAv = av->pos;
            f32 savedScaleXAv = av->scaleX;
            f32 savedScaleYAv = av->scaleY;
            ZunColor savedColorAv = av->color;
            AnmLoadedSprite *savedSpriteAv = av->sprite;

            av->flags.anchor = AnmVmAnchor_TopLeft;
            av->flags.isVisible = 1;
            av->flags.flag1 = 1;
            av->scaleX = 1.0f;
            av->scaleY = 1.0f;
            av->color = 0xFFFFFFFF;

            f32 charWidth = 14.0f;

            av->sprite = grazeLabelSprite;
            av->pos = D3DXVECTOR3(GAME_REGION_LEFT, (f32)(GAME_WINDOW_HEIGHT + 48), 0.49f);
            g_AnmManager->DrawNoRotation(av);
            av->pos.x += grazeLabelW;
            for (const char *ch = grazeBuf; *ch != '\0'; ch++)
            {
                if (*ch != ' ')
                {
                    av->sprite = &g_AnmManager->sprites[(unsigned char)*ch - 0x15];
                    g_AnmManager->DrawNoRotation(av);
                }
                av->pos.x += charWidth;
            }

            {
                char p2grazeBuf[16];
                sprintf(p2grazeBuf, "%d", g_IsOnline ? g_P2GrazeInStage : 0);
                i32 p2nlen = 0;
                for (const char *ch = p2grazeBuf; *ch != '\0'; ch++)
                    p2nlen++;
                av->sprite = grazeLabelSprite;
                av->pos = D3DXVECTOR3((f32)GAME_REGION_RIGHT - grazeLabelW, (f32)(GAME_WINDOW_HEIGHT + 48), 0.49f);
                g_AnmManager->DrawNoRotation(av);
                av->pos.x = (f32)GAME_REGION_RIGHT - grazeLabelW - p2nlen * charWidth;
                for (const char *ch = p2grazeBuf; *ch != '\0'; ch++)
                {
                    if (*ch != ' ')
                    {
                        av->sprite = &g_AnmManager->sprites[(unsigned char)*ch - 0x15];
                        g_AnmManager->DrawNoRotation(av);
                    }
                    av->pos.x += charWidth;
                }
            }

            av->flags.anchor = savedAnchorAv;
            av->flags.isVisible = savedVisibleAv;
            av->pos = savedPosAv;
            av->scaleX = savedScaleXAv;
            av->scaleY = savedScaleYAv;
            av->color = savedColorAv;
            av->sprite = savedSpriteAv;
        }

        if (g_Gui.impl->vms[8].sprite != NULL)
        {
            AnmVm *borderVm = &g_Gui.impl->vms[8];
            D3DXVECTOR3 savedBorderPos = borderVm->pos;
            u32 savedBorderAnchor = borderVm->flags.anchor;
            borderVm->flags.anchor = AnmVmAnchor_TopLeft;
            for (f32 bx = 32.0f; bx < 416.0f; bx += 32.0f)
            {
                borderVm->pos = D3DXVECTOR3(bx, (f32)(GAME_WINDOW_HEIGHT + 64), 0.49f);
                g_AnmManager->DrawNoRotation(borderVm);
            }
            borderVm->pos = savedBorderPos;
            borderVm->flags.anchor = savedBorderAnchor;
        }

        {
            static const f32 powerLabelW = 48.0f;
            AnmLoadedSprite *powerLabelSprite = &g_AnmManager->sprites[15 + ANM_OFFSET_FRONT];

            AnmVm *pwr = &g_AsciiManager.vm0;
            u32 savedAnchorPwr = pwr->flags.anchor;
            u32 savedVisiblePwr = pwr->flags.isVisible;
            D3DXVECTOR3 savedPosPwr = pwr->pos;
            f32 savedScaleXPwr = pwr->scaleX;
            f32 savedScaleYPwr = pwr->scaleY;
            ZunColor savedColorPwr = pwr->color;
            AnmLoadedSprite *savedSpritePwr = pwr->sprite;

            pwr->flags.anchor = AnmVmAnchor_TopLeft;
            pwr->flags.isVisible = 1;
            pwr->flags.flag1 = 1;
            pwr->scaleX = 1.0f;
            pwr->scaleY = 1.0f;
            pwr->color = 0xFFFFFFFF;

            f32 pwrCharW = 14.0f;
            char p1pwrBuf[16];
            if (g_GameManager.currentPower >= 128)
                strcpy(p1pwrBuf, "MAX");
            else
                sprintf(p1pwrBuf, "%d", g_GameManager.currentPower);

            pwr->sprite = powerLabelSprite;
            pwr->pos = D3DXVECTOR3(GAME_REGION_LEFT, (f32)(GAME_WINDOW_HEIGHT + 64), 0.49f);
            g_AnmManager->DrawNoRotation(pwr);
            pwr->pos.x += powerLabelW;
            for (const char *ch = p1pwrBuf; *ch != '\0'; ch++)
            {
                if (*ch != ' ')
                {
                    pwr->sprite = &g_AnmManager->sprites[(unsigned char)*ch - 0x15];
                    g_AnmManager->DrawNoRotation(pwr);
                }
                pwr->pos.x += pwrCharW;
            }

            {
                char p2pwrBuf[16];
                i32 p2PowerVal = g_IsOnline ? (i32)g_P2Power : 0;
                if (p2PowerVal >= 128)
                    strcpy(p2pwrBuf, "MAX");
                else
                    sprintf(p2pwrBuf, "%d", p2PowerVal);
                i32 p2pwrlen = 0;
                for (const char *ch = p2pwrBuf; *ch != '\0'; ch++)
                    p2pwrlen++;
                pwr->sprite = powerLabelSprite;
                pwr->pos = D3DXVECTOR3((f32)GAME_REGION_RIGHT - powerLabelW, (f32)(GAME_WINDOW_HEIGHT + 64), 0.49f);
                g_AnmManager->DrawNoRotation(pwr);
                pwr->pos.x = (f32)GAME_REGION_RIGHT - powerLabelW - p2pwrlen * pwrCharW;
                for (const char *ch = p2pwrBuf; *ch != '\0'; ch++)
                {
                    if (*ch != ' ')
                    {
                        pwr->sprite = &g_AnmManager->sprites[(unsigned char)*ch - 0x15];
                        g_AnmManager->DrawNoRotation(pwr);
                    }
                    pwr->pos.x += pwrCharW;
                }
            }

            pwr->flags.anchor = savedAnchorPwr;
            pwr->flags.isVisible = savedVisiblePwr;
            pwr->pos = savedPosPwr;
            pwr->scaleX = savedScaleXPwr;
            pwr->scaleY = savedScaleYPwr;
            pwr->color = savedColorPwr;
            pwr->sprite = savedSpritePwr;
        }

        if (g_Gui.impl->vms[8].sprite != NULL)
        {
            AnmVm *borderVm = &g_Gui.impl->vms[8];
            D3DXVECTOR3 savedBorderPos = borderVm->pos;
            u32 savedBorderAnchor = borderVm->flags.anchor;
            borderVm->flags.anchor = AnmVmAnchor_TopLeft;
            for (f32 bx = 32.0f; bx < 416.0f; bx += 32.0f)
            {
                borderVm->pos = D3DXVECTOR3(bx, (f32)(GAME_WINDOW_HEIGHT + 112), 0.49f);
                g_AnmManager->DrawNoRotation(borderVm);
            }
            borderVm->pos = savedBorderPos;
            borderVm->flags.anchor = savedBorderAnchor;
        }

        {
            static const f32 hiScoreLabelW = 64.0f;
            AnmLoadedSprite *hiScoreLabelSprite = &g_AnmManager->sprites[9 + ANM_OFFSET_FRONT];

            AnmVm *sv = &g_AsciiManager.vm0;
            u32 savedAnchorSv = sv->flags.anchor;
            u32 savedVisibleSv = sv->flags.isVisible;
            D3DXVECTOR3 savedPosSv = sv->pos;
            f32 savedScaleXSv = sv->scaleX;
            f32 savedScaleYSv = sv->scaleY;
            ZunColor savedColorSv = sv->color;
            AnmLoadedSprite *savedSpriteSv = sv->sprite;

            sv->flags.anchor = AnmVmAnchor_TopLeft;
            sv->flags.isVisible = 1;
            sv->flags.flag1 = 1;
            sv->scaleX = 1.0f;
            sv->scaleY = 1.0f;
            sv->color = 0xFFFFFFFF;

            f32 bsCharW = 14.0f;
            char p1bsBuf[16];
            sprintf(p1bsBuf, "%d", g_GameManager.highScore);

            sv->sprite = hiScoreLabelSprite;
            sv->pos = D3DXVECTOR3(GAME_REGION_LEFT, (f32)(GAME_WINDOW_HEIGHT + 112), 0.49f);
            g_AnmManager->DrawNoRotation(sv);
            sv->pos.x += hiScoreLabelW;
            for (const char *ch = p1bsBuf; *ch != '\0'; ch++)
            {
                if (*ch != ' ')
                {
                    sv->sprite = &g_AnmManager->sprites[(unsigned char)*ch - 0x15];
                    g_AnmManager->DrawNoRotation(sv);
                }
                sv->pos.x += bsCharW;
            }

            {
                char p2bsBuf[16];
                sprintf(p2bsBuf, "%d", g_IsOnline ? g_P2HighScore : 0);
                i32 p2bslen = 0;
                for (const char *ch = p2bsBuf; *ch != '\0'; ch++)
                    p2bslen++;
                sv->sprite = hiScoreLabelSprite;
                sv->pos = D3DXVECTOR3((f32)GAME_REGION_RIGHT - hiScoreLabelW, (f32)(GAME_WINDOW_HEIGHT + 112), 0.49f);
                g_AnmManager->DrawNoRotation(sv);
                sv->pos.x = (f32)GAME_REGION_RIGHT - hiScoreLabelW - p2bslen * bsCharW;
                for (const char *ch = p2bsBuf; *ch != '\0'; ch++)
                {
                    if (*ch != ' ')
                    {
                        sv->sprite = &g_AnmManager->sprites[(unsigned char)*ch - 0x15];
                        g_AnmManager->DrawNoRotation(sv);
                    }
                    sv->pos.x += bsCharW;
                }
            }

            sv->flags.anchor = savedAnchorSv;
            sv->flags.isVisible = savedVisibleSv;
            sv->pos = savedPosSv;
            sv->scaleX = savedScaleXSv;
            sv->scaleY = savedScaleYSv;
            sv->color = savedColorSv;
            sv->sprite = savedSpriteSv;
        }

        g_Supervisor.d3dDevice->SetViewport(&g_Supervisor.viewport);
    }
}

ChainCallbackResult EnemyManager::OnDraw(EnemyManager *mgr)
{
    AnmVm *curEnemyVm;
    Enemy *curEnemy;
    i32 curEnemyVmIdx;
    i32 curEnemyIdx;

    for (curEnemy = &mgr->enemies[0], curEnemyIdx = 0; curEnemyIdx < ARRAY_SIZE_SIGNED(mgr->enemies) - 1;
         curEnemyIdx++, curEnemy++)
    {
        if (!curEnemy->flags.unk5)
        {
            continue;
        }
        if (curEnemy->flags.unk15)
        {
            continue;
        }

        for (curEnemyVm = &curEnemy->vms[0], curEnemyVmIdx = 0; curEnemyVmIdx < 4; curEnemyVmIdx++, curEnemyVm++)
        {
            if (curEnemyVm->anmFileIndex >= 0)
            {
                if (curEnemyVm->autoRotate != 0)
                {
                    curEnemyVm->rotation.z = curEnemy->angle;
                }
                curEnemyVm->pos = curEnemy->position + curEnemyVm->posOffset;
                curEnemyVm->pos.z = 0.495f;
                g_AnmManager->Draw2(curEnemyVm);
            }
        }
        if (curEnemy->flags.unk13 != 0)
        {
            curEnemy->primaryVm.rotation.z = curEnemy->angle;
        }
        curEnemy->primaryVm.pos = curEnemy->position + curEnemy->primaryVm.posOffset;
        curEnemy->primaryVm.pos.z = 0.494f;
        g_AnmManager->Draw2(&curEnemy->primaryVm);
        for (curEnemyVmIdx = 4; curEnemyVmIdx < 8; curEnemyVmIdx++, curEnemyVm++)
        {
            if (curEnemyVm->anmFileIndex >= 0)
            {
                if (curEnemyVm->autoRotate != 0)
                {
                    curEnemyVm->rotation.z = curEnemy->angle;
                }
                curEnemyVm->pos = curEnemy->position + curEnemyVm->posOffset;
                curEnemyVm->pos.z = 0.495f;
                g_AnmManager->Draw2(curEnemyVm);
            }
        }
    }

    if (g_BossHpCircle && g_Gui.bossPresent && g_Gui.bossUIOpacity > 0 && !g_Gui.HasCurrentMsgIdx())
    {
        for (curEnemyIdx = 0; curEnemyIdx < 8; curEnemyIdx++)
        {
            Enemy *boss = mgr->bosses[curEnemyIdx];
            if (boss != NULL && boss->flags.unk5 && boss->flags.isBoss)
            {
                DrawBossHpCircle(boss);
            }
        }
    }

    return CHAIN_CALLBACK_RESULT_CONTINUE;
}

ZunResult EnemyManager::AddedCallback(EnemyManager *enemyManager)
{
    Enemy *enemies = enemyManager->enemies;

    if (enemyManager->stgEnmAnmFilename &&
        g_AnmManager->LoadAnm(ANM_FILE_ENEMY, enemyManager->stgEnmAnmFilename, ANM_OFFSET_ENEMY) != ZUN_SUCCESS)
    {
        return ZUN_ERROR;
    }
    if (enemyManager->stgEnm2AnmFilename &&
        g_AnmManager->LoadAnm(ANM_FILE_ENEMY2, enemyManager->stgEnm2AnmFilename, ANM_OFFSET_ENEMY) != ZUN_SUCCESS)
    {
        return ZUN_ERROR;
    }

    enemyManager->randomItemSpawnIndex = g_Rng.GetRandomU16InRange(ITEM_SPAWNS);
    enemyManager->randomItemTableIndex = g_Rng.GetRandomU16InRange(ITEM_TABLES);

    enemyManager->spellcardInfo.isActive = 0;
    enemyManager->timelineInstr = NULL;

    return ZUN_SUCCESS;
}

ZunResult EnemyManager::DeletedCallback(EnemyManager *mgr)
{
    g_AnmManager->ReleaseAnm(ANM_FILE_ENEMY2);
    g_AnmManager->ReleaseAnm(ANM_FILE_ENEMY);
    return ZUN_SUCCESS;
}

void EnemyManager::CutChain()
{
    g_Chain.Cut(&g_EnemyManagerCalcChain);
    g_Chain.Cut(&g_EnemyManagerDrawChain);
    return;
}

void Enemy::Move()
{
    if (!this->flags.unk4)
    {
        this->position.x += g_Supervisor.effectiveFramerateMultiplier * this->axisSpeed.x;
    }
    else
    {
        this->position.x -= g_Supervisor.effectiveFramerateMultiplier * this->axisSpeed.x;
    }
    this->position.y += g_Supervisor.effectiveFramerateMultiplier * this->axisSpeed.y;
    this->position.z += g_Supervisor.effectiveFramerateMultiplier * this->axisSpeed.z;
}
}; // namespace th06
