#include "SuperWeaponTypeClass.h"
#include "../INI/INIClass.h"
#include "../Houses/HouseTypeClass.h"
#include "../Animations/AnimTypeClass.h"
#include "../Abstract/BuildingTypeClass.h"
#include "../Combat/WarheadTypeClass.h"
#include "../Combat/WeaponTypeClass.h"
#include "../Scenario/ScenarioClass.h"
#include "../Rules/RulesClass.h"
#include "../Game/Game.h"

#include <cstring>
#include <cstdlib>

// ============================================================================
// Static members
// ============================================================================

DynamicVectorClass<SuperWeaponTypeClass*>* SuperWeaponTypeClass::Array = nullptr;

SuperWeaponTypeClass* SuperWeaponTypeClass::Last = nullptr;
int32 SuperWeaponTypeClass::Count = 0;

// ============================================================================
// Static lookup
// ============================================================================

SuperWeaponTypeClass* SuperWeaponTypeClass::Find(const char* pID) {
    if (!Array || !pID) return nullptr;
    for (int32 i = 0; i < Array->Count; ++i) {
        SuperWeaponTypeClass* item = (*Array)[i];
        if (item && !_strcmpi(item->ID, pID)) {
            return item;
        }
    }
    return nullptr;
}

SuperWeaponTypeClass* SuperWeaponTypeClass::FindByIndex(int32 index) {
    if (!Array || index < 0 || index >= Array->Count) return nullptr;
    return (*Array)[index];
}

SuperWeaponTypeClass* SuperWeaponTypeClass::FindByType(SuperWeaponType type) {
    if (!Array) return nullptr;
    for (int32 i = 0; i < Array->Count; ++i) {
        SuperWeaponTypeClass* item = (*Array)[i];
        if (item && item->Type == type) {
            return item;
        }
    }
    return nullptr;
}

int32 SuperWeaponTypeClass::GetCount() {
    if (!Array) return 0;
    return Array->Count;
}

// ============================================================================
// Construction / Destruction
// ============================================================================

SuperWeaponTypeClass::SuperWeaponTypeClass(const char* pID) noexcept :
    ID{0},
    Name{0},
    UIName{0},
    WeaponType(nullptr),
    Type(SuperWeaponType::None),
    RechargeTime(4500),
    Cost(0),
    Side(0),
    Action(0),
    IsPowered(true),
    IsPersistent(false),
    IsOneTime(false),
    DisableableFromShell(false),
    DisableableFromUI(false),
    UseChargeDrain(false),
    ShowTimer(false),
    IsAuxBuilding(false),
    IsGranted(false),
    IsFullMap(false),
    IsAvailable(false),
    IsForbidden(false),
    IsTrain(false),
    IsClickLaunch(false),
    IsDesignator(false),
    IsMultiType(false),
    IsManual(false),
    IsTemporal(false),
    IsPowered_(false),
    IsCharged(false),
    PadByte1(0),
    PadByte2(0),
    PadByte3(0),
    PreClick(false),
    PostClick(false),
    AIDefendAgainst(false),
    ManualControl(false),
    Cursor(0),
    NoCursor(0),
    AnimCount(0),
    PreDependent(-1),
    FlashSidebarTabFrames(-1),
    SpecialSound(-1),
    StartSound(-1),
    LineMultiplier(0),
    Range(0.0f),
    unknown_3C(0),
    PreSound(-1),
    PreSoundPriority(0),
    PostSound(-1),
    PostSoundPriority(0),
    ReadySound(-1),
    ReadySoundPriority(0),
    LightSize(0),
    LightIntensity(0.0),
    LightVisibility(0),
    LightRedTint(0.0),
    LightGreenTint(0.0),
    LightBlueTint(0.0),
    LightFlashFrames(0),
    NukeDamage(0),
    NukeRadius(0),
    NukeRadLevel(0),
    NukeRadDuration(0),
    NukeRadColor(0),
    DominatorDamage(0),
    DominatorRadius(0),
    DominatorMaxScroll(0),
    DominatorCaptureToggle(false),
    DominatorPSIDamage(0),
    DominatorPSIChance(0),
    DominatorPSIRange(0),
    DominatorPSIAnim(nullptr),
    LightningDuration(0),
    LightningDamage(0),
    LightningRadius(0),
    LightningDeferment(0),
    LightningStormDuration(0),
    LightningWarhead(nullptr),
    LightningHitDelay(0),
    LightningScatterDelay(0),
    LightningCellSpread(0),
    LightningSeparation(0),
    ChronoSphereDuration(0),
    ChronoSphereRadius(0),
    ChronoWarpRadius(0),
    ChronoWarpAnim(nullptr),
    ChronoWarpDamage(0),
    ChronoWarpDamageMax(0),
    ChronoWarpDuration(0),
    ChronoWarpActiveDuration(0),
    ChronoWarpFire(true),
    ParaDropType(nullptr),
    ParaDropPlane(nullptr),
    ParaDropCount(0),
    ParaDropNum(0),
    SpyPlaneType(nullptr),
    SpyPlaneCount(0),
    SpyPlaneMission(MissionType::None),
    GeneticMutatorExplosion(nullptr),
    GeneticMutatorWarhead(nullptr),
    GenetixMutatorDamage(0),
    GeneticMutatorRadius(0),
    SWAnim(nullptr),
    CameraAnim(nullptr),
    FireSound(-1),
    FireSoundPriority(0),
    EVA_Ready(-1),
    EVA_Activated(-1),
    EVA_Detected(-1),
    Message_Ready{0},
    Message_Activated{0},
    Message_Detected{0},
    AuxBuilding{nullptr},
    AuxBuildingCount(0),
    MenuText{0},
    HelpText{0},
    CameoShape(nullptr),
    SidebarImage(nullptr)
{
    std::memset(AuxBuilding, 0, sizeof(AuxBuilding));
}

SuperWeaponTypeClass::~SuperWeaponTypeClass() {}

// ============================================================================
// INI
// ============================================================================

// ============================================================================
// INI value tables
// ============================================================================

namespace {

// Original "Type" / "PreDependent" value list.
const char* const SuperTypeNames[] = {
    "MultiMissile", "IronCurtain", "LightningStorm", "ChronoSphere",
    "ChronoWarp", "ParaDrop", "AmerParaDrop", "PsychicDominator",
    "SpyPlane", "GeneticConverter", "ForceShield", "PsychicReveal"
};

// Original "Action" value list (shared by every INI action key).
const char* const SuperActionNames[] = {
    "None", "Move", "NoMove", "Enter", "Self", "Attack", "Harvest", "Select",
    "ToggleSelect", "Capture", "Eaten", "Repair", "Sell", "SellUnit", "NoSell",
    "NoRepair", "Sabotage", "Tote", "DontUse2", "DontUse3", "Nuke",
    "DontUse4", "DontUse5", "DontUse6", "DontUse7", "DontUse8",
    "GuardArea", "Heal", "Damage", "GRepair", "NoDeploy", "NoEnter",
    "NoGRepair", "TogglePower", "NoTogglePower", "EnterTunnel", "NoEnterTunnel",
    "IronCurtain", "LightningStorm", "ChronoSphere", "ChronoWarp", "ParaDrop",
    "PlaceWaypoint", "TibSunBug", "EnterWaypointMode", "FollowWaypoint",
    "SelectWaypoint", "LoopWaypointPath", "DragWaypoint", "AttackWaypoint",
    "EnterWaypoint", "PatrolWaypoint", "AreaAttack", "IvanBomb", "NoIvanBomb",
    "Detonate", "DetonateAll", "DisarmBomb", "SelectNode", "AttackSupport",
    "PlaceBeacon", "SelectBeacon", "AttackMoveNav", "AttackMoveTar", "Demolish",
    "AmerParaDrop", "PsychicDominator", "SpyPlane", "GeneticConverter",
    "ForceShield", "NoForceShield", "Airstrike", "PsychicReveal"
};

int32 MatchSuperType(const char* pText) {
    if (!pText) return -1;
    for (int32 i = 0; i < 12; ++i) {
        if (!_strcmpi(SuperTypeNames[i], pText)) return i;
    }
    return -1;
}

int32 MatchSuperAction(const char* pText) {
    if (!pText) return -1;
    const int32 count = static_cast<int32>(sizeof(SuperActionNames) / sizeof(SuperActionNames[0]));
    for (int32 i = 0; i < count; ++i) {
        if (!_strcmpi(SuperActionNames[i], pText)) return i;
    }
    return -1;
}

} // namespace

bool SuperWeaponTypeClass::LoadFromINI(CCINIClass* pINI) {
    if (!pINI) return false;
    const char* section = ID;
    if (!section || !section[0]) return false;

    // Read type name
    char nameBuf[256];
    pINI->ReadString(section, "Name", "", nameBuf, sizeof(nameBuf));
    int32 j = 0;
    while (nameBuf[j] && j < 31) { Name[j] = nameBuf[j]; ++j; }
    Name[j] = '\0';

    // UIName
    char uiNameBuf[256];
    pINI->ReadString(section, "UIName", "", uiNameBuf, sizeof(uiNameBuf));
    j = 0;
    while (uiNameBuf[j] && j < 31) { UIName[j] = uiNameBuf[j]; ++j; }
    UIName[j] = '\0';

    // WeaponType - resolved through the weapon-type registry.
    char wtBuf[128];
    if (pINI->ReadString(section, "WeaponType", "", wtBuf, sizeof(wtBuf)) && wtBuf[0]) {
        WeaponType = WeaponTypeClass::FindOrAllocate(wtBuf);
    }

    // Action - matched against the shared action string list.
    char actBuf[64];
    pINI->ReadString(section, "Action", "", actBuf, sizeof(actBuf));
    int32 actionIdx = MatchSuperAction(actBuf);
    if (actionIdx >= 0) {
        Action = actionIdx;
    }

    IsPowered = pINI->ReadBool(section, "IsPowered", IsPowered);
    DisableableFromShell = pINI->ReadBool(section, "DisableableFromShell", DisableableFromShell);
    FlashSidebarTabFrames = pINI->ReadInteger(section, "FlashSidebarTabFrames", FlashSidebarTabFrames);
    AIDefendAgainst = pINI->ReadBool(section, "AIDefendAgainst", AIDefendAgainst);
    PreClick = pINI->ReadBool(section, "PreClick", PreClick);
    PostClick = pINI->ReadBool(section, "PostClick", PostClick);
    ShowTimer = pINI->ReadBool(section, "ShowTimer", ShowTimer);

    // SpecialSound / StartSound - vocal sample name lookups; the sample
    // index is resolved against the vocal registry when it is populated.
    char sndBuf[128];
    if (pINI->ReadString(section, "SpecialSound", "", sndBuf, sizeof(sndBuf)) && sndBuf[0]) {
        SpecialSound = -1;
    }
    if (pINI->ReadString(section, "StartSound", "", sndBuf, sizeof(sndBuf)) && sndBuf[0]) {
        StartSound = -1;
    }

    Range = pINI->ReadFloat(section, "Range", Range);
    LineMultiplier = pINI->ReadInteger(section, "LineMultiplier", LineMultiplier);

    // Type - matched against the super-weapon type string list; an
    // unmatched value leaves the type untouched.
    char typeBuf[64];
    pINI->ReadString(section, "Type", "", typeBuf, sizeof(typeBuf));
    int32 typeIdx = MatchSuperType(typeBuf);
    if (typeIdx >= 0) {
        Type = static_cast<SuperWeaponType>(typeIdx);
    }

    // PreDependent - same string list as Type.
    char preBuf[64];
    pINI->ReadString(section, "PreDependent", "", preBuf, sizeof(preBuf));
    int32 preIdx = MatchSuperType(preBuf);
    if (preIdx >= 0) {
        PreDependent = preIdx;
    }

    // AuxBuilding - a single prerequisite building type.
    char auxBuf[64];
    AuxBuildingCount = 0;
    if (pINI->ReadString(section, "AuxBuilding", "", auxBuf, sizeof(auxBuf)) && auxBuf[0]) {
        BuildingTypeClass* bt = BuildingTypeClass::Find(auxBuf);
        if (bt) {
            AuxBuilding[0] = bt;
            AuxBuildingCount = 1;
        }
    }

    UseChargeDrain = pINI->ReadBool(section, "UseChargeDrain", UseChargeDrain);
    ManualControl = pINI->ReadBool(section, "ManualControl", ManualControl);

    // RechargeTime is expressed in minutes; the original scales it by 900
    // frames per minute.  A missing or zero value keeps the default.
    float recharge = pINI->ReadFloat(section, "RechargeTime", 0.0f);
    if (recharge != 0.0f) {
        RechargeTime = static_cast<int32>(recharge * 900.0f);
    }

    // SidebarImage - base cameo name; the ".SHP" suffix is appended when
    // the art file is resolved.
    char imgBuf[64];
    if (pINI->ReadString(section, "SidebarImage", "", imgBuf, sizeof(imgBuf)) && imgBuf[0]) {
        j = 0;
        while (imgBuf[j] && j < 23) { SidebarImageName[j] = imgBuf[j]; ++j; }
        SidebarImageName[j] = '\0';
    } else {
        SidebarImageName[0] = '\0';
    }


    // generated-ini-reads
    // ------------------------------------------------------------------
    // Full key set - every field keeps its current value when the key
    // is absent, so partially specified sections stay valid.
    // ------------------------------------------------------------------
    CCINIClass* pArt = &CCINIClass::INI_Art;
    if (pArt == nullptr)
        pArt = pINI;

    Action = pINI->ReadInteger(section, "Action", Action);
    IsPowered = pINI->ReadBool(section, "IsPowered", IsPowered);
    DisableableFromShell = pINI->ReadBool(section, "DisableableFromShell", DisableableFromShell);
    FlashSidebarTabFrames = pINI->ReadInteger(section, "FlashSidebarTabFrames", FlashSidebarTabFrames);
    AIDefendAgainst = pINI->ReadBool(section, "AIDefendAgainst", AIDefendAgainst);
    PreClick = pINI->ReadBool(section, "PreClick", PreClick);
    PostClick = pINI->ReadBool(section, "PostClick", PostClick);
    ShowTimer = pINI->ReadBool(section, "ShowTimer", ShowTimer);
    Range = pINI->ReadFixed(section, "Range", Range);
    LineMultiplier = pINI->ReadInteger(section, "LineMultiplier", LineMultiplier);
    UseChargeDrain = pINI->ReadBool(section, "UseChargeDrain", UseChargeDrain);
    ManualControl = pINI->ReadBool(section, "ManualControl", ManualControl);
    RechargeTime = pINI->ReadFixed(section, "RechargeTime", RechargeTime);

        return true;
}

// ============================================================================
// Helper methods
// ============================================================================

bool SuperWeaponTypeClass::IsTargetable() const {
    return Type == SuperWeaponType::MultiMissile ||
           Type == SuperWeaponType::LightningStorm ||
           Type == SuperWeaponType::PsychicDominator ||
           Type == SuperWeaponType::ChronoSphere ||
           Type == SuperWeaponType::ChronoWarp ||
           Type == SuperWeaponType::ParaDrop ||
           Type == SuperWeaponType::SpyPlane;
}

bool SuperWeaponTypeClass::IsAutoFire() const {
    return Type == SuperWeaponType::IronCurtain ||
           Type == SuperWeaponType::ForceShield ||
           Type == SuperWeaponType::PsychicReveal;
}

bool SuperWeaponTypeClass::IsSelfTargeted() const {
    return Type == SuperWeaponType::IronCurtain ||
           Type == SuperWeaponType::ForceShield ||
           Type == SuperWeaponType::PsychicReveal;
}

bool SuperWeaponTypeClass::IsDesignatable() const {
    return IsDesignator;
}

bool SuperWeaponTypeClass::RequiresBuilding() const {
    return AuxBuildingCount > 0;
}

bool SuperWeaponTypeClass::HasSound() const {
    return PreSound >= 0 || PostSound >= 0 || ReadySound >= 0 || FireSound >= 0;
}

bool SuperWeaponTypeClass::HasEVA() const {
    return EVA_Ready >= 0 || EVA_Activated >= 0 || EVA_Detected >= 0;
}

bool SuperWeaponTypeClass::HasMessage() const {
    return Message_Ready[0] != '\0' ||
           Message_Activated[0] != '\0' ||
           Message_Detected[0] != '\0';
}

bool SuperWeaponTypeClass::HasLight() const {
    return LightSize > 0 && LightIntensity > 0.0;
}

int32 SuperWeaponTypeClass::GetRechargeTime() const {
    return RechargeTime;
}

int32 SuperWeaponTypeClass::GetCost() const {
    return Cost;
}

// ============================================================================
// SuperWeaponTypeClass - static lookup helpers
// ============================================================================

SuperWeaponTypeClass* SuperWeaponTypeClass::FindOrAllocate(const char* pID)
{
    if (!pID || !_strcmpi(pID, "<none>") || !_strcmpi(pID, "none")) return nullptr;
    SuperWeaponTypeClass* found = Find(pID);
    if (found) return found;
    SuperWeaponTypeClass* newItem = GameCreate<SuperWeaponTypeClass>(pID);
    if (newItem && Array) Array->Add(newItem);
    return newItem;
}
