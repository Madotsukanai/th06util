#pragma once

#include "Chain.hpp"
#include "ChainPriorities.hpp"
#include "ReplayData.hpp"
#include "inttypes.hpp"

namespace th06
{

struct ThpracReplayInfo
{
    i32 isThprac;
    i32 section;
    i32 stage;
    i32 life;
    i32 bomb;
    i32 power;
    i32 frame;
};

struct ReplayManager
{

    static i32 ReadThpracInfo(const u8 *rawFileData, i32 fileSize, ThpracReplayInfo *out);

    static ZunResult RegisterChain(i32 isDemo, char *replayFile);
    static ChainCallbackResult OnUpdate(ReplayManager *mgr);
    static ChainCallbackResult OnUpdateDemoHighPrio(ReplayManager *mgr);
    static ChainCallbackResult OnUpdateDemoLowPrio(ReplayManager *mgr);
    static ChainCallbackResult OnDraw(ReplayManager *mgr);
    static ZunResult AddedCallback(ReplayManager *mgr);
    static ZunResult AddedCallbackDemo(ReplayManager *mgr);
    static ZunResult DeletedCallback(ReplayManager *mgr);
    static void StopRecording();
    static void SaveReplay(char *replay_path, char *param_2);
    static ZunResult ValidateReplayData(ReplayData *data, i32 fileSize);
    static OnlineReplayDataHeader *FindOnlineReplayData(ReplayData *data, i32 fileSize);

    ReplayManager()
    {
    }

    i32 IsDemo()
    {
        return this->isDemo;
    }

    i32 frameId;
    ReplayData *replayData;
    i32 isDemo;
    char *replayFile;
    u8 unk10[52];
    u16 unk44;
    ReplayDataInput *replayInputs;
    ReplayDataInput *replayInputsP2;
    ReplayDataInput *replayInputStageBookmarks[7];
    ReplayDataInput *replayInputStageBookmarksP2[7];
    StageReplayData *stageReplayDataP2[7];
    i32 isOnlineReplay;
    ChainElem *calcChain;
    ChainElem *drawChain;
    ChainElem *calcChainDemoHighPrio;
};
extern ReplayManager *g_ReplayManager;

#define TH_BUTTON_REPLAY_CAPTURE                                                                                       \
    (TH_BUTTON_SHOOT | TH_BUTTON_BOMB | TH_BUTTON_FOCUS | TH_BUTTON_SKIP | TH_BUTTON_DIRECTION)
}; // namespace th06
