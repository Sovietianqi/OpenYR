#pragma once

#include <Abstract/AbstractTypeClass.h>
#include <Core/Definitions.h>
#include <Core/Macros.h>
#include <Core/Memory.h>
#include <COM/IUnknown.h>

// Forward declaration
class CCINIClass;

// ============================================================================
// HouseTypeClass - Defines a playable country/faction type
// Inherits AbstractTypeClass
// Static array of up to 64 house types
// ============================================================================

class AircraftTypeClass;
class InfantryTypeClass;
class UnitTypeClass;

class NOVTABLE HouseTypeClass : public AbstractTypeClass {
public:
    static const AbstractType AbsID = AbstractType::HouseType;

    // Static array of all house types (max 64)
    static constexpr int32 MaxHouseTypes = 64;
    static HouseTypeClass* Array[MaxHouseTypes];
    static int32 ArrayCount;

    // ========================================================================
    // IPersistStream
    // ========================================================================
    virtual HRESULT __stdcall Load(IStream* pStm) override;
    virtual HRESULT __stdcall Save(IStream* pStm, BOOL fClearDirty) override;

    // ========================================================================
    // Destructor
    // ========================================================================
    virtual ~HouseTypeClass();

    // ========================================================================
    // AbstractTypeClass overrides
    // ========================================================================
    virtual bool LoadFromINI(CCINIClass* pINI) override;
    virtual bool SaveToINI(CCINIClass* pINI) override;
    virtual int32 Size() const override;
    virtual HRESULT __stdcall GetClassID(CLSID* pClassID) override;
    virtual AbstractType WhatAmI() const override;
    virtual void ComputeCRC(CRCEngine& crc) const override;

    AbstractType GetAbstractDerivationID() const;
    bool HasThisID(const char* pID) const;
    int32 GetCRC() const;
    const char* get_ID() const;
    const wchar_t* GetUIName() const;

    // ========================================================================
    // Static factory methods
    // ========================================================================
    static HouseTypeClass* FindOrAllocate(const char* pID);
    static HouseTypeClass* Find(const char* pID);
    static int32 FindIndex(const char* pID);
    static int32 FindIndexOfName(const char* pName);
    static HouseTypeClass* FindByIndex(int32 index);
    static void ClearAll();

    // ========================================================================
    // Helpers
    // ========================================================================
    HouseTypeClass* FindParentCountry() const;
    int32 FindParentCountryIndex() const;

    // ========================================================================
    // Default house types / one-time init
    // ========================================================================
    static int32  Init_Defaults();
    static int32  Get_Default_House_Count();
    static const char* Get_Default_House_ID(int32 index);

    // ========================================================================
    // Color scheme / veteran list parsing
    // ========================================================================
    static int32  Parse_Color_RGB(uint8 r, uint8 g, uint8 b);
    void   Parse_Veteran_List(CCINIClass* pINI, const char* pKey,
                              DynamicVectorClass<UnitTypeClass*>& pOut);
    void   Parse_Veteran_List(CCINIClass* pINI, const char* pKey,
                              DynamicVectorClass<InfantryTypeClass*>& pOut);
    void   Parse_Veteran_List(CCINIClass* pINI, const char* pKey,
                              DynamicVectorClass<AircraftTypeClass*>& pOut);

    // ========================================================================
    // Country / side / tech-level assignment
    // ========================================================================
    bool Assign_Country(const char* pParentID);
    bool Assign_Side(int32 sideIndex);
    static int32 Parse_Tech_Level(int32 level);

    // ========================================================================
    // Full per-instance CRC (multiplayer checksum)
    // ========================================================================
    int32 Compute_CRC_Full() const;

    // ========================================================================
    // Constructor
    // ========================================================================
    HouseTypeClass(const char* pID) noexcept;

protected:
    explicit __forceinline HouseTypeClass(noinit_t) noexcept : AbstractTypeClass(noinit) {}

    // ========================================================================
    // Properties
    // ========================================================================
public:
    char            ParentCountry[25];
    BYTE            align_B1[3];
    int32           ArrayIndex;
    int32           ArrayIndex2;
    int32           SideIndex;
    int32           ColorSchemeIndex;
    DWORD           align_C4;

    // Multiplier arrays (TS leftovers, mostly unused)
    double          FirepowerMult;
    double          GroundspeedMult;
    double          AirspeedMult;
    double          ArmorMult;
    double          ROFMult;
    double          CostMult;
    double          BuildtimeMult;

    // Armor multipliers
    float           ArmorInfantryMult;
    float           ArmorUnitsMult;
    float           ArmorAircraftMult;
    float           ArmorBuildingsMult;
    float           ArmorDefensesMult;

    // Cost multipliers
    float           CostInfantryMult;
    float           CostUnitsMult;
    float           CostAircraftMult;
    float           CostBuildingsMult;
    float           CostDefensesMult;

    // Speed multipliers
    float           SpeedInfantryMult;
    float           SpeedUnitsMult;
    float           SpeedAircraftMult;

    // Build time multipliers
    float           BuildtimeInfantryMult;
    float           BuildtimeUnitsMult;
    float           BuildtimeAircraftMult;
    float           BuildtimeBuildingsMult;
    float           BuildtimeDefensesMult;

    float           IncomeMult;

    // Veteran type lists
    DynamicVectorClass<InfantryTypeClass*>  VeteranInfantry;
    DynamicVectorClass<UnitTypeClass*>      VeteranUnits;
    DynamicVectorClass<AircraftTypeClass*>  VeteranAircraft;

    char            Suffix[4];
    char            Prefix;
    bool            Multiplay;
    bool            MultiplayPassive;
    bool            WallOwner;
    bool            SmartAI;
    BYTE            padding_1A9[7];
};

// ============================================================================
// Static member definitions
// ============================================================================
inline HouseTypeClass* HouseTypeClass::Array[MaxHouseTypes] = {};
inline int32 HouseTypeClass::ArrayCount = 0;

// ============================================================================
// Constructor
// ============================================================================
inline HouseTypeClass::HouseTypeClass(const char* pID) noexcept
    : AbstractTypeClass(noinit)
    , ArrayIndex(-1)
    , ArrayIndex2(-1)
    , SideIndex(-1)
    , ColorSchemeIndex(0)
    , align_C4(0)
    , FirepowerMult(1.0)
    , GroundspeedMult(1.0)
    , AirspeedMult(1.0)
    , ArmorMult(1.0)
    , ROFMult(1.0)
    , CostMult(1.0)
    , BuildtimeMult(1.0)
    , ArmorInfantryMult(1.0f)
    , ArmorUnitsMult(1.0f)
    , ArmorAircraftMult(1.0f)
    , ArmorBuildingsMult(1.0f)
    , ArmorDefensesMult(1.0f)
    , CostInfantryMult(1.0f)
    , CostUnitsMult(1.0f)
    , CostAircraftMult(1.0f)
    , CostBuildingsMult(1.0f)
    , CostDefensesMult(1.0f)
    , SpeedInfantryMult(1.0f)
    , SpeedUnitsMult(1.0f)
    , SpeedAircraftMult(1.0f)
    , BuildtimeInfantryMult(1.0f)
    , BuildtimeUnitsMult(1.0f)
    , BuildtimeAircraftMult(1.0f)
    , BuildtimeBuildingsMult(1.0f)
    , BuildtimeDefensesMult(1.0f)
    , IncomeMult(1.0f)
    , Prefix(0)
    , Multiplay(false)
    , MultiplayPassive(false)
    , WallOwner(false)
    , SmartAI(false)
{
    ParentCountry[0] = '\0';
    Suffix[0] = '\0';
    Suffix[1] = '\0';
    Suffix[2] = '\0';
    Suffix[3] = '\0';
    for (int32 i = 0; i < 7; ++i) padding_1A9[i] = 0;

    // Copy ID
    if (pID) {
        int32 i = 0;
        while (pID[i] && i < 23) {
            ID[i] = pID[i];
            ++i;
        }
        ID[i] = '\0';
    }

    // Add to static array
    if (ArrayCount < MaxHouseTypes) {
        ArrayIndex = ArrayCount;
        ArrayIndex2 = ArrayCount;
        Array[ArrayCount] = this;
        ++ArrayCount;
    }
}

// ============================================================================
// Destructor
// ============================================================================
inline HouseTypeClass::~HouseTypeClass() {
    if (ArrayIndex >= 0 && ArrayIndex < MaxHouseTypes && Array[ArrayIndex] == this) {
        Array[ArrayIndex] = nullptr;
    }
}

// ============================================================================
// GetClassID
// ============================================================================
inline HRESULT __stdcall HouseTypeClass::GetClassID(CLSID* pClassID) {
    if (!pClassID) return -1;
    pClassID->Data1 = 0x1A3B5500;
    pClassID->Data2 = 0x4F2E;
    pClassID->Data3 = 0x11D3;
    pClassID->Data4[0] = 0x8A;
    pClassID->Data4[1] = 0x00;
    pClassID->Data4[2] = 0x00;
    pClassID->Data4[3] = 0x60;
    pClassID->Data4[4] = 0x97;
    pClassID->Data4[5] = 0x5E;
    pClassID->Data4[6] = 0x12;
    pClassID->Data4[7] = 0x34;
    return 0;
}

// ============================================================================
// Load / Save (IStream)
// ============================================================================
inline HRESULT __stdcall HouseTypeClass::Load(IStream* pStm) {
    if (!pStm) return -1;
    return 0;
}

inline HRESULT __stdcall HouseTypeClass::Save(IStream* pStm, BOOL fClearDirty) {
    if (!pStm) return -1;
    if (fClearDirty) Dirty = false;
    return 0;
}

// ============================================================================
// WhatAmI
// ============================================================================
inline AbstractType HouseTypeClass::WhatAmI() const {
    return AbstractType::HouseType;
}

// ============================================================================
// ComputeCRC
// ============================================================================
inline void HouseTypeClass::ComputeCRC(CRCEngine& crc) const {
    crc.AddData(&ArrayIndex, sizeof(ArrayIndex));
    crc.AddData(&SideIndex, sizeof(SideIndex));
    crc.AddData(&ColorSchemeIndex, sizeof(ColorSchemeIndex));
}

// ============================================================================
// AbstractTypeClass overrides
// ============================================================================
// NOTE: LoadFromINI and SaveToINI are implemented in HouseTypeClass.cpp

inline AbstractType HouseTypeClass::GetAbstractDerivationID() const {
    return AbstractType::HouseType;
}

inline bool HouseTypeClass::HasThisID(const char* pID) const {
    if (!pID) return false;
    int32 i = 0;
    while (ID[i] && pID[i] && ID[i] == pID[i]) ++i;
    return ID[i] == '\0' && pID[i] == '\0';
}

inline int32 HouseTypeClass::GetCRC() const {
    int32 crc = 0;
    for (int32 i = 0; ID[i]; ++i) crc += static_cast<uint8>(ID[i]);
    crc += ArrayIndex;
    crc += SideIndex;
    return crc;
}

inline int32 HouseTypeClass::Size() const {
    return sizeof(HouseTypeClass);
}

inline const char* HouseTypeClass::get_ID() const {
    return ID;
}

inline const wchar_t* HouseTypeClass::GetUIName() const {
    return UIName;
}

// ============================================================================
// Static factory methods
// ============================================================================
inline HouseTypeClass* HouseTypeClass::FindOrAllocate(const char* pID) {
    if (!pID) return nullptr;
    for (int32 i = 0; i < ArrayCount; ++i) {
        if (Array[i] && Array[i]->HasThisID(pID))
            return Array[i];
    }
    if (ArrayCount < MaxHouseTypes) {
        return new HouseTypeClass(pID);
    }
    return nullptr;
}

inline HouseTypeClass* HouseTypeClass::Find(const char* pID) {
    if (!pID) return nullptr;
    for (int32 i = 0; i < ArrayCount; ++i) {
        if (Array[i] && Array[i]->HasThisID(pID))
            return Array[i];
    }
    return nullptr;
}

inline int32 HouseTypeClass::FindIndex(const char* pID) {
    if (!pID) return -1;
    for (int32 i = 0; i < ArrayCount; ++i) {
        if (Array[i] && Array[i]->HasThisID(pID))
            return i;
    }
    return -1;
}

inline int32 HouseTypeClass::FindIndexOfName(const char* pName) {
    if (!pName) return -1;
    for (int32 i = 0; i < ArrayCount; ++i) {
        if (Array[i]) {
            int32 j = 0;
            while (Array[i]->Name[j] && pName[j] && Array[i]->Name[j] == pName[j]) ++j;
            if (Array[i]->Name[j] == '\0' && pName[j] == '\0')
                return i;
        }
    }
    return -1;
}

inline HouseTypeClass* HouseTypeClass::FindByIndex(int32 index) {
    if (index < 0 || index >= ArrayCount) return nullptr;
    return Array[index];
}

inline void HouseTypeClass::ClearAll() {
    for (int32 i = 0; i < ArrayCount; ++i) {
        delete Array[i];
        Array[i] = nullptr;
    }
    ArrayCount = 0;
}

// ============================================================================
// Helpers
// ============================================================================
inline HouseTypeClass* HouseTypeClass::FindParentCountry() const {
    return Find(ParentCountry);
}

inline int32 HouseTypeClass::FindParentCountryIndex() const {
    return FindIndexOfName(ParentCountry);
}