#pragma once

#include "../Abstract/AbstractClass.h"
#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"

class TechnoClass;

// ============================================================================
// TemporalClass —— 超时空军团兵（Chrono Legionnaire）的"冻结/抹除"武器状态
//
// asm 权威布局（TemporalClass, sizeof=0x50, mappedto_2504）：
//   +0x00  VT               vtable（TemporalClass / IRTTITypeInfo /
//                                 INoticeSink / INoticeSource 四段）
//   +0x04  AbstractClass a  基类
//   +0x24  Owner           施法者（TechnoClass*）
//   +0x28  Victim          被锁定的目标（TechnoClass*）
//   +0x2C  CreationTimer   开始锁定时的帧标记
//   +0x38  field_38        暂未命名（构造清零，Detach/PointerGotInvalid 清零）
//   +0x3C  SourceSW        关联的超级武器/来源对象（构造清零）
//   +0x40  PrevTemporal    "谁在对我施法"链的前驱（TemporalClass*）
//   +0x44  NextTemporal    "谁在对我施法"链的后继（TemporalClass*）
//   +0x48  RemainingWarp   剩余抹除量，逐帧被武器伤害与辅助伤害扣减，兜底触发
//   +0x4C  WeaponDamage    本步武器伤害快照（GetHelperDamagePerStep 写入）
//
// 语义：军团兵开火时 Fire() 把本对象挂到 Victim 的 TemporalTargettingMe
// 上，并把 Victim 记进自己的 Victim 槽；此后每帧 Update() 扣减 RemainingWarp，
// 归零时把目标真正"抹除"（播放 WarpAway 动画、走 DestroyedBy/UnInit 死亡
// 路径并清理链路）。空闲无目标或目标逃出射程时走 LetGo()/Detach() 链条解除。
// ============================================================================
class TemporalClass : public AbstractClass {
public:
    static const AbstractType AbsID = AbstractType::Temporal;

    TemporalClass() noexcept;
    TemporalClass(TechnoClass* pOwner) noexcept;
    virtual ~TemporalClass();

    virtual AbstractType WhatAmI() const override { return AbstractType::Temporal; }
    virtual int32 Size() const override { return 0x50; }

    virtual HRESULT GetClassID(CLSID* pClassID) override;
    virtual HRESULT Load(IStream* pStm) override;
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override;
    virtual void ComputeCRC(CRCEngine& crc) const override;

    // asm: TemporalClass_Update —— 逐帧推进：先做失效/超距检查，再扣减
    //   RemainingWarp，归零时抹除目标。
    virtual void Update() override;

    // asm: TemporalClass_PointerGotInvalid —— 当 Owner 或 Victim 被销毁时由
    //   TechnoClass::PointerGotInvalid 直接调用（非虚，TemporalClass 并不继承
    //   TechnoClass，故此处不标 override）。
    void PointerGotInvalid(AbstractClass* pInvalid, bool removed);

    // asm: TemporalClass_LetGo —— 完整松脱：把后继节点的外部链接归还，清空
    //   全部反向链接、持有状态，并请 Owner 回空闲。
    void LetGo();
    // asm: TemporalClass_JustLetGo —— 即刻松脱：只断自身与 Victim/Owner 的
    //   正反向链接，不触碰链上其它节点。
    void JustLetGo();
    // asm: TemporalClass_Detach —— 从 Victim 与既有前后链上摘除自己。
    void Detach();

    // asm: byte TemporalClass_CanWarpTarget(TemporalClass *this, _TechnoClass *target)
    bool CanWarpTarget(TechnoClass* pTarget);

    // asm: int TemporalClass_GetHelperDamagePerStep(TemporalClass *this, int a2)
    //   沿 NextTemporal 链递归累加每一步的武器伤害（辅助伤害）。
    int32 GetHelperDamagePerStep(int32 a2);

    // asm: void TemporalClass_Fire(TemporalClass *this, _TechnoClass *target)
    //   锁定目标：绑定 Victim/TemporalTargettingMe，插入前后链，并结算首帧
    //   副效果（收矿车告警、建筑时间化等）。
    void Fire(TechnoClass* pTarget);

    // ── 字段（对齐 asm 布局）─────────────────────────────────────────────
    TechnoClass*    Owner;            // +0x24
    TechnoClass*    Victim;           // +0x28
    int32           CreationTimer;    // +0x2C  开始施法的帧标记
    int32           field_38;         // +0x38
    void*           SourceSW;         // +0x3C  来源超级武器/对象
    TemporalClass*  PrevTemporal;     // +0x40
    TemporalClass*  NextTemporal;     // +0x44
    int32           RemainingWarp;    // +0x48
    int32           WeaponDamage;     // +0x4C
};
