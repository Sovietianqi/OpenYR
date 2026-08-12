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
class WarheadTypeClass;

// ============================================================================
// VoxelAnimTypeClass - type definition for voxel animations
// Inherits ObjectTypeClass. Describes a kind of voxel animation (debris,
// meteor, etc.) referencing its VXL/HVA model art and damage characteristics.
// ============================================================================
class NOVTABLE VoxelAnimTypeClass : public ObjectTypeClass {
public:
    static const AbstractType AbsID = AbstractType::VoxelAnimType;

    static DynamicVectorClass<VoxelAnimTypeClass*>* Array;

    static VoxelAnimTypeClass* Find(const char* pID);
    static VoxelAnimTypeClass* FindByIndex(int32 index);
    static int32 GetCount();
    static void Init_Array();
    static void Delete_Array();
    static void Delete_All();

    VoxelAnimTypeClass(const char* pID) noexcept;
    explicit VoxelAnimTypeClass(noinit_t) noexcept : ObjectTypeClass(noinit) {}
    virtual ~VoxelAnimTypeClass();

    virtual HRESULT GetClassID(CLSID* pClassID) override;
    virtual HRESULT Load(IStream* pStm) override;
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override;

    virtual AbstractType WhatAmI() const override;
    virtual int32 Size() const override;

    virtual bool LoadFromINI(CCINIClass* pINI) override;
    virtual void ComputeCRC(CRCEngine& crc) const override;
    int32 GetCRC() const;

    // VoxelAnim-specific helpers
    const char* Get_VXL_Name() const;
    const char* Get_HVA_Name() const;

public:
    char               VoxelName[0x20];
    char               HVAName[0x20];
    double             Damage;
    WarheadTypeClass*  Warhead;
    double             Elasticity;
    double             MaxVelocity;
    double             BounceVelocity;
    int32              DamageRadius;
    bool               IsMeteor;
    bool               IsDebris;
    bool               IsFlat;
    bool               IsAnimated;
    bool               SpawnsAnim;
    int32              SpawnAnimIndex;
    int32              StartAnimIndex;
    bool               WillBounce;
    bool               UseLight;
    int32              LightSize;
    double             LightIntensity;
    int32              Translucency;
    int32              RandomRate;
};
