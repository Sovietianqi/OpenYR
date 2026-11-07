#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Containers/DynamicVectorClass.h"
#include "ObjectClass.h"

// ============================================================================
// Forward declarations
// ============================================================================
class OverlayTypeClass;
class HouseClass;
class Surface;
struct Point2D;

// ============================================================================
// OverlayClass - instance of an overlay (walls, bridges, tiberium, etc.)
// Overlays are map decorations that occupy a single cell. They are stored
// directly inside CellClass rather than the global AbstractClass array, but
// still participate in the persistence (Load/Save) and RTTI machinery.
// ============================================================================
class NOVTABLE OverlayClass : public ObjectClass {
public:
    static const AbstractType AbsID = AbstractType::Overlay;

    static DynamicVectorClass<OverlayClass*>* Array;

    OverlayClass(HouseClass* pOwner = nullptr) noexcept;
    explicit OverlayClass(noinit_t) noexcept : ObjectClass(noinit) {}
    virtual ~OverlayClass();

    virtual HRESULT GetClassID(CLSID* pClassID) override;
    virtual HRESULT Load(IStream* pStm) override;
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override;

    virtual AbstractType WhatAmI() const override;
    virtual int32 Size() const override;
    virtual void ComputeCRC(CRCEngine& crc) const override;

    virtual void Update() override;

    // asm: OverlayClass_GetType (0x5FDE90, mov eax,[ecx+0ACh] ; retn)
    virtual ObjectTypeClass* GetType() const override;
    // asm: OverlayClass_GetAbstractDerivationID (0x5FDF0F, mov eax,14h ; retn)
    virtual int32 GetAbstractDerivationID() const;
    // asm: OverlayClass_PointerGotInvalid (0x71D0xx, retn 8 -> 2 args)
    virtual void PointerGotInvalid(AbstractClass* pInvalid, bool removed) override;

    // asm: OverlayClass_Draw (0x5FD980, __thiscall(this, coords, OtherSurface),
    //   retn 8 -> two arguments).  Selects the overlay frame from the growth
    //   stage / wall-damage byte and blits it at the cell screen position.
    void Draw(Point2D* pCoords, Surface* pOtherSurface) const;

    // asm: OverlayClass::Place (0x5FC480, retn 10h) / OverlayClass_Put (0x5FD070).
    //   Place resolves the target cell from the pixel coords, stamps the overlay
    //   index / data into it and runs the placement side effects; Put re-anchors
    //   an already placed overlay after a scroll.
    bool Place(CoordStruct* pCoords, int32 overlayData, int32 a4);
    bool Put(CoordStruct* pCoords, int32 a3);

    // Overlay-specific helpers
    int32  Get_CRC() const;
    void   Destroy();
    bool   Is_Destroyed() const;
    int32  Get_Overlay_Data() const;
    void   Set_Overlay_Data(int32 data);
    bool   Is_Tiberium() const;
    int32  Get_Tiberium_Value() const;

    OverlayTypeClass* Type;
    int32 TiberiumValue;
    int32 OverlayData;
    int32 Health;
};
