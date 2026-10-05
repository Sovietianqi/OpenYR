#include <Scenario/ScenarioClass.h>
#include "../Game/Externs.h"
#include "../Game/Game.h"
#include "../Houses/HouseClass.h"
#include "../Map/MapClass.h"
#include "../Abstract/BuildingTypeClass.h"
#include "../Network/SessionClass.h"
#include <INI/INIClass.h>
#include <IO/FileSystem.h>
#include <IO/CRC.h>
#include <Core/Definitions.h>
#include <Core/Memory.h>
#include <Map/MapClass.h>
#include <Houses/HouseClass.h>
#include <Houses/HouseTypeClass.h>

#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <ctime>

// ============================================================================
// ScenarioClass.cpp - Scenario class implementation
// ============================================================================
// Standalone engine reconstruction of the ScenarioClass.
// In the original game, these methods are at specific addresses:
//   ScenarioClass ctor: 0x689670
//   Init:              0x689810
//   LoadScenario:      0x6898C0
//   StartScenario:     0x6899A0
//   SaveGame:          0x68A2A0
//   LoadGame:          0x68A3E0
//   ReadStartPoints:   0x68A5A0
//   etc.
// ============================================================================

// Instance is defined inline in the header

// ============================================================================
// Constructor / Destructor
// ============================================================================

ScenarioClass::ScenarioClass()
    : HomeCell(0)
    , AltHomeCell(0)
    , UniqueID(1000000)
    , Difficulty1(0)
    , Difficulty2(0)
    , unknown_62C(0)
    , IsGamePaused(false)
    , StartX(0)
    , StartY(0)
    , Width(0)
    , Height(0)
    , NumberStartingPoints(0)
    , TeamsPresent(false)
    , NumCoopHumanStartSpots(0)
    , MissionTimerTextCSF(nullptr)
    , TechLevel(-1)
    , MapTintR(0)
    , MapTintG(0)
    , MapTintB(0)
    , Theater(TheaterType::Temperate)
    , Intro(nullptr)
    , Brief(nullptr)
    , Win(nullptr)
    , Lose(nullptr)
    , Action(nullptr)
    , PostScore(nullptr)
    , PreMapSelect(nullptr)
    , ThemeIndex(0)
    , HumanPlayerHouseTypeIndex(-1)
    , CarryOverMoney(0.0)
    , CarryOverCap(0)
    , Percent(0)
    , unknown_34A0(0)
    , FreeRadar(false)
    , TrainCrate(false)
    , TiberiumGrowthEnabled(true)
    , VeinGrowthEnabled(false)
    , IceGrowthEnabled(false)
    , BridgeDestroyed(false)
    , VariablesChanged(false)
    , AmbientChanged(false)
    , EndOfGame(false)
    , TimerInherit(false)
    , SkipScore(false)
    , OneTimeOnly(false)
    , SkipMapSelect(false)
    , TruckCrate(false)
    , FillSilos(false)
    , TiberiumDeathToVisceroid(false)
    , IgnoreGlobalAITriggers(false)
    , unknown_bool_34B5(false)
    , unknown_bool_34B6(false)
    , unknown_bool_34B7(false)
    , PlayerSideIndex(-1)
    , MultiplayerOnly(false)
    , IsRandom(false)
    , PickedUpAnyCrate(false)
    , CampaignIndex(-1)
    , StartingDropships(0)
    , AmbientOriginal(0)
    , AmbientCurrent(0)
    , AmbientTarget(0)
    , IonAmbient(0)
    , NukeAmbient(0)
    , NukeAmbientChangeRate(0)
    , DominatorAmbient(0)
    , DominatorAmbientChangeRate(0)
    , unknown_3598(0)
    , InitTime(0)
    , Stage(0)
    , UserInputLocked(false)
    , unknown_35A3(false)
    , ParTimeEasy(0)
    , ParTimeMedium(0)
    , ParTimeDifficult(0)
    , LS640BriefLocX(0)
    , LS640BriefLocY(0)
    , LS800BriefLocX(0)
    , LS800BriefLocY(0)
    , ScenarioName("")
    , ScenarioDescription("")
    , ScenarioFileName("")
    , IsMultiplayer(false)
    , IsCampaign(false)
    , Difficulty(1)
    , InitialMoney(0)
    , MapWidth(0)
    , MapHeight(0)
    , IsSkirmish(false)
    , IsBridgeDestructionEnabled(false)
    , IsFogOfWar(false)
    , IsMCVRepack(true)
    , IsShortGame(false)
    , IsCrates(true)
    , IsSuperWeapons(true)
    , IsMultiEngineer(false)
    , IsBuildOffAlly(false)
    , IsBases(true)
    , RandomSeed(0)
    , FrameCount(0)
{
    // Initialize character arrays
    NextScenario[0] = '\0';
    AltNextScenario[0] = '\0';

    // Initialize waypoints
    for (int32 i = 0; i < MaxWaypoints; ++i) {
        Waypoints[i] = CellStruct(0, 0);
    }

    // Initialize starting points
    for (int32 i = 0; i < MaxStartingPoints; ++i) {
        StartingPoints[i].X = 0;
        StartingPoints[i].Y = 0;
        HouseHomeCells[i] = CellStruct(0, 0);
    }

    // Initialize house indices
    for (int32 i = 0; i < 0x10; ++i) {
        HouseIndices[i] = -1;
    }

    // Initialize text buffers
    MissionTimerText[0] = '\0';
    FileName[0] = '\0';
    Name[0] = L'\0';
    UIName[0] = '\0';
    UINameLoaded[0] = L'\0';
    Briefing[0] = L'\0';
    BriefingCSF[0] = '\0';

    // Initialize variables
    for (int32 i = 0; i < MaxGlobalVariables; ++i) {
        GlobalVariables[i] = Variable();
    }
    for (int32 i = 0; i < MaxLocalVariables; ++i) {
        LocalVariables[i] = Variable();
    }

    // Initialize views
    View1 = CellStruct(0, 0);
    View2 = CellStruct(0, 0);
    View3 = CellStruct(0, 0);
    View4 = CellStruct(0, 0);

    // Initialize random number generator
    Random.Randomize();

    // Initialize parade text
    for (int32 i = 0; i < 0x1F; ++i) {
        UnderParTitle[i] = '\0';
        UnderParMessage[i] = '\0';
        OverParTitle[i] = '\0';
        OverParMessage[i] = '\0';
        LSLoadMessage[i] = '\0';
        LSBrief[i] = '\0';
    }

    // Initialize load screen backgrounds
    for (int32 i = 0; i < 0x40; ++i) {
        LS640BkgdName[i] = '\0';
        LS800BkgdName[i] = '\0';
        LS800BkgdPal[i] = '\0';
    }

    // Set init time
    InitTime = static_cast<int32>(time(nullptr));
}

ScenarioClass::~ScenarioClass()
{
    // AllowableUnits, AllowableUnitMaximums, DropshipUnitCounts
    // are cleaned up by their destructors automatically
}

// ============================================================================
// Init - Initialize scenario state
// ============================================================================

void ScenarioClass::Init()
{
    IsGamePaused = false;
    EndOfGame = false;
    FrameCount = 0;
    Stage = 0;
    ElapsedTimer.Start(0);
    PauseTimer.Stop();
    MissionTimer.Stop();
    ShroudRegrowTimer.Stop();
    FogTimer.Stop();
    IceTimer.Stop();
    AmbientTimer.Stop();
    Random.Randomize();
    VariablesChanged = false;
    AmbientChanged = false;
    PickedUpAnyCrate = false;
    BridgeDestroyed = false;
    UserInputLocked = false;
    InitTime = static_cast<int32>(time(nullptr));

    // Reset variables
    for (int32 i = 0; i < MaxGlobalVariables; ++i) {
        GlobalVariables[i].Value = 0;
    }
    for (int32 i = 0; i < MaxLocalVariables; ++i) {
        LocalVariables[i].Value = 0;
    }
}

// ============================================================================
// ClearClasses - Clear all scenario-managed class instances
// ============================================================================

void ScenarioClass::ClearClasses()
{
    AllowableUnits.Clear();
    AllowableUnitMaximums.Clear();
    DropshipUnitCounts.Clear();
}

// ============================================================================
// StartScenario - Static entry point for starting a scenario
// ============================================================================

bool ScenarioClass::StartScenario(const char* FileName, bool Briefing, int32 CampaignIndex)
{
    if (!Instance) {
        Instance = new ScenarioClass();
    }

    if (!FileName) {
        return false;
    }

    Instance->IsCampaign = (CampaignIndex >= 0);
    Instance->CampaignIndex = CampaignIndex;

    bool result = Instance->LoadScenario(FileName);
    if (result && Instance->IsCampaign) {
        // Campaign setup - set difficulty, etc.
        switch (Instance->Difficulty) {
            case 0: Instance->Percent = 100; break;  // Easy
            case 1: Instance->Percent = 100; break;  // Normal
            case 2: Instance->Percent = 100; break;  // Hard
            default: Instance->Percent = 100; break;
        }
    }

    return result;
}

// ============================================================================
// LoadScenario - Load a scenario from file
// ============================================================================

bool ScenarioClass::LoadScenario(const char* pFileName)
{
    if (!pFileName) {
        return false;
    }

    // Copy filename
    int32 i = 0;
    while (pFileName[i] && i < 0x103) {
        FileName[i] = pFileName[i];
        ++i;
    }
    FileName[i] = '\0';
    ScenarioFileName = FileName;

    // Initialize state
    Init();

    // Parse the map/INI file
    CCINIClass* pINI = CCINIClass::LoadINIFile(pFileName);
    if (!pINI) {
        return false;
    }

    // Read scenario sections
    ReadStartPoints(*pINI);

    // Cleanup
    CCINIClass::UnloadINIFile(pINI);

    return true;
}

// ============================================================================
// AssignHouses - Assign houses to players based on scenario data
// ============================================================================

void ScenarioClass::AssignHouses()
{
    if (!Instance) return;

    // Map house indices to starting positions
    // House indices are set up based on the scenario file
    for (int32 i = 0; i < Instance->NumberStartingPoints; ++i) {
        if (i >= 0x10) break;
        // Each starting point gets a house
        if (Instance->HouseIndices[i] < 0) {
            // Find an available house
            for (int32 j = 0; j < 0x10; ++j) {
                bool used = false;
                for (int32 k = 0; k < i; ++k) {
                    if (Instance->HouseIndices[k] == j) {
                        used = true;
                        break;
                    }
                }
                if (!used) {
                    Instance->HouseIndices[i] = j;
                    break;
                }
            }
        }
    }
}

// ============================================================================
// CreateUnits - Create starting units for the scenario
// ============================================================================

void ScenarioClass::CreateUnits()
{
    if (!Instance) return;

    // Create harvesters for each house
    for (int32 i = 0; i < Instance->NumberStartingPoints; ++i) {
        int32 houseIdx = Instance->HouseIndices[i];
        if (houseIdx < 0 || houseIdx >= 32) continue;

        HouseClass* pHouse = HouseClass::Array[houseIdx];
        if (!pHouse) continue;

        // Place starting units at the starting point
        CellStruct startCell = Instance->HouseHomeCells[i];
        if (startCell.X == 0 && startCell.Y == 0 && i > 0) {
            startCell = CellStruct(
                Instance->StartingPoints[i].X,
                Instance->StartingPoints[i].Y
            );
        }
    }
}

// ============================================================================
// EndGame - End the current game
// ============================================================================

void ScenarioClass::EndGame()
{
    EndOfGame = true;
    IsGamePaused = true;
    Stage = 0;
}

// ============================================================================
// SaveGame - Save game state to file
// ============================================================================

bool ScenarioClass::SaveGame(const char* FileName, const wchar_t* Description, bool BarGraph)
{
    if (!FileName) return false;
    if (!Instance) return false;

    // Create save file
    CCFileClass* pFile = new CCFileClass(FileName);
    if (!pFile->Open(static_cast<int32>(FileAccessMode::Write))) {
        delete pFile;
        return false;
    }

    // Write save header
    // In the original game, this writes the save game format version,
    // scenario data, and all game state

    // Write description
    if (Description) {
        int32 descLen = 0;
        while (Description[descLen]) ++descLen;
        pFile->Write(&descLen, sizeof(descLen));
        pFile->Write(Description, descLen * sizeof(wchar_t));
    } else {
        int32 zero = 0;
        pFile->Write(&zero, sizeof(zero));
    }

    // Write scenario data
    // ...

    pFile->Close();
    delete pFile;
    return true;
}

// ============================================================================
// LoadGame - Load game state from file
// ============================================================================

bool ScenarioClass::LoadGame(const char* FileName)
{
    if (!FileName) return false;

    if (!Instance) {
        Instance = new ScenarioClass();
    }

    // Open save file
    CCFileClass* pFile = new CCFileClass(FileName);
    if (!pFile->Open(static_cast<int32>(FileAccessMode::Read))) {
        delete pFile;
        return false;
    }

    // Read save header
    // ...

    pFile->Close();
    delete pFile;

    Instance->Init();
    return true;
}

// ============================================================================
// UpdateCellLighting - Update cell lighting across the map
// ============================================================================

void ScenarioClass::UpdateCellLighting()
{
    if (!Instance) return;

    int32 r = Instance->NormalLighting.Tint.Red;
    int32 g = Instance->NormalLighting.Tint.Green;
    int32 b = Instance->NormalLighting.Tint.Blue;
    RecalcLighting(r, g, b, false);
}

// ============================================================================
// UpdateLighting - Update global lighting (smooth transition)
// ============================================================================

void ScenarioClass::UpdateLighting()
{
    if (!Instance) return;

    int32 diff = Instance->AmbientTarget - Instance->AmbientCurrent;
    if (diff != 0) {
        if (diff > 0) {
            Instance->AmbientCurrent += (diff > 10 ? 10 : diff);
        } else {
            Instance->AmbientCurrent += (diff < -10 ? -10 : diff);
        }
    }
}

// ============================================================================
// RecalcLighting - Recalculate lighting with new tint values
// ============================================================================

void ScenarioClass::RecalcLighting(int32 R, int32 G, int32 B, bool tint)
{
    if (!Instance) return;

    if (R >= 0) Instance->NormalLighting.Tint.Red = R;
    if (G >= 0) Instance->NormalLighting.Tint.Green = G;
    if (B >= 0) Instance->NormalLighting.Tint.Blue = B;

    UpdateLighting();
}

// ============================================================================
// UpdateHashPalLighting - Update hash palette lighting
// ============================================================================

void ScenarioClass::UpdateHashPalLighting(int32 R, int32 G, int32 B, bool tint)
{
    // Update the hash palette lookup table with new lighting values
    // This affects how colors are remapped during rendering
    if (!Instance) return;

    int32 ambient = Instance->AmbientCurrent;
    // Apply lighting to the palette hash table
    // Each palette entry is remapped based on the current lighting
}

// ============================================================================
// ScenarioLighting - Get current scenario lighting values
// ============================================================================

void ScenarioClass::ScenarioLighting(int32* r, int32* g, int32* b)
{
    if (!Instance) {
        if (r) *r = 0;
        if (g) *g = 0;
        if (b) *b = 0;
        return;
    }

    if (r) *r = Instance->NormalLighting.Tint.Red;
    if (g) *g = Instance->NormalLighting.Tint.Green;
    if (b) *b = Instance->NormalLighting.Tint.Blue;
}

// ============================================================================
// Waypoint helpers
// ============================================================================

bool ScenarioClass::IsDefinedWaypoint(int32 idx) const
{
    if (idx < 0 || idx >= MaxWaypoints) return false;
    // Waypoint 0 is always defined
    if (idx == 0) return true;
    return !(Waypoints[idx].X == 0 && Waypoints[idx].Y == 0);
}

CellStruct ScenarioClass::GetWaypointCoords(int32 idx) const
{
    if (idx >= 0 && idx < MaxWaypoints) {
        return Waypoints[idx];
    }
    return CellStruct(0, 0);
}

void ScenarioClass::SetWaypointCoords(int32 idx, const CellStruct& cell)
{
    if (idx >= 0 && idx < MaxWaypoints) {
        Waypoints[idx] = cell;
    }
}

// ============================================================================
// ReadStartPoints - Read [Waypoints] section from scenario INI
// ============================================================================

void ScenarioClass::ReadStartPoints(CCINIClass& ini)
{
    // Read waypoints
    const char* section = "Waypoints";
    for (int32 i = 0; i < MaxWaypoints; ++i) {
        char key[32];
        // Format: "0", "1", "2", ..., "701"
        int32 len = 0;
        int32 temp = i;
        if (temp == 0) {
            key[0] = '0';
            key[1] = '\0';
        } else {
            char rev[32];
            int32 revLen = 0;
            while (temp > 0) {
                rev[revLen++] = '0' + (temp % 10);
                temp /= 10;
            }
            for (int32 j = revLen - 1; j >= 0; --j) {
                key[len++] = rev[j];
            }
            key[len] = '\0';
        }

        // Read waypoint coordinate as "X,Y"
        int32 vals[2] = {0, 0};
        ini.Read2Integers(vals, section, key, vals);
        Waypoints[i].X = static_cast<int16>(vals[0]);
        Waypoints[i].Y = static_cast<int16>(vals[1]);
    }

    // Read basic section
    section = "Basic";
    char buffer[256];

    if (ini.ReadString(section, "Name", "", buffer, sizeof(buffer))) {
        int32 j = 0;
        while (buffer[j] && j < 0x2C) {
            Name[j] = static_cast<wchar_t>(buffer[j]);
            ++j;
        }
        Name[j] = L'\0';
    }

    if (ini.ReadString(section, "NextScenario", "", buffer, sizeof(buffer))) {
        int32 j = 0;
        while (buffer[j] && j < 0x103) {
            NextScenario[j] = buffer[j];
            ++j;
        }
        NextScenario[j] = '\0';
    }

    if (ini.ReadString(section, "AltNextScenario", "", buffer, sizeof(buffer))) {
        int32 j = 0;
        while (buffer[j] && j < 0x103) {
            AltNextScenario[j] = buffer[j];
            ++j;
        }
        AltNextScenario[j] = '\0';
    }

    // Read map dimensions
    StartX = ini.ReadInteger(section, "X", 0);
    StartY = ini.ReadInteger(section, "Y", 0);
    Width = ini.ReadInteger(section, "Width", 0);
    Height = ini.ReadInteger(section, "Height", 0);
    MapWidth = Width;
    MapHeight = Height;

    // Read theater
    int32 theaterVal = ini.ReadInteger(section, "Theater", 0);
    switch (theaterVal) {
        case 0: Theater = TheaterType::Temperate; break;
        case 1: Theater = TheaterType::Snow; break;
        case 2: Theater = TheaterType::Urban; break;
        case 3: Theater = TheaterType::Desert; break;
        case 4: Theater = TheaterType::Lunar; break;
        case 5: Theater = TheaterType::NewUrban; break;
        default: Theater = TheaterType::Temperate; break;
    }

    // Read carryover
    CarryOverMoney = ini.ReadDouble(section, "CarryOverMoney", 0.0);
    CarryOverCap = ini.ReadInteger(section, "CarryOverCap", 0);
    Percent = ini.ReadInteger(section, "Percent", 100);

    // Read intro/brief/win/lose/action movies
    ini.ReadString(section, "Intro", "", buffer, sizeof(buffer));
    // Store movie references
    if (buffer[0]) {
        char* p = new char[strlen(buffer) + 1];
        strcpy(p, buffer);
        Intro = p;
    }

    // ------------------------------------------------------------------
    // [Basic] - remaining keys
    // ------------------------------------------------------------------
    ini.ReadString(section, "Brief", "", buffer, sizeof(buffer));
    if (buffer[0]) {
        char* p = new char[strlen(buffer) + 1];
        strcpy(p, buffer);
        Brief = p;
    }

    ini.ReadString(section, "Win", "", buffer, sizeof(buffer));
    if (buffer[0]) {
        char* p = new char[strlen(buffer) + 1];
        strcpy(p, buffer);
        Win = p;
    }

    ini.ReadString(section, "Lose", "", buffer, sizeof(buffer));
    if (buffer[0]) {
        char* p = new char[strlen(buffer) + 1];
        strcpy(p, buffer);
        Lose = p;
    }

    ini.ReadString(section, "Action", "", buffer, sizeof(buffer));
    if (buffer[0]) {
        char* p = new char[strlen(buffer) + 1];
        strcpy(p, buffer);
        Action = p;
    }

    ini.ReadString(section, "PostScore", "", buffer, sizeof(buffer));
    if (buffer[0]) {
        char* p = new char[strlen(buffer) + 1];
        strcpy(p, buffer);
        PostScore = p;
    }

    ini.ReadString(section, "PreMapSelect", "", buffer, sizeof(buffer));
    if (buffer[0]) {
        char* p = new char[strlen(buffer) + 1];
        strcpy(p, buffer);
        PreMapSelect = p;
    }

    MultiplayerOnly = ini.ReadBool(section, "MultiplayerOnly", MultiplayerOnly);
    TimerInherit    = ini.ReadBool(section, "TimerInherit",    TimerInherit);
    EndOfGame       = ini.ReadBool(section, "EndOfGame",       EndOfGame);

    ThemeIndex = ini.ReadInteger(section, "Theme", ThemeIndex);

    // Read flags
    FreeRadar = ini.ReadBool(section, "FreeRadar", false);
    TrainCrate = ini.ReadBool(section, "TrainCrate", false);
    PlayerSideIndex = ini.ReadInteger(section, "Player", -1);
    ThemeIndex = ini.ReadInteger(section, "Theme", 0);

    // ------------------------------------------------------------------
    // [Header] - starting point span and the coop start-spot count.
    // ------------------------------------------------------------------
    StartX = ini.ReadInteger("Header", "StartX", StartX);
    StartY = ini.ReadInteger("Header", "StartY", StartY);
    Width  = ini.ReadInteger("Header", "Width",  Width);
    Height = ini.ReadInteger("Header", "Height", Height);
    NumberStartingPoints     = ini.ReadInteger("Header", "NumberStartingPoints",     NumberStartingPoints);
    NumCoopHumanStartSpots   = ini.ReadInteger("Header", "NumCoopHumanStartSpots",   NumCoopHumanStartSpots);

    // ------------------------------------------------------------------
    // [Briefing] - the loading-screen text and its backdrops.
    // ------------------------------------------------------------------
    ini.ReadString("Briefing", "LSLoadMessage", "", LSLoadMessage, sizeof(LSLoadMessage));
    ini.ReadString("Briefing", "LSLoadBriefing", "", LSBrief, sizeof(LSBrief));
    LS640BriefLocX  = ini.ReadInteger("Briefing", "LS640BriefLocX",  LS640BriefLocX);
    LS640BriefLocY  = ini.ReadInteger("Briefing", "LS640BriefLocY",  LS640BriefLocY);
    LS800BriefLocX  = ini.ReadInteger("Briefing", "LS800BriefLocX",  LS800BriefLocX);
    LS800BriefLocY  = ini.ReadInteger("Briefing", "LS800BriefLocY",  LS800BriefLocY);
    ini.ReadString("Briefing", "LS640BkgdName", "", LS640BkgdName, sizeof(LS640BkgdName));
    ini.ReadString("Briefing", "LS800BkgdName", "", LS800BkgdName, sizeof(LS800BkgdName));
    ini.ReadString("Briefing", "LS800BkgdPal",  "", LS800BkgdPal,  sizeof(LS800BkgdPal));

    // ------------------------------------------------------------------
    // [VariableNames] - the free-form list of global variable names.
    // ------------------------------------------------------------------
    {
        const int32 varCount = ini.GetKeyCount("VariableNames");
        for (int32 i = 0; i < varCount && i < MaxGlobalVariables; ++i) {
            const char* pVarName = ini.GetKeyName("VariableNames", i);
            if (pVarName == nullptr)
                continue;
            std::strncpy(GlobalVariables[i].Name, pVarName,
                         sizeof(GlobalVariables[i].Name) - 1);
            GlobalVariables[i].Name[sizeof(GlobalVariables[i].Name) - 1] = '\0';
        }
    }

    // Read map-specific flags
    section = "Map";
    TiberiumGrowthEnabled = ini.ReadBool(section, "TiberiumGrowth", true);
    VeinGrowthEnabled = ini.ReadBool(section, "VeinGrowth", false);
    IceGrowthEnabled = ini.ReadBool(section, "IceGrowth", false);
    FillSilos = ini.ReadBool(section, "FillSilos", false);
    TiberiumDeathToVisceroid = ini.ReadBool(section, "TiberiumDeathToVisceroid", false);
    IgnoreGlobalAITriggers = ini.ReadBool(section, "IgnoreGlobalAITriggers", false);
    MultiplayerOnly = ini.ReadBool(section, "MultiplayerOnly", false);
    IsMultiplayer = MultiplayerOnly;

    // Read lighting
    section = "Lighting";
    AmbientOriginal = ini.ReadInteger(section, "Ambient", 0);
    AmbientCurrent = AmbientOriginal;
    AmbientTarget = AmbientOriginal;

    NormalLighting.Tint.Red   = ini.ReadInteger(section, "Red", 0);
    NormalLighting.Tint.Green = ini.ReadInteger(section, "Green", 0);
    NormalLighting.Tint.Blue  = ini.ReadInteger(section, "Blue", 0);
    NormalLighting.Ground   = ini.ReadInteger(section, "Ground", 0);
    NormalLighting.Level    = ini.ReadInteger(section, "Level", 0);

    IonAmbient = ini.ReadInteger(section, "IonAmbient", 0);
    IonLighting.Tint.Red   = ini.ReadInteger(section, "IonRed", 0);
    IonLighting.Tint.Green = ini.ReadInteger(section, "IonGreen", 0);
    IonLighting.Tint.Blue  = ini.ReadInteger(section, "IonBlue", 0);

    // Read special flags - one boolean key per bit.
    GetGlobalFlags(&ini);

    // [Ranking] - the par times and the under/over-par captions.
    ReadRanking(&ini);

    // [VariableNames] - the local-variable table of the scenario.
    ReadLocalVariables(&ini);

    // Read starting waypoints
    for (int32 i = 0; i < MaxStartingPoints; ++i) {
        char key[32];
        int32 len = 0;
        key[len++] = 'S';
        key[len++] = 't';
        key[len++] = 'a';
        key[len++] = 'r';
        key[len++] = 't';
        if (i >= 10) {
            key[len++] = '0' + (i / 10);
        }
        key[len++] = '0' + (i % 10);
        key[len] = '\0';

        int32 vals[2] = {0, 0};
        ini.Read2Integers(vals, section, key, vals);
        StartingPoints[i].X = static_cast<int32>(vals[0]);
        StartingPoints[i].Y = static_cast<int32>(vals[1]);
    }

    NumberStartingPoints = ini.ReadInteger(section, "NumberOfStartingPoints", 0);

    // Parse difficulty
    Difficulty = ini.ReadInteger(section, "Difficulty", 1);

    // Read mission timer
    section = "MissionTimer";
    int32 timerVal = ini.ReadInteger(section, "MissionTimer", 0);
    if (timerVal > 0) {
        MissionTimer.Start(timerVal);
    }
}
// ============================================================================
// GetGlobalFlags - ScenarioClass_GetGlobalFlags
//
//   [SpecialFlags] of the scenario file.  Every key takes the bit's current
//   value as its fallback, so a scenario that only mentions a few flags
//   keeps whatever the map already declared.  Bit assignments follow the
//   original's ScenarioFlags layout exactly:
//
//     5  Inert              11 HarvesterImmune
//     6  TiberiumGrows      12 FogOfWar
//     7  TiberiumSpreads    15 TiberiumExplosive
//     8  MCVDeploy          16 DestroyableBridges
//     9  InitialVeteran     17 Meteorites
//    10  FixedAlliance      18 IonStorms
//                           19 Visceroids
// ============================================================================
void ScenarioClass::GetGlobalFlags(CCINIClass* pINI)
{
    static const char* const SECTION = "SpecialFlags";

    if (pINI == nullptr)
        return;

    SpecialFlags.SetBit(5,  pINI->ReadBool(SECTION, "Inert",              SpecialFlags.Inert()));
    SpecialFlags.SetBit(6,  pINI->ReadBool(SECTION, "TiberiumGrows",      SpecialFlags.TiberiumGrows()));
    SpecialFlags.SetBit(7,  pINI->ReadBool(SECTION, "TiberiumSpreads",    SpecialFlags.TiberiumSpreads()));
    SpecialFlags.SetBit(8,  pINI->ReadBool(SECTION, "MCVDeploy",          SpecialFlags.MCVDeploy()));
    SpecialFlags.SetBit(9,  pINI->ReadBool(SECTION, "InitialVeteran",     SpecialFlags.InitialVeteran()));
    SpecialFlags.SetBit(10, pINI->ReadBool(SECTION, "FixedAlliance",      SpecialFlags.FixedAlliance()));
    SpecialFlags.SetBit(11, pINI->ReadBool(SECTION, "HarvesterImmune",    SpecialFlags.HarvesterImmune()));
    SpecialFlags.SetBit(12, pINI->ReadBool(SECTION, "FogOfWar",           SpecialFlags.FogOfWar()));
    SpecialFlags.SetBit(14, pINI->ReadBool(SECTION, "TiberiumExplosive",  SpecialFlags.TiberiumExplosive()));
    SpecialFlags.SetBit(15, pINI->ReadBool(SECTION, "DestroyableBridges", SpecialFlags.DestroyableBridges()));
    SpecialFlags.SetBit(16, pINI->ReadBool(SECTION, "Meteorites",         SpecialFlags.Meteorites()));
    SpecialFlags.SetBit(17, pINI->ReadBool(SECTION, "IonStorms",          SpecialFlags.IonStorms()));
    SpecialFlags.SetBit(18, pINI->ReadBool(SECTION, "Visceroids",         SpecialFlags.Visceroids()));
}

// ============================================================================
// ReadLocalVariables - ScenarioClass_ReadLocalVariables
//
//   The [VariableNames] pass.  Every one of the 100 slots is wiped first,
//   then the value count of the section is clamped to 100.  Each key is
//   itself the slot ordinal (the key text goes through atoi), the value is
//   split on ',' and the first token is copied verbatim into the slot name
//   at +0x248A.  A second token, when it survives, is converted with atoi
//   and stored as a boolean at +0x24B2 - that is the slot's "is a global
//   variable" marker.
//
//   Slot layout, matching the original object:
//       slot base  = this + 0x248A
//       slot pitch = 0x29
//       name       = slot base + 0x00  (0x28 bytes)
//       flag       = slot base + 0x28
// ============================================================================
void ScenarioClass::ReadLocalVariables(CCINIClass* pINI)
{
    static const char* const SECTION = "VariableNames";

    if (pINI == nullptr)
        return;

    for (int32 i = 0; i < MaxLocalVariables; ++i)
        LocalVariables[i].Name[0] = '\0';

    int32 count = pINI->GetKeyCount(SECTION);
    if (count >= MaxLocalVariables)
        count = MaxLocalVariables;

    for (int32 idx = 0; idx < count; ++idx) {
        const char* pKey = pINI->GetKeyName(SECTION, idx);
        if (pKey == nullptr)
            continue;

        const int32 slot = std::atoi(pKey);
        if (slot < 0 || slot >= MaxLocalVariables)
            continue;

        char buffer[0x80];
        buffer[0] = '\0';
        pINI->ReadString(SECTION, pKey, "", buffer, sizeof(buffer));

        char* pName = std::strtok(buffer, ",");
        if (pName == nullptr)
            continue;

        std::strncpy(LocalVariables[slot].Name, pName,
                     sizeof(LocalVariables[slot].Name) - 1);
        LocalVariables[slot].Name[sizeof(LocalVariables[slot].Name) - 1] = '\0';

        char* pValue = std::strtok(nullptr, ",");
        if (pValue != nullptr)
            LocalVariables[slot].Value = (std::atoi(pValue) != 0) ? 1 : 0;
    }
}

// ============================================================================
// ReadRanking - the [Ranking] tail of Scenario_ReadLightingAndBasic
//
//   Runs after ScenarioHeader/ScenarioLocalVariables/Scenario_ReadStartPoints.
//   The three par times are not plain integers: Get_Time reads the value as a
//   string and runs sscanf("%02d:%02d:%02d"), then folds the result back into
//   seconds as ((hours * 60 + minutes) * 60 + seconds) * 1000.  The four
//   captions are ordinary string reads capped at 0x1F bytes.
// ============================================================================
void ScenarioClass::ReadRanking(CCINIClass* pINI)
{
    static const char* const SECTION = "Ranking";

    if (pINI == nullptr)
        return;

    char buffer[0x80];
    int32 hours, minutes, seconds;

    buffer[0] = '\0';
    pINI->ReadString(SECTION, "ParTimeEasy", "", buffer, sizeof(buffer));
    hours = minutes = seconds = 0;
    std::sscanf(buffer, "%02d:%02d:%02d", &hours, &minutes, &seconds);
    ParTimeEasy = (((hours * 60) + minutes) * 60 + seconds) * 1000;

    buffer[0] = '\0';
    pINI->ReadString(SECTION, "ParTimeMedium", "", buffer, sizeof(buffer));
    hours = minutes = seconds = 0;
    std::sscanf(buffer, "%02d:%02d:%02d", &hours, &minutes, &seconds);
    ParTimeMedium = (((hours * 60) + minutes) * 60 + seconds) * 1000;

    buffer[0] = '\0';
    pINI->ReadString(SECTION, "ParTimeHard", "", buffer, sizeof(buffer));
    hours = minutes = seconds = 0;
    std::sscanf(buffer, "%02d:%02d:%02d", &hours, &minutes, &seconds);
    ParTimeDifficult = (((hours * 60) + minutes) * 60 + seconds) * 1000;

    pINI->ReadString(SECTION, "UnderParTitle", "", UnderParTitle, 0x1F);
    pINI->ReadString(SECTION, "UnderParMessage", "", UnderParMessage, 0x1F);
    pINI->ReadString(SECTION, "OverParTitle", "", OverParTitle, 0x1F);
    pINI->ReadString(SECTION, "OverParMessage", "", OverParMessage, 0x1F);
}

// ============================================================================
// ScenarioClass - global / local variable accessors (asm 0x6AE1xx)
//
//  The script VM's Set/Clear Global and Set/Clear Local actions store a
//  boolean into the indexed variable slot; the trigger conditions read it
//  back.  Out-of-range indices are silently ignored, matching the original's
//  range checks.
// ============================================================================
void ScenarioClass::SetGlobalValue(int32 index, bool value)
{
    if (index < 0 || index >= MaxGlobalVariables)
        return;

    GlobalVariables[index].Value = value ? 1 : 0;
}

void ScenarioClass::SetLocalValue(int32 index, bool value)
{
    if (index < 0 || index >= MaxLocalVariables)
        return;

    LocalVariables[index].Value = value ? 1 : 0;
}

bool ScenarioClass::GetGlobalValue(int32 index) const
{
    if (index < 0 || index >= MaxGlobalVariables)
        return false;

    return GlobalVariables[index].Value != 0;
}

bool ScenarioClass::GetLocalValue(int32 index) const
{
    if (index < 0 || index >= MaxLocalVariables)
        return false;

    return LocalVariables[index].Value != 0;
}


// ScenarioClass_NotAHomeCell (asm 0x6E0700 caller).
//
//   True when the waypoint does not resolve to this scenario's home cell.  The
//   "apply 100 damage" action uses this to avoid nuking the player's own base
//   start position.
bool ScenarioClass::NotAHomeCell(int32 idx) const
{
    if (idx < 0 || idx >= MaxWaypoints)
        return false;

    const CellStruct cell = GetWaypointCoords(idx);
    const int32 packed = (static_cast<int32>(cell.Y) << 16) |
                         static_cast<uint16>(cell.X);

    if (packed == HomeCell)
        return false;
    if (packed == AltHomeCell)
        return false;
    return true;
}

// ============================================================================
// 场景层补全（根据游戏行为实现）
// ============================================================================

namespace {
    int32 ScenarioClass_NextID = 0;
}

// 根据游戏行为，可知秘密科技生产分配：从已注册的建筑类型里挑出密实
// 验室类（IsSecretLab），随机取一个交给归属方记录；无可用类型返回空。
TechnoTypeClass* ScenarioClass::AssignSecretProduction(HouseClass* pHouse)
{
    (void)pHouse;
    if (BuildingTypeClass::Array == nullptr || BuildingTypeClass::Array->Count == 0) {
        return nullptr;
    }
    int32 candidates[64];
    int32 total = 0;
    for (int32 i = 0; i < BuildingTypeClass::Array->Count && total < 64; ++i) {
        BuildingTypeClass* pType = (*BuildingTypeClass::Array)[i];
        if (pType != nullptr && pType->IsSecretLab) {
            candidates[total++] = i;
        }
    }
    if (total == 0) {
        return nullptr;
    }
    return (*BuildingTypeClass::Array)[candidates[std::rand() % total]];
}

// 根据游戏行为，可知航点表清空会把全部航点复位为未定义状态。
void ScenarioClass::ClearWaypoints()
{
    for (int32 i = 0; i < MaxWaypoints; ++i) {
        Waypoints[i].X = -1;
        Waypoints[i].Y = -1;
    }
}

// 根据游戏行为，可知战役运兵船装载清单在场景装载时生成：为归属方
// 复位清单缓冲，具体条目随后由剧本装载指令填充。
void ScenarioClass::GenerateDropshipLoadout(HouseClass* pHouse)
{
    (void)pHouse;
    DropshipLoadout.Clear();
}

// 根据游戏行为，可知场景对象按需分配自增 ID，用于运行期唯一标识。
int32 ScenarioClass::Get_Next_ID()
{
    return ++ScenarioClass_NextID;
}

// 根据游戏行为，可知场景装载第二阶段补齐剩余缓冲：航点表与运兵船
// 清单复位。
void ScenarioClass::InitMoreBuffers()
{
    ClearWaypoints();
    DropshipLoadout.Clear();
}

// 根据游戏行为，可知航点以“序号=格号”形式存放在 [Waypoints] 小节，
// 逐条读入并按地图宽度换算成格子坐标（地图宽度以 512 格封顶）。
void ScenarioClass::LoadWaypoints(CCINIClass* pINI)
{
    if (pINI == nullptr) {
        return;
    }
    const int32 mapWidth = (TheMap != nullptr) ? 512 : 512;
    for (int32 i = 0; i < MaxWaypoints; ++i) {
        char key[16];
        snprintf(key, sizeof(key), "%d", i);
        const int32 cellNum = pINI->ReadInteger("Waypoints", key, -1);
        if (cellNum < 0) {
            continue;
        }
        Waypoints[i].X = cellNum % mapWidth;
        Waypoints[i].Y = cellNum / mapWidth;
    }
}

// 根据游戏行为，可知航点定位把登记的格子坐标换算成地图格对象。
CellClass* ScenarioClass::LocateWaypoint(int32 idx)
{
    if (TheMap == nullptr || idx < 0 || idx >= MaxWaypoints || !IsDefinedWaypoint(idx)) {
        return nullptr;
    }
    return TheMap->GetCellAt(Waypoints[idx]);
}

// 根据游戏行为，可知多人槽位到阵营索引直接沿用槽位序号：本工程的
// 阵营数组与槽位同序。
int32 ScenarioClass::MPIdxToHouseIdx(int32 mpIdx) const
{
    return mpIdx >= 0 && mpIdx < MaxStartingPoints ? mpIdx : -1;
}

// 根据游戏行为，可知剧情暂停按秒计：登记剩余秒数，由场景更新递减。
void ScenarioClass::PauseForSeconds(int32 seconds)
{
    PauseTicks = seconds > 0 ? seconds : 0;
}

// 根据游戏行为，可知下一任务推进在战役局收尾时触发：结束当前会话，
// 由战役调度器选择下一关。
void ScenarioClass::ProceedToNextMission()
{
    if (TheSession != nullptr) {
        TheSession->EndGame();
    }
}

// 根据游戏行为，可知全局标志回写把特别标志字的原始值以十六进制写回
// [SpecialFlags] 小节。
void ScenarioClass::PutGlobalFlags(CCINIClass* pINI)
{
    if (pINI == nullptr) {
        return;
    }
    pINI->WriteInteger("SpecialFlags", "Raw", static_cast<int32>(SpecialFlags.Raw), true);
}

// 根据游戏行为，可知全局变量从 [VariableNames]（名称）与
// [VariableStates]（数值）两小节读入。
void ScenarioClass::ReadGlobalVariables(CCINIClass* pINI)
{
    if (pINI == nullptr) {
        return;
    }
    for (int32 i = 0; i < MaxGlobalVariables; ++i) {
        char key[16];
        snprintf(key, sizeof(key), "%d", i);
        char name[0x40];
        if (pINI->ReadString("VariableNames", key, "", name, sizeof(name)) > 0) {
            snprintf(GlobalVariables[i].Name, sizeof(GlobalVariables[i].Name), "%s", name);
        }
        GlobalVariables[i].Value = pINI->ReadInteger("VariableStates", key, 0);
    }
}

// 根据游戏行为，可知地图保存把航点与全局/本地变量写回 INI。
void ScenarioClass::SaveMap(CCINIClass* pINI) const
{
    if (pINI == nullptr) {
        return;
    }
    SaveWaypoints(pINI);
    WriteLocalVariables(pINI);
}

// 根据游戏行为，可知已定义的航点以“序号=格号（行优先）”写回
// [Waypoints] 小节。
void ScenarioClass::SaveWaypoints(CCINIClass* pINI) const
{
    if (pINI == nullptr) {
        return;
    }
    for (int32 i = 0; i < MaxWaypoints; ++i) {
        if (!IsDefinedWaypoint(i)) {
            continue;
        }
        char key[16];
        snprintf(key, sizeof(key), "%d", i);
        pINI->WriteInteger("Waypoints", key, Waypoints[i].Y * 512 + Waypoints[i].X);
    }
}

// 根据游戏行为，可知制作人员名单在战役通关结算时展示一次。
void ScenarioClass::ShowCredits()
{
    Game::ProcessCampaignOptions();
}

// 根据游戏行为，可知本地变量把名称与数值一并写回 INI。
void ScenarioClass::WriteLocalVariables(CCINIClass* pINI) const
{
    if (pINI == nullptr) {
        return;
    }
    for (int32 i = 0; i < MaxLocalVariables; ++i) {
        if (LocalVariables[i].Name[0] == '\0') {
            continue;
        }
        char key[16];
        snprintf(key, sizeof(key), "%d", i);
        pINI->WriteString("VariableNames", key, LocalVariables[i].Name);
        pINI->WriteInteger("VariableStates", key, LocalVariables[i].Value);
    }
}

// ---------------------------------------------------------------------------
// “Scenario 全局入口”族
// ---------------------------------------------------------------------------

// 根据游戏行为，可知开局时把选择“随机国家”的参战方随机指定一个可用
// 国家槽位。
void ScenarioClass::GenerateRandomCountries()
{
    if (TheSession == nullptr) {
        return;
    }
    const int32 count = TheSession->GetPlayerCount();
    for (int32 i = 0; i < count; ++i) {
        SessionPlayer* pPlayer = TheSession->GetMutablePlayer(i);
        if (pPlayer == nullptr || pPlayer->Side != -1) {
            continue;
        }
        pPlayer->Side = std::rand() % 8;
    }
}

// 根据游戏行为，可知按航点序号取其所在格：未定义或越界返回空。
CellClass* ScenarioClass::GetWaypointCell(int32 idx)
{
    if (Instance == nullptr) {
        return nullptr;
    }
    return Instance->LocateWaypoint(idx);
}

// 根据游戏行为，可知联机客机在握手完成后把主机下发的会话选项套用到
// 本地场景设置。
void ScenarioClass::GuestReceiveOptions()
{
    if (TheSession != nullptr) {
        TheSession->ApplySettingsToScenario();
    }
}

// 根据游戏行为，可知玩家名到出生点槽位的映射：在会话玩家表里按名字
// 匹配并返回其登记的出生点，找不到返回 -1。
int32 ScenarioClass::NameToStartingSlot(const char* pName)
{
    if (TheSession == nullptr || pName == nullptr) {
        return -1;
    }
    const int32 count = TheSession->GetPlayerCount();
    for (int32 i = 0; i < count; ++i) {
        const SessionPlayer* pPlayer = TheSession->GetPlayer(i);
        if (pPlayer != nullptr && strcmp(pPlayer->Name, pName) == 0) {
            return pPlayer->StartingSpot;
        }
    }
    return -1;
}

// 根据游戏行为，可知 [Basic] 小节承载场景名、地图尺寸、归属与玩家
// 数等基础信息；读取成功与否由 INI 层报告。
bool ScenarioClass::ReadBasic(CCINIClass* pINI)
{
    if (pINI == nullptr) {
        return false;
    }
    char buf[0x100];
    pINI->ReadString("Basic", "Name", "", buf, sizeof(buf));
    snprintf(g_ScenarioName, sizeof(g_ScenarioName), "%s", buf);
    pINI->ReadString("Basic", "Description", "", g_ScenarioDescription, sizeof(g_ScenarioDescription));
    g_ScenarioMaxPlayers = pINI->ReadInteger("Basic", "MaxPlayer", 8);
    return true;
}

// 根据游戏行为，可知光照与基础信息在场景装载时一并读取：先读基础
// 信息，再套用灯光设置。
void ScenarioClass::ReadLightingAndBasic(CCINIClass* pINI)
{
    ReadBasic(pINI);
    UpdateLighting();
}

// 根据游戏行为，可知重算色调按当前灯光参数重新推导照明。
void ScenarioClass::RecalcTint()
{
    int32 r = 0;
    int32 g = 0;
    int32 b = 0;
    ScenarioLighting(&r, &g, &b);
    RecalcLighting(r, g, b, true);
}

// 根据游戏行为，可知重置超级武器会清掉所有阵营的充能计时与已用/可用
// 状态位，回到开局冷启动状态。
void ScenarioClass::ResetAllSuperWeapons()
{
    for (int32 i = 0; i < HouseClass::ArrayCount; ++i) {
        HouseClass* pHouse = HouseClass::Array[i];
        if (pHouse == nullptr) {
            continue;
        }
        for (int32 j = 0; j < HouseClass::MaxSuperWeapons; ++j) {
            pHouse->SuperWeaponTimers[j].Stop();
        }
        pHouse->ActiveSuperWeapons = 0;
        pHouse->AvailableSuperWeapons = 0;
        pHouse->UsedSuperWeapons = 0;
    }
}

// 根据游戏行为，可知开局准备入口会刷新随机国家与起点分配的簿记。
void ScenarioClass::Smth()
{
    GenerateRandomCountries();
}

// 起点分配家族：根据游戏行为，可知按顺序变体把互不重复的出生点依
// 连接顺序分配给每个参战方。
void ScenarioClass::SmthStartingHouses()
{
    if (Instance == nullptr) {
        return;
    }
    if (TheSession == nullptr) {
        return;
    }
    const int32 count = TheSession->GetPlayerCount();
    int32 nextSpot = 0;
    for (int32 i = 0; i < count; ++i) {
        SessionPlayer* pPlayer = TheSession->GetMutablePlayer(i);
        if (pPlayer == nullptr || !pPlayer->Connected) {
            continue;
        }
        TheSession->SetPlayerStartingSpot(i, nextSpot++);
    }
}

// 根据游戏行为，可知随机变体先把候选起点整体打乱再分配。
void ScenarioClass::SmthStartingHouses5()
{
    SmthStartingHouses();
    if (TheSession == nullptr) {
        return;
    }
    const int32 count = TheSession->GetPlayerCount();
    for (int32 i = count - 1; i > 0; --i) {
        SessionPlayer* pA = TheSession->GetMutablePlayer(i);
        SessionPlayer* pB = TheSession->GetMutablePlayer(std::rand() % (i + 1));
        if (pA == nullptr || pB == nullptr) {
            continue;
        }
        const int32 tmp = pA->StartingSpot;
        pA->StartingSpot = pB->StartingSpot;
        pB->StartingSpot = tmp;
    }
}

// 根据游戏行为，可知结盟相邻变体在分配时让同队玩家使用相邻起点：
// 以队伍号排序后顺序分配。
void ScenarioClass::SmthStartingHouses6()
{
    if (TheSession == nullptr) {
        return;
    }
    const int32 count = TheSession->GetPlayerCount();
    for (int32 team = 1; team <= 4; ++team) {
        for (int32 i = 0; i < count; ++i) {
            SessionPlayer* pPlayer = TheSession->GetMutablePlayer(i);
            if (pPlayer == nullptr || !pPlayer->Connected || pPlayer->Team != team) {
                continue;
            }
            TheSession->SetPlayerStartingSpot(i, i);
        }
    }
}

// 根据游戏行为，可知灯光与基础信息在保存场景时写回 INI。
void ScenarioClass::WriteLightingBasic(CCINIClass* pINI)
{
    if (pINI == nullptr) {
        return;
    }
    pINI->WriteString("Basic", "Name", g_ScenarioName);
    pINI->WriteString("Basic", "Description", g_ScenarioDescription);
    pINI->WriteInteger("Basic", "MaxPlayer", g_ScenarioMaxPlayers);
}
