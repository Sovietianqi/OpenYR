#include "SuperWeaponTypeClass.h"
#include "../INI/INIClass.h"
#include "../Houses/HouseTypeClass.h"
#include "../Animations/AnimTypeClass.h"
#include "../Combat/WarheadTypeClass.h"
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
    Type(SuperWeaponType::None),
    RechargeTime(0),
    Cost(0),
    Side(0),
    Action(SuperWeaponAction::None),
    IsPowered(false),
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
    Cursor(0),
    NoCursor(0),
    AnimCount(0),
    PreDependent(0),
    FlashSidebarTabFrames(0),
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

    // Type
    char typeBuf[64];
    pINI->ReadString(section, "Type", "", typeBuf, sizeof(typeBuf));
    if (!_strcmpi(typeBuf, "Nuke")) Type = SuperWeaponType::Nuke;
    else if (!_strcmpi(typeBuf, "IronCurtain")) Type = SuperWeaponType::IronCurtain;
    else if (!_strcmpi(typeBuf, "ForceShield")) Type = SuperWeaponType::ForceShield;
    else if (!_strcmpi(typeBuf, "LightningStorm")) Type = SuperWeaponType::LightningStorm;
    else if (!_strcmpi(typeBuf, "PsychicDominator")) Type = SuperWeaponType::PsychicDominator;
    else if (!_strcmpi(typeBuf, "GeneticMutator")) Type = SuperWeaponType::GeneticMutator;
    else if (!_strcmpi(typeBuf, "ChronoSphere")) Type = SuperWeaponType::ChronoSphere;
    else if (!_strcmpi(typeBuf, "ChronoWarp")) Type = SuperWeaponType::ChronoWarp;
    else if (!_strcmpi(typeBuf, "ParaDrop")) Type = SuperWeaponType::ParaDrop;
    else if (!_strcmpi(typeBuf, "SpyPlane")) Type = SuperWeaponType::SpyPlane;
    else if (!_strcmpi(typeBuf, "PsychicReveal")) Type = SuperWeaponType::PsychicReveal;
    else if (!_strcmpi(typeBuf, "SonarPulse")) Type = SuperWeaponType::SonarPulse;
    else if (!_strcmpi(typeBuf, "HunterSeeker")) Type = SuperWeaponType::HunterSeeker;
    else if (!_strcmpi(typeBuf, "DropPod")) Type = SuperWeaponType::DropPod;
    else Type = SuperWeaponType::None;

    // Basic properties
    RechargeTime = pINI->ReadInteger(section, "RechargeTime", 0);
    Cost = pINI->ReadInteger(section, "Cost", 0);
    Side = pINI->ReadInteger(section, "Side", 0);
    IsPowered = pINI->ReadBool(section, "IsPowered", false);
    IsPersistent = pINI->ReadBool(section, "IsPersistent", false);
    IsOneTime = pINI->ReadBool(section, "IsOneTime", false);
    DisableableFromShell = pINI->ReadBool(section, "DisableableFromShell", false);
    DisableableFromUI = pINI->ReadBool(section, "DisableableFromUI", false);
    UseChargeDrain = pINI->ReadBool(section, "UseChargeDrain", false);
    ShowTimer = pINI->ReadBool(section, "ShowTimer", false);
    IsAuxBuilding = pINI->ReadBool(section, "IsAuxBuilding", false);
    IsGranted = pINI->ReadBool(section, "IsGranted", false);
    IsFullMap = pINI->ReadBool(section, "IsFullMap", false);
    IsAvailable = pINI->ReadBool(section, "IsAvailable", false);
    IsForbidden = pINI->ReadBool(section, "IsForbidden", false);
    IsTrain = pINI->ReadBool(section, "IsTrain", false);
    IsClickLaunch = pINI->ReadBool(section, "IsClickLaunch", false);
    IsDesignator = pINI->ReadBool(section, "IsDesignator", false);
    IsMultiType = pINI->ReadBool(section, "IsMultiType", false);
    IsManual = pINI->ReadBool(section, "IsManual", false);
    IsTemporal = pINI->ReadBool(section, "IsTemporal", false);
    PreClick = pINI->ReadBool(section, "PreClick", false);
    PostClick = pINI->ReadBool(section, "PostClick", false);

    // Action
    char actionBuf[64];
    pINI->ReadString(section, "Action", "", actionBuf, sizeof(actionBuf));
    if (!_strcmpi(actionBuf, "Nuke")) Action = SuperWeaponAction::Nuke;
    else if (!_strcmpi(actionBuf, "IronCurtain")) Action = SuperWeaponAction::IronCurtain;
    else if (!_strcmpi(actionBuf, "ForceShield")) Action = SuperWeaponAction::ForceShield;
    else if (!_strcmpi(actionBuf, "LightningStorm")) Action = SuperWeaponAction::LightningStorm;
    else if (!_strcmpi(actionBuf, "PsychicDominator")) Action = SuperWeaponAction::PsychicDominator;
    else if (!_strcmpi(actionBuf, "GeneticMutator")) Action = SuperWeaponAction::GeneticMutator;
    else if (!_strcmpi(actionBuf, "ChronoSphere")) Action = SuperWeaponAction::ChronoSphere;
    else if (!_strcmpi(actionBuf, "ChronoWarp")) Action = SuperWeaponAction::ChronoWarp;
    else if (!_strcmpi(actionBuf, "ParaDrop")) Action = SuperWeaponAction::ParaDrop;
    else if (!_strcmpi(actionBuf, "SpyPlane")) Action = SuperWeaponAction::SpyPlane;
    else if (!_strcmpi(actionBuf, "PsychicReveal")) Action = SuperWeaponAction::PsychicReveal;
    else if (!_strcmpi(actionBuf, "SonarPulse")) Action = SuperWeaponAction::SonarPulse;
    else if (!_strcmpi(actionBuf, "HunterSeeker")) Action = SuperWeaponAction::HunterSeeker;
    else if (!_strcmpi(actionBuf, "DropPod")) Action = SuperWeaponAction::DropPod;
    else Action = SuperWeaponAction::None;

    // Cursor
    Cursor = pINI->ReadInteger(section, "Cursor", 0);
    NoCursor = pINI->ReadInteger(section, "NoCursor", 0);

    // AnimCount
    AnimCount = pINI->ReadInteger(section, "AnimCount", 0);
    PreDependent = pINI->ReadInteger(section, "PreDependent", 0);
    FlashSidebarTabFrames = pINI->ReadInteger(section, "FlashSidebarTabFrames", 0);

    // Sounds
    PreSound = pINI->ReadInteger(section, "PreSound", -1);
    PreSoundPriority = pINI->ReadInteger(section, "PreSoundPriority", 0);
    PostSound = pINI->ReadInteger(section, "PostSound", -1);
    PostSoundPriority = pINI->ReadInteger(section, "PostSoundPriority", 0);
    ReadySound = pINI->ReadInteger(section, "ReadySound", -1);
    ReadySoundPriority = pINI->ReadInteger(section, "ReadySoundPriority", 0);
    FireSound = pINI->ReadInteger(section, "FireSound", -1);
    FireSoundPriority = pINI->ReadInteger(section, "FireSoundPriority", 0);

    // Light
    LightSize = pINI->ReadInteger(section, "LightSize", 0);
    LightIntensity = pINI->ReadFloat(section, "LightIntensity", 0.0);
    LightVisibility = pINI->ReadInteger(section, "LightVisibility", 0);
    LightRedTint = pINI->ReadFloat(section, "LightRedTint", 0.0);
    LightGreenTint = pINI->ReadFloat(section, "LightGreenTint", 0.0);
    LightBlueTint = pINI->ReadFloat(section, "LightBlueTint", 0.0);
    LightFlashFrames = pINI->ReadInteger(section, "LightFlashFrames", 0);

    // EVA events
    EVA_Ready = pINI->ReadInteger(section, "EVA.Ready", -1);
    EVA_Activated = pINI->ReadInteger(section, "EVA.Activated", -1);
    EVA_Detected = pINI->ReadInteger(section, "EVA.Detected", -1);

    // Message text
    char msgBuf[256];
    pINI->ReadString(section, "Message.Ready", "", msgBuf, sizeof(msgBuf));
    j = 0;
    while (msgBuf[j] && j < 63) { Message_Ready[j] = msgBuf[j]; ++j; }
    Message_Ready[j] = '\0';

    pINI->ReadString(section, "Message.Activated", "", msgBuf, sizeof(msgBuf));
    j = 0;
    while (msgBuf[j] && j < 63) { Message_Activated[j] = msgBuf[j]; ++j; }
    Message_Activated[j] = '\0';

    pINI->ReadString(section, "Message.Detected", "", msgBuf, sizeof(msgBuf));
    j = 0;
    while (msgBuf[j] && j < 63) { Message_Detected[j] = msgBuf[j]; ++j; }
    Message_Detected[j] = '\0';

    // Menu/Help text
    char menuBuf[256];
    pINI->ReadString(section, "MenuText", "", menuBuf, sizeof(menuBuf));
    j = 0;
    while (menuBuf[j] && j < 31) { MenuText[j] = menuBuf[j]; ++j; }
    MenuText[j] = '\0';

    char helpBuf[256];
    pINI->ReadString(section, "HelpText", "", helpBuf, sizeof(helpBuf));
    j = 0;
    while (helpBuf[j] && j < 31) { HelpText[j] = helpBuf[j]; ++j; }
    HelpText[j] = '\0';

    // Animations
    char animBuf[64];
    pINI->ReadString(section, "SW.Animation", "", animBuf, sizeof(animBuf));
    if (animBuf[0]) {
        SWAnim = AnimTypeClass::Find(animBuf);
    }

    pINI->ReadString(section, "SW.AnimCamera", "", animBuf, sizeof(animBuf));
    if (animBuf[0]) {
        CameraAnim = AnimTypeClass::Find(animBuf);
    }

    // Nuke-specific
    if (Type == SuperWeaponType::Nuke) {
        NukeDamage = pINI->ReadInteger(section, "Nuke.Damage", 1000);
        NukeRadius = pINI->ReadInteger(section, "Nuke.Radius", 10);
        NukeRadLevel = pINI->ReadInteger(section, "Nuke.RadLevel", 500);
        NukeRadDuration = pINI->ReadInteger(section, "Nuke.RadDuration", 600);
        NukeRadColor = pINI->ReadInteger(section, "Nuke.RadColor", 0);
    }

    // Dominator-specific
    if (Type == SuperWeaponType::PsychicDominator) {
        DominatorDamage = pINI->ReadInteger(section, "Dominator.Damage", 100);
        DominatorRadius = pINI->ReadInteger(section, "Dominator.Radius", 5);
        DominatorMaxScroll = pINI->ReadInteger(section, "Dominator.MaxScroll", 0);
        DominatorCaptureToggle = pINI->ReadBool(section, "Dominator.CaptureToggle", false);
        DominatorPSIDamage = pINI->ReadInteger(section, "Dominator.PSIDamage", 0);
        DominatorPSIChance = pINI->ReadInteger(section, "Dominator.PSIChance", 0);
        DominatorPSIRange = pINI->ReadInteger(section, "Dominator.PSIRange", 0);

        pINI->ReadString(section, "Dominator.PSIAnim", "", animBuf, sizeof(animBuf));
        if (animBuf[0]) {
            DominatorPSIAnim = AnimTypeClass::Find(animBuf);
        }
    }

    // Lightning-specific
    if (Type == SuperWeaponType::LightningStorm) {
        LightningDuration = pINI->ReadInteger(section, "Lightning.Duration", 420);
        LightningDamage = pINI->ReadInteger(section, "Lightning.Damage", 150);
        LightningRadius = pINI->ReadInteger(section, "Lightning.Radius", 3);
        LightningDeferment = pINI->ReadInteger(section, "Lightning.Deferment", 0);
        LightningStormDuration = pINI->ReadInteger(section, "Lightning.StormDuration", 0);
        LightningHitDelay = pINI->ReadInteger(section, "Lightning.HitDelay", 30);
        LightningScatterDelay = pINI->ReadInteger(section, "Lightning.ScatterDelay", 10);
        LightningCellSpread = pINI->ReadInteger(section, "Lightning.CellSpread", 3);
        LightningSeparation = pINI->ReadInteger(section, "Lightning.Separation", 0);

        char whBuf[64];
        pINI->ReadString(section, "Lightning.Warhead", "", whBuf, sizeof(whBuf));
        if (whBuf[0]) {
            LightningWarhead = WarheadTypeClass::Find(whBuf);
        }
    }

    // ChronoSphere-specific
    if (Type == SuperWeaponType::ChronoSphere) {
        ChronoSphereDuration = pINI->ReadInteger(section, "ChronoSphere.Duration", 60);
        ChronoSphereRadius = pINI->ReadInteger(section, "ChronoSphere.Radius", 0);
    }

    // ChronoWarp-specific
    if (Type == SuperWeaponType::ChronoWarp) {
        ChronoWarpRadius = pINI->ReadInteger(section, "ChronoWarp.Radius", 5);
        ChronoWarpDamage = pINI->ReadInteger(section, "ChronoWarp.Damage", 100);
        ChronoWarpDamageMax = pINI->ReadInteger(section, "ChronoWarp.DamageMax", 200);
        ChronoWarpDuration = pINI->ReadInteger(section, "ChronoWarp.Duration", 60);
        ChronoWarpActiveDuration = pINI->ReadInteger(section, "ChronoWarp.ActiveDuration", 30);
        ChronoWarpFire = pINI->ReadBool(section, "ChronoWarp.Fire", true);

        pINI->ReadString(section, "ChronoWarp.Anim", "", animBuf, sizeof(animBuf));
        if (animBuf[0]) {
            ChronoWarpAnim = AnimTypeClass::Find(animBuf);
        }
    }

    // ParaDrop-specific
    if (Type == SuperWeaponType::ParaDrop) {
        char typeBuf[64];
        pINI->ReadString(section, "ParaDrop.Type", "", typeBuf, sizeof(typeBuf));
        // ParaDropType = AircraftTypeClass::Find(typeBuf);

        pINI->ReadString(section, "ParaDrop.Plane", "", typeBuf, sizeof(typeBuf));
        // ParaDropPlane = AircraftTypeClass::Find(typeBuf);

        ParaDropCount = pINI->ReadInteger(section, "ParaDrop.Count", 0);
        ParaDropNum = pINI->ReadInteger(section, "ParaDrop.Num", 0);
    }

    // SpyPlane-specific
    if (Type == SuperWeaponType::SpyPlane) {
        char typeBuf[64];
        pINI->ReadString(section, "SpyPlane.Type", "", typeBuf, sizeof(typeBuf));
        // SpyPlaneType = AircraftTypeClass::Find(typeBuf);

        SpyPlaneCount = pINI->ReadInteger(section, "SpyPlane.Count", 1);

        char missionBuf[64];
        pINI->ReadString(section, "SpyPlane.Mission", "Attack", missionBuf, sizeof(missionBuf));
        if (!_strcmpi(missionBuf, "Attack")) SpyPlaneMission = MissionType::Attack;
        else if (!_strcmpi(missionBuf, "Move")) SpyPlaneMission = MissionType::Move;
        else if (!_strcmpi(missionBuf, "Guard")) SpyPlaneMission = MissionType::Guard;
        else SpyPlaneMission = MissionType::None;
    }

    // GeneticMutator-specific
    if (Type == SuperWeaponType::GeneticMutator) {
        char expBuf[64];
        pINI->ReadString(section, "GeneticMutator.Explosion", "", expBuf, sizeof(expBuf));
        if (expBuf[0]) {
            GeneticMutatorExplosion = AnimTypeClass::Find(expBuf);
        }

        char whBuf[64];
        pINI->ReadString(section, "GeneticMutator.Warhead", "", whBuf, sizeof(whBuf));
        if (whBuf[0]) {
            GeneticMutatorWarhead = WarheadTypeClass::Find(whBuf);
        }

        GenetixMutatorDamage = pINI->ReadInteger(section, "GeneticMutator.Damage", 0);
        GeneticMutatorRadius = pINI->ReadInteger(section, "GeneticMutator.Radius", 5);
    }

    // AuxBuilding
    char auxBuf[64];
    pINI->ReadString(section, "AuxBuilding", "", auxBuf, sizeof(auxBuf));
    if (auxBuf[0]) {
        AuxBuildingCount = 0;
        // Parse comma-separated building type IDs
        char* token = strtok(auxBuf, ",");
        while (token && AuxBuildingCount < 8) {
            // Skip leading whitespace
            while (*token == ' ' || *token == '\t') ++token;
            // BuildingTypeClass* bt = BuildingTypeClass::Find(token);
            // AuxBuilding[AuxBuildingCount++] = bt;
            token = strtok(nullptr, ",");
        }
    }

    return true;
}

// ============================================================================
// Helper methods
// ============================================================================

bool SuperWeaponTypeClass::IsTargetable() const {
    return Type == SuperWeaponType::Nuke ||
           Type == SuperWeaponType::LightningStorm ||
           Type == SuperWeaponType::PsychicDominator ||
           Type == SuperWeaponType::ChronoSphere ||
           Type == SuperWeaponType::ChronoWarp ||
           Type == SuperWeaponType::ParaDrop ||
           Type == SuperWeaponType::SpyPlane ||
           Type == SuperWeaponType::DropPod;
}

bool SuperWeaponTypeClass::IsAutoFire() const {
    return Type == SuperWeaponType::IronCurtain ||
           Type == SuperWeaponType::ForceShield ||
           Type == SuperWeaponType::PsychicReveal ||
           Type == SuperWeaponType::SonarPulse ||
           Type == SuperWeaponType::HunterSeeker;
}

bool SuperWeaponTypeClass::IsSelfTargeted() const {
    return Type == SuperWeaponType::IronCurtain ||
           Type == SuperWeaponType::ForceShield ||
           Type == SuperWeaponType::PsychicReveal ||
           Type == SuperWeaponType::SonarPulse;
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
// Static registration
// ============================================================================

void SuperWeaponTypeClass::RegisterAll() {
    if (!Array) {
        Array = new DynamicVectorClass<SuperWeaponTypeClass*>();
    }

    struct SWDef {
        const char* ID;
        const char* Name;
        SuperWeaponType Type;
        int32 RechargeTime;
        int32 Cost;
        SuperWeaponAction Action;
        bool IsPowered;
        bool ShowTimer;
        bool IsClickLaunch;
        bool IsDesignator;
        bool IsManual;
        bool IsTemporal;
        const char* AnimName;
        int32 FireSound;
        int32 EVA_Ready;
        int32 EVA_Activated;
        int32 EVA_Detected;
        const char* Message_Ready;
        const char* Message_Activated;
        const char* Message_Detected;
    };

    static const SWDef defs[] = {
        // ── Allied Super Weapons ─────────────────────────────────────────
        {
            "AlliedNuke", "Nuke",
            SuperWeaponType::Nuke, 900, 0,
            SuperWeaponAction::Nuke,
            true, true, true, false, false, false,
            "NUKEBALL", 100, 0, 1, 2,
            "Nuclear missile ready.",
            "Nuclear missile launched.",
            "Nuclear missile detected."
        },
        {
            "WeatherStorm", "Weather Storm",
            SuperWeaponType::LightningStorm, 600, 0,
            SuperWeaponAction::LightningStorm,
            true, true, true, false, false, false,
            "LIGHTNING", 101, 3, 4, 5,
            "Weather storm ready.",
            "Weather storm activated.",
            "Weather storm detected."
        },
        {
            "ChronoSphere", "Chrono Sphere",
            SuperWeaponType::ChronoSphere, 420, 0,
            SuperWeaponAction::ChronoSphere,
            true, true, true, false, false, true,
            "CHRONOFX", 102, 6, 7, 8,
            "Chrono Sphere ready.",
            "Chrono Sphere activated.",
            "Chrono Sphere detected."
        },
        {
            "ChronoWarp", "Chrono Warp",
            SuperWeaponType::ChronoWarp, 360, 0,
            SuperWeaponAction::ChronoWarp,
            true, true, true, false, false, true,
            "CHRONOFX", 102, 6, 7, 8,
            "Chrono Warp ready.",
            "Chrono Warp activated.",
            "Chrono Warp detected."
        },
        {
            "ParaDrop", "Paradrop",
            SuperWeaponType::ParaDrop, 300, 0,
            SuperWeaponAction::ParaDrop,
            true, true, true, false, false, false,
            "PARADROP", 103, 9, 10, 11,
            "Paradrop ready.",
            "Paradrop inbound.",
            "Paradrop detected."
        },
        {
            "SpyPlane", "Spy Plane",
            SuperWeaponType::SpyPlane, 300, 0,
            SuperWeaponAction::SpyPlane,
            true, true, true, false, false, false,
            nullptr, 104, 12, 13, 14,
            "Spy Plane ready.",
            "Spy Plane inbound.",
            "Spy Plane detected."
        },

        // ── Soviet Super Weapons ─────────────────────────────────────────
        {
            "SovietNuke", "Nuclear Missile",
            SuperWeaponType::Nuke, 900, 0,
            SuperWeaponAction::Nuke,
            true, true, true, false, false, false,
            "NUKEBALL", 100, 0, 1, 2,
            "Nuclear missile ready.",
            "Nuclear missile launched.",
            "Nuclear missile detected."
        },
        {
            "IronCurtain", "Iron Curtain",
            SuperWeaponType::IronCurtain, 480, 0,
            SuperWeaponAction::IronCurtain,
            true, true, false, false, false, false,
            "IRONFX", 105, 15, 16, 17,
            "Iron Curtain ready.",
            "Iron Curtain activated.",
            "Iron Curtain detected."
        },
        {
            "ForceShield", "Force Shield",
            SuperWeaponType::ForceShield, 300, 0,
            SuperWeaponAction::ForceShield,
            true, true, false, false, false, false,
            "FORCESHIELD", 106, 18, 19, 20,
            "Force Shield ready.",
            "Force Shield activated.",
            "Force Shield detected."
        },
        {
            "PsychicDominator", "Psychic Dominator",
            SuperWeaponType::PsychicDominator, 600, 0,
            SuperWeaponAction::PsychicDominator,
            true, true, true, false, false, false,
            "DOMINATOR", 107, 21, 22, 23,
            "Psychic Dominator ready.",
            "Psychic Dominator activated.",
            "Psychic Dominator detected."
        },
        {
            "PsychicReveal", "Psychic Reveal",
            SuperWeaponType::PsychicReveal, 300, 0,
            SuperWeaponAction::PsychicReveal,
            true, true, false, false, false, false,
            "PSYCHIC", 108, 24, 25, 26,
            "Psychic Reveal ready.",
            "Psychic Reveal activated.",
            "Psychic Reveal detected."
        },

        // ── Yuri Super Weapons ───────────────────────────────────────────
        {
            "GeneticMutator", "Genetic Mutator",
            SuperWeaponType::GeneticMutator, 600, 0,
            SuperWeaponAction::GeneticMutator,
            true, true, true, false, false, false,
            "DOMINATOR", 109, 27, 28, 29,
            "Genetic Mutator ready.",
            "Genetic Mutator activated.",
            "Genetic Mutator detected."
        },
        {
            "PsychicDominatorYuri", "Psychic Dominator",
            SuperWeaponType::PsychicDominator, 600, 0,
            SuperWeaponAction::PsychicDominator,
            true, true, true, false, false, false,
            "DOMINATOR", 107, 21, 22, 23,
            "Psychic Dominator ready.",
            "Psychic Dominator activated.",
            "Psychic Dominator detected."
        },
        {
            "ForceShieldYuri", "Force Shield",
            SuperWeaponType::ForceShield, 300, 0,
            SuperWeaponAction::ForceShield,
            true, true, false, false, false, false,
            "FORCESHIELD", 106, 18, 19, 20,
            "Force Shield ready.",
            "Force Shield activated.",
            "Force Shield detected."
        },

        // ── Misc Super Weapons ───────────────────────────────────────────
        {
            "SonarPulse", "Sonar Pulse",
            SuperWeaponType::SonarPulse, 300, 0,
            SuperWeaponAction::SonarPulse,
            true, true, false, false, false, false,
            nullptr, 110, 30, 31, 32,
            "Sonar Pulse ready.",
            "Sonar Pulse activated.",
            "Sonar Pulse detected."
        },
        {
            "HunterSeeker", "Hunter Seeker",
            SuperWeaponType::HunterSeeker, 360, 0,
            SuperWeaponAction::HunterSeeker,
            true, true, false, false, false, false,
            nullptr, 111, 33, 34, 35,
            "Hunter Seeker ready.",
            "Hunter Seeker launched.",
            "Hunter Seeker detected."
        },
        {
            "DropPod", "Drop Pod",
            SuperWeaponType::DropPod, 300, 0,
            SuperWeaponAction::DropPod,
            true, true, true, false, false, false,
            "DROPPOD", 112, 36, 37, 38,
            "Drop Pod ready.",
            "Drop Pod inbound.",
            "Drop Pod detected."
        },
    };

    Count = static_cast<int32>(sizeof(defs) / sizeof(defs[0]));

    for (size_t i = 0; i < sizeof(defs) / sizeof(defs[0]); ++i) {
        SuperWeaponTypeClass* sw = new SuperWeaponTypeClass(defs[i].ID);

        int32 j = 0;
        while (defs[i].Name[j] && j < 31) { sw->Name[j] = defs[i].Name[j]; ++j; }
        sw->Name[j] = '\0';

        sw->Type = defs[i].Type;
        sw->RechargeTime = defs[i].RechargeTime;
        sw->Cost = defs[i].Cost;
        sw->Action = defs[i].Action;
        sw->IsPowered = defs[i].IsPowered;
        sw->ShowTimer = defs[i].ShowTimer;
        sw->IsClickLaunch = defs[i].IsClickLaunch;
        sw->IsDesignator = defs[i].IsDesignator;
        sw->IsManual = defs[i].IsManual;
        sw->IsTemporal = defs[i].IsTemporal;
        sw->FireSound = defs[i].FireSound;
        sw->EVA_Ready = defs[i].EVA_Ready;
        sw->EVA_Activated = defs[i].EVA_Activated;
        sw->EVA_Detected = defs[i].EVA_Detected;

        if (defs[i].AnimName) {
            sw->SWAnim = AnimTypeClass::Find(defs[i].AnimName);
        }

        j = 0;
        while (defs[i].Message_Ready[j] && j < 63) { sw->Message_Ready[j] = defs[i].Message_Ready[j]; ++j; }
        sw->Message_Ready[j] = '\0';

        j = 0;
        while (defs[i].Message_Activated[j] && j < 63) { sw->Message_Activated[j] = defs[i].Message_Activated[j]; ++j; }
        sw->Message_Activated[j] = '\0';

        j = 0;
        while (defs[i].Message_Detected[j] && j < 63) { sw->Message_Detected[j] = defs[i].Message_Detected[j]; ++j; }
        sw->Message_Detected[j] = '\0';

        Array->Add(sw);
        Last = sw;
    }
}