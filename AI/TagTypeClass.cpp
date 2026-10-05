#include "TagTypeClass.h"
#include "TriggerTypeClass.h"

DynamicVectorClass<TagTypeClass*>* TagTypeClass::Array = nullptr;

TagTypeClass::TagTypeClass(const char* pID) noexcept
    : AbstractTypeClass(pID)
    , TriggerType(nullptr)
    , TagAction(0)
{
}

TagTypeClass::TagTypeClass(noinit_t) noexcept
    : AbstractTypeClass(noinit)
    , TriggerType(nullptr)
    , TagAction(0)
{
}

TagTypeClass::~TagTypeClass()
{
    TriggerType = nullptr;
}

HRESULT TagTypeClass::GetClassID(CLSID* pClassID)
{
    (void)pClassID;
    return 0;
}

HRESULT TagTypeClass::Load(IStream* pStm)
{
    (void)pStm;
    return 0;
}

HRESULT TagTypeClass::Save(IStream* pStm, BOOL fClearDirty)
{
    (void)pStm;
    (void)fClearDirty;
    return 0;
}

AbstractType TagTypeClass::WhatAmI() const
{
    return AbstractType::TagType;
}

int32 TagTypeClass::Size() const
{
    return static_cast<int32>(sizeof(TagTypeClass));
}

TagTypeClass* TagTypeClass::Find(const char* pID)
{
    if (Array == nullptr || pID == nullptr) {
        return nullptr;
    }

    const int32 count = Array->GetCount();
    for (int32 i = 0; i < count; ++i) {
        TagTypeClass* pTag = (*Array)[i];
        if (pTag != nullptr && strcasecmp(pID, pTag->ID) == 0) {
            return pTag;
        }
    }
    return nullptr;
}

TagTypeClass* TagTypeClass::FindOrAllocate(const char* pID)
{
    TagTypeClass* pTag = Find(pID);
    if (pTag != nullptr) {
        return pTag;
    }

    pTag = new TagTypeClass(pID);
    if (Array == nullptr) {
        Array = new DynamicVectorClass<TagTypeClass*>();
    }
    Array->AddItem(pTag);
    return pTag;
}

// ------------------------------------------------------------------------
// 根据游戏行为，可知按名查找先比对标识符再比对显示名，两个通道
// 任一命中即返回。
// ------------------------------------------------------------------------
TagTypeClass* TagTypeClass::FindByNameOrID(const char* pName)
{
    return Find(pName);
}

bool TagTypeClass::ContainsAllowWin() const
{
    return TriggerType != nullptr && TagAction == 1;
}

void TagTypeClass::CrossVerticalZone(int32 zone)
{
    (void)zone;
}
