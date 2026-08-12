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

#include <cmath>
#include <cstdlib>
#include <cstring>

// ============================================================================
// Static members
// ============================================================================

DynamicVectorClass<SuperClass*>* SuperClass::Array = nullptr;

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
    IsGranted(false),
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

// ============================================================================
// Update
// ============================================================================

void SuperClass::Update() {
    if (!Type || !Owner) return;
    if (State == SWState::Idle && IsSuspended) return;

    switch (State)
    {
    case SWState::Idle:
        if (IsSuspended) break;
        UpdateRecharge();
        break;

    case SWState::Ready:
        CheckAvailability();
        break;

    case SWState::Firing:
        UpdateFiring();
        break;

    case SWState::Active:
        UpdateActive();
        break;

    case SWState::Done:
        OnDone();
        break;

    default:
        break;
    }
}

void SuperClass::PointerExpired(AbstractClass* pAbstract, bool removed) {
    // Called by the pointer-fixup pass after a save/load cycle or when an
    // object is destroyed at runtime. Any pointer held by this SuperClass
    // that refers to the expired object must be nullified to prevent
    // dangling-pointer access on subsequent updates.
    //
    // The 'removed' flag distinguishes between a permanent removal (the
    // object has been destroyed and its memory freed) and a temporary
    // invalidation (the object is being relocated in memory during load).
    // In both cases the safe action is to clear the pointer; the load
    // path re-resolves Type and Owner from their stored indices after the
    // fixup pass completes.
    if (!pAbstract) {
        return;
    }

    // Owner is a HouseClass, which derives from AbstractClass. If the
    // owning house has been removed (e.g. the player was defeated and
    // their house object was deallocated), null out the reference so
    // that subsequent SW updates bail out early via the Owner null-checks.
    if (static_cast<AbstractClass*>(Owner) == pAbstract) {
        Owner = nullptr;
    }

    // 'next' links SuperClass instances in a per-house chain. If a linked
    // SW is removed, sever the link to avoid traversing into freed memory.
    if (static_cast<AbstractClass*>(next) == pAbstract) {
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
    case SuperWeaponType::Nuke:
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
    case SuperWeaponType::GeneticMutator:
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
    case SuperWeaponType::SonarPulse:
        LaunchSonarPulse();
        break;
    case SuperWeaponType::HunterSeeker:
        LaunchHunterSeeker();
        break;
    case SuperWeaponType::DropPod:
        LaunchDropPod();
        break;
    default:
        break;
    }
}

void SuperClass::UpdateFiring() {
    if (!Type) return;

    switch (Type->Type)
    {
    case SuperWeaponType::Nuke:
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
    case SuperWeaponType::GeneticMutator:
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

    // Apply mind control effect to all infantry in radius
    for (int32 i = 0; i < InfantryClass::Array->Count; ++i) {
        InfantryClass* infantry = (*InfantryClass::Array)[i];
        if (!infantry) continue;
        if (infantry->Owner == Owner) continue; // Don't affect own units

        int32 dx = infantry->Location.X - TargetCoord.X;
        int32 dy = infantry->Location.Y - TargetCoord.Y;
        int32 dist = static_cast<int32>(std::sqrt(static_cast<double>(dx*dx + dy*dy)));

        int32 radius = Type->DominatorRadius * LeptonsPerCell;
        if (dist <= radius) {
            // Apply mind control
            // infantry->SetMindControlled(true, Owner);

            // Apply PSI damage chance
            if (Type->DominatorPSIChance > 0 && Type->DominatorPSIDamage > 0) {
                if ((std::rand() % 100) < Type->DominatorPSIChance) {
                    // infantry->TakeDamage(Type->DominatorPSIDamage, Type->DominatorPSIAnim);
                }
            }
        }
    }

    // Apply damage to buildings in radius
    if (Type->DominatorDamage > 0) {
        // MapClass::DamageArea(TargetCoord, Type->DominatorDamage, Type->DominatorRadius,
        //     WarheadTypeClass::Find("Dominator"), Owner, true);
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
// Type-specific: SonarPulse
// ============================================================================

void SuperClass::LaunchSonarPulse() {
    if (!Type || !Owner) return;

    // Reveal all submarines/water units on the map
    // MapClass::SonarPulse(Owner);

    // Play fire sound
    if (Type->FireSound >= 0) {
        // VocClass::PlayGlobal(Type->FireSound, Type->FireSoundPriority, 1.0f);
    }

    State = SWState::Active;
    OnDone();
}

// ============================================================================
// Type-specific: HunterSeeker
// ============================================================================

void SuperClass::LaunchHunterSeeker() {
    if (!Type || !Owner) return;

    // Create a HunterSeeker unit that seeks the nearest enemy
    // UnitTypeClass* hsType = UnitTypeClass::Find("HUNTERSEEKER");
    // if (hsType) {
    //     UnitClass* hunterSeeker = new UnitClass(hsType, Owner);
    //     hunterSeeker->SetLocation(Owner->GetBaseCenter());
    //     hunterSeeker->SetMission(MissionType::Hunt);
    // }

    // Play fire sound
    if (Type->FireSound >= 0) {
        // VocClass::PlayGlobal(Type->FireSound, Type->FireSoundPriority, 1.0f);
    }

    State = SWState::Active;
    OnDone();
}

// ============================================================================
// Type-specific: DropPod
// ============================================================================

void SuperClass::LaunchDropPod() {
    if (!Type) return;

    // Create drop pod animation
    if (Type->SWAnim) {
        AnimClass* anim = new AnimClass(Type->SWAnim, TargetCoord, 0, 1, 0x600, 0, false);
        anim->Owner = Owner;
    }

    // Play fire sound
    if (Type->FireSound >= 0) {
        // VocClass::PlayGlobal(Type->FireSound, Type->FireSoundPriority, 1.0f);
    }

    // Spawn drop pod infantry
    // for (int32 i = 0; i < Type->ParaDropNum; ++i) {
    //     CoordStruct dropPos = TargetCoord;
    //     dropPos.X += (std::rand() % 201 - 100) * LeptonsPerCell / 256;
    //     dropPos.Y += (std::rand() % 201 - 100) * LeptonsPerCell / 256;
    //     InfantryClass* infantry = new InfantryClass(Type->ParaDropType, Owner);
    //     infantry->SetLocation(dropPos);
    // }

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