#pragma once

#include <d3d8.h>
#include <d3dx8math.h>

#include "ZunColor.hpp"
#include "ZunMath.hpp"
#include "ZunResult.hpp"
#include "ZunTimer.hpp"
#include "diffbuild.hpp"
#include "inttypes.hpp"

namespace th06
{
struct AnmLoadedSprite
{
    i32 sourceFileIndex;
    ZunVec2 startPixelInclusive;
    ZunVec2 endPixelInclusive;
    f32 textureHeight;
    f32 textureWidth;
    ZunVec2 uvStart;
    ZunVec2 uvEnd;
    f32 heightPx;
    f32 widthPx;
    i32 spriteId;
};
ZUN_ASSERT_SIZE(AnmLoadedSprite, 0x38);

#define AnmOpcode_Exit 0
#define AnmOpcode_SetActiveSprite 1
#define AnmOpcode_SetScale 2
#define AnmOpcode_SetAlpha 3
#define AnmOpcode_SetColor 4
#define AnmOpcode_Jump 5
#define AnmOpcode_Nop 6
#define AnmOpcode_FlipX 7
#define AnmOpcode_FlipY 8
#define AnmOpcode_SetRotation 9
#define AnmOpcode_SetAngleVel 10
#define AnmOpcode_SetScaleSpeed 11
#define AnmOpcode_Fade 12
#define AnmOpcode_SetBlendAdditive 13
#define AnmOpcode_SetBlendDefault 14
#define AnmOpcode_ExitHide 15
#define AnmOpcode_SetRandomSprite 16
#define AnmOpcode_SetPosition 17
#define AnmOpcode_PosTimeLinear 18
#define AnmOpcode_PosTimeDecel 19
#define AnmOpcode_PosTimeAccel 20
#define AnmOpcode_Stop 21
#define AnmOpcode_InterruptLabel 22
#define AnmOpcode_AnchorTopLeft 23
#define AnmOpcode_StopHide 24
#define AnmOpcode_UsePosOffset 25
#define AnmOpcode_SetAutoRotate 26
#define AnmOpcode_UVScrollX 27
#define AnmOpcode_UVScrollY 28
#define AnmOpcode_SetVisibility 29
#define AnmOpcode_ScaleTime 30
#define AnmOpcode_SetZWriteDisable 31

enum AnmOpcode
{
    ANM_OPCODE_ANM_DELETE,
    ANM_OPCODE_SET_SPRITE,
    ANM_OPCODE_SCALE,
    ANM_OPCODE_ALPHA,
    ANM_OPCODE_COLOR,
    ANM_OPCODE_JUMP,
    ANM_OPCODE_NOP,
    ANM_OPCODE_SCALE_FLIP_X,
    ANM_OPCODE_SCALE_FLIP_Y,
    ANM_OPCODE_ROTATION,
    ANM_OPCODE_ROTATION_SPEED,
    ANM_OPCODE_SCALE_SPEED,
    ANM_OPCODE_ALPHA_INTERP_LINEAR,
    ANM_OPCODE_BLEND_MODE_ADDITIVE,
    ANM_OPCODE_BLEND_MODE_NORMAL,
    ANM_OPCODE_ANM_STATIC,
    ANM_OPCODE_SPRITE_SET_RAND,
    ANM_OPCODE_MOVE_POSITION,
    ANM_OPCODE_MOVE_POSITION_INTERP_LINEAR,
    ANM_OPCODE_MOVE_POSITION_INTERP_DECELERATE_SLOW,
    ANM_OPCODE_MOVE_POSITION_INTERP_ACCELERATE_SLOW,
    ANM_OPCODE_ANM_HALT,
    ANM_OPCODE_INTERRUPT_LABEL,
    ANM_OPCODE_ANCHOR_TOP_LEFT,
    ANM_OPCODE_ANM_HALT_INVISIBLE,
    ANM_OPCODE_POSITION_MODE,
    ANM_OPCODE_SET_AUTO_ROTATE,
    ANM_OPCODE_SCROLL_SET_X,
    ANM_OPCODE_SCROLL_SET_Y,
    ANM_OPCODE_ANM_FLAG_VISIBLE,
    ANM_OPCODE_SCALE_INTERP_LINEAR,
    ANM_OPCODE_FLAG_DISABLE_Z_WRITE
};

struct AnmRawInstr
{
    i16 time;
    u8 opcode;
    u8 argsCount;
    u32 args[10];
};

enum AnmVmFlagsEnum
{
    AnmVmFlags_Visible = 1 << 0,
    AnmVmFlags_1 = 1 << 1,
    AnmVmFlags_VisibleOverride = 1 << 1,
    AnmVmFlags_BlendMode = 1 << 2,
    AnmVmFlags_ColorOp = 1 << 3,
    AnmVmFlags_4 = 1 << 4,
    AnmVmFlags_UsePosOffset = 1 << 5,
    AnmVmFlags_FlipX = 1 << 6,
    AnmVmFlags_FlipY = 1 << 7,
    AnmVmFlags_AnchorLeft = 1 << 8,
    AnmVmFlags_AnchorTop = 1 << 9,
    AnmVmFlags_ZWriteDisable = 1 << 12,
    AnmVmFlags_IsStopped = 1 << 13,
};

#define ANM_VM_INITIAL_FLAGS 0x3

enum AnmVmBlendMode
{
    AnmVmBlendMode_InvSrcAlpha,
    AnmVmBlendMode_One,
};

enum AnmBlendMode
{
    AnmBlendMode_NotSet = -1,
    AnmBlendMode_Normal,
    AnmBlendMode_Additive,
};

enum AnmVmColorOp
{
    AnmVmColorOp_Modulate,
    AnmVmColorOp_Add,
};

enum AnmColorOp
{
    AnmColorOp_NotSet = -1,
    AnmColorOp_Modulate,
    AnmColorOp_Add,
};

enum AnmZWriteState
{
    AnmZWriteState_NotSet = -1,
    AnmZWriteState_On = false,
    AnmZWriteState_Off = true,
};

enum AnmVmMirror
{
    AnmVmMirror_None,
    AnmVmMirror_X,
    AnmVmMirror_Y
};

enum AnmVmAnchor
{
    AnmVmAnchor_Center,
    AnmVmAnchor_Left,
    AnmVmAnchor_Top,
    AnmVmAnchor_TopLeft,
};

enum AnmVmInterpMode
{
    AnmVmInterp_Linear,
    AnmVmInterp_DecelerateSlow,
    AnmVmInterp_AccelerateSlow,
};

enum AnmVertexShader
{
    AnmVertexShader_NotSet = -1,
    AnmVertexShader_0,
    AnmVertexShader_1,
    AnmVertexShader_2,
    AnmVertexShader_3,
};

union AnmVmFlags {
    u16 flags;
    struct
    {
        u32 isVisible : 1;
        u32 isVisibleOverride : 1;
        u32 blendMode : 1;
        u32 colorOp : 1;
        u32 flag4 : 1;
        u32 usePosOffset : 1;
        u32 flip : 2;
        u32 anchor : 2;
        u32 posTime : 2;
        u32 zWriteDisable : 1;
        u32 isStopped : 1;
    };
    struct
    {
        u32 _compat_isVisible : 1;
        u32 flag1 : 1;
        u32 _compat_blendMode : 1;
        u32 _compat_colorOp : 1;
        u32 _compat_flag4 : 1;
        u32 _compat_usePosOffset : 1;
        u32 _compat_flip : 2;
        u32 _compat_anchor : 2;
        u32 moveInterpMode : 2;
        u32 _compat_zWriteDisable : 1;
        u32 _compat_isStopped : 1;
    };
};

struct AnmVm
{
    void Initialize()
    {
        this->uvScrollPos.y = 0.0;
        this->uvScrollPos.x = 0.0;
        this->scaleInterpFinalX = 0.0;
        this->scaleInterpFinalY = 0.0;
        this->angleVel.z = 0.0;
        this->angleVel.y = 0.0;
        this->angleVel.x = 0.0;
        this->rotation.z = 0.0;
        this->rotation.y = 0.0;
        this->rotation.x = 0.0;
        this->scaleX = 1.0;
        this->scaleY = 1.0;
        this->scaleInterpEndTime = 0;
        this->alphaInterpEndTime = 0;
        this->color = D3DCOLOR_RGBA(0xff, 0xff, 0xff, 0xff);
        D3DXMatrixIdentity(&this->matrix);
        this->flags.flags = AnmVmFlags_Visible | AnmVmFlags_1;
        this->autoRotate = 0;
        this->pendingInterrupt = 0;
        this->posInterpEndTime = 0;
        this->currentTimeInScript.Initialize();
    }

    AnmVm()
    {
        this->activeSpriteIndex = -1;
    }

    void SetInvisible()
    {
        this->flags.isVisible = 0;
    }

    D3DXVECTOR3 rotation;
    D3DXVECTOR3 angleVel;
    f32 scaleY;
    f32 scaleX;
    f32 scaleInterpFinalY;
    f32 scaleInterpFinalX;
    D3DXVECTOR2 uvScrollPos;
    ZunTimer currentTimeInScript;
    D3DXMATRIX matrix;
    ZunColor color;
    AnmVmFlags flags;

    i16 alphaInterpEndTime;
    i16 scaleInterpEndTime;
    i16 autoRotate;
    i16 pendingInterrupt;
    i16 posInterpEndTime;

    D3DXVECTOR3 pos;
    f32 scaleInterpInitialY;
    f32 scaleInterpInitialX;
    ZunTimer scaleInterpTime;
    i16 activeSpriteIndex;
    i16 baseSpriteIndex;
    i16 anmFileIndex;

    AnmRawInstr *beginingOfScript;
    AnmRawInstr *currentInstruction;
    AnmLoadedSprite *sprite;
    D3DCOLOR alphaInterpInitial;
    D3DCOLOR alphaInterpFinal;
    D3DXVECTOR3 posInterpInitial;
    D3DXVECTOR3 posInterpFinal;
    D3DXVECTOR3 posOffset;
    ZunTimer posInterpTime;
    i32 timeOfLastSpriteSet;
    ZunTimer alphaInterpTime;
    u8 fontWidth;
    u8 fontHeight;
};
ZUN_ASSERT_SIZE(AnmVm, 0x110);
}; // namespace th06
