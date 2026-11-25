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
class WarheadTypeClass;
class Surface;
struct Point2D;

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

    // asm: InitList_3067 / InitList_3068 —— vec_Terrain 的静态构造/析构
    //   （capacity/increment 置 0x80）。
    static void sctor();
    static void sdtor();

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

    // asm: TerrainClass_GetType (asm:2782) -- "mov eax,[ecx+0C8h] ; retn"
    virtual ObjectTypeClass* GetType() const override;
    // asm: TerrainClass_PointerGotInvalid (asm:2605, retn 8) —— 走 ObjectClass
    //   失效链后，若失效对象是本类的 Type 则清空。
    virtual void PointerGotInvalid(AbstractClass* pInvalid, bool removed) override;

    // asm: TerrainClass_ReceiveDamage (asm:163, retn 1Ch) —— 木质地形受创入口。
    //   仅当弹头 Wood 且类型非 Immune 时才结算；死亡时按 SpawnsTiberium / 点火
    //   分支处理。
    virtual int32 ReceiveDamage(int32* pDamage, int32 typeSource,
                                WarheadTypeClass* pWarhead, TechnoClass* pAttacker,
                                int32 ignoreDefenses, int32 arg10,
                                HouseClass* pSourceHouse);

    // asm: TerrainClass_Draw (asm:1010, retn 8) —— 依 IsAnimated / 已毁标志选取帧与
    //   blitter 旗标（2E00 / 4E00）画出地形 SHP。
    void Draw(Point2D* pCoords, Surface* pSurface) const;

    // asm: TerrainClass_Draw 内 vt+1D0h 的建筑 Z 调整（本类返回 0）。
    int32 Draw_AdjustForBuildingZ() const;
    // asm: TerrainClass_Update 的动画 / 火处理子段。
    void Tmp_AnimTick();
    void Tmp_FireTick();

    // Terrain-specific helpers
    int32  Get_CRC() const;
    void   Draw_It(int32 originX, int32 originY) const;
    bool   Is_Destroyed() const;
    // asm: 类型是否"永不可伤"（TerrainTypeClass.Immune 的薄封装）。
    bool   Is_Veinhole() const;

    // ========================================================================
    // asm: TerrainClass_Burn (0x71C5E0，虚表槽)
    //
    //   char __thiscall TerrainClass_Burn(TerrainClass *this)
    //
    //   点燃木质地形。asm 逐段：
    //     al = [edi+0CDh] / test / jnz lct_Out     ; 已处理过 -> false
    //     al = [edi+0CCh] / test / jnz lct_Out     ; 已在燃烧 -> false
    //     eax = [edi+0C8h]                          ; TerrainTypeClass*
    //     cmp [eax+9Ch], wood / jnz lct_Out         ; 非木质 -> false
    //     cl = [eax+2B1h] / test / jnz lct_Out      ; 类型标志 -> false
    //     eax = Scenario / lea ecx,[eax+218h]
    //     call Random2Class::operator()             ; r
    //     push size AnimClass / call new / test / jz loc_71C699
    //     edx = [edi] / call [edx+48h](this, &buf)  ; 取地形世界坐标
    //     pos = { buf.X, buf.Y, buf.Z + 0x50 }
    //     edx = RulesData / eax = [edx+0B94h] ; TreeFire[]
    //     AnimClass_CTOR(new, TreeFire[r & 1], &pos, 0, 0FFh, 600h, 0)
    //     if (anim): AnimClass::Attach_To(anim, this)
    //                [anim+100h] -= 0x14
    //     [edi+0CCh] = 1 ; mov al,1
    //   lct_Out: xor al,al
    //
    //   字段对应：本类 IsOnFire / FireCount 即 asm 的 +0xCC / +0xCD。
    // ========================================================================
    bool Burn();

    // ------------------------------------------------------------------
    // 字段（asm 偏移见注释；不要求逐字节对应，仅语义等价）。
    // ------------------------------------------------------------------
    TerrainTypeClass* Type;      // +0xC8:  TerrainTypeClass*
    int32 Health;                // ObjectClass::Health 的本地镜像
    int32 Frame;                 // +0xAC:  当前动画帧
    bool IsOnFire;               // +0xCC:  是否正在燃烧
    int32 FireCount;             // +0xCD:  "已处理" / 火计数
    bool  IsAnimActive;          // +0xB0:  本帧动画是否推进
    int32 FrameTimerRate;        // +0xC0:  帧定时器间隔（AnimationRate）
    int32 FrameStep;             // +0xC4:  每拍帧增量（默认 1）
    int32 FieldD0;               // +0xD0:  CRC 参与字段
    int32 FieldD4;               // +0xD4:  CRC 参与字段
    // +0x6C 是 TerrainClass_CTOR 从 type->[0A0h] 拷出的 TreeStrength 缓存；
    // 本工程在 Draw 里直接读 Type->TreeStrength，不再单独占一个槽。
};
