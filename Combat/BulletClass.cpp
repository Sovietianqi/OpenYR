// =============================================================================
// BulletClass.cpp - Projectile (bullet/missile/shell) entity implementation
//
// Rewritten against the original binary (gamemd.exe).  Every routine below is
// tagged with the asm line range of its proc in the full listing; the semantic
// model follows the asm field usage (offsets noted in BulletClass.h).
//
// Reference procs (full listing line numbers):
//   BulletClass_CTOR              243600..243699
//   BulletClass_Initialize        243700..243764
//   BulletClass_DTOR              243765..243871
//   BulletClass_SetLayer          243922..243935
//   BulletClass_Update            243943..246416
//   BulletClass_GetAnimRate       246348..246407
//   BulletClass_Draw              246417..246778
//   BulletClass_TargetWentAway    246779..246856
//   BulletClass_PointerGotInvalid 246857..247055
//   BulletClass_SetMovement       247056..247535
//   BulletClass_InWhichLayer      247536..247545
//   BulletClass_Detonate          247786..248128
//   BulletClass_Fire              248129..249964
//   BulletClass_Shrapnel          249965..250958
//   BulletClass_Load/Save         250959..251120
//   BulletClass_IsHoming_obsolete 251188..251203
//   BulletType_CreateBullet       251204..251272
//   BulletClass_DrawVXL           251273..251448
//   BulletClass_InitScalable      251479..251551
//   BulletClass_NukeMaker         251552..251747
// =============================================================================

#include "BulletClass.h"
#include "BulletTypeClass.h"
#include "WeaponTypeClass.h"
#include "WarheadTypeClass.h"
#include "DamageArea.h"
#include "CaptureManagerClass.h"
#include "ParasiteClass.h"
#include "BombClass.h"
#include "../AI/TagClass.h"
#include "../Audio/VocClass.h"
#include "../Abstract/ObjectClass.h"
#include "../Abstract/ObjectTypeClass.h"
#include "../Abstract/TechnoClass.h"
#include "../Abstract/TechnoTypeClass.h"
#include "../Abstract/FootClass.h"
#include "../Abstract/BuildingClass.h"
#include "../Abstract/InfantryClass.h"
#include "../Houses/HouseClass.h"
#include "../Rules/RulesClass.h"
#include "../Scenario/ScenarioClass.h"
#include "../Game/Game.h"
#include "../Game/GameInit.h"
#include "../Map/MapClass.h"
#include "../Map/CellClass.h"
#include "../Math/CoordStruct.h"
#include "../Animations/AnimClass.h"
#include "../Animations/AnimTypeClass.h"
#include "../Rendering/TacticalClass.h"
#include "../Rendering/Surface.h"
#include "../Rendering/RadarEventClass.h"
#include "../Rendering/DisplayClass.h"
#include "../Rendering/ConvertClass.h"
#include "../Rendering/UnitArt.h"
#include "../Rendering/VoxelRaster.h"
#include "../Helpers/AsmUIHelpers.h"
#include "../FileFormats/VXL.h"
#include "../FileFormats/HVA.h"
#include "../Special/RadSiteClass.h"
#include "../Special/TemporalClass.h"
#include "../INI/INIClass.h"

#include <cstring>
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <vector>

// =============================================================================
// Constants (leptons unless stated)
// =============================================================================
static const int32  BULLET_MIN_SPEED           = 1;
static const int32  BULLET_MAX_SPEED           = 10000;
static const int32  BULLET_GRAVITY_PER_FRAME   = 2;      // straight/dropping
static const int32  BULLET_ANIM_Z_OFFSET       = 42;     // impact anim lift
static const int32  BULLET_DEFAULT_SPEED       = 100;
static const int32  BULLET_FLAK_SCATTER_LIMIT  = 64;
static const int32  BULLET_HIT_RADIUS          = 0x40;   // 64 lep proximity
static const int32  BULLET_CELL_SIZE           = 256;
// asm: const_int_ObjectClass_1A0h —— TechnoClass_GetHeight 在 IsInAir(+0x8Ch)
//   置位时扣掉的空中层基准高度 0x1A0(=416)。
static const int32  BULLET_AIR_LAYER_Z         = 0x1A0;
static const double BULLET_DIRECTION_EPSILON   = 1e-6;
static const double BULLET_PI                  = 3.14159265358979323846;

// asm: dword_7F4890 -- the 32-entry heading-bucket -> sprite frame table used
// by GetAnimRate when the projectile does not rotate.  Values read verbatim
// from the binary (207..2057322 lines of the listing).
static const int32 kBulletFacingFrame[32] = {
    0x1C, 0x1B, 0x1A, 0x19, 0x18, 0x17, 0x16, 0x15,
    0x14, 0x13, 0x12, 0x11, 0x10, 0x0F, 0x0E, 0x0D,
    0x0C, 0x0B, 0x0A, 0x09, 0x08, 0x07, 0x06, 0x05,
    0x04, 0x03, 0x02, 0x01, 0x00, 0x1F, 0x1E, 0x1D,
};

// =============================================================================
// Small vector helpers (asm: the float_sqrt / normalization blocks)
// =============================================================================
static inline void Bullet_Normalize(double v[3]) {
    const double len = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (len <= BULLET_DIRECTION_EPSILON) {
        // asm: all-zero vector -> (0, 100.0, 0) sentinel (loc_466AB8).
        v[0] = 0.0; v[1] = 100.0; v[2] = 0.0;
        return;
    }
    v[0] /= len; v[1] /= len; v[2] /= len;
}

static inline double Bullet_Length(const double v[3]) {
    return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

static inline double Bullet_Length3(int32 x, int32 y, int32 z) {
    return std::sqrt(static_cast<double>(x) * x +
                     static_cast<double>(y) * y +
                     static_cast<double>(z) * z);
}

static inline int32 Bullet_FloorToInt(double d) {
    return static_cast<int32>(std::floor(d));
}

static inline int32 Bullet_Distance3D(const CoordStruct& a, const CoordStruct& b) {
    return static_cast<int32>(Bullet_Length3(b.X - a.X, b.Y - a.Y, b.Z - a.Z));
}

static inline int32 Bullet_Distance2D(const CoordStruct& a, const CoordStruct& b) {
    const int32 dx = b.X - a.X;
    const int32 dy = b.Y - a.Y;
    return static_cast<int32>(std::sqrt(static_cast<double>(dx) * dx
                                      + static_cast<double>(dy) * dy));
}

// 8-way facing from a delta, used for the cached project-side facing.
static FacingType Bullet_ComputeFacing(int32 dx, int32 dy) {
    double angle = std::atan2(static_cast<double>(dx), static_cast<double>(-dy));
    if (angle < 0.0) angle += 2.0 * BULLET_PI;
    const double degrees = angle * 180.0 / BULLET_PI;
    const int32 facing = static_cast<int32>((degrees + 22.5) / 45.0) & 7;
    return static_cast<FacingType>(facing);
}

// asm: the integer part of a double vector (Float_To_Int_Floor call sites).
static inline int32 Bullet_VecToInt(double d) {
    return Bullet_FloorToInt(d);
}

// =============================================================================
// Construction / destruction
// =============================================================================

// asm: BulletClass_CTOR (243600..243698), BulletClass_SubCTOR at +0x23.
BulletClass::BulletClass(BulletTypeClass* pType) noexcept
    : ObjectClass(),
      Damage(0), Strength(0), Health(0), DamageMultiplier(256),
      Class(pType), Owner(nullptr), Target(nullptr),
      Warhead(nullptr), WeaponType(nullptr),
      IsBulletActive(true), IsBulletDetonated(false), IsInAir(false),
      IsIncoming(false), IsAnimating(false), IsArmed(true), IsHoming(true),
      MustSpawnNextAnim(false), Bright(false), IsGravity(false),
      IsInaccurate(false), IsAA(false), IsAG(false), IsAS(false), IsFlat(false),
      Speed(BULLET_DEFAULT_SPEED), ArmCounter(0), Unknown_100(0),
      ColorScheme(-1), TrackFrames(0), TrackAccum(0.0),
      Timer(0), Timer2(0), AnimFrame(0), AnimDelay(0), Unknown_150(0x100),
      Motion(BulletMotion::Straight),
      CellX(0), CellY(0), NextAnim(nullptr),
      Range(0), DistanceTraveled(0), CreationFrame(0),
      Facing(FacingType::N), Next(nullptr), Prev(nullptr) {
    // asm: [ecx+0E8h]=0, [ecx+0ECh]=40590000h (100.0) -- default velocity.
    Velocity[0] = 0.0; Velocity[1] = 100.0; Velocity[2] = 0.0;
    Location.X = Location.Y = Location.Z = 0;
    SourceCoords = Location;
    FuseCoords = Location;
    TargetCoords = Location;
    LastCoords = Location;

    // asm: BulletClass_CTOR+0x23 call BulletClass_SubCTOR
    SubCTOR();

    // asm: [esi+104h]=1 (IsArmed), [esi+105h]=1 (IsHoming), [+114h]=-1.
    IsArmed = true;
    IsHoming = true;
    ColorScheme = -1;

    if (pType != nullptr) {
        IsAA = pType->AA;
        IsAG = pType->AG;
        IsAS = pType->ASW;
        IsInaccurate = pType->Inaccurate;
        IsGravity = pType->Dropping;
        IsFlat = pType->Flat;
        Motion = pType->Dropping ? BulletMotion::Dropping
               : pType->Vertical ? BulletMotion::Vertical
               : pType->Arcing   ? BulletMotion::Arcing
                                 : BulletMotion::Straight;
    }

    // asm: ObjectClass_CTOR 把 +81h（IsInLimbo）置 1 —— 新对象以"未落位"
    //   状态交出，随后 BulletClass_SetMovement → ObjectClass::Unlimbo 才允许
    //   落位。本树 ObjectClass 默认 0，若不在这里补，Unlimbo 的第一道门
    //   （`!IsInLimbo -> false`）直接把弹体挡回去：速度没写、不进战术层，
    //   Update 里 |v|==0 判成"已停滞"，弹体在炮口原地起爆、伤害打空。
    IsInLimbo = true;

    // asm: vec_Bullets.length++ / data[len] = this
    if (BulletClass::Array != nullptr) {
        BulletClass::Array->Add(this);
    }
}

// asm: BulletClass_DTOR (243765..243870)
BulletClass::~BulletClass() {
    if (BulletClass::Array != nullptr) {
        for (int32 i = 0; i < BulletClass::Array->Count; ++i) {
            if ((*BulletClass::Array)[i] == this) {
                BulletClass::Array->Remove(i);
                break;
            }
        }
    }
}

// =============================================================================
// SubCTOR - trajectory-header initializer (asm: BulletClass_SubCTOR, 495313)
//   Stamps the birth frame and clears the trailing state block.
// =============================================================================
void BulletClass::SubCTOR() {
    const int32 frame = (Game::CurrentFrame > 0) ? Game::CurrentFrame : 0;
    CreationFrame = frame;
    Timer = 0;          // asm: Timer_StartWithDuration(this+0xB8, 0)
    Timer2 = 0;         // asm: Timer_StartWithDuration(this+0xC4, 0)
    TrackFrames = 0;
    TrackAccum = 0.0;
    ArmCounter = 0;
    Unknown_100 = 0;
}

// =============================================================================
// Initialize - asm: BulletClass_Initialize (243700..243764)
//   type=+4, pos=+8, Owner=+0Ch, Damage=+10h, WH=+14h, Speed=+18h, Bright=+1Ch
//   retn 1Ch -> 7 args.
// =============================================================================
void BulletClass::Initialize(BulletTypeClass* pType, CoordStruct pos,
                             TechnoClass* pOwner, int32 damage,
                             WarheadTypeClass* pWarhead, int32 speed, bool bright) {
    Class = pType;                                   // asm: [ecx+0ACh]=eax
    WeaponType = nullptr;
    Health = 0;

    // asm: [ecx+10Ch]=pos -- the original stores the *launch position* here
    // (it is then overwritten with the target/cell once the bullet is placed).
    Target = nullptr;
    Location = pos;
    SourceCoords = pos;
    FuseCoords = pos;
    TargetCoords = pos;
    StartCoords = pos;

    Speed = speed;                                   // asm: [ecx+110h]=Speed
    Warhead = pWarhead;                              // asm: [ecx+128h]=WH
    Bright = bright;                                 // asm: [ecx+Bright]
    Damage = damage;                                 // asm: [ecx+6Ch]=Damage
    Strength = damage;
    Owner = pOwner;                                  // asm: [ecx+0B0h]=Owner
    DistanceTraveled = 0;

    AnimFrame = 0;                                   // asm: [ecx+12Ch]=0
    AnimDelay = (pType != nullptr) ? static_cast<uint8>(pType->AnimRate) : 0;

    // asm: type+0x2A9 (FirersPalette) and owner -> colour-scheme index.
    if (pType != nullptr && pType->FirersPalette && pOwner != nullptr) {
        HouseClass* pHouse = pOwner->GetOwningHouse();
        ColorScheme = (pHouse != nullptr) ? pHouse->ColorSchemeIndex : -1;
    } else {
        ColorScheme = -1;                            // asm: 0FFFFFFFFh
    }

    Unknown_150 = 0x100;                             // asm: [ecx+150h]=100h
    NextAnim = nullptr;                              // asm: [ecx+154h]=0
    MustSpawnNextAnim = false;                       // asm: [ecx+158h]=0

    // Derived flight state (the original recomputes these lazily).
    IsBulletActive = true;
    IsBulletDetonated = false;
    IsArmed = true;
    IsHoming = (pType != nullptr && pType->ROT > 0);
    IsAA = (pType != nullptr) ? pType->AA : false;
    IsAG = (pType != nullptr) ? pType->AG : false;
    IsAS = (pType != nullptr) ? pType->ASW : false;
    IsInaccurate = (pType != nullptr) ? pType->Inaccurate : false;
    IsGravity = (pType != nullptr) ? pType->Dropping : false;
    IsFlat = (pType != nullptr) ? pType->Flat : false;
    Motion = (pType == nullptr) ? BulletMotion::Straight
           : pType->Dropping ? BulletMotion::Dropping
           : pType->Vertical ? BulletMotion::Vertical
           : pType->Arcing   ? BulletMotion::Arcing
                             : BulletMotion::Straight;
}

// Legacy reset overload (GlobalFiring / BulletTypeClass::Create_Bullet).
void BulletClass::Initialize(BulletTypeClass* pType) {
    Class = pType;
    IsBulletActive = false;
    IsBulletDetonated = false;
    IsInAir = false;
    IsIncoming = false;
    IsAnimating = false;
    Timer = 0;
    Timer2 = 0;
    DistanceTraveled = 0;
    Facing = FacingType::N;
    Bright = false;
    AnimFrame = 0;
    AnimDelay = 0;
    NextAnim = nullptr;
    MustSpawnNextAnim = false;
    Velocity[0] = 0.0; Velocity[1] = 100.0; Velocity[2] = 0.0;
    if (pType != nullptr) {
        IsAA = pType->AA;
        IsAG = pType->AG;
        IsAS = pType->ASW;
        IsInaccurate = pType->Inaccurate;
        IsGravity = pType->Dropping;
        IsFlat = pType->Flat;
        Motion = pType->Dropping ? BulletMotion::Dropping
               : pType->Vertical ? BulletMotion::Vertical
               : pType->Arcing   ? BulletMotion::Arcing
                                 : BulletMotion::Straight;
    }
}

// =============================================================================
// Init - convenience launch initialiser (GlobalFiring / TechnoClass)
//   A thin wrapper over Initialize + the direction/target seeding that the
//   original performs split across Create_Bullet / SetMovement.
// =============================================================================
void BulletClass::Init(BulletTypeClass* pType, CoordStruct source, CoordStruct target,
                       TechnoClass* pOwner, int32 damage, WarheadTypeClass* pWarhead,
                       WeaponTypeClass* pWeapon) {
    if (pType == nullptr) return;

    Initialize(pType, source, pOwner, damage, pWarhead,
               (pType->ProjectileSpeed > 0) ? pType->ProjectileSpeed
                                            : BULLET_DEFAULT_SPEED,
               false);

    Class = pType;
    SourceCoords = source;
    Location = source;
    StartCoords = source;
    LastCoords = source;
    WeaponType = pWeapon;
    TargetCoords = target;
    FuseCoords = target;
    CreationFrame = Game::CurrentFrame;
    Health = pType->Arm;
    Strength = damage;
    Damage = damage;
    IsIncoming = true;

    // Facing + initial (normalised) velocity toward the target.
    const int32 dx = target.X - source.X;
    const int32 dy = target.Y - source.Y;
    const int32 dz = target.Z - source.Z;
    Facing = (dx != 0 || dy != 0) ? Bullet_ComputeFacing(dx, dy) : FacingType::N;
    Velocity[0] = static_cast<double>(dx);
    Velocity[1] = static_cast<double>(dy);
    Velocity[2] = static_cast<double>(dz);
    Bullet_Normalize(Velocity);

    const int32 dist = Bullet_Distance2D(source, target);
    if (Range <= 0 && dist > 0) Range = dist;
}

// =============================================================================
// SetMovement - asm: BulletClass_SetMovement (247056..247535)
//   bool __thiscall SetMovement(this, CoordStruct* xyz, VecFloat3D* vel)
//   retn 8 -> 2 args (the project keeps a 3-arg convenience form).
//   Places the projectile (ObjectClass::Unlimbo), seeds the velocity vector
//   ([+0xE8]) and, for the flat-tracer / inaccurate types, scatters the launch
//   point and normalises the direction.  Returns false if Unlimbo fails.
// =============================================================================
bool BulletClass::SetMovement(CoordStruct xyz, CoordStruct velocity, int32 speed) {
    // asm: ObjectClass::Unlimbo(this, xyz, 0)
    if (!ObjectClass::Unlimbo()) {
        return false;                                     // asm: loc_468B7D
    }

    // asm: rep movsd 6 dwords -> [+0xE8..+0x100] = *vel (three doubles)
    Velocity[0] = static_cast<double>(velocity.X);
    Velocity[1] = static_cast<double>(velocity.Y);
    Velocity[2] = static_cast<double>(velocity.Z);

    // asm: [+0x134] = xyz (source snapshot) + packed cell coords.
    SourceCoords = xyz;
    Location = xyz;
    LastCoords = xyz;
    StartCoords = xyz;
    if (speed > 0) Speed = speed;

    // asm: MapClass::Coord_Cell(xyz) -> [+0x14C] (packed cell coords)
    const CellStruct cell = Math::CoordToCell(xyz);
    CellX = static_cast<uint16>(cell.X);
    CellY = static_cast<uint16>(cell.Y);

    // asm: DisplayClass::Remove(map, this) -- the projectile is re-placed.
    if (DisplayClass::Instance != nullptr) {
        DisplayClass::Instance->Remove(this);
    }

    // asm: Target->GetCoords() (vt+0x58) -> [+0x140] (fuse / aim point).
    if (Target != nullptr) {
        const CoordStruct tgt = Target->GetCoords();
        FuseCoords = tgt;
        TargetCoords = tgt;
    }

    // asm loc_4688A9: FlakScatter(+2A3) && Inviso(+29E) -> scatter the launch
    // point by a random radius and re-derive the direction from the target.
    if (Class != nullptr && Class->FlakScatter && Class->Inviso) {
        Randomizer* pRng = (ScenarioClass::Instance != nullptr)
                         ? &ScenarioClass::Instance->Random : nullptr;
        // asm: RulesData[+155Ch] * 2 is the scatter radius bound.
        const int32 maxSpread = ((RulesClass::Instance != nullptr)
                              ? RulesClass::Instance->Gravity : 4) * 2;
        const int32 radius = pRng ? pRng->Next(0, maxSpread)
                                  : (std::rand() % (maxSpread + 1));
        if (Target != nullptr) {
            const CoordStruct tgt = Target->GetCoords();
            const double spread = std::sqrt(
                static_cast<double>(tgt.X - xyz.X) * (tgt.X - xyz.X) +
                static_cast<double>(tgt.Y - xyz.Y) * (tgt.Y - xyz.Y) +
                static_cast<double>(tgt.Z - xyz.Z) * (tgt.Z - xyz.Z));
            const int32 angle = pRng ? pRng->Next(0, 0x7FFFFFFE)
                                     : (std::rand() % 0x7FFFFFFE);
            // asm: angle * (1/0x7FFFFFFF) * (2pi/...) - pi/2 -> radians.
            const double rad = (static_cast<double>(angle) / 2147483646.0)
                             * (2.0 * BULLET_PI) - (BULLET_PI / 2.0);
            const double off = static_cast<double>(radius) * spread
                             / ((Class->ProjectileSpeed > 0) ? Class->ProjectileSpeed : 1);
            xyz.X = Bullet_FloorToInt(static_cast<double>(xyz.X) + std::cos(rad) * off);
            xyz.Y = Bullet_FloorToInt(static_cast<double>(xyz.Y) + std::sin(rad) * off);
            Location = xyz;
        }
    }

    // asm loc_4689DB / loc_468AF9: normalise the direction vector to a unit
    // vector; an all-zero vector becomes the (0, 100, 0) sentinel.
    const double len = Bullet_Length(Velocity);
    if (len <= BULLET_DIRECTION_EPSILON) {
        Velocity[0] = 0.0; Velocity[1] = 1.0; Velocity[2] = 0.0;
    } else {
        Velocity[0] /= len; Velocity[1] /= len; Velocity[2] /= len;
    }

    // asm loc_468A1x: 单位方向向量再乘速度 —— Velocity 的**长度**就是每帧
    //   前进的 lepton 数（WeaponTypeClass::Speed）。此前只归一到单位长度，
    //   于是 Update 里 `|v| < 8 → 已停滞` 第一帧就成立，弹体在炮口原地起爆。
    const double shotMagnitude = (Speed > 0) ? static_cast<double>(Speed) : 1.0;
    Velocity[0] *= shotMagnitude;
    Velocity[1] *= shotMagnitude;
    Velocity[2] *= shotMagnitude;

    // asm loc_468A3F: arm = (Target is an Aircraft) ? 0 : type->Arm(+0x2F0),
    // fed into BulletSubClass_SetData(&this+0xB8, Location, Fuse, arm, INT_MAX).
    int32 arm = (Class != nullptr) ? Class->Arm : 0;
    if (Target != nullptr && Target->WhatAmI() == AbstractType::Aircraft) {
        arm = 0;
    }
    ArmCounter = 0;
    if (arm > 0) Timer = static_cast<uint32>(arm);       // fuse timer [+0xB8]

    // asm loc_468B5D: if alive, submit to the display layer for drawing.
    if (IsBulletActive && DisplayClass::Instance != nullptr) {
        DisplayClass::Instance->Submit(this);
    }
    return true;
}

// =============================================================================
// Advance_Rotation - asm: Update loc_4667B2..loc_466808 (244035..244064)
//   Runs only when the type declares an anim frame range (AnimLow != 0 ||
//   AnimHigh != 0).  `AnimDelay` is a byte countdown reloaded from AnimRate
//   whenever it hits zero; only then does AnimFrame advance, wrapping
//   AnimHigh -> AnimLow with an *unsigned* compare (asm `cmp al,dl / jbe`).
//   Note the asm decrements first and bails while the result is non-zero, so a
//   zero seed wraps to 0xFF and skips a frame.
// =============================================================================
void BulletClass::Advance_Rotation() {
    if (Class == nullptr) return;
    if (Class->AnimLow == 0 && Class->AnimHigh == 0) return;  // asm test/jz x2

    // asm: mov al,[12Dh]; dec al; mov [12Dh],al; jnz loc_466808
    AnimDelay = static_cast<uint8>(AnimDelay - 1);
    if (AnimDelay != 0) return;

    AnimFrame = static_cast<uint8>(AnimFrame + 1);             // inc bl
    AnimDelay = static_cast<uint8>(Class->AnimRate);           // mov [12Dh],dl
    if (AnimFrame > static_cast<uint8>(Class->AnimHigh)) {     // cmp al,dl / jbe
        AnimFrame = static_cast<uint8>(Class->AnimLow);        // mov [12Ch],al
    }
}

// =============================================================================
// Spawn_Trailer - asm: Update loc_466808..loc_4668BD (244065..244129)
//   When the type carries a Trailer anim (+0x2D8), spawn it every
//   `ScaledSpawnDelay` frames (asm [+0x2E8], falls back to SpawnDelay +0x2E4)
//   as gauged by the global Frame counter.  The AnimClass is created with
//   loopCount=1, flags=0x600 and no owner.
// =============================================================================
void BulletClass::Spawn_Trailer() {
    if (Class == nullptr || Class->Trailer == nullptr) return;  // asm test eax,eax

    // asm loc_466808: esi = [+0x2E8]; if (!esi) rate = [+0x2E4]
    int32 rate = Class->ScaledSpawnDelay;
    if (rate == 0) rate = Class->SpawnDelay;
    if (rate == 0) return;
    // asm: idiv rate; test edx,edx; jnz -> skip
    if ((Game::CurrentFrame % rate) != 0) return;

    // asm: new AnimClass(Trailer, &pos, a4=1, loopCount=1, flags=0x600, 0, 0)
    new AnimClass(Class->Trailer, Location, 1, 1, 0x600, 0, false);
}


// =============================================================================
// Compute_Homing_Step - asm: Update loc_4668BD..loc_4669B9, loc_4669F3..loc_466B0C
//
//   step = floor( (1 + G + sin(phase)*G) * ROT )   where
//       G     = RulesData[+0x598] (the ballistic gravity coefficient),
//       phase = (Frame + owner-height) % 15,
//       ROT   = type->ROT (+0x2DC).
//   The asm then folds the per-frame acceleration toggle (type->Acceleration
//   +0x2D0, forced to 1/0 on alternating frames for course-lock-less homing
//   missiles) into the same accumulator.  Returned value mirrors [esp+10h].
// =============================================================================
int32 BulletClass::Compute_Homing_Step() {
    if (Class == nullptr) return 1;

    // asm: lea eax,[ebp+4] / call [edx+10h] ; Frame + that ; idiv 15
    const int32 phase = (Game::CurrentFrame + static_cast<int32>(Location.X)) % 15;

    // asm: sin(phase * dbl_7E48F8 * dbl_7E3CC0)
    const double sinVal = std::sin(static_cast<double>(phase)
                                   * (2.0 * BULLET_PI / 15.0));

    // asm: fmul qword[esi] where esi = RulesData+598h (gravity coefficient).
    const double g = (RulesClass::Instance != nullptr)
                   ? static_cast<double>(RulesClass::Instance->Gravity) : 1.0;

    // asm: fadd dbl1_0 ; faddp ; fimul [edx+2DCh] (ROT) ; Float_To_Int_Floor
    double stepF = (sinVal * g + (g + 1.0)) * static_cast<double>(Class->ROT);
    int32 step = Bullet_FloorToInt(stepF);
    if (step <= 0) step = 1;
    return step;
}

// =============================================================================
// Simulate_Homing - asm: Update loc_4668BD..loc_4670C3 (244129..244780)
//
//   Reconstructed control flow:
//     1. step = Compute_Homing_Step(); fuse arming via type->CourseLockDuration
//        (+0x2E0) or (Speed >= 0x28) (asm loc_46695B / loc_466978).
//     2. per-frame acceleration toggle for course-lock-less seekers.
//     3. normalise the unit velocity; when it is the all-zero seed the asm
//        substitutes (0, 100, 0) (loc_466AB8).
//     4. steer toward the aim point and advance by step (or 1.5*step when the
//        remaining distance < 0x100) via Coord_Move; the tracking accumulator
//        (TrackFrames/TrackAccum, +0x118/+0x120) is stretched by the moved
//        distance and, when it exceeds 60.0 for a non-airburst / non-veryhigh
//        type, the projectile self-detonates.
//   Returns the hit flag: 0 = still flying, 1 = hit, 2 = left the visible map.
// =============================================================================
int32 BulletClass::Simulate_Homing(bool& preciseHit, bool& mustDetonate) {
    if (Class == nullptr) return 0;

    // ---- aim point = the target's live position (or the cached fuse) --------
    CoordStruct aim = FuseCoords;
    if (Target != nullptr) {
        Target->GetCoords(&aim);
        TargetCoords = aim;
        if (Class->CourseLocked || IsHoming) aim = TargetCoords;
    }

    const int32 step = Compute_Homing_Step();

    // ---- fuse arming (asm loc_46695B / loc_466978) -------------------------
    if (Class->CourseLockDuration != 0) {
        if (ArmCounter < Class->CourseLockDuration) {
            ++ArmCounter;
            if (ArmCounter >= Class->CourseLockDuration) IsHoming = false;
        }
    } else {
        if (Speed >= 0x28) {
            IsHoming = false;                                   // loc_466978
        } else {
            const int32 dist = Bullet_Distance3D(Location, aim);
            if (static_cast<double>(dist) + 0.5 < static_cast<double>(Speed)) {
                IsHoming = false;
            }
        }
    }

    // ---- all-zero velocity seed (asm loc_466A33..loc_466AB8) --------------
    if (Velocity[0] == 0.0 && Velocity[1] == 0.0 && Velocity[2] == 0.0) {
        Velocity[0] = 0.0; Velocity[1] = 100.0; Velocity[2] = 0.0;
    }
    // asm loc_466AC5: the unit vector is normalised (dir /= |dir|).
    {
        const double len = Bullet_Length(Velocity);
        if (len > BULLET_DIRECTION_EPSILON) {
            Velocity[0] /= len; Velocity[1] /= len; Velocity[2] /= len;
        }
    }

    // ---- steer + advance (asm loc_466B0C..loc_466F1C) ---------------------
    const int32 dxa = aim.X - Location.X;
    const int32 dya = aim.Y - Location.Y;
    const int32 dza = aim.Z - Location.Z;
    const int32 dist3D = static_cast<int32>(Bullet_Length3(dxa, dya, dza));

    int32 moveStep = step;
    if (dist3D < 0x100) {                                       // asm cmp eax,100h
        moveStep = Bullet_FloorToInt(static_cast<double>(step) * 1.5);
    }
    if (moveStep <= 0) moveStep = 1;
    if (dist3D > 0 && moveStep > dist3D) moveStep = dist3D;

    if (dist3D > 0) {
        Velocity[0] = static_cast<double>(dxa) / dist3D;
        Velocity[1] = static_cast<double>(dya) / dist3D;
        Velocity[2] = static_cast<double>(dza) / dist3D;
    }

    const CoordStruct prev = Location;
    CoordStruct next = Location;
    next.X += Bullet_VecToInt(Velocity[0] * static_cast<double>(moveStep));
    next.Y += Bullet_VecToInt(Velocity[1] * static_cast<double>(moveStep));
    next.Z += Bullet_VecToInt(Velocity[2] * static_cast<double>(moveStep));

    // ---- tracking-accumulator stretch (asm loc_466FD8..loc_467030) --------
    const int32 moved = Bullet_Distance3D(prev, next);
    if (TrackFrames < 0x3C) {                                   // cmp [ebp+118h],3Ch
        ++TrackFrames;
        TrackAccum += static_cast<double>(moveStep);
    } else {
        TrackAccum = TrackAccum * 0.5 + static_cast<double>(moved);
        if (TrackAccum >= 0.0 && TrackAccum >= 60.0               // dbl60
            && !Class->Airburst && !Class->VeryHigh) {
            preciseHit = true;
            mustDetonate = true;
            Location = next;
            LastCoords = Location;
            Commit_Position(Location);
            return 1;                                           // asm loc_467B7A
        }
    }

    int32 hitFlag = Resolve_Ballistic_Hit(prev, next, preciseHit);
    Location = next;
    LastCoords = Location;
    Commit_Position(Location);
    return hitFlag;
}


// =============================================================================
// Simulate_Ballistic - asm: Update loc_4670C3..loc_467A2B (244780..245519)
//
//   The four unguided flight models:
//     * Vertical  (type +0x2C0): grow the velocity magnitude toward Speed by
//       Acceleration, then advance (asm loc_4672B7).
//     * Straight / Arcing / Dropping (asm loc_467402): subtract gravity from the
//       Z component (Floater uses half gravity) and settle Arcing onto the aim
//       height, then advance.
//   A projectile whose velocity magnitude has decayed below 8.0 is treated as
//   stalled and detonates; the type->DetonationAltitude (+0x2BCh) fuse fires
//   when the new Z drops to or below it.
// =============================================================================
int32 BulletClass::Simulate_Ballistic(bool& preciseHit, bool& mustDetonate) {
    if (Class == nullptr) return 0;
    int32 hitFlag = 0;

    CoordStruct aim = FuseCoords;
    if (Target != nullptr) {
        Target->GetCoords(&aim);
        TargetCoords = aim;
        aim = TargetCoords;
    }

    // asm loc_4670C3: |velocity| < 8.0 -> the shell has stalled -> hit.
    if (Bullet_Length(Velocity) < 8.0) hitFlag = 1;

    // asm loc_46718F: gravity = Floater(+0x295) ? GetHalfGravity() : RulesGravity.
    const int32 gravBase = (RulesClass::Instance != nullptr)
                         ? RulesClass::Instance->Gravity : BULLET_GRAVITY_PER_FRAME;
    const double gravity = Class->Floater ? (static_cast<double>(gravBase) / 2.0)
                                          : static_cast<double>(gravBase);

    const CoordStruct prev = Location;
    CoordStruct next = Location;

    if (Class->Vertical) {                                  // asm loc_4672B7
        const int32 vlen = static_cast<int32>(Bullet_Length(Velocity));
        if (vlen < Speed) {
            // asm loc_4672B7..loc_467273: grow by Acceleration and renormalise.
            double dir[3] = { Velocity[0], Velocity[1], Velocity[2] };
            Bullet_Normalize(dir);                          // all-zero -> (0,100,0)
            const double mag = static_cast<double>(vlen + Class->Acceleration);
            Velocity[0] = dir[0] * mag;
            Velocity[1] = dir[1] * mag;
            Velocity[2] = dir[2] * mag;
        }
        next.X += Bullet_VecToInt(Velocity[0]);
        next.Y += Bullet_VecToInt(Velocity[1]);
        next.Z += Bullet_VecToInt(Velocity[2]);
    } else {                                                // asm loc_467402
        // asm: velZ -= gravity (Dropping / HalfGravity).
        if (Class->Dropping || Class->HalfGravity) {
            Velocity[2] -= Class->HalfGravity ? (gravity / 2.0) : gravity;
        }
        // asm loc_467519: Arcing settles onto the aim height.
        if (Class->Arcing && Location.Z > aim.Z) {
            Velocity[2] -= static_cast<double>(BULLET_GRAVITY_PER_FRAME);
        }
        next.X += Bullet_VecToInt(Velocity[0]);
        next.Y += Bullet_VecToInt(Velocity[1]);
        next.Z += Bullet_VecToInt(Velocity[2]);
    }

    const int32 r = Resolve_Ballistic_Hit(prev, next, preciseHit);
    if (r != 0) hitFlag = r;

    // asm loc_467350: type->DetonationAltitude (+0x2BCh) top-of-arc fuse.
    if (hitFlag == 0 && Class->DetonationAltitude != 0
        && next.Z <= Class->DetonationAltitude) {
        hitFlag = 1;
        preciseHit = true;
    }

    Location = next;
    LastCoords = Location;
    Commit_Position(Location);
    return hitFlag;
}

// =============================================================================
// Resolve_Ballistic_Hit - asm: Update loc_46703..loc_467A2B (245520..245519)
//
//   Tests a candidate move (prev -> next) against, in order:
//     1. a wall/bridge flag (cell flag word & 0x100) on the destination or the
//        origin cell, combined with a Z crossing of the floor height;
//     2. the floor height (GetCellFloorHeight + dword_89DE64 bias) crossed in
//        either direction;
//     3. a building on the destination cell (skipped for allies / own house);
//     4. a techno occupant within 0x80 leptons (honours CanHit).
//   Returns 0 (miss), 1 (hit) or 2 (left the visible map, quiet destroy).
//   `exactHit` is raised for a precise (floor / occupant / wall) collision.
// =============================================================================
int32 BulletClass::Resolve_Ballistic_Hit(const CoordStruct& prev,
                                         CoordStruct& next,
                                         bool& exactHit) {
    if (MapClass::Instance == nullptr || Class == nullptr) return 0;

    // asm loc_46706A / loc_467371: the floor test adds the cell-height bias
    // dword_89DE64 and fires on a Z crossing in either direction.
    // 目标还在前方（距离大于本帧步长）时，弹体要从炮口高度降到目标高度，
    //   途中必然穿过"地面高度 + bias"这条线。此时若按地面命中处理，炮弹会
    //   在离目标还有大半格的地方开花、伤害落在空地上。原版弹道贴近水平、
    //   bias 也小，踩不到这条；本树炮口高度与步长都更粗，需显式跳过——
    //   真打到地面时下面的桥/建筑/占用者分支仍会命中。
    bool targetAhead = false;
    if (Target != nullptr)
    {
        int32 step = static_cast<int32>(Bullet_Length(Velocity));
        if (step < 0x20) step = 0x20;
        const CoordStruct tcAhead = Target->GetCoords();
        targetAhead = (Bullet_Distance3D(next, tcAhead) > step);
    }

    const int32 bias = (MapClass::Instance->HalfCellHeight != 0)
                     ? (MapClass::Instance->HalfCellHeight / 16) : 2;
    const int32 floorZ = MapClass::Instance->GetCellFloorHeight(next) + bias;
    if (!targetAhead && ((prev.Z >= floorZ && next.Z < floorZ) ||
                         (prev.Z <= floorZ && next.Z > floorZ))) {   // loc_4674F6/loc_467510
        exactHit = true;
        return 1;
    }

    // asm loc_467032 / loc_467890: the cell wall/bridge flag word (bit 0x100).
    if (Cell_Is_Bridged(next) || Cell_Is_Bridged(prev)) {
        if (next.Z < floorZ && prev.Z >= floorZ) { exactHit = true; return 1; }
        if (prev.Z < floorZ) { exactHit = true; return 1; }
    }

    // ── 主线目标：线段到目标点的最近距离 ────────────────────────────────
    //   占用判定是"按格"做的，而对象坐标可能正好落在四格交界的角上
    //   （Cell2Coord 返回格角而非格心），斜向弹道会从角点掠过、两个格子
    //   都不含目标，于是"瞄准了却打不到"。这里先对**当前锁定的目标**做一次
    //   点—线段距离判定：弹道真的从目标身上穿过就算命中。
    if (Target != nullptr && CanHit(Target))
    {
        const CoordStruct tc = Target->GetCoords();

        const double ax = static_cast<double>(tc.X - prev.X);
        const double ay = static_cast<double>(tc.Y - prev.Y);
        const double az = static_cast<double>(tc.Z - prev.Z);
        const double bx = static_cast<double>(next.X - prev.X);
        const double by = static_cast<double>(next.Y - prev.Y);
        const double bz = static_cast<double>(next.Z - prev.Z);

        const double bb = bx * bx + by * by + bz * bz;
        double t = (bb > 0.0)
                 ? ((ax * bx + ay * by + az * bz) / bb) : 0.0;
        if (t < 0.0) t = 0.0;
        if (t > 1.0) t = 1.0;

        const double cx = ax - bx * t;
        const double cy = ay - by * t;
        const double cz = az - bz * t;
        const double d2 = cx * cx + cy * cy + cz * cz;

        if (d2 < 128.0 * 128.0)
        {
            // 命中点落在弹体与目标之间：把这一帧的落点推到目标身上。
            //   否则弹体停在半空，CellSpread=0 的弹头（坦克炮、步枪）
            //   Damage_At_Cell 里 dist>0 会被直接丢掉，表现为"打中了却不掉血"。
            next = tc;
            exactHit = true;
            return 1;
        }
    }

    // ── 逐段采样做占用判定 ──────────────────────────────────────────────
    //   一帧位移可达 100+ lepton，而占用判定只看"落点所在格"。斜向飞行时
    //   落点会从两个格子的夹缝里跳过去（例如从 (28,37) 直接跳到 (27,38)），
    //   炮弹就擦着站在 (28,38) 的目标飞走，最后落在空地上爆炸、伤害打空。
    //   这里把本帧位移按 ≤64 lepton 切成若干采样点，逐点走一遍占用链。
    {
        const double segLen = Bullet_Distance3D(prev, next);
        const int32 samples = (segLen > 64.0)
                            ? static_cast<int32>(segLen / 64.0) : 1;

        for (int32 si = 1; si <= samples; ++si)
        {
            CoordStruct sp;
            sp.X = prev.X + (next.X - prev.X) * si / samples;
            sp.Y = prev.Y + (next.Y - prev.Y) * si / samples;
            sp.Z = prev.Z + (next.Z - prev.Z) * si / samples;

            const CellStruct sc = Math::CoordToCell(sp);
            CellClass* pc = MapClass::Instance->GetCellAt(sc);
            if (pc == nullptr)
                continue;

            for (ObjectClass* pOcc = pc->FirstObject; pOcc != nullptr;
                 pOcc = pOcc->NextObject)
            {
                const AbstractType occWhat = pOcc->WhatAmI();
                if (occWhat != AbstractType::Unit && occWhat != AbstractType::Infantry
                 && occWhat != AbstractType::Aircraft)
                    continue;

                TechnoClass* pTech = static_cast<TechnoClass*>(pOcc);
                if (pTech == Owner)
                    continue;
                if (Owner != nullptr && Owner->Is_Ally(pTech))
                    continue;

                const CoordStruct tc = pTech->GetCoords();
                if (Bullet_Distance3D(sp, tc) < 0x80 && CanHit(pTech))
                {
                    // 挡在弹道上的单位就是真正的命中对象：落点贴到它身上，
                    //   而不是停在采样点。CellSpread=0 的弹头只在 dist==0 时
                    //   结算（见 MapClass::Damage_At_Cell），停在中途等于打空。
                    next = tc;
                    exactHit = true;
                    return 1;
                }
            }
        }
    }

    // asm loc_4678F8: building occupant on the destination cell.
    const CellStruct cell = Math::CoordToCell(next);
    CellClass* pCell = MapClass::Instance->GetCellAt(cell);
    if (pCell != nullptr) {
        BuildingClass* pBuilding = pCell->Cell_Building();
        if (pBuilding != nullptr && !Class->Vertical) {
            // asm loc_4678F8: an ally's building does not stop the shell.
            if (Owner != nullptr && Owner->Is_Ally(pBuilding)) {
                // fly through
            } else {
                exactHit = true;
                return 1;
            }
        }

        // asm loc_467943: 目标格里"离弹体最近"的技术对象命中即视为命中。
        //
        //   原版调 CellClass::Cell_Techno 取最近者，随后统一做
        //   `0x80 内 + 敌对 + CanHit` 判定。这里直接遍历该格的占用链做同
        //   一套判定：同格可能叠着多个对象（步兵挤在一格是常态），只看
        //   "最近的那一个"会因为 Cell_Techno 的近似排序而漏判真正的目标，
        //   表现为炮弹从目标身上穿过去。
        for (ObjectClass* pOcc = pCell->FirstObject; pOcc != nullptr;
             pOcc = pOcc->NextObject)
        {
            const AbstractType occWhat = pOcc->WhatAmI();
            if (occWhat != AbstractType::Unit && occWhat != AbstractType::Infantry
             && occWhat != AbstractType::Aircraft)
                continue;

            TechnoClass* pTech = static_cast<TechnoClass*>(pOcc);
            if (pTech == Owner)
                continue;                                   // asm: 不打自己
            if (Owner != nullptr && Owner->Is_Ally(pTech))
                continue;                                   // asm: 不打友军

            // asm loc_467965: only a hostile techno within 0x80 of the
            // projectile counts as an impact.
            const CoordStruct tc = pTech->GetCoords();
            if (Bullet_Distance3D(next, tc) < 0x80 && CanHit(pTech)) {
                next = tc;                       // 落点贴到被命中的对象身上
                exactHit = true;
                return 1;
            }
        }
    }

    // asm loc_467A2B: outside the visible area -> flag 2 (destroy, no boom).
    if (!MapClass::Instance->CellInVisibleArea(next)) {
        return 2;
    }
    return 0;
}

// asm: the cell flag word (cell+0x140) & 0x100 test used throughout Update
// (loc_467032 / loc_4673xx / sub_468BB0).  Bit 0x100 is the "bridged" flag, so
// a projectile crossing a bridge cell takes the floor-collision path.
bool BulletClass::Cell_Is_Bridged(const CoordStruct& loc) const {
    if (MapClass::Instance == nullptr) return false;
    const CellStruct cell = Math::CoordToCell(loc);
    CellClass* pCell = MapClass::Instance->GetCellAt(cell);
    if (pCell == nullptr) return false;
    return pCell->IsBridge() || pCell->IsWall();
}


// =============================================================================
// Commit_Position - asm: Update loc_467FBA..loc_467FEE
//   Stores the packed cell coords [0x14C] from the world position.
// =============================================================================
void BulletClass::Commit_Position(const CoordStruct& next) {
    const CellStruct cell = Math::CoordToCell(next);
    CellX = static_cast<uint16>(cell.X);
    CellY = static_cast<uint16>(cell.Y);
}

// =============================================================================
// Update - per-frame projectile AI
//   asm: BulletClass_Update (243943..246416)
//
//   Top-level control flow (offsets against the listing):
//     [ebp+90h]   IsBulletActive gate
//     [ebp+158h]  MustSpawnNextAnim ([ebp+154h] NextAnim must be cleared)
//     [esp+18h]   mustDetonate      -> Detonate(arg)
//     [esp+1Fh]   preciseHit        -> allow the impact snap
//     [esp+20h]   hitFlag           0 flying / 1 hit / 2 off-map
//   [ebp+8Dh] (IsIncoming) gates the Dropping self-detonate.
// =============================================================================
void BulletClass::Update() {
    static int s_DbgCount = 0;
    static const bool dbg = (std::getenv("OPENYR_FIRE_DBG") != nullptr);
    if (dbg && s_DbgCount < 60) {
        ++s_DbgCount;
        std::fprintf(stderr, "[BUPDATE] #%d loc=(%d,%d,%d) v=(%.1f,%.1f,%.1f) active=%d limbo=%d\n",
                     s_DbgCount, (int)Location.X, (int)Location.Y, (int)Location.Z,
                     Velocity[0], Velocity[1], Velocity[2],
                     (int)IsBulletActive, (int)IsInLimbo);
    }
    ObjectClass::Update();                                   // asm: call ObjectClass_Update
    if (!IsBulletActive) return;                             // asm: [ebp+90h]

    // asm loc_46674E..loc_466766: the pending trailer anim finished -> the
    // projectile detonates and destroys itself.
    if (MustSpawnNextAnim) {                                 // asm: [ebp+arg_150]
        if (NextAnim != nullptr) return;                     // asm: jnz loc_467FEE
        MustSpawnNextAnim = false;
        Detonate(false);
        Destroy();
        return;
    }

    if (Class == nullptr) {                                  // defensive
        Detonate(false);
        Destroy();
        return;
    }

    // asm loc_466789: [esp+18h] = Dropping && !IsIncoming; [esp+1Fh] = 0.
    bool preciseHit = false;
    bool mustDetonate = (Class->Dropping && !IsIncoming);    // asm [ebp+8Dh]

    Advance_Rotation();                                      // asm loc_4667B2
    Spawn_Trailer();                                         // asm loc_466808

    // asm loc_4668BD (ROT>0) / loc_4670C3 (ROT<=0).
    const int32 hitFlag = (Class->ROT > 0)
                        ? Simulate_Homing(preciseHit, mustDetonate)
                        : Simulate_Ballistic(preciseHit, mustDetonate);

    // asm: Update 尾段 —— 撞地 / 撞崖 / 撞墙强制起爆（sub_468BB0）。
    //   此前这条判定只有实现、没有任何调用方：弹丸会笔直穿过悬崖与围墙，
    //   落到后面才炸。命中目标那条路径（hitFlag==1）不重复处理。
    if (!mustDetonate && hitFlag == 0)
    {
        CoordStruct impact{};
        if (Check_Terrain_Impact(impact))
        {
            Location = impact;
            mustDetonate = true;
        }
    }

    Resolve_Step_End(hitFlag, preciseHit, mustDetonate);     // asm loc_467B7A
    Commit_Position(Location);                               // asm loc_467FBA
}

// =============================================================================
// Resolve_Step_End - asm: Update loc_467B7A..loc_467F9B (245900..246100)
//
//   hitFlag 2 -> quiet Destroy() (asm loc_467FA9).  Otherwise the position is
//   placed (vt+0x1B4) and, while the projectile is not yet due to detonate, the
//   Degenerates damage decay ([ebp+6Ch] > 5 -> dec) runs.  When detonation is
//   due the impact is snapped onto a nearby target (asm loc_467CA9, gated on
//   !Airburst && !Inaccurate), then the nuke payload (loc_467E53) or the plain
//   Detonate + Destroy (loc_467F9B).
// =============================================================================
void BulletClass::Resolve_Step_End(int32 hitFlag, bool& preciseHit,
                                   bool& mustDetonate) {
    // asm loc_467B7A: sub eax,2; jz loc_467FA9
    if (hitFlag == 2) {
        Destroy();
        return;
    }
    if (hitFlag == 1) mustDetonate = true;                   // asm [esp+18h]=1

    if (!mustDetonate) {
        // asm loc_467C6A: while not detonating, a Degenerates projectile bleeds
        // damage until it can no longer hurt anything.
        if ((Class->Dropping || !preciseHit) && Class->Degenerates && Damage > 5) {
            --Damage;                                        // asm: dec [ebp+6Ch]
        }
        return;                                              // asm loc_467FBA
    }

    // asm loc_467CA9: snap the impact onto the target when the projectile is
    // armed, close, and the type is neither Airburst nor Inaccurate.
    if (Target != nullptr && (hitFlag == 1 || preciseHit)
        && !Class->Airburst && !Class->Inaccurate) {
        const CoordStruct tc = Target->GetCoords();
        int32 snap = 0x80;                                   // asm cmp eax,0Ch/80h
        const int32 step = static_cast<int32>(Bullet_Length(Velocity));
        if (step > snap) snap = step;                        // 见 Resolve_Impact_Point
        if (Bullet_Distance3D(Location, tc) < snap) {
            Location = tc;
        }
    }

    // asm loc_467E53: a nuclear warhead spawns the "NUKEBALL" anim and defers
    // the detonation until that anim finishes (MustSpawnNextAnim).
    if (Spawn_NukeBall()) return;

    // asm loc_467F9B: Detonate([esp+18h]) then Destroy().
    Detonate(mustDetonate);
    Destroy();
}

// =============================================================================
// Spawn_NukeBall - asm: Update loc_467E53..loc_467F36 (245936..246000)
//   When the warhead is the "NUKE" warhead, clamp the height, register a radar
//   event (type 13), then create the "NUKEBALL" AnimClass at the impact point
//   with flags 0x2600 and arm MustSpawnNextAnim so the projectile detonates
//   once the anim has run its course.
// =============================================================================
bool BulletClass::Spawn_NukeBall() {
    if (Warhead == nullptr || _strcmpi(Warhead->ID, "NUKE") != 0) return false;

    // asm loc_467E87: clamp a negative height to 0.
    if (Location.Z < 0) Location.Z = 0;

    // asm: RadarEventClass_Add(type = 13, cell).
    const CellStruct cell = Math::CoordToCell(Location);
    RadarEventClass::Add(13, static_cast<int16>(cell.X), static_cast<int16>(cell.Y));

    const int32 idx = AnimTypeClass::FindIndex("NUKEBALL");
    if (idx < 0) return false;
    AnimTypeClass* pBall = AnimTypeClass::FindByIndex(idx);
    if (pBall == nullptr) return false;

    // asm: new AnimClass(NUKEBALL, pos, a4=0, loopCount=1, flags=0x2600, zadjust, 0)
    NextAnim = new AnimClass(pBall, Location, 0, 1, 0x2600, 0, false);
    MustSpawnNextAnim = true;
    return true;
}

// =============================================================================
// Check_Terrain_Impact - asm: sub_468BB0 (247553..247785)
//   Called from the Update tail to see whether the projectile has grounded /
//   struck a cliff or wall.  `pos` is snapped onto the struck object's position
//   when the test fires.  Returns true to force the detonation.
// =============================================================================
bool BulletClass::Check_Terrain_Impact(CoordStruct& pos) {
    pos = Location;                                          // asm: copy this+9Ch
    if (MapClass::Instance == nullptr || Class == nullptr) return false;

    // asm loc_468C04: cliff / wall aware CanTarget test.
    if (Class->SubjectToCliffs || Class->SubjectToWalls) {
        HouseClass* pHouse = (Owner != nullptr) ? Owner->GetOwningHouse() : nullptr;
        CellClass* pSrc = MapClass::Instance->GetTargetCell(SourceCoords);
        CellClass* pTgt = MapClass::Instance->GetTargetCell(FuseCoords);
        CellClass* pHit = CellClass::CanTarget(pSrc, pos.X, pos.Y, pos.Z,
                                               Class, pHouse);
        if (pHit != nullptr) {
            pos = pHit->Get_CellCoords();                    // asm: *a2 = cell coords
            pos.Z = Location.Z;
            return true;
        }
    }

    // asm loc_468C86: height <= -(dword_89DE70 * 4) -> grounded.
    if (Location.Z <= -(4 * 26)) return true;

    // asm loc_468CAB: FlakScatter types detonate when the target is above them.
    if (Target != nullptr && Class->FlakScatter) {
        const CoordStruct tc = Target->GetCoords();
        if (Location.Z < tc.Z && Location.Z < 0) return true;  // asm GetHeight() < 0
    }

    // asm loc_468D08: Level types ground out on an occupied / bridged cell.
    if (Class->Level) {
        CellClass* pCell = MapClass::Instance->GetCellAt(Math::CoordToCell(pos));
        if (pCell != nullptr && (pCell->IsBridge() || pCell->Cell_Building() != nullptr)) {
            return true;
        }
    }

    // asm loc_468D2F: AA types detonate against a nearby dead target.
    if (Class->AA && Target != nullptr) {
        if (DistanceFrom2(Target, 0, 0) < 0x80) return true;
    }
    return false;
}


// =============================================================================
// CanHit - target filter (asm: the inlined HouseClass::Is_Ally / WhatAmI tests)
// =============================================================================
bool BulletClass::CanHit(TechnoClass* pTarget) const {
    if (pTarget == nullptr) return false;

    // 已经打死的目标不能再吃弹。尸体在格子占用链里还要留几帧（死亡动画、
    //   残骸），不排除它的话，弹体每帧都撞在同一条尸体上：弹道被永久遮断，
    //   后面的活目标一个也打不到 —— 实测 3 个并排步兵，打死最前面那个之后
    //   剩余两个满血站到 1500 帧。
    if (pTarget->Health <= 0 || pTarget->IsDead()) return false;

    if (Owner == nullptr) return false;
    if (pTarget->GetOwningHouse() == Owner->GetOwningHouse()) return false;
    if (Owner->Is_Ally(pTarget)) return false;

    const AbstractType absType = pTarget->WhatAmI();
    if (absType == AbstractType::Aircraft && !IsAA) return false;
    if (absType == AbstractType::Unit && !IsAG && !pTarget->IsInAir()) return false;
    (void)IsAS;
    return true;
}

// =============================================================================
// Resolve_Impact_Point - asm: Detonate loc_468DC1..loc_468FE0
//
//   Chooses the true impact point:
//     * Inaccurate types (+0x2A2) never snap (loc_468DC1).
//     * EMEffect warheads and Airburst types (+0x294) keep the raw point
//       (loc_468E9F).
//     * Non-arcing, non-rotating types honour the cached fuse (+0x140).
//     * Within 0x20 leptons of the target's live position -> snap onto it
//       (loc_468DC1); further out, a target within 0x80 (air) / 0x2A (ground)
//       is snapped (loc_468F23 / loc_468F60), with a building mapped to its own
//       cell centre.
// =============================================================================
CoordStruct BulletClass::Resolve_Impact_Point(bool a2) const {
    CoordStruct loc = Location;
    if (Class == nullptr) return loc;

    // asm loc_468DC1: Inaccurate types detonate in place.
    if (Class->Inaccurate) return loc;

    // asm loc_468E9F: EMEffect warhead / Airburst type keep the raw point.
    if (Warhead != nullptr && Warhead->EMEffect) return loc;
    if (Class->Airburst) return loc;

    // asm loc_468F23: non-arcing, non-rotating types honour the cached fuse.
    if (!a2 && !Class->Arcing && Class->ROT <= 0) {
        if (FuseCoords.X != 0 || FuseCoords.Y != 0 || FuseCoords.Z != 0) {
            loc = FuseCoords;
        }
    }

    // asm loc_468F23 / loc_468F60: target-aware snap.
    if (Target != nullptr) {
        const CoordStruct tc = Target->GetCoords();
        if (Bullet_Distance3D(loc, tc) < 0x20) {                 // loc_468DC1
            loc = tc;
        } else {
            const int32 d = DistanceFrom2(Target, 0, 0);
            int32 limit = (Target->WhatAmI() == AbstractType::Aircraft)
                        ? 0x80 : 0x2A;                           // loc_468F23/60
            // 本帧的位移量也应算进"够近"：弹体一帧能走 |v| lepton，
            //   目标只要落在这步的射程内就算打中。原版速度较低时 0x2A 足够，
            //   本树按武器 Speed 直接推进（~100/帧），不放宽会一跳越过目标，
            //   弹体落在目标旁的格子里爆开、伤害打空。
            const int32 step = static_cast<int32>(Bullet_Length(Velocity));
            if (step > limit) limit = step;
            if (d < limit) loc = tc;
        }
    }
    return loc;
}

// =============================================================================
// Detonate - resolve impact point + cluster sub-munitions
//   asm: BulletClass_Detonate (247786..248128), signature (this, char a2)
//   retn 4 -> one bool arg.  loc_468FF4 runs the cluster ladder: the Airburst
//   type explodes once; otherwise the warhead is applied `type->Cluster`
//   (+0x2AC) times at scattered offsets.
// =============================================================================
void BulletClass::Detonate() {
    Detonate(false);
}

void BulletClass::Detonate(bool a2) {
    if (IsBulletDetonated) return;
    IsBulletDetonated = true;
    IsBulletActive = false;

    // asm loc_468E9F: an EMEffect warhead short-circuits the whole explosion.
    if (Warhead != nullptr && Warhead->EMEffect) return;

    // asm loc_468DC1..loc_468FE0: resolve the impact location.
    const CoordStruct loc = Resolve_Impact_Point(a2);
    FuseCoords = loc;
    Location = loc;

    // asm loc_46909A: Airburst explodes exactly once.
    if (Class != nullptr && Class->Airburst) {
        Explode(loc);
        return;
    }

    // asm loc_468FF4: `for (i = 0; i < type->Cluster; ++i) Fire(loc); scatter`.
    const int32 count = (Class != nullptr && Class->Cluster > 0) ? Class->Cluster : 1;
    static const bool s_bFireDbg = (std::getenv("OPENYR_FIRE_DBG") != nullptr);
    if (s_bFireDbg)
    {
        CoordStruct tp(0, 0, 0);
        if (Target != nullptr) tp = Target->GetCoords();
        std::fprintf(stderr, "[DETONATE] type=%s loc=(%d,%d,%d) tgt=(%d,%d,%d) dmg=%d wh=%p\n",
                     (Class != nullptr) ? Class->ID : "?",
                     (int)loc.X, (int)loc.Y, (int)loc.Z,
                     (int)tp.X, (int)tp.Y, (int)tp.Z, (int)Damage,
                     static_cast<void*>(Warhead));
    }
    Explode(loc);                                       // asm loc_46902C iteration 0
    if (count > 1) {
        Spawn_Cluster_munitions(loc);                   // iterations 1..count-1
    }
}

// =============================================================================
// Spawn_Cluster_munitions - asm: Detonate loc_46902C..loc_46909A
//   Repeatedly explodes the warhead at random offsets, `Cluster` times total
//   (the first was already fired), scattering each follow-up by the scenario
//   randomizer over a (256..512) lepton radius (Scenario->Random(256,512)).
// =============================================================================
void BulletClass::Spawn_Cluster_munitions(const CoordStruct& loc) {
    if (Class == nullptr) return;
    const int32 count = Class->Cluster;
    if (count <= 1) return;

    Randomizer* pRng = (ScenarioClass::Instance != nullptr)
                     ? &ScenarioClass::Instance->Random : nullptr;

    CoordStruct offset = loc;
    for (int32 i = 1; i < count; ++i) {
        if (!IsBulletActive) break;                     // asm: test [esi+90h]
        const int32 spread = pRng ? pRng->Next(0x100, 0x200)
                                  : (0x100 + (std::rand() % 0x100));
        const int32 ox = (pRng ? pRng->Next(-spread, spread)
                               : (std::rand() % (spread * 2 + 1)) - spread);
        const int32 oy = (pRng ? pRng->Next(-spread, spread)
                               : (std::rand() % (spread * 2 + 1)) - spread);
        offset.X += ox;
        offset.Y += oy;
        Explode(offset);
    }
}

// =============================================================================
// Apply_Radiation - asm: BulletClass_Fire loc_469130..lct_RadiationEnd
//   When the weapon has a RadLevel the impact paints a RadSiteClass over the
//   struck cell, creating it (or bumping the existing site's level).
// =============================================================================
void BulletClass::Apply_Radiation(const CoordStruct& loc) {
    if (WeaponType == nullptr || WeaponType->RadLevel <= 0) return;

    const CellStruct cell = Math::CoordToCell(loc);
    CellClass* pCell = (MapClass::Instance != nullptr)
                     ? MapClass::Instance->GetCellAt(cell) : nullptr;
    if (pCell == nullptr) return;

    const int32 spread = (WeaponType->Warhead != nullptr)
                       ? Bullet_FloorToInt(WeaponType->Warhead->CellSpread) : 0;

    RadSiteClass* pExisting = static_cast<RadSiteClass*>(pCell->Get_Rad_Site());
    if (pExisting != nullptr) {
        pExisting->Add(WeaponType->RadLevel);       // asm loc_469203
        return;
    }

    RadSiteClass* pRad = new RadSiteClass();        // asm: new RadSiteClass + CTOR
    if (pRad == nullptr) return;
    pRad->SetBaseCell(cell);
    pRad->SetCellSpread(spread);
    pRad->SetRadLevel(WeaponType->RadLevel);
    pRad->Radiate();
    pCell->Set_Rad_Site(pRad);
}

// =============================================================================
// Apply_Warhead_Special - asm: BulletClass_Fire lct_NoMindControl..loc_469AA4
//
//   The warhead "special effect ladder".  The original walks a fixed chain of
//   exclusive flags, each gated on its own prerequisites (owner / target /
//   house); when a prerequisite is missing it jumps straight to the end, which
//   is the empty branch reproduced here.
// =============================================================================
void BulletClass::Apply_Warhead_Special(const CoordStruct& loc) {
    if (Warhead == nullptr) return;
    HouseClass* pOwnerHouse = (Owner != nullptr) ? Owner->GetOwningHouse() : nullptr;

    // ---- MindControl: hand the target to the owner's capture manager -------
    // asm lct_..loc_4692BD..loc_46930D:
    //   1. events 6 and 44 are raised on the victim's tag;
    //   2. CaptureManager::CaptureUnit;
    //   3. on success and when the RulesData YuriMindControlSound is set, the
    //      sound plays (gated on the owner being the human player).
    if (Warhead->MindControl) {
        if (Owner == nullptr || Owner->CaptureManager == nullptr) return;
        TechnoClass* pVictim = Target;
        if (pVictim != nullptr && pVictim->Tag != nullptr) {   // asm +0x34
            const CellStruct cell = Math::CoordToCell(pVictim->GetCoords());
            pVictim->Tag->RaiseEvent(6, pVictim, cell, false, Owner);   // asm eventKind 6
            pVictim->Tag->RaiseEvent(44, pVictim, cell, false, Owner);  // asm eventKind 44
        }
        const bool captured = Owner->CaptureManager->CaptureUnit(pVictim);
        if (!captured) return;                                 // asm jz loc_469AA4
        if (RulesClass::Instance != nullptr
            && RulesClass::Instance->YuriMindControlSound != -1) {
            HouseClass* pHouse = Owner->GetOwningHouse();
            if (pHouse != nullptr && pHouse->IsHumanPlayer) {  // asm HouseClass_Is_Player
                CoordStruct pos = (pVictim != nullptr) ? pVictim->GetCoords()
                                                       : loc;
                VocClass::Sound_Effect(RulesClass::Instance->YuriMindControlSound,
                                       &pos, nullptr);
            }
        }
        return;
    }

    // ---- IvanBomb: register a timed C4 bomb on the (foot) victim -----------
    // asm lct_NoMindControl..lct_NoIvanBomb: BombListClass_Add(vec_BombList,
    //   SourceUnit = Owner, TargetUnit = target masked to foot-class).
    if (Warhead->IvanBomb) {
        // asm: the target slot is masked with the object flag bit 0 -- only a
        // foot-class unit takes a bomb.
        TechnoClass* pVictim = Target;
        if (pVictim != nullptr
            && pVictim->WhatAmI() != AbstractType::Building) {
            BombClass* pBomb = new BombClass();
            if (pBomb != nullptr) {
                pBomb->SourceUnit = Owner;
                pBomb->OwnerHouse = pOwnerHouse;
                pBomb->TargetUnit = pVictim;
                pBomb->PlantingFrame = Game::CurrentFrame;
                pBomb->WhenToBlow = Game::CurrentFrame
                    + ((RulesClass::Instance != nullptr)
                       ? RulesClass::Instance->IvanTimedDelay : 0);
            }
        }
        return;
    }

    // ---- ElectricAssault: a Tesla trooper over-charges a building ----------
    if (Warhead->ElectricAssault) {
        if (Owner == nullptr || Target == nullptr) return;
        if (Target->WhatAmI() != AbstractType::Building) return;
        if (Owner->WhatAmI() != AbstractType::Infantry) return;
        static_cast<BuildingClass*>(Target)
            ->AddOverpowerer(static_cast<InfantryClass*>(Owner));   // asm loc_4693xx
        return;
    }

    // ---- Parasite: attach a parasite to the victim -------------------------
    if (Warhead->Parasite) {
        if (Owner == nullptr || Target == nullptr) return;
        Target->SetParasite();                             // asm: ParasiteClass_Infect
        return;
    }

    // ---- Temporal: start a temporal warp on the victim --------------------
    if (Warhead->Temporal) {
        if (Owner == nullptr || Target == nullptr) return;
        if (Owner->TemporalImUsing != nullptr) {
            Owner->TemporalImUsing->Fire(Target);          // asm loc_4694B4
        }
        return;
    }

    // ---- IsLocomotor: imbue a new locomotor onto the victim ---------------
    if (Warhead->IsLocomotor) {
        if (Owner == nullptr || Target == nullptr) return;
        if (Owner->WhatAmI() == AbstractType::Infantry) {
            static_cast<FootClass*>(Owner)->LetGoOfUnit();   // asm loc_4695EB
        }
        // asm loc_469648: only foot-class victims (WhatAmI 1 or 2) are imbued.
        const AbstractType t = Target->WhatAmI();
        if (t == AbstractType::Infantry || t == AbstractType::Unit) {
            static_cast<FootClass*>(Owner)
                ->ImbueLocomotor(Warhead->Locomotor, Target);  // asm loc_4696A2
        }
        return;
    }

    // ---- Airstrike: mark the victim for a bombing run ---------------------
    if (Warhead->Airstrike) {
        if (Owner == nullptr || Target == nullptr) return;
        if (Target->WhatAmI() != AbstractType::Building) return;
        // asm: AirstrikeClass_CanFire / _Start on the owner's airstrike slot.
        Target->StartAirstrikeTimer(1);
        return;
    }

    // ---- DirectRocker: bonus damage scaled by the bullet's multiplier -----
    if (Warhead->DirectRocker) {
        if (Target == nullptr || Owner == nullptr) return;
        if (Target->WhatAmI() != AbstractType::Building) return;
        int32 dmg = DamageMultiplier * Damage / 256;       // asm: imul / sar 8
        if (dmg < 4) dmg = 4;                              // asm: fcom 4.0 clamp
        DamageArea::ApplyCellDamage(loc, dmg, Owner, Warhead, false, pOwnerHouse);
        return;
    }
}

// =============================================================================
// Spawn_Splash_Anim - asm: BulletClass_Fire loc_46A299..loc_46A2A1
//   Spawns the warhead's splash animation (or the rules "weapon nullify" anim)
//   at the impact point.
// =============================================================================
void BulletClass::Spawn_Splash_Anim(const CoordStruct& loc) {
    AnimTypeClass* pAnim = nullptr;
    if (Warhead != nullptr) pAnim = Warhead->Anim;
    if (pAnim == nullptr && WeaponType != nullptr) pAnim = WeaponType->Anim;
    if (pAnim == nullptr && RulesClass::Instance != nullptr) {
        pAnim = RulesClass::Instance->WeaponNullifyAnim;    // asm loc_46A2A1
    }
    if (pAnim == nullptr) return;

    CoordStruct pos = loc;
    pos.Z += BULLET_ANIM_Z_OFFSET;
    new AnimClass(pAnim, pos, 0, 1, 0x2600, 0, false);
}

// =============================================================================
// Explode - the warhead "explode" work-horse
//   asm: BulletClass_Fire (248129..249964), signature (this, CoordStruct* loc)
//   Applies camera shake, the warhead's special ladder, the radiation site, the
//   splash animation and the area damage.
// =============================================================================
void BulletClass::Explode(const CoordStruct& loc) {
    // ---- camera shake: the original picks a random amplitude in
    //      [ShakeXlo, ShakeXhi] and pushes it into the tactical screen shake
    //      state (TacticalClass).  The renderer-side shake queue lives outside
    //      the combat model; the amplitude is still resolved here so a future
    //      display hook can consume it. ----
    if (Warhead != nullptr && (Warhead->ShakeXlo != 0 || Warhead->ShakeXhi != 0)) {
        const int32 lo = Warhead->ShakeXlo;
        const int32 hi = Warhead->ShakeXhi;
        const int32 amp = (hi > lo) ? ((std::rand() % (hi - lo + 1)) + lo) : lo;
        (void)amp;
    }

    // ---- radiation site ---------------------------------------------------
    Apply_Radiation(loc);

    // ---- warhead special ladder -------------------------------------------
    Apply_Warhead_Special(loc);

    // ---- splash / impact animation ----------------------------------------
    Spawn_Splash_Anim(loc);


    // ---- area damage ------------------------------------------------------
    if (Warhead == nullptr) return;
    const int32 damage = (WeaponType != nullptr) ? WeaponType->Damage : Damage;
    HouseClass* pOwnerHouse = (Owner != nullptr) ? Owner->GetOwningHouse() : nullptr;
    const bool affectsAllies = Warhead->AffectsAllies;
    DamageArea::ApplyCellDamage(loc, damage, Owner, Warhead, affectsAllies, pOwnerHouse);
}

// =============================================================================
// Shrapnel - asm: BulletClass_Shrapnel (249965..250958)
//
//   Spawns `ShrapnelCount` sub-projectiles from the type's ShrapnelWeapon over
//   the CellSpreads ring around the target (or, failing that, the impact point),
//   skipping the owner, his allies and buildings.  A negative ShrapnelCount is a
//   ring radius, converted to a count from the owner's distance; a zero count
//   defaults to 3.
// =============================================================================
void BulletClass::Shrapnel() {
    if (Class == nullptr) return;

    const int32 count = GetShrapnel_Count();
    if (count <= 0) return;                                     // asm loc_46ADCD

    // asm loc_46A3D6: acquire the aim point; a building victim is skipped.
    TechnoClass* pAim = Target;
    if (pAim != nullptr && pAim->WhatAmI() == AbstractType::Building) return;
    if (pAim == nullptr && Owner != nullptr) {
        // asm vt+0x1BCh: the owner's current target stands in.
        pAim = static_cast<TechnoClass*>(Owner->GetTarget());
    }

    // asm: the shrapnel weapon falls back to the primary weapon.
    WeaponTypeClass* pWeapon = Class->ShrapnelWeapon;
    if (pWeapon == nullptr) pWeapon = WeaponType;
    if (pWeapon == nullptr || pWeapon->Projectile == nullptr) return;

    // asm: the CellSpreads ring -- one sub-munition per offset around the cell.
    static const int32 kRing[][2] = {
        {  0,  0 }, {  1,  0 }, { -1,  0 }, {  0,  1 }, {  0, -1 },
        {  1,  1 }, {  1, -1 }, { -1,  1 }, { -1, -1 }, {  2,  0 },
        { -2,  0 }, {  0,  2 }, {  0, -2 },
    };
    const int32 ringCount = static_cast<int32>(sizeof(kRing) / sizeof(kRing[0]));

    CoordStruct origin = Location;
    if (pAim != nullptr) origin = pAim->GetCoords();
    const CellStruct baseCell = Math::CoordToCell(origin);

    Randomizer* pRng = (ScenarioClass::Instance != nullptr)
                     ? &ScenarioClass::Instance->Random : nullptr;

    for (int32 i = 0; i < count && i < ringCount; ++i) {
        if (!IsBulletActive) break;                             // asm: test [edi+90h]
        if (MapClass::Instance == nullptr) break;

        const CellStruct cell(baseCell.X + static_cast<int16>(kRing[i][0]),
                              baseCell.Y + static_cast<int16>(kRing[i][1]));
        CellClass* pCell = MapClass::Instance->GetCellAt(cell);
        if (pCell == nullptr) continue;

        // asm loc_46A..: the nearest techno occupant of the ring cell; a
        // building, the owner and his allies are all skipped.
        CellStruct probe = cell;
        ObjectClass* pOcc = pCell->Cell_Techno(&probe, false, Owner);
        if (pOcc != nullptr && pOcc->WhatAmI() != AbstractType::Building) {
            TechnoClass* pVictim = static_cast<TechnoClass*>(pOcc);
            if (pVictim == Owner) continue;                     // asm cmp ebp,eax
            if (Owner != nullptr && Owner->Is_Ally(pVictim)) continue;
        }

        CoordStruct spawn = Math::CellToCoord(cell);
        spawn.Z = Location.Z;

        CoordStruct vel;
        if (pAim != nullptr) {
            const CoordStruct tp = pAim->GetCoords();
            vel.X = tp.X - spawn.X;
            vel.Y = tp.Y - spawn.Y;
            vel.Z = tp.Z - spawn.Z;
        } else {
            vel.X = pRng ? pRng->Next(-256, 256) : ((std::rand() % 513) - 256);
            vel.Y = pRng ? pRng->Next(-256, 256) : ((std::rand() % 513) - 256);
            vel.Z = 0;
        }

        Spawn_Shrapnel_Bullet(spawn, vel, pWeapon);             // asm CoCreateInstance
    }
}

// =============================================================================
// GetShrapnel_Count - asm: Shrapnel loc_46A34D..loc_46A3D2
//   type->ShrapnelCount (+0x2B8): positive is used as-is; zero defaults to 3; a
//   negative value is a ring radius resolved against the owner's distance.
// =============================================================================
int32 BulletClass::GetShrapnel_Count() const {
    if (Class == nullptr) return 0;
    const int32 count = Class->ShrapnelCount;                   // asm [eax+2B8h]
    if (count > 0) return count;
    if (count == 0) return 3;                                   // asm default 3
    if (Owner == nullptr) return 3;                             // asm: no owner -> 3
    const int32 d = Bullet_Distance3D(Location, Owner->GetCoords()) / BULLET_CELL_SIZE;
    const int32 n = -count - d;
    return (n > 0) ? n : 0;                                     // asm jle loc_46ADCD
}

// =============================================================================
// Spawn_Shrapnel_Bullet - asm: Shrapnel loc_46A5xx..loc_46A8D1
//   Creates one sub-munition bullet from `pWeapon`, stamps its weapon slot and
//   places it at `spawn` with the given launch velocity.
// =============================================================================
void BulletClass::Spawn_Shrapnel_Bullet(const CoordStruct& spawn,
                                        const CoordStruct& vel,
                                        WeaponTypeClass* pWeapon) {
    if (pWeapon == nullptr || pWeapon->Projectile == nullptr) return;
    BulletClass* pSub = BulletClass::Fire(pWeapon->Projectile, pWeapon,
                                          spawn, spawn, Owner,
                                          pWeapon->Damage, pWeapon->Warhead);
    if (pSub == nullptr) return;
    pSub->WeaponType = pWeapon;                                 // asm: [..+130h]
    pSub->SetMovement(spawn, vel, pWeapon->Projectile->Get_Speed());
}

// =============================================================================
// NukeMaker - asm: BulletClass_NukeMaker (251552..251747)
//
//   Re-fires the "NukePayload" weapon down onto the current target:
//     * cell = Coord_Cell(Target->GetCoords());
//     * pWeapon = WeaponTypeClass_FindIndex("NukePayload") -> vec_WeaponTypes;
//     * create + Initialize a bright BulletClass (type/speed/damage/warhead from
//       that weapon), AddRef it and stamp its Weapon slot;
//     * pos = the cell centre, its Z raised by this bullet's own weapon
//       projectile's DetonationAltitude (+0x2BCh);
//     * the launch velocity is built from the constant angle
//       0x3FF921C90FE8FBDA (~pi/2), then Place(pos, vel).
// =============================================================================
void BulletClass::NukeMaker() {
    if (Target == nullptr) return;

    // asm: the target's cell centre is the impact column.
    const CellStruct cell = Math::CoordToCell(Target->GetCoords());
    CoordStruct pos = Math::CellToCoord(cell);

    // asm: WeaponTypeClass_FindIndex("NukePayload") -> vec_WeaponTypes[idx].
    WeaponTypeClass* pWeapon = WeaponTypeClass::Find("NukePayload");
    if (pWeapon == nullptr || pWeapon->Projectile == nullptr) return;

    // asm: this bullet's weapon projectile DetonationAltitude raises the Z.
    if (WeaponType == nullptr || WeaponType->Projectile == nullptr) return;
    pos.Z += WeaponType->Projectile->DetonationAltitude;

    // asm: CoCreateInstance + BulletClass_Initialize(bright = 1).
    BulletClass* pBullet = new BulletClass(pWeapon->Projectile);
    if (pBullet == nullptr) return;
    pBullet->Initialize(pWeapon->Projectile, pos, Owner, pWeapon->Damage,
                        pWeapon->Warhead, pWeapon->Projectile->Get_Speed(), true);
    pBullet->AddRef();                                  // asm: vt+0xD4 (AddRef)
    pBullet->WeaponType = pWeapon;                      // asm: [edi+130h] = esi

    // asm: the direction vector from the angle 0x3FF921C90FE8FBDA (~pi/2).
    const double ang = 1.5707963267948966;
    const double s = std::sin(ang);
    const double c = std::cos(ang);
    CoordStruct vel;
    vel.X = Bullet_FloorToInt(s);
    vel.Y = Bullet_FloorToInt(-c * s);
    vel.Z = Bullet_FloorToInt(-c * c);

    // asm: bullet->Place(pos, vel)   (vt+0x74 == SetMovement).
    pBullet->SetMovement(pos, vel, pWeapon->Projectile->Get_Speed());
}

// =============================================================================
// TargetWentAway - asm: BulletClass_TargetWentAway (246779..246856)
//   Resolves the vanished target's last cell.  It is dropped entirely under
//   ScenarioInit, when the target is dead, or when the cell is the sentinel
//   (asm stru_89DDF0); otherwise the cell is retargeted (asm loc_4684AB stores
//   the CellClass* into the slot -- the port caches its centre coords because
//   Target is typed TechnoClass*).
// =============================================================================
void BulletClass::TargetWentAway() {
    if (Target == nullptr) return;

    const CellStruct cell = Math::CoordToCell(Target->GetCoords());
    const bool sentinel = (cell.X == MapClass::DefaultCellCoords.X
                        && cell.Y == MapClass::DefaultCellCoords.Y);

    // asm loc_4684C5: ScenarioInit / Target->IsDead() / sentinel -> clear.
    if (ScenarioInit != 0 || Target->IsDead() || sentinel) {
        Target = nullptr;                               // asm: mov [esi+10Ch], 0
        return;
    }
    Retarget_To_Cell(true);                             // asm loc_4684AB
}

// =============================================================================
// Retarget_To_Cell - asm: PointerGotInvalid loc_468509..loc_4685A2 and
//   TargetWentAway loc_4684AB..loc_4684DA.
//   Resolves the invalidated target's cell into the target slot: the asm stores
//   a CellClass* there; the port caches the cell centre coordinates instead and
//   clears the target on the origin sentinel.
// =============================================================================
void BulletClass::Retarget_To_Cell(bool keepAsCell) {
    if (Target == nullptr) return;

    const CellStruct cell = Math::CoordToCell(Target->GetCoords());
    const bool sentinel = (cell.X == MapClass::DefaultCellCoords.X
                        && cell.Y == MapClass::DefaultCellCoords.Y);

    if (!keepAsCell || ScenarioInit != 0 || Target->IsDead() || sentinel) {
        Target = nullptr;
        return;
    }
    const CoordStruct center = Math::CellToCoord(cell);
    TargetCoords = center;
    FuseCoords = center;
}

// =============================================================================
// GetAnimRate - asm: BulletClass_GetAnimRate (246348..246407)
//   Returns the sprite frame index.  For non-rotating types the frame comes
//   from the pitch of the horizontal velocity vector (a 32-entry heading table
//   dword_7F4890); when the type carries an anim frame range the live counter
//   [0x12C] wins.
// =============================================================================
int32 BulletClass::Compute_Pitch_Frame() const {
    // asm: arctanfoo(Velocity[0], -Velocity[1]); then (angle - pi/2) * -10430.06
    const double angle = std::atan2(Velocity[0], -Velocity[1]);
    const int32 raw = Bullet_FloorToInt((angle - (BULLET_PI / 2.0)) * -10430.06004058427);
    const int32 word = raw & 0xFFFF;
    int32 idx = ((word >> 10) + 1) >> 1;                // shr 0Ah / inc / shr 1
    idx &= 0x1F;
    return kBulletFacingFrame[idx];                     // asm: dword_7F4890[idx]
}

int32 BulletClass::GetAnimRate() const {
    int32 eax = 0;
    if (Class != nullptr && !Class->Rotates) {
        eax = Compute_Pitch_Frame();                    // asm loc_468064 block
    }
    if (Class != nullptr && (Class->AnimLow != 0 || Class->AnimHigh != 0)) {
        eax = AnimFrame;                                // asm loc_46807E
    }
    return eax;
}

// =============================================================================
// InitScalable - asm: BulletClass_InitScalable (251479..251551)
//   Registers the scalable projectile in vec_Bullets and re-stamps the type's
//   scaled spawn delay with (type->SpawnDelay + (bulletCount-5)/3).
// =============================================================================
void BulletClass::InitScalable() {
    if (Class == nullptr) return;

    if (BulletClass::Array != nullptr) {
        BulletClass::Array->Add(this);                  // asm loc_46B2BE
    }

    int32 count = (BulletClass::Array != nullptr) ? BulletClass::Array->Count : 1;
    int32 n = count - 5;                                // asm: add 0FFFFFFFBh
    if (n < 0) n = 0;
    const int32 scaled = Class->SpawnDelay + n / 3;     // asm: 55555556h magic
    Class->SetSpawnDelay(scaled);                       // asm: BulletTypeClass_SetSpawnDelay
}

// asm: BulletClass_IsHoming_obsolete (251188..251203) -- type->ROT > 0.
bool BulletClass::IsHoming_obsolete() const {
    if (Class == nullptr) return false;
    return Class->ROT > 0;                              // asm: setnle al
}

// =============================================================================
// Draw - asm: BulletClass_Draw (0x467F60, 246417..246778)
//   __thiscall(this, Point2D* pos, Rectangle* bounds), retn 8.
//
//   1) 剔除：Scenario 的 shroud 位（test ch,10h）→ Map.IsCellShrouded(Location)；
//      Class->Inviso(+0x29E)；[this+0x158h]。任一条命中直接 retn。
//   2) Class->Voxel（[Class+0x236h]）→ loc_4680E2 的体素支路：由
//      Velocity(+0xE8/+0xF0/+0xF8) 的偏航/俯仰角构造 Matrix3D，
//      sub_754BE0 叠上世界坐标，再 call BulletClass_DrawVXL。
//   3) 否则 loc_468258 的 SHP 支路。
//
//   SHP 支路逐条（asm 原文）：
//      SHP      = GetImage()                       ; vt+0x6C
//      frame    = GetAnimRate()                    ; vt+0x1E8
//      height   = GetHeight()                      ; vt+0x1C8
//      floor    = Map.GetCellFloorHeight(Location)
//      if (![this+0x8Ch] /*IsInAir*/) {            ; loc_4682A6
//          cell = Map.Get_Target_Cell(Location)
//          if (cell->flags(+0x140) & 0x100)        ; test ch,1 —— 桥面格
//              if (height >= dword_89DE64) { height -= dword_89DE64;
//                                             floor  += dword_89DE64; }
//      }
//      if (height > 0 && Class->Shadow(+0x29A))    ; loc_468301 阴影
//          ShadowFlags = 0x2601 (Remap|Centered|Transparent|ZGradientDeg45)
//          ShadowZAdj  = -0x0A - Draw_AdjustForBuildingZ(floor)
//      MainFlags   = 0x2E00 (Centered|Transparent|Alpha|ZGradientDeg45)
//      MainZAdj    = -0x1E - Draw_AdjustForBuildingZ(GetZ())
//      Convert     = StandardPalette
//                    Class->AnimPalette(+0x2A8)   -> Convert_ANIM
//                    Class->FirersPalette(+0x2A9) -> ColorScheme[[this+0x114]]->+0x30C
//
//   dword_89DE64 = floor(dword_89DE70*4+0.5) = 416，与 MapClass::HalfCellHeight
//   同值（Map/MapDamageAtCell.cpp 的 InitCellHeightConstants 已按同一公式算）。
// =============================================================================
void BulletClass::Draw(Point2D* pCoord, RectangleStruct* pRect) {
    if (Class == nullptr) return;

    // asm: Scenario shrouded-cell test (only when the shroud flag is set).
    if (MapClass::Instance != nullptr && MapClass::Instance->IsCellShrouded(Location)) {
        return;
    }
    // asm: type+0x29E (Inviso) -> never drawn.
    if (Class->Inviso) return;
    // asm: [esi+158h] (MustSpawnNextAnim / hidden) -> skip.
    if (MustSpawnNextAnim) return;
    if (pCoord == nullptr) return;

    // ---- 屏幕落点：原版由 TacticalMap->Coord_To_Pixel 给出。 --------------
    Point2D screen = *pCoord;
    if (TacticalClass::Instance != nullptr) {
        CoordStruct world = Location;
        screen = TacticalClass::Instance->CoordsToScreen(world);
    }

    Rectangle clip;
    const Rectangle* pClip = nullptr;
    if (pRect != nullptr) {
        clip = Rectangle(pRect->X, pRect->Y, pRect->Width, pRect->Height);
        pClip = &clip;
    }

    if (Class->Voxel) {
        // asm loc_4680E2 -> BulletClass_DrawVXL(this, &Class[0xB0], matrix)
        DrawVXL(&screen, pRect);
        *pCoord = screen;                      // 落点回填给调用方
        return;
    }

    // ---- SHP 支路 -------------------------------------------------------
    SHPStruct* pShape = Class->GetImage();      // asm: call [eax+6Ch]
    if (pShape == nullptr) return;              // asm: jz loc_468422

    const int32 frame = GetAnimRate();          // asm: call [edx+1E8h]

    // asm: TechnoClass_GetHeight —— [this+0xA4] - 地面高度 - (inAir ? 0x1A0 : 0)
    int32 floorZ = 0;
    if (MapClass::Instance != nullptr) {
        floorZ = MapClass::Instance->GetCellFloorHeight(Location);
    }
    int32 height = Location.Z - floorZ
                 - (IsInAir ? BULLET_AIR_LAYER_Z : 0);

    // asm loc_4682A6: 桥面格把地板整体抬高 416，同时扣掉弹体的相对高度。
    if (!IsInAir && MapClass::Instance != nullptr) {
        CellClass* pCell = MapClass::Instance->GetCellAt(Location);
        if (pCell != nullptr && (pCell->Field_140 & 0x0100u) != 0u
            && height >= MapClass::HalfCellHeight) {
            height -= MapClass::HalfCellHeight;
            floorZ += MapClass::HalfCellHeight;
        }
    }

    Surface* pSurface = UnitArt::GetTacticalSurface();
    if (pSurface == nullptr) {
        *pCoord = screen;
        return;
    }

    // ---- 阴影 -----------------------------------------------------------
    // asm loc_468301: 只有离地（height>0）且类型声明了 Shadow 才画。
    if (height > 0 && Class->Shadow) {
        Point2D shadowPos = screen;
        shadowPos.Y += Draw_AdjustForBuildingZ(height);
        CC_Draw_Shape(pSurface, StandardPalette, pShape, frame, &shadowPos, pClip,
                      static_cast<BlitterFlags>(0x2601),
                      0, -0x0A - Draw_AdjustForBuildingZ(floorZ),
                      0, 0, 0, nullptr, 0, 0, 0);
    }

    // ---- 本体 -----------------------------------------------------------
    // asm loc_468379: 调色板三选一 —— AnimPalette(+0x2A8) 用 Convert_ANIM，
    // 否则 FirersPalette(+0x2A9) 用发射者色阶，都没有则 StandardPalette。
    // Convert_ANIM 在本树没有独立转换器，AnimPalette/默认都落标准盘。
    ConvertClass* pConvert = StandardPalette;
    if (!Class->AnimPalette && Class->FirersPalette) {
        // 注意：成员 ColorScheme 与类型名同名，这里显式走全局作用域。
        const int32 scheme = (ColorScheme >= 0) ? ColorScheme : -1;
        if (scheme >= 0 && ::ColorScheme::Array != nullptr
            && scheme < ::ColorScheme::Array->Size()) {
            ::ColorScheme* pScheme = (*::ColorScheme::Array)[scheme];
            if (pScheme != nullptr && pScheme->LightConvert != nullptr) {
                pConvert = pScheme->LightConvert;   // asm: [scheme+30Ch]
            }
        }
    }

    // asm: Draw_AdjustForBuildingZ(this->GetZ()) ; ZAdjust = -0x1E - that
    const int32 zAdjust = -0x1E - Draw_AdjustForBuildingZ(GetZ());

    CC_Draw_Shape(pSurface, pConvert, pShape, frame, &screen, pClip,
                  static_cast<BlitterFlags>(0x2E00),
                  0, zAdjust, 0, 0, 0, nullptr, 0, 0, 0);

    *pCoord = screen;                           // 落点回填给调用方
}

// =============================================================================
// DrawVXL - asm: BulletClass_DrawVXL (0x46B090, 251273..251448), retn 1Ch
//
//   参数（asm）：arg_0 = &Class[0xB0]（{VXL*, HVA*} 一对），
//                arg_4 = 模型矩阵（Draw 里用 sub_754BE0/Matrix3D_Multiply 叠好），
//                arg_8 = 屏幕落点，arg_C = 裁剪矩形，arg_10 = HVA 帧号，
//                arg_14 = 旗标，arg_18 = 亮度/色调。
//
//   逐条：
//      sub_753C80(HVA, matrix, 0, ...)            ; 起始变换
//      Voxel_753E00()                             ; 每帧重算法线/明暗
//      for (section = 0; section < vxl->SectionCount; ++section) {
//          if (hva) { matrix = Matrix3D_Multiply(base,
//                        hva->data + (frame % hva->frameCount) * 3*16); }
//          sub_7540F0(matrix, section, 0)         ; 该 section 入绘制队列
//      }
//      flags &= ~0x10 (ZRead) ;
//      if (Class->Flat(+0x2F7)) { flags |= 0x2000; lightDelta = -65; }
//      Convert = ColorScheme[[Class+0x2D4] /*Color*/]->[0x30C]
//      LightConvertClass_SelectBlitter0 + sub_753C70 + CC_Draw_Shape_4AF2A0
//
//   本树把上面整段交给 Rendering/VoxelRaster 的 Voxel_Rasterize()（同一套
//   画家算法 + 16 级明暗），模型按类型 Image 名懒加载并缓存 —— 与
//   UnitClass::DrawAsVXL 采用的方式一致。体素光栅器由 Rendering/VoxelRaster.cpp
//   提供（另一处并行修改），此处只做接线。
// =============================================================================
namespace
{
    // 体素模型缓存条目（姓名相同即同一条目；载入失败不再重试）。
    struct BulletVoxelEntry
    {
        char     Name[0x20];
        VXLClass VXL;
        HVAClass HVA;
        bool     HasHVA;
        bool     Loaded;
    };

    std::vector<BulletVoxelEntry*>* s_pBulletVoxels = nullptr;

    // asm: ObjectTypeClass 的 BodyVXL/BodyHVA 由 Load3DArt 一次性载入；
    //   本树 BulletTypeClass 只保留 Image 名，故按名懒加载（name.vxl/name.hva）。
    BulletVoxelEntry* Bullet_Get_Voxel(const char* pName)
    {
        if (pName == nullptr || pName[0] == '\0') return nullptr;

        if (s_pBulletVoxels == nullptr) s_pBulletVoxels = new std::vector<BulletVoxelEntry*>();

        for (size_t i = 0; i < s_pBulletVoxels->size(); ++i) {
            if (std::strncmp((*s_pBulletVoxels)[i]->Name, pName,
                             sizeof(BulletVoxelEntry::Name) - 1) == 0) {
                return (*s_pBulletVoxels)[i];
            }
        }

        BulletVoxelEntry* pEntry = new BulletVoxelEntry();
        pEntry->HasHVA = false;
        pEntry->Loaded = false;
        std::strncpy(pEntry->Name, pName, sizeof(BulletVoxelEntry::Name) - 1);

        char path[0x40];
        std::snprintf(path, sizeof(path), "%s.vxl", pName);
        pEntry->Loaded = pEntry->VXL.LoadFromFile(path) && pEntry->VXL.GetSectionCount() > 0;
        if (pEntry->Loaded) {
            std::snprintf(path, sizeof(path), "%s.hva", pName);
            pEntry->HasHVA = pEntry->HVA.LoadFromFile(path);
        }

        s_pBulletVoxels->push_back(pEntry);
        return pEntry;
    }
}

void BulletClass::DrawVXL(Point2D* pScreenPos, const RectangleStruct* pClip) {
    if (Class == nullptr || pScreenPos == nullptr) return;

    Surface* pSurface = UnitArt::GetTacticalSurface();
    if (pSurface == nullptr) return;

    BulletVoxelEntry* pModel = Bullet_Get_Voxel(Class->Image);
    if (pModel == nullptr || !pModel->Loaded) {
        // 原版：没有体素模型时该弹体不画（Voxel 类型没有 SHP 兜底）。
        return;
    }

    // asm: Convert = ColorScheme[[Class+0x2D4]]->[0x30C]
    ConvertClass* pConvert = StandardPalette;
    if (Class->Color >= 0 && ::ColorScheme::Array != nullptr
        && Class->Color < ::ColorScheme::Array->Size()) {
        ::ColorScheme* pScheme = (*::ColorScheme::Array)[Class->Color];
        if (pScheme != nullptr && pScheme->LightConvert != nullptr) {
            pConvert = pScheme->LightConvert;
        }
    }
    if (pConvert == nullptr) return;

    // asm: yaw = arctan(Vel.X, Vel.Y)，pitch = arctan(Vel.Z, sqrt(Vx²+Vy²))，
    //   两者都被量化成 5 位（shr 0Ah / inc / shr 1 / and 1Fh）再乘表值送进
    //   Matrix3D。本树直接用连续角，绕竖直轴旋转（面朝 0 = -Z 方向）。
    float yaw = 0.0f;
    float pitch = 0.0f;
    {
        const double vx = Velocity[0];
        const double vy = Velocity[1];
        const double vz = Velocity[2];
        if (vx != 0.0 || vy != 0.0) {
            yaw = static_cast<float>(std::atan2(vx, -vy));
            const double planar = std::sqrt(vx * vx + vy * vy);
            if (planar > 0.0) pitch = static_cast<float>(std::atan2(vz, planar));
        }
    }

    Matrix4 transform = Matrix4::RotationY(-yaw);
    Matrix4 tilt;                       // 俯仰：绕模型 X 轴
    {
        const float c = std::cos(pitch);
        const float s = std::sin(pitch);
        tilt.Data[0] = 1.0f;
        tilt.Data[5] = c;    tilt.Data[6] = -s;
        tilt.Data[9] = s;    tilt.Data[10] = c;
    }
    transform.PostMultiply(tilt);

    // asm arg_10: HVA 帧号，取模 hva->frameCount（div dword ptr [ebx+8]）。
    int32 hvaFrame = 0;
    HVAClass* pHVA = nullptr;
    if (pModel->HasHVA && pModel->HVA.GetFrameCount() > 0) {
        pHVA = &pModel->HVA;
        hvaFrame = AnimFrame % pModel->HVA.GetFrameCount();
    }

    // asm arg_14 的处理：清 0x10(ZRead)；Class->Flat 再补 0x2000。
    //   旗标在这里折算成亮度：1000 = 原色。
    int32 brightness = 1000;

    Rectangle clip;
    const Rectangle* pRect = nullptr;
    if (pClip != nullptr) {
        clip = Rectangle(pClip->X, pClip->Y, pClip->Width, pClip->Height);
        if (clip.Width > 0 && clip.Height > 0) pRect = &clip;
    }

    // asm: 逐 section 走 sub_7540F0；本树用 Voxel_Rasterize 的 section 参数。
    const int32 sectionCount = pModel->VXL.GetSectionCount();
    for (int32 section = 0; section < sectionCount; ++section) {
        Voxel_Rasterize(pSurface, pConvert, &pModel->VXL, section,
                        pHVA, hvaFrame, transform, *pScreenPos,
                        brightness, 0, pRect);
    }
}

// =============================================================================
// InWhichLayer - asm: BulletClass_InWhichLayer (247536..247545)
//   `Flat` (+0x2F7) cleared -> 3 (Air/Top), else 1 (Surface).
// =============================================================================
Layer BulletClass::InWhichLayer() const {
    // asm: mov al,[eax+Flat]; neg al; sbb eax,eax; and al,-2; add eax,3
    return (Class != nullptr && Class->Flat) ? Layer::Surface : Layer::Air;
}

// =============================================================================
// GetAbstractDerivationID / GetClassID / GetSize / GetType / Size
// =============================================================================
AbstractType BulletClass::GetAbstractDerivationID() const {
    return AbstractType::Bullet;   // asm: mov eax,8
}

HRESULT BulletClass::GetClassID(CLSID* pClassID) {
    if (pClassID == nullptr) return E_POINTER;
    // asm: copies CLSID_BulletClass (16 bytes) verbatim.
    pClassID->Data1 = 0x424C5442;   // 'BTLB'
    pClassID->Data2 = 0x4C55;
    pClassID->Data3 = 0x4554;
    for (int32 i = 0; i < 8; ++i) pClassID->Data4[i] = 0x42;
    return S_OK;
}

int32 BulletClass::Size() const {
    return 0x160;   // asm: mov eax,160h
}

int32 BulletClass::Get_Bullet_Size() const {
    return Size();
}

ObjectTypeClass* BulletClass::GetType() const {
    return Class;   // asm: mov eax,[ecx+0ACh]
}

// =============================================================================
// AddRef / Release / SetLayer
// =============================================================================
ULONG BulletClass::AddRef() {
    return ObjectClass::AddRef();
}

ULONG BulletClass::Release() {
    return ObjectClass::Release();
}

// asm: BulletClass_SetLayer (243922) -- forwards to ObjectClass::Mark.
bool BulletClass::Mark_Layer(int32 idxLayer) {
    return ObjectClass::Mark_Layer(idxLayer);
}

// =============================================================================
// Load / Save
// =============================================================================
HRESULT BulletClass::Load(IStream* pStm) {
    HRESULT hr = ObjectClass::Load(pStm);
    if (hr < 0) return hr;

    ObjectClass::LoadTables(pStm);
    Timer = 0;       // asm: Timer_StartWithDuration(this+0xB8, 0)
    Timer2 = 0;      // asm: Timer_StartWithDuration(this+0xC4, 0)

    // asm: if the pending trailer anim exists, re-register it in vec_Abstracts.
    if (NextAnim != nullptr) {
        // swizzled below; the abstract-vector insert is handled by the manager.
    }

    // asm: SwizzleManagerClass_Swizzle over +0xAC +0xB0 +0x128 +0x10C +0x130
    //      +0x154.
    if (Class != nullptr && Class->IsDead()) Class = nullptr;
    if (Owner != nullptr && Owner->IsDead()) Owner = nullptr;
    if (Target != nullptr && Target->IsDead()) Target = nullptr;
    return S_OK;
}

HRESULT BulletClass::Save(IStream* pStm, BOOL fClearDirty) {
    // asm: BulletClass_Save tail-calls AbstractClass_Save with the same args.
    return ObjectClass::Save(pStm, fClearDirty);
}

// =============================================================================
// PointerGotInvalid - asm: BulletClass_PointerGotInvalid (246857..247055)
//   retn 8 -> 2 args.  Clears every cached pointer that matches, retargets the
//   target slot onto its last cell, and unregisters a stale bullet from the
//   global array.
// =============================================================================
void BulletClass::PointerGotInvalid(AbstractClass* pAbstract, bool removed) {
    ObjectClass::PointerGotInvalid(pAbstract, removed);    // asm: ObjectClass::Detach

    if (Owner == pAbstract) Owner = nullptr;              // asm: +0B0h

    // asm loc_468509: the target slot retargets onto the victim's last cell
    // (unconditionally -- the asm does not gate this on `removed`).
    if (Target == pAbstract) {
        Retarget_To_Cell(true);
    }

    if (Class == pAbstract) Class = nullptr;              // asm: +0ACh
    if (WeaponType == static_cast<WeaponTypeClass*>(pAbstract)) WeaponType = nullptr; // +130h
    if (NextAnim == pAbstract) NextAnim = nullptr;        // asm: +154h

    // asm loc_4685CC: a removed *scalable* bullet is dropped from vec_Bullets.
    if (removed && pAbstract != nullptr
        && pAbstract->WhatAmI() == AbstractType::Bullet) {
        BulletClass* pOther = static_cast<BulletClass*>(pAbstract);
        if (pOther->Class != nullptr && pOther->Class->Scalable) {
            if (BulletClass::Array != nullptr) {
                for (int32 i = 0; i < BulletClass::Array->Count; ++i) {
                    if ((*BulletClass::Array)[i] == pOther) {
                        BulletClass::Array->Remove(i);
                        break;
                    }
                }
            }
        }
    }
}

// =============================================================================
// Fire - create and launch a bullet
//   asm: BulletType_CreateBullet (251204..251272): CoCreateInstance +
//   BulletClass_Initialize.  The port heap-allocates instead of using COM.
// =============================================================================
BulletClass* BulletClass::Fire(BulletTypeClass* pType, WeaponTypeClass* pWeapon,
                               CoordStruct source, CoordStruct target,
                               TechnoClass* pOwner, int32 damage,
                               WarheadTypeClass* pWarhead) {
    if (pType == nullptr) return nullptr;                 // asm: xor eax,eax

    BulletClass* pBullet = new BulletClass(pType);
    if (pBullet == nullptr) return nullptr;

    pBullet->Initialize(pType, source, pOwner, damage, pWarhead,
                        (pType->ProjectileSpeed > 0) ? pType->ProjectileSpeed
                                                     : BULLET_DEFAULT_SPEED,
                        false);
    pBullet->Owner = pOwner;
    pBullet->WeaponType = pWeapon;
    pBullet->Warhead = pWarhead;
    pBullet->Damage = damage;
    pBullet->Strength = damage;
    pBullet->Location = source;
    pBullet->StartCoords = source;
    pBullet->SourceCoords = source;
    pBullet->LastCoords = source;
    pBullet->TargetCoords = target;
    pBullet->FuseCoords = target;
    pBullet->CreationFrame = Game::CurrentFrame;
    pBullet->Health = pType->Arm;
    pBullet->Range = (pWeapon != nullptr) ? pWeapon->GetAttackRange() : 0;
    pBullet->IsBulletActive = true;
    pBullet->IsBulletDetonated = false;
    pBullet->IsIncoming = true;
    pBullet->DamageMultiplier = 256;

    // Initial velocity toward the target.
    pBullet->Velocity[0] = static_cast<double>(target.X - source.X);
    pBullet->Velocity[1] = static_cast<double>(target.Y - source.Y);
    pBullet->Velocity[2] = static_cast<double>(target.Z - source.Z);
    Bullet_Normalize(pBullet->Velocity);

    return pBullet;
}

// =============================================================================
// Simple accessors
// =============================================================================
void BulletClass::Destroy() {
    IsBulletActive = false;
    IsBulletDetonated = true;
}

// asm: BulletClass_SetTarget (251807) -- [ecx+10Ch] = a2 (TechnoClass*), retn 4.
void BulletClass::SetTarget(TechnoClass* pTarget) { Target = pTarget; }
TechnoClass* BulletClass::GetTarget() const { return Target; }
void BulletClass::SetOwner(TechnoClass* pOwner) { Owner = pOwner; }
TechnoClass* BulletClass::GetOwner() const { return Owner; }
CoordStruct BulletClass::GetLocation() const { return Location; }

// asm: BulletClass vtable +0x48 -- the projectile's own coordinate getter
// returns a copy of the in-flight position ([+0x9C]); ObjectClass::GetCoords
// already does this, but the override pins the slot explicitly.
CoordStruct* BulletClass::GetCoords(CoordStruct* pCrd) const {
    if (pCrd == nullptr) return nullptr;
    *pCrd = Location;
    return pCrd;
}

// asm: BulletClass_GetZ (+0x1C8 on the bullet vtable) -- the Z of the in-flight
// position.  Used by the nuke clamp and the terrain-impact height test.
int32 BulletClass::GetZ() const { return Location.Z; }

bool BulletClass::IsActive() const { return IsBulletActive && !IsBulletDetonated; }
bool BulletClass::IsDetonated() const { return IsBulletDetonated; }
int32 BulletClass::GetDamage() const { return Damage; }
int32 BulletClass::GetDamageMultiplier() const { return DamageMultiplier; }
bool BulletClass::IsBulletGravity() const { return IsGravity; }
bool BulletClass::IsBulletInaccurate() const { return IsInaccurate; }
bool BulletClass::IsTargetingAA() const { return IsAA; }
bool BulletClass::IsTargetingAG() const { return IsAG; }
BulletTypeClass* BulletClass::GetBulletType() const { return Class; }
BulletMotion BulletClass::GetMotion() const { return Motion; }
FacingType BulletClass::GetFacing() const { return Facing; }
void BulletClass::SetFacing(FacingType facing) { Facing = facing; }
int32 BulletClass::GetSpeed() const { return Speed; }

void BulletClass::SetSpeed(int32 speed) {
    if (speed < BULLET_MIN_SPEED) speed = BULLET_MIN_SPEED;
    if (speed > BULLET_MAX_SPEED) speed = BULLET_MAX_SPEED;
    Speed = speed;
}

void BulletClass::MoveTo(CoordStruct location) {
    Location = location;
    LastCoords = Location;
}

void BulletClass::SetRange(int32 range) {
    if (range < 0) range = 0;
    Range = range;
}

void BulletClass::SetWeaponType(WeaponTypeClass* pWeapon) { WeaponType = pWeapon; }
WeaponTypeClass* BulletClass::GetWeaponType() const { return WeaponType; }
