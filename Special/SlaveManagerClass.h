#pragma once

#include "../Abstract/AbstractClass.h"
#include "../Containers/DynamicVectorClass.h"
#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"

class TechnoClass;
class HouseClass;
class InfantryTypeClass;

static constexpr int32 MAX_SLAVE_SLOTS = 5;

// ============================================================================
// SlaveManagerClass —— 尤里"奴隶矿车"的奴隶管理器
//
// asm 权威来源：/workspace/asm_export/SlaveManagerClass.asm.txt
//   sizeof = 0x64（SlaveManagerClass_GetSize 返回 64h）
//   派生 ID = 0x48（GetAbstractDerivationID）
//   四段 vtable（SlaveManager / IRTTITypeInfo / INoticeSink / INoticeSource）
//   继承 AbstractClass（+0x00..+0x23）
//
//   +0x24  Owner          奴隶矿车本体（TechnoClass*）
//   +0x28  SlaveType      奴隶兵种（InfantryTypeClass*，取自 type->Slave）
//   +0x2C  MaxSlaves      奴隶槽位数（type->SlavesNumber）
//   +0x30  RegenRate      阵亡补员节拍（type->SlaveRegenRate）
//   +0x34  ReloadRate     劳作节拍（type->SlaveReloadRate）
//   +0x38  SlaveControls  DynamicVectorClass<SlaveControl*>（槽位表）
//   +0x50  TimerStart     主 AI 起始帧
//   +0x54  TimerVar       计时器暂存（asm var_8）
//   +0x58  TimerLeft      主 AI 周期
//   +0x5C  State          劳作状态：0=空闲 1=待命 2=开采 4=展开 6=换位
//   +0x60  StateStamp     状态时间戳
//
// 每个 SlaveControl（asm 0x14 字节）：
//   +0x00  Slave          奴隶本体
//   +0x04  State          0=在岗 6=阵亡/待补员
//   +0x08  { TimeStart, field_C, TimeLeft } 三字计时器
// ============================================================================
class SlaveManagerClass : public AbstractClass {
public:
    static const AbstractType AbsID = AbstractType::SlaveManager;

    struct SlaveControl {
        TechnoClass* Slave;     // +0x00
        int32        State;     // +0x04  0=在岗 6=阵亡/待补员
        int32        TimeStart; // +0x08
        int32        field_C;   // +0x0C
        int32        TimeLeft;  // +0x10  补员节拍（SlaveRegenRate）
    };

    enum SlaveState : int32 {
        Slave_Ready = 0,
        Slave_Dead  = 6,
    };

    // 劳作状态机（asm [this+0x5C] 的取值）
    enum ManagerState : int32 {
        Manager_Idle     = 0,
        Manager_Standby  = 1,
        Manager_Harvest  = 2,
        Manager_Deploy   = 4,
        Manager_Relocate = 6,
    };

    // asm: SlaveManagerClass_CTOR (asm:25)  retn 14h = 5 参数
    SlaveManagerClass(TechnoClass* pOwner, InfantryTypeClass* pSlaveType,
                      int32 num, int32 regenRate, int32 reloadRate) noexcept;
    virtual ~SlaveManagerClass();

    virtual AbstractType WhatAmI() const override { return AbstractType::SlaveManager; }
    virtual int32 Size() const override { return 0x64; }

    virtual HRESULT GetClassID(CLSID* pClassID) override;                 // asm:1309
    virtual HRESULT Load(IStream* pStm) override;                        // asm:1351
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override;       // asm:1539
    virtual void ComputeCRC(CRCEngine& crc) const override;              // asm:1252
    virtual void PointerGotInvalid(AbstractClass* pInvalid, bool removed) override;
    virtual void Update() override;                                      // asm:482
    virtual void Init() override;                                        // asm:850

    // asm: SlaveManagerClass_6B0CC0 (asm:794) —— 全队转入待命。
    void AI();
    // asm: SlaveManagerClass_ReplaceWhichBelongsToUnit (asm:415)
    void ReplaceWhichBelongsToUnit(TechnoClass* pNewOwner);
    // asm: SlaveManagerClass_LoseSlave (asm:546)
    void LoseSlave(TechnoClass* pSlave);
    // asm: SlaveManagerClass_ReleaseSlaves (asm:622) —— 移交 / 散去全部奴隶
    void ReleaseSlaves(HouseClass* pNewOwnerHouse);
    void ReleaseAll() { ReleaseSlaves(nullptr); }
    // asm: SlaveManagerClass_ExplicitHarvest (asm:906)
    void ExplicitHarvest();
    // asm: SlaveManagerClass_ShouldWakeUp (asm:1162)
    bool ShouldWakeUp() const;

    // ---- 槽位语义 API ----
    int32        GetSlaveIndex(const TechnoClass* pSlave) const;
    SlaveControl* GetSlaveControl(int32 index) const;
    TechnoClass* GetSlaveAt(int32 index) const;
    SlaveControl* AllocSlave();
    bool         Add_Slave(TechnoClass* pSlave);
    bool         Remove_Slave(TechnoClass* pSlave);
    void         SlaveTo(TechnoClass* pSlave, TechnoClass* pOwner);
    bool         IsSlaveMiner() const;
    void         UpdateSlaveStats();
    int32        GetSlaveCount() const { return SlaveControls.Count; }
    int32        GetState() const { return State; }

    static int32 Get_Abstract_Derivation_ID() { return 0x48; }

    // vec_SlaveManagers —— 全局在册管理器表
    static DynamicVectorClass<SlaveManagerClass*>* Array;

private:
    // 为某个槽位创建奴隶本体（asm CTOR 循环体内联逻辑）。
    TechnoClass* SpawnSlave();
    // 唤醒单个奴隶（asm: slave->vtable[0x3D0]()）。
    void WakeSlave(TechnoClass* pSlave);
    // sub_6AF6C0 —— 逐槽位补员/回收。
    void TickSlaves();
    // sub_6AFD60 —— 劳作推进。
    void TickHarvest();

public:
    TechnoClass*        Owner;          // +0x24
    InfantryTypeClass*  SlaveType;      // +0x28
    int32               MaxSlaves;      // +0x2C
    int32               RegenRate;      // +0x30
    int32               ReloadRate;     // +0x34
    DynamicVectorClass<SlaveControl*> SlaveControls;  // +0x38

    int32               TimerStart;     // +0x50
    int32               TimerVar;       // +0x54
    int32               TimerLeft;      // +0x58
    int32               State;          // +0x5C
    int32               StateStamp;     // +0x60
};
