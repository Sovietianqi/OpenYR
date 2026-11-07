#include "BulletTypeClass.h"
#include "BulletClass.h"
#include <Combat/WeaponTypeClass.h>
#include <Animations/AnimTypeClass.h>
#include "../INI/INIClass.h"
#include "../IO/CRC.h"
#include "../Core/Memory.h"
#include "../Math/CoordStruct.h"

#include <cstring>
#include <cstdio>

// ============================================================================
// BulletTypeClass.cpp - Projectile type implementation
// ============================================================================
// A BulletTypeClass defines the flight model for a weapon projectile. Each
// entry in the [ProjectileTypes] INI block has its own section containing
// keys that control trajectory (Arcing, Dropping, Level), targeting (AA, AG,
// ASW, AN), collision (SubjectToCliffs, SubjectToElevation, SubjectToWalls),
// homing (CourseLocked, Proximity, ROT) and visual properties (Inviso,
// Shadow, FlakScatter).
//
// This file implements:
//   * Static registry (Array) management and name-based lookup
//   * Construction / destruction with full field initialisation
//   * Binary stream persistence (Load / Save)
//   * INI parsing and writing
//   * CRC computation for multiplayer sync verification
//   * Accessor methods for all projectile properties
// ============================================================================

DynamicVectorClass<BulletTypeClass*>* BulletTypeClass::Array = nullptr;

// ----------------------------------------------------------------------------
// Find - locate a projectile type by its (case-insensitive) ID.
// ----------------------------------------------------------------------------
BulletTypeClass* BulletTypeClass::Find(const char* pID) {
    if (!Array || !pID) return nullptr;
    for (int32 i = 0; i < Array->Count; ++i) {
        BulletTypeClass* item = Array->GetItem(i);
        if (item && !_strcmpi(item->ID, pID)) {
            return item;
        }
    }
    return nullptr;
}

// ----------------------------------------------------------------------------
// FindOrAllocate - return an existing projectile type or allocate a new one.
// ----------------------------------------------------------------------------
BulletTypeClass* BulletTypeClass::FindOrAllocate(const char* pID) {
    if (!pID || !pID[0]) return nullptr;
    if (!_strcmpi(pID, "<none>") || !_strcmpi(pID, "none")) return nullptr;

    BulletTypeClass* found = Find(pID);
    if (found) return found;

    BulletTypeClass* newItem = GameCreate<BulletTypeClass>(pID);
    if (newItem && Array) {
        Array->Add(newItem);
    }
    return newItem;
}

int32 BulletTypeClass::GetCount() {
    return Array ? Array->Count : 0;
}

// ============================================================================
// Construction / destruction
//   asm: BulletTypeClass_CTOR (252848..) -- the field defaults are read
//   verbatim from the listing:
//     +0x294 Airburst=0        +0x2A4 AA=0          +0x2D8 Trailer=0
//     +0x295 Floater=0         +0x2A5 AG=1          +0x2DC ROT=0
//     +0x296 SubjectToCliffs=0 +0x2A6 Degenerates=0 +0x2E0 CourseLockDuration=0
//     +0x297 SubjectToElev=0   +0x2A7 Bouncy=0      +0x2E4 SpawnDelay=3
//     +0x298 SubjectToWalls=0  +0x2A8 AnimPalette=0 +0x2E8 ScaledSpawnDelay=0
//     +0x299 VeryHigh=0        +0x2A9 FirersPalette=0 +0x2EC Scalable=0
//     +0x29A Shadow=1          +0x2AC Cluster=1     +0x2F0 Arm=0
//     +0x29B Arcing=0          +0x2B0 AirburstWeapon=0   +0x2F4 AnimLow=0
//     +0x29C Dropping=0        +0x2B4 ShrapnelWeapon=0   +0x2F5 AnimHigh=0
//     +0x29D Level=0           +0x2B8 ShrapnelCount=0    +0x2F6 AnimRate=0
//     +0x29E Inviso=0          +0x2BC DetonationAltitude=0 +0x2F7 Flat=0
//     +0x29F Proximity=0       +0x2C0 Vertical=0
//     +0x2A0 Ranged=0          +0x2C8 Elasticity=0.75 (double)
//     +0x2A1 Rotates=1         +0x2D0 Acceleration=3
//     +0x2A2 Inaccurate=0      +0x2D4 Color=0
//     +0x2A3 FlakScatter=0
// ============================================================================

BulletTypeClass::BulletTypeClass(const char* pID) noexcept
    : ObjectTypeClass(pID),
      ROT(0), Arcing(false), Dropping(false), Inviso(false),
      FlakScatter(false), SubjectToCliffs(false), SubjectToElevation(false),
      SubjectToWalls(false), VeryHigh(false), High(false), Shadow(true),
      AA(false), AG(true), ASW(false), AN(false), Inaccurate(false),
      NoRotate(false), Level(false), Proximity(false), Ranged(false),
      Scalable(false), ScaledSpawnDelay(0), CourseLocked(false), Arm(0),
      ProjectileSpeed(100),
      CourseLockDuration(0), Elasticity(0.75), Acceleration(3), Color(0),
      Floater(false), Degenerates(false), Bouncy(false), Airburst(false),
      Cluster(1), Trailer(nullptr), SpawnDelay(3), Rotates(true), Flat(false),
      AirburstWeapon(nullptr), ShrapnelWeapon(nullptr), ShrapnelCount(0),
      DetonationAltitude(0), Vertical(false), FirersPalette(false),
      AnimLow(0), AnimHigh(0), AnimRate(0), AnimPalette(false),
      TrajectoryKind(0), HalfGravity(false) {
    Image[0] = '\0';
}

BulletTypeClass::~BulletTypeClass() {
}

// ============================================================================
// IPersistStream
//   asm: BulletTypeClass_Load (0x46C6xx) / BulletTypeClass_Save (0x46C7xx).
//   Both delegate to ObjectTypeClass; Load additionally reloads the 3D/theater
//   art and swizzles the three cached sub-type pointers.
// ============================================================================

HRESULT BulletTypeClass::GetClassID(CLSID* pClassID) {
    if (!pClassID) return E_POINTER;
    pClassID->Data1 = 0x42544C42;   // 'BTLB' sentinel for BulletTypeClass
    pClassID->Data2 = 0x5454;
    pClassID->Data3 = 0x5045;
    for (int32 i = 0; i < 8; ++i) pClassID->Data4[i] = 0x42;
    return S_OK;
}

HRESULT BulletTypeClass::Load(IStream* pStm) {
    if (!pStm) return E_POINTER;

    // asm: call ObjectTypeClass_Load; jl -> return the failure code.
    HRESULT hr = ObjectTypeClass::Load(pStm);
    if (hr < 0) return hr;

    // asm loc_46C6DF: Load3DArt + Load_Theater_Art.
    Load3DArt(0);
    Load_Theater_Art(0);

    // asm: SwizzleManagerClass_Swizzle over +0x2D8 (Trailer), +0x2B0
    //   (AirburstWeapon) and +0x2B4 (ShrapnelWeapon).  In this port the three
    //   pointers are re-resolved from their INI names during LoadFromINI, so a
    //   stale pointer simply becomes null here.
    if (Trailer != nullptr && Trailer->IsDead()) Trailer = nullptr;
    if (AirburstWeapon != nullptr && AirburstWeapon->IsDead()) AirburstWeapon = nullptr;
    if (ShrapnelWeapon != nullptr && ShrapnelWeapon->IsDead()) ShrapnelWeapon = nullptr;
    return S_OK;
}

HRESULT BulletTypeClass::Save(IStream* pStm, BOOL fClearDirty) {
    if (!pStm) return E_POINTER;
    // asm: BulletTypeClass_Save tail-calls ObjectTypeClass_Save.
    HRESULT hr = ObjectTypeClass::Save(pStm, fClearDirty);
    if (hr < 0) return hr;
    return S_OK;
}

// ============================================================================
// RTTI / size / invalidation
//   asm: BulletTypeClass_GetAbstractDerivationID -> 9 (AbstractType::BulletType)
//        BulletTypeClass_GetSize                   -> 0x2F8
//        BulletTypeClass_PointerGotInvalid         -> retn 8
// ============================================================================

AbstractType BulletTypeClass::WhatAmI() const {
    return AbstractType::BulletType;
}

AbstractType BulletTypeClass::GetAbstractDerivationID() const {
    return AbstractType::BulletType;    // asm: mov eax,9
}

int32 BulletTypeClass::Size() const {
    return 0x2F8;                       // asm: mov eax,2F8h
}

void BulletTypeClass::PointerGotInvalid(AbstractClass* pInvalid, bool removed) {
    // asm: if (arg0 == this->Trailer) this->Trailer = 0; retn 8
    if (Trailer == static_cast<AnimTypeClass*>(pInvalid)) {
        Trailer = nullptr;
    }
    (void)removed;
}

// ============================================================================
// INI loading
//   asm: BulletTypeClass_LoadFromINI (0x46C200..0x46C43x)
//
//   The section is the type's own ID.  The base ObjectTypeClass reader runs
//   first (returning false aborts).  The keys are read in the binary's exact
//   order; the artmd.ini block (Trailer / SpawnDelay / Rotates / Flat) is only
//   touched when the rules-side "Image" key is non-empty.  Finally the theater
//   art is reloaded when !Level, and the 3D art when the image name is set.
//
//   Key order (rules side): Arm, ROT, CourseLockDuration, Elasticity,
//   Acceleration, Color, Arcing, Floater, SubjectToCliffs, SubjectToElevation,
//   SubjectToWalls, VeryHigh, Shadow, Dropping, Level, Inviso, Proximity,
//   Ranged, Inaccurate, FlakScatter, AA, AG, Degenerates, Bouncy, Airburst,
//   Cluster, Scalable, Image, [Image-gated: Trailer, SpawnDelay, Rotates, Flat],
//   AirburstWeapon, ShrapnelWeapon, ShrapnelCount, DetonationAltitude, Vertical,
//   FirersPalette, [art: AnimLow, AnimHigh, AnimRate, AnimPalette].
// ============================================================================

bool BulletTypeClass::LoadFromINI(CCINIClass* pINI) {
    if (!pINI) return false;

    const char* section = this->ID;

    // asm: ObjectTypeClass_LoadFromINI(this, ini); jz fail.
    if (!ObjectTypeClass::LoadFromINI(pINI)) return false;

    CCINIClass* pArt = &CCINIClass::INI_Art;

    // ---- rules-side scalar / flag block (asm loc_46C2xx in order) ----------
    Arm               = pINI->ReadInteger(section, "Arm", Arm);
    ROT               = pINI->ReadInteger(section, "ROT", ROT);
    CourseLockDuration= pINI->ReadInteger(section, "CourseLockDuration", CourseLockDuration);
    Elasticity        = pINI->ReadFixed(section, "Elasticity", Elasticity);
    Acceleration      = pINI->ReadInteger(section, "Acceleration", Acceleration);
    Color             = pINI->GetColorSchemeIdx(section, "Color", Color);   // asm GetColorSchemeIdx
    Arcing            = pINI->ReadBool(section, "Arcing", Arcing);
    Floater           = pINI->ReadBool(section, "Floater", Floater);
    SubjectToCliffs   = pINI->ReadBool(section, "SubjectToCliffs", SubjectToCliffs);
    SubjectToElevation= pINI->ReadBool(section, "SubjectToElevation", SubjectToElevation);
    SubjectToWalls    = pINI->ReadBool(section, "SubjectToWalls", SubjectToWalls);
    VeryHigh          = pINI->ReadBool(section, "VeryHigh", VeryHigh);
    Shadow            = pINI->ReadBool(section, "Shadow", Shadow);
    Dropping          = pINI->ReadBool(section, "Dropping", Dropping);
    Level             = pINI->ReadBool(section, "Level", Level);
    Inviso            = pINI->ReadBool(section, "Inviso", Inviso);
    Proximity         = pINI->ReadBool(section, "Proximity", Proximity);
    Ranged            = pINI->ReadBool(section, "Ranged", Ranged);
    Inaccurate        = pINI->ReadBool(section, "Inaccurate", Inaccurate);
    FlakScatter       = pINI->ReadBool(section, "FlakScatter", FlakScatter);
    AA                = pINI->ReadBool(section, "AA", AA);
    AG                = pINI->ReadBool(section, "AG", AG);
    Degenerates       = pINI->ReadBool(section, "Degenerates", Degenerates);
    Bouncy            = pINI->ReadBool(section, "Bouncy", Bouncy);
    Airburst          = pINI->ReadBool(section, "Airburst", Airburst);
    Cluster           = pINI->ReadInteger(section, "Cluster", Cluster);
    Scalable          = pINI->ReadBool(section, "Scalable", Scalable);

    // asm: INIClass_GetString("Image", this+0x1F8, 0x19).  Only when the name
    //   is present does the artmd.ini block get read.
    {
        char imageBuf[0x20] = { 0 };
        const int32 n = pINI->ReadString(section, "Image", "", imageBuf, sizeof(imageBuf));
        if (n > 0) {
            // store into the inherited ObjectTypeClass::Image (+0x1F8).
            int32 i = 0;
            for (; imageBuf[i] != '\0' && i < static_cast<int32>(sizeof(Image)) - 1; ++i)
                Image[i] = imageBuf[i];
            Image[i] = '\0';

            // asm: the art block (ART_INI) is read only with a non-empty Image.
            char tmp[0x80] = { 0 };
            if (pArt->ReadString(section, "Trailer", "", tmp, sizeof(tmp)) > 0) {
                AnimTypeClass* pAnim = AnimTypeClass::FindOrAllocate(tmp);
                if (pAnim != nullptr) Trailer = pAnim;
            }
            SpawnDelay = pArt->ReadInteger(section, "SpawnDelay", SpawnDelay);
            // asm: Rotates / Flat are stored inverted (fields +0x2A1 / +0x2F7
            //   are the "no rotate" / "flat" flags).
            {
                const bool rotates = pArt->ReadBool(section, "Rotates", !Rotates);
                Rotates = !rotates;
                Flat = pArt->ReadBool(section, "Flat", Flat);
            }
        }
    }

    // ---- weapon references ------------------------------------------------
    {
        char tmp[0x80] = { 0 };
        if (pINI->ReadString(section, "AirburstWeapon", "", tmp, sizeof(tmp)) > 0) {
            WeaponTypeClass* p = WeaponTypeClass::FindOrAllocate(tmp);
            if (p != nullptr) AirburstWeapon = p;
        }
    }
    {
        char tmp[0x80] = { 0 };
        if (pINI->ReadString(section, "ShrapnelWeapon", "", tmp, sizeof(tmp)) > 0) {
            WeaponTypeClass* p = WeaponTypeClass::FindOrAllocate(tmp);
            if (p != nullptr) ShrapnelWeapon = p;
        }
    }
    ShrapnelCount      = pINI->ReadInteger(section, "ShrapnelCount", ShrapnelCount);
    DetonationAltitude = pINI->ReadInteger(section, "DetonationAltitude", DetonationAltitude);
    Vertical           = pINI->ReadBool(section, "Vertical", Vertical);
    FirersPalette      = pINI->ReadBool(section, "FirersPalette", FirersPalette);

    // ---- art-side anim frame range ----------------------------------------
    AnimLow     = pArt->ReadInteger(section, "AnimLow", AnimLow);
    AnimHigh    = pArt->ReadInteger(section, "AnimHigh", AnimHigh);
    AnimRate    = pArt->ReadInteger(section, "AnimRate", AnimRate);
    AnimPalette = pArt->ReadBool(section, "AnimPalette", AnimPalette);

    // asm loc_46C3EA: reload the theater art when the type is not Level, and
    //   the 3D art when the image name is set.
    if (!Level) Load_Theater_Art(0);
    if (Image[0] != '\0') Load3DArt(0);

    return true;
}

// ============================================================================
// INI writing
// ============================================================================

bool BulletTypeClass::SaveToINI(CCINIClass* pINI) {
    if (!pINI) return false;

    char sectionName[64];
    snprintf(sectionName, sizeof(sectionName), "%s", this->ID);

    pINI->WriteString(sectionName, "Name", this->Name);
    pINI->WriteInteger(sectionName, "ROT", ROT);
    pINI->WriteInteger(sectionName, "Arm", Arm);
    pINI->WriteInteger(sectionName, "Speed", ProjectileSpeed);
    pINI->WriteInteger(sectionName, "ScaledSpawnDelay", ScaledSpawnDelay);

    pINI->WriteBool(sectionName, "Arcing", Arcing);
    pINI->WriteBool(sectionName, "Dropping", Dropping);
    pINI->WriteBool(sectionName, "Inviso", Inviso);
    pINI->WriteBool(sectionName, "FlakScatter", FlakScatter);
    pINI->WriteBool(sectionName, "SubjectToCliffs", SubjectToCliffs);
    pINI->WriteBool(sectionName, "SubjectToElevation", SubjectToElevation);
    pINI->WriteBool(sectionName, "SubjectToWalls", SubjectToWalls);
    pINI->WriteBool(sectionName, "VeryHigh", VeryHigh);
    pINI->WriteBool(sectionName, "High", High);
    pINI->WriteBool(sectionName, "Shadow", Shadow);
    pINI->WriteBool(sectionName, "AA", AA);
    pINI->WriteBool(sectionName, "AG", AG);
    pINI->WriteBool(sectionName, "ASW", ASW);
    pINI->WriteBool(sectionName, "AN", AN);
    pINI->WriteBool(sectionName, "Inaccurate", Inaccurate);
    pINI->WriteBool(sectionName, "NoRotate", NoRotate);
    pINI->WriteBool(sectionName, "Level", Level);
    pINI->WriteBool(sectionName, "Proximity", Proximity);
    pINI->WriteBool(sectionName, "Ranged", Ranged);
    pINI->WriteBool(sectionName, "Scalable", Scalable);
    pINI->WriteBool(sectionName, "CourseLocked", CourseLocked);

    return true;
}

// ============================================================================
// CRC computation
//   asm: BulletTypeClass_GetCRC (0x46C5xx) -- base CRC followed by, in order,
//     15 bools (Airburst, Shadow, Arcing, Dropping, Level, Inviso, Proximity,
//     Ranged, Rotates, Inaccurate, FlakScatter, AA, AG, Degenerates, Bouncy),
//     the Elasticity double, the Acceleration / ROT / Arm dwords and the Flat
//     bool.  Every value is fed as its own primitive (CRC_bool / _double /
//     _DWORD in the listing) via CRCEngine::AddData.
// ============================================================================
namespace {
    inline void BulletType_CRC_Bool(CRCEngine& crc, bool value) {
        const uint8 byte = value ? 1 : 0;
        crc.AddData(&byte, sizeof(byte));
    }
    inline void BulletType_CRC_DWord(CRCEngine& crc, int32 value) {
        crc.AddData(&value, sizeof(value));
    }
    inline void BulletType_CRC_Double(CRCEngine& crc, double value) {
        crc.AddData(&value, sizeof(value));
    }
}

void BulletTypeClass::ComputeCRC(CRCEngine& crc) const {
    ObjectTypeClass::ComputeCRC(crc);               // asm: AbstractTypeClass_GetCRC

    // asm: 15 consecutive CRC_bool calls in the fixed order above.
    BulletType_CRC_Bool(crc, Airburst);
    BulletType_CRC_Bool(crc, Shadow);
    BulletType_CRC_Bool(crc, Arcing);
    BulletType_CRC_Bool(crc, Dropping);
    BulletType_CRC_Bool(crc, Level);
    BulletType_CRC_Bool(crc, Inviso);
    BulletType_CRC_Bool(crc, Proximity);
    BulletType_CRC_Bool(crc, Ranged);
    BulletType_CRC_Bool(crc, Rotates);
    BulletType_CRC_Bool(crc, Inaccurate);
    BulletType_CRC_Bool(crc, FlakScatter);
    BulletType_CRC_Bool(crc, AA);
    BulletType_CRC_Bool(crc, AG);
    BulletType_CRC_Bool(crc, Degenerates);
    BulletType_CRC_Bool(crc, Bouncy);

    BulletType_CRC_Double(crc, Elasticity);          // asm: CRC_double(2C8h)
    BulletType_CRC_DWord(crc, Acceleration);         // asm: CRC_DWORD(2D0h)
    BulletType_CRC_DWord(crc, ROT);                  // asm: CRC_DWORD(2DCh)
    BulletType_CRC_DWord(crc, Arm);                  // asm: CRC_DWORD(2F0h)
    BulletType_CRC_Bool(crc, Flat);                  // asm: CRC_bool(2F7h)
}

int32 BulletTypeClass::GetCRC() const {
    CRCEngine crc;
    ComputeCRC(crc);
    return static_cast<int32>(crc.GetCRC());
}

// ============================================================================
// Accessor methods
// ============================================================================

bool BulletTypeClass::Is_Arcing() const {
    return Arcing;
}

// ----------------------------------------------------------------------------
// Is_Homing - asm: BulletClass_IsHoming_obsolete tests type->ROT > 0; a
// projectile is homing (guided) iff it has a non-zero rotation rate.  Arcing
// and Dropping types still rotate their sprite but are not guided, so the
// guided predicate stays exactly `ROT > 0`.
// ----------------------------------------------------------------------------
bool BulletTypeClass::Is_Homing() const {
    return ROT > 0;                                   // asm: setnle al
}

bool BulletTypeClass::Is_AA() const {
    return AA;
}

bool BulletTypeClass::Is_AG() const {
    return AG;
}

bool BulletTypeClass::Is_ASW() const {
    return ASW;
}

bool BulletTypeClass::Is_AN() const {
    return AN;
}

bool BulletTypeClass::Is_Inviso() const {
    return Inviso;
}

bool BulletTypeClass::Is_Dropping() const {
    return Dropping;
}

bool BulletTypeClass::Is_FlakScatter() const {
    return FlakScatter;
}

bool BulletTypeClass::Is_SubjectToCliffs() const {
    return SubjectToCliffs;
}

bool BulletTypeClass::Is_SubjectToElevation() const {
    return SubjectToElevation;
}

bool BulletTypeClass::Is_SubjectToWalls() const {
    return SubjectToWalls;
}

bool BulletTypeClass::Is_Inaccurate() const {
    return Inaccurate;
}

bool BulletTypeClass::Is_Level() const {
    return Level;
}

bool BulletTypeClass::Is_Proximity() const {
    return Proximity;
}

bool BulletTypeClass::Is_CourseLocked() const {
    return CourseLocked;
}

bool BulletTypeClass::Has_Shadow() const {
    return Shadow;
}

int32 BulletTypeClass::Get_Speed() const {
    return ProjectileSpeed;
}

int32 BulletTypeClass::Get_ROT() const {
    return ROT;
}

int32 BulletTypeClass::Get_Arm() const {
    return Arm;
}

// ============================================================================
// asm: BulletType_CreateBullet (0x46B060)
//
//   BulletClass *__fastcall BulletType_CreateBullet(
//       BulletTypeClass *pType, CoordStruct *pPos, HouseClass *pOwner,
//       int32 a4, int32 a8, int32 warhead, int32 speed,
//       bool bright, int32 a7)
//
//   asm 逐段：
//     CoCreateInstance(CLSID_BulletClass, 0, 7, IID_Invalid1, &bullet)
//     test eax,eax / jge loc_ok / xor eax,eax / retn 14h   ; 失败返回 0
//   loc_ok:
//     BulletClass_Initialize(bullet, pType, pPos, pOwner, a4, warhead,
//                            speed, bright, a7)
//     mov eax,esi / retn 14h
//
//   原版走 COM 激活（CLSID_BulletClass）；本项目用 GameCreateArgs 直接堆分配
//   BulletClass 并由其构造函数完成类型挂接——等价于原版激活 + Initialize 的
//   前半段。剩余方向性参数（a4/a8/warhead/speed/bright/a7）在 Initialize 之
//   后由弹道设置器补齐，与原版的调用顺序一致。
// ============================================================================
BulletClass* BulletTypeClass::Create_Bullet(BulletTypeClass* pType,
                                            const CoordStruct* pPos,
                                            HouseClass* pOwner,
                                            int32 a4, int32 a8,
                                            int32 warhead, int32 speed,
                                            bool bright, int32 a7)
{
    // asm: CoCreateInstance(...) / test eax,eax / jge loc_ok
    if (pType == nullptr)
        return nullptr;                                 // asm: xor eax,eax

    BulletClass* pBullet = GameCreateArgs<BulletClass>(pType);
    if (pBullet == nullptr)
        return nullptr;                                 // asm: retn 14h (eax=0)

    // asm: BulletClass_Initialize(bullet, type, pos, owner, damage, WH, speed,
    //      bright).  The project passes warhead/owner through the 7-arg form.
    const CoordStruct pos = (pPos != nullptr) ? *pPos
                                              : CoordStruct(0, 0, 0);
    WarheadTypeClass* pWH = nullptr;
    pBullet->Initialize(pType, pos, reinterpret_cast<TechnoClass*>(pOwner),
                        a4, pWH, speed, bright);

    // asm: position/owner/directional params seeded after Initialize.
    pBullet->Location = pos;                            // asm: mov eax,[pos]
    pBullet->StartCoords = pos;                         // asm: 起点快照
    pBullet->SourceCoords = pos;
    if (speed > 0)
        pBullet->Speed = speed;                         // asm: [esi+Speed]
    pBullet->Strength = a4;                             // asm: 伤害入口
    pBullet->Health = a8;                               // asm: 耐久入口
    pBullet->Bright = bright;                           // asm: bool bright
    pBullet->Timer = a7;                                // asm: a7 计时/引信
    (void)warhead;                                      // 弹头段由 Detonate 解析

    // asm: mov eax,esi
    return pBullet;
}
