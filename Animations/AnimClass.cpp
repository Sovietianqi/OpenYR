#include "AnimClass.h"
#include "AnimTypeClass.h"
#include "../Combat/WarheadTypeClass.h"
#include "../Combat/WeaponTypeClass.h"
#include "../Combat/BulletClass.h"
#include "../Houses/HouseClass.h"
#include "../Map/MapClass.h"
#include "../Map/CellClass.h"
#include "../Particles/ParticleClass.h"
#include "../Particles/ParticleSystemClass.h"
#include "../Rendering/TacticalClass.h"
#include "../Rendering/DisplayClass.h"
#include "../Rendering/Surface.h"
#include "../Abstract/TechnoClass.h"
#include "../Abstract/FootClass.h"
#include "../Abstract/InfantryClass.h"
#include "../Scenario/ScenarioClass.h"
#include "../Rules/RulesClass.h"
#include "../Game/Game.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

// ============================================================================
// Static members
// ============================================================================

DynamicVectorClass<AnimClass*>* AnimClass::Array = nullptr;

// ============================================================================
// Construction / Destruction
// ============================================================================

// Default construction used by the object factory / array preallocation.
// Mirrors the original binary's AnimClass::AnimClass() default ctor.
AnimClass::AnimClass() noexcept :
    ObjectClass(),
    Type(nullptr),
    OwnerObject(nullptr),
    unknown_D0(0),
    LightConvert(nullptr),
    LightConvertIndex(-1),
    TintColor(0),
    ZAdjust(0),
    YSortAdjust(0),
    FlamingGuyCoords(0, 0, 0),
    FlamingGuyRetries(0),
    IsBuildingAnim(false),
    UnderTemporal(false),
    Paused(false),
    Unpaused(false),
    PausedAnimFrame(0),
    Reverse(false),
    unknown_124(0),
    TranslucencyLevel(0),
    TimeToDie(false),
    AttachedBullet(nullptr),
    Owner(nullptr),
    LoopDelay(0),
    Accum(1.0),
    AnimFlags(BlitterFlags::None),
    HasExtras(false),
    RemainingIterations(0),
    unknown_196(0),
    unknown_197(0),
    IsPlaying_(false),
    IsFogged(false),
    FlamingGuyExpire(false),
    UnableToContinue(false),
    SkipProcessOnce(true),
    Invisible(false),
    PowerOff(false),
    unused_19F(0),
    IsFlaming(false),
    IsNuke(false),
    IsIonCannon(false),
    IsLooping_(false),
    TrailerCount(0),
    TrailerTimer(0),
    LightTimer(0),
    SoundTimer(0),
    ScorchCreated(false),
    CraterCreated(false),
    InfantryCreated(false),
    TiberiumCreated(false),
    ChainAnimCreated(false)
{
    Location = CoordStruct(0, 0, 0);
    std::memset(PaletteName, 0, sizeof(PaletteName));
    std::memset(&Animation, 0, sizeof(Animation));
}

AnimClass::AnimClass(AnimTypeClass* pAnimType, const CoordStruct& loc,
    int32 loopDelay, int32 loopCount,
    uint32 flags, int32 forceZAdjust, bool reverse) noexcept :
    ObjectClass(),
    Type(pAnimType),
    OwnerObject(nullptr),
    unknown_D0(0),
    LightConvert(nullptr),
    LightConvertIndex(-1),
    TintColor(0),
    ZAdjust(0),
    YSortAdjust(0),
    FlamingGuyCoords(0, 0, 0),
    FlamingGuyRetries(0),
    IsBuildingAnim(false),
    UnderTemporal(false),
    Paused(false),
    Unpaused(false),
    PausedAnimFrame(0),
    Reverse(reverse),
    unknown_124(0),
    TranslucencyLevel(0),
    TimeToDie(false),
    AttachedBullet(nullptr),
    Owner(nullptr),
    LoopDelay(loopDelay),
    Accum(1.0),
    AnimFlags(static_cast<BlitterFlags>(flags)),
    HasExtras(false),
    RemainingIterations(0),
    unknown_196(0),
    unknown_197(0),
    IsPlaying_(false),
    IsFogged(false),
    FlamingGuyExpire(false),
    UnableToContinue(false),
    SkipProcessOnce(true),
    Invisible(false),
    PowerOff(false),
    unused_19F(0),
    IsFlaming(false),
    IsNuke(false),
    IsIonCannon(false),
    IsLooping_(false),
    TrailerCount(0),
    TrailerTimer(0),
    LightTimer(0),
    SoundTimer(0),
    ScorchCreated(false),
    CraterCreated(false),
    InfantryCreated(false),
    TiberiumCreated(false),
    ChainAnimCreated(false)
{
    Location = loc;
    std::memset(PaletteName, 0, sizeof(PaletteName));

    if (Type) {
        Animation.Rate = Type->Rate;
        Animation.Start = Type->Start;
        Animation.LoopStart = Type->LoopStart;
        Animation.LoopEnd = Type->LoopEnd;
        Animation.End = Type->End;
        Animation.LoopCount = loopCount;
        Animation.Value = Type->Start;

        ZAdjust = forceZAdjust != 0 ? forceZAdjust : Type->ZAdjust;
        YSortAdjust = Type->YSortAdjust;
        TranslucencyLevel = static_cast<BYTE>(Type->Translucency);
        IsBuildingAnim = Type->ShouldUseCellDrawer;
        HasExtras = Type->IsMeteor || Type->Bouncer;
        Invisible = Type->IsInvisible;
        IsLooping_ = Type->IsLooping;

        if (Type->IsMeteor || Type->Bouncer) {
            HasExtras = true;
            Bounce.Elasticity = static_cast<float>(Type->Elasticity);
            Bounce.IsActive = true;
            Bounce.MaxBounces = 3;
            Bounce.Velocity = CoordStruct(
                (std::rand() % 21 - 10),
                (std::rand() % 21 - 10),
                static_cast<int32>(Type->MinZVel > 0 ? Type->MinZVel : 10.0)
            );
        }

        if (Type->IsFlamingGuy) {
            IsFlaming = true;
            FlamingGuyRetries = 0;
        }

        if (Type->LoopStart >= 0 && Type->LoopEnd >= 0) {
            // Looping is configured via LoopStart/LoopEnd
        }

        if (!_strcmpi(Type->get_ID(), "NukeAnim") || Type->IsNuke) {
            IsNuke = true;
        }
        if (!_strcmpi(Type->get_ID(), "IonCannonAnim") || Type->IsIonCannon) {
            IsIonCannon = true;
        }

        // Initialize trailer timer
        if (Type->TrailerAnim && Type->TrailerSeperation > 0) {
            TrailerTimer = Type->TrailerSeperation;
        }

        // Initialize light timer
        if (Type->LightFlashFrames > 0) {
            LightTimer = Type->LightFlashFrames;
        }

        // Play report sound on creation
        if (Type->Report >= 0) {
            SoundTimer = 1;
        }
    }

    if (!Array) {
        Array = new DynamicVectorClass<AnimClass*>();
    }
    Array->Add(this);
}

AnimClass::~AnimClass() {
    if (Array) {
        for (int32 i = 0; i < Array->Count; ++i) {
            if ((*Array)[i] == this) {
                Array->Remove(i);
                break;
            }
        }
    }

    // Create chain animation on destruction
    if (Type && Type->Next && !ChainAnimCreated) {
        CoordStruct nextLoc = Location;
        new AnimClass(Type->Next, nextLoc, 0, 1, 0x600, 0, false);
    }
}

// ============================================================================
// IPersist
// ============================================================================

HRESULT AnimClass::GetClassID(CLSID* pClassID) {
    if (!pClassID) return E_POINTER;
    pClassID->Data1 = static_cast<uint32>(AbstractType::Anim);
    return S_OK;
}

HRESULT AnimClass::Load(IStream* pStm) {
    if (!pStm) return E_POINTER;

    ULONG read = 0;
    HRESULT hr = S_OK;

    // Read anim type ID and resolve.
    char typeID[0x18];
    hr = pStm->Read(typeID, sizeof(typeID), &read);
    if (hr < 0 || read != sizeof(typeID)) return E_FAIL;
    typeID[sizeof(typeID) - 1] = '\0';
    Type = typeID[0] ? AnimTypeClass::Find(typeID) : nullptr;

    // Read Animation struct fields.
    hr = pStm->Read(&Animation.Rate, sizeof(Animation.Rate), &read);
    if (hr < 0 || read != sizeof(Animation.Rate)) return E_FAIL;
    hr = pStm->Read(&Animation.Start, sizeof(Animation.Start), &read);
    if (hr < 0 || read != sizeof(Animation.Start)) return E_FAIL;
    hr = pStm->Read(&Animation.LoopStart, sizeof(Animation.LoopStart), &read);
    if (hr < 0 || read != sizeof(Animation.LoopStart)) return E_FAIL;
    hr = pStm->Read(&Animation.LoopEnd, sizeof(Animation.LoopEnd), &read);
    if (hr < 0 || read != sizeof(Animation.LoopEnd)) return E_FAIL;
    hr = pStm->Read(&Animation.End, sizeof(Animation.End), &read);
    if (hr < 0 || read != sizeof(Animation.End)) return E_FAIL;
    hr = pStm->Read(&Animation.LoopCount, sizeof(Animation.LoopCount), &read);
    if (hr < 0 || read != sizeof(Animation.LoopCount)) return E_FAIL;
    hr = pStm->Read(&Animation.Value, sizeof(Animation.Value), &read);
    if (hr < 0 || read != sizeof(Animation.Value)) return E_FAIL;
    hr = pStm->Read(&Animation.CurrentLoop, sizeof(Animation.CurrentLoop), &read);
    if (hr < 0 || read != sizeof(Animation.CurrentLoop)) return E_FAIL;

    uint32 animFlags = 0;
    hr = pStm->Read(&animFlags, sizeof(animFlags), &read);
    if (hr < 0 || read != sizeof(animFlags)) return E_FAIL;
    Animation.IsActive   = (animFlags & 0x01) != 0;
    Animation.IsLooping  = (animFlags & 0x02) != 0;
    Animation.HasStarted = (animFlags & 0x04) != 0;

    hr = pStm->Read(Animation.PaletteName, sizeof(Animation.PaletteName), &read);
    if (hr < 0 || read != sizeof(Animation.PaletteName)) return E_FAIL;

    // Read owner object index and resolve.
    int32 ownerObjIdx = -1;
    hr = pStm->Read(&ownerObjIdx, sizeof(ownerObjIdx), &read);
    if (hr < 0 || read != sizeof(ownerObjIdx)) return E_FAIL;
    OwnerObject = (ownerObjIdx >= 0) ? ObjectClass::Get_Instance(ownerObjIdx) : nullptr;

    hr = pStm->Read(&unknown_D0, sizeof(unknown_D0), &read);
    if (hr < 0 || read != sizeof(unknown_D0)) return E_FAIL;

    // Read light convert index and resolve.
    int32 lightConvIdx = -1;
    hr = pStm->Read(&lightConvIdx, sizeof(lightConvIdx), &read);
    if (hr < 0 || read != sizeof(lightConvIdx)) return E_FAIL;
    LightConvert = (lightConvIdx >= 0) ? ObjectClass::Get_Instance(lightConvIdx) : nullptr;

    hr = pStm->Read(&LightConvertIndex, sizeof(LightConvertIndex), &read);
    if (hr < 0 || read != sizeof(LightConvertIndex)) return E_FAIL;
    hr = pStm->Read(&TintColor, sizeof(TintColor), &read);
    if (hr < 0 || read != sizeof(TintColor)) return E_FAIL;
    hr = pStm->Read(&ZAdjust, sizeof(ZAdjust), &read);
    if (hr < 0 || read != sizeof(ZAdjust)) return E_FAIL;
    hr = pStm->Read(&YSortAdjust, sizeof(YSortAdjust), &read);
    if (hr < 0 || read != sizeof(YSortAdjust)) return E_FAIL;

    // Read flaming guy coords.
    hr = pStm->Read(&FlamingGuyCoords.X, sizeof(FlamingGuyCoords.X), &read);
    if (hr < 0 || read != sizeof(FlamingGuyCoords.X)) return E_FAIL;
    hr = pStm->Read(&FlamingGuyCoords.Y, sizeof(FlamingGuyCoords.Y), &read);
    if (hr < 0 || read != sizeof(FlamingGuyCoords.Y)) return E_FAIL;
    hr = pStm->Read(&FlamingGuyCoords.Z, sizeof(FlamingGuyCoords.Z), &read);
    if (hr < 0 || read != sizeof(FlamingGuyCoords.Z)) return E_FAIL;

    hr = pStm->Read(&FlamingGuyRetries, sizeof(FlamingGuyRetries), &read);
    if (hr < 0 || read != sizeof(FlamingGuyRetries)) return E_FAIL;

    // Read packed bool flags (group 1).
    uint32 flags1 = 0;
    hr = pStm->Read(&flags1, sizeof(flags1), &read);
    if (hr < 0 || read != sizeof(flags1)) return E_FAIL;
    IsBuildingAnim = (flags1 & 0x01) != 0;
    UnderTemporal  = (flags1 & 0x02) != 0;
    Paused         = (flags1 & 0x04) != 0;
    Unpaused       = (flags1 & 0x08) != 0;

    hr = pStm->Read(&PausedAnimFrame, sizeof(PausedAnimFrame), &read);
    if (hr < 0 || read != sizeof(PausedAnimFrame)) return E_FAIL;

    // Read packed bool flags (group 2).
    uint32 flags2 = 0;
    hr = pStm->Read(&flags2, sizeof(flags2), &read);
    if (hr < 0 || read != sizeof(flags2)) return E_FAIL;
    Reverse   = (flags2 & 0x01) != 0;
    TimeToDie = (flags2 & 0x02) != 0;
    HasExtras = (flags2 & 0x04) != 0;

    hr = pStm->Read(&unknown_124, sizeof(unknown_124), &read);
    if (hr < 0 || read != sizeof(unknown_124)) return E_FAIL;
    hr = pStm->Read(&TranslucencyLevel, sizeof(TranslucencyLevel), &read);
    if (hr < 0 || read != sizeof(TranslucencyLevel)) return E_FAIL;

    // Read attached bullet index and resolve.
    int32 bulletIdx = -1;
    hr = pStm->Read(&bulletIdx, sizeof(bulletIdx), &read);
    if (hr < 0 || read != sizeof(bulletIdx)) return E_FAIL;
    AttachedBullet = nullptr;
    if (bulletIdx >= 0 && BulletClass::Array && bulletIdx < BulletClass::Array->Count) {
        AttachedBullet = (*BulletClass::Array)[bulletIdx];
    }

    // Read owner house index and resolve.
    int32 ownerIdx = -1;
    hr = pStm->Read(&ownerIdx, sizeof(ownerIdx), &read);
    if (hr < 0 || read != sizeof(ownerIdx)) return E_FAIL;
    Owner = (ownerIdx >= 0) ? HouseClass::GetHouseByIndex(ownerIdx) : nullptr;

    hr = pStm->Read(&LoopDelay, sizeof(LoopDelay), &read);
    if (hr < 0 || read != sizeof(LoopDelay)) return E_FAIL;
    hr = pStm->Read(&Accum, sizeof(Accum), &read);
    if (hr < 0 || read != sizeof(Accum)) return E_FAIL;

    uint32 animFlagsVal = 0;
    hr = pStm->Read(&animFlagsVal, sizeof(animFlagsVal), &read);
    if (hr < 0 || read != sizeof(animFlagsVal)) return E_FAIL;
    AnimFlags = static_cast<BlitterFlags>(animFlagsVal);

    hr = pStm->Read(&RemainingIterations, sizeof(RemainingIterations), &read);
    if (hr < 0 || read != sizeof(RemainingIterations)) return E_FAIL;
    hr = pStm->Read(&unknown_196, sizeof(unknown_196), &read);
    if (hr < 0 || read != sizeof(unknown_196)) return E_FAIL;
    hr = pStm->Read(&unknown_197, sizeof(unknown_197), &read);
    if (hr < 0 || read != sizeof(unknown_197)) return E_FAIL;

    // Read packed bool flags (group 3).
    uint32 flags3 = 0;
    hr = pStm->Read(&flags3, sizeof(flags3), &read);
    if (hr < 0 || read != sizeof(flags3)) return E_FAIL;
    IsPlaying_        = (flags3 & 0x00000001) != 0;
    IsFogged          = (flags3 & 0x00000002) != 0;
    FlamingGuyExpire  = (flags3 & 0x00000004) != 0;
    UnableToContinue  = (flags3 & 0x00000008) != 0;
    SkipProcessOnce   = (flags3 & 0x00000010) != 0;
    Invisible         = (flags3 & 0x00000020) != 0;
    PowerOff          = (flags3 & 0x00000040) != 0;
    unused_19F        = (flags3 & 0x00000080) != 0;
    IsFlaming         = (flags3 & 0x00000100) != 0;
    IsNuke            = (flags3 & 0x00000200) != 0;
    IsIonCannon       = (flags3 & 0x00000400) != 0;
    IsLooping_        = (flags3 & 0x00000800) != 0;

    // Read bounce data.
    hr = pStm->Read(&Bounce.Velocity.X, sizeof(Bounce.Velocity.X), &read);
    if (hr < 0 || read != sizeof(Bounce.Velocity.X)) return E_FAIL;
    hr = pStm->Read(&Bounce.Velocity.Y, sizeof(Bounce.Velocity.Y), &read);
    if (hr < 0 || read != sizeof(Bounce.Velocity.Y)) return E_FAIL;
    hr = pStm->Read(&Bounce.Velocity.Z, sizeof(Bounce.Velocity.Z), &read);
    if (hr < 0 || read != sizeof(Bounce.Velocity.Z)) return E_FAIL;
    hr = pStm->Read(&Bounce.Elasticity, sizeof(Bounce.Elasticity), &read);
    if (hr < 0 || read != sizeof(Bounce.Elasticity)) return E_FAIL;
    hr = pStm->Read(&Bounce.Bounces, sizeof(Bounce.Bounces), &read);
    if (hr < 0 || read != sizeof(Bounce.Bounces)) return E_FAIL;
    hr = pStm->Read(&Bounce.MaxBounces, sizeof(Bounce.MaxBounces), &read);
    if (hr < 0 || read != sizeof(Bounce.MaxBounces)) return E_FAIL;

    uint32 bounceFlags = 0;
    hr = pStm->Read(&bounceFlags, sizeof(bounceFlags), &read);
    if (hr < 0 || read != sizeof(bounceFlags)) return E_FAIL;
    Bounce.IsActive = (bounceFlags & 0x01) != 0;

    hr = pStm->Read(&TrailerCount, sizeof(TrailerCount), &read);
    if (hr < 0 || read != sizeof(TrailerCount)) return E_FAIL;
    hr = pStm->Read(&TrailerTimer, sizeof(TrailerTimer), &read);
    if (hr < 0 || read != sizeof(TrailerTimer)) return E_FAIL;
    hr = pStm->Read(&LightTimer, sizeof(LightTimer), &read);
    if (hr < 0 || read != sizeof(LightTimer)) return E_FAIL;
    hr = pStm->Read(&SoundTimer, sizeof(SoundTimer), &read);
    if (hr < 0 || read != sizeof(SoundTimer)) return E_FAIL;

    // Read packed bool flags (group 4).
    uint32 flags4 = 0;
    hr = pStm->Read(&flags4, sizeof(flags4), &read);
    if (hr < 0 || read != sizeof(flags4)) return E_FAIL;
    ScorchCreated   = (flags4 & 0x01) != 0;
    CraterCreated   = (flags4 & 0x02) != 0;
    InfantryCreated = (flags4 & 0x04) != 0;
    TiberiumCreated = (flags4 & 0x08) != 0;
    ChainAnimCreated = (flags4 & 0x10) != 0;

    hr = pStm->Read(PaletteName, sizeof(PaletteName), &read);
    if (hr < 0 || read != sizeof(PaletteName)) return E_FAIL;

    return S_OK;
}

HRESULT AnimClass::Save(IStream* pStm, BOOL fClearDirty) {
    if (!pStm) return E_POINTER;

    ULONG written = 0;
    HRESULT hr = S_OK;

    // Write anim type ID.
    char typeID[0x18];
    std::memset(typeID, 0, sizeof(typeID));
    if (Type && Type->get_ID()) {
        const char* src = Type->get_ID();
        int32 j = 0;
        while (src[j] && j < static_cast<int32>(sizeof(typeID)) - 1) {
            typeID[j] = src[j]; ++j;
        }
    }
    hr = pStm->Write(typeID, sizeof(typeID), &written);
    if (hr < 0 || written != sizeof(typeID)) return E_FAIL;

    // Write Animation struct fields.
    hr = pStm->Write(&Animation.Rate, sizeof(Animation.Rate), &written);
    if (hr < 0 || written != sizeof(Animation.Rate)) return E_FAIL;
    hr = pStm->Write(&Animation.Start, sizeof(Animation.Start), &written);
    if (hr < 0 || written != sizeof(Animation.Start)) return E_FAIL;
    hr = pStm->Write(&Animation.LoopStart, sizeof(Animation.LoopStart), &written);
    if (hr < 0 || written != sizeof(Animation.LoopStart)) return E_FAIL;
    hr = pStm->Write(&Animation.LoopEnd, sizeof(Animation.LoopEnd), &written);
    if (hr < 0 || written != sizeof(Animation.LoopEnd)) return E_FAIL;
    hr = pStm->Write(&Animation.End, sizeof(Animation.End), &written);
    if (hr < 0 || written != sizeof(Animation.End)) return E_FAIL;
    hr = pStm->Write(&Animation.LoopCount, sizeof(Animation.LoopCount), &written);
    if (hr < 0 || written != sizeof(Animation.LoopCount)) return E_FAIL;
    hr = pStm->Write(&Animation.Value, sizeof(Animation.Value), &written);
    if (hr < 0 || written != sizeof(Animation.Value)) return E_FAIL;
    hr = pStm->Write(&Animation.CurrentLoop, sizeof(Animation.CurrentLoop), &written);
    if (hr < 0 || written != sizeof(Animation.CurrentLoop)) return E_FAIL;

    uint32 animFlags = 0;
    if (Animation.IsActive)   animFlags |= 0x01;
    if (Animation.IsLooping)  animFlags |= 0x02;
    if (Animation.HasStarted) animFlags |= 0x04;
    hr = pStm->Write(&animFlags, sizeof(animFlags), &written);
    if (hr < 0 || written != sizeof(animFlags)) return E_FAIL;

    hr = pStm->Write(Animation.PaletteName, sizeof(Animation.PaletteName), &written);
    if (hr < 0 || written != sizeof(Animation.PaletteName)) return E_FAIL;

    // Write owner object index.
    int32 ownerObjIdx = OwnerObject ? ObjectClass::Find_Index(OwnerObject) : -1;
    hr = pStm->Write(&ownerObjIdx, sizeof(ownerObjIdx), &written);
    if (hr < 0 || written != sizeof(ownerObjIdx)) return E_FAIL;

    hr = pStm->Write(&unknown_D0, sizeof(unknown_D0), &written);
    if (hr < 0 || written != sizeof(unknown_D0)) return E_FAIL;

    // Write light convert index.
    int32 lightConvIdx = LightConvert ? ObjectClass::Find_Index(LightConvert) : -1;
    hr = pStm->Write(&lightConvIdx, sizeof(lightConvIdx), &written);
    if (hr < 0 || written != sizeof(lightConvIdx)) return E_FAIL;

    hr = pStm->Write(&LightConvertIndex, sizeof(LightConvertIndex), &written);
    if (hr < 0 || written != sizeof(LightConvertIndex)) return E_FAIL;
    hr = pStm->Write(&TintColor, sizeof(TintColor), &written);
    if (hr < 0 || written != sizeof(TintColor)) return E_FAIL;
    hr = pStm->Write(&ZAdjust, sizeof(ZAdjust), &written);
    if (hr < 0 || written != sizeof(ZAdjust)) return E_FAIL;
    hr = pStm->Write(&YSortAdjust, sizeof(YSortAdjust), &written);
    if (hr < 0 || written != sizeof(YSortAdjust)) return E_FAIL;

    // Write flaming guy coords.
    hr = pStm->Write(&FlamingGuyCoords.X, sizeof(FlamingGuyCoords.X), &written);
    if (hr < 0 || written != sizeof(FlamingGuyCoords.X)) return E_FAIL;
    hr = pStm->Write(&FlamingGuyCoords.Y, sizeof(FlamingGuyCoords.Y), &written);
    if (hr < 0 || written != sizeof(FlamingGuyCoords.Y)) return E_FAIL;
    hr = pStm->Write(&FlamingGuyCoords.Z, sizeof(FlamingGuyCoords.Z), &written);
    if (hr < 0 || written != sizeof(FlamingGuyCoords.Z)) return E_FAIL;

    hr = pStm->Write(&FlamingGuyRetries, sizeof(FlamingGuyRetries), &written);
    if (hr < 0 || written != sizeof(FlamingGuyRetries)) return E_FAIL;

    // Write packed bool flags (group 1).
    uint32 flags1 = 0;
    if (IsBuildingAnim) flags1 |= 0x01;
    if (UnderTemporal)  flags1 |= 0x02;
    if (Paused)         flags1 |= 0x04;
    if (Unpaused)       flags1 |= 0x08;
    hr = pStm->Write(&flags1, sizeof(flags1), &written);
    if (hr < 0 || written != sizeof(flags1)) return E_FAIL;

    hr = pStm->Write(&PausedAnimFrame, sizeof(PausedAnimFrame), &written);
    if (hr < 0 || written != sizeof(PausedAnimFrame)) return E_FAIL;

    // Write packed bool flags (group 2).
    uint32 flags2 = 0;
    if (Reverse)   flags2 |= 0x01;
    if (TimeToDie) flags2 |= 0x02;
    if (HasExtras) flags2 |= 0x04;
    hr = pStm->Write(&flags2, sizeof(flags2), &written);
    if (hr < 0 || written != sizeof(flags2)) return E_FAIL;

    hr = pStm->Write(&unknown_124, sizeof(unknown_124), &written);
    if (hr < 0 || written != sizeof(unknown_124)) return E_FAIL;
    hr = pStm->Write(&TranslucencyLevel, sizeof(TranslucencyLevel), &written);
    if (hr < 0 || written != sizeof(TranslucencyLevel)) return E_FAIL;

    // Write attached bullet index.
    int32 bulletIdx = -1;
    if (AttachedBullet && BulletClass::Array) {
        for (int32 i = 0; i < BulletClass::Array->Count; ++i) {
            if ((*BulletClass::Array)[i] == AttachedBullet) {
                bulletIdx = i;
                break;
            }
        }
    }
    hr = pStm->Write(&bulletIdx, sizeof(bulletIdx), &written);
    if (hr < 0 || written != sizeof(bulletIdx)) return E_FAIL;

    // Write owner house index.
    int32 ownerIdx = Owner ? Owner->ArrayIndex : -1;
    hr = pStm->Write(&ownerIdx, sizeof(ownerIdx), &written);
    if (hr < 0 || written != sizeof(ownerIdx)) return E_FAIL;

    hr = pStm->Write(&LoopDelay, sizeof(LoopDelay), &written);
    if (hr < 0 || written != sizeof(LoopDelay)) return E_FAIL;
    hr = pStm->Write(&Accum, sizeof(Accum), &written);
    if (hr < 0 || written != sizeof(Accum)) return E_FAIL;

    uint32 animFlagsVal = static_cast<uint32>(AnimFlags);
    hr = pStm->Write(&animFlagsVal, sizeof(animFlagsVal), &written);
    if (hr < 0 || written != sizeof(animFlagsVal)) return E_FAIL;

    hr = pStm->Write(&RemainingIterations, sizeof(RemainingIterations), &written);
    if (hr < 0 || written != sizeof(RemainingIterations)) return E_FAIL;
    hr = pStm->Write(&unknown_196, sizeof(unknown_196), &written);
    if (hr < 0 || written != sizeof(unknown_196)) return E_FAIL;
    hr = pStm->Write(&unknown_197, sizeof(unknown_197), &written);
    if (hr < 0 || written != sizeof(unknown_197)) return E_FAIL;

    // Write packed bool flags (group 3).
    uint32 flags3 = 0;
    if (IsPlaying_)       flags3 |= 0x00000001;
    if (IsFogged)         flags3 |= 0x00000002;
    if (FlamingGuyExpire) flags3 |= 0x00000004;
    if (UnableToContinue) flags3 |= 0x00000008;
    if (SkipProcessOnce)  flags3 |= 0x00000010;
    if (Invisible)        flags3 |= 0x00000020;
    if (PowerOff)         flags3 |= 0x00000040;
    if (unused_19F)       flags3 |= 0x00000080;
    if (IsFlaming)        flags3 |= 0x00000100;
    if (IsNuke)           flags3 |= 0x00000200;
    if (IsIonCannon)      flags3 |= 0x00000400;
    if (IsLooping_)       flags3 |= 0x00000800;
    hr = pStm->Write(&flags3, sizeof(flags3), &written);
    if (hr < 0 || written != sizeof(flags3)) return E_FAIL;

    // Write bounce data.
    hr = pStm->Write(&Bounce.Velocity.X, sizeof(Bounce.Velocity.X), &written);
    if (hr < 0 || written != sizeof(Bounce.Velocity.X)) return E_FAIL;
    hr = pStm->Write(&Bounce.Velocity.Y, sizeof(Bounce.Velocity.Y), &written);
    if (hr < 0 || written != sizeof(Bounce.Velocity.Y)) return E_FAIL;
    hr = pStm->Write(&Bounce.Velocity.Z, sizeof(Bounce.Velocity.Z), &written);
    if (hr < 0 || written != sizeof(Bounce.Velocity.Z)) return E_FAIL;
    hr = pStm->Write(&Bounce.Elasticity, sizeof(Bounce.Elasticity), &written);
    if (hr < 0 || written != sizeof(Bounce.Elasticity)) return E_FAIL;
    hr = pStm->Write(&Bounce.Bounces, sizeof(Bounce.Bounces), &written);
    if (hr < 0 || written != sizeof(Bounce.Bounces)) return E_FAIL;
    hr = pStm->Write(&Bounce.MaxBounces, sizeof(Bounce.MaxBounces), &written);
    if (hr < 0 || written != sizeof(Bounce.MaxBounces)) return E_FAIL;

    uint32 bounceFlags = 0;
    if (Bounce.IsActive) bounceFlags |= 0x01;
    hr = pStm->Write(&bounceFlags, sizeof(bounceFlags), &written);
    if (hr < 0 || written != sizeof(bounceFlags)) return E_FAIL;

    hr = pStm->Write(&TrailerCount, sizeof(TrailerCount), &written);
    if (hr < 0 || written != sizeof(TrailerCount)) return E_FAIL;
    hr = pStm->Write(&TrailerTimer, sizeof(TrailerTimer), &written);
    if (hr < 0 || written != sizeof(TrailerTimer)) return E_FAIL;
    hr = pStm->Write(&LightTimer, sizeof(LightTimer), &written);
    if (hr < 0 || written != sizeof(LightTimer)) return E_FAIL;
    hr = pStm->Write(&SoundTimer, sizeof(SoundTimer), &written);
    if (hr < 0 || written != sizeof(SoundTimer)) return E_FAIL;

    // Write packed bool flags (group 4).
    uint32 flags4 = 0;
    if (ScorchCreated)    flags4 |= 0x01;
    if (CraterCreated)    flags4 |= 0x02;
    if (InfantryCreated)  flags4 |= 0x04;
    if (TiberiumCreated)  flags4 |= 0x08;
    if (ChainAnimCreated) flags4 |= 0x10;
    hr = pStm->Write(&flags4, sizeof(flags4), &written);
    if (hr < 0 || written != sizeof(flags4)) return E_FAIL;

    hr = pStm->Write(PaletteName, sizeof(PaletteName), &written);
    if (hr < 0 || written != sizeof(PaletteName)) return E_FAIL;

    return S_OK;
}

// ============================================================================
// AbstractClass
// ============================================================================

AbstractType AnimClass::WhatAmI() const {
    return AbstractType::Anim;
}

int32 AnimClass::Size() const {
    return sizeof(AnimClass);
}

void AnimClass::Update() {
    if (TimeToDie) return;
    if (Paused) return;

    // Skip first frame processing
    if (SkipProcessOnce) {
        SkipProcessOnce = false;
        return;
    }

    if (UnableToContinue) {
        TimeToDie = true;
        return;
    }

    UpdateAnimation();
    UpdateBounce();
    UpdateExtras();
    ApplyDamage();
    UpdateSound();
    UpdateLight();
    UpdateTrailer();
    CheckFlamingGuy();
    ProcessTiberium();
    ProcessVeins();
    ProcessSpawns();
    ProcessScorch();
    ProcessCrater();

    // Check if animation has finished
    if (Animation.IsAtEnd() && !IsPlaying_) {
        if (RemainingIterations > 0) {
            --RemainingIterations;
            Animation.Reset();
        } else {
            OnAnimationEnd();
        }
    }
}

void AnimClass::PointerExpired(AbstractClass* pAbstract, bool removed) {
    if (OwnerObject == static_cast<ObjectClass*>(pAbstract)) {
        OwnerObject = nullptr;
    }
}

// ============================================================================
// Animation extras
// ============================================================================

int32 AnimClass::AnimExtras() {
    if (HasExtras) {
        if (Type->IsMeteor) {
            // Rotate the animation based on movement direction
            int32 dx = Bounce.Velocity.X;
            int32 dy = Bounce.Velocity.Y;
            if (dx != 0 || dy != 0) {
                double angle = std::atan2(static_cast<double>(dy), static_cast<double>(dx));
                // Store rotation for rendering
                unknown_124 = static_cast<uint32>(angle * 256.0 / (2.0 * 3.141592653589793));
            }
        }
        if (Type->Bouncer) {
            UpdateBounce();
        }
    }
    return 0;
}

int32 AnimClass::GetEnd() const {
    if (!Type) return 0;
    return Type->End;
}

void AnimClass::SetOwnerObject(ObjectClass* pOwner) {
    OwnerObject = pOwner;
}

void AnimClass::Pause() {
    Paused = true;
    Unpaused = false;
    PausedAnimFrame = Animation.Value;
}

void AnimClass::Unpause() {
    Paused = false;
    Unpaused = true;
}

// ============================================================================
// Animation control
// ============================================================================

void AnimClass::Play() {
    IsPlaying_ = true;
    Paused = false;
    SkipProcessOnce = true;
    Animation.Value = Type ? Type->Start : 0;
}

void AnimClass::Stop() {
    IsPlaying_ = false;
    TimeToDie = true;
}

void AnimClass::SetFrame(int32 frame) {
    if (Type) {
        if (frame < 0) frame = 0;
        if (Type->End > 0 && frame > Type->End) frame = Type->End;
        Animation.Value = frame;
    }
}

void AnimClass::NextFrame() {
    Animation.Advance();
}

void AnimClass::MakeInfantry() {
    if (!Type || Type->MakeInfantry <= 0) return;
    if (InfantryCreated) return;
    InfantryCreated = true;

    // Create infantry at the animation's location
    CellStruct cell = CoordMath::CoordToCell(Location);
    HouseClass* spawnOwner = Owner;

    // If MakeInfantryOwner is specified, use that house
    if (Type->MakeInfantryOwner >= 0 && Type->MakeInfantryOwner < MAX_HOUSES) {
        spawnOwner = HouseClass::Array[Type->MakeInfantryOwner];
    }

    // InfantryTypeClass* infType = InfantryTypeClass::FindByIndex(Type->MakeInfantry - 1);
    // if (infType && spawnOwner) {
    //     InfantryClass* infantry = new InfantryClass(infType, spawnOwner);
    //     infantry->SetLocation(Location);
    //     infantry->Place(false);
    // }
}

void AnimClass::CreateExplosion() {
    if (!Type || !Type->Warhead) return;

    CoordStruct center = Location;
    int32 damage = static_cast<int32>(Type->Damage);
    int32 radius = Type->DamageRadius;

    if (damage > 0 && radius > 0) {
        // Apply damage in radius using the warhead
        // MapClass::DamageArea(center, damage, radius, Type->Warhead, Owner, true);
    }
}

void AnimClass::CreateFire() {
    CoordStruct pos = Location;
    CoordStruct vel(0, 0, 5);
    ParticleClass::CreateFireParticle(pos, vel);
}

void AnimClass::CreateSmoke() {
    CoordStruct pos = Location;
    CoordStruct vel(0, 0, 3);
    ParticleClass::CreateSmokeParticle(pos, vel);
}

void AnimClass::CreateSpark() {
    CoordStruct pos = Location;
    int32 vx = (std::rand() % 11 - 5) * LeptonsPerCell / 256;
    int32 vy = (std::rand() % 11 - 5) * LeptonsPerCell / 256;
    int32 vz = std::rand() % 10 + 5;
    CoordStruct vel(vx, vy, vz);
    ParticleClass::CreateSparkParticle(pos, vel);
}

void AnimClass::KillAnim() {
    TimeToDie = true;
}

void AnimClass::FireNuke() {
    if (!Type) return;

    // Nuke animation: create nuke explosion
    CreateExplosion();

    // Nuke flash effect
    // NukeFlash::FadeIn(60);

    // Apply massive damage with nuke warhead
    if (Type->Warhead && Type->Damage > 0) {
        int32 damage = static_cast<int32>(Type->Damage);
        int32 radius = Type->DamageRadius;
        // MapClass::DamageArea(Location, damage, radius, Type->Warhead, Owner, true);

        // Create radiation sites around the blast
        for (int32 i = 0; i < 8; ++i) {
            int32 angle = i * 45 + (std::rand() % 30);
            double rad = angle * 3.141592653589793 / 180.0;
            int32 dist = Type->DamageRadius * LeptonsPerCell * 3 / 4;
            CoordStruct radPos(
                Location.X + static_cast<int32>(std::cos(rad) * dist),
                Location.Y + static_cast<int32>(std::sin(rad) * dist),
                0
            );
            // RadSiteClass::CreateRadSite(radPos, 600, 100, Owner);
        }
    }
}

// ============================================================================
// Special anim creators
// ============================================================================

AnimClass* AnimClass::CreateNukeAnim(const CoordStruct& pos) {
    AnimTypeClass* type = AnimTypeClass::Find("NukeAnim");
    if (!type) type = AnimTypeClass::Find("NUKEBALL");
    AnimClass* anim = new AnimClass(type, pos, 0, 1, 0x600, 0, false);
    anim->IsNuke = true;
    return anim;
}

AnimClass* AnimClass::CreateIonCannonAnim(const CoordStruct& pos) {
    AnimTypeClass* type = AnimTypeClass::Find("IonCannonAnim");
    if (!type) type = AnimTypeClass::Find("IONBEAM");
    AnimClass* anim = new AnimClass(type, pos, 0, 1, 0x600, 0, false);
    anim->IsIonCannon = true;
    return anim;
}

AnimClass* AnimClass::CreateConYardAnim(const CoordStruct& pos) {
    AnimTypeClass* type = AnimTypeClass::Find("CONYARD");
    AnimClass* anim = new AnimClass(type, pos, 0, -1, 0x600, 0, false);
    return anim;
}

AnimClass* AnimClass::CreateWarFactoryAnim(const CoordStruct& pos) {
    AnimTypeClass* type = AnimTypeClass::Find("WARFACTORY");
    AnimClass* anim = new AnimClass(type, pos, 0, -1, 0x600, 0, false);
    return anim;
}

AnimClass* AnimClass::CreateExplosionAnim(const CoordStruct& pos, int32 damageLevel) {
    const char* animName = "EXPLOSION";
    if (damageLevel >= 3) animName = "EXPLOBIG";
    else if (damageLevel >= 2) animName = "EXPLOLRG";
    else if (damageLevel >= 1) animName = "EXPLOMED";

    AnimTypeClass* type = AnimTypeClass::Find(animName);
    if (!type) type = AnimTypeClass::Find("EXPLOSION");
    AnimClass* anim = new AnimClass(type, pos, 0, 1, 0x600, 0, false);
    return anim;
}

AnimClass* AnimClass::CreateFireAnim(const CoordStruct& pos) {
    AnimTypeClass* type = AnimTypeClass::Find("FIRE1");
    if (!type) type = AnimTypeClass::Find("INITFIRE");
    AnimClass* anim = new AnimClass(type, pos, 0, 1, 0x600, 0, false);
    return anim;
}

AnimClass* AnimClass::CreateSmokeAnim(const CoordStruct& pos) {
    AnimTypeClass* type = AnimTypeClass::Find("SMOKE1");
    AnimClass* anim = new AnimClass(type, pos, 0, 1, 0x600, 0, false);
    return anim;
}

// ============================================================================
// Internal update methods
// ============================================================================

void AnimClass::UpdateAnimation() {
    if (!IsPlaying_) return;

    if (LoopDelay > 0) {
        --LoopDelay;
        return;
    }

    Animation.Advance();

    // Handle looping
    if (Animation.HasLooped()) {
        if (Animation.CurrentLoop < Animation.LoopCount || Animation.LoopCount < 0) {
            Animation.Value = Animation.LoopStart;
            Animation.CurrentLoop++;
            if (Type) {
                LoopDelay = Type->RandomLoopDelay.GetRandom();
            }

            // Loop-related events
            OnLoop();
        }
    }

    // Handle ping-pong
    if (Type && Type->PingPong && Animation.IsAtEnd()) {
        Reverse = !Reverse;
        if (Reverse) {
            Animation.Value = Animation.End;
            Animation.Rate = -Animation.Rate;
        } else {
            Animation.Value = Animation.Start;
            Animation.Rate = -Animation.Rate;
        }
    }

    // Handle reverse
    if (Reverse && Animation.Value <= Animation.Start) {
        Animation.Value = Animation.Start;
        if (!IsLooping_) {
            IsPlaying_ = false;
        }
    }
}

void AnimClass::UpdateBounce() {
    if (!HasExtras) return;
    if (!Type->Bouncer && !Type->IsMeteor) return;

    if (Bounce.IsActive) {
        Bounce.Update();
        Location.X += Bounce.Velocity.X;
        Location.Y += Bounce.Velocity.Y;
        Location.Z += Bounce.Velocity.Z;

        if (Location.Z <= 0) {
            Location.Z = 0;
            Bounce.Velocity.Z = -static_cast<int32>(Bounce.Velocity.Z * Bounce.Elasticity);
            ++Bounce.Bounces;

            if (Bounce.Bounces >= Bounce.MaxBounces) {
                Bounce.IsActive = false;
                Bounce.Velocity = CoordStruct(0, 0, 0);
            }
        }
    }
}

void AnimClass::UpdateExtras() {
    if (HasExtras) {
        AnimExtras();
    }
}

void AnimClass::UpdateSound() {
    if (!Type) return;

    // Play report sound on first active frame
    if (SoundTimer == 1 && Type->Report >= 0) {
        // VocClass::PlayGlobal(Type->Report, 0x2000, 1.0f);
        SoundTimer = 0;
    }
}

void AnimClass::UpdateLight() {
    if (!Type || LightTimer <= 0) return;

    --LightTimer;

    if (LightTimer == 0 && Type->LightFlashFrames > 0) {
        // Create a light flash at the animation position
        // TacticalClass::CreateLightFlash(
        //     Location, Type->LightSize, Type->LightIntensity,
        //     Type->LightVisibility,
        //     Type->LightRedTint, Type->LightGreenTint, Type->LightBlueTint
        // );
        LightTimer = Type->LightFlashFrames;
    }
}

void AnimClass::UpdateTrailer() {
    if (!Type || !Type->TrailerAnim) return;
    if (Type->TrailerSeperation <= 0) return;

    --TrailerTimer;

    if (TrailerTimer <= 0) {
        // Spawn a trailer animation behind this one
        CoordStruct trailerPos = Location;
        // Offset slightly behind the main animation
        trailerPos.X -= 10;
        trailerPos.Y -= 10;

        new AnimClass(Type->TrailerAnim, trailerPos, 0, 1, 0x600, 0, false);
        ++TrailerCount;

        TrailerTimer = Type->TrailerSeperation;
    }
}

void AnimClass::ApplyDamage() {
    if (!Type || Type->Damage <= 0.0) return;
    if (Type->DamageRadius <= 0) return;

    // Accumulate fractional damage
    Accum += Type->Damage;
    if (Accum < 1.0) return;

    int32 damageToApply = static_cast<int32>(Accum);
    Accum -= static_cast<double>(damageToApply);

    if (Type->Warhead && damageToApply > 0) {
        // MapClass::DamageArea(Location, damageToApply, Type->DamageRadius, Type->Warhead, Owner, true);
    }
}

void AnimClass::CheckFlamingGuy() {
    if (!Type || !Type->IsFlamingGuy) return;

    if (FlamingGuyRetries < 7) {
        // Pick a new random destination if needed
        if (FlamingGuyCoords.X == 0 && FlamingGuyCoords.Y == 0) {
            FlamingGuyCoords.X = Location.X + (std::rand() % 1001 - 500) * LeptonsPerCell / 256;
            FlamingGuyCoords.Y = Location.Y + (std::rand() % 1001 - 500) * LeptonsPerCell / 256;
            ++FlamingGuyRetries;
        }

        // Move toward destination
        int32 dx = FlamingGuyCoords.X - Location.X;
        int32 dy = FlamingGuyCoords.Y - Location.Y;
        int32 dist = static_cast<int32>(std::sqrt(static_cast<double>(dx*dx + dy*dy)));

        if (dist < 50) {
            FlamingGuyCoords = CoordStruct(0, 0, 0);
        } else if (dist > 0) {
            int32 speed = 50;
            Location.X += dx * speed / dist;
            Location.Y += dy * speed / dist;
        }
    }

    if (FlamingGuyExpire) {
        TimeToDie = true;
    }
}

void AnimClass::ProcessTiberium() {
    if (!Type) return;
    if (!Type->TiberiumChainReaction && !Type->IsTiberium && !Type->IsAnimatedTiberium) return;
    if (TiberiumCreated) return;

    TiberiumCreated = true;

    // Check if on a tiberium-bearing cell
    CellStruct cell = CoordMath::CoordToCell(Location);
    // CellClass* pCell = MapClass::GetCell(cell);
    // if (pCell && pCell->HasTiberium()) {
    //     if (Type->TiberiumChainReaction) {
    //         // Trigger chain reaction on adjacent tiberium cells
    //         for (int32 dx = -1; dx <= 1; ++dx) {
    //             for (int32 dy = -1; dy <= 1; ++dy) {
    //                 if (dx == 0 && dy == 0) continue;
    //                 CellStruct adjCell(cell.X + dx, cell.Y + dy);
    //                 // Check adjacent cell for tiberium and spread chain
    //             }
    //         }
    //     }
    //     if (Type->TiberiumSpreadRadius > 0) {
    //         // Spread tiberium growth in radius
    //     }
    // }
}

void AnimClass::ProcessVeins() {
    if (!Type || !Type->IsVeins) return;

    // Vein growth logic: spread to adjacent cells
    CellStruct cell = CoordMath::CoordToCell(Location);

    // Try to spread to a random adjacent cell
    static const int32 offsetX[8] = { -1, 0, 1, -1, 1, -1, 0, 1 };
    static const int32 offsetY[8] = { -1, -1, -1, 0, 0, 1, 1, 1 };

    int32 dir = std::rand() % 8;
    CellStruct targetCell(cell.X + offsetX[dir], cell.Y + offsetY[dir]);

    // Check if target cell is valid for vein growth
    // CellClass* pTargetCell = MapClass::GetCell(targetCell);
    // if (pTargetCell && pTargetCell->CanGrowVeins()) {
    //     pTargetCell->SetVeins(true);
    // }
}

void AnimClass::ProcessSpawns() {
    if (!Type) return;
    if (Type->SpawnCount <= 0) return;
    if (!Type->Spawns) return;

    // Spawn sub-animations at random intervals
    if (Animation.Value % 3 == 0) { // Every 3 frames
        int32 spawnCount = Type->SpawnCount;
        for (int32 i = 0; i < spawnCount; ++i) {
            CoordStruct spawnPos = Location;
            // Random offset within radius
            spawnPos.X += (std::rand() % 201 - 100) * LeptonsPerCell / 256;
            spawnPos.Y += (std::rand() % 201 - 100) * LeptonsPerCell / 256;

            AnimClass* spawnAnim = new AnimClass(Type->Spawns, spawnPos, 0, 1, 0x600, 0, false);
            spawnAnim->Owner = Owner;
        }
    }
}

void AnimClass::ProcessScorch() {
    if (!Type) return;
    if (!Type->Scorch) return;
    if (ScorchCreated) return;

    // Only create scorch when animation is nearly done
    if (Animation.Value < Animation.End / 2) return;

    ScorchCreated = true;
    CellStruct cell = CoordMath::CoordToCell(Location);

    // Create scorch mark on the cell
    // SmudgeClass::CreateSmudge(cell, SmudgeType::Scorch);
}

void AnimClass::ProcessCrater() {
    if (!Type) return;
    if (!Type->Crater && !Type->ForceBigCraters) return;
    if (CraterCreated) return;

    // Only create crater when animation is nearly done
    if (Animation.Value < Animation.End / 2) return;

    CraterCreated = true;
    CellStruct cell = CoordMath::CoordToCell(Location);

    // Create crater on the cell
    if (Type->ForceBigCraters) {
        // SmudgeClass::CreateSmudge(cell, SmudgeType::BigCrater);
    } else {
        // SmudgeClass::CreateSmudge(cell, SmudgeType::Crater);
    }
}

void AnimClass::OnAnimationEnd() {
    if (!Type) return;

    // Play stop sound
    if (Type->StopSound >= 0) {
        // VocClass::PlayGlobal(Type->StopSound, 0x2000, 1.0f);
    }

    // Create expire animation
    if (Type->ExpireAnim) {
        new AnimClass(Type->ExpireAnim, Location, 0, 1, 0x600, 0, false);
    }

    // Spawn infantry if applicable
    if (Type->MakeInfantry > 0 && !InfantryCreated) {
        InfantryCreated = true;
        MakeInfantry();
    }

    // Create scorch
    if (Type->Scorch && !ScorchCreated) {
        ScorchCreated = true;
        ProcessScorch();
    }

    // Create crater
    if ((Type->Crater || Type->ForceBigCraters) && !CraterCreated) {
        CraterCreated = true;
        ProcessCrater();
    }

    // Play big grey animation
    if (Type->IsBigGrey) {
        // BigGrey screen flash effect
        // ScreenClass::FlashGrey(30);
    }

    TimeToDie = true;
}

void AnimClass::OnLoop() {
    if (!Type) return;

    // On loop, play report sound again if configured
    if (Type->Report >= 0 && SoundTimer == 0) {
        // VocClass::PlayGlobal(Type->Report, 0x2000, 1.0f);
    }

    // Spawn particles on loop
    if (Type->NumParticles > 0) {
        for (int32 i = 0; i < Type->NumParticles; ++i) {
            CoordStruct particlePos = Location;
            particlePos.X += (std::rand() % 101 - 50) * LeptonsPerCell / 256;
            particlePos.Y += (std::rand() % 101 - 50) * LeptonsPerCell / 256;
            // ParticleClass::CreateSparkParticle(particlePos, CoordStruct(0, 0, 5));
        }
    }
}

// ============================================================================
// Rendering helpers
// ============================================================================

void AnimClass::Render(bool isometric) {
    if (!Type || Invisible || TimeToDie) return;

    int32 frame = Animation.Value;
    if (frame < 0) frame = 0;
    if (Type->End > 0 && frame > Type->End) frame = Type->End;

    // Get the SHP surface
    SHPStruct* pShape = Type->AnimShape;
    if (!pShape) return;

    // Calculate draw position
    Point2D drawPos = ConvertCoordToDrawPos();

    // Draw shadow first
    if (Type->Shadow && Type->ShadowShape) {
        // Draw SHP frame with shadow flags
        // DS::DrawSHPFrame(Type->ShadowShape, frame, drawPos, BlitterFlags::Shadow);
    }

    // Draw main animation frame
    BlitterFlags drawFlags = AnimFlags;
    if (Type->Translucent || TranslucencyLevel > 0) {
        drawFlags = drawFlags | BlitterFlags::Translucent;
    }

    if (isometric) {
        // Isometric draw: adjust Y position for Z
        drawPos.Y -= (Location.Z / LevelHeight);
    }

    // DS::DrawSHPFrame(pShape, frame, drawPos, drawFlags);

    // Draw tiled if configured
    if (Type->Tiled) {
        // Draw the animation tiled across the cell
        // CellStruct cell = CoordMath::CoordToCell(Location);
        // for (int32 x = 0; x < 3; ++x) {
        //     for (int32 y = 0; y < 3; ++y) {
        //         Point2D tilePos = drawPos;
        //         tilePos.X += x * 60;
        //         tilePos.Y += y * 30;
        //         DS::DrawSHPFrame(pShape, frame, tilePos, drawFlags);
        //     }
        // }
    }
}

Point2D AnimClass::ConvertCoordToDrawPos() const {
    // Convert world coordinates to isometric screen coordinates
    // Standard isometric transform:
    //   screenX = (worldX - worldY) * (CellWidthInPixels / 2) / LeptonsPerCell
    //   screenY = (worldX + worldY) * (CellHeightInPixels / 2) / LeptonsPerCell

    int32 screenX = (Location.X - Location.Y) * (CellWidthInPixels / 2) / LeptonsPerCell;
    int32 screenY = (Location.X + Location.Y) * (CellHeightInPixels / 2) / LeptonsPerCell;

    return Point2D(screenX, screenY);
}

int32 AnimClass::GetDrawLayer() const {
    if (!Type) return 0;
    return static_cast<int32>(Type->Layer_);
}

int32 AnimClass::GetYSort() const {
    if (!Type) return Location.Y;
    return Location.Y + YSortAdjust;
}

int32 AnimClass::GetZAdjust() const {
    return ZAdjust;
}

bool AnimClass::IsVisible() const {
    return !Invisible && !TimeToDie && !IsFogged;
}

bool AnimClass::ShouldRemoveInFog() const {
    if (!Type) return false;
    return Type->ShouldFogRemove;
}

// ============================================================================
// Static utility
// ============================================================================

AnimClass* AnimClass::FindByID(const char* id) {
    if (!Array || !id) return nullptr;
    for (int32 i = 0; i < Array->Count; ++i) {
        AnimClass* anim = (*Array)[i];
        if (anim && anim->Type && !_strcmpi(anim->Type->get_ID(), id)) {
            return anim;
        }
    }
    return nullptr;
}

int32 AnimClass::GetActiveCount() {
    if (!Array) return 0;
    return Array->Count;
}

void AnimClass::RemoveAll() {
    if (!Array) return;
    while (Array->Count > 0) {
        AnimClass* anim = (*Array)[Array->Count - 1];
        if (anim) {
            GameDelete(anim);
        }
    }
}

void AnimClass::UpdateAll() {
    if (!Array) return;
    // Iterate backwards to allow safe removal
    for (int32 i = Array->Count - 1; i >= 0; --i) {
        AnimClass* anim = (*Array)[i];
        if (anim) {
            if (anim->TimeToDie) {
                GameDelete(anim);
            } else {
                anim->Update();
            }
        }
    }
}

void AnimClass::RenderAll() {
    if (!Array) return;
    for (int32 i = 0; i < Array->Count; ++i) {
        AnimClass* anim = (*Array)[i];
        if (anim && !anim->TimeToDie && anim->IsVisible()) {
            anim->Render(true);
        }
    }
}