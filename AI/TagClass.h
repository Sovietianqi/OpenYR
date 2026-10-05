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
    void SpringAll();

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
    void RaiseEvent(int32 eventKind, AbstractClass* pTagObject, CellStruct loc, bool isRepeating, AbstractClass* pSource);

    void Compute_CRC(CRCEngine& crc) const;

    char Name[0x18];
    DynamicVectorClass<TriggerClass*> TriggerList;
    DynamicVectorClass<AbstractClass*> AssignedObjects;
    bool IsInitialized;
};
