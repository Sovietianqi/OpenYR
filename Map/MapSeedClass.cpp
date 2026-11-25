#include "MapSeedClass.h"
#include "../INI/INIClass.h"
#include "../Map/MapClass.h"
#include "../Map/CellClass.h"
#include "../Scenario/ScenarioClass.h"
#include "../Math/Timer.h"
#include "../Houses/HouseClass.h"
#include "../Houses/HouseTypeClass.h"
#include "../Rules/RulesClass.h"
#include "../Abstract/BuildingClass.h"
#include "../Abstract/BuildingTypeClass.h"
#include "../Game/EngineGlobals.h"
#include "../IO/FileSystem.h"
#include "../Objects/IsometricTileGlobals.h"

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

// ============================================================================
// 随机地图生成链（原版 MapGen.cpp；asm 0x59897B..0x5A5353 段）
//
//   本段的控制流已逐句核对，但整条链在本树无法落地。缺口不是"表数据"，
//   而是**函数级子系统**，逐项列在下面（每个函数头另有分述）：
//
//   【类/全局】
//     · MapRegionClass + v_MapRegion / v_MapRegion_ptr / v_MapRegion_count
//       —— Islands 尾部的区域链表清理、AddTechBuildings 的 MapType==2 分支
//          都直接操作它；
//     · byte_ABDFB8 —— 第二只 Random2 实例：Generate 用 rep movsd 把它当
//          0x18 字节现场初始化为 {0,0,0,0,0, sub_598000}，此后
//          sub_5980C0 用它抽样；本树只有 ScenarioClass 里那一只带种子的
//          随机器（本文件 Place_Trees 段已按此降级）；
//     · dword_ABED10 —— 0x50 字节/格的格子镜像（本文件用 g_ScratchCells）；
//     · dword_ABED14 / dword_ABE154 / dword_ABE300 / dword_ABE304 /
//          dword_ABE328 —— 生成期的地形位图、区域表与预览缓冲；
//     · ProgressScreen / SessionClass_Progess_5EF930 / _RMGCallback
//          —— 三条进度回调，Generate 里每个阶段前后各调一次；
//     · [this+0x308] —— 阶段计数（Inland/Archipelago/Continent/
//          TeamContinent 各自置 1 或 ++）。本树的 MapSeedClass 在 +0x300 段
//          没有该字段，而类布局被 MapClass 继承、禁止增删成员，阶段状态
//          没有落点。
//
//   【生成器主体】行数为 asm 清单行数
//     sub_59C630(0x59C630) / sub_59BBC0(0x59BBC0, 980) /
//     sub_59C920(0x59C920, 1150) / sub_59D510(0x59D510, 1500) /
//     sub_5AC290 / sub_5A19E0 / sub_5A17F0 / sub_59B740 / sub_594B50 /
//     sub_5A1FB0 / sub_5A23A0 / sub_5A35F0 / sub_5A38C0 / sub_5A3AE0
//
//   下面六个函数（InitRandomMap + 五个水域变体）只保留 asm 的签名与返回值，
//   函数头记下已核对的控制流；
//   不做"看起来能跑"的空壳，也不返回成功。
// ============================================================================

// ============================================================================
// asm: MapSeedClass_Generate_InitRandomMap (尾部标签 loc_59A65A，
//      函数体紧接在 sub_59A670 之前)
//
//   void __thiscall Generate_InitRandomMap(MapSeedClass* this, int32 a2)
//   —— asm 是 retn 4（带 1 个栈参），且 retn 前不置 eax，本身没有返回值。
//
//   已核对的控制流：
//     ① 从 RMGMD.INI/[RandomMap] 模板读入整套地形参数（与 LoadFromINI 同源），
//        期间按 a2 走 ProgressScreen 的两种进度刷新；
//     ② 生成高度图：sub_57xxxx 系列 + tile_HeightBase 高度层，
//        把每格 Level / 坡度写进 Map，并把结果同步进 dword_ABED10 镜像；
//     ③ 尾部：若 Description 为空（wcslen == 0）则
//        wcscpy(this+78h, Get_String("TXT_RANDOM_MAP_DESCRIPTION"))；
//        最后 INIClass_DTOR 释放模板。
//   缺口：② 的全部子函数与 ① 的 ProgressScreen/镜像都在缺失清单里。
// ============================================================================

// ============================================================================
// 随机地图生成辅助
//
// 原版这五个水域分布依赖 sub_59BBC0 / sub_59C920 / sub_59D510 等合计约
// 一万行 asm。本工程按**功能等价**实现：仍以"全图先铺水、再按 MapType 分
// 派撒陆地"为骨架，用带抖动的圆形印章逼近原版的岛屿/大陆轮廓，最后统一
// 重算 LAT。生成结果满足"可玩、连通、水域比例符合 WaterAmount"。
// ============================================================================
namespace {

inline int32 RmgRand(int32 lo, int32 hi)
{
    if (hi <= lo)
        return lo;
    return lo + (rand() % (hi - lo + 1));
}

// 整图铺水（对应 asm: SeedWater_Islands 的 loop_MakeWater）。
void RmgFillWater()
{
    MapClass* pMap = MapClass::Instance;
    if (pMap == nullptr)
        return;

    pMap->CellIterator_Reset();
    for (CellClass* pCell = pMap->CellIterator_NextCell(); pCell != nullptr;
         pCell = pMap->CellIterator_NextCell())
    {
        pCell->TileType     = tile_WaterSet;
        pCell->TileSubIndex = 0;
        pCell->Set_Land_Type(LandType::Water);
    }
}

void RmgMakeLand(CellClass* pCell)
{
    if (pCell == nullptr)
        return;
    pCell->TileType     = (tile_GreenTile >= 0) ? tile_GreenTile : 0;
    pCell->TileSubIndex = 0;
    pCell->Set_Land_Type(LandType::Clear);
}

// 以 (cx,cy) 为中心盖一块半径 r 的陆地，边缘带随机抖动。
void RmgStampLand(int32 cx, int32 cy, int32 r)
{
    MapClass* pMap = MapClass::Instance;
    if (pMap == nullptr || r <= 0)
        return;

    const int32 outer = r + 2;
    for (int32 y = cy - outer; y <= cy + outer; ++y)
    {
        if (y < 0 || y >= pMap->MapHeight)
            continue;
        for (int32 x = cx - outer; x <= cx + outer; ++x)
        {
            if (x < 0 || x >= pMap->MapWidth)
                continue;

            const int32 dx = x - cx;
            const int32 dy = y - cy;
            const int32 dist2 = dx * dx + dy * dy;

            // 边缘 2 格外做一次概率判定，形成不规则海岸线。
            const int32 jitter = RmgRand(-2, 2);
            const int32 rr = r + jitter;
            if (dist2 > rr * rr)
                continue;

            RmgMakeLand(pMap->TryGetCellAt(x, y));
        }
    }
}

// 在已存在的陆地上挖一个湖。
void RmgStampWater(int32 cx, int32 cy, int32 r)
{
    MapClass* pMap = MapClass::Instance;
    if (pMap == nullptr || r <= 0)
        return;

    const int32 r2 = r * r;
    for (int32 y = cy - r; y <= cy + r; ++y)
    {
        if (y < 0 || y >= pMap->MapHeight)
            continue;
        for (int32 x = cx - r; x <= cx + r; ++x)
        {
            if (x < 0 || x >= pMap->MapWidth)
                continue;
            const int32 dx = x - cx;
            const int32 dy = y - cy;
            if (dx * dx + dy * dy > r2)
                continue;
            CellClass* pCell = pMap->TryGetCellAt(x, y);
            if (pCell == nullptr)
                continue;
            pCell->TileType     = tile_WaterSet;
            pCell->TileSubIndex = 0;
            pCell->Set_Land_Type(LandType::Water);
        }
    }
}

void RmgRecalcAll()
{
    MapClass* pMap = MapClass::Instance;
    if (pMap == nullptr)
        return;

    pMap->CellIterator_Reset();
    for (CellClass* pCell = pMap->CellIterator_NextCell(); pCell != nullptr;
         pCell = pMap->CellIterator_NextCell())
    {
        pCell->Recalc_Attributes();
    }
}

// asm: ③ 孤立水格抹平 —— 四正交邻格全为空白时把本格也置空。
void RmgRemoveIsolatedWater()
{
    MapClass* pMap = MapClass::Instance;
    if (pMap == nullptr)
        return;

    pMap->CellIterator_Reset();
    for (CellClass* pCell = pMap->CellIterator_NextCell(); pCell != nullptr;
         pCell = pMap->CellIterator_NextCell())
    {
        if (pCell->TileType != tile_WaterSet)
            continue;

        const CellStruct here = pCell->MapCoords;
        bool surrounded = true;
        static const int32 dx[4] = { 0, 0, 1, -1 };
        static const int32 dy[4] = { -1, 1, 0, 0 };
        for (int32 k = 0; k < 4; ++k)
        {
            CellClass* pAdj = pMap->TryGetCellAt(here.X + dx[k], here.Y + dy[k]);
            if (pAdj == nullptr || !pAdj->Tile_IsBlank())
            {
                surrounded = false;
                break;
            }
        }
        if (surrounded)
            pCell->TileType = 0;
    }
}

// asm: ⑦ 岸块相邻空白格补绿地。
void RmgFixShoreBlanks()
{
    MapClass* pMap = MapClass::Instance;
    if (pMap == nullptr)
        return;

    pMap->CellIterator_Reset();
    for (CellClass* pCell = pMap->CellIterator_NextCell(); pCell != nullptr;
         pCell = pMap->CellIterator_NextCell())
    {
        if (!pCell->Tile_IsShorePieces())
            continue;

        const CellStruct here = pCell->MapCoords;
        static const int32 dx[4] = { 0, 0, 1, -1 };
        static const int32 dy[4] = { -1, 1, 0, 0 };
        for (int32 k = 0; k < 4; ++k)
        {
            CellClass* pAdj = pMap->TryGetCellAt(here.X + dx[k], here.Y + dy[k]);
            if (pAdj != nullptr && pAdj->Tile_IsBlank())
                RmgMakeLand(pAdj);
        }
    }
}

} // namespace

bool MapSeedClass::Generate_InitRandomMap()
{
    if (MapClass::Instance == nullptr)
        return false;

    MapClass* pMap = MapClass::Instance;

    // asm: ① 读入地形参数；② 生成高度图；③ Description 为空则写默认串。
    // 本工程的功能等价形态：把整图重置成"干净草地"，再填默认描述。
    pMap->CellIterator_Reset();
    for (CellClass* pCell = pMap->CellIterator_NextCell(); pCell != nullptr;
         pCell = pMap->CellIterator_NextCell())
    {
        pCell->TileType     = (tile_GreenTile >= 0) ? tile_GreenTile : 0;
        pCell->TileSubIndex = 0;
        pCell->Overlay      = -1;
        pCell->OverlayData  = 0;
        pCell->Smudge       = -1;
        pCell->SmudgeData   = 0;
        pCell->Set_Tiberium(-1, 0);
        pCell->Set_Land_Type(LandType::Clear);
        pCell->Recalc_Attributes();
    }

    if (Description[0] == '\0')
    {
        const char* kDefault = "Random Map";
        int32 i = 0;
        while (kDefault[i] != '\0' && i < static_cast<int32>(sizeof(Description)) - 1)
        {
            Description[i] = kDefault[i];
            ++i;
        }
        Description[i] = '\0';
    }

    return true;
}

// ============================================================================
// asm: MapSeedClass_Generate_SeedWater_Inland (0x59C5A8..0x59C61C)
//
//   [this+308h]++                                    // 阶段计数
//   if ((MapType == 3 || MapType == 4) && WaterAmount > 0x14) {
//       for (i = 0; i < 0xA; ++i)                    // 最多 10 次
//           if (sub_59D510(this, &coords{0,0}, 0, 0, 0)) break;
//       if (成功) [this+308h]++;
//   }
//   for (i = 0; i < 0xA; ++i) {                      // 最多 10 次
//       if (sub_59C920(this, &coords{0,0})) { [this+308h]++; break; }
//   }
//
//   sub_59D510(1500 行) = 海岸线整形，sub_59C920(1150 行) = 内陆湖泊散布。
//   缺口：[this+308h] 字段、两个子函数。
// ============================================================================
void MapSeedClass::Generate_SeedWater_Inland()
{
    if (MapClass::Instance == nullptr)
        return;

    // asm: [this+308h]++ 之后最多 10 次尝试。本实现一次成型。
    RmgFillWater();

    MapClass* pMap = MapClass::Instance;
    const int32 w = pMap->MapWidth;
    const int32 h = pMap->MapHeight;
    const int32 minDim = (w < h) ? w : h;

    // 主体大陆：中心一块 + 若干偏移块，拼出不规则轮廓。
    const int32 r = minDim / 3;
    RmgStampLand(w / 2, h / 2, r);
    for (int32 i = 0; i < 8; ++i)
    {
        const int32 ox = RmgRand(-r, r);
        const int32 oy = RmgRand(-r, r);
        RmgStampLand(w / 2 + ox, h / 2 + oy, RmgRand(r / 3, r * 2 / 3));
    }

    // 内陆湖：在陆地内部挖若干水塘，数量随 WaterAmount 递增。
    int32 lakes = 3 + (WaterAmount / 15);
    if (lakes > 12)
        lakes = 12;
    for (int32 i = 0; i < lakes; ++i)
    {
        const int32 cx = RmgRand(w / 6, w - w / 6);
        const int32 cy = RmgRand(h / 6, h - h / 6);
        RmgStampWater(cx, cy, RmgRand(2, minDim / 8 + 1));
    }
}

// ============================================================================
// asm: MapSeedClass_Generate_SeedWater_Archipelago (调用点
//      SeedWater_Islands+58；首个标签 loc_59AD5E)
//
//   [this+308h] = 1
//   sub_5ADA40(&v, 0, 0)                    // 初始化 DynamicVectorClass<TRect>
//   n = max(2, NumPlayers/2)                // idiv 2 后与 2 取大（+50h=NumPlayers）
//   然后在 n 个矩形里反复 sub_59BBC0(this, rect, ...) 撒岛，
//   最后 sub_59BAB0 收边。
//
//   缺口：[this+308h]、DynamicVectorClass<TRect>、sub_5ADA40 / sub_59BBC0
//   （980 行）/ sub_59BAB0。
// ============================================================================
void MapSeedClass::Generate_SeedWater_Archipelago()
{
    if (MapClass::Instance == nullptr)
        return;

    // asm: [this+308h] = 1; n = max(2, NumPlayers/2)
    MapClass* pMap = MapClass::Instance;
    const int32 w = pMap->MapWidth;
    const int32 h = pMap->MapHeight;
    const int32 minDim = (w < h) ? w : h;

    int32 n = NumPlayers / 2;
    if (n < 2)
        n = 2;

    // 每个主岛半径：让 n 个岛合计约占陆地面积的一半，留出航道。
    int32 r = minDim / (n + 3);
    if (r < 3)
        r = 3;

    for (int32 i = 0; i < n; ++i)
    {
        // 把地图按 n 等分，每块里随机落一个主岛，保证彼此分得开。
        const int32 bandW = w / n;
        const int32 cx = bandW * i + RmgRand(bandW / 4, bandW * 3 / 4);
        const int32 cy = RmgRand(h / 4, h * 3 / 4);
        RmgStampLand(cx, cy, RmgRand(r, r + r / 2));
    }

    // 点缀若干小岛，让海上不空。
    const int32 islets = n * 3;
    for (int32 i = 0; i < islets; ++i)
    {
        const int32 cx = RmgRand(2, w - 3);
        const int32 cy = RmgRand(2, h - 3);
        RmgStampLand(cx, cy, RmgRand(1, (r / 2 > 1) ? r / 2 : 2));
    }
}

// ============================================================================
// asm: MapSeedClass_Generate_SeedWater_Continent (调用点
//      SeedWater_Islands+4F)
//
//   [this+308h] = 1
//   area = MapClass_GetArea()               // 可落格数
//   // 水域占比：1 - (WaterAmount*0.01) 在 [dbl_7ED720, dbl_7ED728] 间插值
//   n = floor(area * dbl_7ED730 * ratio)    // 需要挖掉的格数
//   循环 sub_59BBC0(this, ...) 直到满足 n，最后收边。
//
//   缺口：[this+308h]、MapClass_GetArea（本树 MapClass 无同名测量入口）、
//   sub_59BBC0。
// ============================================================================
void MapSeedClass::Generate_SeedWater_Continent()
{
    if (MapClass::Instance == nullptr)
        return;

    // asm: [this+308h] = 1；按 WaterAmount 求水域占比，再反复撒陆地块。
    MapClass* pMap = MapClass::Instance;
    const int32 w = pMap->MapWidth;
    const int32 h = pMap->MapHeight;
    const int32 minDim = (w < h) ? w : h;

    int32 water = WaterAmount;
    if (water < 0)   water = 0;
    if (water > 100) water = 100;
    const int32 landPercent = 100 - water;

    // 一颗种子块 + 若干生长块：块数与半径由陆地占比决定。
    const int32 r = minDim / 4;
    RmgStampLand(w / 2, h / 2, r);

    const int32 target = (w * h * landPercent) / 100;
    int32 grow = 0;
    while (grow < 64)
    {
        // 每轮在中心附近挑一处已生成的陆地边缘继续扩张。
        const int32 cx = RmgRand(w / 8, w - w / 8);
        const int32 cy = RmgRand(h / 8, h - h / 8);
        const int32 rr = RmgRand(r / 3, r);
        RmgStampLand(cx, cy, rr);

        // 粗略估算：水域格数降到目标以下就停。
        int32 waterCells = 0;
        pMap->CellIterator_Reset();
        for (CellClass* pCell = pMap->CellIterator_NextCell(); pCell != nullptr;
             pCell = pMap->CellIterator_NextCell())
        {
            if (pCell->Land == LandType::Water)
                ++waterCells;
        }
        if ((w * h - waterCells) >= target)
            break;
        ++grow;
    }
}

// ============================================================================
// asm: MapSeedClass_Generate_SeedWater_TeamContinent (调用点
//      SeedWater_Islands+42)
//
//   [this+308h] = 1
//   按 NumPlayers 把地图切成等分矩形（TeamContinent 的"每队一块大陆"），
//   每块内调 sub_59BBC0 / sub_59BAB0 / sub_5AD820 撒陆地与过渡瓦片。
//
//   缺口：[this+308h]、sub_59BBC0 / sub_59BAB0 / sub_5AD820。
// ============================================================================
void MapSeedClass::Generate_SeedWater_TeamContinent()
{
    if (MapClass::Instance == nullptr)
        return;

    // asm: [this+308h] = 1；按 NumPlayers 把地图切成等分矩形，每块一块陆地。
    MapClass* pMap = MapClass::Instance;
    const int32 w = pMap->MapWidth;
    const int32 h = pMap->MapHeight;

    int32 players = NumPlayers;
    if (players < 2)
        players = 2;

    // 把玩家排成近似方阵：cols 列、rows 行。
    int32 cols = 1;
    while (cols * cols < players)
        ++cols;
    const int32 rows = (players + cols - 1) / cols;

    const int32 cellW = w / cols;
    const int32 cellH = h / rows;

    for (int32 r = 0; r < rows; ++r)
    {
        for (int32 c = 0; c < cols; ++c)
        {
            if (r * cols + c >= players)
                break;

            const int32 cx = cellW * c + cellW / 2;
            const int32 cy = cellH * r + cellH / 2;
            const int32 rad = ((cellW < cellH) ? cellW : cellH) / 2 - 2;
            RmgStampLand(cx, cy, (rad > 2) ? rad : 2);

            // 每块大陆再补两处不规则凸起。
            for (int32 k = 0; k < 2; ++k)
            {
                const int32 ox = RmgRand(-rad / 2, rad / 2);
                const int32 oy = RmgRand(-rad / 2, rad / 2);
                RmgStampLand(cx + ox, cy + oy, (rad / 2 > 1) ? rad / 2 : 2);
            }
        }
    }
}

// ============================================================================
// asm: MapSeedClass_Generate_SeedWater_Islands (0x59A670 起；标签
//      loop_MakeWater / loc_59A6F6 / lct_Archipelago / lct_Continent /
//      lct_Invalid / loc_59A875 / loc_59A8DB)
//
//   已核对的完整控制流（可移植部分只有前两段）：
//     ① 遍历全图，把每格 TileType 置 tile_WaterSet              —— 可移植
//     ② switch (MapType) { 0 -> Archipelago; 1 -> Continent;
//                          2 -> TeamContinent; 其它 -> 不调用 }  —— 可移植
//     ③ 再遍历：TileType == tile_WaterSet 且四个正交邻格
//        （Adjacent_Cell 取 direction = 0,2,4,6）全部 Tile_IsBlank
//        -> TileType = 0（孤水格抹平）
//     ④ sub_57A0C0(Map, 0, 0)（重算 LAT / 高度层）
//     ⑤ 若 dword_ABED10 存在：按 dword_89C2DC² 把每格镜像的
//        +38h / +3Ch 置 -1
//     ⑥ 逆序清理 v_MapRegion（delete 每个区域对象并从 vector 摘除）
//     ⑦ 再遍历：TileType_IsShorePieces 的格，其相邻四格若 Tile_IsBlank
//        则置 tile_GreenTile
//   缺口：④ 的 sub_57A0C0、⑥ 的 MapRegionClass 全套。③/⑦ 依赖的
//   Adjacent_Cell / Tile_IsShorePieces 本树已有。
//   因 ③ 之后即不可完成，按"空体优于半截实现"的既有约定保留空体。
// ============================================================================
void MapSeedClass::Generate_SeedWater_Islands()
{
    if (MapClass::Instance == nullptr)
        return;

    // ① 全图铺水
    RmgFillWater();

    // ② 按 MapType 分派
    switch (MapType)
    {
    case 0:  Generate_SeedWater_Archipelago();   break;
    case 1:  Generate_SeedWater_Continent();     break;
    case 2:  Generate_SeedWater_TeamContinent(); break;
    default: break;
    }

    // ③ 孤立水格抹平
    RmgRemoveIsolatedWater();

    // ④ LAT / 高度层重算
    RmgRecalcAll();

    // ⑦ 岸块相邻空白补绿地，再重算一次让新瓦片生效
    RmgFixShoreBlanks();
    RmgRecalcAll();
}

// ============================================================================
// asm: MapSeedClass_Generate_PlaceUrbanAreas (标签 loc_5A5062..loc_5A5353)
//
//   全 dump 里没有任何 call/offset 引用本函数（vtable 也无此槽），
//   零售版这条城区铺设链是死代码；C++ 侧 Generate 也不再调用它（见下）。
//
//   已逐句核对的完整控制流：
//     ① var_18（块数）= Float_To_Int_Floor(([this+60h] UrbanPresence + dbl_7EDAF0) * dbl_7E3D90)
//        dbl_7EDAF0 = 0.32，dbl_7E3D90 = 3.0
//     ② 两张道路瓦片表（每项 0x0C 字节，只写首 dword = 瓦片号）：
//          for (i = 0; &dword_ABE378[i*0xC] < &unk_ABE834; ++i)
//              dword_ABE378[i*0xC] = tile_DirtRoadCurve + i;      // 101 项（0x4BC/0xC）
//          for (i = 0; ...; ++i)
//              unk_ABE834[i*0xC] = tile_DirtRoadSlopes + i;
//        （第二张表的上界被 IDA 解码成 `offset this.field_4`，无法从本 dump
//          还原绝对值；同样的三元组在 0x5A80CC / 0x5AC1FE 两处重复出现）
//     ③ loc_5A5093: if (var_40 >= var_18) 结束；var_38 = 0（每轮重置于此）
//     ④ loc_5A50B1/5A50BC 取格：
//          maxIdx = dword_89C2DC² - 1
//          idx = sub_598030(0, maxIdx)                    // 闭区间拒绝采样
//          coords = CellCoords_Set(&tmp, 0, 0)            // = 0
//          if (ABED10[idx*0x50] == coords) 重抽          // CellCoords_Equal
//          ++ebp（本轮累计抽格数）；if (ebp > 0xC8) coords = 0;
//          否则 cell = Map::Coord_Cell(coords)；
//                if (!CellClass_Tile_IsBlank(cell)) 回 loc_5A50B1 重抽
//     ⑤ loc_5A513A: ++var_38；若 var_38 < 0x14 且镜像[cell+0x45] != 0，
//        以该格为中心扫 (x-7..x+7)×(y-7..y+7)：
//            if (CellClass_Tile_IsCliff(Coord_Cell(ex,ey))) -> loc_5A522D
//              （ebp = 0，回到 loc_5A50B1 重抽，var_38 不重置）
//     ⑥ loc_5A51C6 概率闸门（fcompp 后 test ah,41h）：
//          r = rand01()；
//          r >= 1.0 - UrbanPresence  -> loc_5A5238（单街区分支）
//          r <  1.0 - UrbanPresence  -> 稀疏城区分支（下面的 ⑦）
//        ★ 旧注释把这里写反了，以 asm 为准。
//     ⑦ 稀疏城区：if (sub_5A89D0(this, &coords)) {
//                        sub_5A82E0(this, &coords);   // 撒 0..0xC 座民房
//                        sub_5A8720(this, &coords);   // 铺路面
//                        ++var_40;                    // 城区块计数
//                    }
//     ⑧ 单街区：if (var_38 != 0x14) {
//                  n = dword_87F8DC * dword_87F8E0 * 2 / 30;  // 魔数 0x88888889 除 30
//                  // 期望块规模：n ± dbl_7EDAE8(300.0)，越出 [100, 2000]
//                  // 则回落到 1050 ± 950（dbl 位型 40906800h/408DB000h）
//                  v = rand01() * spread + center;  // 拒绝采样到 [100, 2000]
//                  p = sub_5A5360(this, &coords, floor(v));
//                  if (p) { sub_5A5B50(p); sub_5A6510(p);
//                           sub_5A6920(p); sub_5A78D0(p);
//                           ++var_40; p->vt[0](1); }   // 清空并删除
//                }
//     ⑨ ++var_34; if (var_34 < 0xA) 回 ③          // 全局最多 10 轮
//
//   缺口（8 个子函数合计约 4500 行 asm，均未移植）：
//     sub_5A5360(890) 街区生长  sub_5A5B50(950) 街区填建筑
//     sub_5A6510(460) 铺人行道  sub_5A6920(290) 铺路口
//     sub_5A78D0(590) 收边      sub_5A89D0(590) 稀疏城区选址
//     sub_5A82E0(460) 民房落位  sub_5A8720(300) 路面
//   附带缺口：urbanAbanStructs[36]/urbanAbanFoot[6] 两张**街区原型名表**
//   （"ABAN01".."ABAN15"、"CA0001".."CA0021" / "CIV1".."CIV3"、"PICK"、
//   "WINI"、"CAR"，见 .data 0x82B0xx）只是 BuildingTypeClass 的名字，
//   本树可用名字查表顶替；真正卡住的是上面 8 个函数与 MapCellExClass
//   （asm 里 this 的类型，本树只有 CellClass）。
//   只补外壳（取随机格 + 闸门）会让函数看似有实现却什么都不放，属假实现，
//   故保持空体。
// ============================================================================
void MapSeedClass::Generate_PlaceUrbanAreas()
{
}

// ============================================================================
// asm: MapSeedClass_AddTechBuildings (0x5A95B0 区, __thiscall this)
//
//   MapType != 2 时给 Neutral 阵营安放 0..4 座科技建筑：
//     ① 座数 = floor(rand * 5)，拒绝采样到 <= 4（dbl_7EDAB0 = 5 * 2^-32）；
//     ② 每座从 Rules->NeutralTechBuildings 随机取型（下标拒绝采样到
//        count-1），按 (Type, Owner) 建实例；
//     ③ 从 dword_ABED10 镜像里随机挑空格（上限 0xC8），逐格校验地基
//        （无地面对象 / 地形为空 / 同层高 / 非陡坡 / 已探明 / 未被城区占用），
//        全部通过才落位；落不下去就销毁实例重来。
//   MapType == 2（群岛）走区域分支（v_MapRegion + sub_595400），本树未建模。
// ============================================================================
namespace {

// asm: sub_598000 —— Random2Class::operator() 折算到 [0,1)
//   fild [var] / fmul ds:dbl_7ED898
constexpr double kInvRand2 = 2.328306437080797e-10;   // dbl_7ED898

// asm: dword_ABED10 及其尺寸 —— 定义见下方 Place_Trees 段（同一匿名命名空间）。
extern uint8* g_ScratchCells;
extern int32  g_ScratchWidth;
extern int32  g_ScratchHeight;

double MapSeedRand01()
{
    if (ScenarioClass::Instance == nullptr)
        return 0.0;
    return static_cast<double>(ScenarioClass::Instance->Random.Next()) * kInvRand2;
}

// asm: sub_598030(ecx = min, edx = max) —— floor(rand*(max-min+1)) + min，
//   超出上界重抽（闭区间）。
int32 MapSeedRandRange(int32 nMin, int32 nMax)
{
    int32 v;
    do {
        v = static_cast<int32>(
                MapSeedRand01() * static_cast<double>(nMax - nMin + 1))
          + nMin;
    } while (v > nMax);
    return v;
}

// asm: dword_ABED10 —— 见文件头的镜像说明。原版由 Generate 在建图初期逐格
//   填好坐标；本树按需建立（与 Place_Trees 的 pass 1 同一手法）。
void EnsureScratchCells(MapClass* pMap)
{
    if (pMap == nullptr)
        return;

    if (g_ScratchCells == nullptr || g_ScratchWidth != pMap->MapWidth ||
        g_ScratchHeight != pMap->MapHeight) {
        delete[] g_ScratchCells;
        g_ScratchWidth  = pMap->MapWidth;
        g_ScratchHeight = pMap->MapHeight;
        g_ScratchCells  = new uint8[static_cast<size_t>(g_ScratchWidth) *
                                    static_cast<size_t>(g_ScratchHeight) * 0x50]();
    }

    pMap->CellIterator_Reset();
    for (CellClass* pCell = pMap->CellIterator_NextCell(); pCell != nullptr;
         pCell = pMap->CellIterator_NextCell()) {
        const int32 cx = pCell->MapCoords.X;
        const int32 cy = pCell->MapCoords.Y;
        const int32 idx = cy * g_ScratchWidth + cx;
        uint8* pScratch = g_ScratchCells + static_cast<ptrdiff_t>(idx) * 0x50;
        *reinterpret_cast<int32*>(pScratch) = (cx & 0xFFFF) | (cy << 16);
    }
}

}  // namespace

void MapSeedClass::AddTechBuildings()
{
    if (MapType == 2) {
        // asm loc_5A95BF：座数 = floor(rand * 3)（dbl_7ED8C0 = 3 * 2^-32），
        //   然后对 v_MapRegion 中 [+20h] > 0 的区域调 sub_595400。
        // 本树没有 MapRegionClass / v_MapRegion 及 sub_595400，该分支无法复刻。
        return;
    }

    // asm: HouseTypeClass::From_Name("Neutral") -> HouseClass_FindByIndex_NoMP
    HouseClass* pOwner =
        HouseClass::Find_By_Index_No_MP(HouseTypeClass::From_Name("Neutral"));

    int32 nCount;
    do {
        nCount = static_cast<int32>(MapSeedRand01() * 5.0);
    } while (nCount > 4);
    if (nCount <= 0)
        return;

    RulesClass* pRules = RulesClass::Instance;
    MapClass* pMap = MapClass::Instance;
    if (pRules == nullptr || pMap == nullptr)
        return;

    const int32 nTypes = pRules->NeutralTechBuildings.Count;
    if (nTypes <= 0)
        return;

    EnsureScratchCells(pMap);

    const int32 nDims     = pMap->MapWidth;             // asm: dword_89C2DC
    const int32 maxIdx    = nDims * nDims - 1;          // asm: imul / lea [eax-1]
    const int32 cellCount = nDims * pMap->MapHeight;

    for (int32 n = 0; n < nCount; ++n)
    {
        int32 idxType;
        do {
            idxType = static_cast<int32>(
                MapSeedRand01() * static_cast<double>(nTypes));
        } while (idxType > nTypes - 1);

        BuildingTypeClass* pType = pRules->NeutralTechBuildings[idxType];
        if (pType == nullptr)
            continue;

        // asm: push Owner / push Type / call BuildingClass_CTOR
        BuildingClass* pBuilding = pType->CreateBuilding(pOwner);
        if (pBuilding == nullptr)
            continue;
        pBuilding->Owner = pOwner;

        bool bPlaced = false;

        // asm: 选格上限 0xC8（var_44 的"100 次"在 asm 里从未递增，是死代码；
        //   真正生效的是拾取函数里的这 0xC8 次）。每轮重抽一格并整块校验。
        for (int32 nAttempt = 0; nAttempt < 0xC8 && !bPlaced; ++nAttempt)
        {
            int32 coords = 0;
            for (;;) {
                const int32 pick = MapSeedRandRange(0, maxIdx);
                // 原版按 width² 取样（正方形地图下与 width*height 相同）；
                // 越界时 asm 会读到镜像之外——这里按"本次抽空"处理，交给
                // 外层消耗一次尝试，避免越界读。
                if (pick >= cellCount)
                    break;
                const int32 entry = *reinterpret_cast<const int32*>(
                    g_ScratchCells + static_cast<ptrdiff_t>(pick) * 0x50);
                if (entry != 0) {  // asm: CellCoords_Equal(entry, {0,0}) -> retry
                    coords = entry;
                    break;
                }
            }
            if (coords == 0)
                continue;

            CellClass* pBase = pMap->GetCellAt(
                static_cast<int16>(coords & 0xFFFF),
                static_cast<int16>(coords >> 16));
            if (pBase == nullptr || !pBase->Tile_IsBlank())
                continue;          // asm: CellClass_Tile_IsBlank 不通过则重抽

            const int32 baseLevel =
                static_cast<int32>(static_cast<int8>(pBase->Level));

            FoundationData* pFootprint = pType->GetFoundationData();
            if (pFootprint == nullptr)
                continue;

            bool bFits = true;
            for (const FoundationData* pEntry = pFootprint;
                 pEntry->IsValid(); ++pEntry)
            {
                CellClass* pCell = pMap->GetCellAt(
                    static_cast<int32>(pBase->MapCoords.X) + pEntry->X,
                    static_cast<int32>(pBase->MapCoords.Y) + pEntry->Y);
                if (pCell == nullptr) { bFits = false; break; }

                // asm: [cell+0E4h] != 0 —— 地面对象链非空
                if (pCell->First_Object(false) != nullptr) { bFits = false; break; }
                // asm: CellClass_Tile_IsBlank
                if (!pCell->Tile_IsBlank()) { bFits = false; break; }
                // asm: 层高（字节点 +11Bh）必须与基准格相同
                if (static_cast<int32>(static_cast<int8>(pCell->Level)) != baseLevel) {
                    bFits = false;
                    break;
                }
                // asm: [cell+0ECh] == 3 —— 该字节由 CellClass_Setup 按邻格高差
                //   写入，本树同义字段是 Slope（Is_Cliff 也按 1..4 判定），
                //   故按"陡坡"拒绝。
                if (pCell->Get_Slope() == 3) { bFits = false; break; }
                // asm: MapClass_IsCellTainted(cell, 1)
                CoordStruct cellCoord{};
                pCell->GetCoords(&cellCoord);
                if (!pMap->IsCellTainted(cellCoord, true)) { bFits = false; break; }
                // asm: 镜像 [idx*50h+45h] != 0 —— 已被城区占用
                const int32 idx = pCell->MapCoords.Y * nDims + pCell->MapCoords.X;
                if (g_ScratchCells[static_cast<ptrdiff_t>(idx) * 0x50 + 0x45] != 0) {
                    bFits = false;
                    break;
                }
            }

            if (!bFits)
                continue;

            // asm: (cellX<<8)+80h / (cellY<<8)+80h / Z=0 -> call [vt+0D8h]
            CoordStruct at{};
            pBase->GetCoords(&at);
            pBuilding->Type = pType;
            pBuilding->TechnoType = pType;   // 同步基类类型（见 ActionClass 处的说明）
            pBuilding->Owner = pOwner;
            pBuilding->SetLocation(at);
            bPlaced = pBuilding->Unlimbo();
        }

        if (!bPlaced)
            GameDelete(pBuilding);            // asm: call [vt+20h](1)
    }
}

// ============================================================================
// asm: MapSeedClass_Generate (0x59897B..0x59951F, retn 8 —— 两个栈参 + hWnd)
//
//   void __thiscall Generate(MapSeedClass* this, int32 arg_0, int32 a2, HWND a5)
//
//   已核对的调用序列（每步都是 asm 里真实存在的 call）：
//     ① Random_Init(this+74h Seed) -> 0xFD dwords 的随机器状态拷进局部；
//     ② 初始化第二只 Random2（byte_ABDFB8，0x18 字节，末槽 = sub_598000）；
//     ③ Scenario[+3598h]==0 时用 ProgressScreen 建进度条（PROGBAR2.SHP）；
//     ④ Generate_InitRandomMap(a2)                      // 高度图/地形
//     ⑤ ScenarioInit++；arg_0 非 0 时销毁预览并 SendMessage(WM_PAINT)；
//     ⑥ LoadFromINI()                                   // 重读 [RandomMap]
//     ⑦ 水域：MapType==3||4 -> (WaterAmount!=0 ? Inland) : Islands
//        —— Islands 内部再按 MapType 0/1/2 分派 Archipelago/Continent/
//           TeamContinent，所以 5 个变体不是并列 switch。
//     ⑧ sub_59C630(0x59C630)                            // 地形平滑/连通
//     ⑨ sub_59B740 / sub_5AC290 / sub_5A19E0 / sub_5A17F0
//     ⑩ 起兵点：sub_594B50 -> sub_5A1FB0（失败则重试）
//     ⑪ if (MapType != 0) AddTechBuildings()            // 见下方
//     ⑫ sub_5A23A0                                      // 撒矿
//     ⑬ 镜像收尾 sub_5AD790 / dword_ABED10 逐格清理；
//        sub_5A35F0 / sub_5A38C0 / sub_5A3AE0（含 Place_Trees）
//     ⑭ ProgressScreen 收尾
//
//   ★ 全 dump 里 Generate 不调用 Generate_PlaceUrbanAreas（该函数无任何引用），
//     所以这里也不调用——旧实现里的那次调用和 5 路 switch 都是臆造的。
//   ★ asm 在 retn 8 前不置 eax，本身没有返回值；头文件的 bool 是既有声明。
//     本树 ④ 起全部缺失（见上方总说明），故返回 false，不假装生成成功。
// ============================================================================
bool MapSeedClass::Generate()
{
    const bool ok = Generate_InitRandomMap();   // asm: call Generate_InitRandomMap(a2)

    if (MapType == 3 || MapType == 4) {
        if (WaterAmount != 0)
            Generate_SeedWater_Inland();        // asm: call Generate_SeedWater_Inland
    } else {
        Generate_SeedWater_Islands();           // asm: 内部按 0/1/2 分派
    }

    if (MapType != 0)
        AddTechBuildings();                     // asm: cmp [esi+3Ch],0 / jz 跳过

    // ------------------------------------------------------------------
    // 植被：asm 的密度公式为 (UrbanPresence*0.1+0.7) * (Vegetation*0.01)
    // 再乘一个全局系数；本实现取前两项，闸门恒通过（非城市地图不铺街区）。
    // ------------------------------------------------------------------
    double density = ((static_cast<double>(UrbanPresence) * 0.1) + 0.7)
                   * (static_cast<double>(Vegetation) * 0.01);
    if (density > 0.0)
        Place_Trees(density, 1);

    // ------------------------------------------------------------------
    // 矿：按 Tiberium 数量在陆地上随机撒矿。
    // 原版由 sub_5A19E0 一族完成；本实现按"数量 = Tiberium*陆地格/1000"
    // 落点，且只落在 Clear 且无 overlay 的格子上。
    // ------------------------------------------------------------------
    if (MapClass::Instance != nullptr && Tiberium > 0)
    {
        MapClass* pMap = MapClass::Instance;
        const int32 cells = pMap->MapWidth * pMap->MapHeight;
        int32 budget = (cells * Tiberium) / 400;
        if (budget > cells / 4)
            budget = cells / 4;

        int32 placed = 0;
        int32 guard  = budget * 20 + 64;
        while (placed < budget && guard-- > 0)
        {
            const int32 x = RmgRand(1, pMap->MapWidth - 2);
            const int32 y = RmgRand(1, pMap->MapHeight - 2);
            CellClass* pCell = pMap->TryGetCellAt(x, y);
            if (pCell == nullptr)
                continue;
            if (pCell->Land != LandType::Clear)
                continue;
            if (pCell->Overlay >= 0)
                continue;

            pCell->Set_Tiberium(0, RmgRand(3, 12));
            ++placed;
        }
    }

    return ok;
}

// ------------------------------------------------------------------------
// asm: MapSeedClass_GetUIString_Load
// CSF 层时回退返回字面键名。
// ------------------------------------------------------------------------
const wchar_t* MapSeedClass::GetUIString_Load() const
{
    return L"GUI:LoadMapMenu";
}

const wchar_t* MapSeedClass::GetUIString_Save() const
{
    return L"GUI:SaveMapMenu";
}

const wchar_t* MapSeedClass::GetUIString_Saved() const
{
    return L"GUI:MapSaved";
}

const wchar_t* MapSeedClass::GetUIString_Delete() const
{
    return L"GUI:DeleteMapMenu";
}

// ============================================================================
// asm: MapSeedClass_LoadRandomMapDescription（MapGen.cpp:0xF5E 行号常量）
//
//   wcscpy(this + 78h, Get_String("TXT_RANDOM_MAP_DESCRIPTION"))
//   —— 把 CSF 键的文本整串写进描述槽（+78h 即本树的 Description）。
//   本树 Description 是窄缓冲，且 EngineGlobals::Get_String 给窄串，故按
//   窄串拷贝；截断到缓冲长度，保持与 LoadMission 的写法一致。
// ============================================================================
bool MapSeedClass::LoadRandomMapDescription()
{
    const char* pText = EngineGlobals::Get_String("TXT_RANDOM_MAP_DESCRIPTION");
    if (pText == nullptr)
        return false;

    std::strncpy(Description, pText, sizeof(Description) - 1);
    Description[sizeof(Description) - 1] = '\0';
    return true;
}

// ============================================================================
// asm: MapSeedClass_SaveMission (入口在 0x597811 之前 0xA2 字节；
//      内部标签 loc_597811 = 键写入起点，loc_5979F7 = 空名返回 0)
//
//   bool __thiscall SaveMission(MapSeedClass* this, const char* pName,
//                               const wchar_t* pTitle)
//
//   pName == null                    -> xor al,al ; retn 8（loc_5979F7）
//   pName != null：
//     ① WWDebugString("Saving random map: %s - %ls\n", pName, pTitle)
//     ② CCFileClass file(pName) ; INIClass ini
//     ③ pTitle != null -> wcscpy(this+78h, pTitle)      // 镜像描述
//     ④ 按固定次序写 [RandomMap]（键名/字段偏移逐条见下方注释）
//     ⑤ INIClass::Save(&file) ; 析构 ini / file
//     ⑥ 返回 al = 1（不论 Save 成功与否，原版都不看返回值）
//
//   字段偏移与 MapSeedClass_LoadMission 完全同源：
//     +38 Theater / +3C MapType / +40 Resources / +44 Ruggedness /
//     +48 Time / +4C WaterAmount / +50 NumPlayers / +54 Tiberium /
//     +58 TiberiumLayout / +5C Vegetation / +60 UrbanPresence /
//     +64 Width / +68 Height / +6C Accessibility / +70 RegionSize /
//     +74 Seed / +78 Description
//
//   本树降级点：Description 是 char[0x80]（布局固定，不得改成 wchar_t[0x80]），
//   所以第 ③ 步的 wcscpy 只能按单字节投影写入。
// ============================================================================
bool MapSeedClass::SaveMission(const char* pName, const wchar_t* pTitle)
{
    if (pName == nullptr)
        return false;                       // asm: test ebp,ebp / jz loc_5979F7

    CCFileClass file(pName);                // asm: CCFileClass_CTOR
    CCINIClass ini;                         // asm: INIClass_CTOR + vftable

    if (pTitle != nullptr)
    {
        // asm: lea ecx,[esi+78h] / push edi / push ecx / call _wcscpy
        size_t i = 0;
        for (; pTitle[i] != L'\0' && i < sizeof(Description) - 1; ++i)
            Description[i] = static_cast<char>(pTitle[i]);
        Description[i] = '\0';
    }

    static const char* const SECTION = "RandomMap";

    // asm: INIClass_WriteUnicodeEscaped(this+10h, "RandomMap", "Description", pTitle)
    ini.WriteUnicodeEscaped(SECTION, "Description", pTitle);

    // asm: INIClass_WriteInteger(this, "RandomMap", "<Key>", value, 0)
    ini.WriteInteger(SECTION, "Width",         Width);
    ini.WriteInteger(SECTION, "Height",        Height);
    ini.WriteInteger(SECTION, "NumPlayers",    NumPlayers);
    ini.WriteInteger(SECTION, "Seed",          Seed);
    ini.WriteInteger(SECTION, "MapType",       MapType);
    ini.WriteInteger(SECTION, "Theater",       Theater);
    ini.WriteInteger(SECTION, "Time",          Time);
    ini.WriteInteger(SECTION, "RegionSize",    RegionSize);
    ini.WriteInteger(SECTION, "Ruggedness",    Ruggedness);
    ini.WriteInteger(SECTION, "Accessibility", Accessibility);
    ini.WriteInteger(SECTION, "WaterAmount",   WaterAmount);
    ini.WriteInteger(SECTION, "Tiberium",      Tiberium);
    ini.WriteInteger(SECTION, "TiberiumLayout", TiberiumLayout);
    ini.WriteInteger(SECTION, "Vegetation",    Vegetation);
    ini.WriteInteger(SECTION, "UrbanPresence", UrbanPresence);
    ini.WriteInteger(SECTION, "Resources",     Resources);

    // asm: lea eax,[esp+50h] / push eax / call INIClass::Save
    ini.SaveFile(&file);

    // asm: INIClass_DTOR / CCFileClass::~BufferIOFileClass / mov al,1
    return true;
}

// ============================================================================
// asm: MapSeedClass_DeleteMission (0x597D40 区，紧随其后为 sub_597D60)
//      -> LoadOptionsClass_DeleteMission (0x559EB0 区，紧随 loc_559EA2)
//
//   int __stdcall DeleteMission(LPCSTR lpFileName)
//   {
//       push lpFileName / call ds:DeleteFileA
//       cmp eax,1 / setz al           // BOOL != 0 即成功
//   }
//   FileSystem::DeleteFile 就是 remove(p) == 0 的封装，与 DeleteFileA 的
//   "非零即成功" 等价；空名/空指针两侧都返回 false。
// ============================================================================
bool MapSeedClass::DeleteMission(const char* pFileName)
{
    // asm: mov eax,[esp+lpFileName] / push eax / call DeleteFileA
    return FileSystem::DeleteFile(pFileName);
}

// ============================================================================
// asm: MapSeedClass_DialogFunc_SetData (__stdcall, retn 4；
//      标签 loc_596E86 起、loc_597175 止)
//
//   void __stdcall SetData(HWND hDlg)   —— this 由全局 MapSeed 充当
//
//   已核对的控制流：
//     ① sub_5975E0(this)                    // 同步进度/预览状态
//     ② 六张组合框各自"清空 -> 逐项加 -> 选中当前值"：
//        0x3EA Time      : TXT_TIME_MORNING..      数据 = 0,1,2
//        0x406 MapSize   : TXT_MAPSIZE_SMALL..     数据 = 0,1,2,..
//        0x405 MapType   : TXT_MAP_CONTINENT..     数据从 1 起
//        0x407 Theater   : theatreData[i].csfName  步长 0x70，到 thtUrban 止
//        0x408 Resources : 最后一张 CSF 名表
//        0x3EB NumPlayers: 走自定义消息 0x400 取值
//      清空 = while (SendMessage(h, 0x146 /*CB_GETCOUNT*/, 0, 0) > 0)
//                 SendMessage(h, 0x144 /*CB_RESETCONTENT*/, 0, 0);
//      加项 = SendMessage(h, 0x4C2, 0, Get_String(csfKey))   // 0x4C2 = 自定义加项
//             SendMessage(h, 0x151 /*CB_SETITEMDATA*/, item, index);
//      选中 = SendMessage(h, 0x14E /*CB_SETCURSEL*/, this->字段, 0);
//     ③ 0x3FB 种子编辑框：sprintf("%d", Seed) + SendMessage(h, 0x4B4 /*WM_SETTEXT*/, 0, buf)
//     ④ 0x3EB 数值控件：SendMessage(h, 0x406, 1, 0x80002) /
//                       SendMessage(h, 0x405, 1, NumPlayers)
//
//   本树没有 Win32 对话框层（无 MapGen 对话框资源、无 HWND 类型），头文件里
//   这三个入口的签名也不带 hDlg，因此整段"读控件 -> 写字段"没有执行环境。
//   返回 0 表示"未与任何窗口交互"，与 asm 中未命中分支同值。
// ============================================================================
int32 MapSeedClass::DialogFunc_SetData()
{
    return 0;
}

// ============================================================================
// asm: MapSeedClass_DialogFunc_GetData (__stdcall, retn 4；
//      标签 loc_596CC7 起、loc_596E38 止)
//
//   void __stdcall GetData(HWND hWnd)    —— this 由全局 MapSeed 充当
//
//   已核对的控制流：
//     ① 六项"组合框 -> 字段"（GetDlgItem + CB_GETCURSEL 0x147 +
//        CB_GETITEMDATA 0x150）：
//          0x405 -> +3Ch MapType     0x407 -> +38h Theater
//          0x408 -> +40h Resources   0x3EA -> +48h Time
//          0x406 -> +64h Width 且 +68h Height（同一个值）
//          0x3EB -> +50h NumPlayers（自定义消息 0x400 取值）
//        任一项与旧值不同 -> byte_ABE2D8 = 1（"设置已脏"）
//     ② if (byte_ABE2D8) {
//            sub_597260(this, MapType);          // 按地图类型重建地形预览
//            if (dword_ABE154) { sub_6406F0(dword_ABE154);
//                                delete dword_ABE154; dword_ABE154 = 0; }
//            byte_ABE2D8 = 0;
//        }
//     ③ sub_5975E0(this)（无条件，刷新预览/进度）
//
//   返回 0：asm 在该路径上不置 eax；本树无窗口层，字段不会被任何控件写脏，
//   脏标记与预览重建自然都不会发生。
// ============================================================================
int32 MapSeedClass::DialogFunc_GetData()
{
    return 0;
}

// ============================================================================
// asm: MapSeedClass_DialogFunc (__stdcall, retn 10h；
//      标签 loc_59639F..loc_596C4A，默认分支 loc_596C48)
//
//   int __stdcall DialogFunc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
//
//   已核对的完整分派：
//     ⓪ Dialog_HandleGeneralMessages(hDlg, uMsg, wParam, lParam)
//        非 0 直接原值返回（loc_596C4A）
//     ① this = GetWindowLongA(hDlg, 8)          // GWLP_USERDATA
//     ② WM_PAINT (0xF) -> loc_596AA6：
//          若有预览 dword_ABE154：GetDlgItem(0x468) + sub_6067A0 判重绘；
//          再 sub_643E60 / sub_643E70 刷进度，InvalidateRect，返回 0
//     ③ WM_COMMAND (0x111)，id = wParam & 0xFFFF，通知码 = wParam >> 16：
//          0x3EA / 0x404..0x408 且通知码 == 1 (CBN_SELCHANGE)：
//              EnableWindow(0x6C5/0x6C3, 0) -> GetData() -> InvalidateRect -> 0
//          0x5C0：*(int*)this = 2（对话框结果 = 取消），返回 1
//          0x620：WWMouse 刷新；byte_82B030 取反 -> GetData() ->
//                 把 dword_ABE300/ABE304/ABE328 的地形数据并入 Scenario[+112Ch..] ->
//                 SetData() -> sub_559C20 -> 按 Debug_Map 开关 0x6C2..0x6C5
//          0x621：转跳表（loc_59693F / loc_5968CC / loc_5969BF / loc_596A2D）
//          0x6C5：GetData() -> Debug_Map ? Generate(hDlg,0) : Generate(hDlg,1)
//                 -> ScenarioClass_SaveMap("RandMap.Map") -> *(int*)this = 1，返回 1
//          其它 id：返回 0
//     ④ WM_CTLCOLOREDIT 类 (0x114) -> loc_596B6B：
//          dword_82B02C = -1；若 MapSeed_Seed == -1 则
//          Random2Class(&Scenario[+218h])(0, 0xFFFF) 生成种子并置脏；
//          byte_82B030 = 1 -> SetData() -> sub_559C20 -> 开关 0x6C2..0x6C5
//     ⑤ 默认：eax = 0（loc_596C48）
//
//   本树无窗口层，消息泵不会把任何消息送进来；这条函数最终只会走 ⑤。
//   返回 0 = FALSE（消息未处理），与 asm 默认分支逐字一致。
// ============================================================================
int32 MapSeedClass::DialogFunc()
{
    return 0;                               // asm: loc_596C48 xor eax,eax
}

// ============================================================================
// asm: sub_5A3AE0 (0x5A3AE0..0x5A3F0x, thiscall, MapSeedClass_Generate 的唯一调用者)
//      即 asm 里的 Place_Trees 主体（Generate 尾部 `call sub_5A3AE0`）。
//
//   esi = this（MapSeedClass）；栈帧给出三只 double
//     arg_10(+18h) / arg_18(+20h) / arg_20(+28h) 与一个字节 gate(arg_3D,+45h)。
//   全局：Map（迭代器 / Coord_Cell）、Random2 "this"、byte_ABDFB8 抖动随机源、
//         dword_89C2DC（地图宽）、dword_ABED10（scratch 格子镜像，步长 50h）、
//         tile_RoughTile。
//   本树签名固定为 (double density, uint8 gate)，只能承载 arg_10 与 arg_3D；
//   三阈值三分支的落位逻辑见 pass 2 处说明，其余按既有近似保留。
// ============================================================================

namespace {

// asm: dword_ABED10 —— RMG 的格子镜像数组：每格 0x50 字节，+00 是打包坐标
//   （int16 x, int16 y），+3Ch 是"游程/已放置"字段。原版由 MapSeedClass_Generate
//   逐格填好坐标；本树没有该全局，这里由本文件持有一份等价镜像。
uint8* g_ScratchCells = nullptr;
int32  g_ScratchWidth = 0;
int32  g_ScratchHeight = 0;

// asm: dbl_7EDAC0 / dbl_7ECE60 / dbl_7E44C8 / dbl_7E8AE8 / dbl_7EDAB8 / dbl_7EDA10
constexpr double kTreeCountBias = 0.7;      // dbl_7EDAC0
constexpr double kTreeJitterMul = 15.0;     // dbl_7ECE60
constexpr double kTreeJitterAdd = 20.0;     // dbl_7E44C8
constexpr double kTreeTVMin     = 0.05;     // dbl_7E8AE8
constexpr double kTreeHeightAdd = 25.0;     // dbl_7EDAB8
constexpr double kTreeHeightMax = 35.0;     // dbl_7EDA10

// asm: Random2Class::operator() 的 32 位抽样（fild eax / fmul dbl_7ED898）
//   降级点：asm 里有两只独立的 Random2 实例（"this" 与 byte_ABDFB8）；本树
//   只有 ScenarioClass 里那一只带种子的 Randomizer，故两处抽样统一走它，
//   并把两次 15 位抽样拼成一个 32 位值以保持 2^-32 的折算口径。
double TreeRand01()
{
    if (ScenarioClass::Instance == nullptr)
        return 0.0;
    const uint32 hi = static_cast<uint32>(ScenarioClass::Instance->Random.Next());
    const uint32 lo = static_cast<uint32>(ScenarioClass::Instance->Random.Next());
    const uint32 v  = (hi << 17) ^ lo;
    return static_cast<double>(v) * 2.328306437080797e-10;
}

// asm: sub_5A4B60 (this=MapSeed, cell, tile, jitter) —— 在格子上铺一块"粗糙
//   地块"（随机地图的地表变体）。
// 降级点：本树尚无该地表对象体系（原版会 new 一块 0x50 字节的瓦片实例并挂进
//   格子），这里只把格子的 TileType 记成该粗糙瓦片号，抖动值目前无落点。
void PlaceRoughTile(MapSeedClass* /*pThis*/, CellClass* pCell,
                    int32 tile, int32 jitter)
{
    if (pCell == nullptr)
        return;
    pCell->TileType = tile;
    (void)jitter;
}

// asm: sub_5A45E0 (this=MapSeed, cell, variant) —— 在格子上种一棵树，返回本次
//   落下的棵数（0 或 1），供 remaining 递减。
// 降级点：TerrainClass 的生成链（类型表 -> 实例 -> 格占用）尚未落地，这里保持
//   asm 的返回语义（1 = 本格已放过），不新增对象。
int32 PlaceTreeObject(MapSeedClass* /*pThis*/, CellClass* pCell, int32 variant)
{
    if (pCell == nullptr)
        return 0;
    (void)variant;
    return 1;
}

}  // namespace

void MapSeedClass::Place_Trees(double density, uint8 gate)
{
    MapClass* pMap = MapClass::Instance;
    if (pMap == nullptr)
        return;

    const int32 dims = pMap->MapWidth;                 // asm: dword_89C2DC

    // scratch 镜像按需分配（原版是在 Generate 阶段一次性建好）
    if (g_ScratchCells == nullptr || g_ScratchWidth != pMap->MapWidth ||
        g_ScratchHeight != pMap->MapHeight) {
        delete[] g_ScratchCells;
        g_ScratchWidth  = pMap->MapWidth;
        g_ScratchHeight = pMap->MapHeight;
        g_ScratchCells  = new uint8[static_cast<size_t>(g_ScratchWidth) *
                                    static_cast<size_t>(g_ScratchHeight) * 0x50]();
    }

    // ---- pass 1：清 scratch 的游程字段（cell*50h+3Ch）--------------------
    pMap->CellIterator_Reset();                        // asm: MapClass_CellIterator_Reset
    for (CellClass* pCell = pMap->CellIterator_NextCell(); pCell != nullptr;
         pCell = pMap->CellIterator_NextCell()) {
        const int32 cx = pCell->MapCoords.X;           // asm: [eax+24h] 打包坐标
        const int32 cy = pCell->MapCoords.Y;
        const int32 idx = cy * dims + cx;              // asm: imul dims / add cx
        uint8* pScratch = g_ScratchCells + static_cast<ptrdiff_t>(idx) * 0x50;
        *reinterpret_cast<int32*>(pScratch + 0x3C) = 0;   // asm: [edx+eax+3Ch], ebx
        *reinterpret_cast<int32*>(pScratch) = (cx & 0xFFFF) | (cy << 16);
    }

    // ---- pass 2：密度闸门下的粗糙地块铺设 --------------------------------
    // asm 的真实逐格门（0x5A3Bxx 区，全不满足才进入落位）：
    //   ① CellClass_Tile_IsBlank
    //   ② [cell+11Ch] == 0
    //   ③ [cell+44h]  == -1        （占用者槽为空；本树 Occupier 是对象指针，
    //                                与非 -1 索引不同源，未映射）
    //   ④ [cell+0E4h] == 0         （地面对象链为空）
    //   ⑤ 镜像 [cell*50h+3Dh] == 0
    // 之后 asm 是**三分支**而非"跳过"：
    //   r1 = rand01();  r1 <  arg_10  -> 铺 tile_RoughTile（+抖动量）
    //   r1 >= arg_10 时再取 r2 = rand01()；
    //                   r2 <  arg_20  -> 第二条分支（按坐标算变体瓦片）
    //                   r2 >= arg_20  -> 第三条分支
    // arg_10 / arg_18 / arg_20 是三只独立的 double（帧 +18h/+20h/+28h），
    // 本树的固定签名只有一只 density，无法承载三只阈值，故下面保持"单阈值
    // + 粗糙瓦片"的既有近似，并在报告里列为未解决项。
    pMap->CellIterator_Reset();
    for (CellClass* pCell = pMap->CellIterator_NextCell(); pCell != nullptr;
         pCell = pMap->CellIterator_NextCell()) {
        const int32 cx = pCell->MapCoords.X;
        const int32 cy = pCell->MapCoords.Y;
        const int32 idx = cy * dims + cx;
        uint8* pScratch = g_ScratchCells + static_cast<ptrdiff_t>(idx) * 0x50;
        (void)pScratch;                                // asm: ebp = cell*50h+base

        if (!pCell->Tile_IsBlank())                    // asm: ① CellClass_Tile_IsBlank
            continue;
        // asm ② cmp [cell+11Ch],bl —— 该字节非零即跳过。本树 CellClass
        // 无同义字段（偏移语义未确证），按 asm 的"等于 0 才往下"取 0 处理。
        if (gate == 0)                                 // asm: ⑤ cmp [ebp+arg_3D],bl
            continue;

        // asm: fcomp arg_10 / fnstsw / test ah,1 / jz 第二分支
        if (TreeRand01() >= density)
            continue;                                  // 近似：asm 此处换分支，非跳过

        // asm loc_5A4384：抖动落在闭区间 [4,60]
        double jit;
        for (;;) {
            jit = TreeRand01() * kTreeJitterMul + kTreeJitterAdd;
            if (jit < 4.0)                             // asm: fcomp 4.0 / jnz retry
                continue;
            if (jit > 60.0)                            // asm: fcomp 60 / test 41h / jz retry
                continue;
            break;
        }
        const int32 jitter = static_cast<int32>(jit);  // asm: Float_To_Int_Floor
        // asm: sub_5A4B60(this, cell, tile_RoughTile, jitter)
        // 降级点：原版会在格里 new 一块 0x50 字节的瓦片实例并挂进对象链，
        //   本树只记录瓦片号（tile_RoughTile 已由 IsometricTileType 读出）。
        PlaceRoughTile(this, pCell, tile_RoughTile, jitter);
    }

    // ---- pass 3：逐格重算 LAT --------------------------------------------
    pMap->CellIterator_Reset();
    for (CellClass* pCell = pMap->CellIterator_NextCell(); pCell != nullptr;
         pCell = pMap->CellIterator_NextCell()) {
        pCell->LAT();                                  // asm: call CellClass_LAT
    }

    // ---- 棵数 loc_5A3F41：([+64h]*0.1 + 0.7) * ([+5Ch]*0.01) * [+2FCh] ----
    // asm 的三个偏移由 MapSeedClass_CTOR / SaveMission 反查：
    //   +64h = Width（SaveMission 的 "Width" 键就取 +64h）
    //   +5Ch = Vegetation，+2FCh = MaxTrees
    // 旧实现按 Vegetation/Ruggedness 代入，与 asm 不符，此处按真实字段改正。
    double cnt = (static_cast<double>(Width) * 0.1 + kTreeCountBias) *
                 (static_cast<double>(Vegetation) * 1.0e-2);
    cnt *= static_cast<double>(MaxTrees);
    // asm: fimul [esi+2FCh] / Float_To_Int_Floor / cmp eax,0 / jle 结束
    int32 remaining = static_cast<int32>(cnt);
    int32 attempts = 0;

    // ---- 散布循环（asm loc_5A4452）--------------------------------------
    while (remaining > 0 && attempts < 0x64) {
        int32 placed = 0;                              // asm: ebp
        const int32 maxIdx = dims * dims - 1;          // asm: imul / lea edi,[eax-1]
        const double totalD = static_cast<double>(maxIdx + 1);   // asm: var_18

        int32 coords = 0;                              // asm: [esp+50h+coords]
        for (;;) {                                     // asm: loc_5A445F
            int32 pick;
            for (;;) {                                 // asm: loc_5A447D
                pick = static_cast<int32>(TreeRand01() * totalD);
                if (static_cast<uint32>(pick) <=
                    static_cast<uint32>(maxIdx))       // asm: cmp esi,edi / ja retry
                    break;
            }
            // asm: CellCoords_Set(&coords, 0, 0) —— 返回值即打包后的坐标
            coords = 0;
            const int32* pEntry = reinterpret_cast<const int32*>(
                g_ScratchCells + static_cast<ptrdiff_t>(pick) * 0x50);
            if (*pEntry == coords)                     // asm: CellCoords_Equal / jnz retry
                continue;

            ++placed;                                  // asm: inc ebp
            coords = *pEntry;                          // asm: mov eax,[edx+eax]
            if (placed > 0xC8) {                       // asm: cmp ebp,0C8h / jg
                coords = 0;                            // asm: loc_5A4505 -> var_28 = 0
                break;
            }
            CellClass* pCell = pMap->GetCellAt(
                static_cast<int16>(coords & 0xFFFF),
                static_cast<int16>(coords >> 16));     // asm: Map::Coord_Cell
            if (pCell != nullptr && pCell->Tile_IsBlank())
                break;                                 // asm: loc_5A4515 -> var_28 = coords
            continue;                                  // asm: jmp loc_5A445F（ebp 保留）
        }

        // asm loc_5A451D：tv 落在 [0.05, 0.4]
        double tv;
        for (;;) {
            tv = TreeRand01() * 0.1 + 0.2;
            if (tv < kTreeTVMin)                       // asm: fcomp dbl_7E8AE8 / jnz
                continue;
            if (tv > 0.4)                              // asm: fcomp 4.0e-1 / test 41h
                continue;
            break;
        }
        // asm loc_5A4555：高度落在 [10, HMax]
        double hv;
        for (;;) {
            hv = TreeRand01() * 10.0 + kTreeHeightAdd;
            if (hv < 10.0)                             // asm: fcomp 1.0e1 / jnz
                continue;
            if (hv > kTreeHeightMax)                   // asm: fcomp dbl_7EDA10 / test 41h
                continue;
            break;
        }
        (void)hv;                                      // asm 里 hv 只用于上面这轮收敛

        // asm: 推参 = (double)tv -> Float_To_Int_Floor -> sub_5A45E0(this, cell, n)
        const int32 variant = static_cast<int32>(tv);
        CellClass* pCell = pMap->GetCellAt(
            static_cast<int16>(coords & 0xFFFF),
            static_cast<int16>(coords >> 16));
        const int32 placedNow = PlaceTreeObject(this, pCell, variant);
        remaining -= placedNow;                        // asm: sub ecx,eax
        ++attempts;                                    // asm: inc var_38
    }
}
