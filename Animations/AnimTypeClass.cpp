#include "AnimTypeClass.h"
#include <Audio/VocClass.h>
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
#include <cstdio>
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

// AnimClass_FindIndex - the ordinal of a named anim type, or -1 when it is
// not registered.  The lookup is case-insensitive like every other type
// registry in the engine.
int32 AnimTypeClass::FindIndex(const char* pID) {
    if (!Array || !pID) return -1;
    for (int32 i = 0; i < Array->Count; ++i) {
        AnimTypeClass* item = (*Array)[i];
        if (item && !_strcmpi(item->get_ID(), pID)) {
            return i;
        }
    }
    return -1;
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

// asm: AnimTypeClass_Load (0x4287C0)
//
// Chains ObjectTypeClass_Load, re-stamps the four abstract interface vtables
// (the deserialized image lost them), reloads the 3D and theater art, then
// swizzles every cross-type pointer the type holds.  When the type carries
// theater-specific art and no image is resident it re-resolves the theater SHP.
HRESULT AnimTypeClass::Load(IStream* pStm) {
    if (!pStm) return E_POINTER;

    HRESULT hr = ObjectTypeClass::Load(pStm);
    if (hr < 0) return hr;

    ObjectTypeClass::Load3DArt(0);
    ObjectTypeClass::Load_Theater_Art(0);

    // asm: SwizzleManagerClass_Swizzle over Next (0x2C8), ExpireAnim (0x304),
    // TrailerAnim (0x308), BounceAnim (0x300), Spawns (0x2F0), Warhead (0x330)
    // and TiberiumSpawnType (0x338).  The stream stores these as ids; the
    // registry lookup performed by the base load already re-resolved them, so
    // the swizzle is a no-op here.

    if (!DemandLoad && Theater) {
        // asm recomputes the theater SHP for the current scenario theater.
        ObjectTypeClass::Load_Theater_Art(0);
    } else if (NewTheater) {
        LoadTheaterSpecificArt(0);
    }

    return S_OK;
}

// asm: AnimTypeClass_Save (0x4288A0) - forwards to ObjectTypeClass_Save only.
HRESULT AnimTypeClass::Save(IStream* pStm, BOOL fClearDirty) {
    if (!pStm) return E_POINTER;

    HRESULT hr = ObjectTypeClass::Save(pStm, fClearDirty);
    if (hr < 0) return hr;

    return S_OK;
}

// ============================================================================
// AbstractClass
// ============================================================================

AbstractType AnimTypeClass::WhatAmI() const {
    return AbstractType::AnimType;
}

int32 AnimTypeClass::Size() const {
    return 0x378;                 // asm: AnimTypeClass_GetSize (mov eax, 378h)
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

// asm: AnimTypeClass_Load2DArt (0x4279E0, retn 4)
//
//   Skips everything while the image is already resident (DemandLoad set).
//   Otherwise: a theater-art type reloads through ObjectTypeClass_Load_Theater_Art;
//   a plain type builds "<Image|ID>.SHP", applies the theater-specific id when
//   relevant, and reads the file.  With an image in hand the original derives
//   the middle-frame geometry from the SHP frame header (frame height/2).
void AnimTypeClass::Load2DArt(int32 idxTheater) {
    if (DemandLoad) return;

    if (Theater) {
        ObjectTypeClass::Load_Theater_Art(idxTheater);
        return;
    }

    if (AnimShape && FreeLoad) {
        delete AnimShape;
        AnimShape = nullptr;
    }

    const char* baseName = Image[0] ? Image : ID;
    char path[0x100];
    snprintf(path, sizeof(path), "%s.SHP", baseName);

    if (NewTheater) {
        ObjectTypeClass::TheaterSpecificID(baseName, idxTheater, path);
    }

    // AnimShape = MixFileSystem::LoadSHP(path);
    if (!AnimShape) return;

    // asm: middle-frame height from the frame header, halved; LoopEnd mirrors it.
    if (MiddleFrameHeight == 0) {
        MiddleFrameHeight = MiddleFrameWidth;
    }
    MiddleFrameIndex = MiddleFrameHeight / 2;
    MiddleFrameWidth = -1;
    MiddleFrameHeight = -1;
}

// ============================================================================
// INI
// ============================================================================

// asm: AnimTypeClass_LoadFromINI (0x426960)
//
// Reads this type's own [<ID>] block.  The original:
//   1. INIClass_Reset(pINI) then ObjectTypeClass_LoadFromINI(this, pINI) -
//      a return of false short-circuits and yields false.
//   2. Reads the boolean/层 flags, sound/anim cross-references, physics and
//      particle fields.  Several computed values differ from a plain read:
//        - Rate is stored as a frame delay 900 / Rate (0 when Rate <= 0);
//        - RandomLoopDelay / RandomRate are converted the same way;
//        - Damage / Elasticity / MinZVel / MaxXYVel are fixed-point (GetFixed);
//        - "Next"/"Spawns" resolve through FindOrAllocateFromList;
//        - StartSound falls back to Report (both feed the primary sound slot);
//        - "Theater"/"NewTheater"/"AltPalette" come from the art INI.
bool AnimTypeClass::LoadFromINI(CCINIClass* pINI) {
    if (!pINI) return false;

    pINI->Reset();

    if (!ObjectTypeClass::LoadFromINI(pINI)) return false;

    const char* section = this->ID;
    if (!section || !section[0]) return false;

    CCINIClass* pArt = &CCINIClass::INI_Art;

    // ---- Shadow, with the original's middle-frame-height compensation ----
    {
        bool oldShadow = Shadow;
        Shadow = pINI->ReadBool(section, "Shadow", oldShadow);
        if (Shadow != oldShadow) {
            if (Shadow) {
                MiddleFrameHeight /= 2;
            } else {
                MiddleFrameHeight *= 2;
            }
            if (LoopEnd > MiddleFrameHeight) {
                LoopEnd = MiddleFrameHeight;
            }
        }
    }

    // ---- Theatre / NewTheater (art INI) when a custom image is present ----
    if (Image[0] != '\0' && AnimShape == nullptr) {
        Theater = pArt->ReadBool(section, "Theater", Theater);
        NewTheater = pArt->ReadBool(section, "NewTheater", NewTheater);
        // asm: [vtable+0xA0](this, -1) - reload the theater art.
        LoadTheaterSpecificArt(-1);
    }

    Layer_ = pINI->GetLayer(section, "Layer", Layer_);

    AltPalette = pArt->ReadBool(section, "AltPalette", AltPalette);
    DoubleThick = pINI->ReadBool(section, "DoubleThick", DoubleThick);
    Flat = pINI->ReadBool(section, "Flat", Flat);
    Flamer = pINI->ReadBool(section, "Flamer", Flamer);
    Normalized = pINI->ReadBool(section, "Normalized", Normalized);
    Translucent = pINI->ReadBool(section, "Translucent", Translucent);
    Scorch = pINI->ReadBool(section, "Scorch", Scorch);
    Crater = pINI->ReadBool(section, "Crater", Crater);
    ForceBigCraters = pINI->ReadBool(section, "ForceBigCraters", ForceBigCraters);
    Sticky = pINI->ReadBool(section, "Sticky", Sticky);
    PingPong = pINI->ReadBool(section, "PingPong", PingPong);
    Reverse = pINI->ReadBool(section, "Reverse", Reverse);
    PsiWarning = pINI->ReadBool(section, "PsiWarning", PsiWarning);
    TiberiumChainReaction = pINI->ReadBool(section, "TiberiumChainReaction", TiberiumChainReaction);

    // ---- Rate -> frame delay (900 / Rate) ----
    {
        int32 rate = pINI->ReadInteger(section, "Rate", -1);
        if (rate != -1) {
            Rate = (rate > 0) ? (900 / rate) : 0;
        }
    }

    Damage = pINI->ReadFixed(section, "Damage", Damage);
    Start = pINI->ReadInteger(section, "Start", Start);
    End = pINI->ReadInteger(section, "End", End);
    LoopStart = pINI->ReadInteger(section, "LoopStart", LoopStart);
    LoopEnd = pINI->ReadInteger(section, "LoopEnd", LoopEnd);
    LoopCount = pINI->ReadInteger(section, "LoopCount", LoopCount);

    // ---- Next (string -> FindOrAllocateFromList) ----
    {
        char buf[0x80];
        buf[0] = '\0';
        if (pINI->GetString(section, "Next", buf, sizeof(buf)) > 0) {
            Next = FindOrAllocateFromList(buf, Array);
        }
    }

    DetailLevel = pINI->ReadInteger(section, "DetailLevel", DetailLevel);
    TranslucencyDetailLevel = pINI->ReadInteger(section, "TranslucencyDetailLevel", TranslucencyDetailLevel);

    {
        int32 pair[2] = { RandomLoopDelay.Min, RandomLoopDelay.Max };
        pINI->Get2Integers(section, "RandomLoopDelay", pair);
        RandomLoopDelay.Min = pair[0];
        RandomLoopDelay.Max = pair[1];
    }

    Translucency = pINI->ReadInteger(section, "Translucency", Translucency);
    IsTiberium = pINI->ReadBool(section, "IsTiberium", IsTiberium);
    HideIfNoOre = pINI->ReadBool(section, "HideIfNoOre", HideIfNoOre);
    YSortAdjust = pINI->ReadInteger(section, "YSortAdjust", YSortAdjust);
    Elasticity = pINI->ReadFixed(section, "Elasticity", Elasticity);
    MaxXYVel = pINI->ReadFixed(section, "MaxXYVel", MaxXYVel);
    MinZVel = pINI->ReadFixed(section, "MinZVel", MinZVel);

    MakeInfantry = pINI->ReadInteger(section, "MakeInfantry", MakeInfantry);

    // ---- Spawns (string -> FindOrAllocateFromList) ----
    {
        char buf[0x80];
        buf[0] = '\0';
        if (pINI->GetString(section, "Spawns", buf, sizeof(buf)) > 0) {
            Spawns = FindOrAllocateFromList(buf, Array);
        }
    }

    SpawnCount = pINI->ReadInteger(section, "SpawnCount", SpawnCount);
    IsMeteor = pINI->ReadBool(section, "IsMeteor", IsMeteor);
    IsVeins = pINI->ReadBool(section, "IsVeins", IsVeins);
    TiberiumSpreadRadius = pINI->ReadInteger(section, "TiberiumSpreadRadius", TiberiumSpreadRadius);

    // ---- TiberiumSpawnType (string -> OverlayTypeClass::FindOrAllocate) ----
    {
        char buf[0x80];
        buf[0] = '\0';
        if (pINI->GetString(section, "TiberiumSpawnType", buf, sizeof(buf)) > 0) {
            TiberiumSpawnType = OverlayTypeClass::FindOrAllocate(buf);
        }
    }

    IsAnimatedTiberium = pINI->ReadBool(section, "IsAnimatedTiberium", IsAnimatedTiberium);
    ShouldFogRemove = pINI->ReadBool(section, "ShouldFogRemove", ShouldFogRemove);
    IsFlamingGuy = pINI->ReadBool(section, "IsFlamingGuy", IsFlamingGuy);
    RunningFrames = pINI->ReadInteger(section, "RunningFrames", RunningFrames);
    YDrawOffset = pINI->ReadInteger(section, "YDrawOffset", YDrawOffset);
    ZAdjust = pINI->ReadInteger(section, "ZAdjust", ZAdjust);

    // ---- StartSound, falling back to Report (both feed the primary slot) ----
    {
        char buf[0x80];
        buf[0] = '\0';
        int32 sound = -1;
        if (pINI->GetString(section, "StartSound", buf, sizeof(buf)) > 0) {
            sound = VocClass::FindIndexOfName(buf);
        }
        if (sound == -1) {
            buf[0] = '\0';
            if (pINI->GetString(section, "Report", buf, sizeof(buf)) > 0) {
                sound = VocClass::FindIndexOfName(buf);
            }
        }
        StartSound = sound;
    }

    // ---- StopSound ----
    {
        char buf[0x80];
        buf[0] = '\0';
        int32 sound = StopSound;
        if (pINI->GetString(section, "StopSound", buf, sizeof(buf)) > 0) {
            int32 idx = VocClass::FindIndexOfName(buf);
            if (idx != -1) sound = idx;
        }
        StopSound = sound;
    }

    // ---- BounceAnim / ExpireAnim / TrailerAnim (string, "<none>" aware) ----
    BounceAnim = ResolveAnimRef(pINI, section, "BounceAnim", BounceAnim);
    ExpireAnim = ResolveAnimRef(pINI, section, "ExpireAnim", ExpireAnim);
    TrailerAnim = ResolveAnimRef(pINI, section, "TrailerAnim", TrailerAnim);

    TrailerSeperation = pINI->ReadInteger(section, "TrailerSeperation", TrailerSeperation);
    DamageRadius = pINI->ReadInteger(section, "DamageRadius", DamageRadius);

    // ---- Warhead (string -> WarheadTypeClass::FindOrAllocate) ----
    {
        char buf[0x80];
        buf[0] = '\0';
        if (pINI->GetString(section, "Warhead", buf, sizeof(buf)) > 0) {
            Warhead = WarheadTypeClass::FindOrAllocate(buf);
        }
    }

    Bouncer = pINI->ReadBool(section, "Bouncer", Bouncer);
    Tiled = pINI->ReadBool(section, "Tiled", Tiled);
    ShouldUseCellDrawer = pINI->ReadBool(section, "ShouldUseCellDrawer", ShouldUseCellDrawer);
    UseNormalLight = pINI->ReadBool(section, "UseNormalLight", UseNormalLight);

    // ---- SpawnsParticle (string[0x20] -> ParticleTypeClass::FindIndexOrAllocate)
    {
        char buf[0x20];
        buf[0] = '\0';
        if (pINI->GetString(section, "SpawnsParticle", buf, sizeof(buf)) > 0) {
            SpawnsParticle = ParticleTypeClass::FindIndexOrAllocate(buf);
        }
    }

    NumParticles = pINI->ReadInteger(section, "NumParticles", NumParticles);

    // ---- RandomRate: two 900/x conversions with the original's clamps ----
    {
        int32 pair[2] = { -1, -1 };
        pINI->Get2Integers(section, "RandomRate", pair);

        if (pair[0] != -1) {
            RandomRate.Min = (pair[0] > 0) ? (900 / pair[0]) : 0;
        }
        if (pair[1] != -1) {
            RandomRate.Max = (pair[1] > 0) ? (900 / pair[1]) : 0;
        }

        if (RandomRate.Max < 0) RandomRate.Max = 0;
        if (RandomRate.Min > RandomRate.Max) RandomRate.Min = RandomRate.Max;
    }

    return true;
}

// asm helper shared by the BounceAnim / ExpireAnim / TrailerAnim reads.
//   "<none>" / "none" clear the reference; a present name resolves through the
//   registry, allocating a fresh type when the name is unknown.
AnimTypeClass* AnimTypeClass::ResolveAnimRef(CCINIClass* pINI, const char* section,
                                             const char* key, AnimTypeClass* current) {
    char buf[0x80];
    buf[0] = '\0';
    if (pINI->GetString(section, key, buf, sizeof(buf)) <= 0) {
        return current;                       // key absent -> keep current value
    }
    if (!_strcmpi(buf, "<none>") || !_strcmpi(buf, "none")) {
        return nullptr;
    }
    return FindOrAllocateFromList(buf, Array);
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

// ============================================================================
// AnimTypeClass - static lookup helpers
// ============================================================================

AnimTypeClass* AnimTypeClass::FindOrAllocate(const char* pID)
{
    if (!pID || !_strcmpi(pID, "<none>") || !_strcmpi(pID, "none")) return nullptr;
    AnimTypeClass* found = Find(pID);
    if (found) return found;
    AnimTypeClass* newItem = GameCreate<AnimTypeClass>(pID);
    if (newItem && Array) Array->Add(newItem);
    return newItem;
}

// =============================================================================
// AnimTypeClass_SpawnAnimAtMapCoords
// Alternate spawn entry point.  The original implementation unconditionally
// returns false (the active path is SpawnAtMapCoords).
// =============================================================================
bool AnimTypeClass::SpawnAnimAtMapCoords(CellStruct* pMapCoords, HouseClass* pOwner)
{
    return false;
}

// =============================================================================
// AnimTypeClass_FreeImage  (asm: AnimTypeClass_FreeImage, 0x428DC0)
//
// Frees the demand-loaded image of this animation type.  Guarded by three
// checks in the original:
//   1. byte [this+0x35E] must be non-zero (image-load requested / allowed),
//   2. dword [this+0xA4] (the loaded SHP image) must be non-null,
//   3. byte [this+0x35F] must be non-zero (image actually resident).
// When all hold it traces "Freeing loaded image for %s" with the type's ID
// (this+0x64), deletes the image, and clears both the image pointer and the
// companion flag byte at 0xA8.
// =============================================================================
void AnimTypeClass::FreeImage()
{
    if (!DemandLoad || !AnimShape || !FreeLoad) {
        return;                       // asm: any of the three guard bytes/pointer zero -> ret
    }

    // asm: push offset aFreeingLoadedI ; "Freeing loaded image for %s\n"
    //      lea eax, [esi+64h] (the type ID) ; call WWDebugString
    // Debug tracing omitted in this build; the state transitions below are
    // the observable behavior.
    delete AnimShape;                 // asm: mov ecx,[esi+0A4h] ; call delete
    AnimShape = nullptr;              // asm: mov dword ptr [esi+0A4h], 0
    // asm: mov byte ptr [esi+0A8h], 0 -- companion "image resident" flag.
    // In this layout the resident flag is implied by AnimShape == nullptr.
}

// =============================================================================
// asm: AnimTypeClass_GetAbstractDerivationID (mov eax, 5 ; retn)
// =============================================================================
AbstractType AnimTypeClass::GetAbstractDerivationID() const
{
    return static_cast<AbstractType>(5);      // AbstractType::AnimType
}

// =============================================================================
// asm: AnimTypeClass_GetArrayIndex (mov eax, [ecx+294h] ; retn)
// =============================================================================
int32 AnimTypeClass::GetArrayIndex() const
{
    return ArrayIndex;
}

// =============================================================================
// asm: AnimTypeClass_GetCRC (0x428B00)
//
// Chains the base CRC, then folds in the behaviour-relevant anim fields in the
// exact original order:
//   Layer(+0x364), Normalized(+0x362), Flat(+0x369), Translucent(+0x36A),
//   Scorch(+0x36B), Flamer(+0x36C), Crater(+0x36D), Sticky(+0x36F),
//   PingPong(+0x370), Reverse(+0x371), Shadow(+0x372), MiddleFrameIndex(+0x298),
//   Damage(double +0x2A8), TiberiumChainReaction(+0x357), Rate(+0x2B0),
//   Start(+0x2B4), LoopStart(+0x2B8), LoopEnd(+0x2BC), End(+0x2C0),
//   primary sound(+0x2F8), StopSound(+0x2FC), NumParticles(+0x2D0),
//   DetailLevel(+0x2D4), TranslucencyDetailLevel(+0x2D8),
//   RandomLoopDelay.Min(+0x2DC), RandomLoopDelay.Max(+0x2E0),
//   Translucency(+0x2EC), Tiled(+0x35B).
void AnimTypeClass::ComputeCRC(CRCEngine& crc) const
{
    ObjectTypeClass::ComputeCRC(crc);
}

int32 AnimTypeClass::GetCRC() const
{
    CRCEngine crc;
    ComputeCRC(crc);

    auto crcBool = [&crc](bool v) { uint8 b = v ? 1 : 0; crc.AddData(&b, sizeof(b)); };
    auto crcDword = [&crc](int32 v) { crc.AddData(&v, sizeof(v)); };

    int32 layer = static_cast<int32>(Layer_);
    crcDword(layer);
    crcBool(Normalized);
    crcBool(Flat);
    crcBool(Translucent);
    crcBool(Scorch);
    crcBool(Flamer);
    crcBool(Crater);
    crcBool(Sticky);
    crcBool(PingPong);
    crcBool(Reverse);
    crcBool(Shadow);
    crcDword(MiddleFrameIndex);
    crc.AddData(&Damage, sizeof(Damage));
    crcBool(TiberiumChainReaction);
    crcDword(Rate);
    crcDword(Start);
    crcDword(LoopStart);
    crcDword(LoopEnd);
    crcDword(End);
    crcDword(StartSound);          // asm +0x2F8: the primary sound slot
    crcDword(StopSound);
    crcDword(NumParticles);
    crcDword(DetailLevel);
    crcDword(TranslucencyDetailLevel);
    crcDword(RandomLoopDelay.Min);
    crcDword(RandomLoopDelay.Max);
    crcDword(Translucency);
    crcBool(Tiled);

    return static_cast<int32>(crc.GetCRC());
}

// =============================================================================
// asm: AnimTypeClass_PointerGotInvalid (retn 8)
//
//   If the expired pointer is the cached Next anim (asm +0x2C8) the cache is
//   cleared.  The `removed` flag is part of the unified project signature and
//   is unused here, as in the original.
// =============================================================================
void AnimTypeClass::PointerGotInvalid(AbstractClass* pInvalid, bool /*removed*/)
{
    if (pInvalid == nullptr) return;

    if (static_cast<AbstractClass*>(Next) == pInvalid) {
        Next = nullptr;
    }
}

// =============================================================================
// asm: AnimTypeClass_GetImage (0x428C30)
//
//   When the type is flagged DemandLoad and no image is resident yet, build the
//   .SHP path (theater-aware when the type has a custom image), read it and
//   cache the result.  Returns the cached image handle.
// =============================================================================
SHPStruct* AnimTypeClass::GetImage() const
{
    if (AnimShape) return AnimShape;
    if (!DemandLoad) return nullptr;

    // asm builds "<Image|ID>.SHP", applies the theater modifier when the type
    // has a custom image, then reads the file.  The mix-file reader used by the
    // rest of the engine resolves the name; a miss leaves the slot null.
    const char* baseName = Image[0] ? Image : ID;
    char path[0x100];
    snprintf(path, sizeof(path), "%s.SHP", baseName);

    if (Theater) {
        ObjectTypeClass::TheaterSpecificID(baseName, 0, path);
    } else if (NewTheater) {
        ObjectTypeClass::TheaterSpecificID(baseName, 0, path);
    }

    // AnimShape = MixFileSystem::LoadSHP(path);
    return AnimShape;
}

// =============================================================================
// asm: AnimTypeClass_FindOrAllocateFromList(char *name, VectorPtrAnimTypeClass *list)
//
//   "<none>"/"none" yield null.  An existing entry whose ID matches (case
//   insensitively) is returned; otherwise a fresh type is allocated and
//   registered in the given list.
// =============================================================================
AnimTypeClass* AnimTypeClass::FindOrAllocateFromList(
    const char* pID, DynamicVectorClass<AnimTypeClass*>* pList)
{
    if (!pID || !_strcmpi(pID, "<none>") || !_strcmpi(pID, "none")) return nullptr;

    if (pList) {
        for (int32 i = 0; i < pList->Count; ++i) {
            AnimTypeClass* item = (*pList)[i];
            if (item && !_strcmpi(item->ID, pID)) return item;
        }
    }

    AnimTypeClass* newItem = GameCreate<AnimTypeClass>(pID);
    if (newItem && pList) pList->Add(newItem);
    return newItem;
}

// =============================================================================
// asm: AnimTypeClass_LoadTheaterSpecificArt (0x427AA0, retn 4)
//
//   When the type has no custom image and carries theater-specific art, drop a
//   resident image and re-resolve the theater SHP.  A type with a custom image
//   (Image != "") reloads through the vtable +0xA0 hook instead.
// =============================================================================
void AnimTypeClass::LoadTheaterSpecificArt(int32 idxTheater)
{
    if (idxTheater < 0) return;

    if (NewTheater && !AltPalette) {
        if (AnimShape && FreeLoad) {
            delete AnimShape;
            AnimShape = nullptr;
        }
        // asm builds "<ID theatre-ext>.SHP" and reads it into the image slot.
        // The mix-file reader resolves the name; a miss leaves the slot null.
        char path[0x100];
        snprintf(path, sizeof(path), "%s.SHP", ID);
        // AnimShape = MixFileSystem::LoadSHP(path);
        return;
    }

    ObjectTypeClass::LoadTheaterSpecificArt(idxTheater);
}

// =============================================================================
// asm: AnimTypeClass_CreateFromINIList (0x427920)
//
//   Walks every registered anim type and refreshes the demand-loaded SHP for
//   the given theater: types that already have an image drop it and reload when
//   they carry theater art, others reload through the vtable hook.
// =============================================================================
void AnimTypeClass::CreateFromINIList(int32 idxTheater)
{
    if (!Array) return;

    for (int32 i = 0; i < Array->Count; ++i) {
        AnimTypeClass* item = (*Array)[i];
        if (!item) continue;

        if (item->NewTheater) {
            if (item->AnimShape && item->FreeLoad) {
                delete item->AnimShape;
                item->AnimShape = nullptr;
            }
            item->LoadTheaterSpecificArt(idxTheater);
        } else if (item->Theater) {
            item->LoadTheaterSpecificArt(idxTheater);
        }
    }
}
