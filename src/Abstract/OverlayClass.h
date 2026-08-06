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
