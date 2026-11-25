#pragma once

#include "../Core/Definitions.h"
#include "../Containers/DynamicVectorClass.h"

class HouseClass;
class TechnoTypeClass;
class TechnoClass;
class BuildingClass;
class AbstractClass;
class IStream;

// ============================================================================
// FactoryClass - production queue attached to a producing structure
//
// One FactoryClass exists per production structure (war factory, barracks,
// shipyard, etc.).  It owns the queue of pending production orders and
// advances the build progress each frame.  When a build completes the
// completed unit is placed at the factory's exit cell and the house is
// notified.
//
// 原始布局（gamemd.exe 0x4C98xx..0x4CA8xx）逐字段还原：
//   +0x24  Progress                当前产品的生产进度（0..0x36）
//   +0x28  byte                    本帧是否推进过（Update 置位）
//   +0x2C  FrameTimerClass         生产节拍计时器（+0x2C/+0x30/+0x34）
//   +0x38  ProductionEstimateFrames 预计帧数
//   +0x3C  Rate                    每节拍推进量（CTOR 置 1）
//   +0x40  Queue                   DynamicVectorClass<TechnoTypeClass const*>
//   +0x58  CurrentProduction       当前在线产品对象（TechnoClass*）
//   +0x5C  byte                    资金脏标记（Update 清零）
//   +0x5D  IsDifferent             "队列改变"标记，读取即清
//   +0x60  Balance                 当前产品剩余应付金额
//   +0x64  OriginalBalance         原始应付金额
//   +0x68  SpecialItem             阵营专属奖励项（空 = 0xFFFFFFFF）
//   +0x6C  Owner                   所属 HouseClass*
//   +0x70  OnHold                  生产被挂起
//   +0x71  byte                    完成挂起标记
// ============================================================================
class FactoryClass
{
public:
    FactoryClass(BuildingClass* pFactory, HouseClass* pOwner);
    ~FactoryClass();

    // Queue / order management
    bool QueueProduction(TechnoTypeClass* pType);
    // ========================================================================
    // asm: FactoryClass_StartProduction (0x4CA5A6)
    //
    //   void __thiscall FactoryClass_StartProduction(FactoryClass *this)
    //   retn -> 无参数
    //
    //   把队首产品真正开上生产线：弹出 +0x44 队列表的表头，取它的索引
    //   (vft+0x40) 与是否舰船 (TechnoTypeClass+Naval)，再转交
    //   HouseClass::BeginProductionOf。逐段见 .cpp。
    // ========================================================================
    void StartProduction();
    bool RemoveFromQueue(TechnoTypeClass* pType);
    bool IsXQueued(TechnoTypeClass* pType) const;
    int32 CountThisInQueue(TechnoTypeClass* pType) const;
    bool Suspend(bool a2);
    void Unsuspend();
    // ========================================================================
    // asm: FactoryClass_Unsuspend (0x4C9E97)
    //
    //   char __thiscall FactoryClass_Unsuspend(FactoryClass *this, int a2)
    //   retn 4 -> 1 参数
    //
    //   恢复生产队列。逐段见 .cpp 内注释。
    // ========================================================================
    bool Unsuspend_Ex(int32 a2);
    bool CompletedProduction();
    bool AbandonProduction();
    // asm: FactoryClass::Abandon (0x4C9Fxx) —— CompletedProduction 的兄弟，
    //   彻底放弃当前生产并把退款还给阵营。
    bool Abandon();

    // Progress
    void Update();
    // ========================================================================
    // HouseClass::Update_Factories（0x509129）的单厂实现：把"已经造不出来"的
    //   队列条目摘掉、在制品放弃并续线、可造但被挂起的恢复生产。语义逐段写在
    //   .cpp 里。
    // ========================================================================
    void Validate_Production();
    int32 GetBuildTimeFrames() const;
    // ========================================================================
    // asm: HouseClass_Update_Factories_Timing 内联体 —— 按在线产品重算本
    //   工厂的建时估计（建时/54，clamp [1,255]），值变化才写回 +0x38。
    //   （原版把这段直接内联在 HouseClass 的循环里；这里封成工厂自己的方法。）
    // ========================================================================
    void Recalc_Build_Estimate();
    int32 GetProgress() const { return Progress; }
    int32 GetRemainingFrames() const;
    // ========================================================================
    // asm: _sub_4CA180_unused (0x4CA180) —— 在线产品的“剩余应付金额”。
    //   无在线产品 -> 0；否则 [Balance] / (0x36 - Progress)（进度已满则
    //   直接返回 Balance）。与 Update 内联的 span 计算同源。
    // ========================================================================
    int32 Get_Cost() const;
    // asm: FactoryClass::Has_Completed (0x4CA137) —— 完成判定别名。
    bool IsDone() const { return IsCompleted(); }
    bool IsCompleted() const;
    bool IsSuspended() const { return OnHold; }
    bool IsWorking() const;
    // asm: FactoryClass+0x38 (ProductionEstimateFrames) —— 非零表示生产线正在
    //   推进。侧边栏用它区分"右键暂停 / 右键取消"与"左键继续生产"。
    bool Is_Producing() const { return ProductionEstimateFrames != 0; }
    // asm: FactoryClass+0x50 —— 队列里还有未开工的产品（>0 即"排队中"）。
    bool Has_Queued() const { return Queue.Count > 0; }
    // 队列深度（诊断 + 队伍订单插队判定用）。
    int32 GetQueueCount() const { return Queue.Count; }

    // Accessors
    BuildingClass*   GetFactoryBuilding() const { return Factory; }
    HouseClass*      GetOwner() const { return Owner; }
    TechnoTypeClass* GetCurrentOrder() const { return CurrentType; }

    // FactoryClass_IsOnHold (0x4C9BA0): `mov al, [ecx+OnHold]`.
    //   True while the factory's owner has put the queue on hold.
    bool IsOnHold() const { return OnHold; }
    // FactoryClass::Has_Changed (0x4C9BB0).
    //   Reads the "the queue changed since the sidebar last looked" byte and
    //   clears it, so the caller learns about an update exactly once.
    bool Has_Changed() { const bool c = IsDifferent; IsDifferent = false; return c; }
    // FactoryClass::Get_Product (0x4CA130): the object currently on the line.
    TechnoClass* Get_Product() const { return CurrentProduction; }
    // FactoryClass_GetSpecialItem (0x4CA150): the side-specific "special"
    //   item (a free bonus unit the house can build), cached at +0x68.
    int32 GetSpecialItem() const { return SpecialItem; }

    // 诊断：把生产线的内部节拍一并打出来（OPENYR_TEAMDBG 用）。
    int32 Get_EstimateFrames() const { return ProductionEstimateFrames; }
    int32 Get_Balance() const { return Balance; }
    bool  IsCompleteSuspended() const { return CompleteSuspended; }

    // Serialization / CRC
    bool Save(IStream* pStm) const;
    bool Load(IStream* pStm);
    void ComputeCRC(CRCEngine& crc) const;

    // Pointer invalidation —— 统一签名（本批简报第三节）。
    // asm: FactoryClass_PointerGotInvalid (0x4CA580) retn 8
    void PointerGotInvalid(AbstractClass* pInvalid, bool removed);

    // vtable 槽位（十六进制名）
    int32 GetAbstractDerivationID() const { return 0x0C; }  // asm: mov eax,0Ch
    int32 Size() const { return 0x74; }                     // asm: mov eax,74h

    // Global registry
    static DynamicVectorClass<FactoryClass*>* Array;
    static void Init_Array();
    static void Delete_Array();

private:
    bool PlaceProducedUnit(TechnoTypeClass* pType);
    // QueueProduction 的 lct_BuildingType 开工段：让本线立刻开工造 pType。
    bool Start_Specific(TechnoTypeClass* pType);

    // asm: [esi+0x24]
    int32            Progress;
    // asm: [esi+0x28]
    bool             AdvancedThisFrame;
    uint8            pad_25[3];
    // asm: [esi+0x2C] 生产节拍计时器（3 dword：起始帧 / 保留 / 时长）
    int32            ProductionStartFrame;
    // asm: [esi+0x30]
    int32            ProductionTimerPad;
    // asm: [esi+0x34]
    int32            ProductionDuration;
    // asm: [esi+0x38]
    int32            ProductionEstimateFrames;
    // asm: [esi+0x3C]
    int32            Rate;
    // asm: [esi+0x40]  (DynamicVectorClass<TechnoTypeClass const*>)
    DynamicVectorClass<TechnoTypeClass*> Queue;
    // asm: [esi+0x58]
    TechnoClass*     CurrentProduction;
    // asm: [esi+0x5C]
    bool             BalanceDirty;
    // asm: [esi+0x5D]
    bool             IsDifferent;
    uint8            pad_5E[2];
    // asm: [esi+0x60]
    int32            Balance;
    // asm: [esi+0x64]
    int32            OriginalBalance;
    // asm: [esi+0x68]
    int32            SpecialItem;
    // asm: [esi+0x6C]
    HouseClass*      Owner;
    // asm: [esi+0x70]
    bool             OnHold;
    // asm: [esi+0x71]
    bool             CompleteSuspended;

    // ------------------------------------------------------------------------
    // 非 asm 字段：OpenYR 适配层。
    // 原版 FactoryClass 的 owner 就是 +0x6C 的 HouseClass*，工厂建筑由
    // BuildingClass 持有 FactoryClass* 反查；这里保留两个便利字段以便与
    // OpenYR 现有的 BuildingClass::Factory / HouseClass 调用链连通。
    // ------------------------------------------------------------------------
    BuildingClass*   Factory;        // 拥有本工厂的生产建筑（OpenYR 适配）
    TechnoTypeClass* CurrentType;    // 当前正在生产的类型（OpenYR 适配）
};
