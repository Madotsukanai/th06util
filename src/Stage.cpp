#include "Stage.hpp"
#include "AnmIdx.hpp"
#include "AnmManager.hpp"
#include "Chain.hpp"
#include "ChainPriorities.hpp"
#include "FileSystem.hpp"
#include "GameManager.hpp"
#include "Gui.hpp"
#include "ScreenEffect.hpp"
#include "Supervisor.hpp"
#include "ZunColor.hpp"
#include "utils.hpp"
#include <d3d8.h>

#ifdef USE_VULKAN
#include "renderer.hpp"
#endif

namespace th06
{
ChainElem g_StageCalcChain;
ChainElem g_StageOnDrawHighPrioChain;
ChainElem g_StageOnDrawLowPrioChain;

StageFile g_StageFiles[8] = {
    {"dummy", "dummy"},
    {"data/stg1bg.anm", "data/stage1.std"},
    {"data/stg2bg.anm", "data/stage2.std"},
    {"data/stg3bg.anm", "data/stage3.std"},
    {"data/stg4bg.anm", "data/stage4.std"},
    {"data/stg5bg.anm", "data/stage5.std"},
    {"data/stg6bg.anm", "data/stage6.std"},
    {"data/stg7bg.anm", "data/stage7.std"},
};
Stage g_Stage;

Stage::Stage()
{
}

ChainCallbackResult Stage::OnUpdate(Stage *stage)
{
    f32 posInterpRatio;
    f32 facingDirInterpRatio;
    D3DXVECTOR3 pos;
    i32 idx;
    f32 skyFogInterpRatio;
    RawStageInstr *curInsn;

    if (stage->stdData == NULL)
    {
        return CHAIN_CALLBACK_RESULT_CONTINUE;
    }
    if (g_GameManager.isTimeStopped)
    {

        COLOR_SET_COMPONENT(stage->spellcardBackground.color, COLOR_ALPHA_BYTE_IDX, 0x60);
        COLOR_SET_COMPONENT(stage->spellcardBackground.color, COLOR_BLUE_BYTE_IDX, 0x80);
        COLOR_SET_COMPONENT(stage->spellcardBackground.color, COLOR_GREEN_BYTE_IDX, 0x30);
        COLOR_SET_COMPONENT(stage->spellcardBackground.color, COLOR_RED_BYTE_IDX, 0x30);
        return CHAIN_CALLBACK_RESULT_CONTINUE;
    }
    for (;;)
    {
        curInsn = stage->beginningOfScript + stage->instructionIndex;
        switch (curInsn->opcode)
        {
        case STDOP_CAMERA_POSITION_KEY:
            if (curInsn->frame == -1)
            {
                stage->positionInterpInitial = *(D3DXVECTOR3 *)curInsn->args;
                stage->position.x = stage->positionInterpInitial.x;
                stage->position.y = stage->positionInterpInitial.y;
                stage->position.z = stage->positionInterpInitial.z;
            }
            else if (stage->scriptTime.current >= curInsn->frame)
            {
                pos = *(D3DXVECTOR3 *)curInsn->args;
                stage->position.x = pos.x;
                stage->position.y = pos.y;
                stage->position.z = pos.z;
                stage->positionInterpInitial = pos;
                stage->positionInterpStartTime = curInsn->frame;
                stage->instructionIndex++;
                curInsn++;
                while (curInsn->opcode != 0)
                {
                    curInsn++;
                }
                stage->positionInterpEndTime = curInsn->frame;
                stage->positionInterpFinal = *(D3DXVECTOR3 *)curInsn->args;
            }
            break;
        case STDOP_FOG:
            if (stage->scriptTime.current >= curInsn->frame)
            {
                stage->skyFog.color = curInsn->args[0];
                stage->skyFog.nearPlane = ((f32 *)curInsn->args)[1];
                stage->skyFog.farPlane = ((f32 *)curInsn->args)[2];
                if (stage->skyFogInterpDuration == 0)
                {
#ifdef USE_VULKAN
                    g_Renderer->SetFog(true, stage->skyFog.color, stage->skyFog.nearPlane, stage->skyFog.farPlane);
#else
                    g_Supervisor.d3dDevice->SetRenderState(D3DRS_FOGCOLOR, stage->skyFog.color);
                    g_Supervisor.d3dDevice->SetRenderState(D3DRS_FOGSTART, *(u32 *)&stage->skyFog.nearPlane);
                    g_Supervisor.d3dDevice->SetRenderState(D3DRS_FOGEND, *(u32 *)&stage->skyFog.farPlane);
#endif
                }
                stage->instructionIndex++;
                stage->skyFogInterpFinal = stage->skyFog;
                continue;
            }
            break;
        case STDOP_FOG_INTERP:
            if (stage->scriptTime.current >= curInsn->frame)
            {
                stage->skyFogInterpInitial = stage->skyFog;
                stage->skyFogInterpDuration = curInsn->args[0];
                stage->skyFogInterpTimer.InitializeForPopup();
                stage->instructionIndex++;
                continue;
            }
            break;
        case STDOP_CAMERA_FACING:
            if (stage->scriptTime.current >= curInsn->frame)
            {
                stage->facingDirInterpInitial = stage->facingDirInterpFinal;
                stage->facingDirInterpFinal = *(D3DXVECTOR3 *)curInsn->args;
                stage->instructionIndex++;
                continue;
            }
            break;
        case STDOP_CAMERA_FACING_INTERP_LINEAR:
            if (stage->scriptTime.current >= curInsn->frame)
            {
                stage->facingDirInterpDuration = curInsn->args[0];
                stage->facingDirInterpTimer.InitializeForPopup();
                stage->instructionIndex++;
                continue;
            }
            break;
        case STDOP_PAUSE:
            if (stage->unpauseFlag)
            {
                stage->instructionIndex++;
                stage->unpauseFlag = '\0';
                continue;
            }
            break;
        }
        if (curInsn->frame != -1)
        {
            posInterpRatio = (stage->scriptTime.AsFramesFloat() - stage->positionInterpStartTime) /
                             (stage->positionInterpEndTime - stage->positionInterpStartTime);
            pos = stage->positionInterpFinal;
            stage->position.x =
                (pos.x - stage->positionInterpInitial.x) * posInterpRatio + stage->positionInterpInitial.x;
            stage->position.y =
                (pos.y - stage->positionInterpInitial.y) * posInterpRatio + stage->positionInterpInitial.y;
            stage->position.z =
                (pos.z - stage->positionInterpInitial.z) * posInterpRatio + stage->positionInterpInitial.z;
        }
        if (stage->facingDirInterpDuration != 0)
        {
            if (stage->facingDirInterpTimer.current < stage->facingDirInterpDuration)
            {
                stage->facingDirInterpTimer.Tick();
            }
            else
            {
                stage->facingDirInterpTimer.SetCurrent(stage->facingDirInterpDuration);
            }
            pos = stage->facingDirInterpFinal - stage->facingDirInterpInitial;
            facingDirInterpRatio = stage->facingDirInterpTimer.AsFramesFloat() / stage->facingDirInterpDuration;
            g_GameManager.stageCameraFacingDir.x = pos.x * facingDirInterpRatio + stage->facingDirInterpInitial.x;
            g_GameManager.stageCameraFacingDir.y = pos.y * facingDirInterpRatio + stage->facingDirInterpInitial.y;
            g_GameManager.stageCameraFacingDir.z = pos.z * facingDirInterpRatio + stage->facingDirInterpInitial.z;
        }
        if (stage->skyFogInterpDuration != 0)
        {
            stage->skyFogInterpTimer.Tick();
            skyFogInterpRatio = stage->skyFogInterpTimer.AsFramesFloat() / stage->skyFogInterpDuration;
            if (skyFogInterpRatio >= 1.0f)
            {
                skyFogInterpRatio = 1.0;
            }
            for (idx = 0; idx < 4; idx++)
            {
                COLOR_SET_COMPONENT(stage->skyFog.color, idx,
                                    (u8)(((f32)COLOR_GET_COMPONENT(stage->skyFogInterpFinal.color, idx) -
                                          (f32)COLOR_GET_COMPONENT(stage->skyFogInterpInitial.color, idx)) *
                                             skyFogInterpRatio +
                                         (f32)COLOR_GET_COMPONENT(stage->skyFogInterpInitial.color, idx)));
            }
            stage->skyFog.nearPlane =
                (stage->skyFogInterpFinal.nearPlane - stage->skyFogInterpInitial.nearPlane) * skyFogInterpRatio +
                stage->skyFogInterpInitial.nearPlane;
            stage->skyFog.farPlane =
                (stage->skyFogInterpFinal.farPlane - stage->skyFogInterpInitial.farPlane) * skyFogInterpRatio +
                stage->skyFogInterpInitial.farPlane;
#ifdef USE_VULKAN
            g_Renderer->SetFog(true, stage->skyFog.color, stage->skyFog.nearPlane, stage->skyFog.farPlane);
#else
            g_Supervisor.d3dDevice->SetRenderState(D3DRS_FOGCOLOR, stage->skyFog.color);
            g_Supervisor.d3dDevice->SetRenderState(D3DRS_FOGSTART, *(u32 *)&stage->skyFog.nearPlane);
            g_Supervisor.d3dDevice->SetRenderState(D3DRS_FOGEND, *(u32 *)&stage->skyFog.farPlane);
#endif
            if (stage->skyFogInterpTimer.current >= stage->skyFogInterpDuration)
            {
                stage->skyFogInterpDuration = 0;
            }
        }
        if (curInsn->opcode != STDOP_PAUSE)
        {
            stage->scriptTime.Tick();
        }
        stage->UpdateObjects();
        if (stage->spellcardState >= RUNNING)
        {
            if (stage->ticksSinceSpellcardStarted == 0)
            {
                g_AnmManager->SetAndExecuteScriptIdx(&stage->spellcardBackground,
                                                     ANM_SCRIPT_EFFECTS_SPELLCARD_BACKGROUND);
            }
            if (stage->ticksSinceSpellcardStarted == 60)
            {
                stage->spellcardState = static_cast<SpellcardState>(stage->spellcardState + 1);
            }
            stage->ticksSinceSpellcardStarted = stage->ticksSinceSpellcardStarted + 1;
            g_AnmManager->ExecuteScript(&stage->spellcardBackground);
        }
        return CHAIN_CALLBACK_RESULT_CONTINUE;
    }
}

ChainCallbackResult Stage::OnDrawHighPrio(Stage *stage)
{
#ifdef USE_VULKAN
    g_Renderer->SetFog(true, stage->skyFog.color, stage->skyFog.nearPlane, stage->skyFog.farPlane);
#else
    if (stage->skyFogNeedsSetup)
    {
        stage->skyFogNeedsSetup = 0;
        g_Supervisor.d3dDevice->SetRenderState(D3DRS_FOGCOLOR, stage->skyFog.color);
    }
    g_Supervisor.d3dDevice->SetRenderState(D3DRS_FOGSTART, *(u32 *)&stage->skyFog.nearPlane);
    g_Supervisor.d3dDevice->SetRenderState(D3DRS_FOGEND, *(u32 *)&stage->skyFog.farPlane);
#endif

    g_Supervisor.viewport.X = (DWORD)g_GameManager.arcadeRegionTopLeftPos.x;
    g_Supervisor.viewport.Y = (DWORD)g_GameManager.arcadeRegionTopLeftPos.y;
    g_Supervisor.viewport.Width = (DWORD)g_GameManager.arcadeRegionSize.x;
    g_Supervisor.viewport.Height = (DWORD)g_GameManager.arcadeRegionSize.y;
    g_Supervisor.viewport.MinZ = 0.5f;
    g_Supervisor.viewport.MaxZ = 1.0f;
    g_Supervisor.d3dDevice->SetViewport(&g_Supervisor.viewport);
    GameManager::SetupCamera(0);
    if (stage->spellcardState <= RUNNING)
    {
        if (!g_Gui.IsStageFinished())
        {
            stage->RenderObjects(0);
            stage->RenderObjects(1);
        }
    }
    return CHAIN_CALLBACK_RESULT_CONTINUE;
}

ChainCallbackResult Stage::OnDrawLowPrio(Stage *stage)
{
    f32 val;
    i32 stageToSpellcardBackgroundAlpha;
    ZunRect gameRegion;

    if (stage->spellcardState <= RUNNING)
    {
        if (!g_Gui.IsStageFinished())
        {
            g_Supervisor.d3dDevice->SetViewport(&g_Supervisor.viewport);
            stage->RenderObjects(2);
            stage->RenderObjects(3);
            if (stage->spellcardState == RUNNING)
            {
                gameRegion.left = GAME_REGION_LEFT;
                gameRegion.top = GAME_REGION_TOP;
                gameRegion.right = GAME_REGION_LEFT + GAME_REGION_WIDTH;
                gameRegion.bottom = GAME_REGION_TOP + GAME_REGION_HEIGHT;
                stageToSpellcardBackgroundAlpha = ((f32)stage->ticksSinceSpellcardStarted * 255.0f) / 60.0f;
                ScreenEffect::DrawSquare(&gameRegion, stageToSpellcardBackgroundAlpha << 24);
            }
        }
    }
    if (stage->spellcardState >= RUNNING)
    {
        g_AnmManager->Draw(&stage->spellcardBackground);
    }
    g_Supervisor.viewport.MinZ = 0.0;
    g_Supervisor.viewport.MaxZ = 0.5;
    GameManager::SetupCameraStageBackground(0);
    g_Supervisor.d3dDevice->SetViewport(&g_Supervisor.viewport);
    val = 1000.0f;
#ifdef USE_VULKAN
    g_Renderer->SetFog(true, stage->skyFog.color, 1000.0f, 2000.0f);
#else
    g_Supervisor.d3dDevice->SetRenderState(D3DRS_FOGSTART, *(u32 *)&val);
    val = 2000.0f;
    g_Supervisor.d3dDevice->SetRenderState(D3DRS_FOGEND, *(u32 *)&val);
#endif
    return CHAIN_CALLBACK_RESULT_CONTINUE;
}

ZunResult Stage::AddedCallback(Stage *stage)
{
    ZunTimer *facingDirTimer;
    ZunTimer *scriptTimer;

    D3DXVECTOR3 interpFinal;
    D3DXVECTOR3 interpInitial;

    scriptTimer = &stage->scriptTime;
    scriptTimer->InitializeForPopup();

    stage->instructionIndex = 0;
    stage->position.x = 0.0;
    stage->position.y = 0.0;
    stage->position.z = 0.0;
    stage->spellcardState = NOT_RUNNING;
    stage->skyFogInterpDuration = 0;

    if (stage->LoadStageData(g_StageFiles[g_GameManager.currentStage].anmFile,
                             g_StageFiles[g_GameManager.currentStage].stdFile) != ZUN_SUCCESS)
    {
        return ZUN_ERROR;
    }
    stage->skyFog.color = COLOR_BLACK;
    stage->skyFog.nearPlane = 200.0;
    stage->skyFog.farPlane = 500.0;
    interpFinal.x = 0;
    interpFinal.y = 0;
    interpFinal.z = 1.0;
    stage->facingDirInterpFinal = interpFinal;

    interpInitial.x = 0;
    interpInitial.y = 0;
    interpInitial.z = 1.0;
    stage->facingDirInterpInitial = interpInitial;

    stage->facingDirInterpDuration = 1;
    facingDirTimer = &stage->facingDirInterpTimer;
    facingDirTimer->InitializeForPopup();
    stage->unpauseFlag = 0;

#ifdef USE_VULKAN
    g_Renderer->SetFog(true, stage->skyFog.color, stage->skyFog.nearPlane, stage->skyFog.farPlane);
#else
    g_Supervisor.d3dDevice->SetRenderState(D3DRS_FOGCOLOR, stage->skyFog.color);
    g_Supervisor.d3dDevice->SetRenderState(D3DRS_FOGSTART, *(DWORD *)&stage->skyFog.nearPlane);
    g_Supervisor.d3dDevice->SetRenderState(D3DRS_FOGEND, *(DWORD *)&stage->skyFog.farPlane);
#endif
    return ZUN_SUCCESS;
}

ZunResult Stage::RegisterChain(u32 stage)
{

    Stage *stg = &g_Stage;
    ZunTimer *timer;

    memset(stg, 0, sizeof(Stage));
    stg->stdData = NULL;

    timer = &stg->timer;
    timer->InitializeForPopup();

    stg->stage = stage;
    g_StageCalcChain.callback = (ChainCallback)Stage::OnUpdate;
    g_StageCalcChain.addedCallback = NULL;
    g_StageCalcChain.deletedCallback = NULL;
    g_StageCalcChain.addedCallback = (ChainAddedCallback)Stage::AddedCallback;
    g_StageCalcChain.deletedCallback = (ChainDeletedCallback)Stage::DeletedCallback;
    g_StageCalcChain.arg = stg;

    if (g_Chain.AddToCalcChain(&g_StageCalcChain, TH_CHAIN_PRIO_CALC_STAGE))
    {
        return ZUN_ERROR;
    }
    g_StageOnDrawHighPrioChain.callback = (ChainCallback)OnDrawHighPrio;
    g_StageOnDrawHighPrioChain.addedCallback = NULL;
    g_StageOnDrawHighPrioChain.deletedCallback = NULL;
    g_StageOnDrawHighPrioChain.arg = stg;
    g_Chain.AddToDrawChain(&g_StageOnDrawHighPrioChain, TH_CHAIN_PRIO_DRAW_HIGH_PRIO_STAGE);
    g_StageOnDrawLowPrioChain.callback = (ChainCallback)OnDrawLowPrio;
    g_StageOnDrawLowPrioChain.addedCallback = NULL;
    g_StageOnDrawLowPrioChain.deletedCallback = NULL;
    g_StageOnDrawLowPrioChain.arg = stg;
    g_Chain.AddToDrawChain(&g_StageOnDrawLowPrioChain, TH_CHAIN_PRIO_DRAW_LOW_PRIO_STAGE);

    return ZUN_SUCCESS;
}

ZunResult Stage::DeletedCallback(Stage *s)
{
    g_AnmManager->ReleaseAnm(ANM_FILE_STAGEBG);
    if (s->quadVms != NULL)
    {
        void *quadVms = s->quadVms;
        free(quadVms);
        s->quadVms = NULL;
    }
    if (s->stdData != NULL)
    {
        void *stdData = s->stdData;
        free(stdData);
        s->stdData = NULL;
    }
    return ZUN_SUCCESS;
}

void Stage::CutChain()
{
    g_Chain.Cut(&g_StageCalcChain);
    g_Chain.Cut(&g_StageOnDrawHighPrioChain);
    g_Chain.Cut(&g_StageOnDrawLowPrioChain);
}

ZunResult Stage::LoadStageData(char *anmpath, char *stdpath)
{
    RawStageObject *curObj;
    RawStageQuadBasic *curQuad;
    i32 idx;
    i32 vmIdx;
    u32 sizeVmArr;

    if (g_AnmManager->LoadAnm(ANM_FILE_STAGEBG, anmpath, ANM_OFFSET_STAGEBG) != ZUN_SUCCESS)
    {
        return ZUN_ERROR;
    }
    this->stdData = (RawStageHeader *)FileSystem::OpenPath(stdpath, false);
    if (this->stdData == NULL)
    {
        g_GameErrorContext.Log(TH_ERR_STAGE_DATA_CORRUPTED);
        return ZUN_ERROR;
    }
    this->objectsCount = this->stdData->nbObjects;
    this->quadCount = this->stdData->nbFaces;
    this->objectInstances = (RawStageObjectInstance *)(this->stdData->facesOffset + (i32)this->stdData);
    this->beginningOfScript = (RawStageInstr *)(this->stdData->scriptOffset + (i32)this->stdData);
    this->objects = (RawStageObject **)(this->stdData + 1);
    for (idx = 0; idx < this->objectsCount; idx++)
    {
        this->objects[idx] = (RawStageObject *)((i32)this->objects[idx] + (i32)this->stdData);
    }
    sizeVmArr = this->quadCount * sizeof(AnmVm);
    this->quadVms = (AnmVm *)malloc(sizeVmArr);
    for (idx = 0, vmIdx = 0; idx < this->objectsCount; idx++)
    {
        curObj = this->objects[idx];
        curObj->flags = 1;
        curQuad = &curObj->firstQuad;
        while (curQuad->type >= 0)
        {
            g_AnmManager->ExecuteAnmIdx(&this->quadVms[vmIdx], curQuad->anmScript + ANM_OFFSET_STAGEBG);
            curQuad->vmIdx = vmIdx++;
            curQuad = (RawStageQuadBasic *)((u8 *)curQuad + curQuad->byteSize);
        }
    }
    return ZUN_SUCCESS;
}

ZunResult Stage::UpdateObjects()
{
    AnmVm *vm;
    RawStageQuadBasic *objQuad;
    RawStageQuadBasic *objQuadType1;
    i32 objIdx;
    i32 vmsNotFinished;
    RawStageObject *obj;

    for (objIdx = 0; objIdx < this->objectsCount; objIdx++)
    {
        obj = this->objects[objIdx];
        if ((obj->flags & 1) != 0)
        {
            vmsNotFinished = 0;
            objQuad = &obj->firstQuad;
            while (objQuad->type >= 0)
            {
                vm = &this->quadVms[objQuad->vmIdx];
                switch (objQuad->type)
                {
                case 0:
                    g_AnmManager->ExecuteScript(vm);
                    break;
                case 1:

                    objQuadType1 = objQuad;
                    g_AnmManager->ExecuteScript(vm);
                    break;
                }
                if (vm->currentInstruction != NULL)
                {
                    vmsNotFinished++;
                }
                objQuad = (RawStageQuadBasic *)((i32)&objQuad->type + objQuad->byteSize);
            }
            if (vmsNotFinished == 0)
            {
                obj->flags = obj->flags & ~1;
            }
        }
    }
    return ZUN_SUCCESS;
}

static bool IsStageObjectVisible(Stage *stage, RawStageObject *obj, RawStageObjectInstance *instance,
                                 const D3DXVECTOR3 &projectSrc)
{
    D3DXMATRIX worldMatrix;
    D3DXMatrixIdentity(&worldMatrix);
    D3DXVECTOR3 quadPos;

    float vpTop = g_Supervisor.viewport.Y;
    float vpBottom = g_Supervisor.viewport.Y + g_Supervisor.viewport.Height;

    auto inViewport = [&](const D3DXVECTOR3 &pos) { return pos.y >= vpTop && pos.y <= vpBottom; };

    worldMatrix.m[3][0] = obj->position.x + instance->position.x - stage->position.x;
    worldMatrix.m[3][1] = -(obj->position.y + instance->position.y - stage->position.y);
    worldMatrix.m[3][2] = obj->position.z + instance->position.z - stage->position.z + obj->size.z;
    D3DXVec3Project(&quadPos, &projectSrc, &g_Supervisor.viewport, &g_Supervisor.projectionMatrix,
                    &g_Supervisor.viewMatrix, &worldMatrix);
    if (inViewport(quadPos))
    {
        return true;
    }

    worldMatrix.m[3][1] -= obj->size.y;
    D3DXVec3Project(&quadPos, &projectSrc, &g_Supervisor.viewport, &g_Supervisor.projectionMatrix,
                    &g_Supervisor.viewMatrix, &worldMatrix);
    if (inViewport(quadPos))
    {
        return true;
    }

    worldMatrix.m[3][2] -= obj->size.z;
    D3DXVec3Project(&quadPos, &projectSrc, &g_Supervisor.viewport, &g_Supervisor.projectionMatrix,
                    &g_Supervisor.viewMatrix, &worldMatrix);
    if (inViewport(quadPos))
    {
        return true;
    }

    worldMatrix.m[3][1] += obj->size.y;
    D3DXVec3Project(&quadPos, &projectSrc, &g_Supervisor.viewport, &g_Supervisor.projectionMatrix,
                    &g_Supervisor.viewMatrix, &worldMatrix);
    if (inViewport(quadPos))
    {
        return true;
    }

    worldMatrix.m[3][0] = obj->position.x + instance->position.x - stage->position.x + obj->size.x;
    worldMatrix.m[3][1] = -(obj->position.y + instance->position.y - stage->position.y);
    worldMatrix.m[3][2] = obj->position.z + instance->position.z - stage->position.z + obj->size.z;
    D3DXVec3Project(&quadPos, &projectSrc, &g_Supervisor.viewport, &g_Supervisor.projectionMatrix,
                    &g_Supervisor.viewMatrix, &worldMatrix);
    if (inViewport(quadPos))
    {
        return true;
    }

    worldMatrix.m[3][1] -= obj->size.y;
    D3DXVec3Project(&quadPos, &projectSrc, &g_Supervisor.viewport, &g_Supervisor.projectionMatrix,
                    &g_Supervisor.viewMatrix, &worldMatrix);
    if (inViewport(quadPos))
    {
        return true;
    }

    worldMatrix.m[3][2] -= obj->size.z;
    D3DXVec3Project(&quadPos, &projectSrc, &g_Supervisor.viewport, &g_Supervisor.projectionMatrix,
                    &g_Supervisor.viewMatrix, &worldMatrix);
    if (inViewport(quadPos))
    {
        return true;
    }

    worldMatrix.m[3][1] += obj->size.y;
    D3DXVec3Project(&quadPos, &projectSrc, &g_Supervisor.viewport, &g_Supervisor.projectionMatrix,
                    &g_Supervisor.viewMatrix, &worldMatrix);
    if (inViewport(quadPos))
    {
        return true;
    }

    return false;
}

ZunResult Stage::RenderObjects(i32 zLevel)
{
    f32 quadWidth;
    D3DXVECTOR3 projectSrc;
    ZunBool didDraw;
    RawStageQuadBasic *curQuad;
    D3DXVECTOR3 quadPos;
    D3DXVECTOR3 quadScaledPos;
    RawStageObject *obj;
    D3DXMATRIX worldMatrix;
    RawStageObjectInstance *instance;
    i32 instancesDrawn;
    AnmVm *curQuadVm;

    instance = &this->objectInstances[0];
    instancesDrawn = 0;
    didDraw = 0;
    projectSrc.x = 0.0;
    projectSrc.y = 0.0;
    projectSrc.z = 0.0;
    D3DXMatrixIdentity(&worldMatrix);
    while (instance->id >= 0)
    {
        obj = this->objects[instance->id];
        if (obj->zLevel == zLevel)
        {
            if (IsStageObjectVisible(this, obj, instance, projectSrc))
            {
                curQuad = &obj->firstQuad;
                didDraw = 1;
                while (curQuad->type >= 0)
                {
                    curQuadVm = this->quadVms + curQuad->vmIdx;
                    switch (curQuad->type)
                    {
                    case 0:
                        curQuadVm->pos.x = curQuad->position.x + instance->position.x - this->position.x;
                        curQuadVm->pos.y = curQuad->position.y + instance->position.y - this->position.y;
                        curQuadVm->pos.z = curQuad->position.z + instance->position.z - this->position.z;
                        if (curQuad->size.x != 0.0f)
                        {
                            curQuadVm->scaleX = curQuad->size.x / curQuadVm->sprite->widthPx;
                        }
                        if (curQuad->size.y != 0.0f)
                        {
                            curQuadVm->scaleY = curQuad->size.y / curQuadVm->sprite->heightPx;
                        }
                        if (curQuadVm->autoRotate == 2)
                        {
                            if (curQuad->size.x != 0.0f)
                            {
                                quadWidth = curQuad->size.x;
                            }
                            else
                            {
                                quadWidth = curQuadVm->sprite->widthPx;
                            }
                            worldMatrix.m[3][0] = curQuadVm->pos.x;
                            worldMatrix.m[3][1] = -curQuadVm->pos.y;
                            worldMatrix.m[3][2] = curQuadVm->pos.z;
                            D3DXVec3Project(&quadPos, &projectSrc, &g_Supervisor.viewport,
                                            &g_Supervisor.projectionMatrix, &g_Supervisor.viewMatrix, &worldMatrix);
                            worldMatrix.m[3][0] = quadWidth * curQuadVm->scaleX + worldMatrix.m[3][0];
                            D3DXVec3Project(&quadScaledPos, &projectSrc, &g_Supervisor.viewport,
                                            &g_Supervisor.projectionMatrix, &g_Supervisor.viewMatrix, &worldMatrix);
                            curQuadVm->scaleX = (quadScaledPos.x - quadPos.x) / quadWidth;
                            curQuadVm->scaleY = curQuadVm->scaleX;
                            curQuadVm->pos = quadPos;
                            g_AnmManager->DrawFacingCamera(curQuadVm);
                        }
                        else
                        {
                            g_AnmManager->Draw3(curQuadVm);
                        }
                        break;
                    }
                    curQuad = (RawStageQuadBasic *)((i32)curQuad + curQuad->byteSize);
                }
                instancesDrawn++;
            }
        }
        instance++;
    }
    return ZUN_SUCCESS;
}
}; // namespace th06
