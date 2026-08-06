#include <Abstract/OverlayClass.h>
#include <Abstract/OverlayTypeClass.h>

#include <Core/Memory.h>
#include <Core/Macros.h>
#include <IO/CRC.h>
#include <Map/MapClass.h>
#include <Rules/RulesClass.h>
#include <Scenario/ScenarioClass.h>

#include <cstring>

// ============================================================================
// OverlayClass.cpp
//
//  OverlayClass is the runtime instance of an overlay - walls, bridges,
//  tiberium, etc.  Overlays are map decorations that occupy a single cell.
//  They are stored directly inside CellClass rather than the global
//  AbstractClass array, but still participate in the persistence (Load/Save)
//  and RTTI machinery.
//
//  This file implements:
//    * Constructor / destructor
//    * Load / Save (stream serialization)
//    * WhatAmI / Size / ComputeCRC / Get_CRC
//    * Update (tiberium growth / wall decay)
//    * Destroy / Is_Destroyed
//    * Get_Overlay_Data / Set_Overlay_Data
//    * Is_Tiberium / Get_Tiberium_Value
// ============================================================================

// ============================================================================
// Static member definitions
// ============================================================================
DynamicVectorClass<OverlayClass*>* OverlayClass::Array = nullptr;

// ============================================================================
// Constructor
// ============================================================================

OverlayClass::OverlayClass(HouseClass* pOwner) noexcept
    : ObjectClass()
{
    Type          = nullptr;
    TiberiumValue = 0;
    OverlayData   = 0;
    Health        = 0;
    Owner         = pOwner;
    IsSelected    = false;
    IsInLimbo     = false;
}

// ============================================================================
// Destructor
// ============================================================================

OverlayClass::~OverlayClass()
{
    // No heap resources to release.
}

// ============================================================================
// IPersistStream
// ============================================================================

HRESULT OverlayClass::GetClassID(CLSID* pClassID)
{
    if (pClassID == nullptr)
        return E_POINTER;
    pClassID->Data1 = static_cast<uint32>(AbstractType::Overlay);
    return S_OK;
}

HRESULT OverlayClass::Load(IStream* pStm)
{
    if (pStm == nullptr)
        return E_POINTER;

    HRESULT hr = ObjectClass::Load(pStm);
    if (hr < 0) return hr;

    ULONG read = 0;

    // Read type ID
    char typeId[0x18];
    hr = pStm->Read(typeId, sizeof(typeId), &read);
    if (hr < 0 || read != sizeof(typeId)) return E_FAIL;
    typeId[sizeof(typeId) - 1] = '\0';
    Type = typeId[0] ? OverlayTypeClass::Find(typeId) : nullptr;

    hr = pStm->Read(&TiberiumValue, sizeof(TiberiumValue), &read);
    if (hr < 0 || read != sizeof(TiberiumValue)) return E_FAIL;

    hr = pStm->Read(&OverlayData, sizeof(OverlayData), &read);
    if (hr < 0 || read != sizeof(OverlayData)) return E_FAIL;

    hr = pStm->Read(&Health, sizeof(Health), &read);
    if (hr < 0 || read != sizeof(Health)) return E_FAIL;

    return S_OK;
}

HRESULT OverlayClass::Save(IStream* pStm, BOOL fClearDirty)
{
    if (pStm == nullptr)
        return E_POINTER;

    HRESULT hr = ObjectClass::Save(pStm, fClearDirty);
    if (hr < 0) return hr;

    ULONG written = 0;

    // Write type ID
    char typeId[0x18];
    std::memset(typeId, 0, sizeof(typeId));
    if (Type && Type->ID) {
        int32 j = 0;
        while (Type->ID[j] && j < static_cast<int32>(sizeof(typeId)) - 1) {
            typeId[j] = Type->ID[j]; ++j;
        }
    }
    hr = pStm->Write(typeId, sizeof(typeId), &written);
    if (hr < 0 || written != sizeof(typeId)) return E_FAIL;

    hr = pStm->Write(&TiberiumValue, sizeof(TiberiumValue), &written);
    if (hr < 0 || written != sizeof(TiberiumValue)) return E_FAIL;

    hr = pStm->Write(&OverlayData, sizeof(OverlayData), &written);
    if (hr < 0 || written != sizeof(OverlayData)) return E_FAIL;

    hr = pStm->Write(&Health, sizeof(Health), &written);
    if (hr < 0 || written != sizeof(Health)) return E_FAIL;

    return S_OK;
}

// ============================================================================
// RTTI / size
// ============================================================================

AbstractType OverlayClass::WhatAmI() const
{
    return AbstractType::Overlay;
}

int32 OverlayClass::Size() const
{
    return sizeof(OverlayClass);
}

// ============================================================================
// CRC
// ============================================================================

void OverlayClass::ComputeCRC(CRCEngine& crc) const
{
    ObjectClass::ComputeCRC(crc);

    crc.AddData(&TiberiumValue, sizeof(TiberiumValue));
    crc.AddData(&OverlayData,   sizeof(OverlayData));
    crc.AddData(&Health,        sizeof(Health));
}

int32 OverlayClass::Get_CRC() const
{
    CRCEngine crc;
    ComputeCRC(crc);
    return static_cast<int32>(crc.GetCRC());
}

// ============================================================================
// Update
//
//  Called every frame by the main loop.  For tiberium overlays, this handles
//  growth and spread.  For walls, this handles damage propagation.  For other
//  overlay types, this is a no-op.
// ============================================================================

void OverlayClass::Update()
{
    if (Type == nullptr)
        return;

    // Tiberium growth: when the overlay is a tiberium type and the scenario
    // allows growth, increment the growth stage up to the maximum.
    if (Type->IsTiberium && Health > 0)
    {
        if (ScenarioClass::Instance != nullptr)
        {
            if (ScenarioClass::Instance->SpecialFlags.TiberiumGrows())
            {
                // The growth stage is encoded in OverlayData.  Each stage
                // increases the tiberium value.  The full binary uses a
                // timer to control growth rate.
                int32 maxStage = Type->TiberiumGrowthStage;
                if (maxStage > 0 && OverlayData < maxStage)
                {
                    // Increment the growth stage.  The full binary only does
                    // this on specific frames based on the growth timer.
                    TiberiumValue = OverlayData + 1;
                }
            }
        }
    }

    // Wall decay: damaged walls eventually regenerate in the full binary.
    // The standalone build does not implement wall regeneration.

    // Destroyed overlay cleanup.
    if (Health <= 0 && Type->Wall)
    {
        // Walls with 0 health are removed from the cell.  The full binary
        // calls CellClass::Overlay_Destroyed to handle this.
        OverlayData = -1;
    }
}

// ============================================================================
// Destroy / Is_Destroyed
// ============================================================================

void OverlayClass::Destroy()
{
    // Set health to 0 to mark the overlay as destroyed.  The full binary
    // also plays a death animation and updates the cell.
    Health = 0;
    OverlayData = -1;
    TiberiumValue = 0;
}

bool OverlayClass::Is_Destroyed() const
{
    // An overlay is destroyed when its health is 0 or its data is -1.
    return Health <= 0 || OverlayData < 0;
}

// ============================================================================
// Overlay data accessors
// ============================================================================

int32 OverlayClass::Get_Overlay_Data() const
{
    return OverlayData;
}

void OverlayClass::Set_Overlay_Data(int32 data)
{
    OverlayData = data;

    // If this is a tiberium overlay, update the tiberium value to match
    // the growth stage stored in the data field.
    if (Type != nullptr && Type->IsTiberium)
    {
        TiberiumValue = (data >= 0) ? data + 1 : 0;
    }
}

// ============================================================================
// Tiberium helpers
// ============================================================================

bool OverlayClass::Is_Tiberium() const
{
    if (Type == nullptr)
        return false;
    return Type->IsTiberium;
}

int32 OverlayClass::Get_Tiberium_Value() const
{
    // The tiberium value represents the amount of tiberium in the cell.
    // It is derived from the growth stage (OverlayData) and the tiberium
    // type's base value.  The full binary uses a per-type value table;
    // the standalone build returns the stored TiberiumValue.
    if (!Is_Tiberium())
        return 0;

    if (TiberiumValue > 0)
        return TiberiumValue;

    // Fall back to deriving from the growth stage.
    if (OverlayData >= 0)
        return OverlayData + 1;

    return 0;
}
