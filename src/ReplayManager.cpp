#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "Backup.hpp"
#include "Controller.hpp"
#include "FileSystem.hpp"
#include "GameManager.hpp"
#include "Gui.hpp"
#include "Network.hpp"
#include "Player.hpp"
#include "ReplayManager.hpp"
#include "Rng.hpp"
#include "Supervisor.hpp"
#include "utils.hpp"

namespace th06
{
ReplayManager *g_ReplayManager;
u8 g_ReplayP1ShottypeChara = 0;
u8 g_ReplayP2ShottypeChara = 0;
static i32 s_SavedIsOnline = 0;

#define ONLINE_REPLAY_MAGIC "T6ON"
#define ONLINE_REPLAY_VERSION 1

static void RecordReplayInput(ReplayDataInput **cursor, ReplayDataInput **bookmark, i32 frameId, u16 inputs)
{
    if (inputs != (*cursor)->inputKey)
    {
        *cursor += 1;
        *bookmark = *cursor + 1;
        (*cursor)->frameNum = frameId;
        (*cursor)->inputKey = inputs;
        (*cursor)->padding = 0;
    }
}

static const char *FindThpracJson(const u8 *rawFileData, i32 fileSize, u32 *outLen)
{
    u32 magic;
    u32 paramSize;
    const char *json;

    if (fileSize < (i32)(sizeof(ReplayData) + 8))
        return NULL;

    memcpy(&magic, rawFileData + fileSize - 4, 4);
    if (magic != 0x43415250) /* */
        return NULL;

    memcpy(&paramSize, rawFileData + fileSize - 8, 4);
    if (paramSize == 0 || paramSize > 4096 || (i32)(paramSize + 8) > fileSize)
        return NULL;

    json = (const char *)rawFileData + fileSize - 8 - paramSize;
    if (json[0] != '{')
        return NULL;

    if (outLen)
        *outLen = paramSize;
    return json;
}

static i32 ThpracJsonInt(const char *json, u32 jsonLen, const char *key)
{
    const char *end = json + jsonLen;
    const char *p = json;
    i32 keyLen = (i32)strlen(key);

    while (p < end - keyLen - 2)
    {
        if (*p == '"' && memcmp(p + 1, key, keyLen) == 0 && p[keyLen + 1] == '"')
        {
            p += keyLen + 2;
            while (p < end && (*p == ' ' || *p == ':'))
                p++;
            if (p < end && (*p == '-' || (*p >= '0' && *p <= '9')))
                return (i32)atoi(p);
            break;
        }
        p++;
    }
    return -1;
}

static i32 ThpracJsonBool(const char *json, u32 jsonLen, const char *key)
{
    const char *end = json + jsonLen;
    const char *p = json;
    i32 keyLen = (i32)strlen(key);

    while (p < end - keyLen - 2)
    {
        if (*p == '"' && memcmp(p + 1, key, keyLen) == 0 && p[keyLen + 1] == '"')
        {
            p += keyLen + 2;
            while (p < end && (*p == ' ' || *p == ':'))
                p++;
            if (p + 4 <= end && memcmp(p, "true", 4) == 0)
                return 1;
            if (p + 5 <= end && memcmp(p, "false", 5) == 0)
                return 0;
            break;
        }
        p++;
    }
    return -1;
}

static i32 FindThpracJsonInt(const u8 *rawFileData, i32 fileSize, const char *key)
{
    u32 len;
    const char *json = FindThpracJson(rawFileData, fileSize, &len);
    if (!json)
        return -1;
    return ThpracJsonInt(json, len, key);
}

i32 ReplayManager::ReadThpracInfo(const u8 *rawFileData, i32 fileSize, ThpracReplayInfo *out)
{
    u32 len;
    const char *json;

    out->isThprac = 0;
    out->section = -1;
    out->stage = -1;
    out->life = -1;
    out->bomb = -1;
    out->power = -1;
    out->frame = -1;

    if (rawFileData == NULL)
    {
        return 0;
    }
    json = FindThpracJson(rawFileData, fileSize, &len);
    if (json == NULL)
    {
        return 0;
    }
    out->isThprac = 1;
    out->section = ThpracJsonInt(json, len, "section");
    out->stage = ThpracJsonInt(json, len, "stage");
    out->life = ThpracJsonInt(json, len, "life");
    out->bomb = ThpracJsonInt(json, len, "bomb");
    out->power = ThpracJsonInt(json, len, "power");
    out->frame = ThpracJsonInt(json, len, "frame");
    return 1;
}

ZunResult ReplayManager::ValidateReplayData(ReplayData *data, i32 fileSize)
{
    u8 *checksumCursor;
    u32 checksum;
    u8 *obfuscateCursor;
    u8 obfOffset;
    i32 idx;
    ReplayData *decryptedData;

    decryptedData = data;

    if (decryptedData == NULL)
    {
        return ZUN_ERROR;
    }

    if (*(i32 *)decryptedData->magic != *(i32 *)"T6RP")
    {
        return ZUN_ERROR;
    }

    obfuscateCursor = (u8 *)&decryptedData->rngValue3;
    obfOffset = decryptedData->key;
    for (idx = 0; idx < fileSize - (i32)offsetof(ReplayData, rngValue3); idx += 1, obfuscateCursor += 1)
    {
        *obfuscateCursor -= obfOffset;
        obfOffset += 7;
    }

    checksumCursor = (u8 *)&decryptedData->key;
    checksum = 0x3f000318;
    for (idx = 0; idx < fileSize - (i32)offsetof(ReplayData, key); idx += 1, checksumCursor += 1)
    {
        checksum += *checksumCursor;
    }

    if (checksum != decryptedData->checksum)
    {
        return ZUN_ERROR;
    }

    if (decryptedData->version != GAME_VERSION)
    {
        return ZUN_ERROR;
    }

    return ZUN_SUCCESS;
}

OnlineReplayDataHeader *ReplayManager::FindOnlineReplayData(ReplayData *data, i32 fileSize)
{
    u8 *cursor;
    i32 idx;

    if (data == NULL || fileSize < (i32)(sizeof(ReplayData) + sizeof(OnlineReplayDataHeader)))
    {
        return NULL;
    }
    cursor = (u8 *)data + sizeof(ReplayData);
    for (idx = sizeof(ReplayData); idx <= fileSize - (i32)sizeof(OnlineReplayDataHeader); idx++, cursor++)
    {
        OnlineReplayDataHeader *header = (OnlineReplayDataHeader *)cursor;
        if (memcmp(header->magic, ONLINE_REPLAY_MAGIC, 4) == 0 && header->version == ONLINE_REPLAY_VERSION)
        {
            return header;
        }
    }
    return NULL;
}

ZunResult ReplayManager::RegisterChain(i32 isDemo, char *replayFile)
{
    ReplayManager *replayMgr;
    s_SavedIsOnline = g_IsOnline;

    if (g_Supervisor.framerateMultiplier < 0.99f && !isDemo)
    {
        return ZUN_SUCCESS;
    }
    g_Supervisor.framerateMultiplier = 1.0f;
    if (g_ReplayManager == NULL)
    {
        replayMgr = new ReplayManager();
        g_ReplayManager = replayMgr;
        replayMgr->replayData = NULL;
        replayMgr->isDemo = isDemo;
        replayMgr->replayFile = replayFile;
        replayMgr->replayInputsP2 = NULL;
        replayMgr->isOnlineReplay = false;
        for (i32 idx = 0; idx < ARRAY_SIZE_SIGNED(replayMgr->stageReplayDataP2); idx++)
        {
            replayMgr->stageReplayDataP2[idx] = NULL;
            replayMgr->replayInputStageBookmarksP2[idx] = NULL;
        }
        switch (isDemo)
        {
        case false:
            replayMgr->calcChain = g_Chain.CreateElem((ChainCallback)ReplayManager::OnUpdate);
            replayMgr->calcChain->addedCallback = (ChainAddedCallback)AddedCallback;
            replayMgr->calcChain->deletedCallback = (ChainDeletedCallback)DeletedCallback;
            replayMgr->drawChain = g_Chain.CreateElem((ChainCallback)ReplayManager::OnDraw);
            replayMgr->calcChain->arg = replayMgr;
            if (g_Chain.AddToCalcChain(replayMgr->calcChain, TH_CHAIN_PRIO_CALC_REPLAYMANAGER))
            {
                return ZUN_ERROR;
            }
            replayMgr->calcChainDemoHighPrio = NULL;
            break;
        case true:
            replayMgr->calcChain = g_Chain.CreateElem((ChainCallback)ReplayManager::OnUpdateDemoHighPrio);
            replayMgr->calcChain->addedCallback = (ChainAddedCallback)AddedCallbackDemo;
            replayMgr->calcChain->deletedCallback = (ChainDeletedCallback)DeletedCallback;
            replayMgr->drawChain = g_Chain.CreateElem((ChainCallback)ReplayManager::OnDraw);
            replayMgr->calcChain->arg = replayMgr;
            if (g_Chain.AddToCalcChain(replayMgr->calcChain, TH_CHAIN_PRIO_CALC_LOW_PRIO_REPLAYMANAGER_DEMO))
            {
                return ZUN_ERROR;
            }
            replayMgr->calcChainDemoHighPrio = g_Chain.CreateElem((ChainCallback)ReplayManager::OnUpdateDemoLowPrio);
            replayMgr->calcChainDemoHighPrio->arg = replayMgr;
            g_Chain.AddToCalcChain(replayMgr->calcChainDemoHighPrio, TH_CHAIN_PRIO_CALC_HIGH_PRIO_REPLAYMANAGER_DEMO);
            break;
        }
        replayMgr->drawChain->arg = replayMgr;
        g_Chain.AddToDrawChain(replayMgr->drawChain, TH_CHAIN_PRIO_DRAW_REPLAYMANAGER);
    }
    else
    {
        switch (isDemo)
        {
        case false:
            AddedCallback(g_ReplayManager);
            break;
        case true:
            g_ReplayTotalFrames += g_ReplayManager->frameId;
            return AddedCallbackDemo(g_ReplayManager);
            break;
        }
    }
    return ZUN_SUCCESS;
}

#define TH_BUTTON_REPLAY_CAPTURE                                                                                       \
    (TH_BUTTON_SHOOT | TH_BUTTON_BOMB | TH_BUTTON_FOCUS | TH_BUTTON_SKIP | TH_BUTTON_DIRECTION)

ChainCallbackResult ReplayManager::OnUpdate(ReplayManager *mgr)
{
    u16 inputs;
    u16 inputsP2;

    if (!g_GameManager.isInMenu)
    {
        return CHAIN_CALLBACK_RESULT_CONTINUE;
    }
    inputs = g_IsOnline ? (g_Player.input & TH_BUTTON_REPLAY_CAPTURE) : IS_PRESSED(TH_BUTTON_REPLAY_CAPTURE);
    RecordReplayInput(&mgr->replayInputs, &mgr->replayInputStageBookmarks[g_GameManager.currentStage - 1], mgr->frameId,
                      inputs);
    if (g_IsOnline && mgr->replayInputsP2 != NULL)
    {
        inputsP2 = g_Player2.input & TH_BUTTON_REPLAY_CAPTURE;
        RecordReplayInput(&mgr->replayInputsP2, &mgr->replayInputStageBookmarksP2[g_GameManager.currentStage - 1],
                          mgr->frameId, inputsP2);
    }
    mgr->frameId += 1;
    return CHAIN_CALLBACK_RESULT_CONTINUE;
}

ChainCallbackResult ReplayManager::OnUpdateDemoLowPrio(ReplayManager *mgr)
{
    if (!g_GameManager.isInMenu)
    {
        return CHAIN_CALLBACK_RESULT_CONTINUE;
    }
    if (g_Gui.HasCurrentMsgIdx() && g_Gui.IsDialogueSkippable() && mgr->frameId % 3 != 2)
    {
        return CHAIN_CALLBACK_RESULT_RESTART_FROM_FIRST_JOB;
    }
    return CHAIN_CALLBACK_RESULT_CONTINUE;
}

ChainCallbackResult ReplayManager::OnUpdateDemoHighPrio(ReplayManager *mgr)
{
    if (!g_GameManager.isInMenu)
    {
        return CHAIN_CALLBACK_RESULT_CONTINUE;
    }

    if (Supervisor::IsQuickSettingsOpen())
    {
        return CHAIN_CALLBACK_RESULT_CONTINUE;
    }

    if (mgr->frameId < 5)
    {
        utils::LogDebug("replay frame=%d key=0x%x playerPos=(%.1f,%.1f)", mgr->frameId,
                        (u32)mgr->replayInputs->inputKey, g_Player.positionCenter.x, g_Player.positionCenter.y);
    }
    while (mgr->frameId >= mgr->replayInputs[1].frameNum)
    {
        mgr->replayInputs += 1;
    }
    if (mgr->isOnlineReplay && mgr->replayInputsP2 != NULL)
    {
        while (mgr->frameId >= mgr->replayInputsP2[1].frameNum)
        {
            mgr->replayInputsP2 += 1;
        }
    }
    g_CurFrameInput = (g_CurFrameInput & ~TH_BUTTON_REPLAY_CAPTURE) | mgr->replayInputs->inputKey;
    g_Player.previousFrameInput = g_Player.input;
    g_Player.input = mgr->replayInputs->inputKey;
    g_Player2.previousFrameInput = g_Player2.input;
    g_Player2.input = (mgr->isOnlineReplay && mgr->replayInputsP2 != NULL) ? mgr->replayInputsP2->inputKey : 0;
    if (g_LastFrameInput == g_CurFrameInput)
    {
        g_NumOfFramesInputsWereHeld++;
        g_IsEigthFrameOfHeldInput =
            (g_NumOfFramesInputsWereHeld >= 30 && (g_NumOfFramesInputsWereHeld % 8 == 0)) ? 1 : 0;
        if (g_NumOfFramesInputsWereHeld >= 38)
        {
            g_NumOfFramesInputsWereHeld = 30;
        }
    }
    else
    {
        g_NumOfFramesInputsWereHeld = 0;
        g_IsEigthFrameOfHeldInput = 0;
    }
    mgr->frameId += 1;
    return CHAIN_CALLBACK_RESULT_CONTINUE;
}

ChainCallbackResult ReplayManager::OnDraw(ReplayManager *mgr)
{
    return CHAIN_CALLBACK_RESULT_CONTINUE;
}

__inline StageReplayData *AllocateStageReplayData(i32 size)
{
    return (StageReplayData *)malloc(size);
}

__inline void ReleaseReplayData(void *data)
{
    return free(data);
}

__inline void ReleaseStageReplayData(void *data)
{
    return free(data);
}

ZunResult ReplayManager::AddedCallback(ReplayManager *mgr)
{
    StageReplayData *stageReplayData;
    StageReplayData *oldStageReplayData;
    i32 idx;

    mgr->frameId = 0;
    g_LastFrameInput = 0;
    g_CurFrameInput = 0;
    g_NumOfFramesInputsWereHeld = 0;
    g_IsEigthFrameOfHeldInput = 0;
    if (mgr->replayData == NULL)
    {
        mgr->replayData = new ReplayData();
        memcpy(&mgr->replayData->magic[0], "T6RP", 4);
        mgr->replayData->shottypeChara = g_GameManager.character * 2 + g_GameManager.shotType;
        mgr->replayData->version = 0x102;
        mgr->replayData->difficulty = g_GameManager.difficulty;
        memcpy(&mgr->replayData->name, "NO NAME", 4);
        for (idx = 0; idx < ARRAY_SIZE_SIGNED(mgr->replayData->stageReplayData); idx += 1)
        {
            mgr->replayData->stageReplayData[idx] = NULL;
            mgr->stageReplayDataP2[idx] = NULL;
            mgr->replayInputStageBookmarksP2[idx] = NULL;
        }
    }
    else
    {
        if (g_GameManager.currentStage >= 2)
        {
            oldStageReplayData = mgr->replayData->stageReplayData[g_GameManager.currentStage - 2];
            if (oldStageReplayData == NULL)
            {
                return ZUN_ERROR;
            }
            oldStageReplayData->score = g_GameManager.score;
        }
    }
    if (mgr->replayData->stageReplayData[g_GameManager.currentStage - 1] != NULL)
    {
        utils::DebugPrint2("error : replay.cpp");
    }
    mgr->replayData->stageReplayData[g_GameManager.currentStage - 1] = AllocateStageReplayData(sizeof(StageReplayData));
    stageReplayData = mgr->replayData->stageReplayData[g_GameManager.currentStage - 1];
    stageReplayData->bombsRemaining = g_GameManager.bombsRemaining;
    stageReplayData->livesRemaining = g_GameManager.livesRemaining;
    stageReplayData->power = g_GameManager.currentPower;
    stageReplayData->rank = g_GameManager.rank;
    stageReplayData->pointItemsCollected = g_GameManager.pointItemsCollected;
    stageReplayData->randomSeed = g_GameManager.randomSeed;
    stageReplayData->powerItemCountForScore = g_GameManager.powerItemCountForScore;
    stageReplayData->spellSection = (u8)g_IsSpellPracticeGame;
    mgr->replayInputs = stageReplayData->replayInputs;
    mgr->replayInputs->frameNum = 0;
    mgr->replayInputs->inputKey = 0;
    mgr->replayInputs->padding = 0;
    mgr->replayInputsP2 = NULL;
    if (g_IsOnline)
    {
        mgr->stageReplayDataP2[g_GameManager.currentStage - 1] = AllocateStageReplayData(sizeof(StageReplayData));
        memset(mgr->stageReplayDataP2[g_GameManager.currentStage - 1], 0, sizeof(StageReplayData));
        mgr->replayInputsP2 = mgr->stageReplayDataP2[g_GameManager.currentStage - 1]->replayInputs;
        mgr->replayInputsP2->frameNum = 0;
        mgr->replayInputsP2->inputKey = 0;
        mgr->replayInputsP2->padding = 0;
    }
    mgr->unk44 = 0;
    return ZUN_SUCCESS;
}

ZunResult ReplayManager::AddedCallbackDemo(ReplayManager *mgr)
{
    i32 idx;
    StageReplayData *replayData;
    OnlineReplayDataHeader *onlineReplayData;
    const char *thpracJson;
    u32 thpracJsonLen;

    static i32 thpracSection;
    static i32 thpracDlg;
    static i32 thpracLife;
    static i32 thpracBomb;
    static i32 thpracPower;
    static i32 thpracScore;
    static i32 thpracGraze;
    static i32 thpracPoint;
    static i32 thpracRank;
    static i32 thpracRankLock;

    mgr->frameId = 0;
    g_LastFrameInput = 0;
    g_CurFrameInput = 0;
    g_NumOfFramesInputsWereHeld = 0;
    g_IsEigthFrameOfHeldInput = 0;
    if (mgr->replayData == NULL)
    {
        mgr->replayData = (ReplayData *)FileSystem::OpenPath(mgr->replayFile, g_GameManager.demoMode == 0);
        thpracJson = FindThpracJson((u8 *)mgr->replayData, g_LastFileSize, &thpracJsonLen);
        if (thpracJson != NULL)
        {
            thpracSection = ThpracJsonInt(thpracJson, thpracJsonLen, "section");
            thpracDlg = ThpracJsonBool(thpracJson, thpracJsonLen, "dlg");
            thpracLife = ThpracJsonInt(thpracJson, thpracJsonLen, "life");
            thpracBomb = ThpracJsonInt(thpracJson, thpracJsonLen, "bomb");
            thpracPower = ThpracJsonInt(thpracJson, thpracJsonLen, "power");
            thpracScore = ThpracJsonInt(thpracJson, thpracJsonLen, "score");
            thpracGraze = ThpracJsonInt(thpracJson, thpracJsonLen, "graze");
            thpracPoint = ThpracJsonInt(thpracJson, thpracJsonLen, "point");
            thpracRank = ThpracJsonInt(thpracJson, thpracJsonLen, "rank");
            thpracRankLock = ThpracJsonBool(thpracJson, thpracJsonLen, "rankLock");
            if (thpracSection > 0)
            {
                utils::LogDebug("thprac replay: section=%d dlg=%d", thpracSection, thpracDlg);
                g_ThpracSection = thpracSection;
                g_ThpracDlg = (thpracDlg == 1) ? 1 : 0;
                g_ThpracSectionReplay = 1;
            }
            else
            {
                g_ThpracSectionReplay = 0;
            }
        }
        else
        {
            thpracSection = -1;
            thpracLife = -1;
            thpracDlg = -1;
            thpracBomb = -1;
            thpracPower = -1;
            thpracScore = -1;
            thpracGraze = -1;
            thpracPoint = -1;
            thpracRank = -1;
            thpracRankLock = -1;

            g_ThpracSection = 0;
            g_ThpracDlg = 0;
            g_ThpracSectionReplay = 0;
        }
        if (ValidateReplayData(mgr->replayData, g_LastFileSize) != ZUN_SUCCESS)
        {
            return ZUN_ERROR;
        }
        for (idx = 0; idx < ARRAY_SIZE_SIGNED(mgr->replayData->stageReplayData); idx += 1)
        {
            mgr->stageReplayDataP2[idx] = NULL;
            mgr->replayInputStageBookmarksP2[idx] = NULL;
            if (mgr->replayData->stageReplayData[idx] != NULL)
            {
                mgr->replayData->stageReplayData[idx] =
                    (StageReplayData *)((i32)mgr->replayData->stageReplayData[idx] + (i32)mgr->replayData);
            }
        }
        onlineReplayData = FindOnlineReplayData(mgr->replayData, g_LastFileSize);
        mgr->isOnlineReplay = onlineReplayData != NULL;
        g_IsOnline = mgr->isOnlineReplay;
        if (onlineReplayData != NULL)
        {
            for (idx = 0; idx < ARRAY_SIZE_SIGNED(onlineReplayData->stageReplayDataOffsets); idx += 1)
            {
                if (onlineReplayData->stageReplayDataOffsets[idx] != 0)
                {
                    mgr->stageReplayDataP2[idx] =
                        (StageReplayData *)((i32)mgr->replayData + onlineReplayData->stageReplayDataOffsets[idx]);
                }
            }
            g_ReplayP1ShottypeChara = onlineReplayData->reserved & 0xFF;
            g_ReplayP2ShottypeChara = (onlineReplayData->reserved >> 8) & 0xFF;
            if (g_ReplayP1ShottypeChara == 0 && g_ReplayP2ShottypeChara == 0)
            {
                g_ReplayP1ShottypeChara = mgr->replayData->shottypeChara;
                g_ReplayP2ShottypeChara = mgr->replayData->shottypeChara;
            }
        }
    }
    else
    {
        g_IsOnline = mgr->isOnlineReplay;
    }
    if (mgr->replayData->stageReplayData[g_GameManager.currentStage - 1] == NULL)
    {
        return ZUN_ERROR;
    }
    replayData = mgr->replayData->stageReplayData[g_GameManager.currentStage - 1];
    g_GameManager.character = mgr->replayData->shottypeChara / 2;
    g_GameManager.shotType = mgr->replayData->shottypeChara % 2;
    g_GameManager.difficulty = (Difficulty)mgr->replayData->difficulty;
    g_GameManager.pointItemsCollected = replayData->pointItemsCollected;
    g_Rng.Initialize(replayData->randomSeed);
    g_GameManager.rank = replayData->rank;
    g_GameManager.livesRemaining = replayData->livesRemaining;
    g_GameManager.bombsRemaining = replayData->bombsRemaining;
    g_GameManager.currentPower = replayData->power;
    mgr->replayInputs = replayData->replayInputs;
    mgr->replayInputsP2 = (mgr->isOnlineReplay && mgr->stageReplayDataP2[g_GameManager.currentStage - 1] != NULL)
                              ? mgr->stageReplayDataP2[g_GameManager.currentStage - 1]->replayInputs
                              : NULL;
    g_GameManager.powerItemCountForScore = replayData->powerItemCountForScore;
    g_IsSpellPracticeGame = (i32)replayData->spellSection;
    if (g_GameManager.currentStage >= 2 && mgr->replayData->stageReplayData[g_GameManager.currentStage - 2] != NULL)
    {
        g_GameManager.guiScore = mgr->replayData->stageReplayData[g_GameManager.currentStage - 2]->score;
        g_GameManager.score = g_GameManager.guiScore;
    }
    if (thpracSection > 0)
    {
        if (thpracLife >= 0)
            g_GameManager.livesRemaining = (i8)thpracLife;
        if (thpracLife >= 0)
            g_GameManager.bombsRemaining = (i8)thpracLife;
        if (thpracPower >= 0)
            g_GameManager.currentPower = (i16)thpracPower;
        if (thpracScore >= 0)
        {
            g_GameManager.score = thpracScore;
            g_GameManager.guiScore = thpracScore;
        }
        if (thpracGraze >= 0)
        {
            g_GameManager.grazeInStage = thpracGraze;
            g_GameManager.grazeInTotal = thpracGraze;
        }
        if (thpracPoint >= 0)
        {
            g_GameManager.pointItemsCollectedInStage = (u16)thpracPoint;
            g_GameManager.pointItemsCollected = (i16)thpracPoint;
        }
        if (g_GameManager.difficulty != 4 && thpracScore >= 0)
        {
            if (thpracScore >= 60000000)
                g_GameManager.extraLives = 4;
            else if (thpracScore >= 40000000)
                g_GameManager.extraLives = 3;
            else if (thpracScore >= 20000000)
                g_GameManager.extraLives = 2;
            else if (thpracScore >= 10000000)
                g_GameManager.extraLives = 1;
        }
        if (thpracRank >= 0)
            g_GameManager.rank = thpracRank;
        if (thpracRankLock == 1 && thpracRank >= 0)
        {
            g_GameManager.maxRank = thpracRank;
            g_GameManager.minRank = thpracRank;
        }
    }
    utils::LogDebug(
        "AddedCallbackDemo done: lives=%d bombs=%d power=%d rank=%d minRank=%d maxRank=%d seed=%d section=%d",
        (i32)g_GameManager.livesRemaining, (i32)g_GameManager.bombsRemaining, (i32)g_GameManager.currentPower,
        g_GameManager.rank, g_GameManager.minRank, g_GameManager.maxRank, (i32)g_Rng.seed, thpracSection);
    if (thpracSection > 0 && replayData != NULL)
    {
        ReplayDataInput *inp = replayData->replayInputs;
        i32 i;
        for (i = 0; i < 8; i++)
            utils::LogDebug("  input[%d]: frame=%d key=0x%x", i, inp[i].frameNum, (u32)inp[i].inputKey);
    }
    return ZUN_SUCCESS;
}

ZunResult ReplayManager::DeletedCallback(ReplayManager *mgr)
{
    g_Chain.Cut(mgr->drawChain);
    mgr->drawChain = NULL;
    if (mgr->calcChainDemoHighPrio != NULL)
    {
        g_Chain.Cut(mgr->calcChainDemoHighPrio);
        mgr->calcChainDemoHighPrio = NULL;
    }
    ReleaseReplayData(g_ReplayManager->replayData);
    delete g_ReplayManager;
    g_ReplayManager = NULL;

    g_ReplaySpeed = 1;
    g_SavedReplaySpeed = 1;
    g_ReplayFastForwardActive = 0;
    g_ReplayTotalFrames = 0;
    g_IsOnline = s_SavedIsOnline;
    return ZUN_SUCCESS;
}

void ReplayManager::StopRecording()
{
    ReplayManager *mgr = g_ReplayManager;
    if (mgr != NULL)
    {
        mgr->replayInputs += 1;
        mgr->replayInputs->frameNum = mgr->frameId;
        mgr->replayInputs->inputKey = 0;
        mgr->replayInputs += 1;
        mgr->replayInputs->frameNum = 9999999;
        mgr->replayInputs->inputKey = 0;
        mgr->replayInputs->padding = 0;
        mgr->replayInputStageBookmarks[g_GameManager.currentStage - 1] = mgr->replayInputs + 1;
        if (mgr->replayInputsP2 != NULL)
        {
            mgr->replayInputsP2 += 1;
            mgr->replayInputsP2->frameNum = mgr->frameId;
            mgr->replayInputsP2->inputKey = 0;
            mgr->replayInputsP2->padding = 0;
            mgr->replayInputsP2 += 1;
            mgr->replayInputsP2->frameNum = 9999999;
            mgr->replayInputsP2->inputKey = 0;
            mgr->replayInputsP2->padding = 0;
            mgr->replayInputStageBookmarksP2[g_GameManager.currentStage - 1] = mgr->replayInputsP2 + 1;
        }
    }
}

void ReplayManager::SaveReplay(char *replayPath, char *replayName)
{
    ReplayManager *mgr;
    FILE *file;
    u8 *checksumCursor;
    ReplayData replayCopy;
    u8 *obfuscateCursor;
    i32 obfStagePos;
    u8 obfOffset;
    u32 checksum;
    i32 csumStagePos;
    size_t stageReplayPos;
    size_t onlineReplayDataSize;
    size_t onlineReplayDataPos;
    u8 *onlineReplayData;
    OnlineReplayDataHeader *onlineReplayDataHeader;
    f32 slowDown;
    i32 stageIdx;

    if (g_ReplayManager != NULL)
    {
        mgr = g_ReplayManager;
        if (!mgr->IsDemo())
        {
            if (replayPath != NULL)
            {
                ReplayManager::StopRecording();
                if (g_ShowCollision != 1 && g_ShowEnemyMarker != 1 && g_HitDisp != 1)
                {
                    replayCopy = *mgr->replayData;
                    stageReplayPos = sizeof(ReplayData);
                    onlineReplayData = NULL;
                    onlineReplayDataSize = 0;
                    for (stageIdx = 0; stageIdx < ARRAY_SIZE_SIGNED(g_ReplayManager->replayData->stageReplayData);
                         stageIdx += 1)
                    {
                        if (mgr->replayData->stageReplayData[stageIdx] != NULL)
                        {
                            replayCopy.stageReplayData[stageIdx] = (StageReplayData *)stageReplayPos;
                            stageReplayPos += (size_t)mgr->replayInputStageBookmarks[stageIdx] -
                                              (size_t)mgr->replayData->stageReplayData[stageIdx];
                        }
                    }
                    if (g_IsOnline)
                    {
                        onlineReplayDataSize = sizeof(OnlineReplayDataHeader);
                        for (stageIdx = 0; stageIdx < ARRAY_SIZE_SIGNED(mgr->stageReplayDataP2); stageIdx += 1)
                        {
                            if (mgr->stageReplayDataP2[stageIdx] != NULL &&
                                mgr->replayInputStageBookmarksP2[stageIdx] != NULL)
                            {
                                onlineReplayDataSize += (size_t)mgr->replayInputStageBookmarksP2[stageIdx] -
                                                        (size_t)mgr->stageReplayDataP2[stageIdx];
                            }
                        }
                        onlineReplayData = (u8 *)malloc(onlineReplayDataSize);
                        memset(onlineReplayData, 0, onlineReplayDataSize);
                        onlineReplayDataHeader = (OnlineReplayDataHeader *)onlineReplayData;
                        memcpy(onlineReplayDataHeader->magic, ONLINE_REPLAY_MAGIC, 4);
                        onlineReplayDataHeader->version = ONLINE_REPLAY_VERSION;
                        u8 p1_chara =
                            g_NetworkManager.IsHost()
                                ? (g_GameManager.character * 2 + g_GameManager.shotType)
                                : (g_NetworkManager.GetRemoteCharacter() * 2 + g_NetworkManager.GetRemoteShotType());
                        u8 p2_chara =
                            g_NetworkManager.IsHost()
                                ? (g_NetworkManager.GetRemoteCharacter() * 2 + g_NetworkManager.GetRemoteShotType())
                                : (g_GameManager.character * 2 + g_GameManager.shotType);
                        onlineReplayDataHeader->reserved = p1_chara | (p2_chara << 8);
                        onlineReplayDataPos = sizeof(OnlineReplayDataHeader);
                        for (stageIdx = 0; stageIdx < ARRAY_SIZE_SIGNED(mgr->stageReplayDataP2); stageIdx += 1)
                        {
                            if (mgr->stageReplayDataP2[stageIdx] != NULL &&
                                mgr->replayInputStageBookmarksP2[stageIdx] != NULL)
                            {
                                i32 stageSize = (i32)mgr->replayInputStageBookmarksP2[stageIdx] -
                                                (i32)mgr->stageReplayDataP2[stageIdx];
                                onlineReplayDataHeader->stageReplayDataOffsets[stageIdx] =
                                    (u32)(stageReplayPos + onlineReplayDataPos);
                                onlineReplayDataHeader->stageReplayDataSizes[stageIdx] = stageSize;
                                memcpy(onlineReplayData + onlineReplayDataPos, mgr->stageReplayDataP2[stageIdx],
                                       stageSize);
                                onlineReplayDataPos += stageSize;
                            }
                        }
                    }
                    utils::DebugPrint2("%s write ...\n", replayPath);
                    replayCopy.score = g_GameManager.guiScore;
                    slowDown = (g_Supervisor.unk1b4 / g_Supervisor.unk1b8 - 0.5f) * 2.0f;
                    if (slowDown < 0.0f)
                    {
                        slowDown = 0.0f;
                    }
                    else if (slowDown >= 1.0f)
                    {
                        slowDown = 1.0f;
                    }
                    replayCopy.slowdownRate = (1.0f - slowDown) * 100.0f;
                    replayCopy.slowdownRate2 = replayCopy.slowdownRate + 1.12f;
                    replayCopy.slowdownRate3 = replayCopy.slowdownRate + 2.34f;
                    mgr->replayData->stageReplayData[g_GameManager.currentStage - 1]->score = g_GameManager.score;
                    strcpy(replayCopy.name, replayName);
                    _strdate(replayCopy.date);
                    replayCopy.key = g_Rng.GetRandomU16InRange(128) + 64;
                    replayCopy.rngValue3 = g_Rng.GetRandomU16InRange(256);
                    replayCopy.rngValue1 = g_Rng.GetRandomU16InRange(256);
                    replayCopy.rngValue2 = g_Rng.GetRandomU16InRange(256);

                    checksumCursor = (u8 *)&replayCopy.key;
                    checksum = 0x3f000318;
                    for (stageIdx = 0; stageIdx < sizeof(ReplayData) - offsetof(ReplayData, key);
                         stageIdx += 1, checksumCursor += 1)
                    {
                        checksum += *checksumCursor;
                    }
                    for (stageIdx = 0; stageIdx < ARRAY_SIZE_SIGNED(mgr->replayData->stageReplayData); stageIdx += 1)
                    {
                        if (mgr->replayData->stageReplayData[stageIdx] != NULL)
                        {
                            checksumCursor = (u8 *)mgr->replayData->stageReplayData[stageIdx];
                            for (csumStagePos = 0; csumStagePos < (i32)mgr->replayInputStageBookmarks[stageIdx] -
                                                                      (i32)mgr->replayData->stageReplayData[stageIdx];
                                 csumStagePos += 1, checksumCursor += 1)
                            {
                                checksum += *checksumCursor;
                            }
                        }
                    }
                    if (onlineReplayData != NULL)
                    {
                        checksumCursor = onlineReplayData;
                        for (csumStagePos = 0; csumStagePos < (i32)onlineReplayDataSize;
                             csumStagePos += 1, checksumCursor += 1)
                        {
                            checksum += *checksumCursor;
                        }
                    }
                    replayCopy.checksum = checksum;

                    obfuscateCursor = (u8 *)&replayCopy.rngValue3;
                    obfOffset = replayCopy.key;
                    for (stageIdx = 0; stageIdx < sizeof(ReplayData) - offsetof(ReplayData, rngValue3);
                         stageIdx += 1, obfuscateCursor += 1)
                    {
                        *obfuscateCursor += obfOffset;
                        obfOffset += 7;
                    }
                    for (stageIdx = 0; stageIdx < ARRAY_SIZE_SIGNED(mgr->replayData->stageReplayData); stageIdx += 1)
                    {
                        if (mgr->replayData->stageReplayData[stageIdx] != NULL)
                        {
                            obfuscateCursor = (u8 *)mgr->replayData->stageReplayData[stageIdx];
                            for (obfStagePos = 0; obfStagePos < (i32)mgr->replayInputStageBookmarks[stageIdx] -
                                                                    (i32)mgr->replayData->stageReplayData[stageIdx];
                                 obfStagePos += 1, obfuscateCursor += 1)
                            {
                                *obfuscateCursor += obfOffset;
                                obfOffset += 7;
                            }
                        }
                    }
                    if (onlineReplayData != NULL)
                    {
                        obfuscateCursor = onlineReplayData;
                        for (obfStagePos = 0; obfStagePos < (i32)onlineReplayDataSize;
                             obfStagePos += 1, obfuscateCursor += 1)
                        {
                            *obfuscateCursor += obfOffset;
                            obfOffset += 7;
                        }
                    }

                    CreateGameBackup(true);
                    file = fopen(replayPath, "wb");
                    fwrite(&replayCopy, sizeof(ReplayData), 1, file);
                    for (stageIdx = 0; stageIdx < ARRAY_SIZE_SIGNED(mgr->replayData->stageReplayData); stageIdx += 1)
                    {
                        if (mgr->replayData->stageReplayData[stageIdx] != NULL)
                        {
                            fwrite(mgr->replayData->stageReplayData[stageIdx], 1,
                                   (i32)mgr->replayInputStageBookmarks[stageIdx] -
                                       (i32)mgr->replayData->stageReplayData[stageIdx],
                                   file);
                        }
                    }
                    if (onlineReplayData != NULL)
                    {
                        fwrite(onlineReplayData, 1, onlineReplayDataSize, file);
                        free(onlineReplayData);
                    }
                    fclose(file);
                }
            }
            for (stageIdx = 0; stageIdx < ARRAY_SIZE_SIGNED(mgr->replayData->stageReplayData); stageIdx += 1)
            {
                if (g_ReplayManager->replayData->stageReplayData[stageIdx] != NULL)
                {
                    utils::DebugPrint2("Replay Size %d\n", (i32)mgr->replayInputStageBookmarks[stageIdx] -
                                                               (i32)mgr->replayData->stageReplayData[stageIdx]);
                    ReleaseStageReplayData(g_ReplayManager->replayData->stageReplayData[stageIdx]);
                }
                if (mgr->stageReplayDataP2[stageIdx] != NULL)
                {
                    ReleaseStageReplayData(mgr->stageReplayDataP2[stageIdx]);
                    mgr->stageReplayDataP2[stageIdx] = NULL;
                    mgr->replayInputStageBookmarksP2[stageIdx] = NULL;
                }
            }
        }
        g_Chain.Cut(g_ReplayManager->calcChain);
    }
    return;
}
}; // namespace th06
