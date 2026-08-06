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

// ============================================================================
// TerrainTypeClass - type definition for terrain objects
// Inherits ObjectTypeClass. Describes a kind of terrain decoration (tree,
// rock, marble, etc.) including its destructibility and tiberium-spawning
// behavior, loaded from rules INI files.
// ============================================================================
class NOVTABLE TerrainTypeClass : public ObjectTypeClass {
public:
    static const AbstractType AbsID = AbstractType::TerrainType;

    static DynamicVectorClass<TerrainTypeClass*>* Array;

    static TerrainTypeClass* Find(const char* pID);
    static TerrainTypeClass* FindByIndex(int32 index);
    static int32 GetCount();
    static void Init_Array();
    static void Delete_Array();
    static void Delete_All();

    TerrainTypeClass(const char* pID) noexcept;
    explicit TerrainTypeClass(noinit_t) noexcept : ObjectTypeClass(noinit) {}
    virtual ~TerrainTypeClass();

    virtual HRESULT GetClassID(CLSID* pClassID) override;
    virtual HRESULT Load(IStream* pStm) override;
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override;

    virtual AbstractType WhatAmI() const override;
    virtual int32 Size() const override;

    virtual bool LoadFromINI(CCINIClass* pINI) override;
    virtual void ComputeCRC(CRCEngine& crc) const override;
    int32 GetCRC() const;

    // Terrain-specific helpers
    bool Is_Tree() const;
    bool Is_Rocks() const;
    bool Spawns_Tiberium() const;

public:
    bool   IsMarble;
    bool   IsRocks;
    bool   IsTree;
    bool   IsTiberium;
    bool   SpawnsTiberium;
    bool   IsFlammable;
    bool   IsCrushable;
    bool   IsVein;
    bool   IsFog;
    bool   IsAnimated;
    int32  SpawnsTiberiumType;
    int32  SpawnsTiberiumRadius;
    int32  SpawnsTiberiumChance;
    int32  Damage;
    int32  ArmorIndex;
    int32  FrameCount;
    int32  FireAnim;
    char   ArtName[0x20];
};
