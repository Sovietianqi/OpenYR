#include "BulletTypeClass.h"
#include "../INI/INIClass.h"
#include "../IO/CRC.h"

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
// ============================================================================

BulletTypeClass::BulletTypeClass(const char* pID) noexcept
    : ObjectTypeClass(pID),
      ROT(0), Arcing(false), Dropping(false), Inviso(false),
      FlakScatter(false), SubjectToCliffs(false), SubjectToElevation(false),
      SubjectToWalls(false), VeryHigh(false), High(false), Shadow(false),
      AA(false), AG(false), ASW(false), AN(false), Inaccurate(false),
      NoRotate(false), Level(false), Proximity(false), Ranged(false),
      Scalable(false), ScaledSpawnDelay(0), CourseLocked(false), Arm(0),
      ProjectileSpeed(100) {
}

BulletTypeClass::~BulletTypeClass() {
}

// ============================================================================
// IPersistStream
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

    ULONG read = 0;
    HRESULT hr = S_OK;

    // Read ID
    char idBuf[0x18];
    hr = pStm->Read(idBuf, sizeof(idBuf), &read);
    if (hr < 0 || read != sizeof(idBuf)) return E_FAIL;
    std::memcpy(ID, idBuf, sizeof(ID));
    ID[sizeof(ID) - 1] = '\0';

    // Read scalar fields
    hr = pStm->Read(&ROT, sizeof(ROT), &read);
    if (hr < 0 || read != sizeof(ROT)) return E_FAIL;
    hr = pStm->Read(&Arm, sizeof(Arm), &read);
    if (hr < 0 || read != sizeof(Arm)) return E_FAIL;
    hr = pStm->Read(&ScaledSpawnDelay, sizeof(ScaledSpawnDelay), &read);
    if (hr < 0 || read != sizeof(ScaledSpawnDelay)) return E_FAIL;
    hr = pStm->Read(&ProjectileSpeed, sizeof(ProjectileSpeed), &read);
    if (hr < 0 || read != sizeof(ProjectileSpeed)) return E_FAIL;

    // Read boolean flags as a bitmask
    uint32 flags = 0;
    hr = pStm->Read(&flags, sizeof(flags), &read);
    if (hr < 0 || read != sizeof(flags)) return E_FAIL;
    Arcing             = (flags & 0x00000001) != 0;
    Dropping           = (flags & 0x00000002) != 0;
    Inviso             = (flags & 0x00000004) != 0;
    FlakScatter        = (flags & 0x00000008) != 0;
    SubjectToCliffs    = (flags & 0x00000010) != 0;
    SubjectToElevation = (flags & 0x00000020) != 0;
    SubjectToWalls     = (flags & 0x00000040) != 0;
    VeryHigh           = (flags & 0x00000080) != 0;
    High               = (flags & 0x00000100) != 0;
    Shadow             = (flags & 0x00000200) != 0;
    AA                 = (flags & 0x00000400) != 0;
    AG                 = (flags & 0x00000800) != 0;
    ASW                = (flags & 0x00001000) != 0;
    AN                 = (flags & 0x00002000) != 0;
    Inaccurate         = (flags & 0x00004000) != 0;
    NoRotate           = (flags & 0x00008000) != 0;
    Level              = (flags & 0x00010000) != 0;
    Proximity          = (flags & 0x00020000) != 0;
    Ranged             = (flags & 0x00040000) != 0;
    Scalable           = (flags & 0x00080000) != 0;
    CourseLocked       = (flags & 0x00100000) != 0;

    return S_OK;
}

HRESULT BulletTypeClass::Save(IStream* pStm, BOOL fClearDirty) {
    if (!pStm) return E_POINTER;

    ULONG written = 0;
    HRESULT hr = S_OK;

    hr = pStm->Write(ID, sizeof(ID), &written);
    if (hr < 0 || written != sizeof(ID)) return E_FAIL;

    hr = pStm->Write(&ROT, sizeof(ROT), &written);
    if (hr < 0 || written != sizeof(ROT)) return E_FAIL;
    hr = pStm->Write(&Arm, sizeof(Arm), &written);
    if (hr < 0 || written != sizeof(Arm)) return E_FAIL;
    hr = pStm->Write(&ScaledSpawnDelay, sizeof(ScaledSpawnDelay), &written);
    if (hr < 0 || written != sizeof(ScaledSpawnDelay)) return E_FAIL;
    hr = pStm->Write(&ProjectileSpeed, sizeof(ProjectileSpeed), &written);
    if (hr < 0 || written != sizeof(ProjectileSpeed)) return E_FAIL;

    uint32 flags = 0;
    if (Arcing)             flags |= 0x00000001;
    if (Dropping)           flags |= 0x00000002;
    if (Inviso)             flags |= 0x00000004;
    if (FlakScatter)        flags |= 0x00000008;
    if (SubjectToCliffs)    flags |= 0x00000010;
    if (SubjectToElevation) flags |= 0x00000020;
    if (SubjectToWalls)     flags |= 0x00000040;
    if (VeryHigh)           flags |= 0x00000080;
    if (High)               flags |= 0x00000100;
    if (Shadow)             flags |= 0x00000200;
    if (AA)                 flags |= 0x00000400;
    if (AG)                 flags |= 0x00000800;
    if (ASW)                flags |= 0x00001000;
    if (AN)                 flags |= 0x00002000;
    if (Inaccurate)         flags |= 0x00004000;
    if (NoRotate)           flags |= 0x00008000;
    if (Level)              flags |= 0x00010000;
    if (Proximity)          flags |= 0x00020000;
    if (Ranged)             flags |= 0x00040000;
    if (Scalable)           flags |= 0x00080000;
    if (CourseLocked)       flags |= 0x00100000;

    hr = pStm->Write(&flags, sizeof(flags), &written);
    if (hr < 0 || written != sizeof(flags)) return E_FAIL;

    if (fClearDirty) {
        Dirty = false;
    }
    return S_OK;
}

// ============================================================================
// RTTI / size
// ============================================================================

AbstractType BulletTypeClass::WhatAmI() const {
    return AbstractType::BulletType;
}

int32 BulletTypeClass::Size() const {
    return sizeof(BulletTypeClass);
}

// ============================================================================
// INI loading
//
// The original game stores projectile types in [ProjectileTypes] as a list of
// IDs. Each ID has its own section with Yes/No flags and numeric values.
// ============================================================================

bool BulletTypeClass::LoadFromINI(CCINIClass* pINI) {
    if (!pINI) return false;

    char sectionName[64];
    snprintf(sectionName, sizeof(sectionName), "%s", this->ID);
    if (!pINI->SectionExists(sectionName)) return false;

    // Read display name
    char nameBuf[0x31];
    pINI->ReadString(sectionName, "Name", "", nameBuf, sizeof(nameBuf));
    if (nameBuf[0]) {
        int32 i = 0;
        while (nameBuf[i] && i < static_cast<int32>(sizeof(Name)) - 1) {
            Name[i] = nameBuf[i];
            ++i;
        }
        Name[i] = '\0';
    }

    // Read scalar fields
    ROT = pINI->ReadInteger(sectionName, "ROT", ROT);
    Arm = pINI->ReadInteger(sectionName, "Arm", Arm);
    ProjectileSpeed = pINI->ReadInteger(sectionName, "Speed", ProjectileSpeed);
    ScaledSpawnDelay = pINI->ReadInteger(sectionName, "ScaledSpawnDelay", ScaledSpawnDelay);

    // Read boolean flags
    Arcing = pINI->ReadBool(sectionName, "Arcing", Arcing);
    Dropping = pINI->ReadBool(sectionName, "Dropping", Dropping);
    Inviso = pINI->ReadBool(sectionName, "Inviso", Inviso);
    FlakScatter = pINI->ReadBool(sectionName, "FlakScatter", FlakScatter);
    SubjectToCliffs = pINI->ReadBool(sectionName, "SubjectToCliffs", SubjectToCliffs);
    SubjectToElevation = pINI->ReadBool(sectionName, "SubjectToElevation", SubjectToElevation);
    SubjectToWalls = pINI->ReadBool(sectionName, "SubjectToWalls", SubjectToWalls);
    VeryHigh = pINI->ReadBool(sectionName, "VeryHigh", VeryHigh);
    High = pINI->ReadBool(sectionName, "High", High);
    Shadow = pINI->ReadBool(sectionName, "Shadow", Shadow);
    AA = pINI->ReadBool(sectionName, "AA", AA);
    AG = pINI->ReadBool(sectionName, "AG", AG);
    ASW = pINI->ReadBool(sectionName, "ASW", ASW);
    AN = pINI->ReadBool(sectionName, "AN", AN);
    Inaccurate = pINI->ReadBool(sectionName, "Inaccurate", Inaccurate);
    NoRotate = pINI->ReadBool(sectionName, "NoRotate", NoRotate);
    Level = pINI->ReadBool(sectionName, "Level", Level);
    Proximity = pINI->ReadBool(sectionName, "Proximity", Proximity);
    Ranged = pINI->ReadBool(sectionName, "Ranged", Ranged);
    Scalable = pINI->ReadBool(sectionName, "Scalable", Scalable);
    CourseLocked = pINI->ReadBool(sectionName, "CourseLocked", CourseLocked);

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
// ============================================================================

void BulletTypeClass::ComputeCRC(CRCEngine& crc) const {
    crc.AddData(ID, static_cast<int32>(sizeof(ID)));
    crc.AddData(&ROT, sizeof(ROT));
    crc.AddData(&Arm, sizeof(Arm));
    crc.AddData(&ProjectileSpeed, sizeof(ProjectileSpeed));
    crc.AddData(&ScaledSpawnDelay, sizeof(ScaledSpawnDelay));

    uint32 flags = 0;
    if (Arcing)             flags |= 0x00000001;
    if (Dropping)           flags |= 0x00000002;
    if (Inviso)             flags |= 0x00000004;
    if (FlakScatter)        flags |= 0x00000008;
    if (SubjectToCliffs)    flags |= 0x00000010;
    if (SubjectToElevation) flags |= 0x00000020;
    if (SubjectToWalls)     flags |= 0x00000040;
    if (AA)                 flags |= 0x00000400;
    if (AG)                 flags |= 0x00000800;
    if (ASW)                flags |= 0x00001000;
    if (AN)                 flags |= 0x00002000;
    if (Inaccurate)         flags |= 0x00004000;
    if (Level)              flags |= 0x00010000;
    if (Proximity)          flags |= 0x00020000;
    if (CourseLocked)       flags |= 0x00100000;
    crc.AddData(&flags, sizeof(flags));
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
// Is_Homing - a projectile is homing if it has a non-zero ROT and is not
// arcing or dropping (those use ballistic trajectories, not homing).
// ----------------------------------------------------------------------------
bool BulletTypeClass::Is_Homing() const {
    return ROT > 0 && !Arcing && !Dropping;
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
