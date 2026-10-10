#include <Windows.h>
#include <cstdio>
#include <cstring>

#include "SpellPracticeDllApi.hpp"
#include "inttypes.hpp"

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

// Section indices match thprac's th_sections_t enum (1-based, generated from thprac_games_def.json)
enum TH06Section
{
    TH06_ST1_MID1 = 1,
    TH06_ST1_MID2 = 2,
    TH06_ST1_BOSS1 = 3,
    TH06_ST1_BOSS2 = 4,
    TH06_ST1_BOSS3 = 5,
    TH06_ST1_BOSS4 = 6,
    TH06_ST2_MID1 = 7,
    TH06_ST2_BOSS1 = 8,
    TH06_ST2_BOSS2 = 9,
    TH06_ST2_BOSS3 = 10,
    TH06_ST2_BOSS4 = 11,
    TH06_ST2_BOSS5 = 12,
    TH06_ST3_MID1 = 13,
    TH06_ST3_MID2 = 14,
    TH06_ST3_BOSS1 = 15,
    TH06_ST3_BOSS2 = 16,
    TH06_ST3_BOSS3 = 17,
    TH06_ST3_BOSS4 = 18,
    TH06_ST3_BOSS5 = 19,
    TH06_ST3_BOSS6 = 20,
    TH06_ST3_BOSS7 = 21,
    TH06_ST4_BOOKS = 22,
    TH06_ST4_MID1 = 23,
    TH06_ST4_BOSS1 = 24,
    TH06_ST4_BOSS2 = 25,
    TH06_ST4_BOSS3 = 26,
    TH06_ST4_BOSS4 = 27,
    TH06_ST4_BOSS5 = 28,
    TH06_ST4_BOSS6 = 29,
    TH06_ST4_BOSS7 = 30,
    TH06_ST5_MID1 = 31,
    TH06_ST5_MID2 = 32,
    TH06_ST5_BOSS1 = 33,
    TH06_ST5_BOSS2 = 34,
    TH06_ST5_BOSS3 = 35,
    TH06_ST5_BOSS4 = 36,
    TH06_ST5_BOSS5 = 37,
    TH06_ST5_BOSS6 = 38,
    TH06_ST6_MID1 = 39,
    TH06_ST6_MID2 = 40,
    TH06_ST6_BOSS1 = 41,
    TH06_ST6_BOSS2 = 42,
    TH06_ST6_BOSS3 = 43,
    TH06_ST6_BOSS4 = 44,
    TH06_ST6_BOSS5 = 45,
    TH06_ST6_BOSS6 = 46,
    TH06_ST6_BOSS7 = 47,
    TH06_ST6_BOSS8 = 48,
    TH06_ST6_BOSS9 = 49,
    TH06_ST7_MID1 = 50,
    TH06_ST7_MID2 = 51,
    TH06_ST7_MID3 = 52,
    TH06_ST7_END_NS1 = 53,
    TH06_ST7_END_S1 = 54,
    TH06_ST7_END_NS2 = 55,
    TH06_ST7_END_S2 = 56,
    TH06_ST7_END_NS3 = 57,
    TH06_ST7_END_S3 = 58,
    TH06_ST7_END_NS4 = 59,
    TH06_ST7_END_S4 = 60,
    TH06_ST7_END_NS5 = 61,
    TH06_ST7_END_S5 = 62,
    TH06_ST7_END_NS6 = 63,
    TH06_ST7_END_S6 = 64,
    TH06_ST7_END_NS7 = 65,
    TH06_ST7_END_S7 = 66,
    TH06_ST7_END_NS8 = 67,
    TH06_ST7_END_S8 = 68,
    TH06_ST7_END_S9 = 69,
    TH06_ST7_END_S10 = 70,
    // Stage 4 route-specific (thprac section indices 71-106)
    TH06_ST4_RA_BOOKS = 71,
    TH06_ST4_RA_MID1 = 72,
    TH06_ST4_RA_BOSS1 = 73,
    TH06_ST4_RA_BOSS2 = 74,
    TH06_ST4_RA_BOSS3 = 75,
    TH06_ST4_RA_BOSS4 = 76,
    TH06_ST4_RA_BOSS5 = 77,
    TH06_ST4_RA_BOSS6 = 78,
    TH06_ST4_RA_BOSS7 = 79,
    TH06_ST4_RB_BOOKS = 80,
    TH06_ST4_RB_MID1 = 81,
    TH06_ST4_RB_BOSS1 = 82,
    TH06_ST4_RB_BOSS2 = 83,
    TH06_ST4_RB_BOSS3 = 84,
    TH06_ST4_RB_BOSS4 = 85,
    TH06_ST4_RB_BOSS5 = 86,
    TH06_ST4_RB_BOSS6 = 87,
    TH06_ST4_RB_BOSS7 = 88,
    TH06_ST4_MA_BOOKS = 89,
    TH06_ST4_MA_MID1 = 90,
    TH06_ST4_MA_BOSS1 = 91,
    TH06_ST4_MA_BOSS2 = 92,
    TH06_ST4_MA_BOSS3 = 93,
    TH06_ST4_MA_BOSS4 = 94,
    TH06_ST4_MA_BOSS5 = 95,
    TH06_ST4_MA_BOSS6 = 96,
    TH06_ST4_MA_BOSS7 = 97,
    TH06_ST4_MB_BOOKS = 98,
    TH06_ST4_MB_MID1 = 99,
    TH06_ST4_MB_BOSS1 = 100,
    TH06_ST4_MB_BOSS2 = 101,
    TH06_ST4_MB_BOSS3 = 102,
    TH06_ST4_MB_BOSS4 = 103,
    TH06_ST4_MB_BOSS5 = 104,
    TH06_ST4_MB_BOSS6 = 105,
    TH06_ST4_MB_BOSS7 = 106,
    // Full boss-run entries (start from BOSS1, no forced end until stage clear)
    TH06_ST1_BOSS_ALL = 107,
    TH06_ST2_BOSS_ALL = 108,
    TH06_ST3_BOSS_ALL = 109,
    TH06_ST4_RA_BOSS_ALL = 110,
    TH06_ST4_RB_BOSS_ALL = 111,
    TH06_ST4_MA_BOSS_ALL = 112,
    TH06_ST4_MB_BOSS_ALL = 113,
    TH06_ST5_BOSS_ALL = 114,
    TH06_ST6_BOSS_ALL = 115,
    TH06_ST7_BOSS_ALL = 116,
};

namespace
{
const int SPELL_VIEW_ROWS = 12;

// diffMask: bit0=Easy, bit1=Normal, bit2=Hard, bit3=Lunatic, bit4=Extra
// routeMask: bit0=ReimuA, bit1=ReimuB, bit2=MarisaA, bit3=MarisaB
// catkIdx: index into g_GameManager.catk[], -1 for non-spell sections
struct SpellDef
{
    int section; // TH06Section value
    int stage;   // 1-7
    u8 diffMask;
    u8 routeMask;
    i8 catkIdx;
    const char *name;
};

// Spell definitions: section, stage, diffMask, routeMask, name (Shift-JIS)
static const SpellDef kSpellDefs[] = {
    // Stage 1
    {TH06_ST1_MID1, 1, 0x0F, 0xF, -1, "\x93\xB9\x92\x86\x20\x92\xCA\x8F\xED\x31"},
    {TH06_ST1_MID2, 1, 0x0C, 0xF, 0,
     "\x8C\x8E\x95\x84\x81\x75\x83\x80\x81\x5B\x83\x93\x83\x89\x83\x43\x83\x67\x83\x8C\x83\x43\x81\x76"},
    {TH06_ST1_BOSS_ALL, 1, 0x0F, 0xF, -1, "\x83\x7B\x83\x58\x90\xED\x92\xCA\x82\xB5"},
    {TH06_ST1_BOSS1, 1, 0x0F, 0xF, -1, "\x83\x7B\x83\x58\x20\x92\xCA\x8F\xED\x31"},
    {TH06_ST1_BOSS2, 1, 0x0E, 0xF, 1,
     "\x96\xE9\x95\x84\x81\x75\x83\x69\x83\x43\x83\x67\x83\x6F\x81\x5B\x83\x68\x81\x76"},
    {TH06_ST1_BOSS3, 1, 0x0F, 0xF, -1, "\x83\x7B\x83\x58\x20\x92\xCA\x8F\xED\x32"},
    {TH06_ST1_BOSS4, 1, 0x0F, 0xF, 2,
     "\x88\xC5\x95\x84\x81\x75\x83\x66\x83\x42\x83\x7D\x81\x5B\x83\x50\x83\x43\x83\x56\x83\x87\x83\x93\x81\x76"},
    // Stage 2
    {TH06_ST2_MID1, 2, 0x0F, 0xF, -1, "\x93\xB9\x92\x86\x20\x92\xCA\x8F\xED\x31"},
    {TH06_ST2_BOSS_ALL, 2, 0x0F, 0xF, -1, "\x83\x7B\x83\x58\x90\xED\x92\xCA\x82\xB5"},
    {TH06_ST2_BOSS1, 2, 0x0F, 0xF, -1, "\x83\x7B\x83\x58\x20\x92\xCA\x8F\xED\x31"},
    {TH06_ST2_BOSS2, 2, 0x03, 0xF, 3,
     "\x95\x58\x95\x84\x81\x75\x83\x41\x83\x43\x83\x56\x83\x4E\x83\x8B\x83\x74\x83\x48\x81\x5B\x83\x8B\x81\x76"},
    {TH06_ST2_BOSS2, 2, 0x0C, 0xF, 4,
     "\xE8\xB9\x95\x84\x81\x75\x83\x77\x83\x43\x83\x8B\x83\x58\x83\x67\x81\x5B\x83\x80\x81\x76"},
    {TH06_ST2_BOSS3, 2, 0x0F, 0xF, -1, "\x83\x7B\x83\x58\x20\x92\xCA\x8F\xED\x32"},
    {TH06_ST2_BOSS4, 2, 0x0F, 0xF, 5,
     "\x93\x80\x95\x84\x81\x75\x83\x70\x81\x5B\x83\x74\x83\x46\x83\x4E\x83\x67\x83\x74\x83\x8A\x81\x5B\x83\x59\x81"
     "\x76"},
    {TH06_ST2_BOSS5, 2, 0x0E, 0xF, 6,
     "\x90\xE1\x95\x84\x81\x75\x83\x5F\x83\x43\x83\x41\x83\x82\x83\x93\x83\x68\x83\x75\x83\x8A\x83\x55\x81\x5B\x83\x68"
     "\x81\x76"},
    // Stage 3
    {TH06_ST3_MID1, 3, 0x0F, 0xF, -1, "\x93\xB9\x92\x86\x20\x92\xCA\x8F\xED\x31"},
    {TH06_ST3_MID2, 3, 0x03, 0xF, 7, "\x89\xD8\x95\x84\x81\x75\x96\x46\x89\xD8\x88\xBA\xE0\xA3\x81\x76"},
    {TH06_ST3_MID2, 3, 0x0C, 0xF, 8,
     "\x89\xD8\x95\x84\x81\x75\x83\x5A\x83\x89\x83\x4D\x83\x6C\x83\x89\x82\x58\x81\x76"},
    {TH06_ST3_BOSS_ALL, 3, 0x0F, 0xF, -1, "\x83\x7B\x83\x58\x90\xED\x92\xCA\x82\xB5"},
    {TH06_ST3_BOSS1, 3, 0x0F, 0xF, -1, "\x83\x7B\x83\x58\x20\x92\xCA\x8F\xED\x31"},
    {TH06_ST3_BOSS2, 3, 0x0F, 0xF, 9, "\x93\xF8\x95\x84\x81\x75\x8D\xCA\x93\xF8\x82\xCC\x95\x97\x97\xE9\x81\x76"},
    {TH06_ST3_BOSS3, 3, 0x0F, 0xF, -1, "\x83\x7B\x83\x58\x20\x92\xCA\x8F\xED\x32"},
    {TH06_ST3_BOSS4, 3, 0x0C, 0xF, 10, "\x8C\xB6\x95\x84\x81\x75\x89\xD8\x91\x7A\x96\xB2\x8A\x8B\x81\x76"},
    {TH06_ST3_BOSS5, 3, 0x0F, 0xF, -1, "\x83\x7B\x83\x58\x20\x92\xCA\x8F\xED\x33"},
    {TH06_ST3_BOSS6, 3, 0x03, 0xF, 11, "\x8D\xCA\x95\x84\x81\x75\x8D\xCA\x89\x4A\x81\x76"},
    {TH06_ST3_BOSS6, 3, 0x0C, 0xF, 12, "\x8D\xCA\x95\x84\x81\x75\x8D\xCA\x8C\xF5\x97\x90\x95\x91\x81\x76"},
    {TH06_ST3_BOSS7, 3, 0x0E, 0xF, 13, "\x8D\xCA\x95\x84\x81\x75\x8B\xC9\x8D\xCA\xE9\x45\x95\x97\x81\x76"},
    // Stage 4: route-neutral sections (Books, Mid, BOSS_ALL per route, NS1)
    {TH06_ST4_BOOKS, 4, 0x0F, 0xF, -1, "\x96\x82\x93\xB1\x8F\x91"},
    {TH06_ST4_MID1, 4, 0x0F, 0xF, -1, "\x93\xB9\x92\x86\x20\x92\xCA\x8F\xED\x31"},
    {TH06_ST4_RA_BOSS_ALL, 4, 0x0F, 0x1, -1, "\x83\x7B\x83\x58\x90\xED\x92\xCA\x82\xB5"},
    {TH06_ST4_RB_BOSS_ALL, 4, 0x0F, 0x2, -1, "\x83\x7B\x83\x58\x90\xED\x92\xCA\x82\xB5"},
    {TH06_ST4_MA_BOSS_ALL, 4, 0x0F, 0x4, -1, "\x83\x7B\x83\x58\x90\xED\x92\xCA\x82\xB5"},
    {TH06_ST4_MB_BOSS_ALL, 4, 0x0F, 0x8, -1, "\x83\x7B\x83\x58\x90\xED\x92\xCA\x82\xB5"},
    {TH06_ST4_BOSS1, 4, 0x0F, 0xF, -1, "\x83\x7B\x83\x58\x20\x92\xCA\x8F\xED\x31"},
    // Stage 4: ReimuA (routeMask=0x1)
    {TH06_ST4_RA_BOSS4, 4, 0x0F, 0x1, -1, "\x83\x7B\x83\x58\x20\x92\xCA\x8F\xED\x32"},
    // BOSS2: catk[14]=Fire(N), catk[19]=Fire上級(HL)
    {TH06_ST4_RA_BOSS2, 4, 0x02, 0x1, 14,
     "\x89\xCE\x95\x84\x81\x75\x83\x41\x83\x4F\x83\x6A\x83\x56\x83\x83\x83\x43\x83\x93\x81\x76"},
    {TH06_ST4_RA_BOSS2, 4, 0x0C, 0x1, 19,
     "\x89\xCE\x95\x84\x81\x75\x83\x41\x83\x4F\x83\x6A\x83\x56\x83\x83\x83\x43\x83\x93\x8F\xE3\x8B\x89\x81\x76"},
    // BOSS3: catk[14]=Fire(E), catk[17]=Earth上級(N), catk[21]=Earth上級(HL)
    {TH06_ST4_RA_BOSS3, 4, 0x01, 0x1, 14,
     "\x89\xCE\x95\x84\x81\x75\x83\x41\x83\x4F\x83\x6A\x83\x56\x83\x83\x83\x43\x83\x93\x81\x76"},
    {TH06_ST4_RA_BOSS3, 4, 0x02, 0x1, 17,
     "\x93\x79\x95\x84\x81\x75\x83\x8C\x83\x43\x83\x57\x83\x42\x83\x67\x83\x8A\x83\x8A\x83\x67\x83\x93\x8F\xE3\x8B\x89"
     "\x81\x76"},
    {TH06_ST4_RA_BOSS3, 4, 0x0C, 0x1, 21,
     "\x93\x79\x95\x84\x81\x75\x83\x67\x83\x8A\x83\x8A\x83\x67\x83\x93\x83\x56\x83\x46\x83\x43\x83\x4E\x81\x76"},
    {TH06_ST4_RA_BOSS5, 4, 0x0F, 0x1, 27,
     "\x89\xCE\x81\x95\x93\x79\x95\x84\x81\x75\x83\x89\x81\x5B\x83\x94\x83\x40\x83\x4E\x83\x8D\x83\x80\x83\x8C\x83\x4E"
     "\x81\x76"},
    {TH06_ST4_RA_BOSS6, 4, 0x0E, 0x1, 30,
     "\x8B\xE0\x81\x95\x90\x85\x95\x84\x81\x75\x83\x7D\x81\x5B\x83\x4C\x83\x85\x83\x8A\x83\x7C\x83\x43\x83\x59\x83\x93"
     "\x81\x76"},
    {TH06_ST4_RA_BOSS7, 4, 0x0C, 0x1, 28,
     "\x96\xD8\x81\x95\x89\xCE\x95\x84\x81\x75\x83\x74\x83\x48\x83\x8C\x83\x58\x83\x67\x83\x75\x83\x8C\x83\x43\x83\x59"
     "\x81\x76"},
    // Stage 4: ReimuB (routeMask=0x2)
    {TH06_ST4_RB_BOSS4, 4, 0x0F, 0x2, -1, "\x83\x7B\x83\x58\x20\x92\xCA\x8F\xED\x32"},
    // BOSS2: catk[15]=Water(N), catk[23]=Water2(HL)
    {TH06_ST4_RB_BOSS2, 4, 0x02, 0x2, 15,
     "\x90\x85\x95\x84\x81\x75\x83\x76\x83\x8A\x83\x93\x83\x5A\x83\x58\x83\x45\x83\x93\x83\x66\x83\x42\x83\x6C\x81"
     "\x76"},
    {TH06_ST4_RB_BOSS2, 4, 0x0C, 0x2, 23,
     "\x90\x85\x95\x84\x81\x75\x83\x78\x83\x8A\x81\x5B\x83\x43\x83\x93\x83\x8C\x83\x43\x83\x4E\x81\x76"},
    // BOSS3: catk[15]=Water(E), catk[16]=Wood上級(N), catk[20]=Wood上級(HL)
    {TH06_ST4_RB_BOSS3, 4, 0x01, 0x2, 15,
     "\x90\x85\x95\x84\x81\x75\x83\x76\x83\x8A\x83\x93\x83\x5A\x83\x58\x83\x45\x83\x93\x83\x66\x83\x42\x83\x6C\x81"
     "\x76"},
    {TH06_ST4_RB_BOSS3, 4, 0x02, 0x2, 16,
     "\x96\xD8\x95\x84\x81\x75\x83\x56\x83\x8B\x83\x74\x83\x42\x83\x7A\x83\x8B\x83\x93\x8F\xE3\x8B\x89\x81\x76"},
    {TH06_ST4_RB_BOSS3, 4, 0x0C, 0x2, 20,
     "\x96\xD8\x95\x84\x81\x75\x83\x4F\x83\x8A\x81\x5B\x83\x93\x83\x58\x83\x67\x81\x5B\x83\x80\x81\x76"},
    {TH06_ST4_RB_BOSS5, 4, 0x0F, 0x2, 29,
     "\x90\x85\x81\x95\x96\xD8\x95\x84\x81\x75\x83\x45\x83\x48\x81\x5B\x83\x5E\x81\x5B\x83\x47\x83\x8B\x83\x74\x81"
     "\x76"},
    {TH06_ST4_RB_BOSS6, 4, 0x0E, 0x2, 30,
     "\x8B\xE0\x81\x95\x90\x85\x95\x84\x81\x75\x83\x7D\x81\x5B\x83\x4C\x83\x85\x83\x8A\x83\x7C\x83\x43\x83\x59\x83\x93"
     "\x81\x76"},
    {TH06_ST4_RB_BOSS7, 4, 0x0C, 0x2, 31,
     "\x93\x79\x81\x95\x8B\xE0\x95\x84\x81\x75\x83\x47\x83\x81\x83\x89\x83\x8B\x83\x68\x83\x81\x83\x4B\x83\x8A\x83\x58"
     "\x81\x76"},
    // Stage 4: MarisaA (routeMask=0x4)
    {TH06_ST4_MA_BOSS4, 4, 0x0F, 0x4, -1, "\x83\x7B\x83\x58\x20\x92\xCA\x8F\xED\x32"},
    // BOSS2: catk[16]=Wood(N), catk[20]=Wood上級(HL)
    {TH06_ST4_MA_BOSS2, 4, 0x02, 0x4, 16,
     "\x96\xD8\x95\x84\x81\x75\x83\x56\x83\x8B\x83\x74\x83\x42\x83\x7A\x83\x8B\x83\x93\x81\x76"},
    {TH06_ST4_MA_BOSS2, 4, 0x0C, 0x4, 20,
     "\x96\xD8\x95\x84\x81\x75\x83\x56\x83\x8B\x83\x74\x83\x42\x83\x7A\x83\x8B\x83\x93\x8F\xE3\x8B\x89\x81\x76"},
    // BOSS3: catk[16]=Wood(E), catk[19]=Fire上級(N), catk[22]=Fire3(HL)
    {TH06_ST4_MA_BOSS3, 4, 0x01, 0x4, 16,
     "\x96\xD8\x95\x84\x81\x75\x83\x56\x83\x8B\x83\x74\x83\x42\x83\x7A\x83\x8B\x83\x93\x81\x76"},
    {TH06_ST4_MA_BOSS3, 4, 0x02, 0x4, 19,
     "\x89\xCE\x95\x84\x81\x75\x83\x41\x83\x4F\x83\x6A\x83\x56\x83\x83\x83\x43\x83\x93\x8F\xE3\x8B\x89\x81\x76"},
    {TH06_ST4_MA_BOSS3, 4, 0x0C, 0x4, 22,
     "\x89\xCE\x95\x84\x81\x75\x83\x41\x83\x4F\x83\x6A\x83\x8C\x83\x43\x83\x66\x83\x42\x83\x41\x83\x93\x83\x58\x81"
     "\x76"},
    {TH06_ST4_MA_BOSS5, 4, 0x0F, 0x4, 28,
     "\x96\xD8\x81\x95\x89\xCE\x95\x84\x81\x75\x83\x74\x83\x48\x83\x8C\x83\x58\x83\x67\x83\x75\x83\x8C\x83\x43\x83\x59"
     "\x81\x76"},
    {TH06_ST4_MA_BOSS6, 4, 0x0E, 0x4, 31,
     "\x93\x79\x81\x95\x8B\xE0\x95\x84\x81\x75\x83\x47\x83\x81\x83\x89\x83\x8B\x83\x68\x83\x81\x83\x4B\x83\x8A\x83\x58"
     "\x81\x76"},
    {TH06_ST4_MA_BOSS7, 4, 0x0C, 0x4, 27,
     "\x89\xCE\x81\x95\x93\x79\x95\x84\x81\x75\x83\x89\x81\x5B\x83\x94\x83\x40\x83\x4E\x83\x8D\x83\x80\x83\x8C\x83\x4E"
     "\x81\x76"},
    // Stage 4: MarisaB (routeMask=0x8)
    {TH06_ST4_MB_BOSS4, 4, 0x0F, 0x8, -1, "\x83\x7B\x83\x58\x20\x92\xCA\x8F\xED\x32"},
    // BOSS2: catk[17]=Earth(N), catk[21]=Earth上級(HL)
    {TH06_ST4_MB_BOSS2, 4, 0x02, 0x8, 17,
     "\x93\x79\x95\x84\x81\x75\x83\x8C\x83\x43\x83\x57\x83\x42\x83\x67\x83\x8A\x83\x8A\x83\x67\x83\x93\x81\x76"},
    {TH06_ST4_MB_BOSS2, 4, 0x0C, 0x8, 21,
     "\x93\x79\x95\x84\x81\x75\x83\x8C\x83\x43\x83\x57\x83\x42\x83\x67\x83\x8A\x83\x8A\x83\x67\x83\x93\x8F\xE3\x8B\x89"
     "\x81\x76"},
    // BOSS3: catk[17]=Earth(E), catk[18]=Metal(N), catk[26]=Metal2(HL)
    {TH06_ST4_MB_BOSS3, 4, 0x01, 0x8, 17,
     "\x93\x79\x95\x84\x81\x75\x83\x8C\x83\x43\x83\x57\x83\x42\x83\x67\x83\x8A\x83\x8A\x83\x67\x83\x93\x81\x76"},
    {TH06_ST4_MB_BOSS3, 4, 0x02, 0x8, 18,
     "\x8B\xE0\x95\x84\x81\x75\x83\x81\x83\x5E\x83\x8B\x83\x74\x83\x40\x83\x65\x83\x42\x81\x5B\x83\x4F\x81\x76"},
    {TH06_ST4_MB_BOSS3, 4, 0x0C, 0x8, 26,
     "\x8B\xE0\x95\x84\x81\x75\x83\x56\x83\x8B\x83\x6F\x81\x5B\x83\x68\x83\x89\x83\x53\x83\x93\x81\x76"},
    {TH06_ST4_MB_BOSS5, 4, 0x0F, 0x8, 31,
     "\x93\x79\x81\x95\x8B\xE0\x95\x84\x81\x75\x83\x47\x83\x81\x83\x89\x83\x8B\x83\x68\x83\x81\x83\x4B\x83\x8A\x83\x58"
     "\x81\x76"},
    {TH06_ST4_MB_BOSS6, 4, 0x0E, 0x8, 29,
     "\x90\x85\x81\x95\x96\xD8\x95\x84\x81\x75\x83\x45\x83\x48\x81\x5B\x83\x5E\x81\x5B\x83\x47\x83\x8B\x83\x74\x81"
     "\x76"},
    {TH06_ST4_MB_BOSS7, 4, 0x0C, 0x8, 30,
     "\x8B\xE0\x81\x95\x90\x85\x95\x84\x81\x75\x83\x7D\x81\x5B\x83\x4C\x83\x85\x83\x8A\x83\x7C\x83\x43\x83\x59\x83\x93"
     "\x81\x76"},
    // Stage 5
    {TH06_ST5_MID1, 5, 0x0F, 0xF, -1, "\x93\xB9\x92\x86\x20\x92\xCA\x8F\xED\x31"},
    {TH06_ST5_MID2, 5, 0x03, 0xF, 32,
     "\x8A\xEF\x8F\x70\x81\x75\x83\x7E\x83\x58\x83\x66\x83\x42\x83\x8C\x83\x4E\x83\x56\x83\x87\x83\x93\x81\x76"},
    {TH06_ST5_MID2, 5, 0x0C, 0xF, 33,
     "\x8A\xEF\x8F\x70\x81\x75\x8C\xB6\x98\x66\x83\x7E\x83\x58\x83\x66\x83\x42\x83\x8C\x83\x4E\x83\x56\x83\x87\x83\x93"
     "\x81\x76"},
    {TH06_ST5_BOSS_ALL, 5, 0x0F, 0xF, -1, "\x83\x7B\x83\x58\x90\xED\x92\xCA\x82\xB5"},
    {TH06_ST5_BOSS1, 5, 0x0F, 0xF, -1, "\x83\x7B\x83\x58\x20\x92\xCA\x8F\xED\x31"},
    {TH06_ST5_BOSS2, 5, 0x03, 0xF, 34,
     "\x8C\xB6\x8D\xDD\x81\x75\x83\x4E\x83\x8D\x83\x62\x83\x4E\x83\x52\x81\x5B\x83\x76\x83\x58\x81\x76"},
    {TH06_ST5_BOSS2, 5, 0x0C, 0xF, 37,
     "\x8C\xB6\x97\x48\x81\x75\x83\x57\x83\x83\x83\x62\x83\x4E\x81\x45\x83\x55\x81\x45\x83\x8B\x83\x68\x83\x72\x83\x8C"
     "\x81\x76"},
    {TH06_ST5_BOSS3, 5, 0x0F, 0xF, -1, "\x83\x7B\x83\x58\x20\x92\xCA\x8F\xED\x32"},
    {TH06_ST5_BOSS4, 5, 0x03, 0xF, 35,
     "\x8C\xB6\x8F\xDB\x81\x75\x83\x8B\x83\x69\x83\x4E\x83\x8D\x83\x62\x83\x4E\x81\x76"},
    {TH06_ST5_BOSS4, 5, 0x0C, 0xF, 38,
     "\x8C\xB6\x90\xA2\x81\x75\x83\x55\x81\x45\x83\x8F\x81\x5B\x83\x8B\x83\x68\x81\x76"},
    {TH06_ST5_BOSS5, 5, 0x0F, 0xF, -1, "\x83\x7B\x83\x58\x20\x92\xCA\x8F\xED\x33"},
    {TH06_ST5_BOSS6, 5, 0x03, 0xF, 36,
     "\x83\x81\x83\x43\x83\x68\x94\xE9\x8B\x5A\x81\x75\x91\x80\x82\xE8\x83\x68\x81\x5B\x83\x8B\x81\x76"},
    {TH06_ST5_BOSS6, 5, 0x0C, 0xF, 39,
     "\x83\x81\x83\x43\x83\x68\x94\xE9\x8B\x5A\x81\x75\x8E\x45\x90\x6C\x83\x68\x81\x5B\x83\x8B\x81\x76"},
    // Stage 6
    {TH06_ST6_MID1, 6, 0x0F, 0xF, -1, "\x93\xB9\x92\x86\x20\x92\xCA\x8F\xED\x31"},
    {TH06_ST6_MID2, 6, 0x0E, 0xF, 40,
     "\x8A\xEF\x8F\x70\x81\x75\x83\x47\x83\x5E\x81\x5B\x83\x69\x83\x8B\x83\x7E\x81\x5B\x83\x4E\x81\x76"},
    {TH06_ST6_BOSS_ALL, 6, 0x0F, 0xF, -1, "\x83\x7B\x83\x58\x90\xED\x92\xCA\x82\xB5"},
    {TH06_ST6_BOSS1, 6, 0x0F, 0xF, -1, "\x83\x7B\x83\x58\x20\x92\xCA\x8F\xED\x31"},
    {TH06_ST6_BOSS2, 6, 0x03, 0xF, 41,
     "\x93\x56\x94\xB1\x81\x75\x83\x58\x83\x5E\x81\x5B\x83\x49\x83\x75\x83\x5F\x83\x72\x83\x66\x81\x76"},
    {TH06_ST6_BOSS2, 6, 0x0C, 0xF, 46,
     "\x90\x5F\x94\xB1\x81\x75\x97\x63\x82\xAB\x83\x66\x81\x5B\x83\x82\x83\x93\x83\x8D\x81\x5B\x83\x68\x81\x76"},
    {TH06_ST6_BOSS3, 6, 0x0F, 0xF, -1, "\x83\x7B\x83\x58\x20\x92\xCA\x8F\xED\x32"},
    {TH06_ST6_BOSS4, 6, 0x03, 0xF, 42, "\x96\xBB\x95\x84\x81\x75\x8D\x67\x90\x46\x82\xCC\x96\xBB\x8A\x45\x81\x76"},
    {TH06_ST6_BOSS4, 6, 0x0C, 0xF, 47,
     "\x8D\x96\x95\x84\x81\x75\x90\xE7\x96\x7B\x82\xCC\x90\x6A\x82\xCC\x8E\x52\x81\x76"},
    {TH06_ST6_BOSS5, 6, 0x0F, 0xF, -1, "\x83\x7B\x83\x58\x20\x92\xCA\x8F\xED\x33"},
    {TH06_ST6_BOSS6, 6, 0x03, 0xF, 43,
     "\x8E\xF4\xE6\x66\x81\x75\x83\x75\x83\x89\x83\x68\x81\x45\x83\x63\x83\x46\x83\x79\x83\x56\x83\x85\x82\xCC\x8E\xF4"
     "\x82\xA2\x81\x76"},
    {TH06_ST6_BOSS6, 6, 0x0C, 0xF, 48, "\x90\x5F\x8F\x70\x81\x75\x8B\x7A\x8C\x8C\x8B\x53\x8C\xB6\x91\x7A\x81\x76"},
    {TH06_ST6_BOSS7, 6, 0x0F, 0xF, -1, "\x83\x7B\x83\x58\x20\x92\xCA\x8F\xED\x34"},
    {TH06_ST6_BOSS8, 6, 0x03, 0xF, 44,
     "\x8D\x67\x95\x84\x81\x75\x83\x58\x83\x4A\x81\x5B\x83\x8C\x83\x62\x83\x67\x83\x56\x83\x85\x81\x5B\x83\x67\x81"
     "\x76"},
    {TH06_ST6_BOSS8, 6, 0x0C, 0xF, 49,
     "\x8D\x67\x95\x84\x81\x75\x83\x58\x83\x4A\x81\x5B\x83\x8C\x83\x62\x83\x67\x83\x7D\x83\x43\x83\x58\x83\x5E\x81"
     "\x76"},
    {TH06_ST6_BOSS9, 6, 0x03, 0xF, 45, "\x81\x75\x83\x8C\x83\x62\x83\x68\x83\x7D\x83\x57\x83\x62\x83\x4E\x81\x76"},
    {TH06_ST6_BOSS9, 6, 0x0C, 0xF, 50, "\x81\x75\x8D\x67\x90\x46\x82\xCC\x8C\xB6\x91\x7A\x8B\xBD\x81\x76"},
    // Stage 7 (Extra)
    {TH06_ST7_MID1, 7, 0x10, 0xF, 51,
     "\x8C\x8E\x95\x84\x81\x75\x83\x54\x83\x43\x83\x8C\x83\x93\x83\x67\x83\x5A\x83\x8C\x83\x69\x81\x76"},
    {TH06_ST7_MID2, 7, 0x10, 0xF, 52,
     "\x93\xFA\x95\x84\x81\x75\x83\x8D\x83\x43\x83\x84\x83\x8B\x83\x74\x83\x8C\x83\x41\x81\x76"},
    {TH06_ST7_MID3, 7, 0x10, 0xF, 53,
     "\x89\xCE\x90\x85\x96\xD8\x8B\xE0\x93\x79\x95\x84\x81\x75\x8C\xAB\x8E\xD2\x82\xCC\x90\xCE\x81\x76"},
    {TH06_ST7_BOSS_ALL, 7, 0x10, 0xF, -1, "\x83\x7B\x83\x58\x90\xED\x92\xCA\x82\xB5"},
    {TH06_ST7_END_NS1, 7, 0x10, 0xF, -1, "\x83\x7B\x83\x58\x20\x92\xCA\x8F\xED\x31"},
    {TH06_ST7_END_S1, 7, 0x10, 0xF, 54,
     "\x8B\xD6\x8A\xF5\x81\x75\x83\x4E\x83\x89\x83\x93\x83\x78\x83\x8A\x81\x5B\x83\x67\x83\x89\x83\x62\x83\x76\x81"
     "\x76"},
    {TH06_ST7_END_NS2, 7, 0x10, 0xF, -1, "\x83\x7B\x83\x58\x20\x92\xCA\x8F\xED\x32"},
    {TH06_ST7_END_S2, 7, 0x10, 0xF, 55,
     "\x8B\xD6\x8A\xF5\x81\x75\x83\x8C\x81\x5B\x83\x94\x83\x40\x83\x65\x83\x43\x83\x93\x81\x76"},
    {TH06_ST7_END_NS3, 7, 0x10, 0xF, -1, "\x83\x7B\x83\x58\x20\x92\xCA\x8F\xED\x33"},
    {TH06_ST7_END_S3, 7, 0x10, 0xF, 56,
     "\x8B\xD6\x8A\xF5\x81\x75\x83\x74\x83\x48\x81\x5B\x83\x49\x83\x75\x83\x41\x83\x4A\x83\x43\x83\x93\x83\x68\x81"
     "\x76"},
    {TH06_ST7_END_NS4, 7, 0x10, 0xF, -1, "\x83\x7B\x83\x58\x20\x92\xCA\x8F\xED\x34"},
    {TH06_ST7_END_S4, 7, 0x10, 0xF, 57,
     "\x8B\xD6\x8A\xF5\x81\x75\x83\x4A\x83\x53\x83\x81\x83\x4A\x83\x53\x83\x81\x81\x76"},
    {TH06_ST7_END_NS5, 7, 0x10, 0xF, -1, "\x83\x7B\x83\x58\x20\x92\xCA\x8F\xED\x35"},
    {TH06_ST7_END_S5, 7, 0x10, 0xF, 58, "\x8B\xD6\x8A\xF5\x81\x75\x97\xF6\x82\xCC\x96\xC0\x98\x48\x81\x76"},
    {TH06_ST7_END_NS6, 7, 0x10, 0xF, -1, "\x83\x7B\x83\x58\x20\x92\xCA\x8F\xED\x36"},
    {TH06_ST7_END_S6, 7, 0x10, 0xF, 59,
     "\x8B\xD6\x92\x65\x81\x75\x83\x58\x83\x5E\x81\x5B\x83\x7B\x83\x45\x83\x75\x83\x8C\x83\x43\x83\x4E\x81\x76"},
    {TH06_ST7_END_NS7, 7, 0x10, 0xF, -1, "\x83\x7B\x83\x58\x20\x92\xCA\x8F\xED\x37"},
    {TH06_ST7_END_S7, 7, 0x10, 0xF, 60,
     "\x8B\xD6\x92\x65\x81\x75\x83\x4A\x83\x5E\x83\x66\x83\x42\x83\x49\x83\x76\x83\x67\x83\x8A\x83\x62\x83\x4E\x81"
     "\x76"},
    {TH06_ST7_END_NS8, 7, 0x10, 0xF, -1, "\x83\x7B\x83\x58\x20\x92\xCA\x8F\xED\x38"},
    {TH06_ST7_END_S8, 7, 0x10, 0xF, 61,
     "\x8B\xD6\x92\x65\x81\x75\x89\xDF\x8B\x8E\x82\xF0\x8D\x8F\x82\xDE\x8E\x9E\x8C\x76\x81\x76"},
    {TH06_ST7_END_S9, 7, 0x10, 0xF, 62,
     "\x94\xE9\x92\x65\x81\x75\x82\xBB\x82\xB5\x82\xC4\x92\x4E\x82\xE0\x82\xA2\x82\xC8\x82\xAD\x82\xC8\x82\xE9\x82\xA9"
     "\x81\x48\x81\x76"},
    {TH06_ST7_END_S10, 7, 0x10, 0xF, 63, "\x51\x45\x44\x81\x75\x34\x39\x35\x94\x4E\x82\xCC\x94\x67\x96\xE4\x81\x76"},
};

struct MenuState
{
    bool active;
    int difficulty;
    int character;
    int shotType;
    int stageIndex;
    int cursor;
    int topRow;
    int filteredCount;
    int lives;
    int bombs;
    int rank;
    bool configuring;        // true = spell chosen, now adjusting lives/bombs/rank
    int selectedSpellCursor; // cursor value saved when entering config mode
    int frame;               // start frame typed into the "frame specify" row (0 .. FRAME_MAX)
    bool frameEditing;       // true = editing the digits of `frame`
    int frameDigit;          // digit being edited: 0 = ten-thousands .. 4 = ones
    int filteredIndices[ARRAY_SIZE(kSpellDefs)];
};

MenuState g_Menu;

// The list shown to the player has one extra row on top: row 0 is "フレーム指定" (start the stage from a frame the
// player types in). Rows 1.. are the filtered spells, so spell number i is at row i + 1.
// A frame start is reported to the game as the "section" value FRAME_SECTION_BASE + frame (see
// SPELL_PRACTICE_FRAME_SECTION_BASE in SpellPracticeDllApi.hpp), because the DLL is reloaded when the game starts and
// the section value is the only thing that travels from the menu to th06_spell_practice_apply_ecl_patch.
const int FRAME_SECTION_BASE = th06::SPELL_PRACTICE_FRAME_SECTION_BASE;
const int FRAME_MAX = 65535; // the warp time is carried in 16 bits

inline int RowCount()
{
    return g_Menu.filteredCount + 1;
}

// Shift-JIS "フレーム指定"
const char kFrameLabel[] = "\x83\x74\x83\x8C\x81\x5B\x83\x80\x8E\x77\x92\xE8";

bool WasPressed(u16 curInput, u16 lastInput, u16 mask)
{
    return ((curInput & mask) != 0) && ((curInput & mask) != (lastInput & mask));
}

bool WasPressedWeird(u16 curInput, u16 lastInput, u16 isEighthFrameHeldInput, u16 mask)
{
    return WasPressed(curInput, lastInput, mask) || (((curInput & mask) != 0) && (isEighthFrameHeldInput != 0));
}

void ClampScroll()
{
    const int rows = RowCount(); // frame row + spells
    if (g_Menu.cursor < 0)
        g_Menu.cursor = rows - 1;
    if (g_Menu.cursor >= rows)
        g_Menu.cursor = 0;
    if (g_Menu.cursor < g_Menu.topRow)
        g_Menu.topRow = g_Menu.cursor;
    if (g_Menu.cursor >= g_Menu.topRow + SPELL_VIEW_ROWS)
        g_Menu.topRow = g_Menu.cursor - SPELL_VIEW_ROWS + 1;
    if (g_Menu.topRow < 0)
        g_Menu.topRow = 0;
}

u8 DiffBitFromDifficulty(int d)
{
    if (d < 0 || d > 4)
        return 0;
    return (u8)(1u << d);
}

void BuildFilteredList()
{
    g_Menu.filteredCount = 0;
    const int stage = g_Menu.stageIndex + 1;
    // Extra stage (stage == 7) uses diffMask bit4; skip diff filter and use EXTRA bit directly
    const bool isExtraStage = (stage == 7);
    const u8 diffBit = isExtraStage ? (u8)0x10 : DiffBitFromDifficulty(g_Menu.difficulty);
    // routeBit: bit0=ReimuA, bit1=ReimuB, bit2=MarisaA, bit3=MarisaB; 0xF = all routes
    int routeIdx = g_Menu.character * 2 + g_Menu.shotType;
    const u8 routeBit = (routeIdx >= 0 && routeIdx < 4) ? (u8)(1u << routeIdx) : (u8)0xF;

    for (int i = 0; i < (int)ARRAY_SIZE(kSpellDefs); ++i)
    {
        const SpellDef &s = kSpellDefs[i];
        if (s.stage != stage)
            continue;
        if ((s.diffMask & diffBit) == 0)
            continue;
        if (s.routeMask != 0xF && (s.routeMask & routeBit) == 0)
            continue;
        g_Menu.filteredIndices[g_Menu.filteredCount++] = i;
    }
    g_Menu.cursor = 0;
    g_Menu.topRow = 0;
    ClampScroll();
}

void AddItem(th06::SpellPracticeDrawData *d, float x, float y, u32 color, const char *text)
{
    if (d->itemCount < 0 || d->itemCount >= 32)
        return;
    th06::SpellPracticeDrawItem &item = d->items[d->itemCount++];
    item.x = x;
    item.y = y;
    item.color = color;
    std::strncpy(item.text, text, sizeof(item.text) - 1);
    item.text[sizeof(item.text) - 1] = '\0';
}

// ECL write helpers
static u8 *g_EclFile = NULL;

static void ecl_write32(int offset, i32 value)
{
    if (g_EclFile)
        *(i32 *)(g_EclFile + offset) = value;
}
static void ecl_write16(int offset, i16 value)
{
    if (g_EclFile)
        *(i16 *)(g_EclFile + offset) = value;
}
static void ecl_write8(int offset, u8 value)
{
    if (g_EclFile)
        *(u8 *)(g_EclFile + offset) = value;
}

static void ecl_set_health(int offset, i32 ecl_time, i32 health)
{
    ecl_write32(offset, ecl_time);
    ecl_write32(offset + 4, 0x0010006f);
    ecl_write32(offset + 8, 0x00ffff00);
    ecl_write32(offset + 12, health);
}
static void ecl_set_time(int offset, i32 ecl_time, i32 timer)
{
    ecl_write32(offset, ecl_time);
    ecl_write32(offset + 4, 0x00100073);
    ecl_write32(offset + 8, 0x00ffff00);
    ecl_write32(offset + 12, timer);
}
static void ecl_stall(int offset)
{
    ecl_write32(offset, 0x99999);
    ecl_write32(offset + 4, 0x000c0000);
    ecl_write32(offset + 8, 0x0000ff00);
}

extern "C" __declspec(dllexport) int __stdcall th06_spell_practice_open_menu(HWND /*gameWindow*/, int difficulty,
                                                                             int character, int shotType,
                                                                             int stageIndex, void * /*fontPtr*/)
{
    int prevLives = g_Menu.lives;
    int prevBombs = g_Menu.bombs;
    int prevRank = g_Menu.rank;
    int prevFrame = g_Menu.frame;
    std::memset(&g_Menu, 0, sizeof(g_Menu));
    g_Menu.active = true;
    g_Menu.frame = (prevFrame >= 0 && prevFrame <= FRAME_MAX) ? prevFrame : 0;
    g_Menu.frameDigit = 2; // start on the hundreds digit
    g_Menu.difficulty = difficulty;
    g_Menu.character = character;
    g_Menu.shotType = shotType;
    g_Menu.stageIndex = stageIndex;
    if (g_Menu.stageIndex < 0)
        g_Menu.stageIndex = 0;
    if (g_Menu.stageIndex > 6)
        g_Menu.stageIndex = 6;
    g_Menu.lives = (prevLives >= 0 && prevLives <= 8) ? prevLives : 8;
    g_Menu.bombs = (prevBombs >= 0 && prevBombs <= 8) ? prevBombs : 8;
    g_Menu.rank = (prevRank >= 1 && prevRank <= 32) ? prevRank : 16;
    BuildFilteredList();
    return th06::SPELL_PRACTICE_OPEN_RESULT_MENU_RUNNING;
}

extern "C" __declspec(dllexport) void __stdcall th06_spell_practice_draw_menu()
{
}

extern "C" __declspec(dllexport) int __stdcall th06_spell_practice_update_menu(u16 curInput, u16 lastInput,
                                                                               u16 isEighthFrameHeldInput)
{
    if (!g_Menu.active)
        return th06::SPELL_PRACTICE_OPEN_RESULT_NOT_HANDLED;

    const u16 TH_BUTTON_UP = 1 << 4;
    const u16 TH_BUTTON_DOWN = 1 << 5;
    const u16 TH_BUTTON_LEFT = 1 << 6;
    const u16 TH_BUTTON_RIGHT = 1 << 7;
    const u16 TH_BUTTON_SELECTMENU = (1 << 12) | (1 << 0);
    const u16 TH_BUTTON_RETURNMENU = (1 << 3) | (1 << 1);

    if (g_Menu.configuring)
    {
        // Config mode: UP/DOWN moves between lives(0)/bombs(1)/rank(2) rows
        if (WasPressedWeird(curInput, lastInput, isEighthFrameHeldInput, TH_BUTTON_UP))
        {
            if (g_Menu.cursor > 0)
                g_Menu.cursor--;
        }
        if (WasPressedWeird(curInput, lastInput, isEighthFrameHeldInput, TH_BUTTON_DOWN))
        {
            if (g_Menu.cursor < 2)
                g_Menu.cursor++;
        }
        if (g_Menu.cursor == 0)
        {
            if (WasPressedWeird(curInput, lastInput, isEighthFrameHeldInput, TH_BUTTON_LEFT))
            {
                if (g_Menu.lives > 0)
                    g_Menu.lives--;
            }
            if (WasPressedWeird(curInput, lastInput, isEighthFrameHeldInput, TH_BUTTON_RIGHT))
            {
                if (g_Menu.lives < 8)
                    g_Menu.lives++;
            }
        }
        if (g_Menu.cursor == 1)
        {
            if (WasPressedWeird(curInput, lastInput, isEighthFrameHeldInput, TH_BUTTON_LEFT))
            {
                if (g_Menu.bombs > 0)
                    g_Menu.bombs--;
            }
            if (WasPressedWeird(curInput, lastInput, isEighthFrameHeldInput, TH_BUTTON_RIGHT))
            {
                if (g_Menu.bombs < 8)
                    g_Menu.bombs++;
            }
        }
        if (g_Menu.cursor == 2)
        {
            if (WasPressedWeird(curInput, lastInput, isEighthFrameHeldInput, TH_BUTTON_LEFT))
            {
                if (g_Menu.rank > 1)
                    g_Menu.rank--;
            }
            if (WasPressedWeird(curInput, lastInput, isEighthFrameHeldInput, TH_BUTTON_RIGHT))
            {
                if (g_Menu.rank < 32)
                    g_Menu.rank++;
            }
        }
        if (WasPressed(curInput, lastInput, TH_BUTTON_RETURNMENU))
        {
            // Cancel: go back to spell selection
            g_Menu.configuring = false;
            g_Menu.cursor = g_Menu.selectedSpellCursor;
            ClampScroll();
        }
        if (WasPressed(curInput, lastInput, TH_BUTTON_SELECTMENU))
        {
            g_Menu.active = false;
            return th06::SPELL_PRACTICE_OPEN_RESULT_START_GAME;
        }
    }
    else if (g_Menu.frameEditing)
    {
        // Frame specify: LEFT/RIGHT pick a digit of the 5-digit frame number, UP/DOWN change that digit.
        static const int kPow10[5] = {10000, 1000, 100, 10, 1};
        const int p = kPow10[g_Menu.frameDigit];
        const int digit = (g_Menu.frame / p) % 10;
        int newDigit = digit;
        if (WasPressedWeird(curInput, lastInput, isEighthFrameHeldInput, TH_BUTTON_UP))
            newDigit = (digit + 1) % 10;
        if (WasPressedWeird(curInput, lastInput, isEighthFrameHeldInput, TH_BUTTON_DOWN))
            newDigit = (digit + 9) % 10;
        if (newDigit != digit)
        {
            g_Menu.frame += (newDigit - digit) * p;
            if (g_Menu.frame > FRAME_MAX)
                g_Menu.frame = FRAME_MAX;
            if (g_Menu.frame < 0)
                g_Menu.frame = 0;
        }
        if (WasPressedWeird(curInput, lastInput, isEighthFrameHeldInput, TH_BUTTON_LEFT))
            g_Menu.frameDigit = (g_Menu.frameDigit + 4) % 5;
        if (WasPressedWeird(curInput, lastInput, isEighthFrameHeldInput, TH_BUTTON_RIGHT))
            g_Menu.frameDigit = (g_Menu.frameDigit + 1) % 5;
        if (WasPressed(curInput, lastInput, TH_BUTTON_RETURNMENU))
        {
            // Cancel: back to the spell list
            g_Menu.frameEditing = false;
        }
        else if (WasPressed(curInput, lastInput, TH_BUTTON_SELECTMENU))
        {
            // Frame fixed: go on to the lives / bombs / rank screen like for a spell
            g_Menu.frameEditing = false;
            g_Menu.configuring = true;
            g_Menu.cursor = 0;
        }
    }
    else
    {
        // Spell selection mode (row 0 = frame specify)
        if (WasPressedWeird(curInput, lastInput, isEighthFrameHeldInput, TH_BUTTON_UP))
        {
            g_Menu.cursor--;
            ClampScroll();
        }
        if (WasPressedWeird(curInput, lastInput, isEighthFrameHeldInput, TH_BUTTON_DOWN))
        {
            g_Menu.cursor++;
            ClampScroll();
        }
        if (WasPressed(curInput, lastInput, TH_BUTTON_RETURNMENU))
        {
            g_Menu.active = false;
            return th06::SPELL_PRACTICE_OPEN_RESULT_STAY_IN_MENU;
        }
        if (WasPressed(curInput, lastInput, TH_BUTTON_SELECTMENU))
        {
            g_Menu.selectedSpellCursor = g_Menu.cursor;
            if (g_Menu.cursor == 0)
            {
                // Frame specify: type the start frame first
                g_Menu.frameEditing = true;
            }
            else
            {
                // Enter config mode: save spell cursor and move to lives row
                g_Menu.configuring = true;
                g_Menu.cursor = 0;
            }
        }
    }
    return th06::SPELL_PRACTICE_OPEN_RESULT_MENU_RUNNING;
}

// Mirrors the layout of Catk in ResultScreen.hpp (size = 0x40)
struct CatkEntry
{
    u8 base[0x0c];        // Th6k base
    i32 captureScore;     // 0x0c
    u16 idx;              // 0x10
    u8 nameCsum;          // 0x12
    u8 characterShotType; // 0x13
    u32 unk_14;           // 0x14
    char name[32];        // 0x18
    u32 unk_38;           // 0x38
    u16 numAttempts;      // 0x3c
    u16 numSuccess;       // 0x3e
};

extern "C" __declspec(dllexport) int __stdcall th06_spell_practice_get_draw_data(
    th06::SpellPracticeDrawData *outDrawData, const void *catkData, int catkCount)
{
    if (!g_Menu.active || !outDrawData)
        return 0;
    outDrawData->itemCount = 0;

    static const char *kDiffNames[5] = {"Easy", "Norm", "Hard", "Luna", "Ex"};
    char header[96];
    if (g_Menu.stageIndex == 6)
    {
        _snprintf(header, sizeof(header), "Spell List  Stage:Ex");
    }
    else
    {
        _snprintf(header, sizeof(header), "Spell List  Stage:%d  Diff:%s", g_Menu.stageIndex + 1,
                  (g_Menu.difficulty >= 0 && g_Menu.difficulty < 5) ? kDiffNames[g_Menu.difficulty] : "?");
    }
    AddItem(outDrawData, 90.0f, 88.0f, 0xFFF0F0F0, header);

    // Row 0 is "フレーム指定"; rows 1.. are the spells of this stage / difficulty / route.
    {
        const int totalRows = RowCount();
        const int end = (g_Menu.topRow + SPELL_VIEW_ROWS < totalRows) ? (g_Menu.topRow + SPELL_VIEW_ROWS) : totalRows;
        for (int row = g_Menu.topRow; row < end; ++row)
        {
            const float rowY = 120.0f + (row - g_Menu.topRow) * 18.0f;
            if (row == 0)
            {
                AddItem(outDrawData, 110.0f, rowY, 0xFFF0F0FF, kFrameLabel);
                // Current frame value next to the label; the digit being edited is shown in [ ].
                char digits[8];
                _snprintf(digits, sizeof(digits), "%05d", g_Menu.frame);
                char frameText[24];
                int n = 0;
                for (int k = 0; k < 5; ++k)
                {
                    if (g_Menu.frameEditing && k == g_Menu.frameDigit)
                        frameText[n++] = '[';
                    frameText[n++] = digits[k];
                    if (g_Menu.frameEditing && k == g_Menu.frameDigit)
                        frameText[n++] = ']';
                }
                frameText[n] = '\0';
                AddItem(outDrawData, 220.0f, rowY, g_Menu.frameEditing ? 0xFFFFE080 : 0xFFF0F0FF, frameText);
                continue;
            }
            const SpellDef &spell = kSpellDefs[g_Menu.filteredIndices[row - 1]];
            char lineText[128];
            if (spell.catkIdx >= 0 && catkData != NULL && spell.catkIdx < catkCount)
            {
                const CatkEntry *entry = (const CatkEntry *)catkData + spell.catkIdx;
                _snprintf(lineText, sizeof(lineText), "%s  %u/%u", spell.name, (unsigned)entry->numSuccess,
                          (unsigned)entry->numAttempts);
            }
            else
            {
                std::strncpy(lineText, spell.name, sizeof(lineText) - 1);
                lineText[sizeof(lineText) - 1] = '\0';
            }
            AddItem(outDrawData, 110.0f, rowY, 0xFFF0F0FF, lineText);
        }
        if (g_Menu.filteredCount == 0)
        {
            const float msgY = 120.0f + (1 - g_Menu.topRow) * 18.0f;
            if (g_Menu.topRow <= 1)
                AddItem(outDrawData, 110.0f, msgY, 0x80c0c0ff, "(No spells for this selection)");
        }
        // In config mode, spell cursor stays on the chosen row; config cursor is on the right panel
        const int spellCursorRow = g_Menu.configuring ? g_Menu.selectedSpellCursor : g_Menu.cursor;
        AddItem(outDrawData, 90.0f, 120.0f + (spellCursorRow - g_Menu.topRow) * 18.0f, 0xFFFFFFFF, ">>");
    }
    char rankText[32];
    _snprintf(rankText, sizeof(rankText), "Rank: %d", g_Menu.rank);
    AddItem(outDrawData, 380.0f, 156.0f, 0xFFF0F0F0, rankText);
    if (g_Menu.configuring)
    {
        // Show config-mode cursor on the right panel (lives=120, bombs=138, rank=156)
        const float configY = (g_Menu.cursor == 0) ? 120.0f : (g_Menu.cursor == 1) ? 138.0f : 156.0f;
        AddItem(outDrawData, 360.0f, configY, 0xFFFFFFFF, ">>");
        AddItem(outDrawData, 90.0f, 352.0f, 0xC0D0D0D0, "UP/DOWN:Move  SHOT:Start  BOMB:Back");
        AddItem(outDrawData, 90.0f, 364.0f, 0xC0D0D0D0, "LEFT/RIGHT:Change Value");
    }
    else if (g_Menu.frameEditing)
    {
        AddItem(outDrawData, 90.0f, 352.0f, 0xC0D0D0D0, "UP/DOWN:Value  LEFT/RIGHT:Digit");
        AddItem(outDrawData, 90.0f, 364.0f, 0xC0D0D0D0, "SHOT:OK  BOMB:Back");
    }
    else
    {
        AddItem(outDrawData, 90.0f, 352.0f, 0xC0D0D0D0, "UP/DOWN:Move  SHOT:Select  BOMB:Back");
    }
    return outDrawData->itemCount;
}

extern "C" __declspec(dllexport) int __stdcall th06_spell_practice_get_lives()
{
    return g_Menu.lives;
}

extern "C" __declspec(dllexport) void __stdcall th06_spell_practice_set_lives(int lives)
{
    if (lives >= 0 && lives <= 8)
        g_Menu.lives = lives;
}

extern "C" __declspec(dllexport) int __stdcall th06_spell_practice_get_bombs()
{
    return g_Menu.bombs;
}

extern "C" __declspec(dllexport) void __stdcall th06_spell_practice_set_bombs(int bombs)
{
    if (bombs >= 0 && bombs <= 8)
        g_Menu.bombs = bombs;
}

extern "C" __declspec(dllexport) int __stdcall th06_spell_practice_get_rank()
{
    return g_Menu.rank;
}

extern "C" __declspec(dllexport) void __stdcall th06_spell_practice_set_rank(int rank)
{
    if (rank >= 1 && rank <= 32)
        g_Menu.rank = rank;
}

extern "C" __declspec(dllexport) int __stdcall th06_spell_practice_get_selected_spell()
{
    // In config mode, use the saved spell cursor; otherwise use the current cursor
    const int spellCursor = g_Menu.configuring ? g_Menu.selectedSpellCursor : g_Menu.cursor;
    // Row 0 = frame specify: reported as FRAME_SECTION_BASE + frame
    if (spellCursor == 0)
        return FRAME_SECTION_BASE + g_Menu.frame;
    if (spellCursor < 0 || spellCursor > g_Menu.filteredCount)
        return -1;
    return kSpellDefs[g_Menu.filteredIndices[spellCursor - 1]].section;
}

extern "C" __declspec(dllexport) void __stdcall th06_spell_practice_get_selected_params(int *mode, int *section)
{
    // mode/section are not used by the new patch function; kept for API compat
    if (mode)
        *mode = 0;
    if (section)
        *section = 0;
}

extern "C" __declspec(dllexport) void __stdcall th06_spell_practice_set_cursor_to_spell(int spellIndex)
{
    // A frame start (section value FRAME_SECTION_BASE + frame): put the cursor on the frame row, keeping the frame
    if (spellIndex >= FRAME_SECTION_BASE)
    {
        const int frame = spellIndex - FRAME_SECTION_BASE;
        g_Menu.frame = (frame >= 0 && frame <= FRAME_MAX) ? frame : 0;
        g_Menu.cursor = 0;
        ClampScroll();
        return;
    }
    // spellIndex is now a section value
    for (int i = 0; i < g_Menu.filteredCount; i++)
    {
        if (kSpellDefs[g_Menu.filteredIndices[i]].section == spellIndex)
        {
            g_Menu.cursor = i + 1; // row 0 is the frame row
            ClampScroll();
            return;
        }
    }
}

static void s2b_nd()
{
    ecl_write32(0x18fc, 0);
    ecl_write32(0x191c, 0);
    ecl_write32(0x192c, 0);
    ecl_write32(0x194c, 0);
    ecl_write32(0x196c, 0);
    ecl_write32(0x198c, 0);
    ecl_write32(0x19a0, 0);
}

static void s3b_n1()
{
    ecl_write16(0x1274, 0);
    ecl_write16(0x12f0, 0);
    ecl_write16(0x80d6, (i16)0x16d4);
    ecl_write32(0x1f70, 1);
    ecl_write32(0x1f90, 0);
    ecl_write16(0x80dc, (i16)0x24);
    ecl_write32(0x20ec, 0);
    ecl_write32(0x210c, 0);
    ecl_write32(0x212c, 0);
    ecl_write32(0x214c, 0x1e);
    ecl_write32(0x2160, 0x1e);
    ecl_write32(0x2194, 0x1e);
    ecl_write32(0x2188, 150);
}

static void s4b_time()
{
    ecl_write32(0x2790, 0);
    ecl_write32(0x27b0, 0);
    ecl_write32(0x27d0, 0);
    ecl_write32(0x27f0, 0);
    ecl_write32(0x2810, 0);
    ecl_write32(0x2824, 0);
    ecl_write32(0x283c, 0);
    ecl_write32(0x2850, 0);
}

static void s5b()
{
    ecl_write16(0x767c, 0);
    ecl_write32(0x22c8, 0);
    ecl_write32(0x22e8, 0);
    ecl_write32(0x2308, 0);
    ecl_write32(0x2328, 0);
    ecl_write32(0x2348, 0);
    ecl_write16(0x2218, 0);
}

static void s6b()
{
    ecl_write32(0x1686, 0);
    ecl_write32(0x16a6, 0);
    ecl_write32(0x16c6, 0);
    ecl_write32(0x16e6, 0);
    ecl_write16(0x15d6, 0);
}

static void s7b_n1()
{
    ecl_write32(0x339e, 0);
    ecl_write32(0x33ae, 0);
    ecl_write32(0x33ce, 0);
    ecl_write32(0x33ee, 0);
    ecl_write32(0x340e, 0);
    ecl_write32(0x342e, 0);
}

static void s7b_call()
{
    ecl_write32(0x344e, 0);
    ecl_write32(0x3452, 0x00180023);
    ecl_write32(0x3456, 0x00ffff00);
    ecl_write32(0x345a, 0);
    ecl_write32(0x345e, 0);
    ecl_write32(0x3462, 0);
    ecl_stall(0x3466);
}

} // namespace

// Returns true if the given section is a spellcard (not a non-spell normal attack).
// Non-spell sections: MID1, BOSS1, BOSS3, BOSS5, BOSS7, BOOKS, ST7 MID/NS variants.
static bool IsSpellSection(TH06Section s)
{
    switch (s)
    {
    // Stage 1 non-spell
    case TH06_ST1_MID1:
    case TH06_ST1_BOSS1:
    case TH06_ST1_BOSS3:
    // Stage 2 non-spell
    case TH06_ST2_MID1:
    case TH06_ST2_BOSS1:
    case TH06_ST2_BOSS3:
    // Stage 3 non-spell
    case TH06_ST3_MID1:
    case TH06_ST3_BOSS1:
    case TH06_ST3_BOSS3:
    case TH06_ST3_BOSS5:
    // Stage 4 route-neutral non-spell
    case TH06_ST4_BOOKS:
    case TH06_ST4_MID1:
    case TH06_ST4_BOSS1:
    // Stage 4 ReimuA non-spell
    case TH06_ST4_RA_BOOKS:
    case TH06_ST4_RA_MID1:
    case TH06_ST4_RA_BOSS1:
    case TH06_ST4_RA_BOSS4:
    // Stage 4 ReimuB non-spell
    case TH06_ST4_RB_BOOKS:
    case TH06_ST4_RB_MID1:
    case TH06_ST4_RB_BOSS1:
    case TH06_ST4_RB_BOSS4:
    // Stage 4 MarisaA non-spell
    case TH06_ST4_MA_BOOKS:
    case TH06_ST4_MA_MID1:
    case TH06_ST4_MA_BOSS1:
    case TH06_ST4_MA_BOSS4:
    // Stage 4 MarisaB non-spell
    case TH06_ST4_MB_BOOKS:
    case TH06_ST4_MB_MID1:
    case TH06_ST4_MB_BOSS1:
    case TH06_ST4_MB_BOSS4:
    // Stage 5 non-spell
    case TH06_ST5_MID1:
    case TH06_ST5_BOSS1:
    case TH06_ST5_BOSS3:
    case TH06_ST5_BOSS5:
    // Stage 6 non-spell
    case TH06_ST6_MID1:
    case TH06_ST6_BOSS1:
    case TH06_ST6_BOSS3:
    case TH06_ST6_BOSS5:
    case TH06_ST6_BOSS7:
    // Stage 7 non-spell (NS variants only; MID1/MID2/MID3 are all spells)
    case TH06_ST7_END_NS1:
    case TH06_ST7_END_NS2:
    case TH06_ST7_END_NS3:
    case TH06_ST7_END_NS4:
    case TH06_ST7_END_NS5:
    case TH06_ST7_END_NS6:
    case TH06_ST7_END_NS7:
    case TH06_ST7_END_NS8:
    // Full boss-run entries
    case TH06_ST1_BOSS_ALL:
    case TH06_ST2_BOSS_ALL:
    case TH06_ST3_BOSS_ALL:
    case TH06_ST4_RA_BOSS_ALL:
    case TH06_ST4_RB_BOSS_ALL:
    case TH06_ST4_MA_BOSS_ALL:
    case TH06_ST4_MB_BOSS_ALL:
    case TH06_ST5_BOSS_ALL:
    case TH06_ST6_BOSS_ALL:
    case TH06_ST7_BOSS_ALL:
        return false;
    default:
        return true;
    }
}

// Apply ECL patch – spellIndex is a TH06Section value, eclFile is the raw ECL buffer
// shot: 0=ReimuA, 1=ReimuB, 2=MarisaA, 3=MarisaB (passed by caller, avoids hardcoded address)
extern "C" __declspec(dllexport) int __cdecl th06_spell_practice_apply_ecl_patch(int spellIndex, void *eclFile,
                                                                                 int shot)
{
    g_EclFile = (u8 *)eclFile;
    if (!eclFile)
        return 0;

    // Frame specify: no ECL patch, just warp the stage timeline to the typed frame and run the stage on to its end
    // (0xFFFF in the upper half = full boss-run mode: dialogues play, boss BGM switches, no forced end).
    if (spellIndex >= FRAME_SECTION_BASE)
    {
        const i32 frame = (spellIndex - FRAME_SECTION_BASE) & 0xFFFF;
        return frame | (0xFFFF << 16);
    }

    i32 warpTime = 0;

    switch ((TH06Section)spellIndex)
    {
    // ── Stage 1 ──────────────────────────────────────────────────
    case TH06_ST1_MID1:
        warpTime = 0x7d8;
        ecl_write32(0x0ab0, 0x3c);
        ecl_write32(0x0ad0, 0x3c);
        break;
    case TH06_ST1_MID2:
        warpTime = 0x7d8;
        ecl_write32(0x0ab0, 0x3c);
        ecl_write32(0x0ad0, 0x3c);
        ecl_set_health(0x0af0, 0x3c, 0x1f3);
        break;
    case TH06_ST1_BOSS1:
        warpTime = 0x149f;
        ecl_write32(0x16a6, 0);
        ecl_write32(0x16c6, 0);
        ecl_write32(0x16e6, 0x50);
        break;
    case TH06_ST1_BOSS2:
        warpTime = 0x149f;
        ecl_write32(0x16a6, 0);
        ecl_write32(0x16c6, 0);
        ecl_write32(0x16e6, 0x50);
        ecl_stall(0x16f6);
        break;
    case TH06_ST1_BOSS3:
        warpTime = 0x149f;
        ecl_write32(0x16a6, 0);
        ecl_write32(0x16c6, 0);
        ecl_write32(0x16e6, 0x50);
        ecl_write32(0x16f2, 0x10);
        ecl_write32(0x293a, 0);
        ecl_write32(0x294a, 0);
        ecl_write16(0x291e, 0);
        break;
    case TH06_ST1_BOSS4:
        warpTime = 0x149f;
        ecl_write32(0x16a6, 0);
        ecl_write32(0x16c6, 0);
        ecl_write32(0x16e6, 0x50);
        ecl_write32(0x16f2, 0x10);
        ecl_write32(0x293a, 0);
        ecl_write32(0x294a, 0);
        ecl_write16(0x291e, 0);
        ecl_set_time(0x294a, 0, 0);
        ecl_stall(0x295a);
        break;

    // ── Stage 2 ──────────────────────────────────────────────────
    case TH06_ST2_MID1:
        warpTime = 0xa1c;
        break;
    case TH06_ST2_BOSS1:
        warpTime = 0x1760;
        break;
    case TH06_ST2_BOSS2:
        s2b_nd();
        ecl_set_time(0x19a0, 0, 0);
        ecl_stall(0x19b0);
        warpTime = 0x1760;
        break;
    case TH06_ST2_BOSS3:
        s2b_nd();
        ecl_write32(0x19ac, 0x19);
        ecl_write32(0x2138, 0);
        ecl_write32(0x2148, 0x60);
        ecl_write16(0x2110, 0);
        warpTime = 0x1760;
        break;
    case TH06_ST2_BOSS4:
        s2b_nd();
        ecl_write32(0x19ac, 0x19);
        ecl_write32(0x2138, 0);
        ecl_write32(0x2148, 0x60);
        ecl_write16(0x2110, 0);
        ecl_set_time(0x2148, 0x30, 0);
        ecl_stall(0x2158);
        warpTime = 0x1760;
        break;
    case TH06_ST2_BOSS5:
        s2b_nd();
        ecl_write32(0x19ac, 0x19);
        ecl_write32(0x2138, 0);
        ecl_write32(0x2148, 0x60);
        ecl_write16(0x2110, 0);
        ecl_write32(0x2148, 0);
        ecl_write32(0x2154, 0x20);
        ecl_write16(0x33a2, 0);
        ecl_write16(0x337a, 0);
        ecl_write16(0x3392, 0);
        ecl_write32(0x2090, 0x578);
        ecl_write32(0x20b0, 0xffffffff);
        ecl_write32(0x20c0, 0xffffffff);
        ecl_write32(0x20f0, 0x1c);
        warpTime = 0x1760;
        break;

    // ── Stage 3 ──────────────────────────────────────────────────
    case TH06_ST3_MID1:
        warpTime = 0x0edc;
        break;
    case TH06_ST3_MID2:
        warpTime = 0x0edc;
        ecl_write16(0x1274, 0);
        ecl_write16(0x12f0, 0);
        ecl_write32(0x1018, 0);
        ecl_set_health(0x10dc, 0x1e, 0x513);
        ecl_stall(0x10ec);
        break;
    case TH06_ST3_BOSS1:
        s3b_n1();
        warpTime = 0x16d4;
        break;
    case TH06_ST3_BOSS2:
        s3b_n1();
        ecl_write32(0x214c, 0);
        ecl_set_time(0x2160, 0, 0);
        ecl_stall(0x2170);
        warpTime = 0x16d4;
        break;
    case TH06_ST3_BOSS3:
        s3b_n1();
        ecl_write32(0x214c, 0);
        ecl_write32(0x2160, 0);
        ecl_write32(0x216c, 0x14);
        ecl_write16(0x25f4, 0);
        warpTime = 0x16d4;
        break;
    case TH06_ST3_BOSS4:
        s3b_n1();
        ecl_write32(0x214c, 0);
        ecl_write32(0x2160, 0);
        ecl_write32(0x216c, 0x14);
        ecl_write16(0x25f4, 0);
        ecl_set_time(0x267c, 0, 0);
        ecl_stall(0x268c);
        warpTime = 0x16d4;
        break;
    case TH06_ST3_BOSS5:
        s3b_n1();
        ecl_write32(0x214c, 0);
        ecl_write32(0x2160, 0);
        ecl_write32(0x216c, 0x1a);
        ecl_write16(0x31d0, 0);
        warpTime = 0x16d4;
        break;
    case TH06_ST3_BOSS6:
        s3b_n1();
        ecl_write32(0x214c, 0);
        ecl_write32(0x2160, 0);
        ecl_write32(0x216c, 0x1a);
        ecl_write16(0x31d0, 0);
        ecl_set_time(0x3254, 0, 0);
        ecl_stall(0x3264);
        warpTime = 0x16d4;
        break;
    case TH06_ST3_BOSS7:
        s3b_n1();
        ecl_write32(0x214c, 0);
        ecl_write32(0x2160, 0);
        ecl_write32(0x216c, 0x1a);
        ecl_write16(0x31d0, 0);
        ecl_set_time(0x3254, 0, 0);
        ecl_stall(0x3264);
        ecl_write32(0x3168, 0x7d0);
        ecl_write32(0x31a8, 0x21);
        ecl_write32(0x31b8, 0x21);
        ecl_write16(0x4b64, 0);
        ecl_write16(0x4bec, 0);
        warpTime = 0x16d4;
        break;

    // ── Stage 4 (route-neutral) ───────────────────────────────────
    case TH06_ST4_BOOKS:
        warpTime = 0x0d40;
        break;
    case TH06_ST4_MID1:
        warpTime = 0x1024;
        break;
    case TH06_ST4_BOSS1:
        warpTime = 0x29c7;

        break;

    // ── Stage 4 ReimuA ────────────────────────────────────────────
    case TH06_ST4_RA_BOOKS:
        warpTime = 0x0d40;
        break;
    case TH06_ST4_RA_MID1:
        warpTime = 0x1024;
        break;
    case TH06_ST4_RA_BOSS1:
        warpTime = 0x29c7;
        break;
    case TH06_ST4_RA_BOSS2:
        warpTime = 0x29c7;
        s4b_time();
        ecl_set_time(0x2810, 0, 0);
        ecl_stall(0x2820);
        break;
    case TH06_ST4_RA_BOSS3:
        warpTime = 0x29c7;
        s4b_time();
        ecl_write16(0x2854, 0x23);
        ecl_write32(0x285c, 0x25);
        ecl_write16(0x6da0, 0);
        ecl_set_time(0x7440, 0, 0);
        ecl_stall(0x7450);
        break;
    case TH06_ST4_RA_BOSS4:
        warpTime = 0x29c7;
        s4b_time();
        ecl_write16(0x2854, 0x23);
        ecl_write32(0x285c, 0x25);
        ecl_write16(0x6da0, 0);
        ecl_stall(0x7afc); // block advance to BOSS5
        break;
    case TH06_ST4_RA_BOSS5:
        warpTime = 0x29c7;
        s4b_time();
        ecl_write16(0x2854, 0x23);
        ecl_write32(0x285c, 0x27);
        ecl_write16(0x7950, 0);
        ecl_write32(0x7afc, 0);
        ecl_write32(0x7b0c, 0);
        ecl_write32(0x7b2c, 0);
        ecl_write32(0x7b4c, 0);
        ecl_write32(0x7b6c, 0);
        ecl_write32(0x7b8c, 0);
        break;
    case TH06_ST4_RA_BOSS6:
        warpTime = 0x29c7;
        s4b_time();
        ecl_write16(0x2854, 0x23);
        ecl_write32(0x285c, 0x27);
        ecl_write16(0x7950, 0);
        ecl_set_health(0x7afc, 0, 1699);
        ecl_set_health(0x7b0c, 0, 3399);
        ecl_stall(0x7b1c);
        ecl_write16(0x7b04, 0x0200);
        ecl_write16(0x7b14, 0x0c00);
        ecl_write16(0x7bc8, 0);
        ecl_write16(0x7cf4, 0);
        ecl_write16(0x7d0c, 0);
        ecl_write32(0x7d18, 0);
        ecl_write32(0x7d28, 0);
        ecl_write32(0x7d48, 0);
        ecl_write32(0x7d68, 0);
        ecl_write32(0x7d88, 0);
        ecl_write32(0x7da8, 0);
        break;
    case TH06_ST4_RA_BOSS7:
        warpTime = 0x29c7;
        s4b_time();
        ecl_write16(0x2854, 0x23);
        ecl_write32(0x285c, 0x27);
        ecl_write16(0x7950, 0);
        ecl_set_health(0x7afc, 0, 1699);
        ecl_stall(0x7b0c);
        ecl_write16(0x7a74, 41);
        ecl_write16(0x7a94, 41);
        ecl_write16(0x7a54, 1700);
        ecl_write16(0x7a64, 1700);
        ecl_write16(0x7de4, 0);
        ecl_write16(0x7ed0, 0);
        ecl_write16(0x7ee8, 0);
        ecl_write32(0x7ef4, 0);
        ecl_write32(0x7f04, 0);
        ecl_write32(0x7f24, 0);
        ecl_write32(0x7f44, 0);
        ecl_write32(0x7f64, 0);
        ecl_write32(0x7f84, 0);
        break;

    // ── Stage 4 ReimuB ────────────────────────────────────────────
    case TH06_ST4_RB_BOOKS:
        warpTime = 0x0d40;
        break;
    case TH06_ST4_RB_MID1:
        warpTime = 0x1024;
        break;
    case TH06_ST4_RB_BOSS1:
        warpTime = 0x29c7;
        break;
    case TH06_ST4_RB_BOSS2:
        warpTime = 0x29c7;
        s4b_time();
        ecl_set_time(0x2810, 0, 0);
        ecl_stall(0x2820);
        break;
    case TH06_ST4_RB_BOSS3:
        warpTime = 0x29c7;
        s4b_time();
        ecl_write16(0x2854, 0x23);
        ecl_write32(0x285c, 0x25);
        ecl_write16(0x6da0, 0);
        ecl_set_time(0x7440, 0, 0);
        ecl_stall(0x7450);
        break;
    case TH06_ST4_RB_BOSS4:
        warpTime = 0x29c7;
        s4b_time();
        ecl_write16(0x2854, 0x23);
        ecl_write32(0x285c, 0x25);
        ecl_write16(0x6da0, 0);
        ecl_stall(0x7afc); // block advance to BOSS5
        break;
    case TH06_ST4_RB_BOSS5:
        warpTime = 0x29c7;
        s4b_time();
        ecl_write16(0x2854, 0x23);
        ecl_write32(0x285c, 0x27);
        ecl_write16(0x7950, 0);
        ecl_write32(0x7afc, 0);
        ecl_write32(0x7b0c, 0);
        ecl_write32(0x7b2c, 0);
        ecl_write32(0x7b4c, 0);
        ecl_write32(0x7b6c, 0);
        ecl_write32(0x7b8c, 0);
        break;
    case TH06_ST4_RB_BOSS6:
        warpTime = 0x29c7;
        s4b_time();
        ecl_write16(0x2854, 0x23);
        ecl_write32(0x285c, 0x27);
        ecl_write16(0x7950, 0);
        ecl_set_health(0x7afc, 0, 1699);
        ecl_set_health(0x7b0c, 0, 3399);
        ecl_stall(0x7b1c);
        ecl_write16(0x7b04, 0x0200);
        ecl_write16(0x7b14, 0x0c00);
        ecl_write16(0x7bc8, 0);
        ecl_write16(0x7cf4, 0);
        ecl_write16(0x7d0c, 0);
        ecl_write32(0x7d18, 0);
        ecl_write32(0x7d28, 0);
        ecl_write32(0x7d48, 0);
        ecl_write32(0x7d68, 0);
        ecl_write32(0x7d88, 0);
        ecl_write32(0x7da8, 0);
        break;
    case TH06_ST4_RB_BOSS7:
        warpTime = 0x29c7;
        s4b_time();
        ecl_write16(0x2854, 0x23);
        ecl_write32(0x285c, 0x27);
        ecl_write16(0x7950, 0);
        ecl_set_health(0x7afc, 0, 1699);
        ecl_stall(0x7b0c);
        ecl_write16(0x7a74, 41);
        ecl_write16(0x7a94, 41);
        ecl_write16(0x7a54, 1700);
        ecl_write16(0x7a64, 1700);
        ecl_write16(0x7de4, 0);
        ecl_write16(0x7ed0, 0);
        ecl_write16(0x7ee8, 0);
        ecl_write32(0x7ef4, 0);
        ecl_write32(0x7f04, 0);
        ecl_write32(0x7f24, 0);
        ecl_write32(0x7f44, 0);
        ecl_write32(0x7f64, 0);
        ecl_write32(0x7f84, 0);
        break;

    // ── Stage 4 MarisaA ───────────────────────────────────────────
    case TH06_ST4_MA_BOOKS:
        warpTime = 0x0d40;
        break;
    case TH06_ST4_MA_MID1:
        warpTime = 0x1024;
        break;
    case TH06_ST4_MA_BOSS1:
        warpTime = 0x29c7;
        break;
    case TH06_ST4_MA_BOSS2:
        warpTime = 0x29c7;
        s4b_time();
        ecl_set_time(0x2810, 0, 0);
        ecl_stall(0x2820);
        break;
    case TH06_ST4_MA_BOSS3:
        warpTime = 0x29c7;
        s4b_time();
        ecl_write16(0x2854, 0x23);
        ecl_write32(0x285c, 0x25);
        ecl_write16(0x6da0, 0);
        ecl_set_time(0x7440, 0, 0);
        ecl_stall(0x7450);
        break;
    case TH06_ST4_MA_BOSS4:
        warpTime = 0x29c7;
        s4b_time();
        ecl_write16(0x2854, 0x23);
        ecl_write32(0x285c, 0x25);
        ecl_write16(0x6da0, 0);
        ecl_stall(0x7afc); // block advance to BOSS5
        break;
    case TH06_ST4_MA_BOSS5:
        warpTime = 0x29c7;
        s4b_time();
        ecl_write16(0x2854, 0x23);
        ecl_write32(0x285c, 0x27);
        ecl_write16(0x7950, 0);
        ecl_write32(0x7afc, 0);
        ecl_write32(0x7b0c, 0);
        ecl_write32(0x7b2c, 0);
        ecl_write32(0x7b4c, 0);
        ecl_write32(0x7b6c, 0);
        ecl_write32(0x7b8c, 0);
        break;
    case TH06_ST4_MA_BOSS6:
        warpTime = 0x29c7;
        s4b_time();
        ecl_write16(0x2854, 0x23);
        ecl_write32(0x285c, 0x27);
        ecl_write16(0x7950, 0);
        ecl_set_health(0x7afc, 0, 1699);
        ecl_set_health(0x7b0c, 0, 3399);
        ecl_stall(0x7b1c);
        ecl_write16(0x7b04, 0x0200);
        ecl_write16(0x7b14, 0x0c00);
        ecl_write16(0x7bc8, 0);
        ecl_write16(0x7cf4, 0);
        ecl_write16(0x7d0c, 0);
        ecl_write32(0x7d18, 0);
        ecl_write32(0x7d28, 0);
        ecl_write32(0x7d48, 0);
        ecl_write32(0x7d68, 0);
        ecl_write32(0x7d88, 0);
        ecl_write32(0x7da8, 0);
        break;
    case TH06_ST4_MA_BOSS7:
        warpTime = 0x29c7;
        s4b_time();
        ecl_write16(0x2854, 0x23);
        ecl_write32(0x285c, 0x27);
        ecl_write16(0x7950, 0);
        ecl_set_health(0x7afc, 0, 1699);
        ecl_stall(0x7b0c);
        ecl_write16(0x7a74, 41);
        ecl_write16(0x7a94, 41);
        ecl_write16(0x7a54, 1700);
        ecl_write16(0x7a64, 1700);
        ecl_write16(0x7de4, 0);
        ecl_write16(0x7ed0, 0);
        ecl_write16(0x7ee8, 0);
        ecl_write32(0x7ef4, 0);
        ecl_write32(0x7f04, 0);
        ecl_write32(0x7f24, 0);
        ecl_write32(0x7f44, 0);
        ecl_write32(0x7f64, 0);
        ecl_write32(0x7f84, 0);
        break;

    // ── Stage 4 MarisaB ───────────────────────────────────────────
    case TH06_ST4_MB_BOOKS:
        warpTime = 0x0d40;
        break;
    case TH06_ST4_MB_MID1:
        warpTime = 0x1024;
        break;
    case TH06_ST4_MB_BOSS1:
        warpTime = 0x29c7;
        break;
    case TH06_ST4_MB_BOSS2:
        warpTime = 0x29c7;
        s4b_time();
        ecl_set_time(0x2810, 0, 0);
        ecl_stall(0x2820);
        break;
    case TH06_ST4_MB_BOSS3:
        warpTime = 0x29c7;
        s4b_time();
        ecl_write16(0x2854, 0x23);
        ecl_write32(0x285c, 0x25);
        ecl_write16(0x6da0, 0);
        ecl_set_time(0x7440, 0, 0);
        ecl_stall(0x7450);
        break;
    case TH06_ST4_MB_BOSS4:
        warpTime = 0x29c7;
        s4b_time();
        ecl_write16(0x2854, 0x23);
        ecl_write32(0x285c, 0x25);
        ecl_write16(0x6da0, 0);
        ecl_stall(0x7afc); // block advance to BOSS5
        break;
    case TH06_ST4_MB_BOSS5:
        warpTime = 0x29c7;
        s4b_time();
        ecl_write16(0x2854, 0x23);
        ecl_write32(0x285c, 0x27);
        ecl_write16(0x7950, 0);
        ecl_write32(0x7afc, 0);
        ecl_write32(0x7b0c, 0);
        ecl_write32(0x7b2c, 0);
        ecl_write32(0x7b4c, 0);
        ecl_write32(0x7b6c, 0);
        ecl_write32(0x7b8c, 0);
        break;
    case TH06_ST4_MB_BOSS6:
        warpTime = 0x29c7;
        s4b_time();
        ecl_write16(0x2854, 0x23);
        ecl_write32(0x285c, 0x27);
        ecl_write16(0x7950, 0);
        ecl_set_health(0x7afc, 0, 1699);
        ecl_set_health(0x7b0c, 0, 3399);
        ecl_stall(0x7b1c);
        ecl_write16(0x7b04, 0x0200);
        ecl_write16(0x7b14, 0x0c00);
        ecl_write16(0x7bc8, 0);
        ecl_write16(0x7cf4, 0);
        ecl_write16(0x7d0c, 0);
        ecl_write32(0x7d18, 0);
        ecl_write32(0x7d28, 0);
        ecl_write32(0x7d48, 0);
        ecl_write32(0x7d68, 0);
        ecl_write32(0x7d88, 0);
        ecl_write32(0x7da8, 0);
        break;
    case TH06_ST4_MB_BOSS7:
        warpTime = 0x29c7;
        s4b_time();
        ecl_write16(0x2854, 0x23);
        ecl_write32(0x285c, 0x27);
        ecl_write16(0x7950, 0);
        ecl_set_health(0x7afc, 0, 1699);
        ecl_stall(0x7b0c);
        ecl_write16(0x7a74, 41);
        ecl_write16(0x7a94, 41);
        ecl_write16(0x7a54, 1700);
        ecl_write16(0x7a64, 1700);
        ecl_write16(0x7de4, 0);
        ecl_write16(0x7ed0, 0);
        ecl_write16(0x7ee8, 0);
        ecl_write32(0x7ef4, 0);
        ecl_write32(0x7f04, 0);
        ecl_write32(0x7f24, 0);
        ecl_write32(0x7f44, 0);
        ecl_write32(0x7f64, 0);
        ecl_write32(0x7f84, 0);
        break;

    // ── Stage 5 ──────────────────────────────────────────────────
    case TH06_ST5_MID1:
        warpTime = 0x0d2c;
        // bossSubId=0xFFFE: suppress ShouldSkipSpellPracticeDialogue so MsgRead runs,
        // but does NOT trigger the boss-BGM switch in EclManager (that needs 0xFFFF).
        // Do NOT call ecl_write16(0x64a8, 13) — that overwrites the MsgRead opcode with
        // an unknown op, silently disabling the dialogue.
        return (warpTime & 0xFFFF) | (0xFFFE << 16);
    case TH06_ST5_MID2:
        warpTime = 0x0d2c;
        ecl_write16(0x64a4, 0);
        ecl_set_health(0x14d8, 0x1e, 0x2c5);
        ecl_stall(0x14e8);
        break;
    case TH06_ST5_BOSS1:
        warpTime = 0x1e18;
        s5b();
        break;
    case TH06_ST5_BOSS2:
        warpTime = 0x1e18;
        s5b();
        ecl_set_time(0x2348, 0, 0);
        ecl_stall(0x2358);
        break;
    case TH06_ST5_BOSS3:
        warpTime = 0x1e18;
        s5b();
        ecl_write32(0x235c, 0);
        ecl_write32(0x2368, 0x22);
        ecl_write16(0x3778, 0);
        ecl_write16(0x3828, 0);
        break;
    case TH06_ST5_BOSS4:
        warpTime = 0x1e18;
        s5b();
        ecl_write32(0x235c, 0);
        ecl_write32(0x2368, 0x22);
        ecl_write16(0x3778, 0);
        ecl_write16(0x3828, 0);
        ecl_set_time(0x38b8, 0, 0);
        ecl_stall(0x38c8);
        break;
    case TH06_ST5_BOSS5:
        warpTime = 0x1e18;
        s5b();
        ecl_write32(0x235c, 0x1e);
        ecl_write32(0x2368, 0x29);
        ecl_write16(0x4638, 0);
        ecl_write16(0x46e8, 0);
        break;
    case TH06_ST5_BOSS6:
        warpTime = 0x1e18;
        s5b();
        ecl_write32(0x235c, 0x1e);
        ecl_write32(0x2368, 0x29);
        ecl_write16(0x4638, 0);
        ecl_write16(0x46e8, 0);
        ecl_write32(0x235c, 0);
        ecl_set_time(0x4758, 0, 0);
        ecl_stall(0x4768);
        break;

    // ── Stage 6 ──────────────────────────────────────────────────
    case TH06_ST6_MID1:
        warpTime = 0x0a04;
        ecl_write16(0x77f2, 0);
        ecl_write32(0x9e8, 1);
        break;
    case TH06_ST6_MID2: {
        int hp = (shot > 1) ? 1099 : (shot == 0 ? 749 : 999);
        warpTime = 0x0a04;
        ecl_write16(0x77f2, 0);
        ecl_write32(0x0d2c, 0);
        ecl_set_health(0x0d3c, 0, hp);
        ecl_stall(0x0d4c);
        break;
    }
    case TH06_ST6_BOSS1:
        warpTime = 0x0c61;
        s6b();
        break;
    case TH06_ST6_BOSS2:
        warpTime = 0x0c61;
        s6b();
        ecl_set_time(0x1706, 0, 0);
        ecl_stall(0x1716);
        break;
    case TH06_ST6_BOSS3:
        warpTime = 0x0c61;
        s6b();
        ecl_write32(0x1706, 0);
        ecl_write32(0x171a, 0);
        ecl_write32(0x1726, 0x13);
        ecl_write16(0x1b8e, 0);
        ecl_write16(0x1c3e, 0);
        break;
    case TH06_ST6_BOSS4:
        warpTime = 0x0c61;
        s6b();
        ecl_write32(0x1706, 0);
        ecl_write32(0x171a, 0);
        ecl_write32(0x1726, 0x13);
        ecl_write16(0x1b8e, 0);
        ecl_write16(0x1c3e, 0);
        ecl_set_time(0x1cf2, 0, 0);
        ecl_stall(0x1d02);
        break;
    case TH06_ST6_BOSS5:
        warpTime = 0x0c61;
        s6b();
        ecl_write32(0x1706, 0);
        ecl_write32(0x171a, 0x1e);
        ecl_write32(0x1726, 0x17);
        ecl_write16(0x28e2, 0);
        ecl_write16(0x2992, 0);
        break;
    case TH06_ST6_BOSS6:
        warpTime = 0x0c61;
        s6b();
        ecl_write32(0x1706, 0);
        ecl_write32(0x171a, 0x1e);
        ecl_write32(0x1726, 0x17);
        ecl_write16(0x28e2, 0);
        ecl_write16(0x2992, 0);
        ecl_write32(0x171a, 0);
        ecl_set_time(0x2a22, 0, 0);
        ecl_stall(0x2a32);
        break;
    case TH06_ST6_BOSS7:
        warpTime = 0x0c61;
        s6b();
        ecl_write32(0x1706, 0);
        ecl_write32(0x171a, 0);
        ecl_write32(0x1726, 0x1a);
        ecl_write16(0x2d8e, 0);
        ecl_write16(0x2e3e, 0);
        break;
    case TH06_ST6_BOSS8:
        warpTime = 0x0c61;
        s6b();
        ecl_write32(0x1706, 0);
        ecl_write32(0x171a, 0);
        ecl_write32(0x1726, 0x1a);
        ecl_write16(0x2d8e, 0);
        ecl_write16(0x2e3e, 0);
        ecl_set_time(0x2ee6, 0, 0);
        ecl_stall(0x2ef6);
        break;
    case TH06_ST6_BOSS9:
        warpTime = 0x0c61;
        s6b();
        ecl_write32(0x1706, 0);
        ecl_write32(0x171a, 0);
        ecl_write32(0x1732, 0);
        ecl_write32(0x1726, 0x2b);
        ecl_write16(0x1722, 0x0300);
        ecl_write16(0x1736, 0x23);
        ecl_write16(0x173a, 0x0c00);
        ecl_write32(0x173e, 0x2c);
        ecl_write16(0x5c8e, 0);
        ecl_write16(0x6290, 0);
        ecl_write32(0x1622, 0xffffffff);
        break;

    // ── Stage 7 (Extra) ──────────────────────────────────────────
    case TH06_ST7_MID1:
        warpTime = 0x1284;
        ecl_write16(0x0d2e2, 0);
        break;
    case TH06_ST7_MID2:
        warpTime = 0x1284;
        ecl_write16(0x0d2e2, 0);
        ecl_write32(0x1b14, 0x12);
        ecl_write16(0x1c2c, 0);
        break;
    case TH06_ST7_MID3:
        warpTime = 0x1284;
        ecl_write16(0x0d2e2, 0);
        ecl_write32(0x1b14, 0x13);
        ecl_write16(0x1d7c, 0);
        break;
    case TH06_ST7_END_NS1:
        warpTime = 0x2192;
        s7b_n1();
        break;
    case TH06_ST7_END_S1:
        warpTime = 0x2192;
        s7b_n1();
        ecl_set_time(0x344e, 0, 0);
        break;
    case TH06_ST7_END_NS2:
        warpTime = 0x2192;
        s7b_n1();
        s7b_call();
        ecl_write32(0x345a, 0x23);
        ecl_write16(0x4210, 0);
        break;
    case TH06_ST7_END_S2:
        warpTime = 0x2192;
        s7b_n1();
        s7b_call();
        ecl_write32(0x345a, 0x23);
        ecl_write16(0x4210, 0);
        ecl_write32(0x41fc, 0);
        ecl_write32(0x420c, 0);
        ecl_set_time(0x421c, 0, 0);
        ecl_stall(0x422c);
        break;
    case TH06_ST7_END_NS3:
        warpTime = 0x2192;
        s7b_n1();
        s7b_call();
        ecl_write32(0x345a, 0x26);
        ecl_write16(0x4c62, 0);
        break;
    case TH06_ST7_END_S3:
        warpTime = 0x2192;
        s7b_n1();
        s7b_call();
        ecl_write32(0x345a, 0x26);
        ecl_write16(0x4c62, 0);
        ecl_write32(0x4c4e, 0);
        ecl_write32(0x4c5e, 0);
        ecl_set_time(0x4c6e, 0, 0);
        ecl_stall(0x4c7e);
        break;
    case TH06_ST7_END_NS4:
        warpTime = 0x2192;
        s7b_n1();
        s7b_call();
        ecl_write32(0x345a, 0x2b);
        ecl_write16(0x59cc, 0);
        break;
    case TH06_ST7_END_S4:
        warpTime = 0x2192;
        s7b_n1();
        s7b_call();
        ecl_write32(0x345a, 0x2b);
        ecl_write16(0x59cc, 0);
        ecl_write32(0x59b8, 0);
        ecl_write32(0x59c8, 0);
        ecl_set_time(0x59d8, 0, 0);
        ecl_stall(0x59e8);
        break;
    case TH06_ST7_END_NS5:
        warpTime = 0x2192;
        s7b_n1();
        s7b_call();
        ecl_write32(0x345a, 0x2f);
        ecl_write16(0x63a2, 0);
        break;
    case TH06_ST7_END_S5:
        warpTime = 0x2192;
        s7b_n1();
        s7b_call();
        ecl_write32(0x345a, 0x2f);
        ecl_write16(0x63a2, 0);
        ecl_write32(0x638e, 0);
        ecl_write32(0x639e, 0);
        ecl_set_time(0x63ae, 0, 0);
        ecl_stall(0x63be);
        break;
    case TH06_ST7_END_NS6:
        warpTime = 0x2192;
        s7b_n1();
        s7b_call();
        ecl_write32(0x345a, 0x31);
        ecl_write16(0x6b1c, 0);
        break;
    case TH06_ST7_END_S6:
        warpTime = 0x2192;
        s7b_n1();
        s7b_call();
        ecl_write32(0x345a, 0x31);
        ecl_write16(0x6b1c, 0);
        ecl_write32(0x6b08, 0);
        ecl_write32(0x6b18, 0);
        ecl_set_time(0x6b28, 0, 0);
        ecl_stall(0x6b38);
        break;
    case TH06_ST7_END_NS7:
        warpTime = 0x2192;
        s7b_n1();
        s7b_call();
        ecl_write32(0x345a, 0x35);
        ecl_write16(0x78aa, 0);
        break;
    case TH06_ST7_END_S7:
        warpTime = 0x2192;
        s7b_n1();
        s7b_call();
        ecl_write32(0x345a, 0x35);
        ecl_write16(0x78aa, 0);
        ecl_write32(0x7896, 0);
        ecl_write32(0x78a6, 0);
        ecl_set_time(0x78b6, 0, 0);
        ecl_stall(0x78c6);
        break;
    case TH06_ST7_END_NS8:
        warpTime = 0x2192;
        s7b_n1();
        s7b_call();
        ecl_write32(0x345a, 0x38);
        ecl_write16(0x8508, 0);
        break;
    case TH06_ST7_END_S8:
        warpTime = 0x2192;
        s7b_n1();
        s7b_call();
        ecl_write32(0x345a, 0x38);
        ecl_write16(0x8508, 0);
        ecl_write32(0x84f4, 0);
        ecl_write32(0x8504, 0);
        ecl_set_time(0x8514, 0, 0);
        ecl_stall(0x8524);
        break;
    case TH06_ST7_END_S9:
        warpTime = 0x2192;
        s7b_n1();
        s7b_call();
        ecl_write32(0x345a, 0x3b);
        ecl_write16(0x940a, 0);
        ecl_write32(0x93f6, 0);
        ecl_write32(0x9406, 0);
        ecl_write32(0x9416, 0);
        ecl_write32(0x9422, 0);
        ecl_write32(0x943a, 0);
        ecl_write32(0x9466, 0);
        ecl_write32(0x9472, 0);
        ecl_write32(0x9482, 0);
        break;
    case TH06_ST7_END_S10:
        warpTime = 0x2192;
        s7b_n1();
        s7b_call();
        ecl_write32(0x345a, 0x43);
        ecl_write16(0x0bea4, 0);
        ecl_write32(0x0be90, 0);
        ecl_write32(0x0bea0, 0);
        ecl_write32(0x0beb0, 0);
        ecl_write32(0x0bed0, 0);
        ecl_write32(0x0bef0, 0);
        ecl_write32(0x0bf10, 0);
        ecl_write32(0x0bf30, 0);
        ecl_write32(0x0bf50, 0);
        ecl_write32(0x0bf5c, 0);
        ecl_write32(0x0bf74, 0);
        ecl_write32(0x0bfa0, 0);
        ecl_write32(0x0bfac, 0);
        ecl_write32(0x0bfbc, 0);
        break;

    // ── Full boss-run sections ────────────────────────────────────
    case TH06_ST1_BOSS_ALL:
        warpTime = 0x149f;
        ecl_write32(0x16a6, 0);
        ecl_write32(0x16c6, 0);
        ecl_write32(0x16e6, 0x50);
        return (warpTime & 0xFFFF) | (0xFFFF << 16);
    case TH06_ST2_BOSS_ALL:
        warpTime = 0x1760;
        return (warpTime & 0xFFFF) | (0xFFFF << 16);
    case TH06_ST3_BOSS_ALL:
        warpTime = 0x16d4;
        s3b_n1();
        return (warpTime & 0xFFFF) | (0xFFFF << 16);
    case TH06_ST4_RA_BOSS_ALL:
        warpTime = 0x29c7;
        return (warpTime & 0xFFFF) | (0xFFFF << 16);
    case TH06_ST4_RB_BOSS_ALL:
        warpTime = 0x29c7;
        return (warpTime & 0xFFFF) | (0xFFFF << 16);
    case TH06_ST4_MA_BOSS_ALL:
        warpTime = 0x29c7;
        return (warpTime & 0xFFFF) | (0xFFFF << 16);
    case TH06_ST4_MB_BOSS_ALL:
        warpTime = 0x29c7;
        return (warpTime & 0xFFFF) | (0xFFFF << 16);
    case TH06_ST5_BOSS_ALL:
        warpTime = 0x1e18;
        s5b();
        return (warpTime & 0xFFFF) | (0xFFFF << 16);
    case TH06_ST6_BOSS_ALL:
        warpTime = 0x0c61;
        s6b();
        return (warpTime & 0xFFFF) | (0xFFFF << 16);
    case TH06_ST7_BOSS_ALL:
        warpTime = 0x2192;
        s7b_n1();
        return (warpTime & 0xFFFF) | (0xFFFF << 16);

    default:
        break;
    }

    // warpTime in low 16 bits; isSpell flag in bit 16 (used by EclManager to decide
    // whether SPELLCARDEND should trigger practice termination).
    return (warpTime & 0xFFFF) | (IsSpellSection((TH06Section)spellIndex) ? (1 << 16) : 0);
}
