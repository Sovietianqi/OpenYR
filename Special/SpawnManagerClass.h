#pragma once

#include "../Abstract/AbstractClass.h"
#include "../Containers/DynamicVectorClass.h"
#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"

class TechnoClass;
class HouseClass;
class AircraftTypeClass;

// ============================================================================
// SpawnManagerClass —— 发射载具的子机管理器（航母 / 驱逐舰 / 无畏级 / V3）
//
// asm 权威来源：/workspace/asm_export/SpawnManagerClass.asm.txt
//   sizeof = 0x74（SpawnManagerClass_GetSize 返回 74h）
//   四段 vtable：SpawnManagerClass / IRTTITypeInfo / INoticeSink / INoticeSource
//   继承 AbstractClass（+0x00..+0x23）
//
//   +0x24  Owner            母舰（TechnoClass*）
//   +0x28  SpawnType        子机机型（AircraftTypeClass*），取自 type->Spawns
//   +0x2C  MaxSpawns        子机数量上限（type->SpawnsNumber）
//   +0x30  RegenRate        子机再生节拍（type->SpawnRegenRate）
//   +0x34  ReloadRate       子机整备节拍（type->SpawnReloadRate）
//   +0x38  SpawnedNodes     DynamicVectorClass<SpawnControl*>（节点表）
//   +0x50  UpdateTimer      主 AI 节拍（TimeStart / field_4 / TimeLeft）
//   +0x5C  SpawningTimer    连发节拍（同一管理器复用）
//   +0x68  Destination      派发目标（AbstractClass*）
//   +0x6C  Target           待接入目标（AbstractClass*）
//   +0x70  SomeTimestamp    节点机群状态机 0=归位 1=待放飞 2=放飞中
//
// 每个 SpawnControl 节点 0x18 字节：
//   +0x00  Unit             当前停靠子机
//   +0x04  Status           0=空位 1=阵亡回收 2=待命 3=出击 4=归航 6=整备 7=补机
//   +0x08  SpawnTimer       { Start, field_4, Left }
//   +0x14  IsSpawnMissile   是否为导弹型子机（V3/DMisl/CMisl）
// ============================================================================
class SpawnManagerClass : public AbstractClass {
public:
    static const AbstractType AbsID = AbstractType::SpawnManager;

    // 三字计时器，与 asm 的 TimerA / SpawningTimer 字段一一对应。
    struct Timer {
        int32 TimeStart;   // +0x00 起始帧（-1 表示未启用）
        int32 field_4;     // +0x04 asm 未具名
        int32 TimeLeft;    // +0x08 固定时长（不减）
    };

    // 出生点节点，asm: DynamicVectorClass<SpawnManagerClass::SpawnControl *>
    struct SpawnControl {
        TechnoClass* Unit;            // +0x00
        int32        Status;          // +0x04
        Timer        SpawnTimer;      // +0x08 (Start/field_4/Left)
        bool         IsSpawnMissile;  // +0x14
    };

    // 节点状态码（asm 跳转表 states[] 的语义）
    enum NodeStatus : int32 {
        Node_Empty     = 0,   // 空位，等待放飞
        Node_Dead      = 1,   // 子机阵亡，回收该节点
        Node_Holding   = 2,   // 待命
        Node_Attacking = 3,   // 出击攻击
        Node_Returning = 4,   // 归航
        Node_Docked    = 6,   // 归舰整备
        Node_Respawn   = 7,   // 补机
    };

    // asm: SpawnManagerClass_CTOR (asm:25) ; retn 14h = 5 参数
    SpawnManagerClass(TechnoClass* pOwner, AircraftTypeClass* pSpawn,
                      int32 num, int32 regenRate, int32 reloadRate) noexcept;
    explicit SpawnManagerClass(noinit_t) noexcept : AbstractClass(noinit) {}
    virtual ~SpawnManagerClass();

    virtual AbstractType WhatAmI() const override { return AbstractType::SpawnManager; }
    virtual int32 Size() const override { return 0x74; }

    virtual HRESULT GetClassID(CLSID* pClassID) override;
    virtual HRESULT Load(IStream* pStm) override;
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override;
    virtual void LoadTables(IStream* pStm) override;
    virtual void ComputeCRC(CRCEngine& crc) const override;

    // asm: SpawnManagerClass_Update (asm:581) —— 逐帧维护子机队列 / 放飞 / 回收
    virtual void Update() override;

    // asm: SpawnManagerClass_PointerGotInvalid (asm:1576) ; retn 4（单参 void*）
    virtual void PointerGotInvalid(AbstractClass* pInvalid, bool removed) override;

    // asm: SpawnManagerClass_SetTarget (asm:1465) ; retn 4
    void SetTarget(AbstractClass* pTarget);

    // asm: SpawnManagerClass_CreateNode (asm:1499)
    void CreateNode();
    // asm: SpawnManagerClass_KillNodes (asm:446)
    void KillNodes();

    // asm: SpawnManagerClass_CountReadyNodes (asm:1694)
    int32 CountReadyNodes() const;
    // asm: SpawnManagerClass_CountReadySpawns (asm:1739)
    int32 CountReadySpawns() const;
    // asm: SpawnManagerClass_CountSpecialSpawns (asm:1787)
    int32 CountSpecialSpawns() const;

    // asm: SpawnManagerClass_GetSize (asm:2298) 返回 74h
    static int32 Get_Size() { return 0x74; }
    // asm: SpawnManagerClass_GetAbstractDerivationID (asm:2324) 返回 41h
    static int32 Get_Abstract_Derivation_ID() { return 0x41; }

    // 是否为导弹型子机（对应 asm 里与 V3RocketType/DMislType/CMislType 的比较）
    bool IsMissileSpawnType(AircraftTypeClass* pType) const;

private:
    // 派发一个子机并把它放到母舰的发射位（asm state_0 / state_7 的核心）。
    TechnoClass* SpawnUnitAt(const CoordStruct& flh, int32 facing);
    // 按 asm 的三字计时器语义判断是否到点。
    static bool Timer_Elapsed(const Timer& t, int32 frame);

public:
    TechnoClass*        Owner;          // +0x24
    AircraftTypeClass*  SpawnType;      // +0x28
    int32               MaxSpawns;      // +0x2C
    int32               RegenRate;      // +0x30
    int32               ReloadRate;     // +0x34
    DynamicVectorClass<SpawnControl*> SpawnedNodes;   // +0x38

    Timer               UpdateTimer;    // +0x50
    Timer               SpawningTimer;  // +0x5C

    AbstractClass*      Destination;    // +0x68
    AbstractClass*      Target;         // +0x6C
    int32               SomeTimestamp;  // +0x70
};
