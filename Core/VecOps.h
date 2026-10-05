#pragma once

#include "Definitions.h"
#include "Macros.h"
#include "Memory.h"

class AbstractClass;

// ------------------------------------------------------------------------
// 根据游戏行为，可知世界对象的注销面按对象类别分派到各自的
// 全局清单：删除前先做指针有效性校验。
// ------------------------------------------------------------------------
class VecOps {
public:
    static void Building_Delete(void* pBuilding);
    static void Infantry_Delete(void* pInfantry);
    static void Unit_Delete(void* pUnit);
    static void Aircraft_Delete(void* pAircraft);
    static void Terrain_Delete(void* pTerrain);
    static void INT_Delete(int32 index);
};
