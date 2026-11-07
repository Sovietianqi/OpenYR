#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Containers/DynamicVectorClass.h"
#include "../Containers/VectorClass.h"
#include "../Abstract/AbstractTypeClass.h"

// ============================================================================
// Forward declarations
// ============================================================================
class CCINIClass;
class CRCEngine;
class OverlayTypeClass;
class AnimTypeClass;

// ============================================================================
// TiberiumClass - one entry of the [Tiberiums] list
//
//  Describes a kind of ore: how fast it grows and spreads, what it is worth
//  when harvested, the colour scheme it tints the terrain with, the debris
//  animations it throws, and which overlay image window it draws from.
//
//  Every instance appends itself to the global vec_Tiberiums registry in its
//  constructor and stamps its ordinal into the field at +0x98, which is what
//  CellClass overlay indices are translated back into (see
//  Is_Overlay_Idx_Tiberium).
//
//  Layout follows the binary's TiberiumClass (sizeof 0x128), which is
//  AbstractTypeClass plus:
//
//    +0x098 TypeIndex          ordinal in vec_Tiberiums
//    +0x09C Spread
//    +0x0A0 SpreadPercentage   double
//    +0x0A8 Growth
//    +0x0B0 GrowthPercentage   double
//    +0x0B8 Value
//    +0x0BC Power
//    +0x0C0 Color              colour scheme index
//    +0x0C4 Debris             VectorClass<AnimTypeClass const*>
//    +0x0E0 Image              OverlayTypeClass*
//    +0x0E4 ImageStart
//    +0x0E8 NumImages
//    +0x0EC ImageCount
//    +0x0F0..0x124             the map-generation scratch slots (owned by the
//                              MapSeedClass tiberium distribution pass)
// ============================================================================
class NOVTABLE TiberiumClass : public AbstractTypeClass {
public:
    static const AbstractType AbsID = AbstractType::Tiberium;

    static DynamicVectorClass<TiberiumClass*>* Array;

    static TiberiumClass* Find(const char* pID);
    static TiberiumClass* FindOrAllocate(const char* pID);
    static TiberiumClass* FindByIndex(int32 index);
    static int32 GetCount();
    static void Init_Array();
    static void Delete_Array();
    static void Delete_All();

    // ========================================================================
    // 地图生成期的 tiberium 分布暂存区
    //
    //   asm: TiberiumClass_SmthA (0x722D00) / TiberiumClass_SmthB (0x722240)
    //
    //   两者结构完全一致，只有暂存槽偏移不同：
    //     SmthA: 主 +0x110, 辅1 +0x118, 辅2 +0x114, 旗标 +0x10C, 收尾 sub_7233A0
    //     SmthB: 主 +0x0F4, 辅1 +0x0FC, 辅2 +0x0F8, 旗标 +0x0F0, 收尾 sub_7228B0
    //
    //   每个 tiberium 类型各做一遍：
    //     1. 释放主槽里的计数表（先把表内数据清零，再删表体与数据块）
    //     2. 释放两个辅助缓冲
    //     3. 清旗标
    //     4. 按地图面积重建：辅1 = new(area*8)，辅2 = new(area)
    //     5. 主槽 = new CounterClass，大小取自地图面积，数据块清零
    //     6. 调收尾例程做这一轮的分布计算
    //
    //   MapSeedClass_Generate 在生成种子地图时调用它们。
    // ========================================================================
    static void SmthA();
    static void SmthB();

    // 两者共用的骨架；偏移与收尾例程由参数传入。
    void ResetSeedScratch(int32 offMain, int32 offAux1, int32 offAux2,
                          int32 offFlag, void (*pFinalizer)(TiberiumClass*));

    // ========================================================================
    // 收尾例程（asm sub_7233A0 / sub_7228B0）
    //
    //   在暂存区重建完之后，按地图逐格扫描：凡是「当前格所含 tiberium 是本
    //   类型、且该格可通过」的格子，就把它的坐标与一个计数值成对写进辅1
    //   缓冲，并对主槽计数表叠加计数；随后按 SpreadPercentage / GrowthPercentage
    //   与地图面积算出这一轮的分布。
    //
    //   ⚠ 留白：这两个例程各约 200 行，涉及 MapClass 单元格迭代器
    //   （CellIterator_Reset / NextCell）、sub_483620 的通行判定，以及主槽
    //   计数表在 count 与 area 之间的重新哈希。本批次只落地 SmthA/B 的
    //   「释放 + 重建」骨架，收尾计算尚未迁移——按铁律①，宁可留白不写错。
    //   迁移前这里的调用点为原样保留。
    // ========================================================================
    static void SmthA_Finalizer(TiberiumClass* pThis);
    static void SmthB_Finalizer(TiberiumClass* pThis);

    // ========================================================================
    // asm: Tiberiums_SetupRemap (0x4261D0)
    //
    //   遍历动画表，把每个"带 tiberium 辉光"的动画按所在格的 tiberium 颜色
    //   方案重映射。asm 逐段：
    //     for each anim in vec_Anims:
    //       type = [anim+0xC8] ; if (type == 0 || [type+0x360] == 0) continue
    //       coord = { [anim+0x9C], [anim+0xA0], [anim+0xA4] }
    //       cell  = Map.Get_Target_Cell(&coord)
    //       idx   = CellClass::GetContainedTiberiumIndex(cell)
    //       if (idx == -1) continue
    //       tib   = vec_Tiberiums.data[idx] ; if (!tib) continue
    //       scheme = ColorScheme.data[[tib+0xC0]]
    //       [anim+0xD4] = [scheme+0x30C]
    //
    //   本实现走真实成员（TiberiumClass::Array / ColorScheme::Array），
    //   不再经过运行时注入的全局函数指针。
    // ========================================================================
    static void Setup_Remap();

    // TiberiumClass_CreateFromINIList - walks [Tiberiums], key by key.  Each key
    // names an ordinal and each value an art name; an ordinal that is already
    // populated reuses its type, otherwise a new type is allocated, and every
    // one of them is then asked to LoadFromINI.
    static bool CreateFromINIList(CCINIClass* pINI);

    TiberiumClass(const char* pID) noexcept;
    explicit TiberiumClass(noinit_t) noexcept : AbstractTypeClass(noinit) {}
    virtual ~TiberiumClass();

    virtual HRESULT GetClassID(CLSID* pClassID) override;
    virtual HRESULT Load(IStream* pStm) override;
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override;

    virtual AbstractType WhatAmI() const override;
    virtual int32 Size() const override;
    virtual int32 GetArrayIndex() const override;

    virtual bool LoadFromINI(CCINIClass* pINI) override;
    virtual void ComputeCRC(CRCEngine& crc) const override;

    // TiberiumClass_PointerGotInvalid - drops an animation from the debris list
    // when its type is going away.  Borrowed by the abstract pointer
    // invalidation pass the binary calls PointerGotInvalid.
    virtual void PointerGotInvalid(AbstractClass* pAbstract, bool removed) override;

    int32 Get_Type_Index() const { return TypeIndex; }

    // ========================================================================
    // 蔓延 / 生长辅助（对应 CellClass::Grow_Tiberium 对该类型的用量）
    //
    //   asm: TiberiumClass 的 +0xE4 ImageStart / +0xEC ImageCount 定义矿石
    //   overlay 的绘制窗口；+0x9C Spread / +0xA0 SpreadPercentage 与
    //   +0xA8 Growth / +0xB0 GrowthPercentage 决定蔓延与生长的掷骰概率。
    // ========================================================================
    // 把矿石帧号映射到 OverlayType 索引窗口 [ImageStart, ImageStart+ImageCount)。
    int32 Get_Image(int32 frame) const;
    // 按 SpreadPercentage 掷骰决定该格是否蔓延（cellDensity 为现有矿密度）。
    bool  Should_Spread(int32 cellDensity) const;
    // 按 GrowthPercentage 掷骰决定该格矿量是否生长。
    bool  Should_Grow(int32 cellDensity) const;

public:
    int32           TypeIndex;          // +0x98
    int32           Spread;             // +0x9C
    double          SpreadPercentage;   // +0xA0
    int32           Growth;             // +0xA8
    double          GrowthPercentage;   // +0xB0
    int32           Value;              // +0xB8
    int32           Power;              // +0xBC
    int32           Color;              // +0xC0
    VectorClass<AnimTypeClass const*> Debris;   // +0xC4

    OverlayTypeClass* Image;            // +0xE0
    int32           ImageStart;         // +0xE4
    int32           NumImages;          // +0xE8
    int32           ImageCount;         // +0xEC

    // Map-generation scratch.  The MapSeedClass tiberium distribution pass
    // owns these: two growable buffers sized from the map area plus the
    // per-terrain counters they are paired with.
    int32           field_F0;           // +0xF0
    void*           field_F4;           // +0xF4
    void*           field_F8;           // +0xF8
    void*           field_FC;           // +0xFC
    int32           field_100;          // +0x100
    int32           field_104;          // +0x104
    int32           field_108;          // +0x108
    int32           field_10C;          // +0x10C
    void*           field_110;          // +0x110
    void*           field_114;          // +0x114
    void*           field_118;          // +0x118
    int32           field_11C;          // +0x11C
    int32           field_120;          // +0x120
    int32           field_124;          // +0x124
};

// __IsOverlayIdxTiberium - translates an overlay type index into the ordinal of
// the tiberium type it belongs to, or -1 when the overlay is not ore at all.
int32 Is_Overlay_Idx_Tiberium(int32 overlayIndex);
