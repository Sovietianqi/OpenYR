#pragma once

#include "../Abstract/AbstractTypeClass.h"
#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Containers/DynamicVectorClass.h"

class TriggerTypeClass;

// ------------------------------------------------------------------------
// 根据游戏行为，可知标签类型是触发器类型与附着对象的纽带：查找
// 沿标识符进行，允许胜利与区块穿越都有快捷判定。
// ------------------------------------------------------------------------
class TagTypeClass : public AbstractTypeClass {
public:
    static const AbstractType AbsID = AbstractType::TagType;

    static DynamicVectorClass<TagTypeClass*>* Array;

    static TagTypeClass* Find(const char* pID);
    static TagTypeClass* FindOrAllocate(const char* pID);
    static TagTypeClass* FindByNameOrID(const char* pName);

    TagTypeClass(const char* pID) noexcept;
    explicit TagTypeClass(noinit_t) noexcept;
    virtual ~TagTypeClass();

    virtual HRESULT GetClassID(CLSID* pClassID) override;
    virtual HRESULT Load(IStream* pStm) override;
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override;

    virtual AbstractType WhatAmI() const override;
    virtual int32 Size() const override;

    bool ContainsAllowWin() const;
    void CrossVerticalZone(int32 zone);

    TriggerTypeClass* TriggerType;
    int32 TagAction;
};
