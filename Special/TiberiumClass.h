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
    virtual void PointerExpired(AbstractClass* pAbstract, bool removed) override;

    int32 Get_Type_Index() const { return TypeIndex; }

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
