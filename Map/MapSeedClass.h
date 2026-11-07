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

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知随机地图生成面板的原版命名形态：总生成入口按
    // 地图类型分派阶段机（内陆/群岛/岛屿/大陆/团队大陆五种水域分布 +
    // 城区铺设）；界面字符串键与任务文件管理是固定槽位。
    // ------------------------------------------------------------------------
    bool Generate();
    bool Generate_InitRandomMap();

    // ------------------------------------------------------------------------
    // asm: Place_Trees (0x5A4280 区, thiscall; 密度 double 取自调用者帧 ebp+28h，
    //   闸门字节取自 ebp+45h，此处显式作参数)
    //   ① 清 scratch 单元的游程字段（cell*50h+3Ch）；
    //   ② 逐格：空地 && [+11Ch]==0 && 闸门通过 && 随机数小于密度 → 种一块
    //      "粗糙地块"（抖动夹在 [4,60]）；
    //   ③ 逐格重算 LAT；
    //   ④ 按 ([+64h]*0.1+0.7)*([+5Ch]*0.01)*[+2FCh] 求棵数，用拒绝采样把
    //      树撒到空格上（每次尝试最多 200 次落点、最多 100 轮）。
    // ------------------------------------------------------------------------
    void Place_Trees(double density, uint8 gate);
    void Generate_SeedWater_Inland();
    void Generate_SeedWater_Archipelago();
    void Generate_SeedWater_Islands();
    void Generate_SeedWater_Continent();
    void Generate_SeedWater_TeamContinent();
    void Generate_PlaceUrbanAreas();
    void AddTechBuildings();
    const wchar_t* GetUIString_Load() const;
    const wchar_t* GetUIString_Save() const;
    const wchar_t* GetUIString_Saved() const;
    const wchar_t* GetUIString_Delete() const;
    bool LoadRandomMapDescription();
    bool SaveMission(const char* pName, const wchar_t* pTitle);
    static bool DeleteMission(const char* pFileName);
    static int32 DialogFunc_SetData();
    static int32 DialogFunc_GetData();
    static int32 DialogFunc();

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
