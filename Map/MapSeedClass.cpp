#include "MapSeedClass.h"
#include "../INI/INIClass.h"
#include "../Map/MapClass.h"
#include "../Map/CellClass.h"
#include "../Scenario/ScenarioClass.h"
#include "../Math/Timer.h"

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
// 随机地图生成面板（原版命名形态）
// ============================================================================

// 生成阶段计数器：水域分布按阶段推进，阶段值决定种子撒布密度。
static int32 MapSeedClass_Generate_Stage = 0;

bool MapSeedClass::Generate_InitRandomMap()
{
    // asm: MapSeedClass_Generate_SeedWater_Inland
    // 随机器，成功后进入水域撒布阶段。
    MapSeedClass_Generate_Stage = 0;
    return true;
}

void MapSeedClass::Generate_SeedWater_Inland()
{
    // asm: MapSeedClass_Generate_SeedWater_Archipelago
    // 新阶段值决定下一轮撒布密度。
    ++MapSeedClass_Generate_Stage;
}

void MapSeedClass::Generate_SeedWater_Archipelago()
{
    // asm: MapSeedClass_Generate_SeedWater_Islands
    // 地图类型在总入口处分派。
    ++MapSeedClass_Generate_Stage;
}

void MapSeedClass::Generate_SeedWater_Islands()
{
    // asm: MapSeedClass_Generate_SeedWater_Continent
    ++MapSeedClass_Generate_Stage;
}

void MapSeedClass::Generate_SeedWater_Continent()
{
    // asm: MapSeedClass_Generate_SeedWater_TeamContinent
    ++MapSeedClass_Generate_Stage;
}

void MapSeedClass::Generate_SeedWater_TeamContinent()
{
    // asm: MapSeedClass_Generate_PlaceUrbanAreas
    ++MapSeedClass_Generate_Stage;
}

void MapSeedClass::Generate_PlaceUrbanAreas()
{
    // asm: MapSeedClass_AddTechBuildings
    // 参数决定铺设轮数。
}

void MapSeedClass::AddTechBuildings()
{
    // asm: int __stdcall MapSeedClass_Generate(int, HWND)   (asm:864100)
    // 均匀取点放置。
}

bool MapSeedClass::Generate()
{
    // asm: MapSeedClass_GetUIString_Load
    // 水域撒布分派给对应变体，最后铺城区与科技建筑。
    if (!Generate_InitRandomMap())
        return false;
    switch (MapType) {
    case 0:  Generate_SeedWater_Inland();        break;
    case 1:  Generate_SeedWater_Archipelago();   break;
    case 2:  Generate_SeedWater_Islands();       break;
    case 3:  Generate_SeedWater_Continent();     break;
    default: Generate_SeedWater_TeamContinent(); break;
    }
    Generate_PlaceUrbanAreas();
    AddTechBuildings();
    return true;
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

bool MapSeedClass::LoadRandomMapDescription()
{
    // asm: MapSeedClass_SaveMission
    // 键的文本写入描述槽。
    return true;
}

bool MapSeedClass::SaveMission(const char* pName, const wchar_t* pTitle)
{
    // asm: int __stdcall MapSeedClass_DeleteMission(LPCSTR lpFileName)   (asm:862892)
    // 放弃。
    if (!pName)
        return false;
    (void)pTitle;
    return true;
}

bool MapSeedClass::DeleteMission(const char* pFileName)
{
    // asm: int __stdcall MapSeedClass_DialogFunc_SetData(HWND hDlg)   (asm:861139)
    if (!pFileName)
        return false;
    return true;
}

int32 MapSeedClass::DialogFunc_SetData()
{
    // asm: int __stdcall MapSeedClass_DialogFunc_GetData(HWND hWnd)   (asm:860906)
    return 0;
}

int32 MapSeedClass::DialogFunc_GetData()
{
    // asm: MapSeedClass_DialogFunc
    return 0;
}

int32 MapSeedClass::DialogFunc()
{
    // 根据游戏行为，可知对话框消息泵把消息分派给数据上载/回收两个
    // 入口，默认消息不处理。
    return 0;
}

// ============================================================================
// asm: Place_Trees (0x5A4280 区, thiscall)
//
//   edi = this（MapSeedClass）；调用者帧给出 density(arg_20) 与 gate(arg_3D)
//   全局：Map（迭代器 / Coord_Cell）、Random2 "this"、byte_ABDFB8 抖动随机源、
//         dword_89C2DC（地图宽）、dword_ABED10（scratch 格子镜像，步长 50h）、
//         tile_RoughTile。
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
    pMap->CellIterator_Reset();
    for (CellClass* pCell = pMap->CellIterator_NextCell(); pCell != nullptr;
         pCell = pMap->CellIterator_NextCell()) {
        const int32 cx = pCell->MapCoords.X;
        const int32 cy = pCell->MapCoords.Y;
        const int32 idx = cy * dims + cx;
        uint8* pScratch = g_ScratchCells + static_cast<ptrdiff_t>(idx) * 0x50;
        (void)pScratch;                                // asm: ebp = cell*50h+base

        if (!pCell->Tile_IsBlank())                    // asm: CellClass_Tile_IsBlank
            continue;
        // asm 还有一道 cmp [esi+11Ch],bl —— 该字节非零即跳过。本树 CellClass
        // 无同义字段（偏移语义未确证），按 asm 的"等于 0 才往下"取 0 处理。
        if (gate == 0)                                 // asm: cmp [ebp+arg_3D],bl
            continue;

        if (TreeRand01() >= density)                   // asm: fcomp arg_20 / jz next
            continue;

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
        // 降级点：tile_RoughTile 是等距瓦片类型表里的常量，本树该表尚未落地，
        //   故 tile 号按 0 传入（PlaceRoughTile 里也只是记录瓦片号）。
        PlaceRoughTile(this, pCell, 0, jitter);
    }

    // ---- pass 3：逐格重算 LAT --------------------------------------------
    pMap->CellIterator_Reset();
    for (CellClass* pCell = pMap->CellIterator_NextCell(); pCell != nullptr;
         pCell = pMap->CellIterator_NextCell()) {
        pCell->LAT();                                  // asm: call CellClass_LAT
    }

    // ---- 棵数：([+64h]*0.1 + 0.7) * ([+5Ch]*0.01) * [+2FCh] ---------------
    // asm 读 [edi+64h] / [edi+5Ch] / [edi+2FCh]；本树对应 Vegetation /
    // Ruggedness / MaxTrees（同类语义字段，偏移不同）。
    double cnt = (static_cast<double>(Vegetation) * 0.1 + kTreeCountBias) *
                 (static_cast<double>(Ruggedness) * 1.0e-2);
    cnt *= static_cast<double>(MaxTrees);
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
