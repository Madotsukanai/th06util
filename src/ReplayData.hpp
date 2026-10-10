#pragma once

#include "inttypes.hpp"

namespace th06
{
struct ReplayDataInput
{
    i32 frameNum;
    u16 inputKey;
    u16 padding;
};

struct OnlineReplayDataHeader
{
    char magic[4];
    u16 version;
    u16 reserved;
    u32 stageReplayDataOffsets[7];
    u32 stageReplayDataSizes[7];
};
ZUN_ASSERT_SIZE(OnlineReplayDataHeader, 0x40);

struct StageReplayData
{
    i32 score;
    i16 randomSeed;
    i16 pointItemsCollected;
    u8 power;
    i8 livesRemaining;
    i8 bombsRemaining;
    u8 rank;
    i8 powerItemCountForScore;
    u8 spellSection;
    i8 padding[2];
    ReplayDataInput replayInputs[53998];
};
ZUN_ASSERT_SIZE(StageReplayData, 0x69780);

struct ReplayData
{
    char magic[4];
    u16 version;
    u8 shottypeChara;
    u8 difficulty;
    i32 checksum;
    u8 rngValue1;
    u8 rngValue2;
    i8 key;
    i8 rngValue3;
    char date[9];
    char name[8];
    i32 score;
    f32 slowdownRate2;
    f32 slowdownRate;
    f32 slowdownRate3;
    StageReplayData *stageReplayData[7];
};
ZUN_ASSERT_SIZE(ReplayData, 0x50);
}; // namespace th06
