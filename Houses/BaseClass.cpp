#include "BaseClass.h"
#include "../Abstract/BuildingTypeClass.h"
#include "../INI/INIClass.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>

// ============================================================================
// BaseClass - pre-placed starting base (rulesmd.ini [<House>] node list)
// ============================================================================

BaseClass::BaseClass()
    : Nodes(nullptr)
    , Capacity(0)
    , IsAllocated(false)
    , GrowthStep(1)
    , PercentBuilt(0)
    , NodeCount(0)
{
}

BaseClass::~BaseClass()
{
    delete[] Nodes;
}

void BaseClass::Clear()
{
    delete[] Nodes;
    Nodes = nullptr;
    Capacity = 0;
    NodeCount = 0;
}

// ============================================================================
// BaseClass_LoadFromINI - asm 0x42EBED
//
//   "PercentBuilt" keeps its current value when the key is absent.  The
//   "NodeCount" entries are then read from the "%03d" keys; each one is a
//   comma separated list whose first field is a building name (or a leading
//   '-' followed by a raw index), and the next two fields are the cell.
//   An entry is appended when the capacity check passes.
// ============================================================================
void BaseClass::LoadFromINI(CCINIClass* pINI, const char* pSection)
{
    if (pINI == nullptr || pSection == nullptr) {
        return;
    }

    PercentBuilt = pINI->ReadInteger(pSection, "PercentBuilt", PercentBuilt);

    const int32 cntNodes = pINI->ReadInteger(pSection, "NodeCount", 0);
    if (cntNodes <= 0) {
        return;
    }

    for (int32 idxNode = 0; idxNode < cntNodes; ++idxNode)
    {
        char key[0x10];
        std::sprintf(key, "%03d", idxNode);

        char str[0x80];
        str[0] = '\0';
        pINI->ReadString(pSection, key, "", str, sizeof(str));

        int32 firstField = 0;
        if (str[0] == '-')
        {
            // A leading '-' means the field is a raw building index.
            char* pToken = std::strtok(str, ",");
            firstField = std::atoi(pToken);
        }
        else
        {
            char* pToken = std::strtok(str, ",");
            firstField = BuildingTypeClass::FindIndex(pToken);
        }

        char* pX = std::strtok(nullptr, ",");
        const int32 x = std::atoi(pX);

        char* pY = std::strtok(nullptr, ",");
        const int32 y = std::atoi(pY);

        // Growth: only append while the array still has room.  The original
        // grows through the virtual at vtable +8 when the capacity is full.
        if (NodeCount >= Capacity)
        {
            const int32 newCap =
                (Capacity == 0) ? GrowthStep : (Capacity + GrowthStep);
            if (newCap <= NodeCount) {
                continue;
            }

            BaseNodeClass* pNew = new BaseNodeClass[newCap]();
            if (Nodes != nullptr)
            {
                for (int32 i = 0; i < NodeCount; ++i) {
                    pNew[i] = Nodes[i];
                }
                delete[] Nodes;
            }
            Nodes = pNew;
            Capacity = newCap;
        }

        BaseNodeClass& node = Nodes[NodeCount];
        node.pType = reinterpret_cast<BuildingTypeClass*>(
                         static_cast<intptr_t>(firstField));
        node.Cell.X = static_cast<int16>(x);
        node.Cell.Y = static_cast<int16>(y);
        node.field_08 = 0;
        node.field_0C = 0;
        ++NodeCount;
    }
}
