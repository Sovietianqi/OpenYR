#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Containers/DynamicVectorClass.h"
#include "ObjectClass.h"

// ============================================================================
// Forward declarations
// ============================================================================
class VoxelAnimTypeClass;
class HouseClass;

// ============================================================================
// VoxelAnimClass - instance of a voxel-based animation
// Used for falling debris, meteor chunks, and other voxel projectiles that
// tumble and bounce across the map. Inherits ObjectClass and is updated every
// frame by the main loop.
// ============================================================================
class NOVTABLE VoxelAnimClass : public ObjectClass {
public:
    static const AbstractType AbsID = AbstractType::VoxelAnim;

    static DynamicVectorClass<VoxelAnimClass*>* Array;

    static void Init_Array();
    static void Delete_Array();
    static void Delete_All();

    VoxelAnimClass(VoxelAnimTypeClass* pType = nullptr,
                   HouseClass* pOwner = nullptr) noexcept;
    explicit VoxelAnimClass(noinit_t) noexcept : ObjectClass(noinit) {}
    virtual ~VoxelAnimClass();

    virtual HRESULT GetClassID(CLSID* pClassID) override;
    virtual HRESULT Load(IStream* pStm) override;
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override;

    virtual AbstractType WhatAmI() const override;
    virtual int32 Size() const override;
    virtual void ComputeCRC(CRCEngine& crc) const override;

    virtual void Update() override;

    // VoxelAnim-specific helpers
    int32  Get_CRC() const;
    void   Draw_It(int32 originX, int32 originY) const;
    bool   Get_Bounce_Physics() const;

    VoxelAnimTypeClass* Type;
    CoordStruct Velocity;
    int32 Health;
    int32 Lifetime;
    uint32 VoxelFlags;
    bool IsActive;
};
