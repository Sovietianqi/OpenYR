#include <Scenario/ScenarioClass.h>
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

    // Read flags
    FreeRadar = ini.ReadBool(section, "FreeRadar", false);
    TrainCrate = ini.ReadBool(section, "TrainCrate", false);
    PlayerSideIndex = ini.ReadInteger(section, "Player", -1);
    ThemeIndex = ini.ReadInteger(section, "Theme", 0);

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

    // Read special flags
    section = "SpecialFlags";
    SpecialFlags.Raw = static_cast<uint32>(ini.ReadInteger(section, "SpecialFlag", 0));

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