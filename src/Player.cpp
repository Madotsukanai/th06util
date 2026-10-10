#include "Player.hpp"
#include "ResultScreen.hpp"

#include "AnmManager.hpp"
#include "AnmVm.hpp"
#include "AsciiManager.hpp"
#include "BombData.hpp"
#include "BulletData.hpp"
#include "BulletManager.hpp"
#include "ChainPriorities.hpp"
#include "EclManager.hpp"
#include "EffectManager.hpp"
#include "EnemyManager.hpp"
#include "GameManager.hpp"
#include "Gui.hpp"
#include "ItemManager.hpp"
#include "Network.hpp"
#include "Rng.hpp"
#include "ScreenEffect.hpp"
#include "SoundPlayer.hpp"
#include "Supervisor.hpp"
#include "ZunBool.hpp"
#include "i18n.hpp"
#include "utils.hpp"

namespace th06
{
Player g_Player;
Player g_Player2;

extern u8 g_ReplayP1ShottypeChara;
extern u8 g_ReplayP2ShottypeChara;

CharacterData g_CharData[4] = {
    /* */ {4.0, 2.0, 4.0, 2.0, Player::FireBulletReimuA, Player::FireBulletReimuA},
    /* */ {4.0, 2.0, 4.0, 2.0, Player::FireBulletReimuB, Player::FireBulletReimuB},
    /* */ {5.0, 2.5, 5.0, 2.5, Player::FireBulletMarisaA, Player::FireBulletMarisaA},
    /* */ {5.0, 2.5, 5.0, 2.5, Player::FireBulletMarisaB, Player::FireBulletMarisaB},
};

struct HitboxMarkerVertex
{
    f32 x;
    f32 y;
    f32 z;
    f32 rhw;
    DWORD color;
};

enum
{
    HITBOX_MARKER_ANIM_FRAMES = 3,
};

static i32 s_hitboxAnimFrame[2] = {0, 0};

static i32 HitboxMarkerSlot(const Player *p)
{
    return p == &g_Player2 ? 1 : 0;
}

static void LogReplayPlayerStateChange(Player *p)
{
    static i32 s_lastState[2] = {-1, -1};
    const i32 slot = HitboxMarkerSlot(p);

    if (!g_GameManager.isInReplay)
    {
        s_lastState[slot] = -1;
        return;
    }
    if (s_lastState[slot] != (i32)p->playerState)
    {
        utils::LogDebug("Player state change: p=%d gameFrame=%u state %d -> %d invulTimer=%d bulletGrace=%d", slot,
                        (u32)g_GameManager.gameFrames, s_lastState[slot], (i32)p->playerState,
                        (i32)p->invulnerabilityTimer.AsFrames(), (i32)p->bulletGracePeriod);
        s_lastState[slot] = (i32)p->playerState;
    }
}

static void DrawHitboxFilledCircle(f32 x, f32 y, f32 radius, DWORD color)
{
    const i32 segments = 48;
    HitboxMarkerVertex vertices[segments + 2];

    vertices[0].x = x;
    vertices[0].y = y;
    vertices[0].z = 0.0f;
    vertices[0].rhw = 1.0f;
    vertices[0].color = color;

    for (i32 i = 0; i <= segments; i++)
    {
        f32 angle = (f32)i * 2.0f * 3.1415927f / segments;
        vertices[i + 1].x = x + radius * cosf(angle);
        vertices[i + 1].y = y + radius * sinf(angle);
        vertices[i + 1].z = 0.0f;
        vertices[i + 1].rhw = 1.0f;
        vertices[i + 1].color = color;
    }

    g_Supervisor.d3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, segments, vertices, sizeof(HitboxMarkerVertex));
}

static void DrawHitboxRing(f32 x, f32 y, f32 innerRadius, f32 outerRadius, DWORD color)
{
    const i32 segments = 48;
    HitboxMarkerVertex vertices[(segments + 1) * 2];

    for (i32 i = 0; i <= segments; i++)
    {
        f32 angle = (f32)i * 2.0f * 3.1415927f / segments;
        f32 angleCos = cosf(angle);
        f32 angleSin = sinf(angle);
        i32 vertexIdx = i * 2;

        vertices[vertexIdx].x = x + outerRadius * angleCos;
        vertices[vertexIdx].y = y + outerRadius * angleSin;
        vertices[vertexIdx].z = 0.0f;
        vertices[vertexIdx].rhw = 1.0f;
        vertices[vertexIdx].color = color;
        vertices[vertexIdx + 1].x = x + innerRadius * angleCos;
        vertices[vertexIdx + 1].y = y + innerRadius * angleSin;
        vertices[vertexIdx + 1].z = 0.0f;
        vertices[vertexIdx + 1].rhw = 1.0f;
        vertices[vertexIdx + 1].color = color;
    }

    g_Supervisor.d3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, segments * 2, vertices, sizeof(HitboxMarkerVertex));
}

void Player::UpdateHitboxMarker(Player *p)
{
    i32 &frame = s_hitboxAnimFrame[HitboxMarkerSlot(p)];

    if (g_ShowCollision != 1 || p->playerState == PLAYER_STATE_DEAD || p->playerState == PLAYER_STATE_SPAWNING)
    {

        frame = 0;
    }
    else if (p->isFocus)
    {
        if (frame < HITBOX_MARKER_ANIM_FRAMES)
        {
            frame++;
        }
    }
    else if (frame > 0)
    {
        frame--;
    }
}

void Player::DrawHitboxMarker(Player *p)
{
    const i32 frame = s_hitboxAnimFrame[HitboxMarkerSlot(p)];

    if (frame <= 0 || g_ShowCollision != 1 || g_GameManager.isInRetryMenu)
    {
        return;
    }

    const f32 scale = (f32)frame / (f32)HITBOX_MARKER_ANIM_FRAMES;
    const f32 x = g_GameManager.arcadeRegionTopLeftPos.x + p->positionCenter.x;
    const f32 y = g_GameManager.arcadeRegionTopLeftPos.y + p->positionCenter.y;

    const f32 core = 1.41421356f * p->hitboxSize.x;

    DWORD oldLighting;
    DWORD oldAlpha;
    DWORD oldSrc;
    DWORD oldDest;
    DWORD oldZ;
    DWORD oldColorOp;
    DWORD oldAlphaOp;
    DWORD oldColorArg1;
    DWORD oldAlphaArg1;
    DWORD oldVertexShader;
    IDirect3DBaseTexture8 *oldTexture = NULL;

    g_Supervisor.d3dDevice->GetTexture(0, &oldTexture);
    g_Supervisor.d3dDevice->GetVertexShader(&oldVertexShader);
    g_Supervisor.d3dDevice->GetRenderState(D3DRS_LIGHTING, &oldLighting);
    g_Supervisor.d3dDevice->GetRenderState(D3DRS_ALPHABLENDENABLE, &oldAlpha);
    g_Supervisor.d3dDevice->GetRenderState(D3DRS_SRCBLEND, &oldSrc);
    g_Supervisor.d3dDevice->GetRenderState(D3DRS_DESTBLEND, &oldDest);
    g_Supervisor.d3dDevice->GetRenderState(D3DRS_ZENABLE, &oldZ);
    g_Supervisor.d3dDevice->GetTextureStageState(0, D3DTSS_COLOROP, &oldColorOp);
    g_Supervisor.d3dDevice->GetTextureStageState(0, D3DTSS_ALPHAOP, &oldAlphaOp);
    g_Supervisor.d3dDevice->GetTextureStageState(0, D3DTSS_COLORARG1, &oldColorArg1);
    g_Supervisor.d3dDevice->GetTextureStageState(0, D3DTSS_ALPHAARG1, &oldAlphaArg1);

    g_Supervisor.d3dDevice->SetTexture(0, NULL);
    g_Supervisor.d3dDevice->SetVertexShader(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
    g_Supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    g_Supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    g_Supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
    g_Supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
    g_Supervisor.d3dDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
    g_Supervisor.d3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    g_Supervisor.d3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);

    g_Supervisor.d3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    g_Supervisor.d3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    DrawHitboxFilledCircle(x, y, 8.7f * scale, 0x18000000);
    DrawHitboxRing(x, y, 6.0f * scale, 8.2f * scale, 0x48b8b8b8);
    DrawHitboxRing(x, y, 4.9f * scale, 6.6f * scale, 0x38ffffff);
    DrawHitboxRing(x, y, core * scale, 5.0f * scale, 0xf0b00000);
    DrawHitboxFilledCircle(x, y, core * scale, 0xffffffff);
    DrawHitboxRing(x, y, core * scale, 2.3f * scale, 0xc0ffffff);

    g_Supervisor.d3dDevice->SetTexture(0, oldTexture);
    if (oldTexture != NULL)
    {
        oldTexture->Release();
    }
    g_Supervisor.d3dDevice->SetVertexShader(oldVertexShader);
    g_Supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_COLOROP, oldColorOp);
    g_Supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, oldAlphaOp);
    g_Supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_COLORARG1, oldColorArg1);
    g_Supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, oldAlphaArg1);
    g_Supervisor.d3dDevice->SetRenderState(D3DRS_LIGHTING, oldLighting);
    g_Supervisor.d3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, oldAlpha);
    g_Supervisor.d3dDevice->SetRenderState(D3DRS_SRCBLEND, oldSrc);
    g_Supervisor.d3dDevice->SetRenderState(D3DRS_DESTBLEND, oldDest);
    g_Supervisor.d3dDevice->SetRenderState(D3DRS_ZENABLE, oldZ);
}

void Player::ResetHitboxMarker(Player *p)
{
    s_hitboxAnimFrame[HitboxMarkerSlot(p)] = 0;
}

Player::Player()
{
}

ZunResult Player::RegisterChain(u8 unk)
{
    Player *p = &g_Player;
    memset(p, 0, sizeof(Player));

    p->invulnerabilityTimer.InitializeForPopup();
    p->unk_9e1 = unk;
    p->chainCalc = g_Chain.CreateElem((ChainCallback)Player::OnUpdate);
    p->chainDraw1 = g_Chain.CreateElem((ChainCallback)Player::OnDrawHighPrio);
    p->chainDraw2 = g_Chain.CreateElem((ChainCallback)Player::OnDrawLowPrio);
    p->chainCalc->arg = p;
    p->chainDraw1->arg = p;
    p->chainDraw2->arg = p;
    p->chainCalc->addedCallback = (ChainAddedCallback)Player::AddedCallback;
    p->chainCalc->deletedCallback = (ChainDeletedCallback)Player::DeletedCallback;
    if (g_Chain.AddToCalcChain(p->chainCalc, TH_CHAIN_PRIO_CALC_PLAYER))
    {
        return ZUN_ERROR;
    }
    g_Chain.AddToDrawChain(p->chainDraw1, TH_CHAIN_PRIO_DRAW_LOW_PRIO_PLAYER);
    g_Chain.AddToDrawChain(p->chainDraw2, TH_CHAIN_PRIO_DRAW_HIGH_PRIO_PLAYER);

    if (g_IsOnline)
    {
        p = &g_Player2;
        memset(p, 0, sizeof(Player));
        p->invulnerabilityTimer.InitializeForPopup();
        p->unk_9e1 = unk;
        p->chainCalc = g_Chain.CreateElem((ChainCallback)Player::OnUpdate);
        p->chainDraw1 = g_Chain.CreateElem((ChainCallback)Player::OnDrawHighPrio);
        p->chainDraw2 = g_Chain.CreateElem((ChainCallback)Player::OnDrawLowPrio);
        p->chainCalc->arg = p;
        p->chainDraw1->arg = p;
        p->chainDraw2->arg = p;
        p->chainCalc->addedCallback = (ChainAddedCallback)Player::AddedCallback;
        p->chainCalc->deletedCallback = (ChainDeletedCallback)Player::DeletedCallback;
        g_Chain.AddToCalcChain(p->chainCalc, TH_CHAIN_PRIO_CALC_PLAYER);
        g_Chain.AddToDrawChain(p->chainDraw1, TH_CHAIN_PRIO_DRAW_LOW_PRIO_PLAYER);
        g_Chain.AddToDrawChain(p->chainDraw2, TH_CHAIN_PRIO_DRAW_HIGH_PRIO_PLAYER);
    }

    return ZUN_SUCCESS;
}

void Player::CutChain()
{
    g_Chain.Cut(g_Player.chainCalc);
    g_Player.chainCalc = NULL;
    g_Chain.Cut(g_Player.chainDraw1);
    g_Player.chainDraw1 = NULL;
    g_Chain.Cut(g_Player.chainDraw2);
    g_Player.chainDraw2 = NULL;

    if (g_IsOnline)
    {
        g_Chain.Cut(g_Player2.chainCalc);
        g_Player2.chainCalc = NULL;
        g_Chain.Cut(g_Player2.chainDraw1);
        g_Player2.chainDraw1 = NULL;
        g_Chain.Cut(g_Player2.chainDraw2);
        g_Player2.chainDraw2 = NULL;
    }
    return;
}

ZunResult Player::AddedCallback(Player *p)
{
    PlayerBullet *curBullet;
    i32 idx;

    i32 charType = g_GameManager.character;
    i32 shotType = g_GameManager.shotType;

    if (g_IsOnline)
    {
        if (g_GameManager.isInReplay)
        {
            i32 shottypeChara = (p == &g_Player) ? g_ReplayP1ShottypeChara : g_ReplayP2ShottypeChara;
            charType = shottypeChara / 2;
            shotType = shottypeChara % 2;
        }
        else
        {
            if (p == &g_Player)
            {
                if (g_NetworkManager.IsHost())
                {
                    charType = g_GameManager.character;
                    shotType = g_GameManager.shotType;
                }
                else
                {
                    charType = g_NetworkManager.GetRemoteCharacter();
                    shotType = g_NetworkManager.GetRemoteShotType();
                }
            }
            else
            {
                if (g_NetworkManager.IsHost())
                {
                    charType = g_NetworkManager.GetRemoteCharacter();
                    shotType = g_NetworkManager.GetRemoteShotType();
                }
                else
                {
                    charType = g_GameManager.character;
                    shotType = g_GameManager.shotType;
                }
            }
        }
    }

    i32 anmSlot = (p == &g_Player) ? ANM_FILE_PLAYER : ANM_FILE_PLAYER2;
    i32 anmOffset = (p == &g_Player) ? ANM_OFFSET_PLAYER : ANM_OFFSET_PLAYER2;

    switch (charType)
    {
    case CHARA_REIMU:

        if (g_Supervisor.curState != SUPERVISOR_STATE_GAMEMANAGER_REINIT &&
            g_AnmManager->LoadAnm(anmSlot, "data/player00.anm", anmOffset) != ZUN_SUCCESS)
        {
            return ZUN_ERROR;
        }
        g_AnmManager->SetAndExecuteScriptIdx(&p->playerSprite, ANM_SCRIPT_PLAYER_IDLE - ANM_OFFSET_PLAYER + anmOffset);
        break;
    case CHARA_MARISA:
        if (g_Supervisor.curState != SUPERVISOR_STATE_GAMEMANAGER_REINIT &&
            g_AnmManager->LoadAnm(anmSlot, "data/player01.anm", anmOffset) != ZUN_SUCCESS)
        {
            return ZUN_ERROR;
        }
        g_AnmManager->SetAndExecuteScriptIdx(&p->playerSprite, ANM_SCRIPT_PLAYER_IDLE - ANM_OFFSET_PLAYER + anmOffset);
        break;
    }

    if (g_IsOnline)
    {
        p->positionCenter.x = g_GameManager.arcadeRegionSize.x / 2.0f + (p == &g_Player ? -16.0f : 16.0f);
    }
    else
    {
        p->positionCenter.x = g_GameManager.arcadeRegionSize.x / 2.0f;
    }

    p->positionCenter.y = g_GameManager.arcadeRegionSize.y - 64.0f;
    p->positionCenter.z = 0.49;
    p->orbsPosition[0].z = 0.49;
    p->orbsPosition[1].z = 0.49;
    for (idx = 0; idx < ARRAY_SIZE_SIGNED(p->bombRegionSizes); idx++)
    {
        p->bombRegionSizes[idx].x = 0.0;
    }
    p->hitboxSize.x = 1.25;
    p->hitboxSize.y = 1.25;
    p->hitboxSize.z = 5.0;
    p->grabItemSize.x = 12.0;
    p->grabItemSize.y = 12.0;
    p->grabItemSize.z = 5.0;
    p->playerDirection = MOVEMENT_NONE;
    memcpy(&p->characterData, &g_CharData[shotType + charType * 2], sizeof(CharacterData));
    p->characterData.diagonalMovementSpeed = p->characterData.orthogonalMovementSpeed / sqrtf(2.0);
    p->characterData.diagonalMovementSpeedFocus = p->characterData.orthogonalMovementSpeedFocus / sqrtf(2.0);
    p->fireBulletCallback = p->characterData.fireBulletCallback;
    p->fireBulletFocusCallback = p->characterData.fireBulletFocusCallback;
    if (g_ThpracSectionReplay == 1 && g_GameManager.isInReplay)
    {

        p->playerState = PLAYER_STATE_ALIVE;
        p->invulnerabilityTimer.SetCurrent(0);
    }
    else if (g_IsSpellPracticeGame > 0)
    {

        p->playerState = PLAYER_STATE_INVULNERABLE;
        p->invulnerabilityTimer.SetCurrent(0);
    }
    else
    {
        p->playerState = PLAYER_STATE_SPAWNING;
        p->invulnerabilityTimer.SetCurrent(120);
    }
    utils::LogDebug(
        "Player::AddedCallback: p=%d thpracSectionReplay=%d isInReplay=%d spellPractice=%d -> playerState=%d "
        "invulTimer=%d",
        (i32)(p == &g_Player2), (i32)g_ThpracSectionReplay, (i32)g_GameManager.isInReplay, (i32)g_IsSpellPracticeGame,
        (i32)p->playerState, (i32)p->invulnerabilityTimer.AsFrames());
    p->orbState = ORB_HIDDEN;
    g_AnmManager->SetAndExecuteScriptIdx(&p->orbsSprite[0], ANM_SCRIPT_PLAYER_ORB_LEFT - ANM_OFFSET_PLAYER + anmOffset);
    g_AnmManager->SetAndExecuteScriptIdx(&p->orbsSprite[1],
                                         ANM_SCRIPT_PLAYER_ORB_RIGHT - ANM_OFFSET_PLAYER + anmOffset);
    for (curBullet = &p->bullets[0], idx = 0; idx < ARRAY_SIZE_SIGNED(p->bullets); idx++, curBullet++)
    {
        curBullet->bulletState = 0;
    }
    p->fireBulletTimer.SetCurrent(-1);
    p->bombInfo.calc = g_BombData[shotType + charType * 2].calc;
    p->bombInfo.draw = g_BombData[shotType + charType * 2].draw;
    p->bombInfo.isInUse = 0;
    for (idx = 0; idx < ARRAY_SIZE_SIGNED(p->laserTimer); idx++)
    {
        p->laserTimer[idx].InitializeForPopup();
    }
    p->verticalMovementSpeedMultiplierDuringBomb = 1.0;
    p->horizontalMovementSpeedMultiplierDuringBomb = 1.0;
    p->respawnTimer = 8;
    Player::ResetHitboxMarker(p);
    return ZUN_SUCCESS;
}

ZunResult Player::DeletedCallback(Player *p)
{
    if (g_Supervisor.curState != SUPERVISOR_STATE_GAMEMANAGER_REINIT)
    {
        i32 anmSlot = (p == &g_Player) ? ANM_FILE_PLAYER : ANM_FILE_PLAYER2;
        g_AnmManager->ReleaseAnm(anmSlot);
    }
    return ZUN_SUCCESS;
}

ChainCallbackResult Player::OnUpdate(Player *p)
{
    f32 scaleFactor1, scaleFactor2;
    i32 idx;
    D3DXVECTOR3 lastEnemyHit;

    if (g_GameManager.isTimeStopped)
    {
        return CHAIN_CALLBACK_RESULT_CONTINUE;
    }
    for (idx = 0; idx < ARRAY_SIZE_SIGNED(p->bombRegionSizes); idx++)
    {
        p->bombRegionSizes[idx].x = 0.0;
    }
    for (idx = 0; idx < ARRAY_SIZE_SIGNED(p->bombProjectiles); idx++)
    {
        p->bombProjectiles[idx].sizeX = 0.0;
    }
    if (p->bombInfo.isInUse)
    {
        p->bombInfo.calc(p);
    }
    else if (!g_Gui.HasCurrentMsgIdx() && p->respawnTimer != 0 && g_GameManager.bombsRemaining > 0 &&
             ((p->input & (TH_BUTTON_BOMB)) && !(p->previousFrameInput & (TH_BUTTON_BOMB))) && p->bombInfo.calc != NULL)
    {
        g_GameManager.bombsUsed++;
        g_GameManager.bombsRemaining--;
        g_Gui.flags.flag1 = 2;
        p->bombInfo.isInUse = 1;
        p->bombInfo.timer.SetCurrent(0);
        p->bombInfo.duration = 999;
        p->bombInfo.calc(p);
        g_EnemyManager.spellcardInfo.isCapturing = false;
        g_GameManager.DecreaseSubrank(200);
        g_EnemyManager.spellcardInfo.usedBomb = g_EnemyManager.spellcardInfo.isActive;
        if (g_IsSpellPracticeGame > 0 && !g_GameManager.isInReplay && g_EnemyManager.spellcardInfo.isActive)
        {
            i32 bossSubId = g_SpellPracticeBossSubId;
            if (bossSubId > 0 && bossSubId < 0xFFFE)
                ResultScreen::SaveCatk();
        }
    }
    if (p->playerState == PLAYER_STATE_DEAD)
    {
        if (p->respawnTimer != 0)
        {
            p->respawnTimer--;
            if (p->respawnTimer == 0)
            {
                g_GameManager.powerItemCountForScore = 0;
                if (g_IsOnline)
                {
                    D3DXVECTOR3 missDropTargets[6];
                    Rng missDropRng;
                    u16 missDropSeed = (u16)(g_GameManager.randomSeed ^ (g_GameManager.gameFrames * 73) ^
                                             (g_GameManager.deaths * 997));
                    missDropRng.Initialize(missDropSeed);
                    for (idx = 0; idx < ARRAY_SIZE_SIGNED(missDropTargets); idx++)
                    {
                        missDropTargets[idx].x = missDropRng.GetRandomF32ZeroToOne() * 288.0f + 48.0f;
                        missDropTargets[idx].y = missDropRng.GetRandomF32ZeroToOne() * 192.0f - 64.0f;
                        missDropTargets[idx].z = 0.0f;
                    }
                    if (g_GameManager.livesRemaining > 0)
                    {
                        g_ItemManager.SpawnItem(&p->positionCenter, ITEM_POWER_BIG, 2, &missDropTargets[0]);
                        g_ItemManager.SpawnItem(&p->positionCenter, ITEM_POWER_SMALL, 2, &missDropTargets[1]);
                        g_ItemManager.SpawnItem(&p->positionCenter, ITEM_POWER_SMALL, 2, &missDropTargets[2]);
                        g_ItemManager.SpawnItem(&p->positionCenter, ITEM_POWER_SMALL, 2, &missDropTargets[3]);
                        g_ItemManager.SpawnItem(&p->positionCenter, ITEM_POWER_SMALL, 2, &missDropTargets[4]);
                        g_ItemManager.SpawnItem(&p->positionCenter, ITEM_POWER_SMALL, 2, &missDropTargets[5]);
                        if (g_GameManager.currentPower <= 16)
                        {
                            g_GameManager.currentPower = 0;
                        }
                        else
                        {
                            g_GameManager.currentPower -= 16;
                        }
                        g_Gui.flags.flag2 = 2;
                    }
                    else
                    {
                        g_ItemManager.SpawnItem(&p->positionCenter, ITEM_FULL_POWER, 2, &missDropTargets[1]);
                        g_ItemManager.SpawnItem(&p->positionCenter, ITEM_FULL_POWER, 2, &missDropTargets[2]);
                        g_ItemManager.SpawnItem(&p->positionCenter, ITEM_FULL_POWER, 2, &missDropTargets[3]);
                        g_ItemManager.SpawnItem(&p->positionCenter, ITEM_FULL_POWER, 2, &missDropTargets[4]);
                        g_ItemManager.SpawnItem(&p->positionCenter, ITEM_FULL_POWER, 2, &missDropTargets[5]);
                        g_GameManager.currentPower = 0;
                        g_Gui.flags.flag2 = 2;
                        g_GameManager.extraLives = 255;
                    }
                }
                else
                {
                    if (g_GameManager.livesRemaining > 0)
                    {
                        g_ItemManager.SpawnItem(&p->positionCenter, ITEM_POWER_BIG, 2);
                        g_ItemManager.SpawnItem(&p->positionCenter, ITEM_POWER_SMALL, 2);
                        g_ItemManager.SpawnItem(&p->positionCenter, ITEM_POWER_SMALL, 2);
                        g_ItemManager.SpawnItem(&p->positionCenter, ITEM_POWER_SMALL, 2);
                        g_ItemManager.SpawnItem(&p->positionCenter, ITEM_POWER_SMALL, 2);
                        g_ItemManager.SpawnItem(&p->positionCenter, ITEM_POWER_SMALL, 2);
                        if (g_GameManager.currentPower <= 16)
                        {
                            g_GameManager.currentPower = 0;
                        }
                        else
                        {
                            g_GameManager.currentPower -= 16;
                        }
                        g_Gui.flags.flag2 = 2;
                    }
                    else
                    {
                        g_ItemManager.SpawnItem(&p->positionCenter, ITEM_FULL_POWER, 2);
                        g_ItemManager.SpawnItem(&p->positionCenter, ITEM_FULL_POWER, 2);
                        g_ItemManager.SpawnItem(&p->positionCenter, ITEM_FULL_POWER, 2);
                        g_ItemManager.SpawnItem(&p->positionCenter, ITEM_FULL_POWER, 2);
                        g_ItemManager.SpawnItem(&p->positionCenter, ITEM_FULL_POWER, 2);
                        g_GameManager.currentPower = 0;
                        g_Gui.flags.flag2 = 2;
                        g_GameManager.extraLives = 255;
                    }
                }
                g_GameManager.DecreaseSubrank(1600);
            }
        }
        else
        {
            scaleFactor1 = p->invulnerabilityTimer.AsFramesFloat() / 30.0f;
            p->playerSprite.scaleY = 3.0f * scaleFactor1 + 1.0f;
            p->playerSprite.scaleX = 1.0f - 1.0f * scaleFactor1;
            p->playerSprite.color =
                COLOR_SET_ALPHA(COLOR_WHITE, (u32)(255.0f - p->invulnerabilityTimer.AsFramesFloat() * 255.0f / 30.0f));
            p->playerSprite.flags.blendMode = AnmVmBlendMode_One;
            p->previousHorizontalSpeed = 0.0f;
            p->previousVerticalSpeed = 0.0f;
            if (p->invulnerabilityTimer.AsFrames() >= 30)
            {
                p->playerState = PLAYER_STATE_SPAWNING;
                if (g_IsOnline)
                {
                    p->positionCenter.x = g_GameManager.arcadeRegionSize.x / 2.0f + (p == &g_Player ? -16.0f : 16.0f);
                }
                else
                {
                    p->positionCenter.x = g_GameManager.arcadeRegionSize.x / 2.0f;
                }
                p->positionCenter.y = g_GameManager.arcadeRegionSize.y - 64.0f;
                p->positionCenter.z = 0.2;
                p->invulnerabilityTimer.SetCurrent(0);
                p->playerSprite.scaleX = 3.0;
                p->playerSprite.scaleY = 3.0;
                g_AnmManager->SetAndExecuteScriptIdx(&p->playerSprite, ANM_SCRIPT_PLAYER_IDLE);
                if (g_GameManager.livesRemaining <= 0)
                {
                    ZunBool otherPlayerAlive = 0;
                    if (g_IsOnline)
                    {
                        Player *other = (p == &g_Player) ? &g_Player2 : &g_Player;
                        if (other->playerState != PLAYER_STATE_DEAD)
                        {
                            otherPlayerAlive = 1;
                        }
                    }

                    if (otherPlayerAlive)
                    {
                        p->invulnerabilityTimer.SetCurrent(30);
                    }
                    else
                    {
                        if (g_Supervisor.curState == SUPERVISOR_STATE_GAMEMANAGER)
                        {
                            g_GameManager.isInRetryMenu = 1;
                        }
                    }
                }
                else
                {
                    g_GameManager.livesRemaining--;
                    g_Gui.flags.flag0 = 2;
                    if (g_GameManager.difficulty < 4 && g_GameManager.isInPracticeMode == 0)
                    {
                        g_GameManager.bombsRemaining = g_Supervisor.defaultConfig.bombCount;
                    }
                    else
                    {
                        g_GameManager.bombsRemaining = 3;
                    }
                    g_Gui.flags.flag1 = 2;
                    p->playerState = PLAYER_STATE_SPAWNING;
                }
            }
        }
    }

    if (p->playerState == PLAYER_STATE_SPAWNING)
    {
        p->bulletGracePeriod = 90;
        scaleFactor2 = 1.0f - p->invulnerabilityTimer.AsFramesFloat() / 30.0f;
        p->playerSprite.scaleY = 2.0f * scaleFactor2 + 1.0f;
        p->playerSprite.scaleX = 1.0f - 1.0f * scaleFactor2;
        p->playerSprite.flags.blendMode = AnmVmBlendMode_One;
        p->verticalMovementSpeedMultiplierDuringBomb = 1.0f;
        p->horizontalMovementSpeedMultiplierDuringBomb = 1.0f;
        p->playerSprite.color = COLOR_SET_ALPHA(COLOR_WHITE, p->invulnerabilityTimer.AsFrames() * 255 / 30);
        p->respawnTimer = 0;
        if (p->invulnerabilityTimer.AsFrames() >= 30)
        {
            p->playerState = PLAYER_STATE_INVULNERABLE;
            p->playerSprite.scaleX = 1.0f;
            p->playerSprite.scaleY = 1.0f;
            p->playerSprite.color = COLOR_WHITE;
            p->playerSprite.flags.blendMode = AnmVmBlendMode_InvSrcAlpha;
            p->invulnerabilityTimer.SetCurrent(240);
            p->respawnTimer = 6;
        }
    }
    if (p->bulletGracePeriod != 0)
    {
        p->bulletGracePeriod--;
        g_BulletManager.RemoveAllBullets(0);
    }
    if (p->playerState == PLAYER_STATE_INVULNERABLE)
    {
        p->invulnerabilityTimer.Decrement(1);
        if (p->invulnerabilityTimer.AsFrames() <= 0)
        {
            p->playerState = PLAYER_STATE_ALIVE;
            p->invulnerabilityTimer.SetCurrent(0);
            p->playerSprite.flags.colorOp = AnmVmColorOp_Modulate;
            p->playerSprite.color = COLOR_WHITE;
        }
        else if (p->invulnerabilityTimer.AsFrames() % 8 < 2)
        {
            p->playerSprite.flags.colorOp = AnmVmColorOp_Add;
            p->playerSprite.color = 0xff404040;
        }
        else
        {
            p->playerSprite.flags.colorOp = AnmVmColorOp_Modulate;
            p->playerSprite.color = COLOR_WHITE;
        }
    }
    else
    {
        p->invulnerabilityTimer.Tick();
    }
    if (p->playerState != PLAYER_STATE_DEAD && p->playerState != PLAYER_STATE_SPAWNING)
    {
        p->HandlePlayerInputs();
    }
    Player::UpdateHitboxMarker(p);
    g_AnmManager->ExecuteScript(&p->playerSprite);
    Player::UpdatePlayerBullets(p);
    if (p->orbState != ORB_HIDDEN)
    {
        g_AnmManager->ExecuteScript(&p->orbsSprite[0]);
        g_AnmManager->ExecuteScript(&p->orbsSprite[1]);
    }
    lastEnemyHit.x = -999.0;
    lastEnemyHit.y = -999.0;
    lastEnemyHit.z = 0.0;
    p->positionOfLastEnemyHit = lastEnemyHit;
    LogReplayPlayerStateChange(p);
    Player::UpdateFireBulletsTimer(p);
    return CHAIN_CALLBACK_RESULT_CONTINUE;
}

i32 Player::CalcDamageToEnemy(D3DXVECTOR3 *enemyPos, D3DXVECTOR3 *enemyHitboxSize, ZunBool *hitWithLazerDuringBomb)
{
    ZunVec3 bulletTopLeft;
    i32 damage;
    ZunVec3 enemyTopLeft;
    i32 idx;
    PlayerBullet *bullet;

    ZunVec3 bulletBottomRight;
    ZunVec3 enemyBottomRight;

    damage = 0;

    ZunVec3::SetVecCorners(&enemyTopLeft, &enemyBottomRight, enemyPos, enemyHitboxSize);
    bullet = &this->bullets[0];
    if (hitWithLazerDuringBomb)
    {
        *hitWithLazerDuringBomb = false;
    }
    for (idx = 0; idx < ARRAY_SIZE_SIGNED(this->bullets); idx++, bullet++)
    {
        if (bullet->bulletState == PLAYER_BULLET_STATE_INACTIVE ||
            bullet->bulletState != PLAYER_BULLET_STATE_FIRED && bullet->bulletType != BULLET_TYPE_2)
        {
            continue;
        }

        ZunVec3::SetVecCorners(&bulletTopLeft, &bulletBottomRight, &bullet->position, &bullet->size);

        if (bulletTopLeft.y > enemyBottomRight.y || bulletTopLeft.x > enemyBottomRight.x ||
            bulletBottomRight.y < enemyTopLeft.y || bulletBottomRight.x < enemyTopLeft.x)
        {
            continue;
        }
        if (!this->bombInfo.isInUse)
        {
            damage += bullet->damage;
        }
        else
        {
            damage += bullet->damage / 3 != 0 ? bullet->damage / 3 : 1;
        }

        if (bullet->bulletType == BULLET_TYPE_2)
        {
            bullet->damage = bullet->damage / 4;
            if (bullet->damage == 0)
            {
                bullet->damage = 1;
            }
            switch (bullet->sprite.anmFileIndex)
            {
            case ANM_SCRIPT_PLAYER_MARISA_A_ORB_BULLET_1:
                bullet->size.x = 32.0f;
                bullet->size.y = 32.0f;
                break;
            case ANM_SCRIPT_PLAYER_MARISA_A_ORB_BULLET_2:
                bullet->size.x = 42.0f;
                bullet->size.y = 42.0f;
                break;
            case ANM_SCRIPT_PLAYER_MARISA_A_ORB_BULLET_3:
                bullet->size.x = 48.0f;
                bullet->size.y = 48.0f;
                break;
            case ANM_SCRIPT_PLAYER_MARISA_A_ORB_BULLET_4:
                bullet->size.x = 48.0f;
                bullet->size.y = 48.0f;
            }
            if (bullet->unk_140.AsFrames() % 6 == 0)
            {
                g_EffectManager.SpawnParticles(PARTICLE_EFFECT_UNK_5, &bullet->position, 1, COLOR_WHITE);
            }
        }

        if (bullet->bulletType != BULLET_TYPE_LASER)
        {
            if (bullet->bulletState == PLAYER_BULLET_STATE_FIRED)
            {
                g_AnmManager->SetAndExecuteScriptIdx(&bullet->sprite, bullet->sprite.anmFileIndex + 0x20);
                g_EffectManager.SpawnParticles(PARTICLE_EFFECT_UNK_5, &bullet->position, 1, COLOR_WHITE);
                bullet->position.z = 0.1;
            }
            bullet->bulletState = PLAYER_BULLET_STATE_COLLIDED;
            bullet->velocity.x /= 8.0f;
            bullet->velocity.y /= 8.0f;
        }
        else
        {
            this->unk_9e4++;
            if (this->unk_9e4 % 8 == 0)
            {
                *bulletTopLeft.AsD3dXVec() = *enemyPos;
                bulletTopLeft.x = bullet->position.x;

                g_EffectManager.SpawnParticles(PARTICLE_EFFECT_UNK_5, bulletTopLeft.AsD3dXVec(), 1, COLOR_WHITE);
            }
        }
    }
    for (idx = 0; idx < ARRAY_SIZE_SIGNED(this->bombRegionSizes); idx++)
    {
        if (this->bombRegionSizes[idx].x <= 0.0f)
        {
            continue;
        }

        *bulletTopLeft.AsD3dXVec() = this->bombRegionPositions[idx] - this->bombRegionSizes[idx] / 2.0f;
        *bulletBottomRight.AsD3dXVec() = this->bombRegionPositions[idx] + this->bombRegionSizes[idx] / 2.0f;
        if (bulletTopLeft.x > enemyBottomRight.x || bulletBottomRight.x < enemyTopLeft.x ||
            bulletTopLeft.y > enemyBottomRight.y || bulletBottomRight.y < enemyTopLeft.y)
        {
            continue;
        }
        damage += this->bombRegionDamages[idx];
        this->unk_838[idx] += this->bombRegionDamages[idx];
        this->unk_9e4++;
        if (this->unk_9e4 % 4 == 0)
        {
            g_EffectManager.SpawnParticles(PARTICLE_EFFECT_UNK_3, enemyPos, 1, COLOR_WHITE);
        }
        if (this->bombInfo.isInUse && hitWithLazerDuringBomb)
        {
            *hitWithLazerDuringBomb = true;
        }
    }
    return damage;
}

void Player::UpdatePlayerBullets(Player *player)
{
    ZunVec2 vector;
    PlayerBullet *bullet;
    f32 vecLength;
    i32 idx;

    for (idx = 0; idx < ARRAY_SIZE_SIGNED(player->laserTimer); idx++)
    {
        if (player->laserTimer[idx].AsFrames() != 0)
        {
            player->laserTimer[idx].Decrement(1);
        }
    }
    bullet = &player->bullets[0];
    for (idx = 0; idx < ARRAY_SIZE_SIGNED(player->bullets); idx++, bullet++)
    {
        if (bullet->bulletState == PLAYER_BULLET_STATE_INACTIVE)
        {
            continue;
        }

        switch (bullet->bulletType)
        {
        case BULLET_TYPE_1:
            if (bullet->bulletState == PLAYER_BULLET_STATE_FIRED)
            {
                if (player->positionOfLastEnemyHit.x > -100.0f && bullet->unk_140.AsFrames() < 40 &&
                    bullet->unk_140.HasTicked())
                {
                    vector.x = player->positionOfLastEnemyHit.x - bullet->position.x;
                    vector.y = player->positionOfLastEnemyHit.y - bullet->position.y;

                    vecLength = vector.VectorLength() / (bullet->unk_134.y / 4.0f);
                    if (vecLength < 1.0f)
                    {
                        vecLength = 1.0f;
                    }

                    vector.x = vector.x / vecLength + bullet->velocity.x;
                    vector.y = vector.y / vecLength + bullet->velocity.y;

                    vecLength = vector.VectorLengthF64();

                    bullet->unk_134.y = ZUN_MIN(vecLength, 10.0f);

                    if (bullet->unk_134.y < 1.0f)
                    {
                        bullet->unk_134.y = 1.0f;
                    }

                    bullet->velocity.x = (vector.x * bullet->unk_134.y) / vecLength;
                    bullet->velocity.y = (vector.y * bullet->unk_134.y) / vecLength;
                }
                else
                {
                    if (bullet->unk_134.y < 10.0f)
                    {
                        bullet->unk_134.y += 0.33333333f;
                        vector.x = bullet->velocity.x;
                        vector.y = bullet->velocity.y;
                        vecLength = vector.VectorLengthF64();
                        bullet->velocity.x = vector.x * bullet->unk_134.y / vecLength;
                        bullet->velocity.y = vector.y * bullet->unk_134.y / vecLength;
                    }
                }
            }

            break;

        case BULLET_TYPE_2:
            if (bullet->bulletState == PLAYER_BULLET_STATE_FIRED)
            {
                bullet->velocity.y -= 0.3f;
            }
            break;
        case BULLET_TYPE_LASER:

            if (player->laserTimer[bullet->unk_152] == 70)
            {
                bullet->sprite.pendingInterrupt = 1;
            }
            else if (player->laserTimer[bullet->unk_152] == 1)
            {
                bullet->sprite.pendingInterrupt = 1;
            }

            bullet->position = player->orbsPosition[bullet->spawnPositionIdx - 1];

            bullet->position.x += bullet->sidewaysMotion;
            bullet->position.y /= 2.0f;
            bullet->position.z = 0.44f;

            bullet->sprite.scaleY = (bullet->position.y * 2) / 14.0f;

            bullet->size.y = bullet->position.y * 2;
            break;
        }

        bullet->sprite.pos.x = bullet->position[0] += bullet->velocity.x * g_Supervisor.effectiveFramerateMultiplier;

        bullet->sprite.pos.y = bullet->position[1] += bullet->velocity.y * g_Supervisor.effectiveFramerateMultiplier;

        bullet->sprite.pos.z = bullet->position.z;
        if (bullet->bulletType != BULLET_TYPE_LASER &&
            !g_GameManager.IsInBounds(bullet->position.x, bullet->position.y, bullet->sprite.sprite->widthPx,
                                      bullet->sprite.sprite->heightPx))
        {
            bullet->bulletState = PLAYER_BULLET_STATE_INACTIVE;
        }

        if (g_AnmManager->ExecuteScript(&bullet->sprite))
        {
            bullet->bulletState = PLAYER_BULLET_STATE_INACTIVE;
        }
        bullet->unk_140.Tick();
    }
}

ChainCallbackResult Player::OnDrawHighPrio(Player *p)
{
    Player::DrawBullets(p);
    if (p->bombInfo.isInUse != 0 && p->bombInfo.draw != NULL)
    {
        p->bombInfo.draw(p);
    }
    p->playerSprite.pos.x = g_GameManager.arcadeRegionTopLeftPos.x + p->positionCenter.x;
    p->playerSprite.pos.y = g_GameManager.arcadeRegionTopLeftPos.y + p->positionCenter.y;
    p->playerSprite.pos.z = 0.49;
    if (!g_GameManager.isInRetryMenu)
    {
        g_AnmManager->DrawNoRotation(&p->playerSprite);

        if (g_IsOnline && p->playerState != PLAYER_STATE_DEAD)
        {
            D3DXVECTOR3 textPos;
            textPos.x = p->playerSprite.pos.x - 5.0f;
            textPos.y = p->playerSprite.pos.y - 24.0f;
            textPos.z = 0.0f;

            D3DCOLOR savedColor = g_AsciiManager.color;
            D3DXVECTOR2 savedScale = g_AsciiManager.scale;

            g_AsciiManager.color = (p == &g_Player) ? COLOR_LIGHT_RED : COLOR_LIGHTBLUE;
            g_AsciiManager.scale = D3DXVECTOR2(0.6f, 0.6f);
            g_AsciiManager.AddString(&textPos, (p == &g_Player) ? "1P" : "2P");

            g_AsciiManager.color = savedColor;
            g_AsciiManager.scale = savedScale;
        }

        if (p->orbState != ORB_HIDDEN &&
            (p->playerState == PLAYER_STATE_ALIVE || p->playerState == PLAYER_STATE_INVULNERABLE))
        {
            p->orbsSprite[0].pos = p->orbsPosition[0];
            p->orbsSprite[1].pos = p->orbsPosition[1];
            p->orbsSprite[0].pos[0] += g_GameManager.arcadeRegionTopLeftPos.x;
            p->orbsSprite[0].pos[1] += g_GameManager.arcadeRegionTopLeftPos.y;
            p->orbsSprite[1].pos[0] += g_GameManager.arcadeRegionTopLeftPos.x;
            p->orbsSprite[1].pos[1] += g_GameManager.arcadeRegionTopLeftPos.y;
            p->orbsSprite[0].pos.z = 0.491;
            p->orbsSprite[1].pos.z = 0.491;
            g_AnmManager->Draw(&p->orbsSprite[0]);
            g_AnmManager->Draw(&p->orbsSprite[1]);
        }
    }
    return CHAIN_CALLBACK_RESULT_CONTINUE;
}

ChainCallbackResult Player::OnDrawLowPrio(Player *p)
{
    Player::DrawBulletExplosions(p);
    return CHAIN_CALLBACK_RESULT_CONTINUE;
}

ZunResult Player::HandlePlayerInputs()
{
    float intermediateFloat;

    float horizontalOrbOffset;
    float verticalOrbOffset;

    float horizontalSpeed = 0.0;
    float verticalSpeed = 0.0;
    PlayerDirection playerDirection = this->playerDirection;
    u16 input = this->input;

    this->playerDirection = MOVEMENT_NONE;
    if ((input & (TH_BUTTON_UP)))
    {
        this->playerDirection = MOVEMENT_UP;
        if ((input & (TH_BUTTON_LEFT)))
        {
            this->playerDirection = MOVEMENT_UP_LEFT;
        }
        if ((input & (TH_BUTTON_RIGHT)))
        {
            this->playerDirection = MOVEMENT_UP_RIGHT;
        }
    }
    else
    {
        if ((input & (TH_BUTTON_DOWN)))
        {
            this->playerDirection = MOVEMENT_DOWN;
            if ((input & (TH_BUTTON_LEFT)))
            {
                this->playerDirection = MOVEMENT_DOWN_LEFT;
            }
            if ((input & (TH_BUTTON_RIGHT)))
            {
                this->playerDirection = MOVEMENT_DOWN_RIGHT;
            }
        }
        else
        {
            if ((input & (TH_BUTTON_LEFT)))
            {
                this->playerDirection = MOVEMENT_LEFT;
            }
            if ((input & (TH_BUTTON_RIGHT)))
            {
                this->playerDirection = MOVEMENT_RIGHT;
            }
        }
    }
    if ((input & (TH_BUTTON_FOCUS)))
    {
        this->isFocus = true;
    }
    else
    {
        this->isFocus = false;
    }

    switch (this->playerDirection)
    {
    case MOVEMENT_RIGHT:
        if ((input & (TH_BUTTON_FOCUS)))
        {
            horizontalSpeed = this->characterData.orthogonalMovementSpeedFocus;
        }
        else
        {
            horizontalSpeed = this->characterData.orthogonalMovementSpeed;
        }
        break;
    case MOVEMENT_LEFT:
        if ((input & (TH_BUTTON_FOCUS)))
        {
            horizontalSpeed = -this->characterData.orthogonalMovementSpeedFocus;
        }
        else
        {
            horizontalSpeed = -this->characterData.orthogonalMovementSpeed;
        }
        break;
    case MOVEMENT_UP:
        if ((input & (TH_BUTTON_FOCUS)))
        {
            verticalSpeed = -this->characterData.orthogonalMovementSpeedFocus;
        }
        else
        {
            verticalSpeed = -this->characterData.orthogonalMovementSpeed;
        }
        break;
    case MOVEMENT_DOWN:
        if ((input & (TH_BUTTON_FOCUS)))
        {
            verticalSpeed = this->characterData.orthogonalMovementSpeedFocus;
        }
        else
        {
            verticalSpeed = this->characterData.orthogonalMovementSpeed;
        }
        break;
    case MOVEMENT_UP_LEFT:
        if ((input & (TH_BUTTON_FOCUS)))
        {
            horizontalSpeed = -this->characterData.diagonalMovementSpeedFocus;
        }
        else
        {
            horizontalSpeed = -this->characterData.diagonalMovementSpeed;
        }
        verticalSpeed = horizontalSpeed;
        break;
    case MOVEMENT_DOWN_LEFT:
        if ((input & (TH_BUTTON_FOCUS)))
        {
            horizontalSpeed = -this->characterData.diagonalMovementSpeedFocus;
        }
        else
        {
            horizontalSpeed = -this->characterData.diagonalMovementSpeed;
        }
        verticalSpeed = -horizontalSpeed;
        break;
    case MOVEMENT_UP_RIGHT:
        if ((input & (TH_BUTTON_FOCUS)))
        {
            horizontalSpeed = this->characterData.diagonalMovementSpeedFocus;
        }
        else
        {
            horizontalSpeed = this->characterData.diagonalMovementSpeed;
        }
        verticalSpeed = -horizontalSpeed;
        break;
    case MOVEMENT_DOWN_RIGHT:
        if ((input & (TH_BUTTON_FOCUS)))
        {
            horizontalSpeed = this->characterData.diagonalMovementSpeedFocus;
        }
        else
        {
            horizontalSpeed = this->characterData.diagonalMovementSpeed;
        }
        verticalSpeed = horizontalSpeed;
    }

    if (horizontalSpeed < 0.0f && this->previousHorizontalSpeed >= 0.0f)
    {
        g_AnmManager->SetAndExecuteScriptIdx(&this->playerSprite, ANM_SCRIPT_PLAYER_MOVING_LEFT);
    }
    else if (!horizontalSpeed && this->previousHorizontalSpeed < 0.0f)
    {
        g_AnmManager->SetAndExecuteScriptIdx(&this->playerSprite, ANM_SCRIPT_PLAYER_STOPPING_LEFT);
    }

    if (horizontalSpeed > 0.0f && this->previousHorizontalSpeed <= 0.0f)
    {
        g_AnmManager->SetAndExecuteScriptIdx(&this->playerSprite, ANM_SCRIPT_PLAYER_MOVING_RIGHT);
    }
    else if (!horizontalSpeed && this->previousHorizontalSpeed > 0.0f)
    {
        g_AnmManager->SetAndExecuteScriptIdx(&this->playerSprite, ANM_SCRIPT_PLAYER_STOPPING_RIGHT);
    }

    this->previousHorizontalSpeed = horizontalSpeed;
    this->previousVerticalSpeed = verticalSpeed;

    this->positionCenter[0] +=
        horizontalSpeed * this->horizontalMovementSpeedMultiplierDuringBomb * g_Supervisor.effectiveFramerateMultiplier;
    this->positionCenter[1] +=
        verticalSpeed * this->verticalMovementSpeedMultiplierDuringBomb * g_Supervisor.effectiveFramerateMultiplier;

    if (this->positionCenter.x < g_GameManager.playerMovementAreaTopLeftPos.x)
    {
        this->positionCenter.x = g_GameManager.playerMovementAreaTopLeftPos.x;
    }
    else if (g_GameManager.playerMovementAreaTopLeftPos.x + g_GameManager.playerMovementAreaSize.x <
             this->positionCenter.x)
    {
        this->positionCenter.x = g_GameManager.playerMovementAreaTopLeftPos.x + g_GameManager.playerMovementAreaSize.x;
    }

    if (this->positionCenter.y < g_GameManager.playerMovementAreaTopLeftPos.y)
    {
        this->positionCenter.y = g_GameManager.playerMovementAreaTopLeftPos.y;
    }
    else if (g_GameManager.playerMovementAreaTopLeftPos.y + g_GameManager.playerMovementAreaSize.y <
             this->positionCenter.y)
    {
        this->positionCenter.y = g_GameManager.playerMovementAreaTopLeftPos.y + g_GameManager.playerMovementAreaSize.y;
    }

    this->hitboxTopLeft = this->positionCenter - this->hitboxSize;

    this->hitboxBottomRight = this->positionCenter + this->hitboxSize;

    this->grabItemTopLeft = this->positionCenter - this->grabItemSize;

    this->grabItemBottomRight = this->positionCenter + this->grabItemSize;

    this->orbsPosition[0] = this->positionCenter;
    this->orbsPosition[1] = this->positionCenter;

    verticalOrbOffset = 0.0;
    horizontalOrbOffset = verticalOrbOffset;

    if (g_GameManager.currentPower < 8)
    {
        this->orbState = ORB_HIDDEN;
    }
    else if (this->orbState == ORB_HIDDEN)
    {
        this->orbState = ORB_UNFOCUSED;
    }

    auto calcOrbFocusing = [](Player *p, f32 &h, f32 &v) {
        p->focusMovementTimer.Tick();
        f32 t = p->focusMovementTimer.AsFramesFloat() / 8.0f;
        v = (1.0f - t) * 32.0f - 32.0f;
        t *= t;
        h = -16.0f * t + 24.0f;
        if (p->focusMovementTimer.current >= 8)
        {
            p->orbState = ORB_FOCUSED;
        }
    };

    auto calcOrbUnfocusing = [](Player *p, f32 &h, f32 &v) {
        p->focusMovementTimer.Tick();
        f32 t = p->focusMovementTimer.AsFramesFloat() / 8.0f;
        v = (32.0f * t) - 32.0f;
        t = 1.0f - t * t;
        h = -16.0f * t + 24.0f;
        if (p->focusMovementTimer.current >= 8)
        {
            p->orbState = ORB_UNFOCUSED;
        }
    };

    switch (this->orbState)
    {
    case ORB_HIDDEN:
        this->focusMovementTimer.InitializeForPopup();
        break;

    case ORB_UNFOCUSED:
        horizontalOrbOffset = 24.0f;
        this->focusMovementTimer.InitializeForPopup();
        if (this->isFocus)
        {
            this->orbState = ORB_FOCUSING;
            calcOrbFocusing(this, horizontalOrbOffset, verticalOrbOffset);
        }
        break;

    case ORB_FOCUSING:
        if (!this->isFocus)
        {
            this->orbState = ORB_UNFOCUSING;
            this->focusMovementTimer.SetCurrent(8 - this->focusMovementTimer.AsFrames());
            calcOrbUnfocusing(this, horizontalOrbOffset, verticalOrbOffset);
        }
        else
        {
            calcOrbFocusing(this, horizontalOrbOffset, verticalOrbOffset);
        }
        break;

    case ORB_FOCUSED:
        horizontalOrbOffset = 8.0f;
        verticalOrbOffset = -32.0f;
        this->focusMovementTimer.InitializeForPopup();
        if (!this->isFocus)
        {
            this->orbState = ORB_UNFOCUSING;
            calcOrbUnfocusing(this, horizontalOrbOffset, verticalOrbOffset);
        }
        break;

    case ORB_UNFOCUSING:
        if (this->isFocus)
        {
            this->orbState = ORB_FOCUSING;
            this->focusMovementTimer.SetCurrent(8 - this->focusMovementTimer.AsFrames());
            calcOrbFocusing(this, horizontalOrbOffset, verticalOrbOffset);
        }
        else
        {
            calcOrbUnfocusing(this, horizontalOrbOffset, verticalOrbOffset);
        }
        break;
    }

    this->orbsPosition[0].x -= horizontalOrbOffset;
    this->orbsPosition[1].x += horizontalOrbOffset;
    this->orbsPosition[0].y += verticalOrbOffset;
    this->orbsPosition[1].y += verticalOrbOffset;
    if ((input & (TH_BUTTON_SHOOT)) && !g_Gui.HasCurrentMsgIdx())
    {
        this->StartFireBulletTimer(this);
    }
    this->previousFrameInput = input;
    return ZUN_SUCCESS;
}

void Player::DrawBullets(Player *p)
{
    i32 bulletIdx;
    PlayerBullet *bullets;

    bullets = p->bullets;
    for (bulletIdx = 0; bulletIdx < ARRAY_SIZE_SIGNED(p->bullets); bulletIdx++, bullets++)
    {
        if (bullets->bulletState != PLAYER_BULLET_STATE_FIRED)
        {
            continue;
        }
        if (bullets->sprite.autoRotate)
        {
            bullets->sprite.rotation.z = ZUN_PI / 2 - utils::AddNormalizeAngle(bullets->unk_134.z, ZUN_PI);
        }
        g_AnmManager->Draw2(&bullets->sprite);
    }
}

void Player::DrawBulletExplosions(Player *p)
{
    i32 bulletIdx;
    PlayerBullet *bullets;

    bullets = p->bullets;
    for (bulletIdx = 0; bulletIdx < ARRAY_SIZE_SIGNED(p->bullets); bulletIdx++, bullets++)
    {
        if (bullets->bulletState != PLAYER_BULLET_STATE_COLLIDED)
        {
            continue;
        }
        if (bullets->sprite.autoRotate)
        {
            bullets->sprite.rotation.z = ZUN_PI / 2 - utils::AddNormalizeAngle(bullets->unk_134.z, ZUN_PI);
        }
        bullets->sprite.pos.z = 0.4f;
        g_AnmManager->Draw2(&bullets->sprite);
    }
}

void Player::StartFireBulletTimer(Player *p)
{
    if (p->fireBulletTimer.AsFrames() < 0)
    {
        p->fireBulletTimer.InitializeForPopup();
    }
}

ZunResult Player::UpdateFireBulletsTimer(Player *p)
{
    if (p->fireBulletTimer.AsFrames() < 0)
    {
        return ZUN_SUCCESS;
    }

    if (p->fireBulletTimer.HasTicked() && (!g_Player.bombInfo.isInUse || g_GameManager.character != CHARA_MARISA ||
                                           g_GameManager.shotType != SHOT_TYPE_B))
    {
        p->SpawnBullets(p, p->fireBulletTimer.AsFrames());
    }

    p->fireBulletTimer.Tick();

    if (p->fireBulletTimer.AsFrames() >= 30 || p->playerState == PLAYER_STATE_DEAD ||
        p->playerState == PLAYER_STATE_SPAWNING)
    {
        p->fireBulletTimer.SetCurrent(-1);
    }
    return ZUN_SUCCESS;
}

f32 Player::AngleFromPlayer(D3DXVECTOR3 *pos)
{
    f32 relX;
    f32 relY;

    relX = pos->x - this->positionCenter.x;
    relY = pos->y - this->positionCenter.y;
    if (relY == 0.0f && relX == 0.0f)
    {
        return ZUN_PI / 2;
    }
    return atan2f(relY, relX);
}

f32 Player::AngleToPlayer(D3DXVECTOR3 *pos)
{
    f32 relX;
    f32 relY;

    relX = this->positionCenter.x - pos->x;
    relY = this->positionCenter.y - pos->y;
    if (relY == 0.0f && relX == 0.0f)
    {

        return RADIANS(90.0f);
    }
    return atan2f(relY, relX);
}

void Player::SpawnBullets(Player *p, u32 timer)
{
    FireBulletResult bulletResult;
    PlayerBullet *curBullet;
    i32 curBulletIdx;
    u32 idx;

    idx = 0;
    curBullet = p->bullets;

    for (curBulletIdx = 0; curBulletIdx < ARRAY_SIZE_SIGNED(p->bullets); curBulletIdx++, curBullet++)
    {
        if (curBullet->bulletState != PLAYER_BULLET_STATE_INACTIVE)
        {
            continue;
        }
        do
        {
            if (!p->isFocus)
            {
                bulletResult = (*p->fireBulletCallback)(p, curBullet, idx, timer);
            }
            else
            {
                bulletResult = (*p->fireBulletFocusCallback)(p, curBullet, idx, timer);
            }
            if (bulletResult >= 0)
            {
                curBullet->sprite.pos.x = curBullet->position.x;
                curBullet->sprite.pos.y = curBullet->position.y;
                curBullet->sprite.pos.z = 0.495f;
                curBullet->bulletState = PLAYER_BULLET_STATE_FIRED;
            }
            if (bulletResult == FBR_STOP_SPAWNING || bulletResult > 0)
            {
                return;
            }
            idx++;
        } while (bulletResult == FBR_SPAWN_MORE);
    }
}

FireBulletResult Player::FireSingleBullet(Player *player, PlayerBullet *bullet, i32 bulletIdx,
                                          i32 framesSinceLastBullet, CharacterPowerData *powerData)
{
    CharacterPowerBulletData *bulletData;
    i32 bulletFrame;
    i32 unused;
    i32 unused2;
    i32 unused3;

    while (g_GameManager.currentPower >= powerData->power)
    {
        powerData++;
    }

    bulletData = powerData->bullets + bulletIdx;

    bool shouldShoot = false;
    if (bulletData->bulletType == BULLET_TYPE_LASER)
    {
        bulletFrame = bulletData->bulletFrame;
        if (!player->laserTimer[bulletFrame].AsFrames())
        {
            player->laserTimer[bulletFrame].SetCurrent(bulletData->waitBetweenBullets);

            bullet->unk_152 = bulletFrame;
            bullet->spawnPositionIdx = bulletData->spawnPositionIdx;
            bullet->sidewaysMotion = bulletData->motion.x;
            bullet->unk_134.x = bulletData->motion.y;
            shouldShoot = true;
        }
    }
    else if (framesSinceLastBullet % bulletData->waitBetweenBullets == bulletData->bulletFrame)
    {
        shouldShoot = true;
    }

    if (shouldShoot)
    {
        g_AnmManager->SetAndExecuteScriptIdx(&bullet->sprite, bulletData->anmFileIdx);
        if (!bulletData->spawnPositionIdx)
        {
            bullet->position = player->positionCenter;
        }
        else
        {
            bullet->position = player->orbsPosition[bulletData->spawnPositionIdx - 1];
        }
        bullet->position[0] += bulletData->motion.x;
        bullet->position[1] += bulletData->motion.y;

        bullet->position.z = 0.495f;

        bullet->size.x = bulletData->size.x;
        bullet->size.y = bulletData->size.y;
        bullet->size.z = 1.0f;
        bullet->unk_134.z = bulletData->direction;
        bullet->unk_134.y = bulletData->velocity;

        bullet->velocity.x = cosf(bulletData->direction) * bulletData->velocity;

        bullet->velocity.y = sinf(bulletData->direction) * bulletData->velocity;

        bullet->unk_140.InitializeForPopup();

        bullet->bulletType = bulletData->bulletType;
        bullet->damage = bulletData->damage;
        if (bulletData->bulletSoundIdx >= 0)
        {
            g_SoundPlayer.PlaySoundByIdx((SoundIdx)bulletData->bulletSoundIdx, 0);
        }

        return bulletIdx >= powerData->numBullets - 1;
    }

    if (bulletIdx >= powerData->numBullets - 1)
    {
        return FBR_STOP_SPAWNING;
    }
    else
    {
        return FBR_SPAWN_MORE;
    }
}

FireBulletResult Player::FireBulletReimuA(Player *player, PlayerBullet *bullet, u32 bulletIdx,
                                          u32 framesSinceLastBullet)
{
    return player->FireSingleBullet(player, bullet, bulletIdx, framesSinceLastBullet, g_CharacterPowerDataReimuA);
}

FireBulletResult Player::FireBulletReimuB(Player *player, PlayerBullet *bullet, u32 bulletIdx,
                                          u32 framesSinceLastBullet)
{
    return player->FireSingleBullet(player, bullet, bulletIdx, framesSinceLastBullet, g_CharacterPowerDataReimuB);
}

FireBulletResult Player::FireBulletMarisaA(Player *player, PlayerBullet *bullet, u32 bulletIdx,
                                           u32 framesSinceLastBullet)
{
    return player->FireSingleBullet(player, bullet, bulletIdx, framesSinceLastBullet, g_CharacterPowerDataMarisaA);
}

FireBulletResult Player::FireBulletMarisaB(Player *player, PlayerBullet *bullet, u32 bulletIdx,
                                           u32 framesSinceLastBullet)
{
    return player->FireSingleBullet(player, bullet, bulletIdx, framesSinceLastBullet, g_CharacterPowerDataMarisaB);
}

i32 Player::CheckGraze(D3DXVECTOR3 *center, D3DXVECTOR3 *size)
{
    D3DXVECTOR3 bombBottomRight;
    PlayerRect *bombProjectile;
    D3DXVECTOR3 bombTopLeft;
    D3DXVECTOR3 bulletBottomRight;
    D3DXVECTOR3 bulletTopLeft;
    i32 i;

    bulletTopLeft.x = center->x - size->x / 2.0f - 20.0f;
    bulletTopLeft.y = center->y - size->y / 2.0f - 20.0f;
    bulletBottomRight.x = center->x + size->x / 2.0f + 20.0f;
    bulletBottomRight.y = center->y + size->y / 2.0f + 20.0f;
    bombProjectile = this->bombProjectiles;

    for (i = 0; i < ARRAY_SIZE_SIGNED(this->bombProjectiles); i++, bombProjectile++)
    {
        if (bombProjectile->sizeX == 0.0f)
        {
            continue;
        }

        bombTopLeft.x = bombProjectile->posX - bombProjectile->sizeX / 2.0f;
        bombTopLeft.y = bombProjectile->posY - bombProjectile->sizeY / 2.0f;
        bombBottomRight.x = bombProjectile->sizeX / 2.0f + bombProjectile->posX;
        bombBottomRight.y = bombProjectile->sizeY / 2.0f + bombProjectile->posY;

        if (!(bombTopLeft.x > bulletBottomRight.x || bombBottomRight.x < bulletTopLeft.x ||
              bombTopLeft.y > bulletBottomRight.y || bombBottomRight.y < bulletTopLeft.y))
        {
            return 2;
        }
    }

    if (this->playerState == PLAYER_STATE_DEAD || this->playerState == PLAYER_STATE_SPAWNING)
    {
        return 0;
    }
    if (this->hitboxTopLeft.x > bulletBottomRight.x || this->hitboxBottomRight.x < bulletTopLeft.x ||
        this->hitboxTopLeft.y > bulletBottomRight.y || this->hitboxBottomRight.y < bulletTopLeft.y)
    {
        return 0;
    }

    this->ScoreGraze(center);
    return 1;
}

i32 Player::CalcKillBoxCollision(D3DXVECTOR3 *bulletCenter, D3DXVECTOR3 *bulletSize)
{
    PlayerRect *curBombProjectile;
    f32 bulletLeft, bulletTop, bulletRight, bulletBottom;
    f32 bombProjectileLeft, bombProjectileTop, bombProjectileRight, bombProjectileBottom;
    i32 curBombIdx;
    i32 padding1, padding2, padding3, padding4;

    curBombProjectile = this->bombProjectiles;
    bulletLeft = bulletCenter->x - bulletSize->x / 2.0f;
    bulletTop = bulletCenter->y - bulletSize->y / 2.0f;
    bulletRight = bulletCenter->x + bulletSize->x / 2.0f;
    bulletBottom = bulletCenter->y + bulletSize->y / 2.0f;
    for (curBombIdx = 0; curBombIdx < ARRAY_SIZE_SIGNED(this->bombProjectiles); curBombIdx++, curBombProjectile++)
    {
        if (curBombProjectile->sizeX == 0.0f)
        {
            continue;
        }
        bombProjectileLeft = curBombProjectile->posX - curBombProjectile->sizeX / 2.0f;
        bombProjectileTop = curBombProjectile->posY - curBombProjectile->sizeY / 2.0f;
        bombProjectileRight = curBombProjectile->posX + curBombProjectile->sizeX / 2.0f;
        bombProjectileBottom = curBombProjectile->posY + curBombProjectile->sizeY / 2.0f;
        if (!(bombProjectileLeft > bulletRight || bombProjectileRight < bulletLeft ||
              bombProjectileTop > bulletBottom || bombProjectileBottom < bulletTop))
        {
            return 2;
        }
    }
    if (this->hitboxTopLeft.x > bulletRight || this->hitboxTopLeft.y > bulletBottom ||
        this->hitboxBottomRight.x < bulletLeft || this->hitboxBottomRight.y < bulletTop)
    {
        return 0;
    }
    else if (this->playerState != PLAYER_STATE_ALIVE)
    {
        return 1;
    }
    else
    {
        this->Die();
        return 1;
    }
}

i32 Player::CalcLaserHitbox(D3DXVECTOR3 *laserCenter, D3DXVECTOR3 *laserSize, D3DXVECTOR3 *rotation, f32 angle,
                            i32 canGraze)
{
    D3DXVECTOR3 laserTopLeft;
    D3DXVECTOR3 laserBottomRight;
    D3DXVECTOR3 playerRelativeTopLeft;
    D3DXVECTOR3 playerRelativeBottomRight;

    laserTopLeft = this->positionCenter - *rotation;
    utils::Rotate(&laserBottomRight, &laserTopLeft, angle);
    laserBottomRight.z = 0;
    laserTopLeft = laserBottomRight + *rotation;
    playerRelativeTopLeft = laserTopLeft - this->hitboxSize;
    playerRelativeBottomRight = laserTopLeft + this->hitboxSize;

    laserTopLeft = *laserCenter - *laserSize / 2.0f;
    laserBottomRight = *laserCenter + *laserSize / 2.0f;

    bool collides = (playerRelativeTopLeft.x <= laserBottomRight.x && playerRelativeBottomRight.x >= laserTopLeft.x &&
                     playerRelativeTopLeft.y <= laserBottomRight.y && playerRelativeBottomRight.y >= laserTopLeft.y);
    if (collides)
    {
        if (this->playerState != PLAYER_STATE_ALIVE)
        {
            return 0;
        }

        this->Die();
        return 1;
    }

    if (canGraze == 0)
    {
        return 0;
    }

    laserTopLeft.x -= 48.0f;
    laserTopLeft.y -= 48.0f;
    laserBottomRight.x += 48.0f;
    laserBottomRight.y += 48.0f;

    if (playerRelativeTopLeft.x > laserBottomRight.x || playerRelativeBottomRight.x < laserTopLeft.x ||
        playerRelativeTopLeft.y > laserBottomRight.y || playerRelativeBottomRight.y < laserTopLeft.y)
    {
        return 0;
    }
    if (this->playerState == PLAYER_STATE_DEAD || this->playerState == PLAYER_STATE_SPAWNING)
    {
        return 0;
    }

    this->ScoreGraze(&this->positionCenter);
    return 2;
}

i32 Player::CalcItemBoxCollision(D3DXVECTOR3 *itemCenter, D3DXVECTOR3 *itemSize)
{
    if (this->playerState != PLAYER_STATE_ALIVE && this->playerState != PLAYER_STATE_INVULNERABLE)
    {
        return 0;
    }
    D3DXVECTOR3 itemTopLeft;
    D3DXVECTOR3 itemTopLeftTemp = *itemCenter - *itemSize / 2.0f;
    memcpy(&itemTopLeft, &itemTopLeftTemp, sizeof(D3DXVECTOR3));
    D3DXVECTOR3 itemBottomRight;
    D3DXVECTOR3 itemBottomRightTemp = *itemCenter + *itemSize / 2.0f;
    memcpy(&itemBottomRight, &itemBottomRightTemp, sizeof(D3DXVECTOR3));

    if (this->grabItemTopLeft.x > itemBottomRight.x || this->grabItemBottomRight.x < itemTopLeft.x ||
        this->grabItemTopLeft.y > itemBottomRight.y || this->grabItemBottomRight.y < itemTopLeft.y)
    {
        return 0;
    }
    else
    {
        return 1;
    }
}

void Player::ScoreGraze(D3DXVECTOR3 *center)
{
    D3DXVECTOR3 particlePosition;

    if (g_Player.bombInfo.isInUse == 0)
    {
        if (g_GameManager.grazeInStage < 9999)
        {
            g_GameManager.grazeInStage++;
        }
        if (g_GameManager.grazeInTotal < 999999)
        {
            g_GameManager.grazeInTotal++;
        }
    }

    particlePosition = (this->positionCenter + *center) / 2.0f;
    g_EffectManager.SpawnParticles(PARTICLE_EFFECT_UNK_8, &particlePosition, 1, COLOR_WHITE);
    g_GameManager.AddScore(500);
    g_GameManager.IncreaseSubrank(6);
    g_Gui.flags.flag3 = 2;
    g_SoundPlayer.PlaySoundByIdx(SOUND_GRAZE, 0);
}

void Player::Die()
{
    int curLaserTimerIdx;

    g_EnemyManager.spellcardInfo.isCapturing = 0;
    if (g_IsSpellPracticeGame > 0 && !g_GameManager.isInReplay)
    {
        i32 bossSubId = g_SpellPracticeBossSubId;
        if (bossSubId > 0 && bossSubId < 0xFFFE)
        {
            if (!g_EnemyManager.spellcardInfo.isActive)
            {

                if (bossSubId < CATK_NUM_CAPTURES && g_GameManager.catk[bossSubId].numAttempts < 9999)
                {
                    g_GameManager.catk[bossSubId].numAttempts++;
                }
            }
            ResultScreen::SaveCatk();
        }
    }
    g_EffectManager.SpawnParticles(PARTICLE_EFFECT_UNK_12, &this->positionCenter, 1, COLOR_NEONBLUE);
    g_EffectManager.SpawnParticles(PARTICLE_EFFECT_UNK_6, &this->positionCenter, 16, COLOR_WHITE);
    this->playerState = PLAYER_STATE_DEAD;
    this->invulnerabilityTimer.InitializeForPopup();
    g_SoundPlayer.PlaySoundByIdx(SOUND_PICHUN, 0);
    g_GameManager.deaths++;
    for (curLaserTimerIdx = 0; curLaserTimerIdx < ARRAY_SIZE_SIGNED(this->laserTimer); curLaserTimerIdx++)
    {
        this->laserTimer[curLaserTimerIdx].SetCurrent(2);
    }
    return;
}
}; // namespace th06
