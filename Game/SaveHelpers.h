#pragma once

#include "../COM/IUnknown.h"
#include "../Containers/DynamicVectorClass.h"
#include "../Core/Definitions.h"
#include "../Core/Macros.h"

class AbstractClass;

// ============================================================================
// Save - type-table serialization helpers
//
//  根据游戏行为，可知存档流在写场景时把各类型总表逐个登记：每个条目
//  走自身的 Load/Save 虚槽，数组前后各写一个计数。
// ============================================================================

class Save {
public:
    // asm: Save_Color —— 把一份 3 字节 RGB 原样写出。
    static HRESULT Color(const void* pColor, IStream* pStm);
    static HRESULT VectorTerrainType(IStream* pStm, BOOL fClearDirty);
    // asm: Save_VectorInteger —— 先写计数（4 字节）再逐项写 int32。
    static HRESULT VectorInteger(const DynamicVectorClass<int32>* pVec, IStream* pStm);
    static HRESULT VectorAnimType(IStream* pStm, BOOL fClearDirty);
    static HRESULT VectorSmudgeType(IStream* pStm, BOOL fClearDirty);
    static HRESULT VectorBuildingType(IStream* pStm, BOOL fClearDirty);
    static HRESULT VectorUnitType(IStream* pStm, BOOL fClearDirty);
    static HRESULT VectorInfantryType(IStream* pStm, BOOL fClearDirty);
    static HRESULT VectorVoxelAnimType(IStream* pStm, BOOL fClearDirty);
    static HRESULT VectorAircraftType(IStream* pStm, BOOL fClearDirty);
};
