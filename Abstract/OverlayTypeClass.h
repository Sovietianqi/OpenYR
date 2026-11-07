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
class OverlayClass;
class SHPStruct;
class HouseClass;
class Surface;
struct Point2D;

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
    static OverlayTypeClass* FindOrAllocate(const char* pID);
    static OverlayTypeClass* FindByIndex(int32 index);
    // asm: OverlayClass_FindByName 语义：按 ID（大小写不敏感）取类型指针。
    static OverlayTypeClass* FindByName(const char* pID);
    static int32 GetCount();
    static void Init_Array();
    static void Delete_Array();
    static void Delete_All();
    // asm: InitList_2151 / InitList_2152 —— vec_OverlayTypes 的静态构造/析构。
    static void sctor();
    static void sdtor();

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

    // asm: OverlayTypeClass_GetAbstractDerivationID (mov eax, 15h ; retn)
    virtual int32 GetAbstractDerivationID() const;
    // asm: OverlayTypeClass_GetArrayIndex (mov eax, [ecx+294h] ; retn)
    virtual int32 GetArrayIndex() const;

    // asm: OverlayTypeClass_LoadImage - defers the SHP load until first use.
    SHPStruct* LoadImage();
    // asm: OverlayTypeClass_LoadImage 的访问面：返回缓存的 SHP 句柄（+0xA4）。
    SHPStruct* GetImageData() const;

    // asm: OverlayTypeClass_Draw(OverlayTypeClass *this, Point2D *coords,
    //   dwLTRB *OtherSurface, int frameIdx)  (asm:0x5FE3C0, retn 0Ch)。
    //   位置 = coords + 视口偏移(rect_mouseclass_vp.T→X / L→Y) + {0x1E, 0x0F}，
    //   再以 Convert_TEMPERAT、0x600 旗标、ZAdjust=0、Brightness=1000 把
    //   GetImage() 的第 frameIdx 帧贴到 TempSurface。
    void Draw(Point2D* pCoords, Surface* pOtherSurface, int32 frameIdx) const;

    // asm: sub_5FE570 (vtable slot before Who_Can_build_Me) - allocates a
    //   bare OverlayClass instance bound to this type.
    OverlayClass* CreateObject();
    // asm: sub_5FE530 - allocates an OverlayClass and registers it.
    bool CreateAndSubmit(HouseClass* pOwner, uint32 startCell);
    // asm: sub_5FE4C0 - returns the shared {0,0,0x7FFF} size hint block.
    void* GetSizeHint();

    // asm: OverlayTypeClass_CreateFromINIList - resolves the SHP base name for
    //   every registered overlay type (theater-specific or plain).
    static void CreateFromINIList();

    // Overlay-specific helpers
    bool       Is_Wall() const;
    bool       Is_Tiberium() const;
    LandType   Get_Land_Type() const;
    ColorStruct Get_Radar_Color() const;

    // asm: OverlayClass_Get_Radar_Color - resolves the minimap colour from
    //   the loaded SHP's per-frame colour table (falls back to zeroes).
    //   `frame` indexes into the SHP frame colour table; writes R,G,B.
    uint8* Get_Radar_Color(uint8* pOut, int32 frame) const;

public:
    // ------------------------------------------------------------------
    // Field layout mirrors the original OverlayTypeClass struct (sizeof 0x2BC):
    //   HeapID, LandType, CellAnim, DamageLevels, Strength, then the flag
    //   bytes Wall/Tiberium/Crate/.../IsARock and the 3-byte RadarColor.
    // ------------------------------------------------------------------
    int32       HeapID;                 // +0x294: array index (GetArrayIndex)
    int32       CellLandType;           // +0x9C: 覆盖物强制的格子地形（tiberium=6）
    LandType    LandValue;              // +0x298: [Land] land type override
    AnimTypeClass* CellAnim;            // +0x29C: [CellAnim] tiberium cell anim
    int32       DamageLevels;           // +0x2A0: [DamageLevels] (art INI)
    int32       Strength;               // +0x2A4: [Strength]
    bool        Wall;                   // +0x2A8: [Wall]
    bool        IsTiberium;             // +0x2A9: [Tiberium]
    bool        Crate;                  // +0x2AA: [Crate]
    bool        CrateTrigger;           // +0x2AB: [CrateTrigger]
    bool        NoUseTileLandType;      // +0x2AC: [NoUseTileLandType]
    bool        IsVeinholeMonster;      // +0x2AD: [IsVeinholeMonster]
    bool        IsVeins;                // +0x2AE: [IsVeins]
    bool        ImageAllocated;         // +0x2AF: SHP base-name flag
    bool        Explodes;               // +0x2B0: [Explodes]
    bool        ChainReaction;          // +0x2B1: [ChainReaction]
    bool        Overrides;              // +0x2B2: [Overrides]
    bool        DrawFlat;               // +0x2B3: [DrawFlat]
    bool        IsRubble;               // +0x2B4: [IsRubble]
    bool        IsARock;                // +0x2B5: [IsARock]
    uint8       RadarColor[3];          // +0x2B6: [RadarColor] R,G,B

    bool        OwnsImage;              // +0xA8:  SHP 句柄是否由本类型拥有（可释放）
};
