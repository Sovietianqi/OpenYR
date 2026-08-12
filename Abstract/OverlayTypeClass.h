#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Containers/DynamicVectorClass.h"
#include "ObjectTypeClass.h"

// ============================================================================
// Forward declarations
// ============================================================================
class CCINIClass;
class CRCEngine;

// ============================================================================
// OverlayTypeClass - type definition for overlays
// Inherits ObjectTypeClass. Describes a kind of overlay (wall, bridge section,
// tiberium type, etc.) loaded from rules/art INI files.
// ============================================================================
class NOVTABLE OverlayTypeClass : public ObjectTypeClass {
public:
    static const AbstractType AbsID = AbstractType::OverlayType;

    static DynamicVectorClass<OverlayTypeClass*>* Array;

    static const AbstractType AbstractDerivationID = AbstractType::OverlayType;

    static OverlayTypeClass* Find(const char* pID);
    static OverlayTypeClass* FindByIndex(int32 index);
    static int32 GetCount();
    static void Init_Array();
    static void Delete_Array();
    static void Delete_All();

    OverlayTypeClass(const char* pID) noexcept;
    explicit OverlayTypeClass(noinit_t) noexcept : ObjectTypeClass(noinit) {}
    virtual ~OverlayTypeClass();

    virtual HRESULT GetClassID(CLSID* pClassID) override;
    virtual HRESULT Load(IStream* pStm) override;
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override;

    virtual AbstractType WhatAmI() const override;
    virtual int32 Size() const override;

    virtual bool LoadFromINI(CCINIClass* pINI) override;
    virtual bool SaveToINI(CCINIClass* pINI);
    virtual int32 GetCRC() const;
    virtual void ComputeCRC(CRCEngine& crc) const override;
    virtual const char* get_ID() const;

    // Overlay-specific helpers
    bool       Is_Wall() const;
    bool       Is_Tiberium() const;
    LandType   Get_Land_Type() const;
    ColorStruct Get_Radar_Color() const;

public:
    int32       Damage;
    int32       Tiberium;
    bool        Wall;
    bool        Climbs;
    bool        IsTiberium;
    bool        IsVeins;
    bool        IsBridge;
    bool        IsRamp;
    bool        IsWater;
    bool        IsVisible;
    bool        RadialInventory;
    LandType    LandType_;
    ColorStruct RadarColor;
    int32       RadarBrightness;
    int32       ArmorIndex;
    int32       WallBonusDamage;
    int32       TiberiumGrowthStage;
    int32       TiberiumSpreadRadius;
    int32       TiberiumSpreadProbability;
    int32       DeathAnim;
    int32       ShapeCount;
    char        ArtName[0x20];
};
