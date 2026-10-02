#include "MapSeedClass.h"
#include "../INI/INIClass.h"

#include <cstring>

// ============================================================
// MapSeedClass
// ============================================================

MapSeedClass::MapSeedClass()
    : Width(0), Height(0), NumPlayers(0), Seed(0)
    , MapType(0), Theater(0), Time(0), RegionSize(0)
    , Ruggedness(0), Accessibility(0), WaterAmount(0), Tiberium(0)
    , TiberiumLayout(0), Vegetation(0), UrbanPresence(0), Resources(0)
    , RMGMinimumTiberium(0), RMGMaximumTiberium(0)
    , RMGLevelLightSettings(new DynamicVectorClass<int32>())
    , RMGVegetationMinimums(new DynamicVectorClass<int32>())
    , RMGVegetationMaximums(new DynamicVectorClass<int32>())
    , TemperateAmbientLight(new DynamicVectorClass<int32>())
    , SnowAmbientLight(new DynamicVectorClass<int32>())
    , TemperateAmbientRed(new DynamicVectorClass<int32>())
    , TemperateAmbientGreen(new DynamicVectorClass<int32>())
    , TemperateAmbientBlue(new DynamicVectorClass<int32>())
    , SnowAmbientRed(new DynamicVectorClass<int32>())
    , SnowAmbientGreen(new DynamicVectorClass<int32>())
    , SnowAmbientBlue(new DynamicVectorClass<int32>())
    , MaxTrees(0)
{
    Description[0] = '\0';
    TemperateOrePatchLamps[0] = '\0';
    SnowOrePatchLamps[0] = '\0';
}

MapSeedClass::~MapSeedClass()
{
    delete RMGLevelLightSettings;
    delete RMGVegetationMinimums;
    delete RMGVegetationMaximums;

    delete TemperateAmbientLight;
    delete SnowAmbientLight;

    delete TemperateAmbientRed;
    delete TemperateAmbientGreen;
    delete TemperateAmbientBlue;

    delete SnowAmbientRed;
    delete SnowAmbientGreen;
    delete SnowAmbientBlue;

    RMGLevelLightSettings = nullptr;
    RMGVegetationMinimums = nullptr;
    RMGVegetationMaximums = nullptr;

    TemperateAmbientLight = nullptr;
    SnowAmbientLight = nullptr;

    TemperateAmbientRed = nullptr;
    TemperateAmbientGreen = nullptr;
    TemperateAmbientBlue = nullptr;

    SnowAmbientRed = nullptr;
    SnowAmbientGreen = nullptr;
    SnowAmbientBlue = nullptr;
}

// Reads [General] of RMGMD.INI.  The integer lists are read as comma
// separated vectors, so a single value, a list and an empty entry all
// behave the same way they do in the original.
bool MapSeedClass::LoadFromINI(CCINIClass* pINI)
{
    if (!pINI)
        return false;

    const char* section = "General";

    if (pINI->GetSection(section) == nullptr)
        return false;

    RMGMinimumTiberium = pINI->ReadInteger(section, "RMGMinimumTiberium", RMGMinimumTiberium);
    RMGMaximumTiberium = pINI->ReadInteger(section, "RMGMaximumTiberium", RMGMaximumTiberium);

    RMGLevelLightSettings->Clear();
    pINI->GetVectorIntegers(section, "RMGLevelLightSettings", *RMGLevelLightSettings);

    RMGVegetationMinimums->Clear();
    pINI->GetVectorIntegers(section, "RMGVegetationMinimums", *RMGVegetationMinimums);

    RMGVegetationMaximums->Clear();
    pINI->GetVectorIntegers(section, "RMGVegetationMaximums", *RMGVegetationMaximums);

    TemperateAmbientLight->Clear();
    pINI->GetVectorIntegers(section, "TemperateAmbientLight", *TemperateAmbientLight);

    SnowAmbientLight->Clear();
    pINI->GetVectorIntegers(section, "SnowAmbientLight", *SnowAmbientLight);

    TemperateAmbientRed->Clear();
    pINI->GetVectorIntegers(section, "TemperateAmbientRed", *TemperateAmbientRed);

    TemperateAmbientGreen->Clear();
    pINI->GetVectorIntegers(section, "TemperateAmbientGreen", *TemperateAmbientGreen);

    TemperateAmbientBlue->Clear();
    pINI->GetVectorIntegers(section, "TemperateAmbientBlue", *TemperateAmbientBlue);

    SnowAmbientRed->Clear();
    pINI->GetVectorIntegers(section, "SnowAmbientRed", *SnowAmbientRed);

    SnowAmbientGreen->Clear();
    pINI->GetVectorIntegers(section, "SnowAmbientGreen", *SnowAmbientGreen);

    SnowAmbientBlue->Clear();
    pINI->GetVectorIntegers(section, "SnowAmbientBlue", *SnowAmbientBlue);

    MaxTrees = pINI->ReadInteger(section, "MaxTrees", MaxTrees);

    pINI->ReadString(section, "TemperateOrePatchLamps", "",
                     TemperateOrePatchLamps, sizeof(TemperateOrePatchLamps));
    pINI->ReadString(section, "SnowOrePatchLamps", "",
                     SnowOrePatchLamps, sizeof(SnowOrePatchLamps));

    TemperateOrePatchLamps[sizeof(TemperateOrePatchLamps) - 1] = '\0';
    SnowOrePatchLamps[sizeof(SnowOrePatchLamps) - 1] = '\0';

    return true;
}

// ============================================================================
// LoadMission - MapSeedClass_LoadMission
//
//   Opens a generated map file and merges its [RandomMap] block over the
//   current settings.  Every key keeps the value already held as its
//   fallback, so re-loading a partially specified map preserves the rest.
//   A null or unreadable filename is a no-op, as in the original.
// ============================================================================
bool MapSeedClass::LoadMission(const char* pFileName)
{
    if (pFileName == nullptr || pFileName[0] == '\0')
        return false;

    CCINIClass* pINI = CCINIClass::LoadINIFile(pFileName);
    if (pINI == nullptr)
        return false;

    CCINIClass& ini = *pINI;

    static const char* const SECTION = "RandomMap";

    char description[0x80];
    description[0] = '\0';
    ini.ReadString(SECTION, "Description", "", description, sizeof(description));
    std::strncpy(Description, description, sizeof(Description) - 1);
    Description[sizeof(Description) - 1] = '\0';

    Width         = ini.ReadInteger(SECTION, "Width",         Width);
    Height        = ini.ReadInteger(SECTION, "Height",        Height);
    NumPlayers    = ini.ReadInteger(SECTION, "NumPlayers",    NumPlayers);
    Seed          = ini.ReadInteger(SECTION, "Seed",          Seed);
    MapType       = ini.ReadInteger(SECTION, "MapType",       MapType);
    Theater       = ini.ReadInteger(SECTION, "Theater",       Theater);
    Time          = ini.ReadInteger(SECTION, "Time",          Time);
    RegionSize    = ini.ReadInteger(SECTION, "RegionSize",    RegionSize);
    Ruggedness    = ini.ReadInteger(SECTION, "Ruggedness",    Ruggedness);
    Accessibility = ini.ReadInteger(SECTION, "Accessibility", Accessibility);
    WaterAmount   = ini.ReadInteger(SECTION, "WaterAmount",   WaterAmount);
    Tiberium      = ini.ReadInteger(SECTION, "Tiberium",      Tiberium);
    TiberiumLayout = ini.ReadInteger(SECTION, "TiberiumLayout", TiberiumLayout);
    Vegetation    = ini.ReadInteger(SECTION, "Vegetation",    Vegetation);
    UrbanPresence = ini.ReadInteger(SECTION, "UrbanPresence", UrbanPresence);
    Resources     = ini.ReadInteger(SECTION, "Resources",     Resources);

    return true;
}
