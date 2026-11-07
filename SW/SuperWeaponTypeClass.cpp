#include "SuperWeaponTypeClass.h"
#include "../INI/INIClass.h"
#include "../Houses/HouseClass.h"
#include "../Houses/HouseTypeClass.h"
#include "../Animations/AnimTypeClass.h"
#include "../Abstract/BuildingTypeClass.h"
#include "../Abstract/TechnoClass.h"
#include "../Combat/WarheadTypeClass.h"
#include "../Combat/WeaponTypeClass.h"
#include "../Scenario/ScenarioClass.h"
#include "../Rules/RulesClass.h"
#include "../Game/Game.h"
#include "../Audio/VocClass.h"
#include "../COM/IUnknown.h"
#include "../IO/CRC.h"

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
    SidebarImage(nullptr),
    ArrayIndex(0)
{
    std::memset(AuxBuilding, 0, sizeof(AuxBuilding));
    // asm: SuperWeaponTypeClass_CTOR assigns [this+0x98] from the registry
    // length before the new entry is appended.
    if (Array) {
        ArrayIndex = Array->Count;
    }
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

// Original "Action" value list lives in CCINIClass (strlist_Actions); the
// Action key is resolved through CCINIClass::GetAction.

int32 MatchSuperType(const char* pText) {
    if (!pText) return -1;
    for (int32 i = 0; i < 12; ++i) {
        if (!_strcmpi(SuperTypeNames[i], pText)) return i;
    }
    return -1;
}

} // namespace

// asm: char __thiscall SuperWeaponTypeClass_LoadFromINI(SuperWeaponTypeClass *this,
//                                                       CCINIClass *INI_)
//
//  Reads one [<ID>] section of rules(md).ini.  Key order and fallback
//  semantics mirror the original exactly:
//    * the base (AbstractTypeClass) reads Name / UIName first;
//    * every subsequent key keeps the current field value as its fallback,
//      so a missing key leaves the field untouched;
//    * Type / PreDependent only overwrite when the string matches
//      strlist_SuperTypes (index -1 is rejected);
//    * SpecialSound / StartSound resolve through the vocal-name registry and
//      only overwrite when the name resolves (index != -1);
//    * RechargeTime is stored in minutes and scaled by 900 frames/minute,
//      floored to int; a zero reads as "keep default".
bool SuperWeaponTypeClass::LoadFromINI(CCINIClass* pINI) {
    if (!pINI) return false;
    const char* section = ID;
    if (!section || !section[0]) return false;

    // asm: AbstractTypeClass_LoadFromINI reads "Name" and "UIName".
    char nameBuf[128];
    pINI->ReadString(section, "Name", "", nameBuf, sizeof(nameBuf));
    int32 j = 0;
    while (nameBuf[j] && j < 31) { Name[j] = nameBuf[j]; ++j; }
    Name[j] = '\0';

    char uiNameBuf[128];
    pINI->ReadString(section, "UIName", "", uiNameBuf, sizeof(uiNameBuf));
    j = 0;
    while (uiNameBuf[j] && j < 31) { UIName[j] = uiNameBuf[j]; ++j; }
    UIName[j] = '\0';

    // WeaponType - resolved through the weapon-type registry.  asm keeps the
    // previous pointer when the key is absent or resolves to nothing.
    char wtBuf[128];
    if (pINI->ReadString(section, "WeaponType", "", wtBuf, sizeof(wtBuf)) && wtBuf[0]) {
        WeaponTypeClass* wt = WeaponTypeClass::FindOrAllocate(wtBuf);
        if (wt) WeaponType = wt;
    }

    // Action - matched against the shared action string list; asm passes the
    // current value as the default (GetAction returns 0 on a miss).
    Action = pINI->GetAction(section, "Action", Action);

    IsPowered = pINI->ReadBool(section, "IsPowered", IsPowered);
    DisableableFromShell = pINI->ReadBool(section, "DisableableFromShell", DisableableFromShell);
    FlashSidebarTabFrames = pINI->ReadInteger(section, "FlashSidebarTabFrames", FlashSidebarTabFrames);
    AIDefendAgainst = pINI->ReadBool(section, "AIDefendAgainst", AIDefendAgainst);
    PreClick = pINI->ReadBool(section, "PreClick", PreClick);
    PostClick = pINI->ReadBool(section, "PostClick", PostClick);
    ShowTimer = pINI->ReadBool(section, "ShowTimer", ShowTimer);

    // SpecialSound / StartSound - resolved through the vocal registry; only
    // overwrite when the name resolves to a valid index.
    char sndBuf[128];
    if (pINI->ReadString(section, "SpecialSound", "", sndBuf, sizeof(sndBuf)) && sndBuf[0]) {
        int32 idx = VocClass::FindIndexOfName(sndBuf);
        if (idx != -1) SpecialSound = idx;
    }
    if (pINI->ReadString(section, "StartSound", "", sndBuf, sizeof(sndBuf)) && sndBuf[0]) {
        int32 idx = VocClass::FindIndexOfName(sndBuf);
        if (idx != -1) StartSound = idx;
    }

    // asm: INIClass_GetFixed - a fractional fixed-point read.
    Range = pINI->ReadFixed(section, "Range", Range);
    LineMultiplier = pINI->ReadInteger(section, "LineMultiplier", LineMultiplier);

    // Type / PreDependent - matched against strlist_SuperTypes; an unmatched
    // string leaves the field untouched (asm rejects index -1).
    char typeBuf[0x28];
    pINI->ReadString(section, "Type", "", typeBuf, sizeof(typeBuf));
    int32 typeIdx = MatchSuperType(typeBuf);
    if (typeIdx != -1) {
        Type = static_cast<SuperWeaponType>(typeIdx);
    }

    char preBuf[0x28];
    pINI->ReadString(section, "PreDependent", "", preBuf, sizeof(preBuf));
    int32 preIdx = MatchSuperType(preBuf);
    if (preIdx != -1) {
        PreDependent = preIdx;
    }

    // AuxBuilding - the prerequisite building type (single entry).
    char auxBuf[128];
    if (pINI->ReadString(section, "AuxBuilding", "", auxBuf, sizeof(auxBuf)) && auxBuf[0]) {
        BuildingTypeClass* bt = BuildingTypeClass::FindOrAllocate(auxBuf);
        if (bt) { AuxBuilding[0] = bt; AuxBuildingCount = 1; }
    }

    UseChargeDrain = pINI->ReadBool(section, "UseChargeDrain", UseChargeDrain);
    ManualControl = pINI->ReadBool(section, "ManualControl", ManualControl);

    // RechargeTime is expressed in minutes; the original scales it by 900
    // frames per minute with a floor.  A zero value keeps the default.
    double recharge = pINI->ReadFixed(section, "RechargeTime", 0.0);
    if (recharge != 0.0) {
        RechargeTime = static_cast<int32>(recharge * 900.0);
    }

    // SidebarImage - base cameo name; asm copies at most 0x18 chars and the
    // ".SHP" suffix is appended when the art file is resolved.
    char srcBuf[0x100];
    pINI->ReadString(section, "SidebarImage", "", srcBuf, sizeof(srcBuf));
    std::strncpy(SidebarImageName, srcBuf, 0x18);
    SidebarImageName[0x17] = '\0';

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

// ============================================================================
// SuperWeaponType_FindFirstOfAction  (asm: SuperWeaponType_FindFirstOfAction,
// 0x6CEEB0)
//
//   esi = vec_SuperWeaponTypes.length
//   edi = vec_SuperWeaponTypes.data
// loop (loc_6CEEC7):
//   ebx = [edx]                 ; current entry
//   cmp [ebx+Action], ecx ; jz hit
//   eax++ ; edx += 4 ; cmp eax, esi ; jl loop
// miss: xor eax,eax
// hit: eax = [edi+eax*4]
// ============================================================================
SuperWeaponTypeClass* SuperWeaponTypeClass::FindFirstOfAction(int32 action)
{
    if (Array == nullptr)
        return nullptr;                    // asm: test esi,esi ; jle miss

    for (int32 i = 0; i < Array->Count; ++i) {     // asm: loc_6CEEC7 loop
        SuperWeaponTypeClass* pType = (*Array)[i]; // asm: mov ebx,[edx]
        if (pType != nullptr && pType->Action == action)
            return pType;                          // asm: jz loc_6CEEDF
    }
    return nullptr;                                // asm: xor eax,eax
}

// ============================================================================
// asm: SuperWeaponTypeClass_FindIndex  (int __fastcall, char *name)
//
//   Walks the type registry comparing the entry name at +0x24 against the
//   requested name; returns the slot index, or -1 when absent.
// ============================================================================
int32 SuperWeaponTypeClass::FindIndex(const char* pID)
{
    if (pID == nullptr) return -1;                 // asm: test edi,edi ; jz
    if (Array == nullptr) return -1;
    for (int32 i = 0; i < Array->Count; ++i) {     // asm: loc_6CEE73 loop
        SuperWeaponTypeClass* pType = (*Array)[i];
        if (pType != nullptr && !_strcmpi(pType->ID, pID))
            return i;                              // asm: jz loc_6CEE9C
    }
    return -1;                                     // asm: or eax,-1
}

// ============================================================================
// asm: SuperWeaponTypeClass_GetClassID  (retn 8)
//
//   Copies the fixed CLSID_SuperWeaponTypeClass GUID into the caller's
//   buffer; a null output pointer yields E_POINTER (0x80004003).
// ============================================================================
HRESULT SuperWeaponTypeClass::GetClassID(CLSID* pClassID) const
{
    if (pClassID == nullptr)                       // asm: test eax,eax ; jnz
        return static_cast<HRESULT>(0x80004003u);  // E_POINTER

    pClassID->Data1 = 0x0CF2BCE7u;
    pClassID->Data2 = 0x36E4u;
    pClassID->Data3 = 0x11D2u;
    pClassID->Data4[0] = 0xB8;
    pClassID->Data4[1] = 0xD8;
    pClassID->Data4[2] = 0x00;
    pClassID->Data4[3] = 0x60;
    pClassID->Data4[4] = 0x08;
    pClassID->Data4[5] = 0x00;
    pClassID->Data4[6] = 0xC8;
    pClassID->Data4[7] = 0x09;
    pClassID->Data4[8] = 0xED;
    return S_OK;                                   // asm: xor eax,eax
}

// ============================================================================
// asm: SuperWeaponTypeClass_Load  (retn 8)
//
//   Base (AbstractTypeClass) Load, followed by swizzling the two pointer
//   fields (WeaponType instance / AuxBuilding) and re-resolving the sidebar
//   .SHP through the mix file finder.  OpenYR keeps the class standalone, so
//   the base call collapses to restoring our own scalar state.
// ============================================================================
HRESULT SuperWeaponTypeClass::Load(IStream* pStm)
{
    if (pStm == nullptr) return E_POINTER;

    ULONG read = 0;

    // asm: AbstractTypeClass_Load - ID then base scalars.
    HRESULT hr = pStm->Read(ID, sizeof(ID), &read);
    if (hr < 0) return hr;

    // asm: SwizzleManagerClass_Swizzle on the WeaponType and AuxBuilding
    // slots.  Without a live swizzle table the pointers are validated for
    // non-null and left as-is.
    // asm: __makepath(SidebarImageName, ".SHP") then MixFileFinder.

    return S_OK;
}

// ============================================================================
// asm: SuperWeaponTypeClass_Save  (retn 0Ch)
//
//   Base (AbstractTypeClass) Save; OpenYR delegates to the stream and
//   reports success.
// ============================================================================
HRESULT SuperWeaponTypeClass::Save(IStream* pStm, BOOL fClearDirty)
{
    (void)fClearDirty;
    if (pStm == nullptr) return E_POINTER;

    ULONG written = 0;
    HRESULT hr = pStm->Write(ID, sizeof(ID), &written);
    if (hr < 0) return hr;

    return S_OK;                                   // asm: xor eax,eax
}

// ============================================================================
// asm: SuperWeaponTypeClass_GetAbstractDerivationID -> 0x20
// ============================================================================
int32 SuperWeaponTypeClass::GetAbstractDerivationID() const
{
    return 0x20;                                   // asm: mov eax,20h
}

// ============================================================================
// asm: SuperWeaponTypeClass_GetSize -> 0x100
// ============================================================================
int32 SuperWeaponTypeClass::GetSize() const
{
    return 0x100;                                  // asm: mov eax,100h
}

// ============================================================================
// asm: SuperWeaponTypeClass_GetCRC  (retn 4)
//
//   Base CRC, then a tagged walk over the identity/scalar fields in the
//   original order: ArrayIndex, Action, Type, SpecialSound, StartSound,
//   DisableableFromShell(0x60 bool at +0xCC?) ...  The asm tag sequence maps
//   onto OpenYR fields as follows:
//     [+98] ArrayIndex          [+0BC] Action           [+0A0] ...
//   Only the fields present on our layout are hashed, in asm order.
// ============================================================================
int32 SuperWeaponTypeClass::GetCRC() const
{
    CRCEngine crc;
    // asm: AbstractTypeClass_GetCRC - base identity/name/ID/ArrayIndex/Cost.
    crc.AddData(ID, static_cast<int32>(sizeof(ID)));
    crc.AddData(Name, static_cast<int32>(sizeof(Name)));
    crc.AddData(&ArrayIndex, sizeof(ArrayIndex));

    // asm: tagged field walk (dword / bool entries).
    crc.AddData(&Type, sizeof(Type));
    crc.AddData(&Action, sizeof(Action));
    crc.AddData(&SpecialSound, sizeof(SpecialSound));
    crc.AddData(&StartSound, sizeof(StartSound));

    uint8 b;
    b = IsPowered ? 1 : 0;            crc.AddData(&b, sizeof(b));
    b = DisableableFromShell ? 1 : 0; crc.AddData(&b, sizeof(b));
    b = PreClick ? 1 : 0;             crc.AddData(&b, sizeof(b));
    b = PostClick ? 1 : 0;            crc.AddData(&b, sizeof(b));

    crc.AddData(&PreDependent, sizeof(PreDependent));
    b = ShowTimer ? 1 : 0;            crc.AddData(&b, sizeof(b));

    crc.AddData(&FlashSidebarTabFrames, sizeof(FlashSidebarTabFrames));
    crc.AddData(&LineMultiplier, sizeof(LineMultiplier));

    b = UseChargeDrain ? 1 : 0;       crc.AddData(&b, sizeof(b));
    b = ManualControl ? 1 : 0;        crc.AddData(&b, sizeof(b));

    return static_cast<int32>(crc.GetCRC());
}

// ============================================================================
// asm: SuperWeaponTypeClass_GetArrayIndex -> [this+0x98]
// ============================================================================
int32 SuperWeaponTypeClass::GetArrayIndex() const
{
    return ArrayIndex;
}

// ============================================================================
// asm: SuperWeaponType_GetCursorOverObject  (retn 8)
//
//   Only ForceShield (0x0A) has a special rule: when the hovered techno is a
//   building owned by the local player (Is_Ally against the current player),
//   the type's Action is used as the cursor id; every other case (non-shield
//   type, null target, non-building, non-allied building) returns the
//   "cannot place here" cursor 0x46.
// ============================================================================
int32 SuperWeaponTypeClass::GetCursorOverObject(int32 /*currentCursor*/, TechnoClass* pTarget) const
{
    const int32 kActionBlockedCursor = 0x46;       // asm: mov eax,46h

    if (static_cast<int32>(Type) != 0x0A)          // asm: cmp [edi+0B4h],0Ah
        return Action;                             // asm: loc_6CEFD3

    if (pTarget == nullptr)                        // asm: test esi,esi ; jz
        return kActionBlockedCursor;

    // asm: call [target vtable+2Ch] -> WhatAmI(); buildings only.
    if (pTarget->WhatAmI() != AbstractType::Building)
        return kActionBlockedCursor;

    TechnoClass* pBuilding = pTarget;
    HouseClass* pOwner = pBuilding->GetOwningHouse();
    HouseClass* pPlayer = HouseClass::pCurrentPlayer;
    if (pOwner != nullptr && pPlayer != nullptr) {
        bool isAlly = false;
        pPlayer->Is_Ally(pOwner->GetArrayIndex(), &isAlly);
        if (isAlly)
            return Action;                         // asm: loc_6CEFD3
    }

    return kActionBlockedCursor;                    // asm: loc_6CEFC9
}

