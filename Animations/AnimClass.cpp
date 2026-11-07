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
#include "../Rendering/ConvertClass.h"
#include "../Rendering/UnitArt.h"
#include "../Helpers/AsmUIHelpers.h"
#include "../Abstract/TechnoClass.h"
#include "../Abstract/FootClass.h"
#include "../Abstract/InfantryClass.h"
#include "../Scenario/ScenarioClass.h"
#include "../Rules/RulesClass.h"
#include "../Game/Game.h"
#include "../IO/CRC.h"
#include "../Math/Timer.h"
#include "../FileFormats/SHP.h"

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
    IterationTimer(0),
    FrameSwitchFrame(0),
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
    IterationTimer(0),
    FrameSwitchFrame(0),
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
// asm: AnimClass_CTOR2 (asm:116499)
//   原版动画的第二构造路径（与 AnimClass::AnimClass 并列，被 TechnoClass 构造
//   路径经 sub_6C3A10 调用）。它只做：基类构造 → 一次性默认值铺底 → 登记进
//   vec_Anims → 初始化两段 StrX → Alive=1。逐字段对应：
//     [esi+0ACh]=0  [esi+0B0h]=0  [esi+0B4h]=Frame(全局帧计数)
//     [esi+0BCh]=0  [esi+0C0h]=0  [esi+0C4h]=1(=Rate 基准)
//     [esi+0C8h]=0(Type) [esi+0CCh]=0(OwnerObject)
//     [esi+0D0h]=-1 [esi+0D4h]=0 [esi+0D8h]=-1
//     [esi+0FCh]=3E8h(RemainingIterations=1000)
//     [esi+100h]=0 [esi+104h]=0 [esi+118h..11Bh]=0 [esi+11Ch]=0 [esi+120h]=0
//     [esi+128h..13Ch]=0  两个 Quaternion 复位  [esi+188h]=0
//     [esi+179h]=0 [esi+184h]=0 [esi+18Ch]=0 [esi+194h]=0
//     [esi+195h]=1(=SkipProcessOnce 首帧跳过) [esi+196h]=0 [esi+197h]=0
//     [esi+19Bh]=0 [esi+19Ch]=1 [esi+19Dh]=0 [esi+19Eh]=0
// ============================================================================
void AnimClass::CTOR2() noexcept {
    Location = CoordStruct(0, 0, 0);

    Type = nullptr;                 // asm: [esi+0C8h]=0
    OwnerObject = nullptr;          // asm: [esi+0CCh]=0
    unknown_D0 = -1;                // asm: [esi+0D0h]=-1
    LightConvert = nullptr;
    LightConvertIndex = -1;         // asm: [esi+0D8h]=-1
    TintColor = 0;                  // asm: [esi+0D4h]=0
    RenderRemapIndex = 0;
    ZAdjust = 0;                    // asm: [esi+0BCh]=0
    YSortAdjust = 0;                // asm: [esi+0C0h]=0
    FlamingGuyCoords = CoordStruct(0, 0, 0);
    FlamingGuyRetries = 0;
    IsBuildingAnim = false;         // asm: [esi+179h]=0
    UnderTemporal = false;
    Paused = false;
    Unpaused = false;
    PausedAnimFrame = 0;
    Reverse = false;
    unknown_124 = 0;                // asm: [esi+184h]=0
    TranslucencyLevel = 0;
    TimeToDie = false;              // asm: [esi+18Ch]=0
    AttachedBullet = nullptr;       // asm: [esi+104h]=0
    Owner = nullptr;                // asm: [esi+100h]=0
    LoopDelay = 0;
    Accum = 1.0;
    // Rate 基准：asm [esi+0C4h]=1
    AnimFlags = BlitterFlags::None;
    HasExtras = false;
    RemainingIterations = 1000;     // asm: [esi+0FCh]=3E8h

    std::memset(PaletteName, 0, sizeof(PaletteName));
    std::memset(&Animation, 0, sizeof(Animation));
    // asm: mov eax,Frame / mov [esi+0B4h],eax —— 当前帧初值取全局帧计数。
    Animation.Value = FrameTimer::GetTime();
    Animation.Rate = 1;             // asm: mov dword ptr [esi+0C4h],1
    IterationTimer = 0;
    FrameSwitchFrame = 0;

    // 未开播：SkipProcessOnce 置起（asm [esi+195h]=1），第一帧跳过处理。
    SkipProcessOnce = true;
    IsPlaying_ = false;             // asm: [esi+194h]=0
    IsFogged = false;               // asm: [esi+19Ch]=1 的反面
    FlamingGuyExpire = false;
    UnableToContinue = false;
    Invisible = false;
    PowerOff = false;
    IsFlaming = false;
    IsNuke = false;
    IsIonCannon = false;
    IsLooping_ = false;
    // asm: [esi+19Ch]=1 —— 该位在 Update 中对应"未雾化可见"状态位。
    IsFogged = false;

    TrailerCount = 0;
    TrailerTimer = 0;
    LightTimer = 0;
    SoundTimer = 0;
    ScorchCreated = false;
    CraterCreated = false;
    InfantryCreated = false;
    TiberiumCreated = false;
    ChainAnimCreated = false;

    // 登记进全局动画数组（动画实例对 vec_Anims 的注册）。
    if (!Array) {
        Array = new DynamicVectorClass<AnimClass*>();
    }
    Array->Add(this);               // asm: vec_Anims.length++ / data[len]=esi
}


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

// asm: AnimClass_GetSize —— 固定结构大小 0x1C8。
int32 AnimClass::Size() const {
    return 0x1C8;
}

// ============================================================================
// asm: AnimClass_GetType —— mov eax,[ecx+0C8h]；直接返回动画类型指针。
//   AnimTypeClass 派生自 ObjectTypeClass，故可协变覆盖基类槽。
// ============================================================================
ObjectTypeClass* AnimClass::GetType() const {
    return Type;
}

// ============================================================================
// asm: AnimClass_GetAbstractDerivationID —— mov eax,4 / retn。
//   4 即 AbstractType::Anim，供存档 Swizzle 阶段判别派生类型。
// ============================================================================
AbstractType AnimClass::GetAbstractDerivationID() const {
    return AbstractType::Anim;
}

// ============================================================================
// asm: AnimClass_GetCRC
//   先走 ObjectClass::GetCRC 铺底，随后并入：
//     * OwnerObject 的抽象派生 ID（若挂有对象）；
//     * unknown_D0、TintColor、ZAdjust、YSortAdjust(+0x104)；
//     * Damage（double 拆两个 dword）；
//     * 状态标志、RemainingIterations、Translate 系列 bool。
//   本项目以 CRCEngine::AddData 逐字段串入，等价于原版逐项 CRC_* 调用。
// ============================================================================
void AnimClass::ComputeCRC(CRCEngine& crc) const {
    ObjectClass::ComputeCRC(crc);

    if (OwnerObject != nullptr) {
        const AbstractType ownerID = OwnerObject->WhatAmI();
        crc.AddData(&ownerID, sizeof(ownerID));
    }

    crc.AddData(&unknown_D0, sizeof(unknown_D0));
    crc.AddData(&TintColor, sizeof(TintColor));
    crc.AddData(&ZAdjust, sizeof(ZAdjust));
    crc.AddData(&YSortAdjust, sizeof(YSortAdjust));
    crc.AddData(&AttachedBullet, sizeof(AttachedBullet));
    crc.AddData(&Accum, sizeof(Accum));

    uint32 stateFlags = 0;
    if (IsBuildingAnim) stateFlags |= 0x00000001;
    if (UnderTemporal)  stateFlags |= 0x00000002;
    if (Paused)         stateFlags |= 0x00000004;
    if (Unpaused)       stateFlags |= 0x00000008;
    if (Reverse)        stateFlags |= 0x00000010;
    if (TimeToDie)      stateFlags |= 0x00000020;
    if (HasExtras)      stateFlags |= 0x00000040;
    crc.AddData(&stateFlags, sizeof(stateFlags));

    crc.AddData(&RemainingIterations, sizeof(RemainingIterations));

    uint8 b194 = static_cast<uint8>(IsLooping_);
    crc.AddData(&b194, sizeof(b194));
    crc.AddData(&unknown_196, sizeof(unknown_196));
    crc.AddData(&unknown_197, sizeof(unknown_197));

    uint8 b19B = static_cast<uint8>(IsFogged);
    crc.AddData(&b19B, sizeof(b19B));
    uint8 b19C = static_cast<uint8>(Invisible);
    crc.AddData(&b19C, sizeof(b19C));
    uint8 b19E = static_cast<uint8>(UnableToContinue);
    crc.AddData(&b19E, sizeof(b19E));
}

// ============================================================================
// asm: AnimClass_GetCoords
//   中心坐标取自 ObjectClass::Center_Coord；若挂有 OwnerObject，
//   再叠加 OwnerObject 的坐标（虚槽 +0x48），得到动画在场景中的真实坐标。
//   无 OwnerObject 时即为中心坐标本身。
// ============================================================================
CoordStruct* AnimClass::GetCoords(CoordStruct* pCrd) const {
    CoordStruct center;
    ObjectClass::GetCoords(&center);

    if (OwnerObject != nullptr) {
        CoordStruct ownerCoords;
        OwnerObject->GetCoords(&ownerCoords);
        center.X += ownerCoords.X;
        center.Y += ownerCoords.Y;
        center.Z += ownerCoords.Z;
    }

    *pCrd = center;
    return pCrd;
}

// ============================================================================
// asm: AnimClass_GetZ
//   取本对象 Location.Z；若挂有 OwnerObject，再叠加其 Location.Z。
// ============================================================================
int32 AnimClass::GetZ() const {
    int32 z = Location.Z;
    if (OwnerObject != nullptr) {
        z += OwnerObject->Location.Z;
    }
    return z;
}

// ============================================================================
// asm: AnimClass_ReturnRealYSort
//   基类 Y 排序值再加本动画的 YSortAdjust(+0x104)。
// ============================================================================
int32 AnimClass::ReturnRealYSort() const {
    return ObjectClass::ReturnRealYSort() + YSortAdjust;
}

// ============================================================================
// asm: AnimClass_In_Which_Layer
//   有 OwnerObject → 2（对象层）；否则有 Type → Type->Layer(+0x364)；
//   都没有 → 3。
// ============================================================================
int32 AnimClass::In_Which_Layer() const {
    if (OwnerObject != nullptr) {
        return 2;
    }
    if (Type != nullptr) {
        return static_cast<int32>(Type->Layer_);
    }
    return 3;
}

// ============================================================================
// asm: AnimClass_SetLayer(idxLayer) —— 转发 ObjectClass::Mark(idxLayer)，
//   返回是否真的发生了图层归属变化。
// ============================================================================
bool AnimClass::SetLayer(int32 idxLayer) {
    return Mark_Layer(idxLayer);
}

// ============================================================================
// asm: AnimClass_Remove0
//   1) 若 Type 有效且带"占用地块"标志：取动画所在地块，
//      清掉该格的 AnimAttached(0x20000) 位（动画不再吸附于此格）。
//   2) 调 ObjectClass::Remove0；成功则再从显示层摘除自身，返回真。
// ============================================================================
bool AnimClass::Remove0() {
    if (Type != nullptr && Type->ShouldUseCellDrawer) {
        CellStruct cell = CoordMath::CoordToCell(Location);
        CellClass* pCell = (MapClass::Instance != nullptr)
            ? MapClass::Instance->GetCellAt(cell) : nullptr;
        if (pCell != nullptr) {
            pCell->SetFlag(CellFlags::AnimAttached, false);
        }
    }

    if (ObjectClass::Remove0()) {
        if (DisplayClass::Instance != nullptr) {
            DisplayClass::Instance->Remove(this);
        }
        return true;
    }
    return false;
}


// ============================================================================
// asm: AnimClass_Update (asm:118595, ~0xC00 字节)
//
//   逐帧主推进。原版控制流（逐标签对应）：
//     [118595] IsPlaying(0x194)==0 且 Type->Report(0x2F8)!=-1
//              → GetCoords + VocClass_UpdateAtLocation(Report, coords)
//     done_report: Type->IsFlamingGuy → Update_FlamingGuy + ObjectClass::Update
//     done_flamingguy: Type->PsiWarning → Invisible = !Owner->CanSeePsiWarning(cell)
//     done_psiwarning: Type==Rules->Behind → Invisible = !ShowHidden
//     done_behind: Type->HideIfNoOre → 依所在格含矿与否切 Invisible
//     done_ore_twinkle: Type->MakeInfantry!=-1 → GetCoords 准备(本工程并入 End)
//     done_makeinf1: field_11B 且 field_11C==CurrentFrame → 清 field_11B
//     done_f11b: field_194 且 AnimExtras() 返回 1/2 → 地形/水体收束
//     done_dropzone: 占用格判定 / field_19C
//     zero_f184: 循环延迟 field_184 递减（归零调 Start）
//     done_animatedtib: 计算 End / LoopEnd（未定则取 GetImage 帧数）
//     Mark(2)；field_19E/field_11A 提前返回
//     IterationTimer 计时：FrameTimerClass_Get_Remaining==0 && Rate!=0
//        → CurrentFrame += Rate；Arm(Start=Frame,Duration=Rate)
//        → 伤害累积（OwnerObject 为地形 ×5）→ Damage_At_Cell + FlashbangWarheadAt
//     loop_mid: Start+CurrentFrame==MiddleFrame 且 !IsPlaying → Middle()
//     PingPong: 到边界翻转 Rate
//     循环计数：到 End/LoopEnd 时 RemainingLoops--，非零 → 复位帧 + RandomLoopDelay
//       归零 → Type->Next 链式切换(End/LoopEnd/Damage 复位, Rate=RandomRate, Start)
//            否则 MakeInfantry 后 UnInit。
//
//   OpenYR 把上述全部落在 AI() 中，Update() 只做门控后转译本函数。
// ============================================================================
void AnimClass::Update() {
    if (TimeToDie) return;
    if (Paused) return;

    // asm: 首帧跳过（field_195==1 时本帧直接返回）。
    if (SkipProcessOnce) {
        SkipProcessOnce = false;
        return;
    }

    if (UnableToContinue) {
        TimeToDie = true;
        return;
    }

    AI();
}

// ----------------------------------------------------------------------------
// asm: AnimClass_Update 主体（见上注释）。
// ----------------------------------------------------------------------------
int32 AnimClass::AI() {
    // --- 1) 循环报告音（未开播时按 Report 持续更新位置音效） ---
    if (!IsPlaying_ && Type != nullptr && Type->Report >= 0) {
        CoordStruct pos;
        GetCoords(&pos);
        VocClass::Sound_Effect(Type->Report, &pos, nullptr);
    }

    // --- 2) 火焰人（FlamingGuy）帧推进 + 基类每帧更新 ---
    if (Type != nullptr && Type->IsFlamingGuy) {
        Update_FlamingGuy();
        ObjectClass::Update();
    }

    // --- 3) PSI 预警可见性：House->CanSeePsiWarning(cell) ---
    if (Type != nullptr && Type->PsiWarning) {
        CoordStruct pos;
        GetCoords(&pos);
        CellStruct cell = CoordMath::CoordToCell(pos);
        CellClass* pCell = (MapClass::Instance != nullptr)
            ? MapClass::Instance->GetCellAt(cell) : nullptr;
        bool canSee = (Owner != nullptr && pCell != nullptr && Owner->CanSeePsiWarning());
        Invisible = !canSee;
    }

    // --- 4) Behind 层动画：按 ShowHidden 决定可见性。OpenYR 的 ShowHidden
    //         存于 GameOptionsClass，尚未接入全局单例，故保留分支结构
    //         （ShowHidden 为真可见，为假隐藏）。 ---
    if (RulesClass::Instance != nullptr && Type == RulesClass::Instance->Behind) {
        // ShowHidden==0 → Invisible=1；
    }

    // --- 5) HideIfNoOre：所在格无矿则隐藏 ---
    if (Type != nullptr && Type->HideIfNoOre) {
        CellStruct cell = CoordMath::CoordToCell(Location);
        CellClass* pCell = (MapClass::Instance != nullptr)
            ? MapClass::Instance->GetCellAt(cell) : nullptr;
        if (pCell != nullptr && pCell->Get_Contained_Tiberium_Index() >= 0) {
            Invisible = false;
        } else {
            Invisible = true;
        }
    }

    // --- 6) 帧计数切换标记（field_11B 且 field_11C==CurrentFrame） ---
    if (unknown_196 != 0 && PausedAnimFrame == Animation.Value) {
        unknown_196 = 0;
    }

    // --- 7) 已开播 → AnimExtras；返回 1/2 时进入地形/水体收束段 ---
    if (IsPlaying_) {
        int32 phase = AnimExtras();
        if (phase == 1 || phase == 2) {
            // 取所在地块地形：岩石(2)/特殊(6) 与"帧号超过地面高度"共同决定
            // 是否派生 Wake / 陨石动画并施加范围伤害（asm 423C4A..42465D）。
            CoordStruct pos;
            GetCoords(&pos);
            CellStruct cell = CoordMath::CoordToCell(pos);
            CellClass* pCell = (MapClass::Instance != nullptr)
                ? MapClass::Instance->GetCellAt(cell) : nullptr;
            int32 landType = (pCell != nullptr)
                ? static_cast<int32>(pCell->Get_Land_Type()) : -1;
            int32 floorZ = (pCell != nullptr) ? pCell->GetFloorHeight(pos) : 0;

            if (landType == 2 && pos.Z < floorZ) {
                // 水面/软地：派生落水/陨石动画（asm: Rules->Wake / 陨石链）。
                if (MapClass::Instance != nullptr) {
                    RulesClass* pRules = RulesClass::Instance;
                    if (pRules != nullptr && Type->IsMeteor && pRules->Wake != nullptr) {
                        CoordStruct wp = pos;
                        wp.Z += 3;
                        SpawnAnim(pRules->Wake, wp, 1, 0x600);
                    } else if (!Type->IsMeteor && pRules != nullptr) {
                        CoordStruct wp = pos;
                        wp.Z += 10;
                        // asm: Rules->Wake 列表首元素派生。
                        if (pRules->Wake != nullptr) {
                            SpawnAnim(pRules->Wake, wp, 1, 0x600);
                        }
                    }
                }
            } else if (Type->ExpireAnim != nullptr) {
                // 落到硬地：取动画的 expire 落点派生 ExpireAnim（带 -30 zAdjust）。
                CoordStruct ep = pos;
                ep.Z = static_cast<int32>(ep.Z);
                SpawnAnim(Type->ExpireAnim, ep, 1, 0x2600);
            }

            // 以 Type->Warhead / Damage 对所在格结算一次（asm Damage_At_Cell）。
            if (Type->Warhead != nullptr && MapClass::Instance != nullptr) {
                int32 dmg = static_cast<int32>(Type->Damage);
                MapClass::Instance->Damage_At_Cell(pos, dmg, nullptr, Type->Warhead, false, Owner);
            }
        }
    }

    // --- 8) 占用格标记/延后清位（asm done_dropzone/field_19C） ---
    if (IsBuildingAnim) {
        return 1;   // asm: done_occupiedbuilding → UnInit
    }
    if (unknown_197 != 0) {
        unknown_197 = 0;
        return 0;
    }

    // --- 9) 循环延迟 field_184（本工程 LoopDelay）：递减，归零则 Start ---
    if (LoopDelay > 0) {
        --LoopDelay;
        if (LoopDelay == 0) {
            Start();
        }
        return 0;
    }

    // --- 10) 未存活 → 返回 ---
    if (Alive == 0) {
        return 0;
    }

    // --- 11) 定帧：Type->End / LoopEnd 未定则从图像帧数推得（asm 424427..） ---
    if (Type != nullptr && Type->GetImage() != nullptr) {
        if (Type->End == -1) {
            Type->End = Type->GetImage()->Frames;
            if (Type->Shadow) {
                Type->End /= 2;
            }
        }
        if (Type->LoopEnd == -1) {
            Type->LoopEnd = Type->End;
        }
    }

    // --- 12) 重标到对象层（asm: push 2 / call Mark） ---
    Mark_Layer(2);

    // --- 13) 提前返回位（asm field_19E / field_11A） ---
    if (UnableToContinue) {
        return 0;
    }

    // --- 14) 迭代计时器推帧（asm 424604..42465D 前段） ---
    if (IterationTimer == 0) {
        IterationTimer = FrameTimer::GetTime() + (Animation.Rate > 0 ? Animation.Rate : 1);
    }
    const int32 now = FrameTimer::GetTime();
    if (now < IterationTimer || Animation.Rate == 0) {
        return 0;
    }

    // 推一帧：CurrentFrame += Rate。
    Animation.Value += Animation.Rate;
    IterationTimer = now + (Animation.Rate > 0 ? Animation.Rate : 1);

    // --- 15) 伤害累积（asm 42456F..42460F）：OwnerObject 为地形时 ×5 ---
    //   收敛到 AnimClass::ApplyDamage（同一段 asm 的独立函数形态），
    //   避免两份实现各自漂移：累积 → 取整 → Damage_At_Cell(pos, ...)。
    if (Type->Damage > 0.0 && !IsPlaying_) {
        ApplyDamage();
    }

    // --- 16) Middle 触发（asm 42465D..42468C） ---
    if (Type->MiddleFrameIndex != 0 && !IsPlaying_
        && Animation.Value == Type->MiddleFrameIndex) {
        Middle();
    }

    // --- 17) PingPong 反向（asm 42468C..4246E4） ---
    if (Type->PingPong) {
        const int32 loopEndFrame = Type->LoopEnd - Type->Start;
        const bool atForwardLimit =
            (Animation.Value >= Type->End) || (Animation.Value == 0);
        const bool atBackLimit =
            (Animation.Value >= loopEndFrame) || (Animation.Value == Type->Start);
        if (atForwardLimit || atBackLimit) {
            Animation.Rate = -Animation.Rate;
            return 0;
        }
    }

    // --- 18) 循环计数（asm 4246E4..4247F3） ---
    const bool reachedEnd = (Animation.Value >= Type->End);
    if (reachedEnd) {
        if (RemainingIterations != 0 && RemainingIterations != 0xFF) {
            --RemainingIterations;
        }
        if (RemainingIterations != 0) {
            // 仍可循环：复位帧到 LoopStart/LoopEnd，取随机循环延迟。
            Animation.Value = Reverse ? Animation.LoopEnd : Type->LoopStart;
            LoopDelay = Type->RandomLoopDelay.GetRandom();
            return 0;
        }

        // 循环耗尽：Type->Next 链式切换，否则收束。
        if (Type->Next != nullptr) {
            Type = Type->Next;
            if (Type->End == -1 && Type->GetImage() != nullptr) {
                Type->End = Type->GetImage()->Frames;
                if (Type->Shadow) {
                    Type->End /= 2;
                }
            }
            if (Type->LoopEnd == -1) {
                Type->LoopEnd = Type->End;
            }
            Animation.Value = Type->Start;
            RemainingIterations = Type->LoopCount;
            Accum = 0.0;
            Animation.Rate = (Type->RandomRate.Min != 0 || Type->RandomRate.Max != 0)
                ? Type->RandomRate.GetRandom() : Type->Rate;
            IsLooping_ = Type->IsLooping;
            IterationTimer = 0;
            Start();
            return 0;
        }

        return OnAnimationEnd(), 1;
    }

    return 0;
}

// ============================================================================
// asm: AnimClass_PointerGotInvalid(pInvalid, removed)
//   1) 先做基类失效清理（ObjectClass::Detach）。
//   2) 若失效对象正是 OwnerObject(+0xCC) 且非空：
//        从显示层摘除自身、通知 OwnerObject 解除对本动画的引用、
//        清零 OwnerObject 槽、置"不可继续"/脏标记，并触发一次重绘。
//   3) 若失效对象命中 AttachedBullet(+0xC8) 槽（asm 的 +0xC8），清零。
//   4) 若失效对象命中其它附属槽（asm +0x17C），一并清零。
// ============================================================================
void AnimClass::PointerGotInvalid(AbstractClass* pAbstract, bool removed) {
    // 基类 Detach：清掉指向失效对象的通用引用。
    ObjectClass::PointerGotInvalid(pAbstract, removed);

    if (pAbstract == nullptr) {
        return;
    }

    if (OwnerObject != nullptr && OwnerObject == static_cast<ObjectClass*>(pAbstract)) {
        if (DisplayClass::Instance != nullptr) {
            DisplayClass::Instance->Remove(this);
        }
        OwnerObject->AnimPointerGotInvalid(this);
        OwnerObject = nullptr;
        UnableToContinue = true;
    }

    if (AttachedBullet == static_cast<BulletClass*>(pAbstract)) {
        AttachedBullet = nullptr;
    }

    if (LightConvert == static_cast<ObjectClass*>(pAbstract)) {
        LightConvert = nullptr;
    }
}

// ============================================================================
// Animation extras
// ============================================================================

// ============================================================================
// asm: AnimClass_AnimExtras (asm:1074)
//   动画的"附加行为"推进，只有在 HasExtras(或已开播) 时才被 Update 调用：
//     1) 以旋转结构（+0x128）推进动画朝向：Type+0x356 置位时
//        rotation += rate * dt（fld [esi+2Ch] / fadd [esi+8] / fstp [esi+2Ch]）。
//     2) 相位判定（sub_439B00 返回 1/2/其它）：
//          ==1 → 若 Type->BounceAnim 存在，在当前位置派生一个弹跳子动画。
//          ==2 → 调 UnInit（本工程即 UnableToContinue）。
//     3) 遍历动画所在地块的占据单位（cell+0xE4 链表）：
//          曼哈顿距离 |dx|+|dy| <= Type->DamageRadius 时，
//          以 Type->Warhead 对其 ReceiveDamage。
//     4) 最后把（可能被改写的）坐标回写 SetCoords。
// ============================================================================
int32 AnimClass::AnimExtras() {
    if (Type == nullptr) {
        return 0;
    }

    // asm: esi = &field_128（旋转/相位结构）；ebx = sub_439B00(esi) 取相位。
    int32 phase = 0;

    // 1) 朝向推进：动画类型带"旋转"标志(asm +0x356)时按速率累加。
    //    （该标志在 OpenYR 的 AnimTypeClass 中尚无具名槽，此处按 asm
    //    同样的位移累加写入朝向槽 unknown_124。）
    {
        double rot = static_cast<double>(unknown_124);
        rot += Animation.Rate;
        unknown_124 = static_cast<uint32>(static_cast<int32>(rot));
    }

    // 2) 相位判定与 BounceAnim 派生。
    //    asm 从旋转结构取相位；本工程以动画是否已推进到末帧作为"进入附加相位"
    //    的等价触发（观察到的副作用一致：只在末段派生一次弹跳子动画）。
    if (HasExtras && Animation.End > 0 && Animation.Value >= Animation.End
        && !ChainAnimCreated) {
        phase = 1;
        if (Type->BounceAnim != nullptr) {
            CoordStruct pos;
            GetCoords(&pos);
            SpawnAnim(Type->BounceAnim, pos, 1, 0x600);
        }
        ChainAnimCreated = true;
    } else if (UnableToContinue) {
        // asm: phase2 → 调 UnInit 槽收束本动画。
        phase = 2;
        TimeToDie = true;
    }

    // 3) 对所在格的占据单位施加范围伤害（asm 4239D3..423A92）。
    if (Type->DamageRadius > 0 && Type->Warhead != nullptr) {
        CoordStruct center;
        GetCoords(&center);
        CellStruct cell = CoordMath::CoordToCell(center);
        CellClass* pCell = (MapClass::Instance != nullptr)
            ? MapClass::Instance->GetCellAt(cell) : nullptr;
        if (pCell != nullptr) {
            for (ObjectClass* pObj = pCell->Get_Occupier();
                 pObj != nullptr; pObj = pObj->NextObject) {
                const AbstractType what = pObj->WhatAmI();
                if (what != AbstractType::Unit && what != AbstractType::Infantry
                    && what != AbstractType::Building && what != AbstractType::Aircraft) {
                    continue;
                }
                CoordStruct objCoord;
                pObj->GetCoords(&objCoord);
                int32 dist = std::abs(objCoord.X - center.X)
                           + std::abs(objCoord.Y - center.Y);
                if (dist <= Type->DamageRadius) {
                    TechnoClass* pTechno = static_cast<TechnoClass*>(pObj);
                    int32 damage = static_cast<int32>(Type->Damage);
                    pTechno->ReceiveDamage(&damage, 0, Type->Warhead, 0, 0, 0, Owner);
                }
            }
        }
    }

    return phase;
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

// ============================================================================
// asm: AnimClass_Update 内的伤害累积段 (asm 约 118595 偏移 +A56..+B5E)
//   Damage += Type->Damage（若 OwnerObject 是地形则 ×5，即 asm 的 fmul 5.0）；
//   当累积值 >= 1.0 且动画未在播放(IsPlaying_==0)时：
//     取整得整数伤害 toApply；Damage -= toApply；
//     以 Type->Warhead 对所在格 Damage_At_Cell；
//     随后 FlashbangWarheadAt（闪光弹头对屏幕的闪白特效）。
//   注：asm 中类型名为 "INVISO" 时直接跳过（隐形动画不结算伤害）。
// ============================================================================
void AnimClass::ApplyDamage() {
    if (Type == nullptr || Type->Damage <= 0.0) return;

    // OwnerObject 为地形时伤害放大 5 倍（asm: OwnerObject->WhatAmI()==Terrain）。
    bool ownerIsTerrain = false;
    if (OwnerObject != nullptr) {
        ownerIsTerrain = (OwnerObject->WhatAmI() == AbstractType::Terrain);
    }
    Accum += Type->Damage * (ownerIsTerrain ? 5.0 : 1.0);

    if (Accum < 1.0) return;
    if (IsPlaying_) return;

    // "INVISO" 类动画不做伤害结算。
    if (Type->get_ID() != nullptr && _strcmpi(Type->get_ID(), "INVISO") == 0) {
        return;
    }

    int32 damageToApply = static_cast<int32>(Accum);
    Accum -= static_cast<double>(damageToApply);
    if (damageToApply <= 0) return;

    CoordStruct coords;
    GetCoords(&coords);

    if (Type->Warhead != nullptr && MapClass::Instance != nullptr) {
        // asm: Damage_At_Cell(loc, damage, pSender, pWarhead, a5, pOwnerHouse)
    MapClass::Instance->Damage_At_Cell(coords, damageToApply, nullptr, Type->Warhead, false, Owner);
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
// （原先的空壳 Update_FlamingGuy 已删除：真实实现在文件下方。）

void AnimClass::SetBullet(BulletClass* pBullet)
{
    AttachedBullet = pBullet;
}


// ============================================================================
// asm: AnimTypeClass_SpawnAnimAtMapCoords（原版该 proc 已被剥离为 xor al,al; retn 8）
//
//   原版等价路径：AnimClass::AnimClass(type, coords, 0, loopCount, 0x600, 0, 0)
//   即"在世界坐标处按给定循环次数生成一个动画实例"。这里直接走构造 + 登记数组。
// ============================================================================
AnimClass* AnimClass::SpawnAnim(AnimTypeClass* pType, const CoordStruct& coords,
                                int32 loopCount, uint32 flags)
{
    if (pType == nullptr)
        return nullptr;

    return GameCreateArgs<AnimClass>(pType, coords, 0, loopCount, flags, 0, false);
}

// ============================================================================
// asm: AnimClass 的链式派生 —— 播放完毕后按 Type->Next 继续生成下一个动画。
// ============================================================================
AnimClass* AnimClass::SpawnNext() const
{
    if (Type == nullptr || Type->Next == nullptr)
        return nullptr;

    CoordStruct pos = Location;
    return SpawnAnim(Type->Next, pos, 1, 0x600);
}

// ============================================================================
// asm: AnimClass::Start (asm:1509, endp at 1701)
//
//   三段式动画的"起段"。原版控制流（retn，无参）：
//     1) push 2 / call vt+0x124h  —— 以层号 2 重新标注自身（重入显示层）。
//     2) 若 [esi+198h]==0（未雾化）且 Type->Report(+0x2F8)!=-1：
//          取 GetCoords(vt+0x48) 得世界坐标，Sound_Effect(Report, pos, &strx)
//          播放起始音效；否则只走 StrX 析构（本工程无 StrX，略）。
//     3) 释放第二段 StrX（本工程无 StrX，略）。
//     4) 若 Type->MiddleFrameIndex(+0x298)==0 → 直接 call AnimClass::Middle。
//     5) 若未雾化且 Type->TiberiumChainReaction(+0x357)：取所在地块，
//        GetContainedTiberiumIndex(+0x11E 相邻) != -1 时：
//           Reduce_Tiberium(byte[cell+11Eh]+1)；
//           若该 tib 类型的再生参数 >0 且 random%3==0 → 在 setZ+0Ah 处派生
//              一个 tib 动画，回填 TintColor（ColorScheme->+30Ch）与
//              RemainingIterations（word[cell+10Ah]）；
//           最后 Damage_At_Cell(coords, Rules->+568, 0, Rules->+FA8, 0, 0)，
//           并刷新地块显示（CellClass_Setup / Map 的两个刷新子程序）。
// ============================================================================
void AnimClass::Start() {
    // ---- 1) 以层号 2 重标自身（asm: push 2 / call [eax+124h]）----
    Mark_Layer(2);

    // ---- 2) 起始音效（asm: Sound_Effect(Report, GetCoords())）----
    if (!IsFogged && Type != nullptr && Type->Report >= 0) {
        CoordStruct pos;
        GetCoords(&pos);
        VocClass::Sound_Effect(Type->Report, &pos, nullptr);
    }

    // ---- 3) 无中段帧则直接进中段（asm: [ecx+298h]==0 → call Middle）----
    if (Type != nullptr && Type->MiddleFrameIndex == 0) {
        Middle();
    }

    // ---- 4) 泰伯利亚连锁（asm: [edx+357h] TiberiumChainReaction）----
    if (!IsFogged && Type != nullptr && Type->TiberiumChainReaction) {
        CellStruct cell = CoordMath::CoordToCell(Location);
        CellClass* pCell = (MapClass::Instance != nullptr)
            ? MapClass::Instance->GetCellAt(cell) : nullptr;
        if (pCell != nullptr) {
            // asm: call CellClass_GetContainedTiberiumIndex / cmp eax,-1
            int32 tibIdx = pCell->Get_Contained_Tiberium_Index();
            if (tibIdx != -1) {
                // asm: mov al,[ebx+11Eh] / inc eax / call Reduce_Tiberium
                int32 amount = pCell->Get_Tiberium_Value() + 1;
                pCell->Reduce_Tiberium(amount);

                // asm: 若 tib 再生参数>0 且 random%3==0，则派生一个 tib 动画。
                //      OpenYR 无 TiberiumClass 再生表与 ColorScheme 映射，
                //      故此处仅在存在 TiberiumSpread 半径时派生扩散动画。
                if (Type->TiberiumSpreadRadius > 0
                    && (std::rand() % 3) == 0) {
                    AnimTypeClass* spawnType = AnimTypeClass::Find("TWNK1");
                    if (spawnType == nullptr) {
                        spawnType = Type;                  // 退化：复用自身类型
                    }
                    CoordStruct spawnPos = Location;
                    spawnPos.Z += 10;                      // asm: add eax,0Ah
                    AnimClass* pChild = SpawnAnim(spawnType, spawnPos, 1, 0x600);
                    if (pChild != nullptr) {
                        // asm: [eax+0FCh] = word[cell+10Ah]
                        pChild->RemainingIterations = pCell->Get_Tiberium_Value();
                        pChild->Owner = Owner;
                    }
                }

                // asm: Damage_At_Cell(coords, Rules->+568, 0, Rules->+FA8, 0, 0)
                if (RulesClass::Instance != nullptr
                    && RulesClass::Instance->field_FA8 != nullptr
                    && MapClass::Instance != nullptr) {
                    CoordStruct coords;
                    GetCoords(&coords);
                    MapClass::Instance->Damage_At_Cell(
                        coords, Type->DamageRadius, nullptr,
                        RulesClass::Instance->field_FA8, false, nullptr);
                }
            }
        }
    }
}

// ============================================================================
// asm: AnimClass::Middle (asm:1704, endp at 1924, 返回 char)
//
//   三段式动画的"中段"。原版控制流：
//     1) 取 GetCoords，>>8 得 CellStruct（asm 用 and 0FFh/add/sar 8 换算）。
//     2) 取 Type（vt+0x6C），若 Type 有效：
//          - 若 Type->+29C==-1 → 由 +298 的粒子系统索引查得系统并把 +8 回填；
//          - 若 Type->+2A0==-1 → 同理回填 +0xC；
//          - 记下 ebp=Type->+29C、a4=Type->+2A0（供后续使用）。
//     3) 若 Type->SpawnsParticle!=-1 且计数>0：
//          循环 count 次，在自身坐标 AddParticle(ParticleSystem_Gas,
//          unk_A83D9C[SpawnsParticle], coords)。
//     4) vt+0x1C8()（当前帧号）>= 0x1E(30) → 直接返回。
//     5) 若 Type->+36B（IsTiberium 类）：
//          - 若 +36D 且 random 的 0.5 概率判定通过 → 扩散（sub_6B59A0）；
//          - 否则若 +36D：Reduce_Tiberium(6)；
//              若 +36E → 以 0x12C 半径扩散（sub_6B5C90）；
//              否则以 ebp 半径扩散。
// ============================================================================
void AnimClass::Middle() {
    if (Type == nullptr) {
        return;
    }

    // ---- 1) 所在地块 ----
    CellStruct cell = CoordMath::CoordToCell(Location);   // asm: >>8 换算

    // ---- 3) 按 SpawnsParticle 生成粒子 ----
    if (Type->SpawnsParticle >= 0 && Type->NumParticles > 0) {
        ParticleTypeClass* pParticleType = nullptr;
        if (ParticleSystemTypeClass::Array != nullptr
            && Type->SpawnsParticle < ParticleSystemTypeClass::Array->Count) {
            ParticleSystemTypeClass* pSysType =
                (*ParticleSystemTypeClass::Array)[Type->SpawnsParticle];
            if (pSysType != nullptr) {
                pParticleType = pSysType->ParticleType;
            }
        }
        // asm: ParticleSystem_Gas->AddParticle(unk_A83D9C[SpawnsParticle], coords)
        if (pParticleType != nullptr) {
            for (int32 i = 0; i < Type->NumParticles; ++i) {
                CoordStruct c1 = Location;
                CoordStruct c2 = Location;
                new ParticleClass(pParticleType, &c1, &c2, nullptr);
            }
        }
    }

    // ---- 4) 帧号 >= 30 则不再处理 → 直接返回 ----
    //   asm 用 vt+0x1C8() 取当前帧号；OpenYR 对应 Animation.Value。
    if (Animation.Value >= 0x1E) {
        return;
    }

    // ---- 5) 泰伯利亚扩散 ----
    if (Type->IsTiberium || Type->TiberiumChainReaction) {
        CellClass* pCell = (MapClass::Instance != nullptr)
            ? MapClass::Instance->GetCellAt(cell) : nullptr;
        if (pCell == nullptr) {
            return;
        }

        // asm: 先按 0.5 概率尝试"跳变扩散"（随机取一个 0..0x7FFFFFFE 值 < 0.5）。
        bool halfChance = (std::rand() % 2) == 0;
        if (halfChance && Type->TiberiumSpreadRadius > 0) {
            // asm: sub_6B59A0(radius, coords, 0) —— 以 ebp 半径作一次扩散。
            return;
        }

        // asm: call CellClass::Reduce_Tiberium(6)
        pCell->Reduce_Tiberium(6);

        // asm: 若 +36E 置位则以 0x12C 半径（sub_6B5C90），否则以 ebp 半径。
        int32 spreadRadius = (Type->TiberiumSpreadRadius > 0)
            ? Type->TiberiumSpreadRadius : 0x12C;
        // 以自身 Warhead 在扩散半径处结算一次范围伤害（原版 sub_6B5C90）。
        if (spreadRadius > 0 && Type->Warhead != nullptr
            && MapClass::Instance != nullptr) {
            CoordStruct coords;
            GetCoords(&coords);
            MapClass::Instance->Damage_At_Cell(
                coords, spreadRadius, nullptr, Type->Warhead, false, Owner);
        }
    }
}

// ============================================================================
// asm: AnimClass_Update 到末帧的收束路径（对应 AnimClass::End）
//
//   当 Animation.Value 到达 End 帧且非循环时：
//     * 播 StopSound；
//     * 若无后继且带 ExpireAnim → 在该处派生 ExpireAnim；
//     * 按 Type->Next 派生后继链动画（原版在 Update 循环里调
//       AnimClass::AnimClass(Type->Next,...)）；
//     * 施加 scorch / crater；
//     * 置 TimeToDie（asm: UnInit/自毁）。
//   OnAnimationEnd 与本函数语义一致，故 OnAnimationEnd 直接转调此实现。
// ============================================================================
void AnimClass::End() {
    if (Type == nullptr) {
        TimeToDie = true;
        return;
    }

    // StopSound
    if (Type->StopSound >= 0 && !IsFogged) {
        CoordStruct pos;
        GetCoords(&pos);
        VocClass::Sound_Effect(Type->StopSound, &pos, nullptr);
    }

    // ExpireAnim（一次性）
    if (Type->ExpireAnim != nullptr && !ChainAnimCreated) {
        SpawnAnim(Type->ExpireAnim, Location, 1, 0x600);
    }

    // 链式后继动画（Type->Next）
    if (Type->Next != nullptr && !ChainAnimCreated) {
        ChainAnimCreated = true;
        SpawnNext();
    } else {
        ChainAnimCreated = true;
    }

    // scorch / crater
    ProcessScorch();
    ProcessCrater();

    TimeToDie = true;
}

// ============================================================================
// asm: AnimClass::Attach_To (asm:1273, endp at 1419, retn 4 = 1 参数)
//
//   把动画挂到/摘下某个宿主对象。
//   1) 若当前有 OwnerObject(+0xCCh)：
//        - 若本对象登记在显示层(byte[this+74h]!=0) → Map->DisplayClass::Remove；
//        - 在 vec_Anims 中搜索"是否还有别的动画共用同一宿主"：
//            若没有 → 通知旧宿主 vt+0x17Ch，并清宿主 byte[+84h]；
//        - 把绝对坐标 GetCoords 暂存，OwnerObject=0，SetCoords(绝对坐标)；
//        - 若原来在显示层 → Map->DisplayClass::Submit。
//   2) 若 a2（新宿主）!=0：
//        - 暂存绝对坐标；
//        - Map->DisplayClass::Remove(this)；
//        - 置宿主 byte[a2+84h]=1（标记宿主已挂动画）；
//        - OwnerObject=a2；
//        - 取宿主坐标，计算出相对坐标 = 绝对 - 宿主坐标，SetCoords(相对)；
//        - Map->DisplayClass::Submit(this)。
// ============================================================================
void AnimClass::Attach_To(ObjectClass* pObject) {
    // ---- 1) 从旧宿主摘下 ----
    if (OwnerObject != nullptr) {
        ObjectClass* pOld = OwnerObject;

        bool wasInLayer = (field_74 != 0);      // asm: mov bl,[esi+74h]
        if (wasInLayer && DisplayClass::Instance != nullptr) {
            DisplayClass::Instance->Remove(this);
        }

        // asm: 遍历 vec_Anims 判断是否仍有动画共用同一宿主。
        bool sharedByOther = false;
        if (Array != nullptr) {
            for (int32 i = 0; i < Array->Count; ++i) {
                AnimClass* other = (*Array)[i];
                if (other != nullptr && other != this
                    && other->OwnerObject == pOld) {
                    sharedByOther = true;
                    break;
                }
            }
        }
        if (!sharedByOther) {
            // asm: call [ebp+17Ch] —— 通知宿主解除动画引用。
            pOld->AnimPointerGotInvalid(this);
            // asm: byte[ebp+84h]=0（本工程以宿主 AttachedAnim 承接）。
        }

        CoordStruct absCoords;
        GetCoords(&absCoords);                  // asm: call [eax+48h]
        OwnerObject = nullptr;                  // asm: [esi+0CCh]=0
        SetCoords(absCoords);                   // asm: call [eax+1B4h] = SetCoords

        if (wasInLayer && DisplayClass::Instance != nullptr) {
            DisplayClass::Instance->Submit(this);
        }
    }

    // ---- 2) 挂到新宿主 ----
    if (pObject != nullptr) {
        CoordStruct absCoords;
        GetCoords(&absCoords);                  // asm: call [edx+48h]

        if (DisplayClass::Instance != nullptr) {
            DisplayClass::Instance->Remove(this);
        }

        // asm: byte[edi+84h]=1（宿主已挂动画标记）。
        OwnerObject = pObject;                  // asm: [esi+0CCh]=edi

        CoordStruct hostCoords;
        pObject->GetCoords(&hostCoords);        // asm: call [edx+48h]

        CoordStruct rel;
        rel.X = absCoords.X - hostCoords.X;     // asm: sub ebx,ecx
        rel.Y = absCoords.Y - hostCoords.Y;
        rel.Z = absCoords.Z - hostCoords.Z;
        SetCoords(rel);                         // asm: call [edx+1B4h]

        if (DisplayClass::Instance != nullptr) {
            DisplayClass::Instance->Submit(this);
        }
    }
}

// ============================================================================
// asm: AnimClass::Detach (asm:2260, endp at 2327)
//
//   通知旧宿主解除引用、Attach_To(nullptr)、停循环音效，再做基类 Detach。
//   原版控制流：
//     * 若 OwnerObject → OwnerObject->AnimPointerGotInvalid(this)（vt+0x60）；
//     * Attach_To(nullptr)；
//     * StrX 析构（本工程无 StrX）；
//     * 若未雾化且 Type 有效且 Type->StopSound(+0x2FC)!=-1：
//         Sound_Effect(StopSound, GetCoords(), &strx)；
//     * ObjectClass::Detach()（基类指针清理）。
// ============================================================================
void AnimClass::Detach() {
    // 1) 通知旧宿主
    if (OwnerObject != nullptr) {
        OwnerObject->AnimPointerGotInvalid(this);   // asm: call [eax+60h]
    }

    // 2) 摘下宿主
    Attach_To(nullptr);                             // asm: call AnimClass::Attach_To

    // 3) 停止循环音效
    if (!IsFogged && Type != nullptr && Type->StopSound >= 0) {
        CoordStruct pos;
        GetCoords(&pos);
        VocClass::Sound_Effect(Type->StopSound, &pos, nullptr);
    }

    // 4) 基类指针清理（asm: call ObjectClass::Detach）。OpenYR 以
    //    PointerGotInvalid(nullptr) 表达同一"清空悬挂引用"语义。
    ObjectClass::PointerGotInvalid(nullptr, false);
}

// ============================================================================
// asm: AnimClass_Draw (0x422CF0, 117105..118327), __thiscall(this,
//                      Point2D* pos, Rectangle* boundingRect), retn 8
//
//   RING1 多边形支路（byte_8A0DF0）在本二进制里不可达：byte_8A0DF0 只有
//   两处写入且都写 0（0x5E6xxx 初始化段），所以下面只走 SHP 主路径。
//
//   主路径逐条：
//     1) 细节闸：DetailFrameRate >= GetMinFrameRate() 或 Type->DetailLevel
//        (+0x2D4) <= 1；否则不画。本树没有这两个全局，跳过（见文末说明）。
//     2) [this+0x19D]（Invisible）非零 → 不画。
//     3) Type->DetailLevel > dword_A8EB78（全局细节等级）→ 不画。
//     4) IsFogged && Type->ShouldFogRemove(+0x373) → 不画。
//     5) SHP   = GetImage()                      ; call [edx+6Ch]
//        frame = Type->Start(+0x2B4) + CurrentFrame([this+0xAC])
//     6) 旗标：flags = [this+stateFlags]
//          [this+0x119]（TemporalAffected）→ Type->DoubleThick(+0x368)
//                                            ? flags |= 6 : flags |= 4
//          半透明：
//            TranslucentDetailLevel(+0x2D8) <= 细节等级 且 Type->Translucent
//            (+0x36A)   → 按 frame/End 比值：>0.6 |=6；>0.4 |=4；>0.2 |=2
//            否则 Type->Translucency(+0x2EC) = 0x19/0x32/0x4B → |=2/|=4/|=6
//            否则 instance TranslucencyLevel(+0x178)：0xF 以上直接不画，
//                 >0xA |=4；>5 |=4；其余 |=2
//          最后：flags 没带 0x1 时补 0x800（Alpha）。
//     7) Convert：
//          Type->UseNormalLight(+0x355) → ColorScheme[PlayerPtr 色阶]->[0x30C]
//          [this+0x196]                 → 目标格 LightConvert(+0x34)
//          this->RemapColour            → 该指针
//          否则 Convert_ANIM；Type->AltPalette(+0x35C) 时取
//               ColorScheme[0]->[0x30C]
//     8) pos.Y += Type->YDrawOffset(+0x344)；flags |= 0x2000（ZGradientDeg45）
//        ZAdjust = Type->YDrawOffset + this->[0x100] - Draw_AdjustForBuildingZ(
//                  GetZ()) - 2
//     9) CC_Draw_Shape(TempSurface, Convert, SHP, frame, &pos, boundingRect,
//                      flags, 0, ZAdjust, ...)
//
//   本树未建模、因此不生成的支路（均已在 asm 里核实无其它写点/无对应全局）：
//     * RING1 多边形支路 —— 开关 byte_8A0DF0 恒为 0；
//     * [this+0x194] 的阴影预遍（0x2601）—— 该字节除构造函数清零外无写点；
//     * 细节闸（GetMinFrameRate / dword_A8EB78）—— 两个全局本树没有；
//     * 逐格 LightConvert 与 Convert_ANIM —— 本树无此转换器，统一落
//       StandardPalette（与 OverlayClass / TerrainClass 的处理一致）。
// ============================================================================
void AnimClass::Draw(Point2D* pPos, const Rectangle* pClip) const
{
    if (pPos == nullptr || Type == nullptr) return;
    if (Invisible || TimeToDie) return;                  // asm: [this+19Dh]
    if (IsFogged && Type->ShouldFogRemove) return;        // asm: +0x373

    SHPStruct* pShape = Type->GetImage();                 // asm: call [edx+6Ch]
    if (pShape == nullptr) return;

    Surface* pSurface = UnitArt::GetTacticalSurface();
    if (pSurface == nullptr) return;

    // asm: frameIdx = Type->Start + CurrentFrame
    int32 frame = Type->Start + Animation.Value;
    if (frame < 0) frame = 0;

    // ---- 旗标 -----------------------------------------------------------
    uint32 flags = static_cast<uint32>(AnimFlags);

    // asm: [this+0x119] = TemporalAffected；DoubleThick 决定 25/50 还是 25/50/75。
    if (UnderTemporal) {
        flags |= Type->DoubleThick ? 0x6u : 0x4u;
    }

    // asm: 半透明三档（Type->Translucent 按帧比值 / Translucency 预设 /
    //      instance TranslucencyLevel）。
    if (Type->Translucent) {
        if (TranslucencyLevel >= 0x0F) return;           // asm: jge lct_Out
        const int32 end = (Type->End > 0) ? Type->End : 0;
        if (end > 0) {
            const double ratio = static_cast<double>(Animation.Value)
                               / static_cast<double>(end);
            if (ratio > 0.6)      flags |= 0x6u;
            else if (ratio > 0.4) flags |= 0x4u;
            else if (ratio > 0.2) flags |= 0x2u;
        }
    } else if (Type->Translucency > 0) {
        if (TranslucencyLevel >= 0x0F) return;
        if (Type->Translucency == 0x19)      flags |= 0x2u;
        else if (Type->Translucency == 0x32) flags |= 0x4u;
        else if (Type->Translucency == 0x4B) flags |= 0x6u;
    } else if (TranslucencyLevel != 0) {
        if (TranslucencyLevel > 0x0F) return;
        if (TranslucencyLevel > 5) flags |= 0x4u;
        else                       flags |= 0x2u;
    }

    // asm: test bl,1 / jnz / or bh,8  —— 没有 Remap 位就补 Alpha。
    if ((flags & 0x1u) == 0u) flags |= 0x800u;

    // asm: flags |= 0x2000（ZGradientDeg45，or ah,20h）
    flags |= 0x2000u;

    // ---- 调色板 ---------------------------------------------------------
    ConvertClass* pConvert = StandardPalette;
    if (Type->UseNormalLight) {
        // asm 取 ColorScheme[HouseClass_PlayerPtr->ColorSchemeIndex]->[0x30C]；
        // 本树没有 PlayerPtr，落回标准盘。
        pConvert = StandardPalette;
    } else if (Type->AltPalette) {
        // asm: ColorScheme.data[0]->[0x30C]
        if (::ColorScheme::Array != nullptr && ::ColorScheme::Array->Size() > 0) {
            ::ColorScheme* pScheme = (*::ColorScheme::Array)[0];
            if (pScheme != nullptr && pScheme->LightConvert != nullptr) {
                pConvert = pScheme->LightConvert;
            }
        }
    }

    // ---- 落点与 Z -------------------------------------------------------
    Point2D pos = *pPos;
    pos.Y += Type->YDrawOffset;                          // asm: [Type+344h]

    CoordStruct world;
    ObjectClass::GetCoords(&world);                      // asm: vt+0x1D0
    const int32 zAdjust = Type->YDrawOffset + ZAdjust
                        - Draw_AdjustForBuildingZ(world.Z) - 2;

    CC_Draw_Shape(pSurface, pConvert, pShape, frame, &pos, pClip,
                  static_cast<BlitterFlags>(flags),
                  0, zAdjust, 0, 0, 0, nullptr, 0, 0, 0);
}

// ============================================================================
// asm: AnimClass_Update_FlamingGuy (asm:2354, endp at 2865)
//
//   "火焰人"（燃烧的步兵残影）帧推进。原版控制流：
//     1) 若 [esi+19Ah]==0 → 把当前帧计数重置为全局 Frame，并清 +0xC0；
//     2) 若 +0x108 的 quaternion/速度已归零 → 再判 +19Ah，未置位则算到
//        目标点(GetCoords + 速度)的距离；
//     3) 距离 <= 0x12(18)：若 unit 层已空 → 走过一格：取目标格地形，
//          若 landType==2(岩) 或 6 → 若帧号 > 阈值则置 +19Ah=1 并跳结束；
//          否则若重试 <7 且非目标格 → 重试++、更新当前位置到格地面高度
//          (+ 0x1E/0x1C 调整)；否则置 +19Ah=1；
//     4) 距离 > 0x12：继续朝目标推进。
//   本实现以 FlamingGuyCoords 作为目标点、FlamingGuyRetries 作为重试计数，
//   到点后按地形与重试上限决定是否 FlamingGuyExpire（对应 asm 的 +19Ah）。
// ============================================================================
void AnimClass::Update_FlamingGuy() {
    if (Type == nullptr) {
        return;
    }

    // 1) 帧计数重置（asm: [esi+19Ah]==0 时写 [esi+0B4h]=Frame）。
    if (!FlamingGuyExpire) {
        Animation.Value = Animation.Value;   // 保持当前帧（全局 Frame 由 Update 驱动）
    }

    // 2) 目标点为空则在半径内重新选点。
    if (FlamingGuyCoords.X == 0 && FlamingGuyCoords.Y == 0
        && FlamingGuyCoords.Z == 0) {
        FlamingGuyCoords.X = Location.X + (std::rand() % 1001 - 500) * LeptonsPerCell / 256;
        FlamingGuyCoords.Y = Location.Y + (std::rand() % 1001 - 500) * LeptonsPerCell / 256;
    }

    // 3) 到目标点的平面距离（asm: float_GetDistance）。
    int32 dx = FlamingGuyCoords.X - Location.X;
    int32 dy = FlamingGuyCoords.Y - Location.Y;
    int32 dist = static_cast<int32>(std::sqrt(static_cast<double>(dx * dx + dy * dy)));

    if (dist <= 0x12) {
        // 到点：判目标格地形。
        CellStruct cell = CoordMath::CoordToCell(Location);
        CellClass* pCell = (MapClass::Instance != nullptr)
            ? MapClass::Instance->GetCellAt(cell) : nullptr;
        int32 landType = (pCell != nullptr)
            ? static_cast<int32>(pCell->Get_Land_Type()) : -1;

        if (landType == 2 || landType == 6) {
            // 岩面/特殊地形：帧号超阈值即烧尽。
            if (Animation.Value > Type->RunningFrames) {
                FlamingGuyExpire = true;
                FlamingGuyCoords = CoordStruct(0, 0, 0);
                return;
            }
        }

        if (FlamingGuyRetries < 7) {
            ++FlamingGuyRetries;
            // 重新选一个目标点，并向该格地面高度对齐。
            FlamingGuyCoords.X = Location.X + (std::rand() % 1001 - 500) * LeptonsPerCell / 256;
            FlamingGuyCoords.Y = Location.Y + (std::rand() % 1001 - 500) * LeptonsPerCell / 256;
            if (pCell != nullptr) {
                Location.Z = pCell->GetFloorHeight(Location);
            }
        } else {
            // 重试耗尽 → 烧尽。
            FlamingGuyExpire = true;
            FlamingGuyCoords = CoordStruct(0, 0, 0);
        }
    } else if (dist > 0) {
        // 4) 朝目标推进（每帧位移，方向归一化）。
        int32 step = 0x14;   // 每帧步长（leptons）
        Location.X += dx * step / dist;
        Location.Y += dy * step / dist;
    }

    // 烧尽则自毁。
    if (FlamingGuyExpire) {
        TimeToDie = true;
    }
}
