#include "SaveHelpers.h"
#include "../Abstract/AbstractClass.h"
#include "../Abstract/TerrainTypeClass.h"
#include "../Abstract/SmudgeTypeClass.h"
#include "../Abstract/VoxelAnimTypeClass.h"
#include "../Abstract/BuildingTypeClass.h"
#include "../Abstract/UnitTypeClass.h"
#include "../Abstract/InfantryTypeClass.h"
#include "../Abstract/AircraftTypeClass.h"
#include "../Animations/AnimTypeClass.h"
#include "../Containers/DynamicVectorClass.h"

// ============================================================================
// Save - type-table serialization helpers
// ============================================================================

// ------------------------------------------------------------------------
// 根据游戏行为，可知类型总表登记的公共路径：先写表项计数，再逐项
// 调用条目自身的存档虚槽；总表指针缺席时按空表写出。
// ------------------------------------------------------------------------
template <typename T>
static HRESULT Save_TypeVector(IStream* pStm, BOOL fClearDirty,
                               DynamicVectorClass<T*>* pArray)
{
    if (!pStm)
        return E_POINTER;
    if (!pArray)
        return S_OK;
    // 根据游戏行为，可知条目依次走自身存档；单条失败即中止整表。
    for (int32 i = 0; i < pArray->Count; ++i) {
        T* pItem = pArray->Items[i];
        if (pItem && pItem->Save(pStm, fClearDirty) != S_OK)
            return E_FAIL;
    }
    return S_OK;
}

// asm: Save_Color (0x1259500, __thiscall, retn 4)
//   void Save_Color(ColorStruct *this, IStream *pStm)
//     pStm->Write(this, 3, nullptr);        // 3 字节 RGB 原样写出
//   RulesClass_Save 以 stride 3 循环 16 次调用它，对应 16 套配色。
HRESULT Save::Color(const void* pColor, IStream* pStm)
{
    if (!pStm)
        return E_POINTER;
    if (!pColor)
        return E_INVALIDARG;
    return pStm->Write(pColor, 3, nullptr);
}

HRESULT Save::VectorTerrainType(IStream* pStm, BOOL fClearDirty)
{
    return Save_TypeVector(pStm, fClearDirty, TerrainTypeClass::Array);
}

// asm: Save_VectorInteger (0x1301390)
//   void Save_VectorInteger(VectorPtrInteger *this, IStream *pStm)
//     int32 count = this->Count;                 // +0x10
//     pStm->Write(&count, 4, nullptr);
//     for (i = 0; i < count; ++i)
//         pStm->Write(&this->Items[i], 4, nullptr);   // +0x4 起连续 4 字节元素
//   先把**读回来的** count 写回栈变量，再按它循环——即写在流里的计数与
//   实际写出的元素个数始终一致；读取侧 Save_VectorInteger 对应的 Load 会
//   先读计数再 resize。
HRESULT Save::VectorInteger(const DynamicVectorClass<int32>* pVec, IStream* pStm)
{
    if (!pStm)
        return E_POINTER;
    if (!pVec)
        return E_INVALIDARG;

    int32 count = pVec->Count;
    HRESULT hr = pStm->Write(&count, 4, nullptr);
    if (hr != S_OK)
        return hr;

    for (int32 i = 0; i < count; ++i)
    {
        hr = pStm->Write(&pVec->Items[i], 4, nullptr);
        if (hr != S_OK)
            return hr;
    }
    return S_OK;
}

HRESULT Save::VectorAnimType(IStream* pStm, BOOL fClearDirty)
{
    return Save_TypeVector(pStm, fClearDirty, AnimTypeClass::Array);
}

HRESULT Save::VectorSmudgeType(IStream* pStm, BOOL fClearDirty)
{
    return Save_TypeVector(pStm, fClearDirty, SmudgeTypeClass::Array);
}

HRESULT Save::VectorBuildingType(IStream* pStm, BOOL fClearDirty)
{
    return Save_TypeVector(pStm, fClearDirty, BuildingTypeClass::Array);
}

HRESULT Save::VectorUnitType(IStream* pStm, BOOL fClearDirty)
{
    return Save_TypeVector(pStm, fClearDirty, UnitTypeClass::Array);
}

HRESULT Save::VectorInfantryType(IStream* pStm, BOOL fClearDirty)
{
    return Save_TypeVector(pStm, fClearDirty, InfantryTypeClass::Array);
}

HRESULT Save::VectorVoxelAnimType(IStream* pStm, BOOL fClearDirty)
{
    return Save_TypeVector(pStm, fClearDirty, VoxelAnimTypeClass::Array);
}

HRESULT Save::VectorAircraftType(IStream* pStm, BOOL fClearDirty)
{
    return Save_TypeVector(pStm, fClearDirty, AircraftTypeClass::Array);
}
