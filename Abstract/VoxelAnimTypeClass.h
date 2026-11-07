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
class AnimTypeClass;
class ParticleSystemTypeClass;
class TechnoTypeClass;

// ============================================================================
// VoxelAnimTypeClass - type definition for voxel animations
//
// asm: VoxelAnimTypeClass_CTOR (0x74AE58) / VoxelAnimTypeClass_GetSize (0x74BA00)
//
//  Inherits ObjectTypeClass (size 0x294).  Describes a kind of voxel animation
//  (falling debris, meteor chunk, etc.).  The class body occupies 0x294..0x307;
//  the derived part starts at the first field below.
//
//  asm 布局（ObjectTypeClass 之后，基址 0x294）：
//    0x294 bool             Normalized
//    0x295 bool             Translucent
//    0x296 bool             ShareSource       (Share*Data 三者合并后的标志)
//    0x298 int32            VoxelIndex
//    0x29C int32            Duration
//    0x2A0 double           Elasticity
//    0x2A8 double           MinAngularVelocity
//    0x2B0 double           MaxAngularVelocity
//    0x2B8 double           MinZVel
//    0x2C0 double           MaxZVel
//    0x2C8 double           MaxXYVel
//    0x2D0 bool             IsMeteor
//    0x2D4 VoxelAnimTypeClass* Spawns
//    0x2D8 int32            SpawnCount
//    0x2DC int32            StartSound
//    0x2E0 int32            StopSound
//    0x2E4 AnimTypeClass*   BounceAnim
//    0x2E8 AnimTypeClass*   ExpireAnim
//    0x2EC AnimTypeClass*   TrailerAnim
//    0x2F0 int32            Damage
//    0x2F4 int32            DamageRadius
//    0x2F8 WarheadTypeClass* Warhead
//    0x2FC ParticleSystemTypeClass* AttachedSystem
//    0x300 bool             IsTiberium
//
//  asm: VoxelAnimTypeClass_GetCRC (0x74B5E0) 覆盖 CRC 的字段集合：
//    294 bool / 295 bool / 296 bool / 298 dword / 29C dword /
//    2A0 2A8 2B0 2B8 2C0 2C8 double ×6 /
//    2D0 bool / 2D8 dword / 2DC dword / 2E0 dword / 2F0 dword /
//    2F4 dword / 300 bool
//  指针字段（2D4/2E4/2E8/2EC/2F8/2FC）由 Swizzle 重绑，不参与 CRC。
// ============================================================================
class NOVTABLE VoxelAnimTypeClass : public ObjectTypeClass {
public:
    static const AbstractType AbsID = AbstractType::VoxelAnimType;

    static DynamicVectorClass<VoxelAnimTypeClass*>* Array;

    static VoxelAnimTypeClass* Find(const char* pID);
    static VoxelAnimTypeClass* FindOrAllocate(const char* pID);
    static VoxelAnimTypeClass* FindByIndex(int32 index);

    // asm: sub_74B642 (0x74B642) -- 按名字返回下标，未命中 -1。
    static int32 FindIndex(const char* pID);
    // asm: sub_74B620 (0x74B620) -- 按下标返回该项 ID 字符串（+0x24），
    //      下标越界返回 "Null"。
    static const char* GetNameByIndex(int32 index);

    static int32 GetCount();
    static void Init_Array();
    static void Delete_Array();
    static void Delete_All();

    // asm: VoxelAnimTypeClass_CTOR (retn 4)
    VoxelAnimTypeClass(const char* pID) noexcept;
    explicit VoxelAnimTypeClass(noinit_t) noexcept : ObjectTypeClass(noinit) {}
    // asm: VoxelAnimTypeClass_SDDTOR (0x74BA80)
    virtual ~VoxelAnimTypeClass();

    // asm: VoxelAnimTypeClass_GetClassID (0x74B6C0, retn 8)
    virtual HRESULT GetClassID(CLSID* pClassID) override;
    // asm: VoxelAnimTypeClass_Load (0x74B790, retn 8)
    virtual HRESULT Load(IStream* pStm) override;
    // asm: VoxelAnimTypeClass_Save (0x74B8D0, retn 0Ch)
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override;

    virtual AbstractType WhatAmI() const override;
    // asm: VoxelAnimTypeClass_GetSize (0x74BA00) -- 0x308
    virtual int32 Size() const override;
    // asm: VoxelAnimTypeClass_GetAbstractDerivationID (0x74B9F0) -- 0x2A (42)
    virtual AbstractType GetAbstractDerivationID() const;

    // asm: VoxelAnimTypeClass_LoadFromINI (0x74B010, retn 4)
    virtual bool LoadFromINI(CCINIClass* pINI) override;
    virtual void ComputeCRC(CRCEngine& crc) const override;
    int32 GetCRC() const;

    // asm: VoxelAnimTypeClass_PointerGotInvalid (0x74B8F0, retn 8)
    virtual void PointerGotInvalid(AbstractClass* pInvalid, bool removed) override;

    // 体素动画专属辅助（asm 直接读 ObjectTypeClass::Image 派生 .VXL 路径）
    const char* Get_VXL_Name() const;
    const char* Get_HVA_Name() const;

    // ------------------------------------------------------------------
    // 字段（布局见类首注释；功能与 asm 一致，字段名不强求逐字节对应）
    // ------------------------------------------------------------------
    bool         Normalized;
    bool         Translucent;
    bool         ShareSource;
    int32        VoxelIndex;
    int32        Duration;
    double       Elasticity;
    double       MinAngularVelocity;
    double       MaxAngularVelocity;
    double       MinZVel;
    double       MaxZVel;
    double       MaxXYVel;
    bool         IsMeteor;
    VoxelAnimTypeClass* Spawns;
    int32        SpawnCount;
    int32        StartSound;
    int32        StopSound;
    AnimTypeClass* BounceAnim;
    AnimTypeClass* ExpireAnim;
    AnimTypeClass* TrailerAnim;
    int32        Damage;
    int32        DamageRadius;
    WarheadTypeClass* Warhead;
    ParticleSystemTypeClass* AttachedSystem;
    bool         IsTiberium;

    // asm: ShareSource 命中时把源的 VXL/HVA 槽（+0xB0..+0xC4 中的一对）
    // 复制到本类。本项目以"记录共享来源类型"等价承载该语义。
    TechnoTypeClass* ShareSourceType;

    // ========================================================================
    // asm: Voxel_ProcessNormalsTables (0x758680)
    //
    //   this = ecx -> 三个浮点缩放因子 {s0@0, s1@4, s2@8}
    //   edx        = 法线表下标
    //
    //   data = NormalTables[idx] ; len = NormalTableLengths[idx]
    //   for i in [0, len):  entry = data + i*3 floats
    //     sum = entry.x*s0 + entry.z*s2 + entry.y*s1     (asm FPU 求和顺序)
    //     if sum < 0.0 -> Voxel_NormalizedNodes[i] = 0
    //     else         -> Voxel_NormalizedNodes[i] =
    //                       (uint8)Float_To_Int_Floor(sum * 16.0)
    //   nodes[0xFD] = nodes[0xFE] = nodes[0xFF] = 0x10
    //
    //   原版直接读引擎 .data 里的三张全局表；这里通过注入接口装载，
    //   量化逻辑与求和顺序保持与 asm 一致（铁律①：只写能确证的部分）。
    // ========================================================================
    static void Set_Normal_Table(int32 idx, const float* pData, int32 length);
    static int32 Get_Normal_Table_Count();
    static uint8 Get_Voxel_Normalized_Node(int32 idx);

    // asm: this=缩放三连 {s0,s1,s2}，tableIdx=法线表下标
    static void Process_Normals_Tables(const float* pScale, int32 tableIdx);

    // ========================================================================
    // asm: Voxel_ProcessNormalsTables_Alt (真实入口 0x758724，retn 8)
    //
    //   紧邻 Process_Normals_Tables（0x758680）的姊妹函数 —— 同一张法线表、
    //   同一个输出缓冲 Voxel_NormalizedNodes，但多了一条**光照/镜面**支路，
    //   参数形态也不同（它是 __thiscall：ecx + edx 两个指针 + 两个栈参）。
    //
    //   ecx = a（源法线 float[3]）; edx = d（增量 float[3]）;
    //   arg_0 = k（偏振系数 float）; arg_4 = tableIdx（int32）
    // ========================================================================
    static void Process_Normals_Tables_Alt(const float* pNormal,
                                            const float* pDelta,
                                            float k, int32 tableIdx);
};
