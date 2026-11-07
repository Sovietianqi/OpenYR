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
    static TerrainTypeClass* FindOrAllocate(const char* pID);
    static TerrainTypeClass* FindByIndex(int32 index);
    // asm: TerrainTypeClass_FindOrAllocate 的 ID 查询面（大小写不敏感）。
    static TerrainTypeClass* FindByName(const char* pID);
    static int32 GetCount();
    static void Init_Array();
    static void Delete_Array();
    static void Delete_All();

    // asm: TerrainTypeClass_CreateFromINIList (asm:218) —— 依剧场索引把地形类型
    //   的 "<ID>.<theater ext>" SHP 解析进 ImageSHP。
    static void CreateFromINIList(int32 idxTheater);

    TerrainTypeClass(const char* pID) noexcept;
    explicit TerrainTypeClass(noinit_t) noexcept : ObjectTypeClass(noinit) {}
    virtual ~TerrainTypeClass();

    virtual HRESULT GetClassID(CLSID* pClassID) override;
    virtual HRESULT Load(IStream* pStm) override;
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override;

    virtual AbstractType WhatAmI() const override;
    virtual int32 Size() const override;
    // asm: 类型派生标识（TerrainClass::ComputeCRC 按类型取 ID 用）。
    virtual int32 GetAbstractDerivationID() const;
    // asm: TerrainTypeClass_GetArrayIndex (asm:847) -- "mov eax,[ecx+294h] ; retn"
    virtual int32 GetArrayIndex() const;

    virtual bool LoadFromINI(CCINIClass* pINI) override;
    // asm: 类型层 INI 读取别名（转发 LoadFromINI）。
    virtual bool Read_INI(CCINIClass* pINI) override;
    virtual void ComputeCRC(CRCEngine& crc) const override;
    int32 GetCRC() const;

    // asm: 类型层 SHP 句柄访问面（对应 ObjectTypeClass::GetImage / +0xA4）。
    SHPStruct* GetImageData() const;

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
    // asm: +0xA0 —— 树木强度提示。构造时置 -1，LoadFromINI 见 -1 时回落到
    //   RulesClass::TreeStrength（原版 +1144h，缺省 0x19）。TerrainClass 在
    //   绘制时用它决定是否取 SHP 的 1 号帧（值 < 2 时）。
    int32  TreeStrength;
    char   ArtName[0x20];
    // ------------------------------------------------------------------
    // Rules / Art INI fields
    // ------------------------------------------------------------------
    bool         IsVeinhole;
    bool         WaterBound;
    Foundation   FoundationValue;
    uint8        RadarColor[3];
    int32        AnimationRate;
    double       AnimationProbability;
    int32        TemperateOccupationBits;
    int32        SnowOccupationBits;
};
