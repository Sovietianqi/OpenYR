#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Containers/DynamicVectorClass.h"

// ============================================================================
// Forward declarations
// ============================================================================

class AnimTypeClass;
class WarheadTypeClass;
class BuildingTypeClass;
class HouseTypeClass;
class AircraftTypeClass;
class InfantryTypeClass;
class SHPStruct;

// ============================================================================
// SuperWeaponType
// ============================================================================

enum class SuperWeaponType : int32 {
    None                = 0,
    Nuke                = 1,
    IronCurtain         = 2,
    ForceShield         = 3,
    LightningStorm      = 4,
    PsychicDominator    = 5,
    GeneticMutator      = 6,
    ChronoSphere        = 7,
    ChronoWarp          = 8,
    ParaDrop            = 9,
    SpyPlane            = 10,
    PsychicReveal       = 11,
    SonarPulse          = 12,
    HunterSeeker        = 13,
    DropPod             = 14,
    Count               = 15
};

enum class SuperWeaponAction : int32 {
    None                = 0,
    Nuke                = 1,
    IronCurtain         = 2,
    ForceShield         = 3,
    LightningStorm      = 4,
    PsychicDominator    = 5,
    GeneticMutator      = 6,
    ChronoSphere        = 7,
    ChronoWarp          = 8,
    ParaDrop            = 9,
    SpyPlane            = 10,
    PsychicReveal       = 11,
    SonarPulse          = 12,
    HunterSeeker        = 13,
    DropPod             = 14,
    Count               = 15
};

enum class MissionType : int32 {
    None                = 0,
    Attack              = 1,
    Move                = 2,
    Guard               = 3,
    Hunt                = 4,
    Paradrop            = 5,
    Count               = 6
};

// ============================================================================
// SuperWeaponTypeClass
// ============================================================================

class SuperWeaponTypeClass {
public:
    static DynamicVectorClass<SuperWeaponTypeClass*>* Array;

    static SuperWeaponTypeClass* Find(const char* pID);
    static SuperWeaponTypeClass* FindByIndex(int32 index);
    static SuperWeaponTypeClass* FindByType(SuperWeaponType type);
    static int32 GetCount();

    SuperWeaponTypeClass(const char* pID) noexcept;
    ~SuperWeaponTypeClass();

    bool LoadFromINI(CCINIClass* pINI);

    // Helper methods
    bool IsTargetable() const;
    bool IsAutoFire() const;
    bool IsSelfTargeted() const;
    bool IsDesignatable() const;
    bool RequiresBuilding() const;
    bool HasSound() const;
    bool HasEVA() const;
    bool HasMessage() const;
    bool HasLight() const;
    int32 GetRechargeTime() const;
    int32 GetCost() const;

    static void RegisterAll();

public:
    static SuperWeaponTypeClass* Last;
    static int32 Count;

    char ID[0x20];
    char Name[0x20];
    char UIName[0x20];
    SuperWeaponType Type;
    int32 RechargeTime;
    int32 Cost;
    int32 Side;
    SuperWeaponAction Action;
    bool IsPowered;
    bool IsPersistent;
    bool IsOneTime;
    bool DisableableFromShell;
    bool DisableableFromUI;
    bool UseChargeDrain;
    bool ShowTimer;
    bool IsAuxBuilding;
    bool IsGranted;
    bool IsFullMap;
    bool IsAvailable;
    bool IsForbidden;
    bool IsTrain;
    bool IsClickLaunch;
    bool IsDesignator;
    bool IsMultiType;
    bool IsManual;
    bool IsTemporal;
    bool IsPowered_;
    bool IsCharged;
    BYTE PadByte1;
    BYTE PadByte2;
    BYTE PadByte3;
    bool PreClick;
    bool PostClick;
    int32 Cursor;
    int32 NoCursor;
    int32 AnimCount;
    int32 PreDependent;
    int32 FlashSidebarTabFrames;
    int32 unknown_3C;
    int32 PreSound;
    int32 PreSoundPriority;
    int32 PostSound;
    int32 PostSoundPriority;
    int32 ReadySound;
    int32 ReadySoundPriority;
    int32 LightSize;
    double LightIntensity;
    int32 LightVisibility;
    double LightRedTint;
    double LightGreenTint;
    double LightBlueTint;
    int32 LightFlashFrames;

    // Nuke specific
    int32 NukeDamage;
    int32 NukeRadius;
    int32 NukeRadLevel;
    int32 NukeRadDuration;
    int32 NukeRadColor;

    // Dominator specific
    int32 DominatorDamage;
    int32 DominatorRadius;
    int32 DominatorMaxScroll;
    bool DominatorCaptureToggle;
    int32 DominatorPSIDamage;
    int32 DominatorPSIChance;
    int32 DominatorPSIRange;
    AnimTypeClass* DominatorPSIAnim;

    // Lightning specific
    int32 LightningDuration;
    int32 LightningDamage;
    int32 LightningRadius;
    int32 LightningDeferment;
    int32 LightningStormDuration;
    WarheadTypeClass* LightningWarhead;
    int32 LightningHitDelay;
    int32 LightningScatterDelay;
    int32 LightningCellSpread;
    int32 LightningSeparation;

    // ChronoSphere specific
    int32 ChronoSphereDuration;
    int32 ChronoSphereRadius;

    // ChronoWarp specific
    int32 ChronoWarpRadius;
    AnimTypeClass* ChronoWarpAnim;
    int32 ChronoWarpDamage;
    int32 ChronoWarpDamageMax;
    int32 ChronoWarpDuration;
    int32 ChronoWarpActiveDuration;
    bool ChronoWarpFire;

    // ParaDrop specific
    InfantryTypeClass* ParaDropType;
    AircraftTypeClass* ParaDropPlane;
    int32 ParaDropCount;
    int32 ParaDropNum;

    // SpyPlane specific
    AircraftTypeClass* SpyPlaneType;
    int32 SpyPlaneCount;
    MissionType SpyPlaneMission;

    // GeneticMutator specific
    AnimTypeClass* GeneticMutatorExplosion;
    WarheadTypeClass* GeneticMutatorWarhead;
    int32 GenetixMutatorDamage;
    int32 GeneticMutatorRadius;

    // Animations
    AnimTypeClass* SWAnim;
    AnimTypeClass* CameraAnim;

    // Sounds
    int32 FireSound;
    int32 FireSoundPriority;

    // EVA Events
    int32 EVA_Ready;
    int32 EVA_Activated;
    int32 EVA_Detected;

    // Messages
    char Message_Ready[0x40];
    char Message_Activated[0x40];
    char Message_Detected[0x40];

    // Auxiliary buildings
    BuildingTypeClass* AuxBuilding[8];
    int32 AuxBuildingCount;

    // UI
    char MenuText[0x20];
    char HelpText[0x20];
    SHPStruct* CameoShape;
    SHPStruct* SidebarImage;
};