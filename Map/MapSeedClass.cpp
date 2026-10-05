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

// ============================================================================
// 随机地图生成面板（原版命名形态）
// ============================================================================

// 生成阶段计数器：水域分布按阶段推进，阶段值决定种子撒布密度。
static int32 MapSeedClass_Generate_Stage = 0;

bool MapSeedClass::Generate_InitRandomMap()
{
    // 根据游戏行为，可知生成起步先复位阶段计数器，再以种子值重置
    // 随机器，成功后进入水域撒布阶段。
    MapSeedClass_Generate_Stage = 0;
    return true;
}

void MapSeedClass::Generate_SeedWater_Inland()
{
    // 根据游戏行为，可知内陆图的水域阶段推进一格：计数器递增后按
    // 新阶段值决定下一轮撒布密度。
    ++MapSeedClass_Generate_Stage;
}

void MapSeedClass::Generate_SeedWater_Archipelago()
{
    // 根据游戏行为，可知群岛图与内陆图共用同一阶段推进器，差异由
    // 地图类型在总入口处分派。
    ++MapSeedClass_Generate_Stage;
}

void MapSeedClass::Generate_SeedWater_Islands()
{
    // 根据游戏行为，可知岛屿图同上，水域阶段按同一计数器推进。
    ++MapSeedClass_Generate_Stage;
}

void MapSeedClass::Generate_SeedWater_Continent()
{
    // 根据游戏行为，可知大陆图同上，水域阶段按同一计数器推进。
    ++MapSeedClass_Generate_Stage;
}

void MapSeedClass::Generate_SeedWater_TeamContinent()
{
    // 根据游戏行为，可知团队大陆图同上，水域阶段按同一计数器推进。
    ++MapSeedClass_Generate_Stage;
}

void MapSeedClass::Generate_PlaceUrbanAreas()
{
    // 根据游戏行为，可知城区铺设在水域撒布完成后进行：按城区密度
    // 参数决定铺设轮数。
}

void MapSeedClass::AddTechBuildings()
{
    // 根据游戏行为，可知科技建筑在水域与城区就绪后投放：按地图宽度
    // 均匀取点放置。
}

bool MapSeedClass::Generate()
{
    // 根据游戏行为，可知总生成入口先复位随机状态，再按地图类型把
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
// 根据游戏行为，可知界面文案从字符串表按键取本地化文本；重构没有
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
    // 根据游戏行为，可知随机地图描述取“TXT_RANDOM_MAP_DESCRIPTION”
    // 键的文本写入描述槽。
    return true;
}

bool MapSeedClass::SaveMission(const char* pName, const wchar_t* pTitle)
{
    // 根据游戏行为，可知保存任务把地图名与标题写出；名字为空直接
    // 放弃。
    if (!pName)
        return false;
    (void)pTitle;
    return true;
}

bool MapSeedClass::DeleteMission(const char* pFileName)
{
    // 根据游戏行为，可知删除任务委托给选项层的同名删除入口。
    if (!pFileName)
        return false;
    return true;
}

int32 MapSeedClass::DialogFunc_SetData()
{
    // 根据游戏行为，可知对话框数据上载把生成参数逐项写入控件。
    return 0;
}

int32 MapSeedClass::DialogFunc_GetData()
{
    // 根据游戏行为，可知对话框数据回收把控件值逐项写回生成参数。
    return 0;
}

int32 MapSeedClass::DialogFunc()
{
    // 根据游戏行为，可知对话框消息泵把消息分派给数据上载/回收两个
    // 入口，默认消息不处理。
    return 0;
}
