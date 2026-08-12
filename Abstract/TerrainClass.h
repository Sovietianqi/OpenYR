#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Containers/DynamicVectorClass.h"
#include "ObjectClass.h"

// ============================================================================
// Forward declarations
// ============================================================================
class TerrainTypeClass;
class HouseClass;

// ============================================================================
// TerrainClass - instance of a terrain object (trees, rocks, etc.)
// Terrain objects are blocking map decorations rendered from SHP art. They can
// be destroyed, catch fire, and (for some types) spread tiberium.
// ============================================================================
class NOVTABLE TerrainClass : public ObjectClass {
public:
    static const AbstractType AbsID = AbstractType::Terrain;

    static DynamicVectorClass<TerrainClass*>* Array;

    static void Init_Array();
    static void Delete_Array();
    static void Delete_All();

    TerrainClass(HouseClass* pOwner = nullptr) noexcept;
    explicit TerrainClass(noinit_t) noexcept : ObjectClass(noinit) {}
    virtual ~TerrainClass();

    virtual HRESULT GetClassID(CLSID* pClassID) override;
    virtual HRESULT Load(IStream* pStm) override;
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override;

    virtual AbstractType WhatAmI() const override;
    virtual int32 Size() const override;
    virtual void ComputeCRC(CRCEngine& crc) const override;

    virtual bool IsSightable() const;

    virtual void Update() override;

    // Terrain-specific helpers
    int32  Get_CRC() const;
    void   Draw_It(int32 originX, int32 originY) const;
    bool   Is_Destroyed() const;

    TerrainTypeClass* Type;
    int32 Health;
    int32 Frame;
    bool IsOnFire;
    int32 FireCount;
};
