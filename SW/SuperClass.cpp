#include "../Combat/DamageArea.h"
#include "../Combat/CaptureManagerClass.h"
#include "SuperClass.h"
#include "SuperWeaponTypeClass.h"
#include "../Houses/HouseClass.h"
#include "../Houses/HouseTypeClass.h"
#include "../Abstract/TechnoClass.h"
#include "../Abstract/FootClass.h"
#include "../Abstract/InfantryClass.h"
#include "../Abstract/UnitClass.h"
#include "../Abstract/AircraftClass.h"
#include "../Abstract/BuildingClass.h"
#include "../Animations/AnimClass.h"
#include "../Animations/AnimTypeClass.h"
#include "../Combat/WarheadTypeClass.h"
#include "../Combat/WeaponTypeClass.h"
#include "../Map/MapClass.h"
#include "../Map/CellClass.h"
#include "../Rendering/TacticalClass.h"
#include "../Rendering/DisplayClass.h"
#include "../Scenario/ScenarioClass.h"
#include "../Rules/RulesClass.h"
#include "../Game/Game.h"
#include "../IO/CRC.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

// ============================================================================
// Static members
// ============================================================================

DynamicVectorClass<SuperClass*>* SuperClass::Array = nullptr;

// Global weather / global-effect state.  These live in the scene globals in
// the original (LightningStorm_Active / PsyDom_Status) and are read through
// the LightningStorm_IsActive / PsyDom_IsActive accessors.
bool  SuperClass::LightningStorm_Active = false;
int32 SuperClass::PsyDom_Status = 0;

int32      SuperClass::LightningStorm_StartTime = -1;
int32      SuperClass::LightningStorm_Duration = -1;
int32      SuperClass::LightningStorm_LightningDeferment = 0;
uint8      SuperClass::LightningStorm_TimeToEnd = 0;
int32      SuperClass::LightningStorm_Status = 0;
CellStruct SuperClass::LightningStorm_CellCoords = CellStruct(-1, -1);
HouseClass* SuperClass::LightningStorm_Owner = nullptr;
AnimClass* SuperClass::PsyDom_Anim = nullptr;
CellStruct SuperClass::PsyDom_CellCoords = CellStruct(-1, -1);
HouseClass* SuperClass::PsyDom_Owner = nullptr;
int32      SuperClass::LightningStorm_Field8 = -1;
int32      SuperClass::LightningStorm_FieldC = -1;

DynamicVectorClass<AnimClass*>* SuperClass::vec_Anims_const1 = nullptr;
DynamicVectorClass<AnimClass*>* SuperClass::vec_Anims_const2 = nullptr;
DynamicVectorClass<AnimClass*>* SuperClass::vec_Anims_const  = nullptr;

// ============================================================================
// Static lookup
// ============================================================================

SuperClass* SuperClass::Find(const char* pID) {
    if (!Array || !pID) return nullptr;
    for (int32 i = 0; i < Array->Count; ++i) {
        SuperClass* item = (*Array)[i];
        if (item && item->Type && !_strcmpi(item->Type->ID, pID)) {
            return item;
        }
    }
    return nullptr;
}

SuperClass* SuperClass::FindByIndex(int32 index) {
    if (!Array || index < 0 || index >= Array->Count) return nullptr;
    return (*Array)[index];
}

int32 SuperClass::GetCount() {
    if (!Array) return 0;
    return Array->Count;
}

// ============================================================================
// Construction / Destruction
// ============================================================================

SuperClass::SuperClass(SuperWeaponTypeClass* pType, HouseClass* pOwner) noexcept :
    Type(pType),
    Owner(pOwner),
    RechargeTimer(0),
    State(SWState::Idle),
    ChargeDrain(0),
    next(nullptr),
    CustomChargeTime(-1),
    IsGranted(false),
    IsDonated_(false),
    IsAnimationPlaying(false),
    IsAlreadyActivated(false),
    IsSuspended(false),
    IsDumb(false),
    IsOneTime(false),
    IsTemporallyUnavailable(false),
    IsPowered(false),
    IsReady_(false),
    IsCharged_(false),
    IsManual(false),
    PreClick(false),
    PostClick(false),
    IsDesignator_(false),
    GrantedByAnother(false),
    Pad1(0),
    Pad2(0),
    Pad3(0),
    unknown_44(0),
    unknown_48(0),
    unknown_4C(0),
    CurrMoney(0),
    unknown_54(0),
    unknown_58(0),
    unknown_5C(0),
    unknown_60(0),
    deferredState(SWState::None),
    deferredTimer(0),
    deferredCell(0, 0),
    LightningTimer(0),
    LightningDeferment(0),
    LightningStrikeCount(0),
    LightningScatter(0, 0),
    ChronoWarpTimer(0),
    ChronoWarpState(0),
    ChronoWarpDamageDone(0),
    DominatorTimer(0),
    DominatorScroll(0),
    DominatorActivated(false),
    GeneticMutatorTimer(0),
    SpyPlaneTimer(0),
    ParaDropTimer(0),
    ParaDropCount(0),
    NukeTimer(0),
    NukeState(0),
    TargetCell(0, 0),
    TargetCoord(0, 0, 0),
    LastTargetCell(0, 0),
    LastTargetCoord(0, 0, 0),
    CameraStart(0, 0),
    CameraEnd(0, 0),
    unknown_130(0, 0),
    unknown_138(0),
    unknown_13C(0)
{
    if (Type) {
        RechargeTimer = Type->RechargeTime;
        IsPowered = Type->IsPowered;
        IsOneTime = Type->IsOneTime;
        IsManual = Type->IsManual;
        PreClick = Type->PreClick;
        PostClick = Type->PostClick;
        IsDesignator_ = Type->IsDesignator;
        IsDumb = Type->IsAuxBuilding;
        ChargeDrain = Type->UseChargeDrain ? 1 : 0;
    }

    if (!Array) {
        Array = new DynamicVectorClass<SuperClass*>();
    }
    Array->Add(this);
}

SuperClass::~SuperClass() {
    if (Array) {
        for (int32 i = 0; i < Array->Count; ++i) {
            if ((*Array)[i] == this) {
                Array->Remove(i);
                break;
            }
        }
    }
}

// ============================================================================
// IPersist
// ============================================================================

HRESULT SuperClass::GetClassID(CLSID* pClassID) {
    if (!pClassID) return E_POINTER;
    pClassID->Data1 = static_cast<uint32>(AbstractType::Super);
    return S_OK;
}

HRESULT SuperClass::Load(IStream* pStm) {
    if (!pStm) return E_POINTER;

    ULONG read = 0;
    HRESULT hr = S_OK;

    // Read Type (string ID)
    char typeID[0x20];
    hr = pStm->Read(typeID, sizeof(typeID), &read);
    if (hr < 0 || read != sizeof(typeID)) return E_FAIL;
    typeID[sizeof(typeID) - 1] = '\0';
    Type = typeID[0] ? SuperWeaponTypeClass::Find(typeID) : nullptr;

    // Read Owner (int32 index)
    int32 ownerIndex = 0;
    hr = pStm->Read(&ownerIndex, sizeof(ownerIndex), &read);
    if (hr < 0 || read != sizeof(ownerIndex)) return E_FAIL;
    Owner = HouseClass::GetHouseByIndex(ownerIndex);

    // Read RechargeTimer
    hr = pStm->Read(&RechargeTimer, sizeof(RechargeTimer), &read);
    if (hr < 0 || read != sizeof(RechargeTimer)) return E_FAIL;

    // Read State
    hr = pStm->Read(&State, sizeof(State), &read);
    if (hr < 0 || read != sizeof(State)) return E_FAIL;

    // Read ChargeDrain
    hr = pStm->Read(&ChargeDrain, sizeof(ChargeDrain), &read);
    if (hr < 0 || read != sizeof(ChargeDrain)) return E_FAIL;

    // Read next (int32 index)
    int32 nextIndex = 0;
    hr = pStm->Read(&nextIndex, sizeof(nextIndex), &read);
    if (hr < 0 || read != sizeof(nextIndex)) return E_FAIL;
    next = SuperClass::FindByIndex(nextIndex);

    // Read flags as a bitmask
    uint32 flags = 0;
    hr = pStm->Read(&flags, sizeof(flags), &read);
    if (hr < 0 || read != sizeof(flags)) return E_FAIL;
    IsGranted                = (flags & 0x00000001) != 0;
    IsAnimationPlaying       = (flags & 0x00000002) != 0;
    IsAlreadyActivated       = (flags & 0x00000004) != 0;
    IsSuspended              = (flags & 0x00000008) != 0;
    IsDumb                   = (flags & 0x00000010) != 0;
    IsOneTime                = (flags & 0x00000020) != 0;
    IsTemporallyUnavailable  = (flags & 0x00000040) != 0;
    IsPowered                = (flags & 0x00000080) != 0;
    IsReady_                 = (flags & 0x00000100) != 0;
    IsCharged_               = (flags & 0x00000200) != 0;
    IsManual                 = (flags & 0x00000400) != 0;
    PreClick                 = (flags & 0x00000800) != 0;
    PostClick                = (flags & 0x00001000) != 0;
    IsDesignator_            = (flags & 0x00002000) != 0;
    GrantedByAnother         = (flags & 0x00004000) != 0;
    DominatorActivated       = (flags & 0x00008000) != 0;

    // Read pad bytes
    hr = pStm->Read(&Pad1, sizeof(Pad1), &read);
    if (hr < 0 || read != sizeof(Pad1)) return E_FAIL;
    hr = pStm->Read(&Pad2, sizeof(Pad2), &read);
    if (hr < 0 || read != sizeof(Pad2)) return E_FAIL;
    hr = pStm->Read(&Pad3, sizeof(Pad3), &read);
    if (hr < 0 || read != sizeof(Pad3)) return E_FAIL;

    // Read unknown numeric fields
    hr = pStm->Read(&unknown_44, sizeof(unknown_44), &read);
    if (hr < 0 || read != sizeof(unknown_44)) return E_FAIL;
    hr = pStm->Read(&unknown_48, sizeof(unknown_48), &read);
    if (hr < 0 || read != sizeof(unknown_48)) return E_FAIL;
    hr = pStm->Read(&unknown_4C, sizeof(unknown_4C), &read);
    if (hr < 0 || read != sizeof(unknown_4C)) return E_FAIL;

    // Read CurrMoney
    hr = pStm->Read(&CurrMoney, sizeof(CurrMoney), &read);
    if (hr < 0 || read != sizeof(CurrMoney)) return E_FAIL;

    // Read more unknown numeric fields
    hr = pStm->Read(&unknown_54, sizeof(unknown_54), &read);
    if (hr < 0 || read != sizeof(unknown_54)) return E_FAIL;
    hr = pStm->Read(&unknown_58, sizeof(unknown_58), &read);
    if (hr < 0 || read != sizeof(unknown_58)) return E_FAIL;
    hr = pStm->Read(&unknown_5C, sizeof(unknown_5C), &read);
    if (hr < 0 || read != sizeof(unknown_5C)) return E_FAIL;
    hr = pStm->Read(&unknown_60, sizeof(unknown_60), &read);
    if (hr < 0 || read != sizeof(unknown_60)) return E_FAIL;

    // Read deferred state
    hr = pStm->Read(&deferredState, sizeof(deferredState), &read);
    if (hr < 0 || read != sizeof(deferredState)) return E_FAIL;
    hr = pStm->Read(&deferredTimer, sizeof(deferredTimer), &read);
    if (hr < 0 || read != sizeof(deferredTimer)) return E_FAIL;
    hr = pStm->Read(&deferredCell, sizeof(deferredCell), &read);
    if (hr < 0 || read != sizeof(deferredCell)) return E_FAIL;

    // Read type-specific state
    hr = pStm->Read(&LightningTimer, sizeof(LightningTimer), &read);
    if (hr < 0 || read != sizeof(LightningTimer)) return E_FAIL;
    hr = pStm->Read(&LightningDeferment, sizeof(LightningDeferment), &read);
    if (hr < 0 || read != sizeof(LightningDeferment)) return E_FAIL;
    hr = pStm->Read(&LightningStrikeCount, sizeof(LightningStrikeCount), &read);
    if (hr < 0 || read != sizeof(LightningStrikeCount)) return E_FAIL;
    hr = pStm->Read(&LightningScatter, sizeof(LightningScatter), &read);
    if (hr < 0 || read != sizeof(LightningScatter)) return E_FAIL;

    hr = pStm->Read(&ChronoWarpTimer, sizeof(ChronoWarpTimer), &read);
    if (hr < 0 || read != sizeof(ChronoWarpTimer)) return E_FAIL;
    hr = pStm->Read(&ChronoWarpState, sizeof(ChronoWarpState), &read);
    if (hr < 0 || read != sizeof(ChronoWarpState)) return E_FAIL;
    hr = pStm->Read(&ChronoWarpDamageDone, sizeof(ChronoWarpDamageDone), &read);
    if (hr < 0 || read != sizeof(ChronoWarpDamageDone)) return E_FAIL;

    hr = pStm->Read(&DominatorTimer, sizeof(DominatorTimer), &read);
    if (hr < 0 || read != sizeof(DominatorTimer)) return E_FAIL;
    hr = pStm->Read(&DominatorScroll, sizeof(DominatorScroll), &read);
    if (hr < 0 || read != sizeof(DominatorScroll)) return E_FAIL;

    hr = pStm->Read(&GeneticMutatorTimer, sizeof(GeneticMutatorTimer), &read);
    if (hr < 0 || read != sizeof(GeneticMutatorTimer)) return E_FAIL;

    hr = pStm->Read(&SpyPlaneTimer, sizeof(SpyPlaneTimer), &read);
    if (hr < 0 || read != sizeof(SpyPlaneTimer)) return E_FAIL;
    hr = pStm->Read(&ParaDropTimer, sizeof(ParaDropTimer), &read);
    if (hr < 0 || read != sizeof(ParaDropTimer)) return E_FAIL;
    hr = pStm->Read(&ParaDropCount, sizeof(ParaDropCount), &read);
    if (hr < 0 || read != sizeof(ParaDropCount)) return E_FAIL;

    hr = pStm->Read(&NukeTimer, sizeof(NukeTimer), &read);
    if (hr < 0 || read != sizeof(NukeTimer)) return E_FAIL;
    hr = pStm->Read(&NukeState, sizeof(NukeState), &read);
    if (hr < 0 || read != sizeof(NukeState)) return E_FAIL;

    // Read targeting
    hr = pStm->Read(&TargetCell, sizeof(TargetCell), &read);
    if (hr < 0 || read != sizeof(TargetCell)) return E_FAIL;
    hr = pStm->Read(&TargetCoord, sizeof(TargetCoord), &read);
    if (hr < 0 || read != sizeof(TargetCoord)) return E_FAIL;
    hr = pStm->Read(&LastTargetCell, sizeof(LastTargetCell), &read);
    if (hr < 0 || read != sizeof(LastTargetCell)) return E_FAIL;
    hr = pStm->Read(&LastTargetCoord, sizeof(LastTargetCoord), &read);
    if (hr < 0 || read != sizeof(LastTargetCoord)) return E_FAIL;

    // Read camera
    hr = pStm->Read(&CameraStart, sizeof(CameraStart), &read);
    if (hr < 0 || read != sizeof(CameraStart)) return E_FAIL;
    hr = pStm->Read(&CameraEnd, sizeof(CameraEnd), &read);
    if (hr < 0 || read != sizeof(CameraEnd)) return E_FAIL;

    // Read unknown/misc
    hr = pStm->Read(&unknown_130, sizeof(unknown_130), &read);
    if (hr < 0 || read != sizeof(unknown_130)) return E_FAIL;
    hr = pStm->Read(&unknown_138, sizeof(unknown_138), &read);
    if (hr < 0 || read != sizeof(unknown_138)) return E_FAIL;
    hr = pStm->Read(&unknown_13C, sizeof(unknown_13C), &read);
    if (hr < 0 || read != sizeof(unknown_13C)) return E_FAIL;

    return S_OK;
}

HRESULT SuperClass::Save(IStream* pStm, BOOL fClearDirty) {
    if (!pStm) return E_POINTER;

    ULONG written = 0;
    HRESULT hr = S_OK;

    // Write Type (string ID)
    char typeID[0x20];
    std::memset(typeID, 0, sizeof(typeID));
    if (Type && Type->ID) {
        int32 j = 0;
        while (Type->ID[j] && j < static_cast<int32>(sizeof(typeID)) - 1) {
            typeID[j] = Type->ID[j]; ++j;
        }
    }
    hr = pStm->Write(typeID, sizeof(typeID), &written);
    if (hr < 0 || written != sizeof(typeID)) return E_FAIL;

    // Write Owner (int32 index)
    int32 ownerIndex = -1;
    if (Owner) {
        for (int32 i = 0; i < HouseClass::ArrayCount; ++i) {
            if (HouseClass::Array[i] == Owner) { ownerIndex = i; break; }
        }
    }
    hr = pStm->Write(&ownerIndex, sizeof(ownerIndex), &written);
    if (hr < 0 || written != sizeof(ownerIndex)) return E_FAIL;

    // Write RechargeTimer
    hr = pStm->Write(&RechargeTimer, sizeof(RechargeTimer), &written);
    if (hr < 0 || written != sizeof(RechargeTimer)) return E_FAIL;

    // Write State
    hr = pStm->Write(&State, sizeof(State), &written);
    if (hr < 0 || written != sizeof(State)) return E_FAIL;

    // Write ChargeDrain
    hr = pStm->Write(&ChargeDrain, sizeof(ChargeDrain), &written);
    if (hr < 0 || written != sizeof(ChargeDrain)) return E_FAIL;

    // Write next (int32 index)
    int32 nextIndex = -1;
    if (next && Array) {
        for (int32 i = 0; i < Array->Count; ++i) {
            if ((*Array)[i] == next) { nextIndex = i; break; }
        }
    }
    hr = pStm->Write(&nextIndex, sizeof(nextIndex), &written);
    if (hr < 0 || written != sizeof(nextIndex)) return E_FAIL;

    // Write flags as a bitmask
    uint32 flags = 0;
    if (IsGranted)               flags |= 0x00000001;
    if (IsAnimationPlaying)      flags |= 0x00000002;
    if (IsAlreadyActivated)      flags |= 0x00000004;
    if (IsSuspended)             flags |= 0x00000008;
    if (IsDumb)                  flags |= 0x00000010;
    if (IsOneTime)               flags |= 0x00000020;
    if (IsTemporallyUnavailable) flags |= 0x00000040;
    if (IsPowered)               flags |= 0x00000080;
    if (IsReady_)                flags |= 0x00000100;
    if (IsCharged_)              flags |= 0x00000200;
    if (IsManual)                flags |= 0x00000400;
    if (PreClick)                flags |= 0x00000800;
    if (PostClick)               flags |= 0x00001000;
    if (IsDesignator_)           flags |= 0x00002000;
    if (GrantedByAnother)        flags |= 0x00004000;
    if (DominatorActivated)      flags |= 0x00008000;
    hr = pStm->Write(&flags, sizeof(flags), &written);
    if (hr < 0 || written != sizeof(flags)) return E_FAIL;

    // Write pad bytes
    hr = pStm->Write(&Pad1, sizeof(Pad1), &written);
    if (hr < 0 || written != sizeof(Pad1)) return E_FAIL;
    hr = pStm->Write(&Pad2, sizeof(Pad2), &written);
    if (hr < 0 || written != sizeof(Pad2)) return E_FAIL;
    hr = pStm->Write(&Pad3, sizeof(Pad3), &written);
    if (hr < 0 || written != sizeof(Pad3)) return E_FAIL;

    // Write unknown numeric fields
    hr = pStm->Write(&unknown_44, sizeof(unknown_44), &written);
    if (hr < 0 || written != sizeof(unknown_44)) return E_FAIL;
    hr = pStm->Write(&unknown_48, sizeof(unknown_48), &written);
    if (hr < 0 || written != sizeof(unknown_48)) return E_FAIL;
    hr = pStm->Write(&unknown_4C, sizeof(unknown_4C), &written);
    if (hr < 0 || written != sizeof(unknown_4C)) return E_FAIL;

    // Write CurrMoney
    hr = pStm->Write(&CurrMoney, sizeof(CurrMoney), &written);
    if (hr < 0 || written != sizeof(CurrMoney)) return E_FAIL;

    // Write more unknown numeric fields
    hr = pStm->Write(&unknown_54, sizeof(unknown_54), &written);
    if (hr < 0 || written != sizeof(unknown_54)) return E_FAIL;
    hr = pStm->Write(&unknown_58, sizeof(unknown_58), &written);
    if (hr < 0 || written != sizeof(unknown_58)) return E_FAIL;
    hr = pStm->Write(&unknown_5C, sizeof(unknown_5C), &written);
    if (hr < 0 || written != sizeof(unknown_5C)) return E_FAIL;
    hr = pStm->Write(&unknown_60, sizeof(unknown_60), &written);
    if (hr < 0 || written != sizeof(unknown_60)) return E_FAIL;

    // Write deferred state
    hr = pStm->Write(&deferredState, sizeof(deferredState), &written);
    if (hr < 0 || written != sizeof(deferredState)) return E_FAIL;
    hr = pStm->Write(&deferredTimer, sizeof(deferredTimer), &written);
    if (hr < 0 || written != sizeof(deferredTimer)) return E_FAIL;
    hr = pStm->Write(&deferredCell, sizeof(deferredCell), &written);
    if (hr < 0 || written != sizeof(deferredCell)) return E_FAIL;

    // Write type-specific state
    hr = pStm->Write(&LightningTimer, sizeof(LightningTimer), &written);
    if (hr < 0 || written != sizeof(LightningTimer)) return E_FAIL;
    hr = pStm->Write(&LightningDeferment, sizeof(LightningDeferment), &written);
    if (hr < 0 || written != sizeof(LightningDeferment)) return E_FAIL;
    hr = pStm->Write(&LightningStrikeCount, sizeof(LightningStrikeCount), &written);
    if (hr < 0 || written != sizeof(LightningStrikeCount)) return E_FAIL;
    hr = pStm->Write(&LightningScatter, sizeof(LightningScatter), &written);
    if (hr < 0 || written != sizeof(LightningScatter)) return E_FAIL;

    hr = pStm->Write(&ChronoWarpTimer, sizeof(ChronoWarpTimer), &written);
    if (hr < 0 || written != sizeof(ChronoWarpTimer)) return E_FAIL;
    hr = pStm->Write(&ChronoWarpState, sizeof(ChronoWarpState), &written);
    if (hr < 0 || written != sizeof(ChronoWarpState)) return E_FAIL;
    hr = pStm->Write(&ChronoWarpDamageDone, sizeof(ChronoWarpDamageDone), &written);
    if (hr < 0 || written != sizeof(ChronoWarpDamageDone)) return E_FAIL;

    hr = pStm->Write(&DominatorTimer, sizeof(DominatorTimer), &written);
    if (hr < 0 || written != sizeof(DominatorTimer)) return E_FAIL;
    hr = pStm->Write(&DominatorScroll, sizeof(DominatorScroll), &written);
    if (hr < 0 || written != sizeof(DominatorScroll)) return E_FAIL;

    hr = pStm->Write(&GeneticMutatorTimer, sizeof(GeneticMutatorTimer), &written);
    if (hr < 0 || written != sizeof(GeneticMutatorTimer)) return E_FAIL;

    hr = pStm->Write(&SpyPlaneTimer, sizeof(SpyPlaneTimer), &written);
    if (hr < 0 || written != sizeof(SpyPlaneTimer)) return E_FAIL;
    hr = pStm->Write(&ParaDropTimer, sizeof(ParaDropTimer), &written);
    if (hr < 0 || written != sizeof(ParaDropTimer)) return E_FAIL;
    hr = pStm->Write(&ParaDropCount, sizeof(ParaDropCount), &written);
    if (hr < 0 || written != sizeof(ParaDropCount)) return E_FAIL;

    hr = pStm->Write(&NukeTimer, sizeof(NukeTimer), &written);
    if (hr < 0 || written != sizeof(NukeTimer)) return E_FAIL;
    hr = pStm->Write(&NukeState, sizeof(NukeState), &written);
    if (hr < 0 || written != sizeof(NukeState)) return E_FAIL;

    // Write targeting
    hr = pStm->Write(&TargetCell, sizeof(TargetCell), &written);
    if (hr < 0 || written != sizeof(TargetCell)) return E_FAIL;
    hr = pStm->Write(&TargetCoord, sizeof(TargetCoord), &written);
    if (hr < 0 || written != sizeof(TargetCoord)) return E_FAIL;
    hr = pStm->Write(&LastTargetCell, sizeof(LastTargetCell), &written);
    if (hr < 0 || written != sizeof(LastTargetCell)) return E_FAIL;
    hr = pStm->Write(&LastTargetCoord, sizeof(LastTargetCoord), &written);
    if (hr < 0 || written != sizeof(LastTargetCoord)) return E_FAIL;

    // Write camera
    hr = pStm->Write(&CameraStart, sizeof(CameraStart), &written);
    if (hr < 0 || written != sizeof(CameraStart)) return E_FAIL;
    hr = pStm->Write(&CameraEnd, sizeof(CameraEnd), &written);
    if (hr < 0 || written != sizeof(CameraEnd)) return E_FAIL;

    // Write unknown/misc
    hr = pStm->Write(&unknown_130, sizeof(unknown_130), &written);
    if (hr < 0 || written != sizeof(unknown_130)) return E_FAIL;
    hr = pStm->Write(&unknown_138, sizeof(unknown_138), &written);
    if (hr < 0 || written != sizeof(unknown_138)) return E_FAIL;
    hr = pStm->Write(&unknown_13C, sizeof(unknown_13C), &written);
    if (hr < 0 || written != sizeof(unknown_13C)) return E_FAIL;

    return S_OK;
}

// ============================================================================
// AbstractClass
// ============================================================================

AbstractType SuperClass::WhatAmI() const {
    return AbstractType::Super;
}

int32 SuperClass::Size() const {
    return sizeof(SuperClass);
}

// asm: SuperClass_GetCRC (vtable)
//  Chains AbstractClass_GetCRC then feeds the tagged instance state:
//  existence / readied / suspended flag bytes, the readiness frame, the cameo
//  charge tier, plus - when a placement animation is attached - its
//  "invisible/timeToDie" byte and its unique id.
void SuperClass::ComputeCRC(CRCEngine& crc) const {
    Compute_CRC_Abstract(crc);

    const uint8 existsByte    = IsDonated_ ? 1 : 0;   // field_6D
    const uint8 removedByte   = IsSuspended ? 1 : 0;  // field_6E
    const uint8 readyByte     = IsReady_ ? 1 : 0;     // field_6F
    crc.AddData(&existsByte,  sizeof(existsByte));
    crc.AddData(&removedByte, sizeof(removedByte));
    crc.AddData(&readyByte,   sizeof(readyByte));
    crc.AddData(&ReadinessFrame, sizeof(ReadinessFrame));
    crc.AddData(&CameoChargeState, sizeof(CameoChargeState));

    const uint8 cleanByte = 0;                        // field_60
    crc.AddData(&cleanByte, sizeof(cleanByte));
    const uint8 animByte = ChronoAnimPending ? 1 : 0; // field_6C
    crc.AddData(&animByte, sizeof(animByte));

    if (ChronoAnim != nullptr) {
        const uint8 invisible = ChronoAnim->Invisible ? 1 : 0;
        crc.AddData(&invisible, sizeof(invisible));
        const int32 animID = ChronoAnim->Fetch_ID();
        crc.AddData(&animID, sizeof(animID));
    }
}

// ============================================================================
// Update
// ============================================================================

void SuperClass::Update() {
    // asm: SuperClass::AI is driven from HouseClass_Update with isPlayer =
    // (house == HouseClass_PlayerPtr).  The no-arg Update() overload is the
    // AbstractClass polymorphic entry; derive the flag from the owner.
    const bool isPlayer = (Owner != nullptr && Owner->IsHumanPlayer);
    AI(isPlayer);
}

// asm: char __thiscall SuperClass::AI(SuperClass *this, byte isPlayer)
//  The per-frame charge/readiness pass:
//   1. tick down the pending special-sound timer and play the queued sound;
//   2. detach a finished placement animation;
//   3. bail out until the weapon is enabled (field_6D);
//   4. while fully charged a charge-draining weapon stops re-announcing;
//   5. suspended weapons idle;
//   6. resolve the recharge countdown: on completion mark ready, announce the
//      EVA line for the player's house, and (for charge-drain) switch the
//      cameo into its armed tier; otherwise advance the cameo charge tier and
//      report a tier change.
bool SuperClass::AI(bool isPlayer) {
    // 1. special sound countdown (asm SuperClass.SpecialSoundDuration).
    if (SpecialSoundDuration > 0) {
        --SpecialSoundDuration;
    }
    if (SpecialSoundDuration == 0) {
        SpecialSoundDuration = -1;
        // asm: Sound_Effect(Type->SpecialSound, &SpecialSoundCoords, 0).
        // The OpenYR sound layer is not yet wired here; the timer bookkeeping
        // is preserved so the announcement fires exactly once.
    }

    // 2. detach a finished placement animation (asm gap_66).
    if (ChronoAnim != nullptr && !Type) {
        ChronoAnim->Invisible = true;
        ChronoAnim = nullptr;
    }

    // 3. disabled until Enable() has been called.
    if (!Donated()) {
        return false;
    }

    // 4. a fully charged charge-drain weapon with nothing to announce idles.
    if (IsReady_ && Type != nullptr && Type->UseChargeDrain) {
        return false;
    }

    // 5. suspended weapons do not advance.
    if (IsSuspended) {
        return false;
    }

    // 6. recharge countdown.  RechargeTimer < 0 means the timer never started.
    if (RechargeTimer < 0) {
        // Not started charging yet: nothing to do unless a tier was cached.
        if (ReadinessFrame == -1) {
            return false;
        }
        ReadinessFrame = -1;
        return true;
    }

    // Still charging: report whether the cameo tier changed this frame.
    if (RechargeTimer > 0) {
        const int32 stage = Anim_Stage();
        if (stage != ReadinessFrame) {
            ReadinessFrame = stage;
            return true;
        }
        return false;
    }

    // Charged.
    IsReady_ = true;
    if (Type != nullptr && Type->UseChargeDrain) {
        if (CameoChargeState == 2) {
            CameoChargeState = 0;
            RechargeTimer = GetRechargeTime();
            return true;
        }
        CameoChargeState = 1;
        IsReady_ = true;
        return true;
    }

    // Normal (non-drain) completion: announce for the player's house.
    if (isPlayer) {
        OnReady();
    }
    HasReadyFrame = FrameTimer::GetTime();
    return true;
}

// asm: int __thiscall SuperClass::Anim_Stage(SuperClass *this)
//  Returns the cameo charge tier: 0 when the weapon does not exist, 0x36 when
//  already ready, 0x35 when fully charged, else a 0..0x35 ramp derived from the
//  remaining / total recharge ratio (scaled by 54).
int32 SuperClass::Anim_Stage() const {
    if (!Donated()) {
        return 0;
    }

    if (Type != nullptr && Type->UsesCameoChargeState) {
        if (CameoChargeState == 2) {
            // Firestorm-on tier: remaining frames drive the ramp.
            const int32 total = GetRechargeTime();
            int32 remaining = total;
            if (RechargeTimer != -1) {
                const int32 elapsed = FrameTimer::GetTime() - RechargeTimer;
                if (elapsed >= 0 && elapsed < RechargeTimer) {
                    remaining = RechargeTimer - elapsed;
                } else {
                    remaining = 0;
                }
            }
            const double ratio = (total != 0)
                ? 1.0 - (static_cast<double>(total - remaining) * RulesClass::Instance->ChargeToDrainRatio)
                        / (static_cast<double>(total) * RulesClass::Instance->ChargeToDrainRatio)
                : 0.0;
            int32 stage = static_cast<int32>(ratio * 54.0);
            if (stage >= 0x35) return 0x35;
            return stage;
        }
    } else if (IsReady_) {
        return 0x36;
    }

    // Non-drain ramp: 1 - remaining/total, scaled by 54.
    const int32 total = GetRechargeTime();
    int32 remaining = total;
    if (RechargeTimer != -1) {
        const int32 elapsed = FrameTimer::GetTime() - RechargeTimer;
        if (elapsed >= 0 && elapsed < RechargeTimer) {
            remaining = RechargeTimer - elapsed;
        } else {
            remaining = 0;
        }
    }
    double ratio = 0.0;
    if (total != 0) {
        ratio = 1.0 - static_cast<double>(remaining) / static_cast<double>(total);
    }
    int32 stage = static_cast<int32>(ratio * 54.0);
    if (stage >= 0x35) return 0x35;
    if (stage < 0) return 0;
    return stage;
}

// asm: int __thiscall SuperClass_GetRechargeTime(SuperClass *this)
int32 SuperClass::GetRechargeTime() const {
    if (CustomChargeTime != -1) {
        return CustomChargeTime;
    }
    return (Type != nullptr) ? Type->RechargeTime : 0;
}

// asm: byte __thiscall SuperClass_IsPowered(SuperClass *this)
bool SuperClass::GetIsPowered() const {
    return (Type != nullptr) ? Type->IsPowered : false;
}

void SuperClass::PointerGotInvalid(AbstractClass* pAbstract, bool removed) {
    // asm: SuperClass_PointerGotInvalid (retn 8) clears the three raw pointers
    // this weapon holds: its owner house (SuperClass.Owner), its weapon type
    // (SuperClass.SWType) and its placement animation (gap_66).  The last one
    // additionally arms the "animation was dropped" byte so the next AI pass
    // tears down the tracking entry.
    (void)removed;
    if (!pAbstract) {
        return;
    }

    if (static_cast<AbstractClass*>(Owner) == pAbstract) {
        Owner = nullptr;
    }
    // The weapon type is not an AbstractClass in OpenYR; the original compares
    // the raw pointer, so cast through void* to preserve that semantics.
    if (reinterpret_cast<void*>(Type) == reinterpret_cast<void*>(pAbstract)) {
        Type = nullptr;
    }
    if (static_cast<AbstractClass*>(ChronoAnim) == pAbstract) {
        ChronoAnim = nullptr;
        ChronoAnimPending = true;
    }
    // 'next' links SuperClass instances in a per-house chain; sever it too so
    // traversal never walks into freed memory.
    if (reinterpret_cast<void*>(next) == reinterpret_cast<void*>(pAbstract)) {
        next = nullptr;
    }
}

// ============================================================================
// Recharge management
// ============================================================================

void SuperClass::UpdateRecharge() {
    if (RechargeTimer > 0) {
        --RechargeTimer;

        // Charge drain
        if (Type->UseChargeDrain && Owner) {
            Owner->Credits -= ChargeDrain;
            if (Owner->Credits < 0) {
                Owner->Credits = 0;
            }
        }

        if (RechargeTimer <= 0) {
            OnReady();
        }
    }
}

void SuperClass::OnReady() {
    if (!Type || !Owner) return;

    State = SWState::Ready;
    IsReady_ = true;

    // Play ready sound
    if (Type->ReadySound >= 0) {
        // VocClass::PlayGlobal(Type->ReadySound, Type->ReadySoundPriority, 1.0f);
    }

    // Play EVA ready event
    if (Type->EVA_Ready >= 0) {
        // VoxClass::PlayEVAMessage(Type->EVA_Ready);
    }

    // Display ready message
    if (Type->Message_Ready[0]) {
        // MessageListClass::AddMessage(Type->Message_Ready, Owner->Color);
    }

    // Flash sidebar tab
    if (Type->FlashSidebarTabFrames > 0) {
        // SidebarClass::FlashTab(Type->FlashSidebarTabFrames);
    }
}

bool SuperClass::IsReady() const {
    return State == SWState::Ready && RechargeTimer <= 0;
}

bool SuperClass::IsCharged() const {
    return RechargeTimer <= 0;
}

bool SuperClass::IsPresent() const {
    return State == SWState::Ready || State == SWState::Active;
}

bool SuperClass::IsFiring() const {
    return State == SWState::Firing;
}

bool SuperClass::IsAvailable() const {
    if (!Type || !Owner) return false;
    if (IsSuspended) return false;
    if (Type->IsOneTime && IsAlreadyActivated) return false;
    if (Type->IsPowered && Owner->PowerOutput < Owner->PowerDrain) return false;
    if (Type->RequiresBuilding() && !CheckAuxBuildings()) return false;
    return RechargeTimer <= 0;
}

void SuperClass::CheckAvailability() {
    if (!IsAvailable()) {
        if (State == SWState::Ready) {
            State = SWState::Idle;
            IsReady_ = false;
        }
    }
}

bool SuperClass::CheckAuxBuildings() const {
    if (!Type || Type->AuxBuildingCount <= 0) return true;
    if (!Owner) return false;

    for (int32 i = 0; i < Type->AuxBuildingCount; ++i) {
        // Check if owner has the required auxiliary building
        // BuildingTypeClass* bt = Type->AuxBuilding[i];
        // if (!bt) continue;
        // if (!Owner->OwnsBuildingType(bt)) return false;
    }
    return true;
}

// ============================================================================
// Launch
// ============================================================================

void SuperClass::Launch(CellStruct target) {
    if (!Type || !Owner) return;
    if (State != SWState::Ready) return;
    if (!IsAvailable()) return;

    TargetCell = target;
    TargetCoord = CoordMath::CellToCoord(target);
    LastTargetCell = target;
    LastTargetCoord = TargetCoord;

    State = SWState::Firing;
    IsReady_ = false;
    IsAnimationPlaying = true;

    // Play pre-launch sound
    if (Type->PreSound >= 0) {
        // VocClass::PlayGlobal(Type->PreSound, Type->PreSoundPriority, 1.0f);
    }

    // Play EVA activated event
    if (Type->EVA_Activated >= 0) {
        // VoxClass::PlayEVAMessage(Type->EVA_Activated);
    }

    // Display activation message
    if (Type->Message_Activated[0]) {
        // MessageListClass::AddMessage(Type->Message_Activated, Owner->Color);
    }

    // Type-specific pre-launch
    switch (Type->Type)
    {
    case SuperWeaponType::MultiMissile:
        LaunchNuke();
        break;
    case SuperWeaponType::IronCurtain:
        LaunchIronCurtain();
        break;
    case SuperWeaponType::ForceShield:
        LaunchForceShield();
        break;
    case SuperWeaponType::LightningStorm:
        LaunchLightningStorm();
        break;
    case SuperWeaponType::PsychicDominator:
        LaunchPsychicDominator();
        break;
    case SuperWeaponType::GeneticConverter:
        LaunchGeneticMutator();
        break;
    case SuperWeaponType::ChronoSphere:
        LaunchChronoSphere();
        break;
    case SuperWeaponType::ChronoWarp:
        LaunchChronoWarp();
        break;
    case SuperWeaponType::ParaDrop:
        LaunchParaDrop();
        break;
    case SuperWeaponType::SpyPlane:
        LaunchSpyPlane();
        break;
    case SuperWeaponType::PsychicReveal:
        LaunchPsychicReveal();
        break;
    default:
        break;
    }
}

void SuperClass::UpdateFiring() {
    if (!Type) return;

    switch (Type->Type)
    {
    case SuperWeaponType::MultiMissile:
        UpdateNukeFiring();
        break;
    case SuperWeaponType::LightningStorm:
        UpdateLightningStormFiring();
        break;
    case SuperWeaponType::ChronoWarp:
        UpdateChronoWarpFiring();
        break;
    case SuperWeaponType::PsychicDominator:
        UpdateDominatorFiring();
        break;
    case SuperWeaponType::GeneticConverter:
        UpdateGeneticMutatorFiring();
        break;
    case SuperWeaponType::ParaDrop:
        UpdateParaDropFiring();
        break;
    case SuperWeaponType::SpyPlane:
        UpdateSpyPlaneFiring();
        break;
    default:
        // Immediate transition to Active for other types
        State = SWState::Active;
        break;
    }
}

void SuperClass::UpdateActive() {
    if (!Type) return;

    switch (Type->Type)
    {
    case SuperWeaponType::LightningStorm:
        UpdateLightningStormActive();
        break;
    case SuperWeaponType::ChronoWarp:
        UpdateChronoWarpActive();
        break;
    case SuperWeaponType::IronCurtain:
        UpdateIronCurtainActive();
        break;
    case SuperWeaponType::ForceShield:
        UpdateForceShieldActive();
        break;
    default:
        // Most types complete immediately
        if (!IsAnimationPlaying) {
            OnDone();
        }
        break;
    }
}

void SuperClass::OnDone() {
    if (!Type) return;

    State = SWState::Done;

    // Play post-launch sound
    if (Type->PostSound >= 0) {
        // VocClass::PlayGlobal(Type->PostSound, Type->PostSoundPriority, 1.0f);
    }

    // Reset for next use
    if (!Type->IsOneTime) {
        RechargeTimer = Type->RechargeTime;
        State = SWState::Idle;
        IsReady_ = false;
        IsAnimationPlaying = false;
    } else {
        IsAlreadyActivated = true;
        IsSuspended = true;
    }
}

// ============================================================================
// Type-specific: Nuke
// ============================================================================

void SuperClass::LaunchNuke() {
    if (!Type) return;

    NukeState = 0;
    NukeTimer = 60; // 60-frame countdown before impact

    // Create camera animation at target
    if (Type->CameraAnim) {
        CameraStart = CoordMath::CoordToCell(TargetCoord);
        // Camera animation setup
    }

    // Create SW animation
    if (Type->SWAnim) {
        AnimClass::CreateNukeAnim(TargetCoord);
    }

    // Play fire sound
    if (Type->FireSound >= 0) {
        // VocClass::PlayGlobal(Type->FireSound, Type->FireSoundPriority, 1.0f);
    }
}

void SuperClass::UpdateNukeFiring() {
    if (NukeTimer > 0) {
        --NukeTimer;
        if (NukeTimer <= 0) {
            DetonateNuke();
        }
    }
}

void SuperClass::DetonateNuke() {
    if (!Type) return;

    // Apply massive damage at target location
    int32 damage = Type->NukeDamage;
    int32 radius = Type->NukeRadius;

    // MapClass::DamageArea(TargetCoord, damage, radius, WarheadTypeClass::Find("Nuke"), Owner, true);

    // Create radiation sites
    for (int32 i = 0; i < 8; ++i) {
        int32 angle = i * 45 + (std::rand() % 30);
        double rad = angle * 3.141592653589793 / 180.0;
        int32 dist = (radius * LeptonsPerCell) * 3 / 4;
        CoordStruct radPos(
            TargetCoord.X + static_cast<int32>(std::cos(rad) * dist),
            TargetCoord.Y + static_cast<int32>(std::sin(rad) * dist),
            0
        );
        // RadSiteClass::CreateRadSite(radPos, Type->NukeRadDuration, Type->NukeRadLevel, Owner);
    }

    // Create explosion animation
    AnimClass::CreateExplosionAnim(TargetCoord, 3);

    // Light flash
    // TacticalClass::CreateLightFlash(TargetCoord, Type->LightSize, Type->LightIntensity,
    //     Type->LightVisibility, Type->LightRedTint, Type->LightGreenTint, Type->LightBlueTint);

    IsAnimationPlaying = false;
    State = SWState::Active;
    OnDone();
}

// ============================================================================
// Type-specific: IronCurtain
// ============================================================================

void SuperClass::LaunchIronCurtain() {
    if (!Type || !Owner) return;

    // Apply IronCurtain effect to all of owner's units in range
    int32 radius = 5; // default radius in cells
    CellStruct ownerCell = CoordMath::CoordToCell(TargetCoord);

    // Find all owned objects in range and apply invulnerability
    for (int32 i = 0; i < MAX_HOUSES; ++i) {
        if (HouseClass::GetHouseByIndex(i) != Owner) continue;
        // Iterate owner's techno objects
        // for each techno in range:
        //   techno->SetIronCurtained(true, Type->ChronoSphereDuration);
    }

    // Create activation animation
    if (Type->SWAnim) {
        AnimClass* anim = new AnimClass(Type->SWAnim, TargetCoord, 0, 1, 0x600, 0, false);
        anim->Owner = Owner;
    }

    // Play fire sound
    if (Type->FireSound >= 0) {
        // VocClass::PlayGlobal(Type->FireSound, Type->FireSoundPriority, 1.0f);
    }

    State = SWState::Active;
}

void SuperClass::UpdateIronCurtainActive() {
    // IronCurtain is duration-based, handled by TechnoClass timer
    // The SW itself transitions to done immediately
    State = SWState::Done;
    OnDone();
}

// ============================================================================
// Type-specific: ForceShield
// ============================================================================

void SuperClass::LaunchForceShield() {
    if (!Type || !Owner) return;

    // Apply ForceShield effect to all buildings in range
    CellStruct ownerCell = CoordMath::CoordToCell(TargetCoord);

    // Create force shield animation
    if (Type->SWAnim) {
        AnimClass* anim = new AnimClass(Type->SWAnim, TargetCoord, 0, 1, 0x600, 0, false);
        anim->Owner = Owner;
    }

    // Play fire sound
    if (Type->FireSound >= 0) {
        // VocClass::PlayGlobal(Type->FireSound, Type->FireSoundPriority, 1.0f);
    }

    // Apply shield effect to buildings
    // for each building in range:
    //   building->SetForceShielded(true, Type->ChronoSphereDuration);

    State = SWState::Active;
}

void SuperClass::UpdateForceShieldActive() {
    // ForceShield is duration-based, handled by BuildingClass timer
    State = SWState::Done;
    OnDone();
}

// ============================================================================
// Type-specific: LightningStorm
// ============================================================================

void SuperClass::LaunchLightningStorm() {
    if (!Type) return;

    LightningTimer = Type->LightningDeferment;
    LightningStrikeCount = 0;
    LightningDeferment = Type->LightningDeferment;
    LightningScatter = CellStruct(0, 0);

    // Create storm cloud animation
    if (Type->SWAnim) {
        AnimClass* anim = new AnimClass(Type->SWAnim, TargetCoord, 0, 1, 0x600, 0, false);
        anim->Owner = Owner;
    }

    // Play fire sound
    if (Type->FireSound >= 0) {
        // VocClass::PlayGlobal(Type->FireSound, Type->FireSoundPriority, 1.0f);
    }

    State = SWState::Firing;
}

void SuperClass::UpdateLightningStormFiring() {
    if (LightningTimer > 0) {
        --LightningTimer;

        // Check if it's time to strike
        if (LightningTimer % Type->LightningHitDelay == 0) {
            DoLightningStrike();
        }
    } else {
        State = SWState::Active;
        LightningTimer = Type->LightningStormDuration;
    }
}

void SuperClass::UpdateLightningStormActive() {
    if (LightningTimer > 0) {
        --LightningTimer;

        if (LightningTimer % Type->LightningHitDelay == 0) {
            DoLightningStrike();
        }
    } else {
        OnDone();
    }
}

void SuperClass::DoLightningStrike() {
    if (!Type) return;

    ++LightningStrikeCount;

    // Calculate strike position with scatter
    int32 scatterX = (std::rand() % (Type->LightningCellSpread * 2 + 1)) - Type->LightningCellSpread;
    int32 scatterY = (std::rand() % (Type->LightningCellSpread * 2 + 1)) - Type->LightningCellSpread;

    CellStruct strikeCell(
        TargetCell.X + scatterX + LightningScatter.X,
        TargetCell.Y + scatterY + LightningScatter.Y
    );

    // Change scatter direction occasionally
    if (LightningStrikeCount % Type->LightningScatterDelay == 0) {
        LightningScatter.X = (std::rand() % 5) - 2;
        LightningScatter.Y = (std::rand() % 5) - 2;
    }

    CoordStruct strikeCoord = CoordMath::CellToCoord(strikeCell);

    // Create lightning bolt animation
    AnimTypeClass* lightningAnim = AnimTypeClass::Find("LIGHTNING");
    if (lightningAnim) {
        new AnimClass(lightningAnim, strikeCoord, 0, 1, 0x600, 0, false);
    }

    // Apply damage
    if (Type->LightningWarhead && Type->LightningDamage > 0) {
        // MapClass::DamageArea(strikeCoord, Type->LightningDamage, Type->LightningRadius,
        //     Type->LightningWarhead, Owner, true);
    }
}

// ============================================================================
// Type-specific: PsychicDominator
// ============================================================================

void SuperClass::LaunchPsychicDominator() {
    if (!Type) return;

    DominatorTimer = 30; // Warm-up frames
    DominatorScroll = 0;
    DominatorActivated = false;

    // Create dominator animation
    if (Type->SWAnim) {
        AnimClass* anim = new AnimClass(Type->SWAnim, TargetCoord, 0, 1, 0x600, 0, false);
        anim->Owner = Owner;
    }

    // Play fire sound
    if (Type->FireSound >= 0) {
        // VocClass::PlayGlobal(Type->FireSound, Type->FireSoundPriority, 1.0f);
    }

    State = SWState::Firing;
}

void SuperClass::UpdateDominatorFiring() {
    if (DominatorTimer > 0) {
        --DominatorTimer;
        if (DominatorTimer <= 0) {
            ActivateDominator();
        }
    }
}

void SuperClass::ActivateDominator() {
    if (!Type || DominatorActivated) return;
    DominatorActivated = true;

    RulesClass* pRules = RulesClass::Instance;

    // ------------------------------------------------------------------
    // asm: PsyDom_Fire (asm:688234)
    //
    //   1. 取爆点格：Map.Coord_Cell(PsyDom_CellCoords) → cell->Get_Coords()
    //      （格中心，vt+0x48）。
    //   2. `new IonBlastClass` + CTOR(damage=1) —— 离子爆闪视觉。
    //      OpenYR 未建模 IonBlastClass，此处略（见报告存疑项），
    //      视觉由第 3 步的动画承担。
    //   3. `new AnimClass(Rules->[300h], 格中心, 0,1,600h,0,0)`，指针存
    //      PsyDom_Anim（一次性动画，本项目由 vec_Anims 自管生命周期）。
    //   4. ★ `Damage_At_Cell_Update_489280(loc, Rules->DominatorDamage,
    //        Sender=0, Rules->DominatorWarhead, a5=1, LightningStorm_Owner)`
    //      —— 全引擎统一的「范围伤害」入口（= OpenYR 的
    //      DamageArea::ApplyCellDamage → MapClass::Damage_At_Cell）。
    //   5. 以 CellSpread_Table 枚举爆炸半径内的格，逐格抓占用单位，
    //      对「非建筑 / 非心灵免疫 / 非铁幕 / 非气球悬浮 / 非空中 / 未被控」
    //      的目标：先解绑旧控制者（FreeUnit），再把控制权交给施放方，
    //      并在目标头顶挂 PermaControlledAnimationType 动画。
    // ------------------------------------------------------------------
    if (MapClass::Instance == nullptr || pRules == nullptr) {
        IsAnimationPlaying = false;
        State = SWState::Active;
        OnDone();
        return;
    }

    const CellStruct centerCell = Math::CoordToCell(TargetCoord);
    const CoordStruct  centerLoc  = CellClass::Cell2Coord(centerCell);

    // ---- 3. 爆点动画（Rules->DominatorFirstAnim，asm [ecx+300h]）--------
    AnimTypeClass* pBoomAnim = pRules->DominatorFirstAnim;
    if (pBoomAnim == nullptr) pBoomAnim = pRules->DominatorSecondAnim;
    if (pBoomAnim != nullptr) {
        AnimClass* pAnim = new AnimClass(pBoomAnim, centerLoc, 0, 1, 0x600, 0, false);
        if (pAnim != nullptr)
            pAnim->Owner = Owner;
    }

    // ---- 4. 统一范围伤害入口 --------------------------------------------
    if (pRules->DominatorDamage > 0 && pRules->DominatorWarhead != nullptr) {
        HouseClass* pOwnerHouse = Owner;
        DamageArea::ApplyCellDamage(centerLoc, pRules->DominatorDamage,
                                    nullptr,           // asm: Sender = 0
                                    pRules->DominatorWarhead,
                                    true,              // asm: a5 = 1
                                    pOwnerHouse);
    }

    // ---- 5. 心灵控制：按 CellSpread_Table 枚举半径内的占用单位 ----------
    //   asm: `mov eax,[Rules+308h] / cmp eax,0Ah / jl / mov eax,0Ah`
    //        → spread 级数取 DominatorCaptureRange，上限 10。
    int32 spread = pRules->DominatorCaptureRange;
    if (spread > 10) spread = 10;
    if (spread < 0) spread = 0;

    // 施放方的控制管理器：原版挂在统治者建筑（+0x2C0）上。这里在其
    // 所属建筑里找第一个带 CaptureManager 的（即心灵信标/控制塔本体）。
    CaptureManagerClass* pController = nullptr;
    if (Owner != nullptr && BuildingClass::Array != nullptr) {
        for (int32 i = 0; i < BuildingClass::Array->Count; ++i) {
            BuildingClass* pB = (*BuildingClass::Array)[i];
            if (pB == nullptr || pB->IsDead()) continue;
            if (pB->Owner != Owner) continue;
            if (pB->CaptureManager != nullptr) {
                pController = pB->CaptureManager;
                break;
            }
        }
    }

    for (int32 dx = -spread; dx <= spread; ++dx) {
        for (int32 dy = -spread; dy <= spread; ++dy) {
            const CellStruct cell(static_cast<int16>(centerCell.X + dx),
                                  static_cast<int16>(centerCell.Y + dy));
            CellClass* pCell = MapClass::Instance->GetCellAt(cell);
            if (pCell == nullptr) continue;

            for (ObjectClass* pObj = pCell->Get_Occupier(); pObj != nullptr;
                 pObj = pObj->NextObject) {
                const AbstractType what = pObj->WhatAmI();
                if (what != AbstractType::Infantry && what != AbstractType::Unit
                    && what != AbstractType::Aircraft) {
                    continue;                       // asm: 建筑(6) 直接跳过
                }

                TechnoClass* pVictim = static_cast<TechnoClass*>(pObj);
                if (pVictim == nullptr || pVictim->IsDead()) continue;
                if (pVictim->Owner == Owner) continue;   // 不控制自己人

                TechnoTypeClass* pVType = pVictim->GetTechnoType();
                if (pVType != nullptr && pVType->ImmuneToPsionics) continue;
                if (pVictim->IsIronCurtained()) continue;      // asm: vt+160h
                if (pVType != nullptr && pVType->BalloonHover) continue;
                if (pVictim->InAir()) continue;                // asm: vt+54h

                // asm: 目标已有控制器（[esi+2C0h]）→ FreeUnit 先解绑。
                if (pVictim->CaptureManager != nullptr) {
                    pVictim->CaptureManager->FreeUnit(pVictim);
                    pVictim->CaptureManager = nullptr;
                }

                // asm: `call [edx+3D4h](this, PsyDom_Owner, 1)` —— 把控制权
                //   交给施放方，并置 [esi+2C4h]=1（永久控制标志）。
                if (pController != nullptr) {
                    pController->CaptureUnit(pVictim);
                }

                // asm: PermaControlledAnimationType 挂在目标头顶
                //   （坐标 Z += MindControlRingOffset）。
                if (pRules->PermaControlledAnimationType != nullptr) {
                    CoordStruct ring = pVictim->GetCoords();
                    if (pVType != nullptr)
                        ring.Z += pVType->MindControlRingOffset;
                    AnimClass* pRing = new AnimClass(
                        pRules->PermaControlledAnimationType, ring, 0, 1, 0x600, 0, false);
                    if (pRing != nullptr)
                        pRing->Attach_To(pVictim);
                }
            }
        }
    }

    IsAnimationPlaying = false;
    State = SWState::Active;
    OnDone();
}

// ============================================================================
// Type-specific: GeneticMutator
// ============================================================================

void SuperClass::LaunchGeneticMutator() {
    if (!Type) return;

    GeneticMutatorTimer = 30;

    // Create mutator animation
    if (Type->SWAnim) {
        AnimClass* anim = new AnimClass(Type->SWAnim, TargetCoord, 0, 1, 0x600, 0, false);
        anim->Owner = Owner;
    }

    // Play fire sound
    if (Type->FireSound >= 0) {
        // VocClass::PlayGlobal(Type->FireSound, Type->FireSoundPriority, 1.0f);
    }

    State = SWState::Firing;
}

void SuperClass::UpdateGeneticMutatorFiring() {
    if (GeneticMutatorTimer > 0) {
        --GeneticMutatorTimer;
        if (GeneticMutatorTimer <= 0) {
            ActivateGeneticMutator();
        }
    }
}

void SuperClass::ActivateGeneticMutator() {
    if (!Type) return;

    // Convert all enemy infantry in radius to Brutes
    for (int32 i = 0; i < InfantryClass::Array->Count; ++i) {
        InfantryClass* infantry = (*InfantryClass::Array)[i];
        if (!infantry) continue;
        if (infantry->Owner == Owner) continue;

        int32 dx = infantry->Location.X - TargetCoord.X;
        int32 dy = infantry->Location.Y - TargetCoord.Y;
        int32 dist = static_cast<int32>(std::sqrt(static_cast<double>(dx*dx + dy*dy)));

        int32 radius = Type->GeneticMutatorRadius * LeptonsPerCell;
        if (dist <= radius) {
            // Create mutation explosion animation
            if (Type->GeneticMutatorExplosion) {
                new AnimClass(Type->GeneticMutatorExplosion, infantry->Location, 0, 1, 0x600, 0, false);
            }

            // Convert to Brute
            // InfantryTypeClass* bruteType = InfantryTypeClass::Find("BRUTE");
            // if (bruteType) {
            //     InfantryClass* brute = new InfantryClass(bruteType, Owner);
            //     brute->SetLocation(infantry->Location);
            //     infantry->Kill();
            // }
        }
    }

    // Apply genetic damage
    if (Type->GenetixMutatorDamage > 0 && Type->GeneticMutatorWarhead) {
        // MapClass::DamageArea(TargetCoord, Type->GenetixMutatorDamage, Type->GeneticMutatorRadius,
        //     Type->GeneticMutatorWarhead, Owner, true);
    }

    IsAnimationPlaying = false;
    State = SWState::Active;
    OnDone();
}

// ============================================================================
// Type-specific: ChronoSphere
// ============================================================================

void SuperClass::LaunchChronoSphere() {
    if (!Type) return;

    // Create chrono animation
    if (Type->SWAnim) {
        AnimClass* anim = new AnimClass(Type->SWAnim, TargetCoord, 0, 1, 0x600, 0, false);
        anim->Owner = Owner;
    }

    // Play fire sound
    if (Type->FireSound >= 0) {
        // VocClass::PlayGlobal(Type->FireSound, Type->FireSoundPriority, 1.0f);
    }

    State = SWState::Active;
}

// ============================================================================
// Type-specific: ChronoWarp
// ============================================================================

void SuperClass::LaunchChronoWarp() {
    if (!Type) return;

    ChronoWarpTimer = Type->ChronoWarpDuration;
    ChronoWarpState = 0;
    ChronoWarpDamageDone = 0;

    // Create chrono warp animation
    if (Type->SWAnim) {
        AnimClass* anim = new AnimClass(Type->SWAnim, TargetCoord, 0, 1, 0x600, 0, false);
        anim->Owner = Owner;
    }

    // Play fire sound
    if (Type->FireSound >= 0) {
        // VocClass::PlayGlobal(Type->FireSound, Type->FireSoundPriority, 1.0f);
    }

    State = SWState::Firing;
}

void SuperClass::UpdateChronoWarpFiring() {
    if (ChronoWarpTimer > 0) {
        --ChronoWarpTimer;
        if (ChronoWarpTimer <= 0) {
            ChronoWarpTimer = Type->ChronoWarpActiveDuration;
            ChronoWarpState = 1;
            State = SWState::Active;
        }
    }
}

void SuperClass::UpdateChronoWarpActive() {
    if (ChronoWarpTimer > 0) {
        --ChronoWarpTimer;

        // Apply chrono warp damage each frame
        if (ChronoWarpDamageDone < Type->ChronoWarpDamageMax) {
            int32 frameDamage = Type->ChronoWarpDamage / Type->ChronoWarpActiveDuration;
            if (frameDamage < 1) frameDamage = 1;

            ChronoWarpDamageDone += frameDamage;

            // Apply damage to all units/structures in radius
            // MapClass::DamageArea(TargetCoord, frameDamage, Type->ChronoWarpRadius,
            //     WarheadTypeClass::Find("ChronoWarp"), Owner, true);

            // Teleport random units out of the warp zone
            if (Type->ChronoWarpFire) {
                // Create fire animations in the warp zone
                for (int32 i = 0; i < 3; ++i) {
                    CoordStruct firePos = TargetCoord;
                    firePos.X += (std::rand() % 201 - 100) * LeptonsPerCell / 256;
                    firePos.Y += (std::rand() % 201 - 100) * LeptonsPerCell / 256;
                    AnimClass::CreateFireAnim(firePos);
                }
            }
        }

        if (ChronoWarpTimer <= 0) {
            OnDone();
        }
    }
}

// ============================================================================
// Type-specific: ParaDrop
// ============================================================================

void SuperClass::LaunchParaDrop() {
    if (!Type) return;

    ParaDropTimer = 60; // Time for planes to arrive
    ParaDropCount = 0;

    // Play fire sound
    if (Type->FireSound >= 0) {
        // VocClass::PlayGlobal(Type->FireSound, Type->FireSoundPriority, 1.0f);
    }

    State = SWState::Firing;
}

void SuperClass::UpdateParaDropFiring() {
    if (ParaDropTimer > 0) {
        --ParaDropTimer;

        // Spawn paradrop planes at intervals
        if (ParaDropTimer % 20 == 0 && ParaDropCount < Type->ParaDropCount) {
            SpawnParaDropPlane();
            ++ParaDropCount;
        }

        if (ParaDropTimer <= 0 && ParaDropCount >= Type->ParaDropCount) {
            OnDone();
        }
    }
}

void SuperClass::SpawnParaDropPlane() {
    if (!Type || !Owner) return;

    // Create a paradrop plane at map edge flying toward target
    CoordStruct startPos(0, TargetCoord.Y, 1000);
    CoordStruct endPos(TargetCoord);

    // AircraftClass* plane = new AircraftClass(Type->ParaDropPlane, Owner);
    // plane->SetLocation(startPos);
    // plane->SetMission(MissionType::Paradrop);
    // plane->SetTarget(TargetCoord);
    // plane->ParadropCount = Type->ParaDropNum;

    // Create paradrop animation
    if (Type->SWAnim) {
        AnimClass* anim = new AnimClass(Type->SWAnim, TargetCoord, 0, 1, 0x600, 0, false);
        anim->Owner = Owner;
    }
}

// ============================================================================
// Type-specific: SpyPlane
// ============================================================================

void SuperClass::LaunchSpyPlane() {
    if (!Type) return;

    SpyPlaneTimer = 30;

    // Play fire sound
    if (Type->FireSound >= 0) {
        // VocClass::PlayGlobal(Type->FireSound, Type->FireSoundPriority, 1.0f);
    }

    State = SWState::Firing;
}

void SuperClass::UpdateSpyPlaneFiring() {
    if (SpyPlaneTimer > 0) {
        --SpyPlaneTimer;

        if (SpyPlaneTimer <= 0) {
            SpawnSpyPlane();
            OnDone();
        }
    }
}

void SuperClass::SpawnSpyPlane() {
    if (!Type || !Owner) return;

    // Create spy plane aircraft
    CoordStruct startPos(0, TargetCoord.Y, 2000);
    CoordStruct endPos(MapClass::Instance->MapWidth * LeptonsPerCell, TargetCoord.Y, 2000);

    for (int32 i = 0; i < Type->SpyPlaneCount; ++i) {
        CoordStruct offset(0, i * 256 * LeptonsPerCell, 0);
        // AircraftClass* spyPlane = new AircraftClass(Type->SpyPlaneType, Owner);
        // spyPlane->SetLocation(startPos + offset);
        // spyPlane->SetMission(Type->SpyPlaneMission);
        // spyPlane->SetTarget(TargetCoord);

        // Reveal shroud along flight path
        // MapClass::RevealArea(startPos + offset, 3, Owner);
    }
}

// ============================================================================
// Type-specific: PsychicReveal
// ============================================================================

void SuperClass::LaunchPsychicReveal() {
    if (!Type || !Owner) return;

    // Reveal entire map temporarily
    // MapClass::RevealAll(Owner, true);

    // Create psychic reveal animation
    if (Type->SWAnim) {
        CoordStruct center(MapClass::Instance->MapWidth * LeptonsPerCell / 2, MapClass::Instance->MapHeight * LeptonsPerCell / 2, 0);
        AnimClass* anim = new AnimClass(Type->SWAnim, center, 0, 1, 0x600, 0, false);
        anim->Owner = Owner;
    }

    // Play fire sound
    if (Type->FireSound >= 0) {
        // VocClass::PlayGlobal(Type->FireSound, Type->FireSoundPriority, 1.0f);
    }

    State = SWState::Active;
    OnDone();
}

// ============================================================================
// Targeting
// ============================================================================

void SuperClass::SetTarget(CellStruct target) {
    TargetCell = target;
    TargetCoord = CoordMath::CellToCoord(target);
}

CellStruct SuperClass::GetTarget() const {
    return TargetCell;
}

CoordStruct SuperClass::GetTargetCoord() const {
    return TargetCoord;
}

bool SuperClass::CanTargetCell(CellStruct cell) const {
    if (!Type) return false;

    // Check if cell is within map bounds
    if (cell.X < 0 || cell.Y < 0 || cell.X >= MapClass::Instance->MapWidth || cell.Y >= MapClass::Instance->MapHeight)
        return false;

    // Check if cell is revealed (for targeting)
    if (Owner) {
        // if (!MapClass::IsCellRevealed(cell, Owner)) return false;
    }

    return true;
}

bool SuperClass::IsValidTarget() const {
    return CanTargetCell(TargetCell);
}

// ============================================================================
// Cursor management
// ============================================================================

int32 SuperClass::GetCursor() const {
    if (!Type) return 0;
    return Type->Cursor;
}

int32 SuperClass::GetNoCursor() const {
    if (!Type) return 0;
    return Type->NoCursor;
}

bool SuperClass::IsClickLaunch() const {
    if (!Type) return false;
    return Type->IsClickLaunch;
}

bool SuperClass::IsDesignator() const {
    if (!Type) return false;
    return Type->IsDesignator;
}

bool SuperClass::IsSelfTargeted() const {
    if (!Type) return false;
    return Type->IsSelfTargeted();
}

bool SuperClass::IsAutoFire() const {
    if (!Type) return false;
    return Type->IsAutoFire();
}

bool SuperClass::IsTargetable() const {
    if (!Type) return false;
    return Type->IsTargetable();
}

// ============================================================================
// State management
// ============================================================================

void SuperClass::Suspend() {
    IsSuspended = true;
}

void SuperClass::Resume() {
    IsSuspended = false;
}

void SuperClass::Reset() {
    if (!Type) return;
    RechargeTimer = Type->RechargeTime;
    State = SWState::Idle;
    IsReady_ = false;
    IsAnimationPlaying = false;
    IsAlreadyActivated = false;
    IsSuspended = false;
}

void SuperClass::ForceFire() {
    if (State == SWState::Ready) {
        Launch(TargetCell);
    }
}

void SuperClass::Grant() {
    IsGranted = true;
    if (Type && State == SWState::Idle && RechargeTimer <= 0) {
        OnReady();
    }
}

void SuperClass::Revoke() {
    IsGranted = false;
    State = SWState::Idle;
    IsReady_ = false;
}

// ============================================================================
// Static utility
// ============================================================================

void SuperClass::UpdateAll() {
    if (!Array) return;
    for (int32 i = Array->Count - 1; i >= 0; --i) {
        SuperClass* sw = (*Array)[i];
        if (sw) {
            sw->Update();
        }
    }
}

void SuperClass::RemoveAll() {
    if (!Array) return;
    while (Array->Count > 0) {
        SuperClass* sw = (*Array)[Array->Count - 1];
        if (sw) {
            GameDelete(sw);
        }
    }
}

SuperClass* SuperClass::FindByOwner(HouseClass* pOwner, SuperWeaponType swType) {
    if (!Array || !pOwner) return nullptr;
    for (int32 i = 0; i < Array->Count; ++i) {
        SuperClass* sw = (*Array)[i];
        if (sw && sw->Owner == pOwner && sw->Type && sw->Type->Type == swType) {
            return sw;
        }
    }
    return nullptr;
}

int32 SuperClass::GetReadyCount(HouseClass* pOwner) {
    if (!Array || !pOwner) return 0;
    int32 count = 0;
    for (int32 i = 0; i < Array->Count; ++i) {
        SuperClass* sw = (*Array)[i];
        if (sw && sw->Owner == pOwner && sw->IsReady()) {
            ++count;
        }
    }
    return count;
}
// ============================================================================
// Click-through pairing
// ============================================================================

 // SuperClass_StopPreclickAnim.  When a pre-click animation is
// playing it is finished off (zeroed remaining loops) and the instance is
// unlinked from the abstract registry before the animation reference is
// dropped.  The player-owned path additionally clears the pending click.
void SuperClass::StopPreclickAnim(bool isPlayer)
{
    // The pre-click animation handle lives alongside the deferred state; when
    // it is live the instance is registered in the abstract list.
    if (IsAnimationPlaying)
    {
        IsAnimationPlaying = false;

        if (AbstractClass::Array != nullptr)
        {
            AbstractClass* self = this;
            for (int32 i = 0; i < AbstractClass::Array->Count; ++i)
            {
                if (AbstractClass::Array->GetItem(i) == self)
                {
                    AbstractClass::Array->Remove(i);
                    break;
                }
            }
        }
    }

    if (isPlayer)
        PreClick = false;
}

 // SuperClass::Discharged.  Consume the weapon's charge.
// `coords` is the cell the weapon was aimed at, `ignoreRecharge` is the
// "don't start the recharge timer" flag the AI passes.  A human owner with the
// weapon already queued advances its deferred state; otherwise the recharge
// timer is restarted from the type's RechargeTime.
void SuperClass::Discharged(const CellStruct& coords, bool ignoreRecharge)
{
    if (Type == nullptr)
        return;

    TargetCell = coords;

    // The binary only does the deferred-state bookkeeping for powered types
    // when the owner is human; the AI path skips straight to the recharge.
    const bool isHuman = (Owner != nullptr && Owner->IsHumanPlayer);

    if (isHuman && State == SWState::Idle)
    {
        State = SWState::Ready;
        deferredCell = coords;

        // Recompute the remaining charge so a follow-up click resumes the
        // countdown where it left off.
        if (RechargeTimer > 0)
        {
            // The binary divides the elapsed charge by the rules-side scale
            // (RulesClass +0x760) and floors the result.
            const double scale = RulesClass::Instance->ChargeToDrainRatio;
            if (scale > 0.0)
            {
                int32 remaining = static_cast<int32>(RechargeTimer - (RechargeTimer / scale));
                if (remaining < 0)
                    remaining = 0;
                RechargeTimer = remaining;
            }
        }
    }

    if (ignoreRecharge)
        return;

    // Restart the recharge countdown; the weapon is no longer ready.
    RechargeTimer = Type->RechargeTime;
    State = SWState::Idle;
    IsCharged_ = false;

    // Hand the shot to the type-specific launch handler.
    Launch(coords);
}

// ============================================================================
// SuperClass - scripted charge manipulation
//
//  These are the three entry points the trigger-action family drives.  They
//  are the scripted counterparts of the automatic bookkeeping UpdateRecharge
//  performs.
// ============================================================================

 // 根据游戏行为，可知 SetSWCharge 负责下面这段逻辑。
//
//  Refuses to run unless the weapon is present (the "exists" byte at +0x6D)
//  and the requested percentage lies in 0..100.  The remaining recharge frames
//  are then `RechargeTime - floor(percent * 0.01 * RechargeTime)`; when that
//  difference is exactly zero the weapon is marked ready (+0x6F = 1).  The
//  recharge timer at +0x30 is re-armed, lazily stamping the current frame as
//  its start time when the timer was idle.
void SuperClass::SetCharge(int32 percent)
{
    if (!IsPresent())
        return;

    if (percent < 0 || percent > 100)
        return;

    // An explicit scripted recharge time overrides the type's nominal one.
    int32 rechargeTime = CustomChargeTime;
    if (rechargeTime == -1)
        rechargeTime = (Type != nullptr) ? Type->RechargeTime : 0;

    const int32 elapsed = static_cast<int32>(
        static_cast<double>(percent) * 0.01 * static_cast<double>(rechargeTime));

    const int32 remaining = rechargeTime - elapsed;

    if (remaining == 0)
        IsReady_ = true;

    RechargeTimer = remaining;
}

 // 根据游戏行为，可知 SetSWRecharge 负责下面这段逻辑。
//
//  `mov [ecx+24h], eax` - stores the frame count straight into the scripted
//  recharge override slot.  The next recharge computation picks it up.
void SuperClass::SetRecharge(int32 frames)
{
    CustomChargeTime = frames;
}

 // 根据游戏行为，可知 ResetSWRecharge 负责下面这段逻辑。
//
//  Restores the -1 sentinel so the type's own RechargeTime is used again.
void SuperClass::ResetRecharge()
{
    CustomChargeTime = -1;
}


 // 根据游戏行为，可知 Strike 负责下面这段逻辑。
//
//  asm: LightningStorm_Strike (asm:686488)
//
//    int __stdcall LightningStorm_Strike(int coords)
//      cell = Map.Coord_Cell(coords)
//      ; 取 WeatherConBolts[0] 的"柱高"，取半后经 sub_6D2120 换成 Z
//      height = WeatherConBolts[0]->vt[9Ch]() / 2 ; sub_6D2120(height)
//      pos = { cell.X<<8 + 80h, cell.Y<<8 + 80h,
//              height + (cell->[140h] & 100h ? dword_A9FA84 : 0)
//                     + cell->[11Bh] * dword_A9FA90 }
//      if (pos == qword_A9FA34) return            ; 哨兵坐标：不打雷
//      idx = Scenario.Random() % Rules->[2CCh]
//      type = Rules->[2C0h][idx]                  ; = RulesClass::WeatherConBolts
//      anim = new AnimClass(type, pos, 0, 1, 600h, 0, 0)
//      vec_Anims_const1.Add(anim); vec_Anims_const2.Add(anim)
//
//  ★ 原版本函数**不直接结算伤害**：落雷的伤害由这根雷电动画自身的
//    Warhead（AnimClass_ApplyDamage → Damage_At_Cell）承担。
//    此前 OpenYR 在这里调 `DamageArea::ApplyCellDamage(..., nullptr, nullptr, ...)`，
//    弹头为 null 时入口立刻返回，等于**一点伤害都没有**，且完全不产生动画。
//    现在按 asm 生成雷电动画（伤害随之沿统一入口结算）；另在规则提供了
//    [Lightning] 的伤害/弹头时补一次直接结算，避免美术侧动画没配
//    Warhead 时超武彻底空转。
void SuperClass::LightningStorm_Strike(const CellStruct& cell)
{
    if (MapClass::Instance == nullptr)
        return;

    RulesClass* pRules = RulesClass::Instance;

    // 落脚点：格中心，Z 落到地面高度（asm 的柱高/地形档位换算，见上）。
    CoordStruct pos = CellClass::Cell2Coord(cell);
    pos.Z = MapClass::Instance->GetCellFloorHeight(pos);

    // asm: 从 Rules->WeatherConBolts 里随机取一个雷电动画类型。
    AnimTypeClass* pBoltAnim = nullptr;
    if (pRules != nullptr && pRules->WeatherConBolts.Count > 0) {
        int32 idx = 0;
        if (ScenarioClass::Instance != nullptr) {
            idx = ScenarioClass::Instance->Random.Next(
                      0, pRules->WeatherConBolts.Count - 1);
        } else if (pRules->WeatherConBolts.Count > 1) {
            idx = std::rand() % pRules->WeatherConBolts.Count;
        }
        pBoltAnim = pRules->WeatherConBolts[idx];
    }

    if (pBoltAnim != nullptr) {
        // asm: flags = 600h / loopCount = 1 / a4 = 0 / a8 = 0 / ForcedZAdjust = 0
        //   本函数是 __stdcall 且只有 coords 一个参数（asm `retn 4`），
        //   没有 owner 可挂，动画归属留空。
        new AnimClass(pBoltAnim, pos, 0, 1, 0x600, 0, false);
    }

    // 规则侧的直接结算（见上方说明：原版由动画弹头承担，这里兜底）。
    if (pRules != nullptr && pRules->LightningDamage > 0
        && pRules->LightningWarhead != nullptr) {
        DamageArea::ApplyCellDamage(pos, pRules->LightningDamage,
                                    nullptr, pRules->LightningWarhead,
                                    true, nullptr);
    }
}

// ============================================================================
// 静态成员定义（补全）
// ============================================================================
CoordStruct SuperClass::Default_CellCoords;
CoordStruct SuperClass::Default_RoomCoords;

// 动画跟踪表：持有放置动画的超武登记于此，动画结束时摘除。
static DynamicVectorClass<SuperClass*> g_ChronoAnimWatchers;

static void SuperClass_AddChronoWatcher(SuperClass* pSuper)
{
    if (!pSuper)
        return;
    for (int32 i = 0; i < g_ChronoAnimWatchers.Count; ++i) {
        if (g_ChronoAnimWatchers.Items[i] == pSuper)
            return;
    }
    g_ChronoAnimWatchers.Add(pSuper);
}

static void SuperClass_RemoveChronoWatcher(SuperClass* pSuper)
{
    if (!pSuper)
        return;
    for (int32 i = 0; i < g_ChronoAnimWatchers.Count; ++i) {
        if (g_ChronoAnimWatchers.Items[i] == pSuper) {
            g_ChronoAnimWatchers.Remove(i);
            return;
        }
    }
}

// ------------------------------------------------------------------------
// asm: char __thiscall SuperClass_IsReadyToFire(SuperClass *this)   (asm:1471526)
// 充能展示态型按 0=充能/1=就绪/2=生效中 三态给出判定。挂起态一律
// 短路为不可用。
// ------------------------------------------------------------------------
bool SuperClass::IsReadyToFire() const
{
    if (IsSuspended)
        return false;
    if (!Type || !Type->UsesCameoChargeState)
        return IsReady_;
    return CameoChargeState != 0;
}

const char* SuperClass::NameReadiness() const
{
    if (IsSuspended)
        return "Hold";
    if (Type && Type->UsesCameoChargeState) {
        switch (CameoChargeState) {
        case 0:  return "Charging";
        case 1:  return "Ready";
        case 2:  return "Firestorm On";
        default: return nullptr;
        }
    }
    return IsReady_ ? "Ready" : nullptr;
}

bool SuperClass::ShouldFlash() const
{
    if (IsSuspended)
        return false;
    if (Type && Type->UsesCameoChargeState) {
        if (CameoChargeState == 0)
            return false;
    } else if (!IsReady_) {
        return false;
    }
    // asm: SuperClass_IgnoreClick
    // 值从闪亮起点帧起算，起点帧加帧数超过当前帧就继续闪。
    const int32 frames = Type ? Type->FlashSidebarTabFrames : 0;
    if (frames == -1)
        return true;
    if (frames == 0)
        return false;
    return FlashStartFrame + frames > FrameTimer::GetTime();
}

// asm: SuperClass_IgnoreClick
void SuperClass::IgnoreClick(bool ignore)
{
    (void)ignore;
}

void SuperClass::CreateChronoAnim(const CoordStruct& coords)
{
    // asm: SuperClass_IRTTITypeInfo_GetClassID
    // 摘除登记，挂起标记一并复位；随后在目标点上方 5 莱通处以规则库
    // 的放置动画型新建一次性动画并重新登记。
    if (ChronoAnim) {
        ChronoAnim->TimeToDie = true;
        ChronoAnim = nullptr;
    }
    if (ChronoAnimPending) {
        SuperClass_RemoveChronoWatcher(this);
        ChronoAnimPending = false;
    }
    RulesClass* pRules = RulesClass::Instance;
    if (!pRules || !pRules->ChronoPlacement)
        return;
    CoordStruct pos(coords.X, coords.Y, coords.Z + 5);
    AnimClass* pAnim = new AnimClass(pRules->ChronoPlacement, pos, 0, 1, 0x600, 0, false);
    if (!pAnim)
        return;
    ChronoAnim = pAnim;
    ChronoAnimPending = true;
    SuperClass_AddChronoWatcher(this);
}

// asm: SuperClass_IRTTITypeInfo_GetClassID
// （原版返回 0x39）作为返回码。
HRESULT SuperClass::IRTTITypeInfo_GetClassID(CLSID* pClassID)
{
    (void)pClassID;
    return static_cast<HRESULT>(0x39);
}

// asm: SuperClass_Init_DefaultCellCoords
// 坐标三维全清。
void SuperClass::Init_DefaultCellCoords()
{
    Default_CellCoords.X = 0;
    Default_CellCoords.Y = 0;
}

void SuperClass::Init_DefaultRoomCoords()
{
    Default_RoomCoords = CoordStruct(0, 0, 0);
}

// ============================================================================
// asm: LightningStorm_Start(duration, deferment, coords, owner)
//      —— 由触发动作 101（lct_TriggerAction_LightningStormStrike）调用。
//
//   语义：以路点坐标为中心开启雷暴 —— 若尚未激活，则置
//   LightningStorm_Active（全局标志，TeamClass/绘制侧读它），并在该格落雷。
//   已经激活时不重复开启（asm: LightningStorm_IsActive 非零则直接返回）。
// ============================================================================
void SuperClass::LightningStorm_Start(const CellStruct& cell)
{
    if (LightningStorm_IsActive())
        return;

    LightningStorm_Active = true;
    LightningStorm_Strike(cell);
}

// ============================================================================
// asm: 触发动作 93（lct_TriggerAction_IonCannonStrike）
//      —— 在路点格引爆离子炮：按 Rules 的 IonCannonDamage / IonCannonWarhead
//         对该格施加范围伤害。
//   （原版此处 new 一个 IonBlastClass 承载爆点动画 + 延时伤害；OpenYR 未建模
//     IonBlastClass，故直接按等价语义在爆点施加同一伤害。）
// ============================================================================
void SuperClass::Ion_Cannon_Strike(const CellStruct& cell)
{
    if (MapClass::Instance == nullptr)
        return;

    const CoordStruct coord = CellClass::Cell2Coord(cell);

    int32 damage = 1000;                       // 规则未加载时的兜底值
    WarheadTypeClass* pWarhead = nullptr;
    if (RulesClass::Instance != nullptr) {
        damage   = RulesClass::Instance->IonCannonDamage;
        pWarhead = RulesClass::Instance->IonCannonWarhead;
    }

    // 参数序：(center, damage, pSource, pWarhead, affectsAllies, pSourceHouse)
    DamageArea::ApplyCellDamage(coord, damage, nullptr, pWarhead, true, nullptr);
}
