#include "AnimTypeClass.h"
#include "AnimClass.h"
#include "../INI/INIClass.h"
#include "../Abstract/ObjectClass.h"
#include "../Abstract/OverlayTypeClass.h"
#include "../Combat/WarheadTypeClass.h"
#include "../Rendering/Surface.h"
#include "../FileFormats/SHP.h"
#include "../Game/Game.h"
#include "../Rules/RulesClass.h"

#include <cstring>
#include <cstdlib>

// ============================================================================
// Static members
// ============================================================================

DynamicVectorClass<AnimTypeClass*>* AnimTypeClass::Array = nullptr;

// ============================================================================
// Static lookup
// ============================================================================

AnimTypeClass* AnimTypeClass::Find(const char* pID) {
    if (!Array || !pID) return nullptr;
    for (int32 i = 0; i < Array->Count; ++i) {
        AnimTypeClass* item = (*Array)[i];
        if (item && !_strcmpi(item->get_ID(), pID)) {
            return item;
        }
    }
    return nullptr;
}

AnimTypeClass* AnimTypeClass::FindByIndex(int32 index) {
    if (!Array || index < 0 || index >= Array->Count) return nullptr;
    return (*Array)[index];
}

int32 AnimTypeClass::GetCount() {
    if (!Array) return 0;
    return Array->Count;
}

// ============================================================================
// Construction / Destruction
// ============================================================================

AnimTypeClass::AnimTypeClass(const char* pID) noexcept :
    ObjectTypeClass(pID),
    MiddleFrameIndex(-1),
    MiddleFrameWidth(0),
    MiddleFrameHeight(0),
    unknown_2A4(0),
    Damage(0.0),
    Rate(0),
    Start(0),
    LoopStart(-1),
    LoopEnd(-1),
    End(0),
    LoopCount(0),
    Next(nullptr),
    SpawnsParticle(0),
    NumParticles(0),
    DetailLevel(0),
    TranslucencyDetailLevel(0),
    RandomLoopDelay(0, 0),
    RandomRate(0, 0),
    Translucency(0),
    Spawns(nullptr),
    SpawnCount(0),
    Report(-1),
    StopSound(-1),
    BounceAnim(nullptr),
    ExpireAnim(nullptr),
    TrailerAnim(nullptr),
    TrailerSeperation(0),
    Elasticity(0.0),
    MinZVel(0.0),
    unknown_double_320(0.0),
    MaxXYVel(0.0),
    Warhead(nullptr),
    DamageRadius(0),
    TiberiumSpawnType(nullptr),
    TiberiumSpreadRadius(0),
    YSortAdjust(0),
    YDrawOffset(0),
    ZAdjust(0),
    MakeInfantry(0),
    RunningFrames(0),
    IsFlamingGuy(false),
    IsVeins(false),
    IsMeteor(false),
    TiberiumChainReaction(false),
    IsTiberium(false),
    HideIfNoOre(false),
    Bouncer(false),
    Tiled(false),
    ShouldUseCellDrawer(false),
    UseNormalLight(false),
    IsNuke(false),
    IsIonCannon(false),
    DemandLoad(false),
    FreeLoad(false),
    IsAnimatedTiberium(false),
    AltPalette(false),
    Normalized(false),
    Layer_(Layer::Ground),
    DoubleThick(false),
    Flat(false),
    Translucent(false),
    Scorch(false),
    Flamer(false),
    Crater(false),
    ForceBigCraters(false),
    Sticky(false),
    PingPong(false),
    Reverse(false),
    Shadow(false),
    PsiWarning(false),
    ShouldFogRemove(false),
    NumFrames(0),
    FrameRate(0),
    IsLooping(false),
    IsInvisible(false),
    IsFlameThrower(false),
    IsNormalized(false),
    IsBigGrey(false),
    IsParticle(false),
    IsRailgun(false),
    MakeInfantryOwner(-1),
    LightSize(0),
    LightIntensity(0.0),
    LightVisibility(0),
    LightRedTint(0.0),
    LightGreenTint(0.0),
    LightBlueTint(0.0),
    LightFlashFrames(0),
    AnimShape(nullptr),
    ShadowShape(nullptr)
{
    std::memset(AnimationName, 0, sizeof(AnimationName));
}

AnimTypeClass::~AnimTypeClass() {}

// ============================================================================
// IPersist
// ============================================================================

HRESULT AnimTypeClass::GetClassID(CLSID* pClassID) {
    if (!pClassID) return E_POINTER;
    pClassID->Data1 = static_cast<uint32>(AbstractType::AnimType);
    return S_OK;
}

HRESULT AnimTypeClass::Load(IStream* pStm) {
    if (!pStm) return E_POINTER;

    ULONG read = 0;
    HRESULT hr = S_OK;

    // Read ID
    char idBuf[0x18];
    hr = pStm->Read(idBuf, sizeof(idBuf), &read);
    if (hr < 0 || read != sizeof(idBuf)) return E_FAIL;
    std::memcpy(ID, idBuf, sizeof(ID));
    ID[sizeof(ID) - 1] = '\0';

    // Read scalar fields
    hr = pStm->Read(&MiddleFrameIndex, sizeof(MiddleFrameIndex), &read);
    if (hr < 0 || read != sizeof(MiddleFrameIndex)) return E_FAIL;
    hr = pStm->Read(&MiddleFrameWidth, sizeof(MiddleFrameWidth), &read);
    if (hr < 0 || read != sizeof(MiddleFrameWidth)) return E_FAIL;
    hr = pStm->Read(&MiddleFrameHeight, sizeof(MiddleFrameHeight), &read);
    if (hr < 0 || read != sizeof(MiddleFrameHeight)) return E_FAIL;
    hr = pStm->Read(&unknown_2A4, sizeof(unknown_2A4), &read);
    if (hr < 0 || read != sizeof(unknown_2A4)) return E_FAIL;
    hr = pStm->Read(&Damage, sizeof(Damage), &read);
    if (hr < 0 || read != sizeof(Damage)) return E_FAIL;
    hr = pStm->Read(&Rate, sizeof(Rate), &read);
    if (hr < 0 || read != sizeof(Rate)) return E_FAIL;
    hr = pStm->Read(&Start, sizeof(Start), &read);
    if (hr < 0 || read != sizeof(Start)) return E_FAIL;
    hr = pStm->Read(&LoopStart, sizeof(LoopStart), &read);
    if (hr < 0 || read != sizeof(LoopStart)) return E_FAIL;
    hr = pStm->Read(&LoopEnd, sizeof(LoopEnd), &read);
    if (hr < 0 || read != sizeof(LoopEnd)) return E_FAIL;
    hr = pStm->Read(&End, sizeof(End), &read);
    if (hr < 0 || read != sizeof(End)) return E_FAIL;
    hr = pStm->Read(&LoopCount, sizeof(LoopCount), &read);
    if (hr < 0 || read != sizeof(LoopCount)) return E_FAIL;

    // Read Next anim type ID and resolve.
    char nextID[0x18];
    hr = pStm->Read(nextID, sizeof(nextID), &read);
    if (hr < 0 || read != sizeof(nextID)) return E_FAIL;
    nextID[sizeof(nextID) - 1] = '\0';
    Next = nextID[0] ? AnimTypeClass::Find(nextID) : nullptr;

    hr = pStm->Read(&SpawnsParticle, sizeof(SpawnsParticle), &read);
    if (hr < 0 || read != sizeof(SpawnsParticle)) return E_FAIL;
    hr = pStm->Read(&NumParticles, sizeof(NumParticles), &read);
    if (hr < 0 || read != sizeof(NumParticles)) return E_FAIL;
    hr = pStm->Read(&DetailLevel, sizeof(DetailLevel), &read);
    if (hr < 0 || read != sizeof(DetailLevel)) return E_FAIL;
    hr = pStm->Read(&TranslucencyDetailLevel, sizeof(TranslucencyDetailLevel), &read);
    if (hr < 0 || read != sizeof(TranslucencyDetailLevel)) return E_FAIL;

    // Read RandomStruct fields.
    hr = pStm->Read(&RandomLoopDelay.Min, sizeof(RandomLoopDelay.Min), &read);
    if (hr < 0 || read != sizeof(RandomLoopDelay.Min)) return E_FAIL;
    hr = pStm->Read(&RandomLoopDelay.Max, sizeof(RandomLoopDelay.Max), &read);
    if (hr < 0 || read != sizeof(RandomLoopDelay.Max)) return E_FAIL;
    hr = pStm->Read(&RandomRate.Min, sizeof(RandomRate.Min), &read);
    if (hr < 0 || read != sizeof(RandomRate.Min)) return E_FAIL;
    hr = pStm->Read(&RandomRate.Max, sizeof(RandomRate.Max), &read);
    if (hr < 0 || read != sizeof(RandomRate.Max)) return E_FAIL;

    hr = pStm->Read(&Translucency, sizeof(Translucency), &read);
    if (hr < 0 || read != sizeof(Translucency)) return E_FAIL;

    // Read Spawns anim type ID and resolve.
    char spawnsID[0x18];
    hr = pStm->Read(spawnsID, sizeof(spawnsID), &read);
    if (hr < 0 || read != sizeof(spawnsID)) return E_FAIL;
    spawnsID[sizeof(spawnsID) - 1] = '\0';
    Spawns = spawnsID[0] ? AnimTypeClass::Find(spawnsID) : nullptr;

    hr = pStm->Read(&SpawnCount, sizeof(SpawnCount), &read);
    if (hr < 0 || read != sizeof(SpawnCount)) return E_FAIL;
    hr = pStm->Read(&Report, sizeof(Report), &read);
    if (hr < 0 || read != sizeof(Report)) return E_FAIL;
    hr = pStm->Read(&StopSound, sizeof(StopSound), &read);
    if (hr < 0 || read != sizeof(StopSound)) return E_FAIL;

    // Read BounceAnim ID and resolve.
    char bounceID[0x18];
    hr = pStm->Read(bounceID, sizeof(bounceID), &read);
    if (hr < 0 || read != sizeof(bounceID)) return E_FAIL;
    bounceID[sizeof(bounceID) - 1] = '\0';
    BounceAnim = bounceID[0] ? AnimTypeClass::Find(bounceID) : nullptr;

    // Read ExpireAnim ID and resolve.
    char expireID[0x18];
    hr = pStm->Read(expireID, sizeof(expireID), &read);
    if (hr < 0 || read != sizeof(expireID)) return E_FAIL;
    expireID[sizeof(expireID) - 1] = '\0';
    ExpireAnim = expireID[0] ? AnimTypeClass::Find(expireID) : nullptr;

    // Read TrailerAnim ID and resolve.
    char trailerID[0x18];
    hr = pStm->Read(trailerID, sizeof(trailerID), &read);
    if (hr < 0 || read != sizeof(trailerID)) return E_FAIL;
    trailerID[sizeof(trailerID) - 1] = '\0';
    TrailerAnim = trailerID[0] ? AnimTypeClass::Find(trailerID) : nullptr;

    hr = pStm->Read(&TrailerSeperation, sizeof(TrailerSeperation), &read);
    if (hr < 0 || read != sizeof(TrailerSeperation)) return E_FAIL;
    hr = pStm->Read(&Elasticity, sizeof(Elasticity), &read);
    if (hr < 0 || read != sizeof(Elasticity)) return E_FAIL;
    hr = pStm->Read(&MinZVel, sizeof(MinZVel), &read);
    if (hr < 0 || read != sizeof(MinZVel)) return E_FAIL;
    hr = pStm->Read(&unknown_double_320, sizeof(unknown_double_320), &read);
    if (hr < 0 || read != sizeof(unknown_double_320)) return E_FAIL;
    hr = pStm->Read(&MaxXYVel, sizeof(MaxXYVel), &read);
    if (hr < 0 || read != sizeof(MaxXYVel)) return E_FAIL;

    // Read Warhead ID and resolve.
    char warheadID[0x18];
    hr = pStm->Read(warheadID, sizeof(warheadID), &read);
    if (hr < 0 || read != sizeof(warheadID)) return E_FAIL;
    warheadID[sizeof(warheadID) - 1] = '\0';
    Warhead = warheadID[0] ? WarheadTypeClass::Find(warheadID) : nullptr;

    hr = pStm->Read(&DamageRadius, sizeof(DamageRadius), &read);
    if (hr < 0 || read != sizeof(DamageRadius)) return E_FAIL;

    // Read TiberiumSpawnType ID and resolve.
    char tiberiumID[0x18];
    hr = pStm->Read(tiberiumID, sizeof(tiberiumID), &read);
    if (hr < 0 || read != sizeof(tiberiumID)) return E_FAIL;
    tiberiumID[sizeof(tiberiumID) - 1] = '\0';
    TiberiumSpawnType = tiberiumID[0] ? OverlayTypeClass::Find(tiberiumID) : nullptr;

    hr = pStm->Read(&TiberiumSpreadRadius, sizeof(TiberiumSpreadRadius), &read);
    if (hr < 0 || read != sizeof(TiberiumSpreadRadius)) return E_FAIL;
    hr = pStm->Read(&YSortAdjust, sizeof(YSortAdjust), &read);
    if (hr < 0 || read != sizeof(YSortAdjust)) return E_FAIL;
    hr = pStm->Read(&YDrawOffset, sizeof(YDrawOffset), &read);
    if (hr < 0 || read != sizeof(YDrawOffset)) return E_FAIL;
    hr = pStm->Read(&ZAdjust, sizeof(ZAdjust), &read);
    if (hr < 0 || read != sizeof(ZAdjust)) return E_FAIL;
    hr = pStm->Read(&MakeInfantry, sizeof(MakeInfantry), &read);
    if (hr < 0 || read != sizeof(MakeInfantry)) return E_FAIL;
    hr = pStm->Read(&RunningFrames, sizeof(RunningFrames), &read);
    if (hr < 0 || read != sizeof(RunningFrames)) return E_FAIL;

    // Read packed bool flags (group 1)
    uint32 flags1 = 0;
    hr = pStm->Read(&flags1, sizeof(flags1), &read);
    if (hr < 0 || read != sizeof(flags1)) return E_FAIL;
    IsFlamingGuy           = (flags1 & 0x00000001) != 0;
    IsVeins                = (flags1 & 0x00000002) != 0;
    IsMeteor               = (flags1 & 0x00000004) != 0;
    TiberiumChainReaction  = (flags1 & 0x00000008) != 0;
    IsTiberium             = (flags1 & 0x00000010) != 0;
    HideIfNoOre            = (flags1 & 0x00000020) != 0;
    Bouncer                = (flags1 & 0x00000040) != 0;
    Tiled                  = (flags1 & 0x00000080) != 0;
    ShouldUseCellDrawer    = (flags1 & 0x00000100) != 0;
    UseNormalLight         = (flags1 & 0x00000200) != 0;
    IsNuke                 = (flags1 & 0x00000400) != 0;
    IsIonCannon            = (flags1 & 0x00000800) != 0;
    DemandLoad             = (flags1 & 0x00001000) != 0;
    FreeLoad               = (flags1 & 0x00002000) != 0;
    IsAnimatedTiberium     = (flags1 & 0x00004000) != 0;
    AltPalette             = (flags1 & 0x00008000) != 0;
    Normalized             = (flags1 & 0x00010000) != 0;

    // Read Layer enum as int32.
    int32 layerVal = 0;
    hr = pStm->Read(&layerVal, sizeof(layerVal), &read);
    if (hr < 0 || read != sizeof(layerVal)) return E_FAIL;
    Layer_ = static_cast<Layer>(layerVal);

    // Read packed bool flags (group 2)
    uint32 flags2 = 0;
    hr = pStm->Read(&flags2, sizeof(flags2), &read);
    if (hr < 0 || read != sizeof(flags2)) return E_FAIL;
    DoubleThick      = (flags2 & 0x00000001) != 0;
    Flat             = (flags2 & 0x00000002) != 0;
    Translucent      = (flags2 & 0x00000004) != 0;
    Scorch           = (flags2 & 0x00000008) != 0;
    Flamer           = (flags2 & 0x00000010) != 0;
    Crater           = (flags2 & 0x00000020) != 0;
    ForceBigCraters  = (flags2 & 0x00000040) != 0;
    Sticky           = (flags2 & 0x00000080) != 0;
    PingPong         = (flags2 & 0x00000100) != 0;
    Reverse          = (flags2 & 0x00000200) != 0;
    Shadow           = (flags2 & 0x00000400) != 0;
    PsiWarning       = (flags2 & 0x00000800) != 0;
    ShouldFogRemove  = (flags2 & 0x00001000) != 0;

    hr = pStm->Read(AnimationName, sizeof(AnimationName), &read);
    if (hr < 0 || read != sizeof(AnimationName)) return E_FAIL;
    hr = pStm->Read(&NumFrames, sizeof(NumFrames), &read);
    if (hr < 0 || read != sizeof(NumFrames)) return E_FAIL;
    hr = pStm->Read(&FrameRate, sizeof(FrameRate), &read);
    if (hr < 0 || read != sizeof(FrameRate)) return E_FAIL;

    // Read packed bool flags (group 3)
    uint32 flags3 = 0;
    hr = pStm->Read(&flags3, sizeof(flags3), &read);
    if (hr < 0 || read != sizeof(flags3)) return E_FAIL;
    IsLooping      = (flags3 & 0x01) != 0;
    IsInvisible    = (flags3 & 0x02) != 0;
    IsFlameThrower = (flags3 & 0x04) != 0;
    IsNormalized   = (flags3 & 0x08) != 0;
    IsBigGrey      = (flags3 & 0x10) != 0;
    IsParticle     = (flags3 & 0x20) != 0;
    IsRailgun      = (flags3 & 0x40) != 0;

    hr = pStm->Read(&MakeInfantryOwner, sizeof(MakeInfantryOwner), &read);
    if (hr < 0 || read != sizeof(MakeInfantryOwner)) return E_FAIL;
    hr = pStm->Read(&LightSize, sizeof(LightSize), &read);
    if (hr < 0 || read != sizeof(LightSize)) return E_FAIL;
    hr = pStm->Read(&LightIntensity, sizeof(LightIntensity), &read);
    if (hr < 0 || read != sizeof(LightIntensity)) return E_FAIL;
    hr = pStm->Read(&LightVisibility, sizeof(LightVisibility), &read);
    if (hr < 0 || read != sizeof(LightVisibility)) return E_FAIL;
    hr = pStm->Read(&LightRedTint, sizeof(LightRedTint), &read);
    if (hr < 0 || read != sizeof(LightRedTint)) return E_FAIL;
    hr = pStm->Read(&LightGreenTint, sizeof(LightGreenTint), &read);
    if (hr < 0 || read != sizeof(LightGreenTint)) return E_FAIL;
    hr = pStm->Read(&LightBlueTint, sizeof(LightBlueTint), &read);
    if (hr < 0 || read != sizeof(LightBlueTint)) return E_FAIL;
    hr = pStm->Read(&LightFlashFrames, sizeof(LightFlashFrames), &read);
    if (hr < 0 || read != sizeof(LightFlashFrames)) return E_FAIL;

    // Read shape pointers as indices (reloaded on demand, not restored).
    int32 animShapeIdx = -1;
    hr = pStm->Read(&animShapeIdx, sizeof(animShapeIdx), &read);
    if (hr < 0 || read != sizeof(animShapeIdx)) return E_FAIL;
    AnimShape = nullptr;

    int32 shadowShapeIdx = -1;
    hr = pStm->Read(&shadowShapeIdx, sizeof(shadowShapeIdx), &read);
    if (hr < 0 || read != sizeof(shadowShapeIdx)) return E_FAIL;
    ShadowShape = nullptr;

    return S_OK;
}

HRESULT AnimTypeClass::Save(IStream* pStm, BOOL fClearDirty) {
    if (!pStm) return E_POINTER;

    ULONG written = 0;
    HRESULT hr = S_OK;

    // Write ID
    hr = pStm->Write(ID, sizeof(ID), &written);
    if (hr < 0 || written != sizeof(ID)) return E_FAIL;

    // Write scalar fields
    hr = pStm->Write(&MiddleFrameIndex, sizeof(MiddleFrameIndex), &written);
    if (hr < 0 || written != sizeof(MiddleFrameIndex)) return E_FAIL;
    hr = pStm->Write(&MiddleFrameWidth, sizeof(MiddleFrameWidth), &written);
    if (hr < 0 || written != sizeof(MiddleFrameWidth)) return E_FAIL;
    hr = pStm->Write(&MiddleFrameHeight, sizeof(MiddleFrameHeight), &written);
    if (hr < 0 || written != sizeof(MiddleFrameHeight)) return E_FAIL;
    hr = pStm->Write(&unknown_2A4, sizeof(unknown_2A4), &written);
    if (hr < 0 || written != sizeof(unknown_2A4)) return E_FAIL;
    hr = pStm->Write(&Damage, sizeof(Damage), &written);
    if (hr < 0 || written != sizeof(Damage)) return E_FAIL;
    hr = pStm->Write(&Rate, sizeof(Rate), &written);
    if (hr < 0 || written != sizeof(Rate)) return E_FAIL;
    hr = pStm->Write(&Start, sizeof(Start), &written);
    if (hr < 0 || written != sizeof(Start)) return E_FAIL;
    hr = pStm->Write(&LoopStart, sizeof(LoopStart), &written);
    if (hr < 0 || written != sizeof(LoopStart)) return E_FAIL;
    hr = pStm->Write(&LoopEnd, sizeof(LoopEnd), &written);
    if (hr < 0 || written != sizeof(LoopEnd)) return E_FAIL;
    hr = pStm->Write(&End, sizeof(End), &written);
    if (hr < 0 || written != sizeof(End)) return E_FAIL;
    hr = pStm->Write(&LoopCount, sizeof(LoopCount), &written);
    if (hr < 0 || written != sizeof(LoopCount)) return E_FAIL;

    // Write Next anim type ID.
    char nextID[0x18];
    std::memset(nextID, 0, sizeof(nextID));
    if (Next && Next->ID) {
        int32 j = 0;
        while (Next->ID[j] && j < static_cast<int32>(sizeof(nextID)) - 1) {
            nextID[j] = Next->ID[j]; ++j;
        }
    }
    hr = pStm->Write(nextID, sizeof(nextID), &written);
    if (hr < 0 || written != sizeof(nextID)) return E_FAIL;

    hr = pStm->Write(&SpawnsParticle, sizeof(SpawnsParticle), &written);
    if (hr < 0 || written != sizeof(SpawnsParticle)) return E_FAIL;
    hr = pStm->Write(&NumParticles, sizeof(NumParticles), &written);
    if (hr < 0 || written != sizeof(NumParticles)) return E_FAIL;
    hr = pStm->Write(&DetailLevel, sizeof(DetailLevel), &written);
    if (hr < 0 || written != sizeof(DetailLevel)) return E_FAIL;
    hr = pStm->Write(&TranslucencyDetailLevel, sizeof(TranslucencyDetailLevel), &written);
    if (hr < 0 || written != sizeof(TranslucencyDetailLevel)) return E_FAIL;

    // Write RandomStruct fields.
    hr = pStm->Write(&RandomLoopDelay.Min, sizeof(RandomLoopDelay.Min), &written);
    if (hr < 0 || written != sizeof(RandomLoopDelay.Min)) return E_FAIL;
    hr = pStm->Write(&RandomLoopDelay.Max, sizeof(RandomLoopDelay.Max), &written);
    if (hr < 0 || written != sizeof(RandomLoopDelay.Max)) return E_FAIL;
    hr = pStm->Write(&RandomRate.Min, sizeof(RandomRate.Min), &written);
    if (hr < 0 || written != sizeof(RandomRate.Min)) return E_FAIL;
    hr = pStm->Write(&RandomRate.Max, sizeof(RandomRate.Max), &written);
    if (hr < 0 || written != sizeof(RandomRate.Max)) return E_FAIL;

    hr = pStm->Write(&Translucency, sizeof(Translucency), &written);
    if (hr < 0 || written != sizeof(Translucency)) return E_FAIL;

    // Write Spawns anim type ID.
    char spawnsID[0x18];
    std::memset(spawnsID, 0, sizeof(spawnsID));
    if (Spawns && Spawns->ID) {
        int32 j = 0;
        while (Spawns->ID[j] && j < static_cast<int32>(sizeof(spawnsID)) - 1) {
            spawnsID[j] = Spawns->ID[j]; ++j;
        }
    }
    hr = pStm->Write(spawnsID, sizeof(spawnsID), &written);
    if (hr < 0 || written != sizeof(spawnsID)) return E_FAIL;

    hr = pStm->Write(&SpawnCount, sizeof(SpawnCount), &written);
    if (hr < 0 || written != sizeof(SpawnCount)) return E_FAIL;
    hr = pStm->Write(&Report, sizeof(Report), &written);
    if (hr < 0 || written != sizeof(Report)) return E_FAIL;
    hr = pStm->Write(&StopSound, sizeof(StopSound), &written);
    if (hr < 0 || written != sizeof(StopSound)) return E_FAIL;

    // Write BounceAnim ID.
    char bounceID[0x18];
    std::memset(bounceID, 0, sizeof(bounceID));
    if (BounceAnim && BounceAnim->ID) {
        int32 j = 0;
        while (BounceAnim->ID[j] && j < static_cast<int32>(sizeof(bounceID)) - 1) {
            bounceID[j] = BounceAnim->ID[j]; ++j;
        }
    }
    hr = pStm->Write(bounceID, sizeof(bounceID), &written);
    if (hr < 0 || written != sizeof(bounceID)) return E_FAIL;

    // Write ExpireAnim ID.
    char expireID[0x18];
    std::memset(expireID, 0, sizeof(expireID));
    if (ExpireAnim && ExpireAnim->ID) {
        int32 j = 0;
        while (ExpireAnim->ID[j] && j < static_cast<int32>(sizeof(expireID)) - 1) {
            expireID[j] = ExpireAnim->ID[j]; ++j;
        }
    }
    hr = pStm->Write(expireID, sizeof(expireID), &written);
    if (hr < 0 || written != sizeof(expireID)) return E_FAIL;

    // Write TrailerAnim ID.
    char trailerID[0x18];
    std::memset(trailerID, 0, sizeof(trailerID));
    if (TrailerAnim && TrailerAnim->ID) {
        int32 j = 0;
        while (TrailerAnim->ID[j] && j < static_cast<int32>(sizeof(trailerID)) - 1) {
            trailerID[j] = TrailerAnim->ID[j]; ++j;
        }
    }
    hr = pStm->Write(trailerID, sizeof(trailerID), &written);
    if (hr < 0 || written != sizeof(trailerID)) return E_FAIL;

    hr = pStm->Write(&TrailerSeperation, sizeof(TrailerSeperation), &written);
    if (hr < 0 || written != sizeof(TrailerSeperation)) return E_FAIL;
    hr = pStm->Write(&Elasticity, sizeof(Elasticity), &written);
    if (hr < 0 || written != sizeof(Elasticity)) return E_FAIL;
    hr = pStm->Write(&MinZVel, sizeof(MinZVel), &written);
    if (hr < 0 || written != sizeof(MinZVel)) return E_FAIL;
    hr = pStm->Write(&unknown_double_320, sizeof(unknown_double_320), &written);
    if (hr < 0 || written != sizeof(unknown_double_320)) return E_FAIL;
    hr = pStm->Write(&MaxXYVel, sizeof(MaxXYVel), &written);
    if (hr < 0 || written != sizeof(MaxXYVel)) return E_FAIL;

    // Write Warhead ID.
    char warheadID[0x18];
    std::memset(warheadID, 0, sizeof(warheadID));
    if (Warhead && Warhead->ID) {
        int32 j = 0;
        while (Warhead->ID[j] && j < static_cast<int32>(sizeof(warheadID)) - 1) {
            warheadID[j] = Warhead->ID[j]; ++j;
        }
    }
    hr = pStm->Write(warheadID, sizeof(warheadID), &written);
    if (hr < 0 || written != sizeof(warheadID)) return E_FAIL;

    hr = pStm->Write(&DamageRadius, sizeof(DamageRadius), &written);
    if (hr < 0 || written != sizeof(DamageRadius)) return E_FAIL;

    // Write TiberiumSpawnType ID.
    char tiberiumID[0x18];
    std::memset(tiberiumID, 0, sizeof(tiberiumID));
    if (TiberiumSpawnType && TiberiumSpawnType->ID) {
        int32 j = 0;
        while (TiberiumSpawnType->ID[j] && j < static_cast<int32>(sizeof(tiberiumID)) - 1) {
            tiberiumID[j] = TiberiumSpawnType->ID[j]; ++j;
        }
    }
    hr = pStm->Write(tiberiumID, sizeof(tiberiumID), &written);
    if (hr < 0 || written != sizeof(tiberiumID)) return E_FAIL;

    hr = pStm->Write(&TiberiumSpreadRadius, sizeof(TiberiumSpreadRadius), &written);
    if (hr < 0 || written != sizeof(TiberiumSpreadRadius)) return E_FAIL;
    hr = pStm->Write(&YSortAdjust, sizeof(YSortAdjust), &written);
    if (hr < 0 || written != sizeof(YSortAdjust)) return E_FAIL;
    hr = pStm->Write(&YDrawOffset, sizeof(YDrawOffset), &written);
    if (hr < 0 || written != sizeof(YDrawOffset)) return E_FAIL;
    hr = pStm->Write(&ZAdjust, sizeof(ZAdjust), &written);
    if (hr < 0 || written != sizeof(ZAdjust)) return E_FAIL;
    hr = pStm->Write(&MakeInfantry, sizeof(MakeInfantry), &written);
    if (hr < 0 || written != sizeof(MakeInfantry)) return E_FAIL;
    hr = pStm->Write(&RunningFrames, sizeof(RunningFrames), &written);
    if (hr < 0 || written != sizeof(RunningFrames)) return E_FAIL;

    // Write packed bool flags (group 1)
    uint32 flags1 = 0;
    if (IsFlamingGuy)          flags1 |= 0x00000001;
    if (IsVeins)               flags1 |= 0x00000002;
    if (IsMeteor)              flags1 |= 0x00000004;
    if (TiberiumChainReaction) flags1 |= 0x00000008;
    if (IsTiberium)            flags1 |= 0x00000010;
    if (HideIfNoOre)           flags1 |= 0x00000020;
    if (Bouncer)               flags1 |= 0x00000040;
    if (Tiled)                 flags1 |= 0x00000080;
    if (ShouldUseCellDrawer)   flags1 |= 0x00000100;
    if (UseNormalLight)        flags1 |= 0x00000200;
    if (IsNuke)                flags1 |= 0x00000400;
    if (IsIonCannon)           flags1 |= 0x00000800;
    if (DemandLoad)            flags1 |= 0x00001000;
    if (FreeLoad)              flags1 |= 0x00002000;
    if (IsAnimatedTiberium)    flags1 |= 0x00004000;
    if (AltPalette)            flags1 |= 0x00008000;
    if (Normalized)            flags1 |= 0x00010000;
    hr = pStm->Write(&flags1, sizeof(flags1), &written);
    if (hr < 0 || written != sizeof(flags1)) return E_FAIL;

    // Write Layer enum as int32.
    int32 layerVal = static_cast<int32>(Layer_);
    hr = pStm->Write(&layerVal, sizeof(layerVal), &written);
    if (hr < 0 || written != sizeof(layerVal)) return E_FAIL;

    // Write packed bool flags (group 2)
    uint32 flags2 = 0;
    if (DoubleThick)     flags2 |= 0x00000001;
    if (Flat)            flags2 |= 0x00000002;
    if (Translucent)     flags2 |= 0x00000004;
    if (Scorch)          flags2 |= 0x00000008;
    if (Flamer)          flags2 |= 0x00000010;
    if (Crater)          flags2 |= 0x00000020;
    if (ForceBigCraters) flags2 |= 0x00000040;
    if (Sticky)          flags2 |= 0x00000080;
    if (PingPong)        flags2 |= 0x00000100;
    if (Reverse)         flags2 |= 0x00000200;
    if (Shadow)          flags2 |= 0x00000400;
    if (PsiWarning)      flags2 |= 0x00000800;
    if (ShouldFogRemove) flags2 |= 0x00001000;
    hr = pStm->Write(&flags2, sizeof(flags2), &written);
    if (hr < 0 || written != sizeof(flags2)) return E_FAIL;

    hr = pStm->Write(AnimationName, sizeof(AnimationName), &written);
    if (hr < 0 || written != sizeof(AnimationName)) return E_FAIL;
    hr = pStm->Write(&NumFrames, sizeof(NumFrames), &written);
    if (hr < 0 || written != sizeof(NumFrames)) return E_FAIL;
    hr = pStm->Write(&FrameRate, sizeof(FrameRate), &written);
    if (hr < 0 || written != sizeof(FrameRate)) return E_FAIL;

    // Write packed bool flags (group 3)
    uint32 flags3 = 0;
    if (IsLooping)      flags3 |= 0x01;
    if (IsInvisible)    flags3 |= 0x02;
    if (IsFlameThrower) flags3 |= 0x04;
    if (IsNormalized)   flags3 |= 0x08;
    if (IsBigGrey)      flags3 |= 0x10;
    if (IsParticle)     flags3 |= 0x20;
    if (IsRailgun)      flags3 |= 0x40;
    hr = pStm->Write(&flags3, sizeof(flags3), &written);
    if (hr < 0 || written != sizeof(flags3)) return E_FAIL;

    hr = pStm->Write(&MakeInfantryOwner, sizeof(MakeInfantryOwner), &written);
    if (hr < 0 || written != sizeof(MakeInfantryOwner)) return E_FAIL;
    hr = pStm->Write(&LightSize, sizeof(LightSize), &written);
    if (hr < 0 || written != sizeof(LightSize)) return E_FAIL;
    hr = pStm->Write(&LightIntensity, sizeof(LightIntensity), &written);
    if (hr < 0 || written != sizeof(LightIntensity)) return E_FAIL;
    hr = pStm->Write(&LightVisibility, sizeof(LightVisibility), &written);
    if (hr < 0 || written != sizeof(LightVisibility)) return E_FAIL;
    hr = pStm->Write(&LightRedTint, sizeof(LightRedTint), &written);
    if (hr < 0 || written != sizeof(LightRedTint)) return E_FAIL;
    hr = pStm->Write(&LightGreenTint, sizeof(LightGreenTint), &written);
    if (hr < 0 || written != sizeof(LightGreenTint)) return E_FAIL;
    hr = pStm->Write(&LightBlueTint, sizeof(LightBlueTint), &written);
    if (hr < 0 || written != sizeof(LightBlueTint)) return E_FAIL;
    hr = pStm->Write(&LightFlashFrames, sizeof(LightFlashFrames), &written);
    if (hr < 0 || written != sizeof(LightFlashFrames)) return E_FAIL;

    // Write shape pointers as indices (rendering resources, reloaded on demand).
    int32 animShapeIdx = AnimShape ? 0 : -1;
    hr = pStm->Write(&animShapeIdx, sizeof(animShapeIdx), &written);
    if (hr < 0 || written != sizeof(animShapeIdx)) return E_FAIL;

    int32 shadowShapeIdx = ShadowShape ? 0 : -1;
    hr = pStm->Write(&shadowShapeIdx, sizeof(shadowShapeIdx), &written);
    if (hr < 0 || written != sizeof(shadowShapeIdx)) return E_FAIL;

    return S_OK;
}

// ============================================================================
// AbstractClass
// ============================================================================

AbstractType AnimTypeClass::WhatAmI() const {
    return AbstractType::AnimType;
}

int32 AnimTypeClass::Size() const {
    return sizeof(AnimTypeClass);
}

// ============================================================================
// ObjectTypeClass
// ============================================================================

bool AnimTypeClass::SpawnAtMapCoords(CellStruct* pMapCoords, HouseClass* pOwner) {
    if (!pMapCoords) return false;
    // Animations are spawned at specific coordinates, not at map coords
    // This is handled by the AnimClass constructor
    return false;
}

ObjectClass* AnimTypeClass::CreateObject(HouseClass* pOwner) {
    // Factory method: instantiate an AnimClass from this type definition
    // and return it as an ObjectClass pointer for generic object handling.
    // The animation is created at the origin (0,0,0); the caller is
    // expected to reposition it via SetCoords before it becomes visible.
    //
    // The loop count mirrors CreateAnim(): -1 for looping animations
    // (IsLooping == true) so they repeat indefinitely, and 1 for
    // one-shot animations so they play exactly once and then expire.
    // The flags value 0x600 and zero Z-adjust match the defaults used
    // throughout the codebase (see CreateAnim, LaunchNuke, etc.).
    CoordStruct coord(0, 0, 0);
    AnimClass* anim = new AnimClass(this, coord, 0,
        IsLooping ? -1 : 1, 0x600, 0, Reverse);

    if (anim && pOwner) {
        anim->SetOwner(pOwner);
    }

    return anim;
}

SHPStruct* AnimTypeClass::LoadImage() {
    if (AnimShape) return AnimShape;

    // Load SHP file from the animation name
    if (AnimationName[0]) {
        // SHP loading from mix files
        // AnimShape = MixFileSystem::LoadSHP(AnimationName);
        // After loading, determine frame dimensions
        if (AnimShape) {
            NumFrames = AnimShape->Frames;
            MiddleFrameIndex = NumFrames / 2;

            // Calculate middle frame dimensions for drawing
            if (MiddleFrameIndex >= 0 && MiddleFrameIndex < NumFrames) {
                Rectangle bounds = AnimShape->GetFrameBounds(MiddleFrameIndex);
                MiddleFrameWidth = bounds.Width;
                MiddleFrameHeight = bounds.Height;
            }
        }
    }

    return AnimShape;
}

void AnimTypeClass::Load2DArt() {
    LoadImage();

    // Load shadow image if shadow flag is set
    if (Shadow && !ShadowShape && AnimationName[0]) {
        // Try to load shadow variant (e.g., "xxx.shp" -> "xxxS.shp")
        char shadowName[64];
        std::memset(shadowName, 0, sizeof(shadowName));
        int32 len = 0;
        while (AnimationName[len] && len < 60) {
            shadowName[len] = AnimationName[len];
            ++len;
        }
        // Replace .shp with S.shp
        if (len > 4) {
            shadowName[len - 4] = 'S';
            shadowName[len - 3] = '.';
            shadowName[len - 2] = 's';
            shadowName[len - 1] = 'h';
            shadowName[len] = 'p';
            shadowName[len + 1] = '\0';
        }
        // ShadowShape = MixFileSystem::LoadSHP(shadowName);
    }
}

// ============================================================================
// INI
// ============================================================================

bool AnimTypeClass::LoadFromINI(CCINIClass* pINI) {
    if (!pINI) return false;
    const char* section = this->ID;
    if (!section || !section[0]) return false;

    // Read animation name
    char animNameBuf[256];
    pINI->ReadString(section, "Image", "", animNameBuf, sizeof(animNameBuf));
    const char* animName = animNameBuf;
    if (animName && animName[0]) {
        int32 j = 0;
        while (animName[j] && j < 31) {
            AnimationName[j] = animName[j];
            ++j;
        }
        AnimationName[j] = '\0';
    }

    // Frame timing
    Rate = pINI->ReadInteger(section, "Rate", 0);
    Start = pINI->ReadInteger(section, "Start", 0);
    LoopStart = pINI->ReadInteger(section, "LoopStart", -1);
    LoopEnd = pINI->ReadInteger(section, "LoopEnd", -1);
    End = pINI->ReadInteger(section, "End", 0);
    LoopCount = pINI->ReadInteger(section, "LoopCount", 0);

    // Random timing
    RandomLoopDelay.Min = pINI->ReadInteger(section, "RandomLoopDelayMin", 0);
    RandomLoopDelay.Max = pINI->ReadInteger(section, "RandomLoopDelayMax", 0);
    RandomRate.Min = pINI->ReadInteger(section, "RandomRateMin", 0);
    RandomRate.Max = pINI->ReadInteger(section, "RandomRateMax", 0);

    // Damage
    Damage = pINI->ReadFloat(section, "Damage", 0.0);
    DamageRadius = pINI->ReadInteger(section, "DamageRadius", 0);

    // Warhead
    char warheadBuf[64];
    pINI->ReadString(section, "Warhead", "", warheadBuf, sizeof(warheadBuf));
    if (warheadBuf[0]) {
        Warhead = WarheadTypeClass::Find(warheadBuf);
    }

    // Visual
    Translucency = pINI->ReadInteger(section, "Translucency", 0);
    TranslucencyDetailLevel = pINI->ReadInteger(section, "TranslucencyDetailLevel", 0);
    ZAdjust = pINI->ReadInteger(section, "ZAdjust", 0);
    YSortAdjust = pINI->ReadInteger(section, "YSortAdjust", 0);
    YDrawOffset = pINI->ReadInteger(section, "YDrawOffset", 0);
    DetailLevel = pINI->ReadInteger(section, "DetailLevel", 0);

    // Layer
    char layerBuf[32];
    pINI->ReadString(section, "Layer", "Ground", layerBuf, sizeof(layerBuf));
    if (!_strcmpi(layerBuf, "Air")) Layer_ = Layer::Air;
    else if (!_strcmpi(layerBuf, "Top")) Layer_ = Layer::Top;
    else if (!_strcmpi(layerBuf, "Surface")) Layer_ = Layer::Surface;
    else Layer_ = Layer::Ground;

    // Infantry spawn
    MakeInfantry = pINI->ReadInteger(section, "MakeInfantry", 0);
    MakeInfantryOwner = pINI->ReadInteger(section, "MakeInfantryOwner", -1);

    // Sound
    Report = pINI->ReadInteger(section, "Report", -1);
    StopSound = pINI->ReadInteger(section, "StopSound", -1);

    // Spawn/particle
    SpawnCount = pINI->ReadInteger(section, "SpawnCount", 0);
    NumParticles = pINI->ReadInteger(section, "NumParticles", 0);
    SpawnsParticle = pINI->ReadInteger(section, "SpawnsParticle", 0);

    // Physics
    Elasticity = pINI->ReadFloat(section, "Elasticity", 0.0);
    MinZVel = pINI->ReadFloat(section, "MinZVel", 0.0);
    MaxXYVel = pINI->ReadFloat(section, "MaxXYVel", 0.0);
    TrailerSeperation = pINI->ReadInteger(section, "TrailerSeperation", 0);
    RunningFrames = pINI->ReadInteger(section, "RunningFrames", 0);

    // Tiberium
    TiberiumSpreadRadius = pINI->ReadInteger(section, "TiberiumSpreadRadius", 0);

    // Next animation
    char nextBuf[64];
    pINI->ReadString(section, "Next", "", nextBuf, sizeof(nextBuf));
    if (nextBuf[0]) {
        Next = AnimTypeClass::Find(nextBuf);
    }

    // Bounce animation
    char bounceBuf[64];
    pINI->ReadString(section, "BounceAnim", "", bounceBuf, sizeof(bounceBuf));
    if (bounceBuf[0]) {
        BounceAnim = AnimTypeClass::Find(bounceBuf);
    }

    // Expire animation
    char expireBuf[64];
    pINI->ReadString(section, "ExpireAnim", "", expireBuf, sizeof(expireBuf));
    if (expireBuf[0]) {
        ExpireAnim = AnimTypeClass::Find(expireBuf);
    }

    // Trailer animation
    char trailerBuf[64];
    pINI->ReadString(section, "Trailer", "", trailerBuf, sizeof(trailerBuf));
    if (trailerBuf[0]) {
        TrailerAnim = AnimTypeClass::Find(trailerBuf);
    }

    // Spawns animation
    char spawnsBuf[64];
    pINI->ReadString(section, "Spawns", "", spawnsBuf, sizeof(spawnsBuf));
    if (spawnsBuf[0]) {
        Spawns = AnimTypeClass::Find(spawnsBuf);
    }

    // Read bool flags
    IsFlamingGuy = pINI->ReadBool(section, "IsFlamingGuy", false);
    IsVeins = pINI->ReadBool(section, "IsVeins", false);
    IsMeteor = pINI->ReadBool(section, "IsMeteor", false);
    IsTiberium = pINI->ReadBool(section, "IsTiberium", false);
    HideIfNoOre = pINI->ReadBool(section, "HideIfNoOre", false);
    TiberiumChainReaction = pINI->ReadBool(section, "TiberiumChainReaction", false);
    IsAnimatedTiberium = pINI->ReadBool(section, "IsAnimatedTiberium", false);
    Bouncer = pINI->ReadBool(section, "Bouncer", false);
    Tiled = pINI->ReadBool(section, "Tiled", false);
    ShouldUseCellDrawer = pINI->ReadBool(section, "ShouldUseCellDrawer", false);
    UseNormalLight = pINI->ReadBool(section, "UseNormalLight", false);
    AltPalette = pINI->ReadBool(section, "AltPalette", false);
    Normalized = pINI->ReadBool(section, "Normalized", false);
    DoubleThick = pINI->ReadBool(section, "DoubleThick", false);
    Flat = pINI->ReadBool(section, "Flat", false);
    Translucent = pINI->ReadBool(section, "Translucent", false);
    Scorch = pINI->ReadBool(section, "Scorch", false);
    Flamer = pINI->ReadBool(section, "Flamer", false);
    Crater = pINI->ReadBool(section, "Crater", false);
    ForceBigCraters = pINI->ReadBool(section, "ForceBigCraters", false);
    Sticky = pINI->ReadBool(section, "Sticky", false);
    PingPong = pINI->ReadBool(section, "PingPong", false);
    Reverse = pINI->ReadBool(section, "Reverse", false);
    Shadow = pINI->ReadBool(section, "Shadow", false);
    PsiWarning = pINI->ReadBool(section, "PsiWarning", false);
    ShouldFogRemove = pINI->ReadBool(section, "ShouldFogRemove", false);
    IsLooping = pINI->ReadBool(section, "Loop", false);
    IsInvisible = pINI->ReadBool(section, "Invisible", false);
    IsFlameThrower = pINI->ReadBool(section, "IsFlameThrower", false);
    IsBigGrey = pINI->ReadBool(section, "BigGrey", false);
    IsParticle = pINI->ReadBool(section, "IsParticle", false);
    IsRailgun = pINI->ReadBool(section, "IsRailgun", false);
    IsNuke = pINI->ReadBool(section, "IsNuke", false);
    IsIonCannon = pINI->ReadBool(section, "IsIonCannon", false);
    DemandLoad = pINI->ReadBool(section, "DemandLoad", false);
    FreeLoad = pINI->ReadBool(section, "FreeLoad", false);

    // Light settings
    LightSize = pINI->ReadInteger(section, "LightSize", 0);
    LightIntensity = pINI->ReadFloat(section, "LightIntensity", 0.0);
    LightVisibility = pINI->ReadInteger(section, "LightVisibility", 0);
    LightRedTint = pINI->ReadFloat(section, "LightRedTint", 0.0);
    LightGreenTint = pINI->ReadFloat(section, "LightGreenTint", 0.0);
    LightBlueTint = pINI->ReadFloat(section, "LightBlueTint", 0.0);
    LightFlashFrames = pINI->ReadInteger(section, "LightFlashFrames", 0);

    return true;
}

ObjectClass* AnimTypeClass::CreateAnim() {
    CoordStruct coord(0, 0, 0);
    AnimClass* anim = new AnimClass(this, coord, 0,
        IsLooping ? -1 : 1, 0x600, 0, Reverse);
    return anim;
}

// ============================================================================
// Helper methods
// ============================================================================

bool AnimTypeClass::HasDamage() const {
    return Damage > 0.0 && DamageRadius > 0 && Warhead != nullptr;
}

bool AnimTypeClass::HasSound() const {
    return Report >= 0 || StopSound >= 0;
}

bool AnimTypeClass::HasLight() const {
    return LightSize > 0 && LightIntensity > 0.0;
}

bool AnimTypeClass::IsGroundLayer() const {
    return Layer_ == Layer::Ground;
}

bool AnimTypeClass::IsAirLayer() const {
    return Layer_ == Layer::Air;
}

bool AnimTypeClass::IsTopLayer() const {
    return Layer_ == Layer::Top;
}

int32 AnimTypeClass::GetTotalFrames() const {
    if (End > 0) return End - Start + 1;
    return 0;
}

double AnimTypeClass::GetAnimationDuration() const {
    if (Rate <= 0 || End <= 0) return 0.0;
    return static_cast<double>(End - Start) / static_cast<double>(Rate);
}

int32 AnimTypeClass::GetLoopStartFrame() const {
    if (LoopStart >= 0) return LoopStart;
    return Start;
}

int32 AnimTypeClass::GetLoopEndFrame() const {
    if (LoopEnd >= 0) return LoopEnd;
    return End;
}

// ============================================================================
// Static registration
// ============================================================================

void AnimTypeClass::RegisterAll() {
    if (!Array) {
        Array = new DynamicVectorClass<AnimTypeClass*>();
    }

    struct AnimDef {
        const char* ID;
        const char* Image;
        int32 rate;
        int32 end;
        bool loop;
        bool translucent;
        int32 damage;
        int32 damageRadius;
        Layer layer;
        const char* next;
        bool shadow;
        bool scorch;
        bool crater;
        bool bigGrey;
        bool makeInfantry;
        bool isNuke;
        bool isIon;
    };

    static const AnimDef defs[] = {
        // ── Fire animations ─────────────────────────────────────────────
        { "INITFIRE",  "initfire.shp",  2, 8,  false, false, 0, 0,
          Layer::Ground, nullptr, false, false, false, false, false, false, false },
        { "FIRE1",     "fire1.shp",     3, 30, false, false, 0, 0,
          Layer::Ground, nullptr, false, false, false, false, false, false, false },
        { "FIRE2",     "fire2.shp",     3, 30, false, false, 0, 0,
          Layer::Ground, nullptr, false, false, false, false, false, false, false },
        { "FIRE3",     "fire3.shp",     3, 30, false, false, 0, 0,
          Layer::Ground, nullptr, false, false, false, false, false, false, false },
        { "FIRE4",     "fire4.shp",     3, 24, false, false, 0, 0,
          Layer::Ground, nullptr, false, false, false, false, false, false, false },
        { "FIRESM",    "firesm.shp",    3, 16, false, false, 0, 0,
          Layer::Ground, nullptr, false, false, false, false, false, false, false },

        // ── Smoke animations ────────────────────────────────────────────
        { "SMOKE1",    "smoke1.shp",    4, 20, false, true,  0, 0,
          Layer::Air, nullptr, false, false, false, false, false, false, false },
        { "SMOKE2",    "smoke2.shp",    4, 20, false, true,  0, 0,
          Layer::Air, nullptr, false, false, false, false, false, false, false },
        { "SMOKE3",    "smoke3.shp",    4, 16, false, true,  0, 0,
          Layer::Air, nullptr, false, false, false, false, false, false, false },
        { "SMOKEDARK", "smokdark.shp",  4, 20, false, true,  0, 0,
          Layer::Air, nullptr, false, false, false, false, false, false, false },

        // ── Explosion animations ────────────────────────────────────────
        { "EXPLOSION",  "explosion.shp", 3, 24, false, false, 50, 2,
          Layer::Ground, nullptr, false, true, true, false, false, false, false },
        { "EXPLOMED",   "explomed.shp",  3, 24, false, false, 100, 3,
          Layer::Ground, nullptr, false, true, true, false, false, false, false },
        { "EXPLOLRG",   "explolrg.shp",  3, 30, false, false, 150, 4,
          Layer::Ground, nullptr, false, true, true, false, false, false, false },
        { "EXPLOBIG",   "explobig.shp",  3, 36, false, false, 200, 5,
          Layer::Ground, nullptr, false, true, true, true, false, false, false },
        { "TWLT070",    "twlt070.shp",   3, 24, false, false, 50, 2,
          Layer::Ground, nullptr, false, true, true, false, false, false, false },

        // ── Superweapon animations ──────────────────────────────────────
        { "NUKEBALL",  "nukeball.shp",  2, 40, false, false, 1000, 10,
          Layer::Top, nullptr, false, false, false, false, false, true, false },
        { "IONBEAM",   "ionbeam.shp",   2, 30, false, false, 500, 3,
          Layer::Top, nullptr, false, false, false, false, false, false, true },
        { "IRONFX",    "ironfx.shp",    3, 20, false, false, 0, 0,
          Layer::Top, nullptr, false, false, false, false, false, false, false },
        { "CHRONOFX",  "chronofx.shp",  3, 15, false, false, 0, 0,
          Layer::Top, nullptr, false, false, false, false, false, false, false },
        { "LIGHTNING", "lightning.shp", 2, 10, false, false, 100, 3,
          Layer::Top, nullptr, false, false, false, false, false, false, false },
        { "DOMINATOR", "dominator.shp", 2, 20, false, false, 0, 0,
          Layer::Top, nullptr, false, false, false, false, false, false, false },
        { "PSYCHIC",   "psychic.shp",   2, 15, false, false, 0, 0,
          Layer::Top, nullptr, false, false, false, false, false, false, false },
        { "PARADROP",  "paradrop.shp",  2, 10, false, false, 0, 0,
          Layer::Top, nullptr, false, false, false, false, false, false, false },
        { "DROPPOD",   "droppod.shp",   2, 10, false, false, 0, 0,
          Layer::Top, nullptr, false, false, false, false, false, false, false },
        { "FORCESHIELD","fshield.shp",  2, 20, false, false, 0, 0,
          Layer::Top, nullptr, false, false, false, false, false, false, false },

        // ── Building anims (loop) ───────────────────────────────────────
        { "CONYARD",   "conyard.shp",   2, 30, true,  false, 0, 0,
          Layer::Ground, nullptr, true, false, false, false, false, false, false },
        { "WARFACTORY","warfctry.shp",  2, 30, true,  false, 0, 0,
          Layer::Ground, nullptr, true, false, false, false, false, false, false },
        { "BARRACKS",  "barracks.shp",  2, 30, true,  false, 0, 0,
          Layer::Ground, nullptr, true, false, false, false, false, false, false },
        { "POWERPLANT","powerplt.shp",  2, 30, true,  false, 0, 0,
          Layer::Ground, nullptr, true, false, false, false, false, false, false },
        { "REFINERY",  "refinery.shp",   2, 30, true,  false, 0, 0,
          Layer::Ground, nullptr, true, false, false, false, false, false, false },
        { "BATTLAB",   "battlab.shp",   2, 30, true,  false, 0, 0,
          Layer::Ground, nullptr, true, false, false, false, false, false, false },
        { "AIRFORCE",  "airforce.shp",  2, 30, true,  false, 0, 0,
          Layer::Ground, nullptr, true, false, false, false, false, false, false },

        // ── Scorch / crater ─────────────────────────────────────────────
        { "SCORCH1",   "scorch1.shp",   0, 1,  false, false, 0, 0,
          Layer::Ground, nullptr, false, true, false, false, false, false, false },
        { "SCORCH2",   "scorch2.shp",   0, 1,  false, false, 0, 0,
          Layer::Ground, nullptr, false, true, false, false, false, false, false },
        { "SCORCH3",   "scorch3.shp",   0, 1,  false, false, 0, 0,
          Layer::Ground, nullptr, false, true, false, false, false, false, false },
        { "CRATER1",   "crater1.shp",   0, 1,  false, false, 0, 0,
          Layer::Ground, nullptr, false, false, true, false, false, false, false },
        { "CRATER2",   "crater2.shp",   0, 1,  false, false, 0, 0,
          Layer::Ground, nullptr, false, false, true, false, false, false, false },

        // ── Infantry death anims ────────────────────────────────────────
        { "DIE1",      "die1.shp",      3, 15, false, false, 0, 0,
          Layer::Ground, nullptr, true, false, false, false, false, false, false },
        { "DIE2",      "die2.shp",      3, 15, false, false, 0, 0,
          Layer::Ground, nullptr, true, false, false, false, false, false, false },
        { "FLAMEGUY",  "flameguy.shp",  3, 20, false, false, 0, 0,
          Layer::Ground, nullptr, true, false, false, false, false, false, false },

        // ── Weather / environment ───────────────────────────────────────
        { "RAIN",      "rain.shp",      2, 4,  true,  true,  0, 0,
          Layer::Air, nullptr, false, false, false, false, false, false, false },
        { "SNOW",      "snow.shp",      2, 4,  true,  true,  0, 0,
          Layer::Air, nullptr, false, false, false, false, false, false, false },
        { "METEOR",    "meteor.shp",    3, 12, false, false, 100, 2,
          Layer::Top, nullptr, false, false, true, false, false, false, false },

        // ── Veins / Tiberium ────────────────────────────────────────────
        { "VEINS",     "veins.shp",     2, 8,  true,  false, 0, 0,
          Layer::Ground, nullptr, false, false, false, false, false, false, false },
        { "VEINHOLE",  "veinhole.shp",  2, 20, false, false, 0, 0,
          Layer::Ground, nullptr, false, false, false, false, false, false, false },
        { "TIBERIUM",  "tiberium.shp",  2, 16, true,  false, 0, 0,
          Layer::Ground, nullptr, false, false, false, false, false, false, false },

        // ── Misc ────────────────────────────────────────────────────────
        { "SPARK",     "spark.shp",     4, 10, false, false, 0, 0,
          Layer::Ground, nullptr, false, false, false, false, false, false, false },
        { "SPARK2",    "spark2.shp",    4, 10, false, false, 0, 0,
          Layer::Ground, nullptr, false, false, false, false, false, false, false },
        { "BLOOD",     "blood.shp",     3, 8,  false, false, 0, 0,
          Layer::Ground, nullptr, true, false, false, false, false, false, false },
        { "BLOOD2",    "blood2.shp",    3, 8,  false, false, 0, 0,
          Layer::Ground, nullptr, true, false, false, false, false, false, false },
        { "DEBRIS",    "debris.shp",    3, 12, false, false, 0, 0,
          Layer::Ground, nullptr, true, false, false, false, false, false, false },
        { "WATEREXP",  "waterexp.shp",  3, 20, false, false, 0, 0,
          Layer::Ground, nullptr, false, false, false, false, false, false, false },
        { "WATERCOL",  "watercol.shp",  3, 12, false, false, 0, 0,
          Layer::Ground, nullptr, false, false, false, false, false, false, false },
        { "SHOCKWAVE", "shockwave.shp", 2, 16, false, false, 0, 0,
          Layer::Top, nullptr, false, false, false, false, false, false, false },
        { "MINDPULSE", "mindpulse.shp", 2, 12, false, false, 0, 0,
          Layer::Top, nullptr, false, false, false, false, false, false, false },
        { "TELEPORT",  "teleport.shp",  3, 10, false, false, 0, 0,
          Layer::Top, nullptr, false, false, false, false, false, false, false },
    };

    for (size_t i = 0; i < sizeof(defs) / sizeof(defs[0]); ++i) {
        AnimTypeClass* at = new AnimTypeClass(defs[i].ID);
        int32 j = 0;
        while (defs[i].Image[j] && j < 31) {
            at->AnimationName[j] = defs[i].Image[j];
            ++j;
        }
        at->AnimationName[j] = '\0';
        at->Rate = defs[i].rate;
        at->End = defs[i].end;
        at->IsLooping = defs[i].loop;
        at->Translucent = defs[i].translucent;
        at->Damage = defs[i].damage;
        at->DamageRadius = defs[i].damageRadius;
        at->Layer_ = defs[i].layer;
        at->Shadow = defs[i].shadow;
        at->Scorch = defs[i].scorch;
        at->Crater = defs[i].crater;
        at->IsBigGrey = defs[i].bigGrey;
        at->MakeInfantry = defs[i].makeInfantry ? 1 : 0;
        at->IsNuke = defs[i].isNuke;
        at->IsIonCannon = defs[i].isIon;

        // Link next animation
        if (defs[i].next) {
            at->Next = AnimTypeClass::Find(defs[i].next);
        }

        Array->Add(at);
    }
}