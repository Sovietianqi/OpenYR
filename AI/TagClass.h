#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Containers/DynamicVectorClass.h"
#include "../Abstract/AbstractClass.h"

// ============================================================================
// Forward declarations
// ============================================================================
class TriggerClass;
class TriggerTypeClass;
class TagTypeClass;
class HouseClass;

// ============================================================================
// TagClass - tag used to link map objects to one or more triggers
// A tag carries a name (its ID) and a list of TriggerClass pointers. Objects
// that reference a tag will fire every trigger attached to it when their
// trigger event occurs. Inherits AbstractClass for persistence/RTTI.
// ============================================================================
class NOVTABLE TagClass : public AbstractClass {
public:
    static const AbstractType AbsID = AbstractType::Tag;

    static DynamicVectorClass<TagClass*>* Array;
    // asm: vec_Tags_0x4 —— 与 vec_Tags_0x10（即 Array）平行的第二张实例表，
    //   路点区域进入判定（FootClass_UpdatePosition）走这张。
    static DynamicVectorClass<TagClass*>* Array4;

    static TagClass* Find(const char* pName);
    static TagClass* FindOrAllocate(const char* pName);

    TagClass(const char* pName = nullptr) noexcept;
    explicit TagClass(noinit_t) noexcept : AbstractClass(noinit) {}
    virtual ~TagClass();

    virtual HRESULT GetClassID(CLSID* pClassID) override;
    virtual HRESULT Load(IStream* pStm) override;
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override;

    virtual AbstractType WhatAmI() const override;
    virtual int32 Size() const override;

    void AttachTrigger(TriggerClass* pTrigger);
    void DetachTrigger(TriggerClass* pTrigger);

    // ------------------------------------------------------------------------
    // asm: LogicClass_Update 的 Tag 段（L751177…L751336）。
    // 对本标签逐条上抛"全局/世界"事件族（50/27/28/36/37/45/46/13/51/14），
    // 条件取自 Scenario 的变更标志；一旦某条事件被接住就停止本标签后续上抛。
    // 这是原版每帧唯一的触发器驱动入口。
    // ------------------------------------------------------------------------
    void SpringAll();

    // asm: LogicClass_Update+0x32 的每帧循环体（遍历 vec_Tags_0x10）。
    static void ProcessTagEvents();

    // Task-required trigger/object management API
    void Add_Trigger(TriggerClass* pTrigger);
    void Remove_Trigger(TriggerClass* pTrigger);
    int32 Get_Trigger_Count() const;
    TriggerClass* Get_Trigger(int32 index) const;
    void Clear_Triggers();
    void Assign_To_Object(AbstractClass* pObject);
    bool Is_Assigned(AbstractClass* pObject) const;

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知标签补全原版命名形态：沿挂载触发器逐个询问——
    // 任一可重复、区域检查线命中、路点区域进入、允许胜利；事件上抛按
    // 事件类型广播到所有挂载触发器。
    // ------------------------------------------------------------------------
    bool IsTriggerRepeating() const;
    bool CrossHorizontal(int32 zone);
    bool CrossVertical(int32 zone);
    bool IsWPZoneEntry() const;
    bool DoesAllowWin() const;
    bool IsAttachedToTrigger(TriggerClass* pTrigger) const;
    // asm: TagClass_IsAttachedToTrigger(TagClass* this, _TriggerTypeClass* targetTrigger)
    //   沿 AttachedTrigger 链查找"目标触发器类型"。
    //   asm: mov eax,[ecx+TagClass.AttachedTrigger]; loop: cmp eax,ecx;
    //        mov eax,[eax+TagClass.AttachedTrigger]; jnz loop
    bool IsAttachedToTrigger(TriggerTypeClass* pTargetTrigger) const;
    // asm: char __thiscall TagClass_RaiseEvent(TagClass *this, eEventKind eventKind,
    //        _ObjectClass *TagObject, wXY loc, char isRepeating, _TechnoClass *source)
    //   retn 14h —— 5 参，返回"是否有触发器真正执行了动作"。
    bool RaiseEvent(int32 eventKind, AbstractClass* pTagObject, CellStruct loc, bool isRepeating, AbstractClass* pSource);

    // ------------------------------------------------------------------------
    // asm: FootClass_UpdatePosition 的 [CellTags] 判定段。
    //   单位每进入一个新格（含飞行器越过），若该格挂有 AttachedTag，就向
    //   该标签上抛事件 1（EnteredBy）。这是"单位踩点触发剧情"的唯一入口。
    //   TriggerEventType 与 EventKind 同值，故直接传 1。
    // ------------------------------------------------------------------------
    static void SpringCellTag(CellStruct cell, AbstractClass* pObject);

    // asm: TagClass_GetCRC / _GetSize / _GetAbstractDerivationID
    int32 GetCRC() const;
    int32 GetSize() const;
    int32 GetAbstractDerivationID() const;

    void Compute_CRC(CRCEngine& crc) const;

    char Name[0x18];
    DynamicVectorClass<TriggerClass*> TriggerList;

    // ── asm 权威字段（TagClass struc, sizeof=0x18）──────────────────────────
    //   TagType          +0x24  _TagTypeClass*   本实例所属的标签类型
    //   AttachedTrigger  +0x28  _TriggerClass*   挂载的触发器实例链（单链）
    //   field_2C         +0x2C  dword
    //   DefaultCoords    +0x30  dwXY             默认坐标
    //   field_34         +0x34  byte
    //   IsExecuting      +0x35  byte
    TagTypeClass*   TagType;
    TriggerClass*   AttachedTrigger;
    int32           field_2C;
    CellStruct      DefaultCoords;
    uint8           field_34;
    bool            IsExecuting;
    DynamicVectorClass<AbstractClass*> AssignedObjects;
    bool IsInitialized;
};
