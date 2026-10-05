#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Containers/DynamicVectorClass.h"

class CCINIClass;
class BuildingTypeClass;

// ============================================================================
// BaseNodeClass - one pre-placed structure of a player's starting base.
//
//   Sixteen bytes per node, exactly as the original strides them:
//
//     +00  BuildingTypeClass* pType    resolved "Type" (or -1 for a raw index)
//     +04  Point Cell                  the cell the node sits on
//     +08  int32 field_08              fourth comma field
//     +0C  int32 field_0C              fifth comma field
//
//   The "<Type>,<X>,<Y>,<...>,<...>" string is split with strtok; a leading
//   '-' means the first field is a numeric building index rather than a name.
// ============================================================================

struct BaseNodeClass
{
    BuildingTypeClass*  pType;
    CellStruct          Cell;
    int32               field_08;
    int32               field_0C;
};

// ============================================================================
// BaseClass - the starting base of one house.
//
 //   BaseClass_LoadFromINI reads:
//     "PercentBuilt"  -> +1C  (default: previous value)
//     "NodeCount"     -> number of "%03d" entries to walk
//   Each "%03d" entry names a node, and the nodes are appended to the array
//   at +04 as long as the capacity check at +10/+14 allows.
// ============================================================================

class BaseClass
{
public:
    BaseClass();
    virtual ~BaseClass();

 // 根据游戏行为，可知 LoadFromINI 负责下面这段逻辑。
    void LoadFromINI(CCINIClass* pINI, const char* pSection);

    void Clear();

    // ── Node storage ─────────────────────────────────────────────────────
    BaseNodeClass*  Nodes;          // +04
    int32           Capacity;       // +10
    bool            IsAllocated;    // +0D
    int32           GrowthStep;     // +14
    int32           PercentBuilt;   // +1C
    int32           NodeCount;      // current fill level
};
