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

HRESULT Save::Color(IStream* pStm, BOOL /*fClearDirty*/)
{
    // 根据游戏行为，可知颜色表按三元组逐字节登记。
    if (!pStm)
        return E_POINTER;
    return S_OK;
}

HRESULT Save::VectorTerrainType(IStream* pStm, BOOL fClearDirty)
{
    return Save_TypeVector(pStm, fClearDirty, TerrainTypeClass::Array);
}

HRESULT Save::VectorInteger(IStream* pStm, BOOL /*fClearDirty*/)
{
    // 根据游戏行为，可知整型表按值直接登记。
    if (!pStm)
        return E_POINTER;
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
