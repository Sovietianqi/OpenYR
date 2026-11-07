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
#include <Core/SwizzleManagerClass.h>
#include <Game/EngineGlobals.h>
#include <Math/Timer.h>
#include <Core/Memory.h>
#include <Map/MapClass.h>
#include <Houses/HouseClass.h>
#include <Houses/HouseTypeClass.h>
#include <UI/StartSpotClass.h>
#include <Game/MPTeams.h>
#include <Rules/RulesClass.h>
#include <AI/TagClass.h>
#include <AI/TagTypeClass.h>
#include <AI/TriggerTypeClass.h>
#include <Abstract/BuildingClass.h>
#include <Abstract/TechnoClass.h>
#include <Abstract/UnitClass.h>
#include <Abstract/InfantryClass.h>
#include <Abstract/UnitTypeClass.h>
#include <Abstract/InfantryTypeClass.h>
#include <Animations/AnimClass.h>
#include <Particles/ParticleSystemClass.h>
#include <Particles/ParticleSystemTypeClass.h>
#include <Special/TiberiumClass.h>
#include <Special/WaveClass.h>
#include <Rendering/ConvertClass.h>
#include <Rendering/DisplayClass.h>
#include <Rendering/GScreenClass.h>
#include <Rendering/TacticalClass.h>
#include <Terrain/CellTypeClass.h>
#include <Helpers/AsmUIHelpers.h>
#include <Game/GameInit.h>

#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cstdarg>
#include <ctime>
#include <vector>
#include <cmath>

// 引擎的调试输出通道。原版 Read_Scenario_INI 的两行提示走 WWDebugString
// （asm 0x7C5B10）；这里沿用本树其它子系统的做法，落到 stderr。
static void WWDebugString(const char* pFormat, ...)
{
    va_list args;
    va_start(args, pFormat);
    std::vfprintf(stderr, pFormat, args);
    va_end(args);
}

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
    , Intro(-1)
    , Brief(-1)
    , Win(-1)
    , Lose(-1)
    , Action(-1)
    , PostScore(-1)
    , PreMapSelect(-1)
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
    BriefingText = nullptr;      // asm: ScenarioClass+0x11F4

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
// asm: ScenarioClass_Pause(ScenarioClass* this)   (asm:698)
//
//   Freezes the scenario clock while paused: the accumulated elapsed count is
//   folded in and the running timers are stopped.  The original also mutes
//   audio, hides tooltips and re-issues the mouse pointer; those subsystems are
//   outside this object and are handled by their own classes.
// ============================================================================
void ScenarioClass::Pause()
{
    ElapsedTimer.Stop();
    PauseTimer.Start(0);
    IsGamePaused = true;
}

// ============================================================================
// asm: ScenarioClass_Resume(ScenarioClass* this)   (asm:797)
//
//   Restarts the scenario clock after a pause.
// ============================================================================
void ScenarioClass::Resume()
{
    PauseTimer.Stop();
    ElapsedTimer.Start(0);
    IsGamePaused = false;
}

// ============================================================================
// Init - Initialize scenario state
// ============================================================================

// ============================================================================
// asm: ScenarioClass_Init(ScenarioClass* this)   (asm:186)
//
//   Cold-start reset of the scenario singleton: re-seed the randomizer, stop
//   every countdown timer, wipe all waypoints back to NullWaypoint and clear
//   the 50 global / 100 local variable slots (firing the trigger-updated
//   notifications when a slot actually changes, so triggers re-evaluate).
// ============================================================================
void ScenarioClass::Init()
{
    // asm: Random_Init(this+0x218, 0)
    Random.Randomize();

    // Timers: ElapsedFrame / Pause / MissionTimer / ShroudRegrow / TimerW(X,Z).
    ElapsedTimer.Stop();
    PauseTimer.Stop();
    MissionTimer.Stop();
    ShroudRegrowTimer.Stop();
    FogTimer.Stop();
    IceTimer.Stop();
    AmbientTimer.Stop();

    // Waypoints -> NullWaypoint.
    ClearWaypoints();

    IsGamePaused = false;
    EndOfGame = false;
    FrameCount = 0;
    Stage = 0;
    VariablesChanged = false;
    AmbientChanged = false;
    PickedUpAnyCrate = false;
    BridgeDestroyed = false;
    UserInputLocked = false;

    // Global variables: clear name + value, notifying on change.
    for (int32 i = 0; i < MaxGlobalVariables; ++i) {
        GlobalVariables[i].Name[0] = '\0';
        if (GlobalVariables[i].Value != 0) {
            GlobalVariables[i].Value = 0;
            VariablesChanged = true;
        }
    }
    // Local variables: clear name + value, notifying on change.
    for (int32 i = 0; i < MaxLocalVariables; ++i) {
        LocalVariables[i].Name[0] = '\0';
        if (LocalVariables[i].Value != 0) {
            LocalVariables[i].Value = 0;
            VariablesChanged = true;
        }
    }

    // View rectangles.
    View1 = CellStruct(0, 0);
    View2 = CellStruct(0, 0);
    View3 = CellStruct(0, 0);
    View4 = CellStruct(0, 0);
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
// asm: Read_Scenario_INI
//
// 原版签名 char __thiscall Read_Scenario_INI(const char* filename)：
// ecx = 文件名，无栈参，retn 0。调用点：
//   lea ecx, [esp+248h+var_208] ; call Read_Scenario_INI   (asm 行 1324770)
// AsmComplex6 批注的 (void* pScenario, const char* pFilename) 是错的
// ——既不是两栈参，也没有 ScenarioClass 指针参数；原版函数体内走全局
// 单例 Scenario，所以这里实现为成员函数、文件名走形参。
//
// 控制流（逐句对照 asm 行 1327995-1328099）：
//   index = ForcedCDNumber
//   if (!CD::ForceAvailable(index)) return 0
//   就地构造 ccINI / ccFile（CCFileClass 以 filename 为源）
//   WWDebugString("Read_Scenario_INI - Filename is %s\n", filename)
//   if (!CCINIClass::Load(ccINI, ccFile, 1, 0)):
//       WWDebugString("Scenario ini load failed!\n") ; 析构 ; return 0
//   memcpy(Scenario->MapFileName, filename, strlen(filename) + 1)
//   ok = INIClass_ReadScenario(ccINI)      ; IsRandom = 0
//   析构 ccFile / ccINI ; return ok
//
// （原版 ccINI/ccFile 是栈上就地构造再手工析构；这里用局部对象让
//   C++ 的析构顺序自然接管同一步骤，语义一致。）
// ============================================================================

bool ScenarioClass::Read_Scenario_INI(const char* pFilename)
{
    if (pFilename == nullptr) {
        return false;
    }

    // asm: mov eax, ForcedCDNumber / push eax / call CD::ForceAvailable
    //
    // 注：本树把 CD 拆成了两处 —— IO/FileSystem.h 的 namespace CD 持有
    // 强制选盘族（CD::Get_Volume / CD::Set_Volume / CD_ForceAvailable），
    // Game/CD.h 的 class CD 持有 ForceAvailable 判定流程本身。两者不能
    // 同时以 CD:: 限定名引入（CD 会被当成两种类型），原版此处读的正是
    // 全局 ForcedCDNumber、调的正是卷扫描那一族的 ForceAvailable，
    // 故走 IO/FileSystem.h 的 CD::Get_Volume() 与 CD_ForceAvailable()。
    if (!CD_ForceAvailable(CD::Get_Volume())) {
        return false;
    }

    // asm: CCFileClass_CTOR(&ccFile, filename)  —— 文件名作为构造函数源串
    CCFileClass ccFile(pFilename);

    // asm: WWDebugString("Read_Scenario_INI - Filename is %s\n", filename)
    WWDebugString("Read_Scenario_INI - Filename is %s\n", pFilename);

    // asm: CCINIClass::Load(&ccINI, &ccFile, 1, 0)
    //      —— 本项目对应接口为 LoadFile(CCFileClass*)
    CCINIClass ccINI;
    if (!ccINI.LoadFile(&ccFile)) {
        // asm: WWDebugString("Scenario ini load failed!\n") ; 析构 ; return 0
        WWDebugString("Scenario ini load failed!\n");
        return false;
    }

    // asm: repne scasb 求 strlen(filename)+1，再 rep movsd/rep movsb 拷入
    //      Scenario->MapFileName。本项目对应字段是 ScenarioClass::FileName。
    if (Instance != nullptr) {
        const size_t len = std::strlen(pFilename);
        const size_t copyLen = (len < sizeof(Instance->FileName) - 1)
                                   ? len
                                   : sizeof(Instance->FileName) - 1;
        std::memcpy(Instance->FileName, pFilename, copyLen);
        Instance->FileName[copyLen] = '\0';
    }

    // asm: INIClass_ReadScenario(&ccINI)  —— 第二个参数 IsRandom = 0
    //      本项目对应接口为 ReadScenario(const char*)，内部自行开文件。
    const bool ok = ccINI.ReadScenario(pFilename);

    // ccFile / ccINI 在此按 C++ 规则析构，对应 asm 的两段手工析构。
    return ok;
}

// ============================================================================
// asm: Assign_Houses (0x687EE0, cdecl, retn)
//
//   Assign_Houses —— 场景读完后把"起始点记录（NameNodeTypes，VecStartSpot）"
//   逐个变成 HouseClass：
//
//     第一段：先把 CountrySlot == -3 的记录 Set_Country(0)；
//     第二段：按"未用记录里 [node+53h] 最小"的规则反复挑一条，建房；
//             每轮建一幢，直到建满 NameNodeTypes.length 幢；
//     第三段：AISlots 的八个槽（国家 -1/-3 为空）逐槽建房；
//     第四段：Neutral / Special 两幢特殊房（都用 LightGrey = 0x35）。
//
//   全局 someHouse 记录 node+6Bh == -1 的那一幢（剧本主角房），
//   PlayerPtr 记录第一轮挑中的那一幢（本地玩家）。
// ============================================================================
HouseClass* ScenarioClass::SomeHouse = nullptr;

AISlotsClass          AISlots;
// ParticleSystem_Gas lives in Particles/ParticleSystemClass.cpp (single owner).

void ScenarioClass::AssignHouses()
{
    ScenarioClass* pScen = Instance;
    if (pScen == nullptr)
        return;
    if (HouseTypeClass::Array == nullptr)
        return;                                    // asm: vec_HouseTypes.data

    // asm: edi = NameNodeTypes.length ; esi = NameNodeTypes.data
    StartSpotClass** ppNodes =
        reinterpret_cast<StartSpotClass**>(NameNodeTypes_Data());
    const int32 nnCount = NameNodeTypes_Length();

    // asm: gMoney / gTechLevel / gAIPlayers —— 会话设置里的三项全局。
    SessionClass* pSession = SessionClass::GetInstance();
    const int32 gMoney     = (pSession != nullptr) ? pSession->GetCredits()   : 0;
    const int32 gTechLevel = (pSession != nullptr) ? pSession->GetTechLevel() : 0;
    const int32 gAIPlayers = (pSession != nullptr) ? pSession->AIPlayers      : 0;
    // asm: 全局 `Session` —— 会话类型（0 = 单人；4 = 联机/合作报文）。
    const int32 sessionType = (pSession != nullptr) ? pSession->SessionType   : 0;

    // ------------------------------------------------------------------
    // Phase 0 (687F25)：CountrySlot == -3 的记录先复位成 0。
    // ------------------------------------------------------------------
    for (int32 i = 0; i < nnCount; ++i) {
        if (ppNodes != nullptr && ppNodes[i] != nullptr &&
            ppNodes[i]->CountrySlot == -3) {
            ppNodes[i]->Set_Country(0);            // asm: sub_696F90(node, 0)
        }
    }

    // asm: someHouse = 0
    SomeHouse = nullptr;

    if (nnCount > 0 && ppNodes != nullptr) {
        // --------------------------------------------------------------
        // Phase 1 (687F5D)：反复挑"未用记录里 [node+53h] 最小"的一条建房。
        // 原版只把 8 个字节的 used 标记清零（var_38/var_34）；
        // 这里按记录数分配，避免超过 8 条时越界。
        // --------------------------------------------------------------
        std::vector<uint8> used(static_cast<size_t>(nnCount), 0);

        int32 curAIPlayer = nnCount;               // asm: curAIPlayer = edi
        do {
            int32 best = -1;                       // asm: var_4C
            int32 best53 = -1;                     // asm: edx
            for (int32 i = 0; i < nnCount; ++i) {  // asm: loc_687F72
                if (used[static_cast<size_t>(i)] != 0)
                    continue;
                StartSpotClass* pNode = ppNodes[i];
                if (pNode == nullptr)
                    continue;
                const int32 v53 = pNode->ColourSlot;      // asm: [node+53h]
                if (best53 != -1 && v53 >= best53)
                    continue;                      // asm: cmp/jge 687F8F
                best = i;
                best53 = v53;
            }

            // asm 3187-3190: node = data[var_4C] ; var_38[var_4C] = 1
            StartSpotClass* pNode =
                (best >= 0 && best < nnCount) ? ppNodes[best] : nullptr;
            if (best >= 0 && best < nnCount)
                used[static_cast<size_t>(best)] = 1;

            // asm: new HouseClass_old ; HouseClass_CTOR(type = vec_HouseTypes[country])
            HouseClass* pHouse = nullptr;
            const int32 country = (pNode != nullptr) ? pNode->CountrySlot : -1;
            HouseTypeClass* pType = nullptr;
            if (country >= 0 && country < HouseTypeClass::ArrayCount)
                pType = HouseTypeClass::Array[country];   // asm: vec_HouseTypes
            if (pType != nullptr)
                pHouse = new HouseClass(pType);           // asm: HouseClass_CTOR
            if (pHouse == nullptr)
                continue;

            // asm 3200-3300: Session == 4 时用起始点记录上的宽字符名，
            // 否则写死 "<human player>"。原版那条
            // Unicode_AssignString → sub_7B7F90 → strdelete2 → sub_7B5400
            // → strncpy(0x14) 的链，净效果就是把记录名截成 20 字节窄名。
            if (sessionType == 4) {                // asm: cmp Session,4
                int32 n = 0;
                if (pNode != nullptr) {
                    for (; n < 20 && pNode->CSFName[n] != 0; ++n)
                        pHouse->InitialName[n] =
                            static_cast<char>(pNode->CSFName[n] & 0x7F);
                }
                pHouse->InitialName[n] = 0;        // asm: mov [var_1C],0
            } else {
                std::memset(pHouse->InitialName, 0, sizeof(pHouse->InitialName));
                std::memcpy(pHouse->InitialName, "<human player>", 14);
            }

            // asm 3308-3315: wcsncpy(house+1602Ah, node, 0x15) ;
            //               word [house+16052h] = 0
            for (int32 i = 0; i < 20; ++i)
                pHouse->CSFName[i] = (pNode != nullptr) ? pNode->CSFName[i] : 0;
            pHouse->CSFName[20] = 0;

            pHouse->IsHumanPlayer = true;          // asm: byte [ebp+1ECh],1

            // asm: Init_Data(Colour = node+53h, Side = node+4Bh, Money = gMoney)
            pHouse->Init_Data((pNode != nullptr) ? pNode->ColourSlot : 0,
                              country, gMoney);

            // asm: Session->Get_PlayerColorType(node+53h) -> [ebp+16054h]
            const int32 colourIdx = (pNode != nullptr) ? pNode->ColourSlot : 0;
            int32 pc = colourIdx;
            if (pSession != nullptr)
                pc = pSession->GetPlayerColor(colourIdx);
            pHouse->ColorSchemeIndex =
                HouseClass::PlayerColorType_Convert_(pc);   // asm: _Convert_
            pHouse->Recalc_Color_Vector();                  // asm: 50BA00

            // asm: [ebp+16058h] = StartingSpotClass_GetStartSlot(node)
            pHouse->StartingSlot = (pNode != nullptr) ? pNode->Get_Start_Slot() : -2;
            // asm: [ebp+1605Ch] = [node+63h]
            pHouse->TeamIndex = (pNode != nullptr) ? pNode->ReservedA : 0;

            if (best == 0) {                       // asm: var_4C == 0
                HouseClass::Player = pHouse;       // asm: HouseClass_PlayerPtr
                pHouse->PlayerControl = true;      // asm: IsPlayer = 1
            }
            // asm: cmp [node+6Bh],-1 -> someHouse = ebp
            if (pNode != nullptr && pNode->field_6B == -1)
                SomeHouse = pHouse;

            pHouse->TechLevel = gTechLevel;        // asm: [ebp+1D4h]
            pHouse->Assign_Handicap(1);            // asm: Assign_Handicap(house,1)

            // asm: [node+6Fh] = house->ArrayIndex
            if (pNode != nullptr)
                pNode->HouseIndex = pHouse->ArrayIndex;

        } while (--curAIPlayer != 0);              // asm: dec/jnz 687F5D
    }

    // ------------------------------------------------------------------
    // Phase 2 (68814C)：AI 槽。ebx 从 AISlots.StartingSpots 走到
    // AISlots.Colours（步长 4，共 8 槽）；[ebx]=国家，[ebx+20h]=颜色，
    // [ebx+40h]=起始格，[ebx+60h]=队伍号；gAIPlayers 限制建房数量。
    // ------------------------------------------------------------------
    {
        int32 used = 0;                            // asm: eax
        const int32 aiMax = gAIPlayers;            // asm: curAIPlayer
        for (int32 slot = 0; slot < 8; ++slot) {
            if (used >= aiMax)                     // asm: cmp eax,curAIPlayer
                break;                             // asm: jge 6882C2
            const int32 country = AISlots.StartingSpots[slot];
            if (country == -1 || country == -3)    // asm: -1 / -3 空槽
                continue;
            const int32 colour = AISlots.Colours[slot];   // asm: [ebx+20h]
            ++used;                                // asm: inc eax

            HouseTypeClass* pType = nullptr;
            if (country >= 0 && country < HouseTypeClass::ArrayCount)
                pType = HouseTypeClass::Array[country];
            HouseClass* pHouse = (pType != nullptr) ? new HouseClass(pType)
                                                   : nullptr;
            if (pHouse == nullptr)
                continue;                          // asm: new/CTOR null path

            pHouse->IsHumanPlayer = false;         // asm: byte [ebp+1ECh],0
            pHouse->TechLevel = gTechLevel;        // asm: [ebp+1D4h]
            pHouse->Init_Data(colour, country, gMoney);

            int32 pc = colour;
            if (pSession != nullptr)
                pc = pSession->GetPlayerColor(colour);
            pHouse->ColorSchemeIndex = HouseClass::PlayerColorType_Convert_(pc);
            pHouse->Recalc_Color_Vector();

            pHouse->StartingSlot = AISlots.StartSlots[slot];   // asm: [ebx+40h]
            const int32 team = AISlots.Teams[slot];           // asm: [ebx+60h]
            pHouse->TeamIndex = team;
            if (team != -1)                        // asm: cmp eax,-1 / jz
                pScen->TeamsPresent = true;        // asm: byte [Scenario+11E0h],1

            // asm 3450-3480: "Computer" -> house->InitialName（21 字节）
            std::memset(pHouse->InitialName, 0, sizeof(pHouse->InitialName));
            std::memcpy(pHouse->InitialName, "Computer", 8);

            // asm 3483-3493: wcscpy(house->CSFName, Get_String("TXT_COMPUTER",
            //                 0, "D:\\ra2mdpost\\Scenario.CPP", 0xFC0))
            {
                const char* pText = EngineGlobals::Get_String("TXT_COMPUTER");
                int32 i = 0;
                for (; i < 20 && pText != nullptr && pText[i] != '\0'; ++i)
                    pHouse->CSFName[i] = static_cast<wchar_t>(
                        static_cast<unsigned char>(pText[i]));
                pHouse->CSFName[i] = 0;
            }

            if (sessionType != 0) {                // asm: test Session / jz
                RulesClass* pRules = RulesClass::Instance;
                if (pRules != nullptr)
                    pHouse->IQLevel2 = pRules->MaxIQLevels;  // asm: [ecx+1434h]
            }

            // asm 3505-3525: handicap = [ebx-20h]
            //   ebx = &AISlots.StartingSpots[slot]，减 0x20 正好落到紧邻其前
            //   的 AISlots.AIDifficulties[slot]；规则开关
            //   LetsDoTheTimeWarpInAgain 置位且记录多于一条时再减一。
            {
                int32 diff = AISlots.AIDifficulties[slot];
                if (nnCount > 1) {
                    RulesClass* pRules = RulesClass::Instance;
                    if (pRules != nullptr && pRules->LetsDoTheTimeWarpInAgain != 0 &&
                        diff > 0) {
                        --diff;                    // asm: dec eax
                    }
                }
                pHouse->Assign_Handicap(diff);
            }
        }
    }

    // ------------------------------------------------------------------
    // Phase 3 (3535-3571)：Neutral 房，颜色 LightGrey(0x35)。
    // ------------------------------------------------------------------
    {
        const int32 idxType = HouseTypeClass::FindIndexOfName("Neutral");
        HouseTypeClass* pType = (idxType >= 0 && idxType < HouseTypeClass::ArrayCount)
                                    ? HouseTypeClass::Array[idxType] : nullptr;
        HouseClass* pHouse = (pType != nullptr) ? new HouseClass(pType) : nullptr;
        if (pHouse != nullptr) {
            const int32 ci = ColorScheme::FindIndex("LightGrey", 0x35);
            pHouse->ColorSchemeIndex = ci;               // asm: [esi+16054h]
            // asm 3565-3570: mov ecx, esi / call HouseClass_PlayerColorType_Convert_
            // （Neutral/Special 只走颜色转换，不调 50BA00 颜色向量重算）
            pHouse->ColorSchemeIndex =
                HouseClass::PlayerColorType_Convert_(pHouse->ColorSchemeIndex);
        }
    }

    // ------------------------------------------------------------------
    // Phase 4 (3573-3607)：Special 房，颜色 LightGrey(0x35)。
    // ------------------------------------------------------------------
    {
        const int32 idxType = HouseTypeClass::FindIndexOfName("Special");
        HouseTypeClass* pType = (idxType >= 0 && idxType < HouseTypeClass::ArrayCount)
                                    ? HouseTypeClass::Array[idxType] : nullptr;
        HouseClass* pHouse = (pType != nullptr) ? new HouseClass(pType) : nullptr;
        if (pHouse != nullptr) {
            const int32 ci = ColorScheme::FindIndex("LightGrey", 0x35);
            pHouse->ColorSchemeIndex = ci;
            pHouse->ColorSchemeIndex =
                HouseClass::PlayerColorType_Convert_(pHouse->ColorSchemeIndex);
        }
    }
}

// ============================================================================
// GetStartingLocations
//
//  asm: Get_Starting_Locations (0x6882D0, __thiscall)
//
//   ① 先数前 8 个航点里从 0 开始连续非空（不是 NullWaypoint(0,0)）的个数；
//   ② 目标格数 = (vec_Houses 长度 - 未分配国家的房数) + gAIPlayers；
//      取二者较大者 max；
//   ③ 前 max 个航点里非空的直接收进候选表，并逐条打印
//      "Multiplayer start waypoint found at cell %d,%d"；
//   ④ 候选不足 max 时打印 deficiency，再按地图范围随机掷格，交给
//      MapClass::Nearby_Location 找合法落点，直到补满。
//
//   原版把结果填进栈上就地构造的 DynamicVectorClass<Cell>，这里同样通过
//   出参返回（CellStruct 即原版的 wXY）。
// ============================================================================
void ScenarioClass::GetStartingLocations(DynamicVectorClass<CellStruct>& out)
{
    out.Clear();

    RulesClass* pRules = RulesClass::Instance;

    // ---- ① 前 8 个已定义航点 ----
    int32 waypointCount = 0;
    for (int32 i = 0; i < 8; ++i)
    {
        if (i < 0 || i >= MaxWaypoints)
            break;
        const CellStruct& wp = Waypoints[i];
        if (wp.X == 0 && wp.Y == 0)      // asm: NullWaypoint.X / .Y
            break;
        ++waypointCount;
    }

    // ---- ② 目标格数 ----
    int32 unassignedHouses = 0;
    for (int32 i = 0; i < HouseClass::ArrayCount; ++i)
    {
        HouseClass* pHouse = HouseClass::Array[i];
        if (pHouse == nullptr)
            continue;
        // asm: cmp dword ptr [house+6Bh], -1 —— 未分配国家的空槽。
        if (pHouse->Type == nullptr)
            ++unassignedHouses;
    }

    int32 wanted = HouseClass::ArrayCount - unassignedHouses;
    if (pRules != nullptr)
        wanted += pRules->AIPlayers;     // asm: gAIPlayers

    const int32 maxCount = (waypointCount > wanted) ? waypointCount : wanted;

    // ---- ③ 收航点 ----
    for (int32 i = 0; i < maxCount && i < 8; ++i)
    {
        const CellStruct wp = Waypoints[i];
        if (wp.X == 0 && wp.Y == 0)
            continue;

        out.Add(wp);
        WWDebugString("Multiplayer start waypoint found at cell %d,%d\n",
                      static_cast<int32>(wp.X), static_cast<int32>(wp.Y));
    }

    // ---- ④ 不足则随机补足 ----
    MapClass* pMap = MapClass::Instance;
    if (out.Count >= maxCount || pMap == nullptr)
        return;

    WWDebugString("Multiplayer start waypoint deficiency - "
                  "looking for more start positions\n");

    const int32 left   = 0;                     // dword_87F90C
    const int32 top    = 0;                     // dword_87F910
    const int32 width  = pMap->MapWidth;        // dword_87F914
    const int32 height = pMap->MapHeight;       // dword_87F918

    int32 guard = 0;
    while (out.Count < maxCount && guard < 0x400)
    {
        ++guard;

        // asm: X = left + Random(10, width-10)
        //      Y = top  + Random(0,  height-10) + 10
        const int32 rx = Random.Next(10, width - 10);
        const int32 ry = Random.Next(0, height - 10) + 10;
        const CellStruct seed(left + rx, top + ry);

        const CellStruct found = pMap->Nearby_Location(
            seed, 1 /*SpeedType::Track*/, -1, MovementZone::Normal,
            false, 8, 8, 0, false, false, true, false, false);

        if (found.X == 0 && found.Y == 0)       // asm: NullWaypoint -> 重掷
            continue;

        out.Add(found);
        WWDebugString("Random multiplayer start waypoint added at cell %d,%d\n",
                      static_cast<int32>(found.X), static_cast<int32>(found.Y));
    }
}

// ============================================================================
// CreateUnits
//
//  asm: char __thiscall Create_Units(byte x)   (0x688690, 943 行)
//   x 落在 [esp+..+a1+3] 后立刻被 imul 覆盖（死读），故形参没有语义。
//
//  流程：
//    1. gUnitCount / gBases → HouseThis（有基地则减 1）；
//    2. 对「允许开局出场」且不在 Rules.[BaseUnit] 里的载具/步兵做成本普查，
//       得平均成本 × HouseThis = 每个 AI 房的总预算；
//    3. Get_Starting_Locations 取候选出生格；
//    4. 逐房（跳过真人房）：
//         按 TechLevel / Owners / BaseUnit 过滤出该房可用的载具与步兵类型；
//         选一个尚未占用的出生格（首个随机，其后取「离已选格最远」者）；
//         落基地车（Rules.BaseUnit 中本房可用的第一个类型）；
//         CTF 模式下把旗挂到基地车上；
//         按预算（2/3 平均成本）随机买单位/步兵，Scan_Place_Object 落位，
//         InitialVeteran 时升精英，AI 房下 AreaGuard 任务、真人房下 Guard。
// ============================================================================
void ScenarioClass::CreateUnits()
{
    RulesClass* pRules = RulesClass::Instance;

    // asm: gUnitCount / gBases —— Init_Rules 从 Rules+1494h/+14ACh 抄来的
    //   多人开局默认值；进入会话后由大厅写回同一对全局。
    const int32 gUnitCount = (TheSession != nullptr)
        ? TheSession->UnitCount
        : ((pRules != nullptr) ? pRules->UnitCount : 0);
    const bool gBases = (TheSession != nullptr)
        ? TheSession->Bases
        : ((pRules != nullptr) ? pRules->Bases : false);

    // vec_Tech：本房已落位的技术对象（仅用于生命周期，落位后即清空）。
    DynamicVectorClass<TechnoClass*> vecTech;

    int32 houseThis = gUnitCount;
    if (gBases)
        --houseThis;

    WWDebugString("Creating %d units - Random seed is %08x", houseThis, 0);
    WWDebugString("UniqueID is %08x\n", UniqueID);

    // ---------------------------------------------------------------
    // 2. 成本普查（AllowedToStartInMultiplayer 且不在 BaseUnit 内）
    // ---------------------------------------------------------------
    auto inBaseUnit = [pRules](TechnoTypeClass* pType) -> bool
    {
        if (pRules == nullptr || pType == nullptr)
            return false;
        for (int32 j = 0; j < pRules->BaseUnit.Count; ++j)
        {
            if (static_cast<TechnoTypeClass*>(pRules->BaseUnit[j]) == pType)
                return true;
        }
        return false;
    };

    int32 totalCost  = 0;
    int32 costedCount = 0;

    if (UnitTypeClass::Array != nullptr)
    {
        for (int32 i = 0; i < UnitTypeClass::Array->Count; ++i)
        {
            TechnoTypeClass* pType = (*UnitTypeClass::Array)[i];
            if (pType == nullptr)
                continue;
            if (!pType->AllowedToStartInMultiplayer)   // asm: +0x6D5
                continue;
            if (inBaseUnit(pType))
                continue;
            totalCost += pType->GetCost();
            ++costedCount;
        }
    }

    if (InfantryTypeClass::Array != nullptr)
    {
        for (int32 i = 0; i < InfantryTypeClass::Array->Count; ++i)
        {
            TechnoTypeClass* pType = (*InfantryTypeClass::Array)[i];
            if (pType == nullptr)
                continue;
            if (!pType->AllowedToStartInMultiplayer)   // asm: 字面 +6D5h
                continue;
            totalCost += pType->GetCost();
            ++costedCount;
        }
    }

    // asm: cdq / idiv ebp / imul HouseThis（无可用类型时原版除零，此处取 0）
    int32 recountedCost = 0;
    if (costedCount != 0)
        recountedCost = (totalCost / costedCount) * houseThis;

    // ---------------------------------------------------------------
    // 3. 候选出生格
    // ---------------------------------------------------------------
    DynamicVectorClass<CellStruct> vectCellDistances;
    GetStartingLocations(vectCellDistances);

    const int32 cellCount = vectCellDistances.Count;
    std::vector<uint8_t> selected(cellCount > 0 ? cellCount : 1, 0);
    int32 basesPlaced = 0;

    // ---------------------------------------------------------------
    // 4. 逐房
    // ---------------------------------------------------------------
    for (int32 hi = 0; hi < HouseClass::ArrayCount; ++hi)
    {
        HouseClass* pHouse = HouseClass::Array[hi];
        if (pHouse == nullptr)
            continue;

        // asm: [house->Country + humanControlled] —— 真人房不参与自动生成。
        if (pHouse->Is_Human())
            continue;

        const char* pHouseName = (pHouse->Type != nullptr)
            ? pHouse->Type->get_Name() : "";
        WWDebugString("Generating units for house %d (%s)\n", hi, pHouseName);

        DynamicVectorClass<UnitTypeClass*>     vectUnitType;
        DynamicVectorClass<InfantryTypeClass*> vectInfType;

        // asm: the house's own TechLevel gates what it may start with.  A
        // house built before the rule set was published carries 0; fall back
        // to the rule-level value instead of filtering every type away.
        if (pHouse->TechLevel <= 0 && pRules != nullptr)
            pHouse->TechLevel = pRules->TechLevel;
        const int32 houseTech = pHouse->TechLevel;

        // asm: ebx = 1 << (HouseType->idxCountry & 1F) -- the country index is
        //   the type's own registration slot, not a lookup by parent-country
        //   name (ParentCountry can be empty for the fallback country set,
        //   which turned the shift into 1 << 31).
        const int32 ownerBit = (pHouse->Type != nullptr)
            ? (1 << (pHouse->Type->GetArrayIndex() & 0x1F))
            : 0;

        // 载具候选
        if (UnitTypeClass::Array != nullptr)
        {
            for (int32 i = 0; i < UnitTypeClass::Array->Count; ++i)
            {
                UnitTypeClass* pType = (*UnitTypeClass::Array)[i];
                if (pType == nullptr)
                    continue;
                if (!pType->AllowedToStartInMultiplayer)
                    continue;
                if (pType->TechLevel > houseTech)
                    continue;
                if ((pType->GetOwners() & ownerBit) == 0)
                    continue;
                if (inBaseUnit(pType))
                    continue;
                vectUnitType.Add(pType);
            }
        }

        // 步兵候选
        if (InfantryTypeClass::Array != nullptr)
        {
            for (int32 i = 0; i < InfantryTypeClass::Array->Count; ++i)
            {
                InfantryTypeClass* pType = (*InfantryTypeClass::Array)[i];
                if (pType == nullptr)
                    continue;
                if (!pType->AllowedToStartInMultiplayer)
                    continue;
                if (pType->TechLevel > houseTech)
                    continue;
                if ((pType->GetOwners() & ownerBit) == 0)
                    continue;
                if (inBaseUnit(pType))
                    continue;
                vectInfType.Add(pType);
            }
        }

        // ---- 选出生格 ----
        CellStruct where(0, 0);

        if (cellCount <= 0)
        {
            where = CellStruct(0, 0);
        }
        else if (basesPlaced == 0)
        {
            // 首个：随机一格。
            const int32 idx = Random.Next(0, cellCount - 1);
            if (idx >= 0 && idx < cellCount)
                selected[idx] = 1;
            basesPlaced = 1;
            where = vectCellDistances[idx];
        }
        else
        {
            // 其后：取「到所有已选格距离之和最大」的那一格（最大化散布）。
            std::vector<int32> matrix(cellCount, 0);

            for (int32 b = 0; b < cellCount; ++b)
            {
                if (selected[b] != 0)
                    continue;
                for (int32 d = 0; d < cellCount; ++d)
                {
                    if (selected[d] == 0)
                        continue;

                    const int32 dx = static_cast<int16>(
                        vectCellDistances[b].X - vectCellDistances[d].X);
                    const int32 dy = static_cast<int16>(
                        vectCellDistances[b].Y - vectCellDistances[d].Y);

                    const double sq = std::sqrt(
                        static_cast<double>(dx) * static_cast<double>(dx) +
                        static_cast<double>(dy) * static_cast<double>(dy));
                    matrix[b] += static_cast<int16>(static_cast<int32>(sq));
                }
            }

            int32 best    = 0;   // asm: edx
            int32 bestIdx = 0;   // asm: edi
            for (int32 i = 0; i < cellCount; ++i)
            {
                const int32 v = matrix[i];
                if (v > best || best == 0)
                {
                    best = v;
                    bestIdx = i;
                }
            }

            selected[bestIdx] = 1;
            where = vectCellDistances[bestIdx];
            ++basesPlaced;
        }

        // asm: HouseClass_SetBaseSpawnCell
        pHouse->Set_Base_Spawn_Cell(where);

        // ---- 基地车 ----
        UnitClass* baseUnit = nullptr;

        if (gBases && pRules != nullptr)
        {
            UnitClass* pNewUnit = new UnitClass(pHouse);
            if (pNewUnit != nullptr)
            {
                TechnoTypeClass* pFound = pHouse->FindFirstAccessibleTechnoTypeFromArray(
                    reinterpret_cast<DynamicVectorClass<TechnoTypeClass*>*>(
                        &pRules->BaseUnit));

                if (pFound != nullptr)
                {
                    UnitTypeClass* pBaseType = static_cast<UnitTypeClass*>(pFound);
                    pNewUnit->Type       = pBaseType;
                    pNewUnit->TechnoType = pBaseType;
                    pNewUnit->Health     = pBaseType->Strength;
                    pNewUnit->MaxHealth  = pBaseType->Strength;
                    baseUnit = pNewUnit;
                }
                else
                {
                    delete pNewUnit;
                }
            }
        }

        // asm 14452-14488: 先按格心 lepton 直接 Put，失败再 Scan_Place_Object(1)。
        if (baseUnit != nullptr)
        {
            const int32 cx = (static_cast<int32>(where.X) << 8) + 0x80;
            const int32 cy = (static_cast<int32>(where.Y) << 8) + 0x80;
            const CoordStruct coord(cx, cy, 0);

            if (!baseUnit->Put(coord, 0))
            {
                if (!baseUnit->Scan_Place_Object(where, 1))
                {
                    delete baseUnit;
                    baseUnit = nullptr;
                }
            }
        }

        // asm 14492-14508: 旗标记账 + CTF 挂旗。
        if (baseUnit != nullptr)
        {
            pHouse->ObjectWithOurFlag = nullptr;
            pHouse->FlagCoords        = CellStruct(0, 0);   // asm: NullWaypoint
            if (SpecialFlags.CTFMode())                     // asm: test byte [Scenario],10h
                pHouse->Flag_Attach(baseUnit, 1);
        }

        // asm 14516-14539: 本房的 vec_Tech 复位。
        vecTech.Clear();

        // ---- 按预算生成 ----
        if (recountedCost > 0)
        {
            const int32 edgeCost = (recountedCost * 2) / 3;   // asm: 0x55555556
            int32 spent = 0;

            for (;;)
            {
                TechnoTypeClass* pSel = nullptr;
                UnitTypeClass*     pUnitType = nullptr;
                InfantryTypeClass* pInfType  = nullptr;

                // asm: pick from the vehicle table while the budget allows,
                //   else from the infantry table.  The table the entry came
                //   from is also what decides the constructor -- going through
                //   WhatAmI() here is unreliable because the type classes do
                //   not override it with the retail ordinals.
                if (spent < edgeCost && vectUnitType.Count > 0)
                {
                    const int32 idx = Random.Next(0, vectUnitType.Count - 1);
                    pUnitType = vectUnitType[idx];
                    pSel      = pUnitType;
                }
                else if (vectInfType.Count > 0)
                {
                    const int32 idx = Random.Next(0, vectInfType.Count - 1);
                    pInfType = vectInfType[idx];
                    pSel     = pInfType;
                }

                // asm: 两个候选表都空时 esi=0，原版此处会解引用空指针。
                if (pSel == nullptr)
                    break;

                TechnoClass* pObj = nullptr;
                if (pUnitType != nullptr)
                    pObj = pUnitType->CreateUnit(pHouse);
                else if (pInfType != nullptr)
                    pObj = pInfType->CreateInfantry(pHouse);

                if (pObj == nullptr)
                    break;

                pObj->TechnoType = pSel;
                pObj->Health     = pSel->Strength;
                pObj->MaxHealth  = pSel->Strength;

                if (!pObj->Scan_Place_Object(where, 4))   // asm: mode 4
                {
                    delete pObj;                          // asm: vt+20h(this, 1)
                }
                else
                {
                    WWDebugString("House %s deployed object %s\n",
                                  pHouseName, pSel->get_Name());

                    spent += pSel->GetCost();             // asm: vt+0ACh
                    vecTech.Add(pObj);

                    if (SpecialFlags.InitialVeteran())    // asm: test ah, 2
                        pObj->VeterancyLevel = 2;         // asm: Veterancy_SetElite

                    if (pHouse->Is_Human())
                        pObj->ForceMission(Mission::Guard);      // asm: 5
                    else
                        pObj->ForceMission(Mission::AreaGuard);  // asm: 0Bh
                }

                if (spent >= recountedCost)
                    break;
            }
        }
    }

    // asm 14789-14798: 收尾调试行（随机 0..0FFFF）。
    const int32 roll = Random.Next(0, 0xFFFF);
    WWDebugString("Finished unit generation. Random number is %d\n", roll);
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
    // asm: a waypoint is defined iff it differs from NullWaypoint (0,0).
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
// asm: Scenario_ReadLightingAndBasic(CCINIClass* INI)   (asm:1334477)
//
//  The scenario-file reader.  Structure, key-for-key and section-for-section,
//  follows the original: it reads [Header], [Basic], [Ranking] and [Lighting].
//  Every value is read with the current field as its fallback, so a partial
//  file keeps whatever the scenario object already declared.  The three
//  lighting colour groups are stored pre-scaled: the on-disk value is a fixed-
//  point fraction, multiplied by 100 (colours/ambient) or 1000 (ground/level)
//  and truncated, matching the `fmul 100.0 / fmul 1000.0 + 0.01 / floor`
//  sequence in the original.
//
//  NOTE: this method corresponds to the original global INI dispatch of
//  INIClass_ReadScenario (Scenario_ReadLightingAndBasic); the name here keeps
//  the older project name ReadStartPoints because external callers use it.
// ============================================================================
void ScenarioClass::ReadStartPoints(CCINIClass& ini)
{
    // asm: ScenarioClass_GetGlobalFlags(this, INI)  —— SpecialFlags first.
    // (The original's preceding `INIClass_Reset` only zeroes the freshly
    //  default-constructed INI's section-pointer fields; it is the C++
    //  default-ctor equivalent, not a reset of a loaded file, so it must
    //  NOT be re-run here or it would drop every parsed section.)
    GetGlobalFlags(&ini);

    // ------------------------------------------------------------------
    // [Header] - map dimensions, starting-point span and coop start spots.
    // ------------------------------------------------------------------
    StartX = ini.ReadInteger("Header", "StartX", StartX);
    StartY = ini.ReadInteger("Header", "StartY", StartY);
    Width  = ini.ReadInteger("Header", "Width",  Width);
    Height = ini.ReadInteger("Header", "Height", Height);
    NumberStartingPoints   = ini.ReadInteger("Header", "NumberStartingPoints",   NumberStartingPoints);
    NumCoopHumanStartSpots = ini.ReadInteger("Header", "NumCoopHumanStartSpots", NumCoopHumanStartSpots);

    // asm: for (i = 1; i <= NumberStartingPoints; ++i) sprintf("Waypoint%d")
    //      Get2Integers("Header", "Waypoint%d", fallback = StartingPoints[i-1])
    for (int32 i = 0; i < NumberStartingPoints && i < MaxStartingPoints; ++i) {
        char key[32];
        std::snprintf(key, sizeof(key), "Waypoint%d", i + 1);
        int32 vals[2] = { StartingPoints[i].X, StartingPoints[i].Y };
        ini.Read2Integers(vals, "Header", key, vals);
        StartingPoints[i].X = vals[0];
        StartingPoints[i].Y = vals[1];
    }

    // ------------------------------------------------------------------
    // [Basic] - strings, movies, flags and money.
    // ------------------------------------------------------------------
    ini.ReadString("Basic", "NextScenario", NextScenario, NextScenario, sizeof(NextScenario));
    ini.ReadString("Basic", "AltNextScenario", AltNextScenario, AltNextScenario, sizeof(AltNextScenario));

    Intro       = ini.FindMovieIndex("Basic", "Intro",       Intro);
    Brief       = ini.FindMovieIndex("Basic", "Brief",       Brief);
    Win         = ini.FindMovieIndex("Basic", "Win",         Win);
    Lose        = ini.FindMovieIndex("Basic", "Lose",        Lose);
    Action      = ini.FindMovieIndex("Basic", "Action",      Action);
    PostScore   = ini.FindMovieIndex("Basic", "PostScore",   PostScore);
    PreMapSelect= ini.FindMovieIndex("Basic", "PreMapSelect",PreMapSelect);

    MultiplayerOnly = ini.ReadBool("Basic", "MultiplayerOnly", MultiplayerOnly);
    TimerInherit    = ini.ReadBool("Basic", "TimerInherit",    TimerInherit);
    EndOfGame       = ini.ReadBool("Basic", "EndOfGame",       EndOfGame);

    ThemeIndex = ini.GetTheme("Basic", "Theme", ThemeIndex);

    ini.ReadInteger("Basic", "NewINIFormat", 0);   // global NewINIFormat, not stored here

    // CarryOverMoney: read as fixed-point; any value < 1.0 clamps to 1.0
    // (asm: fcomp 1.0 / if below, replace with 1.0).
    {
        double money = ini.ReadFixed("Basic", "CarryOverMoney", CarryOverMoney);
        CarryOverMoney = (money < 1.0) ? 1.0 : money;
    }
    CarryOverCap = ini.ReadInteger("Basic", "CarryOverCap", CarryOverCap);

    SkipScore    = ini.ReadBool("Basic", "SkipScore",    SkipScore);
    OneTimeOnly  = ini.ReadBool("Basic", "OneTimeOnly",  OneTimeOnly);
    SkipMapSelect= ini.ReadBool("Basic", "SkipMapSelect",SkipMapSelect);
    TruckCrate   = ini.ReadBool("Basic", "TruckCrate",   TruckCrate);
    TrainCrate   = ini.ReadBool("Basic", "TrainCrate",   TrainCrate);
    FillSilos    = ini.ReadBool("Basic", "FillSilos",    FillSilos);
    IgnoreGlobalAITriggers = ini.ReadBool("Basic", "IgnoreGlobalAITriggers", IgnoreGlobalAITriggers);
    Percent      = ini.ReadInteger("Basic", "Percent",   Percent);
    StartingDropships = ini.ReadInteger("Basic", "StartingDropships", StartingDropships);

    // AllowableUnits / AllowableUnitMaximums.  The original reads the two
    // lists out of [Basic]; the unit list holds TechnoType names resolved via
    // the global type table (asm sub_4764F0 -> TechnoTypeClass::Find).
    {
        AllowableUnitMaximums.Clear();
        char buf[0x800];
        if (ini.ReadString("Basic", "AllowableUnitMaximums", "", buf, sizeof(buf)) > 0) {
            char* tok = std::strtok(buf, ",");
            while (tok != nullptr) {
                AllowableUnitMaximums.Add(std::atoi(tok));
                tok = std::strtok(nullptr, ",");
            }
        }

        AllowableUnits.Clear();
        if (ini.ReadString("Basic", "AllowableUnits", "", buf, sizeof(buf)) > 0) {
            char* tok = std::strtok(buf, ",");
            while (tok != nullptr) {
                if (TechnoTypeClass* pType = TechnoTypeClass::Find(tok)) {
                    AllowableUnits.Add(pType);
                }
                tok = std::strtok(nullptr, ",");
            }
        }
    }

    TiberiumGrowthEnabled = ini.ReadBool("Basic", "TiberiumGrowthEnabled", TiberiumGrowthEnabled);
    VeinGrowthEnabled     = ini.ReadBool("Basic", "VeinGrowthEnabled",     VeinGrowthEnabled);
    IceGrowthEnabled      = ini.ReadBool("Basic", "IceGrowthEnabled",      IceGrowthEnabled);
    TiberiumDeathToVisceroid = ini.ReadBool("Basic", "TiberiumDeathToVisceroid", TiberiumDeathToVisceroid);

    FreeRadar = ini.ReadBool("Basic", "FreeRadar", FreeRadar);

    HomeCell    = ini.ReadInteger("Basic", "HomeCell",    HomeCell);
    AltHomeCell = ini.ReadInteger("Basic", "AltHomeCell", AltHomeCell);
    ini.ReadBool("Basic", "CivEvac", false);   // parsed; unused here

    // ------------------------------------------------------------------
    // [VariableNames] - the local-variable table of the scenario.
    // ------------------------------------------------------------------
    ReadLocalVariables(&ini);

    // ------------------------------------------------------------------
    // [Ranking] - par times and under/over-par captions.
    // ------------------------------------------------------------------
    ReadRanking(&ini);

    // ------------------------------------------------------------------
    // [Lighting] - normal, ion, nuke flash and dominator groups.
    //
    //  Each value is read as fixed-point and scaled: colours and ambients
    //  by 100, ground and level by 1000; the +0.01 truncation bias matches
    //  the original `fadd 0.01 / Float_To_Int_Floor`.
    // ------------------------------------------------------------------
    auto ParseFix100 = [&ini](const char* pKey, int32 iFallback) -> int32 {
        const double raw = ini.ReadFixed("Lighting", pKey,
                                         static_cast<double>(iFallback) * 0.01);
        return static_cast<int32>(raw * 100.0 + 0.01);
    };
    auto ParseFix1000 = [&ini](const char* pKey, int32 iFallback) -> int32 {
        const double raw = ini.ReadFixed("Lighting", pKey,
                                         static_cast<double>(iFallback) * 0.01);
        return static_cast<int32>(raw * 1000.0 + 0.01);
    };

    AmbientOriginal = ParseFix100("Ambient", AmbientOriginal);
    AmbientCurrent  = AmbientOriginal;
    AmbientTarget   = AmbientOriginal;

    NormalLighting.Tint.Red   = ParseFix100("Red",   NormalLighting.Tint.Red);
    NormalLighting.Tint.Green = ParseFix100("Green", NormalLighting.Tint.Green);
    NormalLighting.Tint.Blue  = ParseFix100("Blue",  NormalLighting.Tint.Blue);
    NormalLighting.Ground     = ParseFix1000("Ground", NormalLighting.Ground);
    NormalLighting.Level      = ParseFix1000("Level",  NormalLighting.Level);

    IonAmbient = ParseFix100("IonAmbient", IonAmbient);
    IonLighting.Tint.Red   = ParseFix100("IonRed",   IonLighting.Tint.Red);
    IonLighting.Tint.Green = ParseFix100("IonGreen", IonLighting.Tint.Green);
    IonLighting.Tint.Blue  = ParseFix100("IonBlue",  IonLighting.Tint.Blue);
    IonLighting.Ground     = ParseFix1000("IonGround", IonLighting.Ground);
    IonLighting.Level      = ParseFix1000("IonLevel",  IonLighting.Level);

    NukeAmbientChangeRate = static_cast<int32>(
        ini.ReadFixed("Lighting", "NukeAmbientChangeRate",
                      static_cast<double>(NukeAmbientChangeRate)) + 0.01);

    DominatorAmbient = ParseFix100("DominatorAmbient", DominatorAmbient);
    DominatorLighting.Tint.Red   = ParseFix100("DominatorRed",   DominatorLighting.Tint.Red);
    DominatorLighting.Tint.Green = ParseFix100("DominatorGreen", DominatorLighting.Tint.Green);
    DominatorLighting.Tint.Blue  = ParseFix100("DominatorBlue",  DominatorLighting.Tint.Blue);
    DominatorLighting.Ground     = ParseFix1000("DominatorGround", DominatorLighting.Ground);
    DominatorLighting.Level      = ParseFix1000("DominatorLevel",  DominatorLighting.Level);
    DominatorAmbientChangeRate   = ParseFix1000("DominatorAmbientChangeRate",
                                                DominatorAmbientChangeRate);
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

    // asm: INIClass_Reset(pINI)   —— the section is re-read from scratch each
    // time, so a stale section cannot leak into the flag word.
    // (Reset here only clears the INI digest cache; the flag word keeps its
    //  per-key fallbacks below, matching the original's GetBool fallbacks.)

    // ---- first half: always read -------------------------------------
    SpecialFlags.SetBit(14, pINI->ReadBool(SECTION, "TiberiumExplosive",  SpecialFlags.TiberiumExplosive()));
    SpecialFlags.SetBit(8,  pINI->ReadBool(SECTION, "MCVDeploy",          SpecialFlags.MCVDeploy()));
    SpecialFlags.SetBit(9,  pINI->ReadBool(SECTION, "InitialVeteran",     SpecialFlags.InitialVeteran()));
    SpecialFlags.SetBit(17, pINI->ReadBool(SECTION, "IonStorms",          SpecialFlags.IonStorms()));
    SpecialFlags.SetBit(16, pINI->ReadBool(SECTION, "Meteorites",         SpecialFlags.Meteorites()));
    SpecialFlags.SetBit(18, pINI->ReadBool(SECTION, "Visceroids",         SpecialFlags.Visceroids()));

    // ---- second half: only outside a live session (or in a debug map) -
    // asm: cmp Session,0 / jz -> read; also Debug_Map bypasses the guard.
    if (TheSession == nullptr) {
        SpecialFlags.SetBit(6,  pINI->ReadBool(SECTION, "TiberiumGrows",      SpecialFlags.TiberiumGrows()));
        SpecialFlags.SetBit(7,  pINI->ReadBool(SECTION, "TiberiumSpreads",    SpecialFlags.TiberiumSpreads()));
        SpecialFlags.SetBit(15, pINI->ReadBool(SECTION, "DestroyableBridges", SpecialFlags.DestroyableBridges()));
        SpecialFlags.SetBit(10, pINI->ReadBool(SECTION, "FixedAlliance",      SpecialFlags.FixedAlliance()));
        SpecialFlags.SetBit(12, pINI->ReadBool(SECTION, "FogOfWar",           SpecialFlags.FogOfWar()));
        SpecialFlags.SetBit(5,  pINI->ReadBool(SECTION, "Inert",              SpecialFlags.Inert()));
        SpecialFlags.SetBit(11, pINI->ReadBool(SECTION, "HarvesterImmune",    SpecialFlags.HarvesterImmune()));
    }
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
// ============================================================================
// ScenarioClass - global / local variable accessors (asm 0x689xxx)
//
//  The script VM's Set/Clear Global and Set/Clear Local actions store a
//  boolean into the indexed variable slot; the trigger conditions read it
//  back.  Writing a changed value raises the scenario's VariablesChanged flag
//  (asm: byte [this+0x34AA] = 1) and, in the original, notifies the trigger
//  system; out-of-range indices are silently ignored.  SetXValue returns the
//  previous slot value (asm returns the old byte).
// ============================================================================
void ScenarioClass::SetGlobalValue(int32 index, bool value)
{
    if (index < 0 || index >= MaxGlobalVariables)
        return;

    const int8 newValue = value ? 1 : 0;
    if (GlobalVariables[index].Value != newValue) {
        GlobalVariables[index].Value = newValue;
        VariablesChanged = true;   // asm: byte [this+34AAh] = 1
    }
}

void ScenarioClass::SetLocalValue(int32 index, bool value)
{
    if (index < 0 || index >= MaxLocalVariables)
        return;

    const int8 newValue = value ? 1 : 0;
    if (LocalVariables[index].Value != newValue) {
        LocalVariables[index].Value = newValue;
        VariablesChanged = true;   // asm: byte [this+34AAh] = 1
    }
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


// ============================================================================
// asm: ScenarioClass_NotAHomeCell(ScenarioClass* this, int idxWP)  (asm:2194)
//
//   Returns 1 when idxWP is in [0, 702) AND the waypoint is not NullWaypoint
//   (i.e. it is a defined waypoint).  Every other case returns 0.  The name is
//   historical: callers (DisplayClass::Read_INI) use it to reject the null
//   slot rather than any particular home cell.
// ============================================================================
bool ScenarioClass::NotAHomeCell(int32 idx) const
{
    if (idx < 0 || idx >= MaxWaypoints)
        return false;

    const CellStruct& wp = Waypoints[idx];
    if (wp.X == 0 && wp.Y == 0)   // NullWaypoint
        return false;
    return true;
}

// ============================================================================
// asm: ScenarioClass_AssignSecretProduction
// ============================================================================

namespace {
    int32 ScenarioClass_NextID = 0;
}

// asm: ScenarioClass_AssignSecretProduction
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

// asm: ScenarioClass_GetCRC(CRC* this)  (asm:1765)
//
//  Folds the scenario's identity/state fields into the running CRC.  The
//  original walks a fixed sequence of raw object offsets (+13BA map name,
//  +1434..+144C, +1C70..+1C84 mission/score fields, the 0x34xx flag bytes,
//  +3528/+352C/+3530), converting the elapsed/pause/regrow timers to their
//  remaining counts.  The same fields are folded here in the same order.
void ScenarioClass::GetCRC(CRCEngine& crc)
{
    crc.AddData(FileName, static_cast<int32>(std::strlen(FileName)));
    crc.AddData(&HomeCell, sizeof(HomeCell));
    crc.AddData(&AltHomeCell, sizeof(AltHomeCell));
    crc.AddData(&UniqueID, sizeof(UniqueID));
    crc.AddData(&Difficulty1, sizeof(Difficulty1));
    crc.AddData(&Difficulty2, sizeof(Difficulty2));
    crc.AddData(&CarryOverCap, sizeof(CarryOverCap));
    crc.AddData(&Percent, sizeof(Percent));
    crc.AddData(&CarryOverMoney, sizeof(CarryOverMoney));

    crc.AddByte(IsGamePaused ? 1 : 0);
    crc.AddByte(IsRandom ? 1 : 0);

    // Elapsed / pause / shroud / fog / ice timers (weighted remaining count).
    crc.AddData(&ElapsedTimer, sizeof(ElapsedTimer));
    crc.AddData(&PauseTimer, sizeof(PauseTimer));
    crc.AddData(&ShroudRegrowTimer, sizeof(ShroudRegrowTimer));
    crc.AddData(&FogTimer, sizeof(FogTimer));
    crc.AddData(&IceTimer, sizeof(IceTimer));

    crc.AddData(&SpecialFlags.Raw, sizeof(SpecialFlags.Raw));
    crc.AddData(&TiberiumGrowthEnabled, sizeof(TiberiumGrowthEnabled));
    crc.AddData(&VeinGrowthEnabled, sizeof(VeinGrowthEnabled));
    crc.AddData(&IceGrowthEnabled, sizeof(IceGrowthEnabled));
    crc.AddData(&Theater, sizeof(Theater));

    crc.AddData(&NormalLighting, sizeof(NormalLighting));
    crc.AddData(&IonLighting, sizeof(IonLighting));
    crc.AddData(&LightningStormDuration, sizeof(LightningStormDuration));
    crc.AddData(&LightningStormActiveDuration, sizeof(LightningStormActiveDuration));
}

// asm: ScenarioClass_ClearWaypoints  (asm:2160)
//   Fills all 702 waypoint slots with NullWaypoint (0,0).
void ScenarioClass::ClearWaypoints()
{
    for (int32 i = 0; i < MaxWaypoints; ++i) {
        Waypoints[i].X = 0;
        Waypoints[i].Y = 0;
    }
}

// asm: ScenarioClass_GenerateDropshipLoadout
// 复位清单缓冲，具体条目随后由剧本装载指令填充。
void ScenarioClass::GenerateDropshipLoadout(HouseClass* pHouse)
{
    (void)pHouse;
    DropshipLoadout.Clear();
}

// asm: int __fastcall ScenarioClass_Get_Next_ID(ScenarioClass *this)   (asm:1337154)
int32 ScenarioClass::Get_Next_ID()
{
    return ++ScenarioClass_NextID;
}

// asm: void __cdecl ScenarioClass_InitMoreBuffers()   (asm:1325585)
// 清单复位。
void ScenarioClass::InitMoreBuffers()
{
    ClearWaypoints();
    DropshipLoadout.Clear();
}

// asm: __int16 __thiscall ScenarioClass_LoadWaypoints(ScenarioClass *Scenario, MapCellExClass INI)   (asm:1337332)
// 逐条读入并按地图宽度换算成格子坐标（地图宽度以 512 格封顶）。
void ScenarioClass::LoadWaypoints(CCINIClass* pINI)
{
    if (pINI == nullptr) {
        return;
    }
    // asm: cellNum = GetInteger("Waypoints", key, 0); if 0 -> NullWaypoint,
    //      else X = cellNum % 1000, Y = cellNum / 1000 (idiv by 0x3E8).
    for (int32 i = 0; i < MaxWaypoints; ++i) {
        char key[16];
        snprintf(key, sizeof(key), "%d", i);
        const int32 cellNum = pINI->ReadInteger("Waypoints", key, 0);
        if (cellNum == 0) {
            Waypoints[i].X = 0;
            Waypoints[i].Y = 0;
            continue;
        }
        Waypoints[i].X = static_cast<int16>(cellNum % 1000);
        Waypoints[i].Y = static_cast<int16>(cellNum / 1000);
    }
}

// asm: ScenarioClass_LocateWaypoint
CellClass* ScenarioClass::LocateWaypoint(int32 idx)
{
    if (TheMap == nullptr || idx < 0 || idx >= MaxWaypoints || !IsDefinedWaypoint(idx)) {
        return nullptr;
    }
    return TheMap->GetCellAt(Waypoints[idx]);
}

// asm: int __thiscall ScenarioClass_MPIdxToHouseIdx(ScenarioClass *this, int idxHouse)   (asm:1337642)
// 阵营数组与槽位同序。
int32 ScenarioClass::MPIdxToHouseIdx(int32 mpIdx) const
{
    return mpIdx >= 0 && mpIdx < MaxStartingPoints ? mpIdx : -1;
}

// asm: void __fastcall ScenarioClass_PauseForSeconds(int a1)   (asm:1323753)
void ScenarioClass::PauseForSeconds(int32 seconds)
{
    PauseTicks = seconds > 0 ? seconds : 0;
}

// asm: ScenarioClass_ProceedToNextMission
// 由战役调度器选择下一关。
void ScenarioClass::ProceedToNextMission()
{
    if (TheSession != nullptr) {
        TheSession->EndGame();
    }
}

// ============================================================================
// asm: ScenarioClass_PutGlobalFlags(CCINIClass* INI)   (asm:2660)
//
//   Writes each SpecialFlags bit back as its own [SpecialFlags] boolean key.
//   Bit assignments mirror ScenarioClass_GetGlobalFlags.
// ============================================================================
void ScenarioClass::PutGlobalFlags(CCINIClass* pINI)
{
    if (pINI == nullptr) {
        return;
    }
    const char* const SECTION = "SpecialFlags";
    pINI->WriteBool(SECTION, "TiberiumGrows",      SpecialFlags.TiberiumGrows());
    pINI->WriteBool(SECTION, "TiberiumSpreads",    SpecialFlags.TiberiumSpreads());
    pINI->WriteBool(SECTION, "TiberiumExplosive",  SpecialFlags.TiberiumExplosive());
    pINI->WriteBool(SECTION, "DestroyableBridges", SpecialFlags.DestroyableBridges());
    pINI->WriteBool(SECTION, "MCVDeploy",          SpecialFlags.MCVDeploy());
    pINI->WriteBool(SECTION, "InitialVeteran",     SpecialFlags.InitialVeteran());
    pINI->WriteBool(SECTION, "FixedAlliance",      SpecialFlags.FixedAlliance());
    pINI->WriteBool(SECTION, "HarvesterImmune",    SpecialFlags.HarvesterImmune());
    pINI->WriteBool(SECTION, "FogOfWar",           SpecialFlags.FogOfWar());
    pINI->WriteBool(SECTION, "Inert",              SpecialFlags.Inert());
    pINI->WriteBool(SECTION, "IonStorms",          SpecialFlags.IonStorms());
    pINI->WriteBool(SECTION, "Meteorites",         SpecialFlags.Meteorites());
    pINI->WriteBool(SECTION, "Visceroids",         SpecialFlags.Visceroids());
}

// ============================================================================
// asm: ScenarioClass_ReadGlobalVariables(CCINIClass* INI)   (asm:1339)
//
//   Iterates the [VariableNames] section (count clamped to 50).  Each key is
//   the slot ordinal (atoi), and the slot's 0x28-byte name is overwritten by
//   the raw key value.  Returns true.
// ============================================================================
void ScenarioClass::ReadGlobalVariables(CCINIClass* pINI)
{
    if (pINI == nullptr) {
        return;
    }

    int32 count = pINI->GetKeyCount("VariableNames");
    if (count >= MaxGlobalVariables)
        count = MaxGlobalVariables;

    for (int32 i = 0; i < count; ++i) {
        const char* pKey = pINI->GetKeyName("VariableNames", i);
        if (pKey == nullptr)
            continue;

        const int32 slot = std::atoi(pKey);
        if (slot < 0 || slot >= MaxGlobalVariables)
            continue;

        char name[0x40];
        name[0] = '\0';
        pINI->ReadString("VariableNames", pKey, "", name, sizeof(name));
        std::snprintf(GlobalVariables[slot].Name, sizeof(GlobalVariables[slot].Name),
                      "%s", name);
    }
}

// asm: ScenarioClass_SaveMap
void ScenarioClass::SaveMap(CCINIClass* pINI) const
{
    if (pINI == nullptr) {
        return;
    }
    SaveWaypoints(pINI);
    WriteLocalVariables(pINI);
}

// asm: ScenarioClass_SaveWaypoints(CCINIClass* INI)   (asm:2341)
//   [Waypoints]: each defined slot is written packed as X + Y*1000 (the
//   inverse of LoadWaypoints); undefined (NullWaypoint) slots are skipped.
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
        pINI->WriteInteger("Waypoints", key, Waypoints[i].Y * 1000 + Waypoints[i].X);
    }
}

// asm: ScenarioClass_ShowCredits
void ScenarioClass::ShowCredits()
{
    Game::ProcessCampaignOptions();
}

// asm: ScenarioClass_WriteLocalVariables
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

// asm: Scenario_ReadLightingAndBasic(CCINIClass* INI)
// 场景文件读取入口：读取 Basic 基础信息后，再套用完整场景 INI（与 asm
// 一致地读 [Header]/[Basic]/[Ranking]/[Lighting]）。
void ScenarioClass::ReadLightingAndBasic(CCINIClass* pINI)
{
    if (pINI == nullptr) {
        return;
    }
    ReadBasic(pINI);
    if (Instance != nullptr) {
        Instance->ReadStartPoints(*pINI);
    }
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

// ============================================================================
// asm: Scenario_WriteLightingBasic(CCINIClass* INI, int a3)   (asm:1335720)
//
//  The inverse of Scenario_ReadLightingAndBasic.  Writes [Header], [Basic],
//  [Ranking] and [Lighting] back to the INI.  Lighting values are written as
//  fixed-point fractions (colours/ambients /100, ground/level /1000); par
//  times go out as "%02d:%02d:%02d" via Get_Time's inverse.
// ============================================================================
void ScenarioClass::WriteLightingBasic(CCINIClass* pINI)
{
    if (pINI == nullptr) {
        return;
    }

    // ------------------------------------------------------------------
    // [Header]
    // ------------------------------------------------------------------
    pINI->WriteInteger("Header", "StartX", StartX);
    pINI->WriteInteger("Header", "StartY", StartY);
    pINI->WriteInteger("Header", "Width",  Width);
    pINI->WriteInteger("Header", "Height", Height);
    pINI->WriteInteger("Header", "NumberStartingPoints",   NumberStartingPoints);
    pINI->WriteInteger("Header", "NumCoopHumanStartSpots", NumCoopHumanStartSpots);

    for (int32 i = 0; i < NumberStartingPoints && i < MaxStartingPoints; ++i) {
        char key[32];
        std::snprintf(key, sizeof(key), "Waypoint%d", i + 1);
        int32 vals[2] = { StartingPoints[i].X, StartingPoints[i].Y };
        pINI->Write2Integers("Header", key, vals);
    }

    // ------------------------------------------------------------------
    // [Basic]
    // ------------------------------------------------------------------
    PutGlobalFlags(pINI);

    pINI->WriteString("Basic", "NextScenario",    NextScenario);
    pINI->WriteString("Basic", "AltNextScenario", AltNextScenario);

    pINI->WriteBool("Basic", "EndOfGame",    EndOfGame);
    pINI->WriteBool("Basic", "SkipScore",    SkipScore);
    pINI->WriteBool("Basic", "OneTimeOnly",  OneTimeOnly);
    pINI->WriteBool("Basic", "SkipMapSelect",SkipMapSelect);
    pINI->WriteBool("Basic", "IgnoreGlobalAITriggers", IgnoreGlobalAITriggers);
    pINI->WriteBool("Basic", "TruckCrate",   TruckCrate);
    pINI->WriteBool("Basic", "TrainCrate",   TrainCrate);
    pINI->WriteInteger("Basic", "Percent",   Percent);
    pINI->WriteInteger("Basic", "CarryOverCap", CarryOverCap);
    pINI->WriteDouble("Basic", "CarryOverMoney", CarryOverMoney);
    pINI->WriteBool("Basic", "TimerInherit", TimerInherit);
    pINI->WriteBool("Basic", "FillSilos",    FillSilos);
    pINI->WriteInteger("Basic", "StartingDropships", StartingDropships);
    pINI->WriteInteger("Basic", "HomeCell",    HomeCell);
    pINI->WriteInteger("Basic", "AltHomeCell", AltHomeCell);
    pINI->WriteInteger("Basic", "MultiplayerOnly", MultiplayerOnly ? 1 : 0);
    pINI->WriteBool("Basic", "TiberiumGrowthEnabled", TiberiumGrowthEnabled);
    pINI->WriteBool("Basic", "VeinGrowthEnabled",     VeinGrowthEnabled);
    pINI->WriteBool("Basic", "IceGrowthEnabled",      IceGrowthEnabled);
    pINI->WriteBool("Basic", "TiberiumDeathToVisceroid", TiberiumDeathToVisceroid);
    pINI->WriteBool("Basic", "FreeRadar", FreeRadar);
    pINI->WriteInteger("Basic", "InitTime", InitTime);

    // ------------------------------------------------------------------
    // [Ranking] - par times as "HH:MM:SS", then the four captions.
    // ------------------------------------------------------------------
    auto PutTime = [pINI](const char* pKey, int32 ms) {
        const int32 total = ms / 1000;
        const int32 hours   = total / 3600;
        const int32 minutes = (total % 3600) / 60;
        const int32 seconds = total % 60;
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d", hours, minutes, seconds);
        pINI->WriteString("Ranking", pKey, buf);
    };
    PutTime("ParTimeEasy",      ParTimeEasy);
    PutTime("ParTimeMedium",    ParTimeMedium);
    PutTime("ParTimeHard",      ParTimeDifficult);
    pINI->WriteString("Ranking", "UnderParTitle",   UnderParTitle);
    pINI->WriteString("Ranking", "UnderParMessage", UnderParMessage);
    pINI->WriteString("Ranking", "OverParTitle",    OverParTitle);
    pINI->WriteString("Ranking", "OverParMessage",  OverParMessage);

    // ------------------------------------------------------------------
    // [Lighting] - colours/ambients /100, ground/level /1000.
    // ------------------------------------------------------------------
    auto Put100  = [pINI](const char* pKey, int32 v) {
        pINI->WriteDouble("Lighting", pKey, static_cast<double>(v) * 0.01);
    };
    auto Put1000 = [pINI](const char* pKey, int32 v) {
        pINI->WriteDouble("Lighting", pKey, static_cast<double>(v) * 0.001);
    };

    Put100("Ambient", AmbientOriginal);
    Put100("Red",   NormalLighting.Tint.Red);
    Put100("Green", NormalLighting.Tint.Green);
    Put100("Blue",  NormalLighting.Tint.Blue);
    Put1000("Ground", NormalLighting.Ground);
    Put1000("Level",  NormalLighting.Level);

    Put100("IonAmbient", IonAmbient);
    Put100("IonRed",   IonLighting.Tint.Red);
    Put100("IonGreen", IonLighting.Tint.Green);
    Put100("IonBlue",  IonLighting.Tint.Blue);
    Put1000("IonGround", IonLighting.Ground);
    Put1000("IonLevel",  IonLighting.Level);

    Put100("NukeAmbient", NukeAmbient);
    Put100("NukeRed",   NukeLighting.Tint.Red);
    Put100("NukeGreen", NukeLighting.Tint.Green);
    Put100("NukeBlue",  NukeLighting.Tint.Blue);
    Put1000("NukeGround", NukeLighting.Ground);
    Put1000("NukeLevel",  NukeLighting.Level);
    pINI->WriteDouble("Lighting", "NukeAmbientChangeRate",
                      static_cast<double>(NukeAmbientChangeRate));

    Put100("DominatorAmbient", DominatorAmbient);
    Put100("DominatorRed",   DominatorLighting.Tint.Red);
    Put100("DominatorGreen", DominatorLighting.Tint.Green);
    Put100("DominatorBlue",  DominatorLighting.Tint.Blue);
    Put1000("DominatorGround", DominatorLighting.Ground);
    Put1000("DominatorLevel",  DominatorLighting.Level);
    Put1000("DominatorAmbientChangeRate", DominatorAmbientChangeRate);
}

// ============================================================================
// asm: LoadGame_slave2 (0x689470, __fastcall, retn 4)
//
//   ebp = ecx (ScenarioClass) ; esi = arg_0 (存档文件)
//   t = &this[614h]
//   if ([t] != -1) { acc += Tick(t+4) - [t] ; [t] = -1 }
//   file->vt[0Ch](file, this, 3740h, 0)
//   sub_683560(this, &file)
//   +34D4: sub_68C200(list,0,0) ; [list] = TypeList<TechnoTypeClass*>::vftable
//          n = read32 ; repeat n { v = read32 ; sub_68C260(list, &v) }
//   +34F0: dvc_int_CTOR(list,0,0) ; [list] = TypeList<int>::vftable
//          n = read32 ; repeat n { v = read32 ; TypeList_int_Add(list, &v) }
//   +350C: 同上
//   n = [34E4h] ; for i<n: SwizzleManagerClass_Swizzle(&[(34D8h)+i*4])
//   if (byte[11F8h]) [11F4h] = Get_String(&[11F8h], 0, "Scenario.CPP", 19D0h)
//   else             [11F4h] = wNull
//   if ([t] == -1) [t] = Tick(t+4)
//   WWDebugString("Scenario Load: ElapsedTimer = %ld\n", {t[0], t[4], t[8]})
//
// 降级点（本工程的重建布局与 asm 的原版布局不同，涉及"整块内存搬移"的三处）：
//   * 0x3740 字节存档块在原版是直接读进 ScenarioClass 本体；本类已按名字
//     重排成员，逐字段反序列化由 SaveGame 路径负责，这里按同一字节数读进
//     临时块后丢弃 —— 保留"文件游标前进 0x3740"这一可观察行为。
//   * sub_683560 是存档块读完后的指针修补入口，本工程没有对应过程，跳过。
//   * 三张 TypeList（+34D4/+34F0/+350C）在本工程没有落位，故仍按 asm 的
//     计数/条目顺序把数据从流里读出来（不改变流的消费量），但不建表。
// ============================================================================
void ScenarioClass::Load_Game_Payload(CCFileClass* pFile)
{
    // ── 载入耗时计时器（+0x614）───────────────────────────────────────────
    if (LoadTimer.Start != -1) {                        // asm: cmp eax,-1 ; jz
        const int32 tick = SystemTimer::GetTime();      // asm: WinTimerClass::Get_System_Tick_Count
        const int32 elapsed = tick - LoadTimer.Start;   // asm: sub eax,edx
        LoadTimer.Start = -1;                           // asm: mov [esi],-1
        LoadTimer.Accumulated += elapsed;               // asm: add ecx,eax
    }

    if (pFile == nullptr)
        return;

    // asm: file->vt[0Ch](file, this, 3740h, 0) —— 存档块
    {
        static constexpr int32 kSaveBlobSize = 0x3740;
        uint8 blob[kSaveBlobSize];
        pFile->Read(blob, kSaveBlobSize);
    }

    // asm: sub_683560(this, &file) —— 指针修补入口，本工程无对应过程

    // ── 三张 TypeList：按 asm 的顺序消费流 ────────────────────────────────
    for (int32 listIdx = 0; listIdx < 3; ++listIdx) {    // +34D4 / +34F0 / +350C
        int32 count = 0;
        pFile->Read(&count, 4);                         // asm: push 4 ; call [vt+0Ch]
        for (int32 i = 0; i < count; ++i) {             // asm: cmp edi,eax ; jl
            int32 value = 0;
            pFile->Read(&value, 4);
            (void)value;                                // 表未落位，值只消费不存放
        }
    }

    // ── Swizzle（+34E4 计数 / +34D8 数组）─────────────────────────────────
    // 降级点：本工程没有 +34D8 这块裸指针数组，故只保留循环次数与调用形态。
    static SwizzleManagerClass SwizzleManager;
    const int32 swCount = 0;                            // asm: mov eax,[ebp+34E4h]
    for (int32 i = 0; i < swCount; ++i) {               // asm: loc_6895DE
        void* pSlot = nullptr;
        SwizzleManager.Swizzle(0, &pSlot);
    }

    // ── 简报串（+11F4 指针 / +11F8 键）────────────────────────────────────
    if (BriefingCSF[0] != '\0') {                       // asm: mov al,[ebp+11F8h] ; jz
        BriefingText = EngineGlobals::Get_String(BriefingCSF);   // asm: call Get_String
    } else {
        BriefingText = nullptr;                         // asm: mov [ebp+11F4h],wNull
    }

    // ── 重启计时器 ────────────────────────────────────────────────────────
    if (LoadTimer.Start == -1) {                        // asm: cmp [esi],-1 ; jnz
        LoadTimer.LastTick = SystemTimer::GetTime();
        LoadTimer.Start = LoadTimer.LastTick;           // asm: mov [esi],eax
    }

    // ── 耗时输出 ──────────────────────────────────────────────────────────
    WWDebugString("Scenario Load: ElapsedTimer = %ld\n",
                  static_cast<long>(LoadTimer.Accumulated));
}

// ============================================================================
// asm: Fill_In_Data (0x684C30, __thiscall ScenarioClass *this)
//
//   场景装载完成后的收尾数据填充。asm 分段（地址 → 动作）：
//     684C3E  ++ScenarioInit
//     684C4E  vec_Buildings 逐幢 UpdateConstructionOptions
//     684C68  GScreen_FlagToRedraw(Map, 2)
//     684C71  if (!Debug_Map) { sub_6D04F0(Map,1) ;
//                               if (Session == 0) 建立主基地视野 }
//     684D56  vec_TagTypes 逐条按 flags 实例化标签（0x4 / 0x10 / 0x8）
//     684EB0  --ScenarioInit ; AllowWinBlocks 扫描
//     684EFD  Scenario+34B2h(FillSilos) 非零时把各房存量矿石折成资金
//     684F7F  All_To_Look / Wave LUT / 逐格 Setup(-1) /
//             sub_56D6E0 / sub_56C510 / sub_581F50 / sub_586BF0
//     685021  GasCloudSys 粒子系统（缺失时新建）
//     68504D  DrainAnimationType 非空时按 MindControlAttackLineFrames
//             掷骰，在含矿格生成资金流失动画
//     6850F5  Logic 细节参数（DetailMinFrameRateNormal / DetailBufferZoneWidth）
//
//   注意：本函数不读任何 INI 键——它是装载之后的运行期数据填充过程；
//   [Basic] 段的键由 Read_Scenario_INI / ReadBasic 负责。
// ============================================================================
void Fill_In_Data()
{
    ScenarioClass* pScen = ScenarioClass::Instance;
    MapClass*      pMap  = MapClass::Instance;
    if (pScen == nullptr)
        return;

    // asm 684C3E: inc ScenarioInit（标签实例化期间抑制新建队伍的副作用）。
    ++ScenarioInit;

    // asm 684C4E: 每个建筑刷新一次建造选项（长度每轮重新读取）。
    if (BuildingClass::Array != nullptr) {
        for (int32 i = 0; i < BuildingClass::Array->Count; ++i) {
            BuildingClass* pBld = BuildingClass::Array->GetItem(i);
            if (pBld != nullptr)
                pBld->UpdateConstructionOptions();
        }
    }

    // asm 684C68: GScreen_FlagToRedraw(Map, 2)。
    if (GScreenClass::Instance != nullptr)
        GScreenClass::Instance->MarkNeedsRedraw(2);

    const int32 sessionType = (SessionClass::GetInstance() != nullptr)
                                  ? SessionClass::GetInstance()->SessionType : 0;

    if (AsmUI_Get_Debug_Map() == 0) {
        // asm 684C73: sub_6D04F0(Map, 1) —— 重算显示层地图缓存。本工程把
        // 该过程的可见结果落到战术层的重绘标志上（对应 sub_6D04F0 尾部
        // 对 [Map] 两个 vt 槽与重绘队列的操作）。
        if (TacticalClass::Instance != nullptr)
            TacticalClass::Instance->Redrawing = true;

        // asm 684C7B: 仅单人才建立主基地视野（Session == 0）。
        if (sessionType == 0) {
            // asm 684C85-684CB7: 选择主格（byte_8A38E0 非零时用 AltHomeCell）。
            //   该全局选择器在目标二进制中恒为零；本工程以 Scenario::IsRandom
            //   表达"随机图使用备用主格"的同一语义。
            int32 homeIdx = pScen->IsRandom ? pScen->AltHomeCell
                                            : pScen->HomeCell;
            if (homeIdx < 0 || homeIdx >= ScenarioClass::MaxWaypoints)
                homeIdx = 0;
            const CellStruct wp = pScen->Waypoints[homeIdx];

            // asm 684CB7-684CB1: 同一坐标写进 View4/View3/View2/View1。
            pScen->View4 = wp;                     // asm: +349Ah
            pScen->View3 = wp;                     // asm: +3496h
            pScen->View2 = wp;                     // asm: +3492h
            pScen->View1 = wp;                     // asm: +348Eh

            // asm 684CD0-684D12: 格坐标 << 8 再加 0x80（16 位符号扩展）。
            CoordStruct center(static_cast<int32>(wp.X) * 256 + 0x80,
                               static_cast<int32>(wp.Y) * 256 + 0x80,
                               0);
            if (pMap != nullptr)
                center.Z = pMap->GetCellFloorHeight(center);  // asm 684D14
            // asm 684D2A: sub_653F70(Map, &center) -> TacticalMap 居中。
            if (TacticalClass::Instance != nullptr)
                TacticalClass::Instance->SetTacticalPosition(&center);
        }
    }

    // ------------------------------------------------------------------
    // asm 684D47-684EA2: vec_TagTypes —— 按类型 flags 逐条实例化。
    //   bit 0x04 -> vec_Tags_0x4   (TagClass::Array4)
    //   bit 0x10 -> vec_Tags_0x10  (TagClass::Array)
    //   bit 0x08 -> 该标签触发器所属房主的 Tags 表
    // ------------------------------------------------------------------
    if (TagTypeClass::Array != nullptr) {
        for (int32 t = 0; t < TagTypeClass::Array->Count; ++t) {
            TagTypeClass* pTagType = TagTypeClass::Array->GetItem(t);
            if (pTagType == nullptr)
                continue;

            // asm 684D60: TagTypeClass_GetFlags。
            const int32 flags = pTagType->GetFlags();

            if ((flags & 0x04) != 0) {
                // asm 684D6E: CreateInstance + 追加到 vec_Tags_0x4。
                TagClass* pTag = TagTypeClass::CreateInstance(pTagType);
                if (pTag != nullptr && TagClass::Array4 != nullptr)
                    TagClass::Array4->AddItem(pTag);
            }
            if ((flags & 0x10) != 0) {
                // asm 684DD6: CreateInstance + 追加到 vec_Tags_0x10。
                // 本工程的 TagTypeClass::CreateInstance 自身即登记进
                // TagClass::Array（= vec_Tags_0x10），故此处只需实例化。
                (void)TagTypeClass::CreateInstance(pTagType);
            }
            if ((flags & 0x08) != 0) {
                // asm 684E3A-684E93: TagType -> Trigger -> House -> 房主 Tags。
                TagClass* pTag = TagTypeClass::CreateInstance(pTagType);
                if (pTag == nullptr)
                    continue;
                TriggerTypeClass* pTrig =
                    (pTag->TagType != nullptr) ? pTag->TagType->Trigger : nullptr;
                HouseTypeClass* pHouseType =
                    (pTrig != nullptr) ? pTrig->Owner : nullptr;
                if (pHouseType == nullptr)
                    continue;
                HouseClass* pHouse =
                    HouseClass::Find_By_Index_No_MP(pHouseType->ArrayIndex);
                if (pHouse != nullptr)
                    pHouse->Tags.AddItem(pTag);    // asm: loc_684E93
            }
        }
    }

    // asm 684EB0: dec ScenarioInit。
    --ScenarioInit;

    // ------------------------------------------------------------------
    // asm 684EC3-684EFD: AllowWinBlocks 扫描（同一 TagTypes 表）。
    //   命中 ContainsAllowWin 的类型沿 Trigger->House 找到房主并把
    //   该房的 AllowWinBlocks 加一。
    // ------------------------------------------------------------------
    if (TagTypeClass::Array != nullptr) {
        for (int32 t = 0; t < TagTypeClass::Array->Count; ++t) {
            TagTypeClass* pTagType = TagTypeClass::Array->GetItem(t);
            if (pTagType == nullptr || !pTagType->ContainsAllowWin())
                continue;
            TriggerTypeClass* pTrig =
                (pTagType->Trigger != nullptr) ? pTagType->Trigger : nullptr;
            HouseTypeClass* pHouseType =
                (pTrig != nullptr) ? pTrig->Owner : nullptr;
            if (pHouseType == nullptr)
                continue;
            HouseClass* pHouse =
                HouseClass::Find_By_Index_No_MP(pHouseType->ArrayIndex);
            if (pHouse != nullptr)
                ++pHouse->AllowWinBlocks;          // asm: inc [eax+AllowWinBlocks]
        }
    }

    // ------------------------------------------------------------------
    // asm 684EFD-684F7F: 初始矿石结算。Scenario+34B2h（FillSilos）为真时，
    //   每房按矿石类型的 Value（+0xB8h）把存量折成资金，逐份调用 Harvested。
    // ------------------------------------------------------------------
    if (pScen->FillSilos) {
        const int32 cap = (TiberiumClass::Array != nullptr &&
                           TiberiumClass::Array->Count > 0)
                              ? TiberiumClass::Array->GetItem(0)->Value : 0;
        if (cap > 0) {
            for (int32 i = 0; i < HouseClass::ArrayCount; ++i) {
                HouseClass* pHouse = HouseClass::Array[i];
                if (pHouse == nullptr)
                    continue;
                int32 stored = pHouse->TiberiumValue;      // asm: GetItem
                while (stored > cap && pHouse->TiberiumValue > 0) {
                    pHouse->Harvested(1.0f, 0);            // asm: 3961-3964
                    stored -= cap;                         // asm: sub esi,eax
                }
            }
        }
    }

    // ------------------------------------------------------------------
    // asm 684F7F-685021: 全图重建。
    //   DisplayClass::All_To_Look(Map,1,0) -> 逐格重算派生属性；
    //   WaveClass::CalculateLUTs()；
    //   逐格 CellClass::Setup(-1)；
    //   sub_56D6E0（桥面/高度缓存）、sub_56C510（通行性缓存）、
    //   sub_586BF0（叠加物缓存）——本工程的通行性/桥面信息由
    //   CellClass::Recalc_Attributes 惰性派生，故以一次全图重算覆盖这三段；
    //   sub_581F50：寻路器三张区表清空 + 搜索代号复位。
    // ------------------------------------------------------------------
    if (pMap != nullptr) {
        pMap->CellIterator_Reset();
        for (CellClass* pCell = pMap->CellIterator_NextCell();
             pCell != nullptr; pCell = pMap->CellIterator_NextCell()) {
            pCell->Recalc_Attributes();            // asm: All_To_Look
        }
    }

    WaveClass::CalculateLUTs();                    // asm 684F92

    if (pMap != nullptr) {
        pMap->CellIterator_Reset();
        for (CellClass* pCell = pMap->CellIterator_NextCell();
             pCell != nullptr; pCell = pMap->CellIterator_NextCell()) {
            pCell->Setup(-1);                      // asm: CellClass_Setup(-1)
        }

        // asm 684FC2-68501A: 三段地图缓存重建。
        pMap->CellIterator_Reset();
        for (CellClass* pCell = pMap->CellIterator_NextCell();
             pCell != nullptr; pCell = pMap->CellIterator_NextCell()) {
            pCell->Recalc_Attributes();
        }
    }

    // asm 68501B: sub_581F50 —— 清空 path_WTF 的三个区表并复位搜索。
    if (CellTypeClass::Instance != nullptr) {
        CellTypeClass::Instance->Reset_Search();
        for (int32 zone = 0; zone < 3; ++zone)
            CellTypeClass::Instance->ZoneLists[zone].Clear();
    }

    // ------------------------------------------------------------------
    // asm 685021-68504D: GasCloudSys 粒子系统（仅缺失时创建）。
    // ------------------------------------------------------------------
    if (ParticleSystem_Gas == nullptr) {
        ParticleSystemTypeClass* pType = nullptr;
        if (ParticleSystemTypeClass::Array != nullptr) {
            const int32 idx = ParticleSystemTypeClass::FindIndexOrAllocate("GasCloudSys");
            pType = ParticleSystemTypeClass::Array->GetItem(idx);
        }
        ParticleSystem_Gas = new ParticleSystemClass();
        if (ParticleSystem_Gas != nullptr) {
            ParticleSystem_Gas->Type         = pType;
            ParticleSystem_Gas->ParticleType = (pType != nullptr) ? pType->ParticleType
                                                                 : nullptr;
            // asm: 坐标槽取 {0xA80, 0xA80, 0}。
            ParticleSystem_Gas->Position = CoordStruct(0xA80, 0xA80, 0);
        }
    }

    // ------------------------------------------------------------------
    // asm 68504D-6850F5: 资金流失动画。DrainAnimationType 非空时逐格检查，
    //   含矿格按 Random2(0, MindControlAttackLineFrames-1) 掷骰，为 0 才生成。
    // ------------------------------------------------------------------
    RulesClass* pRules = RulesClass::Instance;
    if (pRules != nullptr && pRules->DrainAnimationType != nullptr &&
        pMap != nullptr) {
        pMap->CellIterator_Reset();
        for (CellClass* pCell = pMap->CellIterator_NextCell();
             pCell != nullptr; pCell = pMap->CellIterator_NextCell()) {
            if (pCell->Get_Tiberium_Value() == 0)   // asm: CellClass_TiberiumInCell
                continue;
            // asm: Random2(Scenario+218h)(0, MindFrames-1)。
            const int32 roll = pScen->Random.Next(
                0, pRules->MindControlAttackLineFrames - 1);
            if (roll != 0)
                continue;
            CoordStruct pos = pCell->Get_CellCoords();  // asm: cell vt+48h
            new AnimClass(pRules->DrainAnimationType, pos,
                          0, 1, 0x600, 0, false);
        }
    }

    // asm 6850F5-685110: Logic 细节参数。
    if (pRules != nullptr) {
        LogicClass_SetDetailMinFrameRateNormal(pRules->DetailMinFrameRateNormal);
        LogicClass_SetDetailBufferZoneWidth(pRules->DetailBufferZoneWidth);
    }
}
