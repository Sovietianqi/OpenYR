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

    // ========================================================================
    // asm: VoxelAnimClass_SCTOR (静态全局数组构造, 被 CRT _atexit 登记)
    //   v_VoxelAnim 的 vftable 置 DynamicVectorClass 表、Capacity=0、
    //   IsAllocated(byte_887394)=1、incrementAmount(dword_88739C)=0Ah(10)、
    //   count=0，并把 SDTOR 登记进 _atexit。等价于 Init_Array 的按需构造，
    //   但这里显式按 asm 常量铺底（初始容量步进 10）。
    // ========================================================================
    static void SCTOR();
    // asm: VoxelAnimClass_SDTOR (静态全局数组析构, _atexit 回调)
    //   把 vftable 还原为 VectorClass 表，若 IsAllocated 置位则 delete 数据区，
    //   清 IsAllocated 与 Capacity。等价于 Delete_Array。
    static void SDTOR();

    VoxelAnimClass(VoxelAnimTypeClass* pType = nullptr,
                   HouseClass* pOwner = nullptr) noexcept;

    // ========================================================================
    // asm: VoxelAnimClass_CTOR (经 VoxelAnim_Spawn 调用；本类构造函数已覆盖
    // 字段初始化部分)。工厂入口负责"分配 + 构造"这一步，与 asm 里
    // `push size CellClass / call new / ... / VoxelAnimClass_CTOR` 的
    // 分配-构造两段序列等价。
    // ========================================================================
    static VoxelAnimClass* Create(VoxelAnimTypeClass* pType,
                                  const CoordStruct& loc);
    explicit VoxelAnimClass(noinit_t) noexcept : ObjectClass(noinit) {}
    virtual ~VoxelAnimClass();

    virtual HRESULT GetClassID(CLSID* pClassID) override;
    virtual HRESULT Load(IStream* pStm) override;
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override;

    virtual AbstractType WhatAmI() const override;
    virtual int32 Size() const override;
    virtual void ComputeCRC(CRCEngine& crc) const override;

    virtual void Update() override;

    // ------------------------------------------------------------------------
    // asm: VoxelAnimClass_Update 主体 —— 碎片物理推进（初速、重力、弹跳、
    //   落地静止、生命周期、派生动画与范围伤害）。Update() 转译本函数。
    //   返回非 0 表示本帧已请求 UnInit（自毁）。
    // ------------------------------------------------------------------------
    int32 AI();

    // VoxelAnim-specific helpers
    int32  Get_CRC() const;
    void   Draw_It(int32 originX, int32 originY) const;
    bool   Get_Bounce_Physics() const;

    // ------------------------------------------------------------------------
    // asm: VoxelAnimClass_Update 落点伤害段（loc_74A884..），由 AI() 调用。
    // ------------------------------------------------------------------------
    void SettleOnImpact();

    VoxelAnimTypeClass* Type;
    CoordStruct Velocity;
    int32 Health;
    int32 Lifetime;
    uint32 VoxelFlags;
    bool IsActive;
};
