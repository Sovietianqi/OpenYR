#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Containers/DynamicVectorClass.h"

// ============================================================================
// MapSeedClass - random map generator settings
//
//  Holds everything the RMG needs to synthesise a map: the amount of
//  tiberium to seed, the vegetation density range, the per-theater ambient
//  light and the ore patch lamp animations.  The values come from the
//  [General] section of RMGMD.INI.
// ============================================================================

class MapSeedClass
{
public:
    MapSeedClass();
    ~MapSeedClass();

    bool LoadFromINI(class CCINIClass* pINI);

    // Loads a generated map's [RandomMap] block.  The filename is opened as
    // a file, parsed and its settings merged over the current values; an
    // empty or unreadable name is a no-op.
    bool LoadMission(const char* pFileName);

    // ------------------------------------------------------------------
    // Random map parameters ([RandomMap] of the generated map)
    // ------------------------------------------------------------------
    int32 Width;
    int32 Height;
    int32 NumPlayers;
    int32 Seed;
    int32 MapType;
    int32 Theater;
    int32 Time;
    int32 RegionSize;
    int32 Ruggedness;
    int32 Accessibility;
    int32 WaterAmount;
    int32 Tiberium;
    int32 TiberiumLayout;
    int32 Vegetation;
    int32 UrbanPresence;
    int32 Resources;
    char  Description[0x80];

    // ------------------------------------------------------------------
    // Tiberium seeding
    // ------------------------------------------------------------------
    int32 RMGMinimumTiberium;
    int32 RMGMaximumTiberium;

    // ------------------------------------------------------------------
    // Lighting / vegetation
    // ------------------------------------------------------------------
    DynamicVectorClass<int32>* RMGLevelLightSettings;
    DynamicVectorClass<int32>* RMGVegetationMinimums;
    DynamicVectorClass<int32>* RMGVegetationMaximums;

    DynamicVectorClass<int32>* TemperateAmbientLight;
    DynamicVectorClass<int32>* SnowAmbientLight;

    DynamicVectorClass<int32>* TemperateAmbientRed;
    DynamicVectorClass<int32>* TemperateAmbientGreen;
    DynamicVectorClass<int32>* TemperateAmbientBlue;

    DynamicVectorClass<int32>* SnowAmbientRed;
    DynamicVectorClass<int32>* SnowAmbientGreen;
    DynamicVectorClass<int32>* SnowAmbientBlue;

    // ------------------------------------------------------------------
    // Trees / lamps
    // ------------------------------------------------------------------
    int32 MaxTrees;
    char  TemperateOrePatchLamps[0x80];
    char  SnowOrePatchLamps[0x80];
};
