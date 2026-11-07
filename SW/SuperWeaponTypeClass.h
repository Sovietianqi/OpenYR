#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Containers/DynamicVectorClass.h"

// ============================================================================
// Forward declarations
// ============================================================================

class AnimTypeClass;
class WarheadTypeClass;
class WeaponTypeClass;
class BuildingTypeClass;
class HouseTypeClass;
class AircraftTypeClass;
class InfantryTypeClass;
class SHPStruct;
class CRCEngine;
class IStream;
class TechnoClass;

// ============================================================================
// SuperWeaponType
//
//  Values mirror the original "Type" string list (strlist_SuperTypes):
//  MultiMissile, IronCurtain, LightningStorm, ChronoSphere, ChronoWarp,
//  ParaDrop, AmerParaDrop, PsychicDominator, SpyPlane, GeneticConverter,
//  ForceShield, PsychicReveal.  An unset Type stays -1 in the original
//  constructor.
// ============================================================================

enum class SuperWeaponType : int32 {
    None                = -1,
    MultiMissile        = 0,
    IronCurtain         = 1,
    LightningStorm      = 2,
    ChronoSphere        = 3,
    ChronoWarp          = 4,
    ParaDrop            = 5,
    AmerParaDrop        = 6,
    PsychicDominator    = 7,
    SpyPlane            = 8,
    GeneticConverter    = 9,
    ForceShield         = 10,
    PsychicReveal       = 11,
    Count               = 12
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
    static SuperWeaponTypeClass* FindOrAllocate(const char* pID);
    static SuperWeaponTypeClass* FindByIndex(int32 index);
    static SuperWeaponTypeClass* FindByType(SuperWeaponType type);

    // asm: index in [SWTypes]; returns the registry slot of pID, or -1.
    // (SuperWeaponTypeClass_FindIndex)
    static int32 FindIndex(const char* pID);

    // asm: SuperWeaponType_FindFirstOfAction (0x6CEEB0)
    // Walks the type registry and returns the first entry whose Action
    // equals the requested value; null when nothing matches.
    static SuperWeaponTypeClass* FindFirstOfAction(int32 action);
    static int32 GetCount();

    SuperWeaponTypeClass(const char* pID) noexcept;
    ~SuperWeaponTypeClass();

    bool LoadFromINI(CCINIClass* pINI);

    // ------------------------------------------------------------------
    // Persistence / registry (asm parity)
    // ------------------------------------------------------------------
    // asm: SuperWeaponTypeClass_GetClassID  (retn 8, out GUID*)
    HRESULT GetClassID(CLSID* pClassID) const;
    // asm: SuperWeaponTypeClass_Load  (base AbstractTypeClass_Load + swizzle)
    HRESULT Load(IStream* pStm);
    // asm: SuperWeaponTypeClass_Save  (base AbstractTypeClass_Save)
    HRESULT Save(IStream* pStm, BOOL fClearDirty);
    // asm: SuperWeaponTypeClass_GetAbstractDerivationID -> 0x20
    int32 GetAbstractDerivationID() const;
    // asm: SuperWeaponTypeClass_GetSize -> 0x100
    int32 GetSize() const;
    // asm: SuperWeaponTypeClass_GetCRC (walks base CRC then tagged fields)
    int32 GetCRC() const;
    // asm: SuperWeaponTypeClass_GetArrayIndex -> [this+0x98]
    int32 GetArrayIndex() const;

    // asm: SuperWeaponType_GetCursorOverObject  (retn 8)
    // Returns the mouse cursor for hovering a target; ForceShield against an
    // allied building yields the "cannot place" cursor (0x46), otherwise the
    // type's Action value is used as the cursor id.
    int32 GetCursorOverObject(int32 currentCursor, TechnoClass* pTarget) const;

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

public:
    static SuperWeaponTypeClass* Last;
    static int32 Count;

    char ID[0x20];
    char Name[0x20];
    char UIName[0x20];
    WeaponTypeClass* WeaponType;
    SuperWeaponType Type;
    int32 RechargeTime;
    int32 Cost;
    int32 Side;
    int32 Action;
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
    bool AIDefendAgainst;
    bool ManualControl;
    int32 Cursor;
    int32 NoCursor;
    int32 AnimCount;
    int32 PreDependent;
    int32 FlashSidebarTabFrames;
    int32 SpecialSound;
    int32 StartSound;
    int32 LineMultiplier;
    float Range;
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
    // 根据游戏行为，可知侧栏 cameo 有常规充能旗标与充能展示态两套就绪
    // 表达；页签闪烁帧数窗口由 FlashSidebarTabFrames 承载。
    bool UsesCameoChargeState = false;
    int32 AuxBuildingCount;

    // UI
    char MenuText[0x20];
    char HelpText[0x20];
    char SidebarImageName[0x18];
    SHPStruct* CameoShape;
    SHPStruct* SidebarImage;
    // asm: registry slot index ([this+0x98]); assigned from the array length
    // at construction, returned verbatim by GetArrayIndex.
    int32 ArrayIndex;
    // ------------------------------------------------------------------
    // Rules / Art INI fields
    // ------------------------------------------------------------------
};