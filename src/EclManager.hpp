#pragma once

#include "ItemManager.hpp"
#include "SoundPlayer.hpp"
#include "ZunBool.hpp"
#include "ZunColor.hpp"
#include "ZunMath.hpp"
#include "ZunResult.hpp"
#include "diffbuild.hpp"
#include "inttypes.hpp"
#include <Windows.h>
#include <d3dx8math.h>

namespace th06
{

struct Enemy;
struct EnemyEclContext;
struct EnemyManager;

enum EclVarId
{
    ECL_VAR_I32_0 = -10001,
    ECL_VAR_I32_1 = -10002,
    ECL_VAR_I32_2 = -10003,
    ECL_VAR_I32_3 = -10004,
    ECL_VAR_F32_0 = -10005,
    ECL_VAR_F32_1 = -10006,
    ECL_VAR_F32_2 = -10007,
    ECL_VAR_F32_3 = -10008,
    ECL_VAR_I32_4 = -10009,
    ECL_VAR_I32_5 = -10010,
    ECL_VAR_I32_6 = -10011,
    ECL_VAR_I32_7 = -10012,
    ECL_VAR_DIFFICULTY = -10013,
    ECL_VAR_RANK = -10014,
    ECL_VAR_ENEMY_POS_X = -10015,
    ECL_VAR_ENEMY_POS_Y = -10016,
    ECL_VAR_ENEMY_POS_Z = -10017,
    ECL_VAR_PLAYER_POS_X = -10018,
    ECL_VAR_PLAYER_POS_Y = -10019,
    ECL_VAR_PLAYER_POS_Z = -10020,
    ECL_VAR_PLAYER_ANGLE = -10021,
    ECL_VAR_ENEMY_TIMER = -10022,
    ECL_VAR_PLAYER_DISTANCE = -10023,
    ECL_VAR_ENEMY_LIFE = -10024,
    ECL_VAR_PLAYER_SHOT = -10025,
};

struct EclTimelineInstrArgs
{
    u32 uintVar1;
    u32 uintVar2;
    u32 uintVar3;
    u16 ushortVar1;
    u16 ushortVar2;
    u32 uintVar4;

    D3DXVECTOR3 *Var1AsVec()
    {
        return (D3DXVECTOR3 *)&this->uintVar1;
    }
};

struct EclTimelineInstr
{
    i16 time;
    i16 arg0;
    i16 opCode;
    i16 size;
    EclTimelineInstrArgs args;
};

union EclRawInstrArg {
    struct
    {
        i8 a;
        i8 b;
        i8 c;
        i8 d;
    } by;
    struct
    {
        i16 lo;
        i16 hi;
    } sh;
    f32 f32;
    i32 i32;
    EclVarId id;
};

struct EclRawInstrAluArgs
{
    EclVarId res;
    EclRawInstrArg arg1;
    EclRawInstrArg arg2;
    EclRawInstrArg arg3;
    EclRawInstrArg arg4;
};

struct EclRawInstrJumpArgs
{
    i32 time;
    i32 offset;
    EclVarId var;
};

struct EclRawInstrCallArgs
{
    i32 eclSub;
    i32 var0;
    f32 float0;
    EclVarId cmpLhs;
    i32 cmpRhs;
};

struct EclRawInstrCmpArgs
{
    EclRawInstrArg lhs;
    EclRawInstrArg rhs;
};

struct EclRawInstrMoveArgs
{
    ZunVec3 pos;
};

struct EclRawInstrAnmSetMainArgs
{
    i32 scriptIdx;
};

struct EclRawInstrAnmSetSlotArgs
{
    i32 vmIdx;
    i32 scriptIdx;
};

struct EclRawInstrAnmSetDeathArgs
{
    i8 deathAnm1;
    i8 deathAnm2;
    i8 deathAnm3;
};

struct EclRawInstrBulletArgs
{
    i16 sprite;
    i16 color;
    EclVarId count1;
    EclVarId count2;
    f32 speed1;
    f32 speed2;
    f32 angle1;
    f32 angle2;
    i32 flags;
};

struct EclRawInstrLaserArgs
{
    i16 sprite;
    i16 color;
    f32 angle;
    f32 speed;
    f32 startOffset;
    f32 endOffset;
    f32 startLength;
    f32 width;
    i32 startTime;
    i32 duration;
    i32 despawnDuration;
    i32 hitboxStartTime;
    i32 hitboxEndDelay;
    i32 flags;
};

struct EclRawInstrLaserOpArgs
{
    i32 laserIdx;
    ZunVec3 arg1;
};

struct EclRawInstrBulletEffectsArgs
{
    EclVarId ivar1;
    EclVarId ivar2;
    EclVarId ivar3;
    EclVarId ivar4;
    f32 fvar1;
    f32 fvar2;
    f32 fvar3;
    f32 fvar4;
};

struct EclRawInstrSetInt
{
    i32 i32;
};

struct EclRawInstrSpellcardEffectArgs
{
    i32 effectColorId;
    ZunVec3 pos;
    f32 effectDistance;
};

struct EclRawInstrMoveBoundSetArgs
{
    ZunVec2 lowerMoveLimit;
    ZunVec2 upperMoveLimit;
};

struct EclRawInstrAnmSetPosesArgs
{
    i16 anmExDefault;
    i16 anmExFarLeft;
    i16 anmExFarRight;
    i16 anmExLeft;
    i16 anmExRight;
};

struct EclRawInstrSetInterruptArgs
{
    i32 interruptSub;
    i32 interruptId;
};

struct EclRawInstrSpellcardStartArgs
{
    i16 spellcardSprite;
    i16 spellcardId;
    char spellcardName[1];
};

struct EclRawInstrEffectParticleArgs
{
    i32 effectId;
    i32 numParticles;
    ZunColor particleColor;
};

struct EclRawInstrTimeSetArgs
{
    EclVarId timeToSet;
};

struct EclRawInstrDropItemArgs
{
    ItemType itemId;
};

struct EclRawInstrEnemyCreateArgs
{
    i32 subId;
    ZunVec3 pos;
    i16 life;
    i16 itemDrop;
    i32 score;
};
ZUN_ASSERT_SIZE(EclRawInstrEnemyCreateArgs, 0x18);

struct EclRawInstrAnmInterruptSlotArgs
{
    i32 vmId;
    i32 interruptId;
};

struct EclRawInstrBulletSoundArgs
{
    SoundIdx bulletSfx;
};

struct EclRawInstrBulletRankInfluenceArgs
{
    f32 bulletRankSpeedLow;
    f32 bulletRankSpeedHigh;
    i32 bulletRankAmount1Low;
    i32 bulletRankAmount1High;
    i32 bulletRankAmount2Low;
    i32 bulletRankAmount2High;
};

struct EclRawInstrExInstrArgs
{
    u32 exInstrIndex;
    union {
        i32 i32Param;
        u8 u8Param;
    };
};

union EclRawInstrArgs {
    EclRawInstrAluArgs alu;
    EclRawInstrCmpArgs cmp;
    EclRawInstrJumpArgs jump;
    EclRawInstrCallArgs call;
    EclRawInstrAnmSetMainArgs anmSetMain;
    EclRawInstrAnmSetPosesArgs anmSetPoses;
    EclRawInstrAnmSetSlotArgs anmSetSlot;
    EclRawInstrAnmSetDeathArgs anmSetDeath;
    EclRawInstrMoveArgs move;
    EclRawInstrBulletArgs bullet;
    EclRawInstrLaserArgs laser;
    EclRawInstrLaserOpArgs laserOp;
    EclRawInstrBulletEffectsArgs bulletEffects;
    EclRawInstrSpellcardEffectArgs spellcardEffect;
    EclRawInstrMoveBoundSetArgs moveBoundSet;
    EclRawInstrSetInterruptArgs setInterrupt;
    EclRawInstrSpellcardStartArgs spellcardStart;
    EclRawInstrEffectParticleArgs effectParticle;
    EclRawInstrTimeSetArgs timeSet;
    EclRawInstrDropItemArgs dropItem;
    EclRawInstrEnemyCreateArgs enemyCreate;
    EclRawInstrAnmInterruptSlotArgs anmInterruptSlot;
    EclRawInstrBulletSoundArgs bulletSound;
    EclRawInstrBulletRankInfluenceArgs bulletRankInfluence;
    EclRawInstrExInstrArgs exInstr;
    i32 setInt;

    i32 GetBossLifeCount()
    {
        return this->setInt;
    }
};

struct EclRawInstr
{
    i32 time;
    i16 opCode;
    i16 offsetToNext;
    u8 unk_8;

    u8 skipForDifficulty;
    u8 unk_a;
    u8 unk_b;
    EclRawInstrArgs args;
};

struct EclRawHeader
{
    i16 subCount;
    i16 mainCount;
    EclTimelineInstr *timelineOffsets[3];
    EclRawInstr *subOffsets[0];
};
ZUN_ASSERT_SIZE(EclRawHeader, 0x10);

enum EclRawInstrOpcode
{
    ECL_OPCODE_NOP,
    ECL_OPCODE_UNIMP,
    ECL_OPCODE_JUMP,
    ECL_OPCODE_JUMPDEC,
    ECL_OPCODE_SETINT,
    ECL_OPCODE_SETFLOAT,
    ECL_OPCODE_SETINTRAND,
    ECL_OPCODE_SETINTRANDMIN,
    ECL_OPCODE_SETFLOATRAND,
    ECL_OPCODE_SETFLOATRANDMIN,
    ECL_OPCODE_SETVARSELFX,
    ECL_OPCODE_SETVARSELFY,
    ECL_OPCODE_SETVARSELFZ,
    ECL_OPCODE_MATHINTADD,
    ECL_OPCODE_MATHINTSUB,
    ECL_OPCODE_MATHINTMUL,
    ECL_OPCODE_MATHINTDIV,
    ECL_OPCODE_MATHINTMOD,
    ECL_OPCODE_MATHINC,
    ECL_OPCODE_MATHDEC,
    ECL_OPCODE_MATHFLOATADD,
    ECL_OPCODE_MATHFLOATSUB,
    ECL_OPCODE_MATHFLOATMUL,
    ECL_OPCODE_MATHFLOATDIV,
    ECL_OPCODE_MATHFLOATMOD,
    ECL_OPCODE_MATHATAN2,
    ECL_OPCODE_MATHNORMANGLE,
    ECL_OPCODE_CMPINT,
    ECL_OPCODE_CMPFLOAT,
    ECL_OPCODE_JUMPLSS,
    ECL_OPCODE_JUMPLEQ,
    ECL_OPCODE_JUMPEQU,
    ECL_OPCODE_JUMPGRE,
    ECL_OPCODE_JUMPGEQ,
    ECL_OPCODE_JUMPNEQ,
    ECL_OPCODE_CALL,
    ECL_OPCODE_RET,
    ECL_OPCODE_CALLLSS,
    ECL_OPCODE_CALLLEQ,
    ECL_OPCODE_CALLEQU,
    ECL_OPCODE_CALLGRE,
    ECL_OPCODE_CALLGEQ,
    ECL_OPCODE_CALLNEQ,
    ECL_OPCODE_MOVEPOSITION,
    ECL_OPCODE_MOVEAXISVELOCITY,
    ECL_OPCODE_MOVEVELOCITY,
    ECL_OPCODE_MOVEANGULARVELOCITY,
    ECL_OPCODE_MOVESPEED,
    ECL_OPCODE_MOVEACCELERATION,
    ECL_OPCODE_MOVERAND,
    ECL_OPCODE_MOVERANDINBOUND,
    ECL_OPCODE_MOVEATPLAYER,
    ECL_OPCODE_MOVEDIRTIMEDECELERATE,
    ECL_OPCODE_MOVEDIRTIMEDECELERATEFAST,
    ECL_OPCODE_MOVEDIRTIMEACCELERATE,
    ECL_OPCODE_MOVEDIRTIMEACCELERATEFAST,
    ECL_OPCODE_MOVEPOSITIONTIMELINEAR,
    ECL_OPCODE_MOVEPOSITIONTIMEDECELERATE,
    ECL_OPCODE_MOVEPOSITIONTIMEDECELERATEFAST,
    ECL_OPCODE_MOVEPOSITIONTIMEACCELERATE,
    ECL_OPCODE_MOVEPOSITIONTIMEACCELERATEFAST,
    ECL_OPCODE_MOVETIMEDECELERATE,
    ECL_OPCODE_MOVETIMEDECELERATEFAST,
    ECL_OPCODE_MOVETIMEACCELERATE,
    ECL_OPCODE_MOVETIMEACCELERATEFAST,
    ECL_OPCODE_MOVEBOUNDSSET,
    ECL_OPCODE_MOVEBOUNDSDISABLE,
    ECL_OPCODE_BULLETFANAIMED,
    ECL_OPCODE_BULLETFAN,
    ECL_OPCODE_BULLETCIRCLEAIMED,
    ECL_OPCODE_BULLETCIRCLE,
    ECL_OPCODE_BULLETOFFSETCIRCLEAIMED,
    ECL_OPCODE_BULLETOFFSETCIRCLE,
    ECL_OPCODE_BULLETRANDOMANGLE,
    ECL_OPCODE_BULLETRANDOMSPEED,
    ECL_OPCODE_BULLETRANDOM,
    ECL_OPCODE_SHOOTINTERVAL,
    ECL_OPCODE_SHOOTINTERVALDELAYED,
    ECL_OPCODE_SHOOTDISABLED,
    ECL_OPCODE_SHOOTENABLED,
    ECL_OPCODE_SHOOTNOW,
    ECL_OPCODE_SHOOTOFFSET,
    ECL_OPCODE_BULLETEFFECTS,
    ECL_OPCODE_BULLETCANCEL,
    ECL_OPCODE_BULLETSOUND,
    ECL_OPCODE_LASERCREATE,
    ECL_OPCODE_LASERCREATEAIMED,
    ECL_OPCODE_LASERINDEX,
    ECL_OPCODE_LASERROTATE,
    ECL_OPCODE_LASERROTATEFROMPLAYER,
    ECL_OPCODE_LASEROFFSET,
    ECL_OPCODE_LASERTEST,
    ECL_OPCODE_LASERCANCEL,
    ECL_OPCODE_SPELLCARDSTART,
    ECL_OPCODE_SPELLCARDEND,
    ECL_OPCODE_ENEMYCREATE,
    ECL_OPCODE_ENEMYKILLALL,
    ECL_OPCODE_ANMSETMAIN,
    ECL_OPCODE_ANMSETPOSES,
    ECL_OPCODE_ANMSETSLOT,
    ECL_OPCODE_ANMSETDEATH,
    ECL_OPCODE_ANM_DEATH_EFFECTS = ECL_OPCODE_ANMSETDEATH,
    ECL_OPCODE_BOSSSET,
    ECL_OPCODE_SPELLCARDEFFECT,
    ECL_OPCODE_ENEMYSETHITBOX,
    ECL_OPCODE_ENEMYFLAGCOLLISION,
    ECL_OPCODE_ENEMY_FLAG_COLLISION = ECL_OPCODE_ENEMYFLAGCOLLISION,
    ECL_OPCODE_ENEMYFLAGCANTAKEDAMAGE,
    ECL_OPCODE_ENEMY_FLAG_CAN_TAKE_DAMAGE = ECL_OPCODE_ENEMYFLAGCANTAKEDAMAGE,
    ECL_OPCODE_EFFECTSOUND,
    ECL_OPCODE_ENEMYFLAGDEATH,
    ECL_OPCODE_ENEMY_FLAGS_DEATH = ECL_OPCODE_ENEMYFLAGDEATH,
    ECL_OPCODE_DEATHCALLBACKSUB,
    ECL_OPCODE_ENEMYINTERRUPTSET,
    ECL_OPCODE_ENEMYINTERRUPT,
    ECL_OPCODE_ENEMYLIFESET,
    ECL_OPCODE_BOSSTIMERSET,
    ECL_OPCODE_PHASE_TIMER_SET = ECL_OPCODE_BOSSTIMERSET,
    ECL_OPCODE_LIFECALLBACKTHRESHOLD,
    ECL_OPCODE_LIFECALLBACKSUB,
    ECL_OPCODE_TIMERCALLBACKTHRESHOLD,
    ECL_OPCODE_TIMERCALLBACKSUB,
    ECL_OPCODE_ENEMYFLAGINTERACTABLE,
    ECL_OPCODE_ENEMY_FLAG_INTERACTABLE = ECL_OPCODE_ENEMYFLAGINTERACTABLE,
    ECL_OPCODE_EFFECTPARTICLE,
    ECL_OPCODE_DROPITEMS,
    ECL_OPCODE_ANMFLAGROTATION,
    ECL_OPCODE_ANM_FLAG_ROTATION = ECL_OPCODE_ANMFLAGROTATION,
    ECL_OPCODE_EXINSCALL,
    ECL_OPCODE_EXINSREPEAT,
    ECL_OPCODE_TIMESET,
    ECL_OPCODE_ECL_TIME_ADD = ECL_OPCODE_TIMESET,
    ECL_OPCODE_DROPITEMID,
    ECL_OPCODE_STDUNPAUSE,
    ECL_OPCODE_BOSSSETLIFECOUNT,
    ECL_OPCODE_DEBUGWATCH,
    ECL_OPCODE_ANMINTERRUPTMAIN,
    ECL_OPCODE_ANMINTERRUPTSLOT,
    ECL_OPCODE_ENEMYFLAGDISABLECALLSTACK,
    ECL_OPCODE_ENEMY_FLAG_DISABLE_CALLSTACK = ECL_OPCODE_ENEMYFLAGDISABLECALLSTACK,
    ECL_OPCODE_BULLETRANKINFLUENCE,
    ECL_OPCODE_ENEMYFLAGINVISIBLE,
    ECL_OPCODE_ENEMY_FLAG_INVISIBLE = ECL_OPCODE_ENEMYFLAGINVISIBLE,
    ECL_OPCODE_BOSSTIMERCLEAR,
    ECL_OPCODE_PHASE_TIMER_CLEAR = ECL_OPCODE_BOSSTIMERCLEAR,
    ECL_OPCODE_LASERCLEARALL,
    ECL_OPCODE_SPELLCARDFLAGTIMEOUT,
    ECL_OPCODE_SPELLCARD_FLAG_TIMEOUT = ECL_OPCODE_SPELLCARDFLAGTIMEOUT,
};

enum TimelineOpcode
{
    TIMELINE_OPCODE_ENEMY_CREATE,
    TIMELINE_OPCODE_DUMMY_CREATE,
    TIMELINE_OPCODE_ENEMY_CREATE_MIRROR,
    TIMELINE_OPCODE_DUMMY_CREATE_MIRROR,
    TIMELINE_OPCODE_ENEMY_CREATE_RANDOM,
    TIMELINE_OPCODE_DUMMY_CREATE_RANDOM,
    TIMELINE_OPCODE_ENEMY_CREATE_MIRROR_RANDOM,
    TIMELINE_OPCODE_DUMMY_CREATE_MIRROR_RANDOM,
    TIMELINE_OPCODE_MSG_READ,
    TIMELINE_OPCODE_MSG_WAIT,
    TIMELINE_OPCODE_BOSS_INTERRUPT,
    TIMELINE_OPCODE_PLAYER_POWER,
    TIMELINE_OPCODE_BOSS_WAIT,
};

struct EclManagerExtraData
{
    u8 unk_0[0x80];
    f32 starAngleTable[6];
    u8 unk_98[0x68];
    D3DXVECTOR3 coords[8];
};
ZUN_ASSERT_SIZE(EclManagerExtraData, 0x160);

typedef EclTimelineInstr TimelineInstr;

struct EclManager
{
    ZunResult Load(char *ecl);
    void Unload();
    ZunResult RunEcl(Enemy *enemy);
    ZunResult CallEclSub(EnemyEclContext *enemyEcl, i16 subId);

    EclRawHeader *eclFile;
    EclRawInstr **subTable;
    EclTimelineInstr *timeline;
    EclManagerExtraData extra;
};
ZUN_ASSERT_SIZE(EclManager, 0x16c);

extern EclManager g_EclManager;
}; // namespace th06
