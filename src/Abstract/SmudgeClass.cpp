#include <Abstract/SmudgeClass.h>
#include <Abstract/SmudgeTypeClass.h>

#include <Core/Memory.h>
#include <Core/Macros.h>
#include <IO/CRC.h>
#include <Map/MapClass.h>
#include <Rules/RulesClass.h>
#include <Scenario/ScenarioClass.h>

#include <cstring>

// ============================================================================
// SmudgeClass.cpp
//
//  SmudgeClass is the runtime instance of a smudge - scorch marks, craters,
//  bibs and other non-blocking ground decorations left behind by explosions.
//  Smudges are stored per-cell inside CellClass but still participate in the
//  persistence (Load/Save) and RTTI machinery inherited from ObjectClass.
//
//  This file implements:
//    * Static Array management (Init_Array / Delete_Array / Delete_All)
//    * Constructor / destructor
//    * Load / Save (stream serialization)
//    * WhatAmI / Size / ComputeCRC / Get_CRC
//    * Update (animated smudge frame advance / crater chain expansion)
//    * Draw_It (ground decoration render hook)
//    * Is_Visible (line-of-sight / fog visibility test)
// ============================================================================

// ============================================================================
// Static member definitions
// ============================================================================
DynamicVectorClass<SmudgeClass*>* SmudgeClass::Array = nullptr;

// ============================================================================
// Static array management
// ============================================================================

void SmudgeClass::Init_Array()
{
    if (Array != nullptr)
        return;

    Array = static_cast<DynamicVectorClass<SmudgeClass*>*>(
        YRMemory::Allocate(sizeof(DynamicVectorClass<SmudgeClass*>)));

    if (Array != nullptr)
    {
        new (Array) DynamicVectorClass<SmudgeClass*>();
    }
}

void SmudgeClass::Delete_Array()
{
    if (Array == nullptr)
        return;

    Array->~DynamicVectorClass<SmudgeClass*>();
    YRMemory::Deallocate(Array);
    Array = nullptr;
}

void SmudgeClass::Delete_All()
{
    if (Array == nullptr)
        return;

    for (int32 i = Array->Count - 1; i >= 0; --i)
    {
        SmudgeClass* item = Array->Items[i];
        if (item != nullptr)
        {
            GameDelete(item);
        }
        Array->Remove(i);
    }
}

// ============================================================================
// Constructor
// ============================================================================

SmudgeClass::SmudgeClass(HouseClass* pOwner) noexcept
    : ObjectClass()
{
    Type     = nullptr;
    Frame    = 0;
    Health   = 0;
    IsActive = true;
    Owner    = pOwner;
    IsSelected = false;
    IsInLimbo  = false;
}

// ============================================================================
// Destructor
// ============================================================================

SmudgeClass::~SmudgeClass()
{
    // No heap resources to release at this level.
}

// ============================================================================
// IPersistStream
// ============================================================================

HRESULT SmudgeClass::GetClassID(CLSID* pClassID)
{
    if (pClassID == nullptr)
        return E_POINTER;
    pClassID->Data1 = static_cast<uint32>(AbstractType::Smudge);
    return S_OK;
}

HRESULT SmudgeClass::Load(IStream* pStm)
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
    Type = typeId[0] ? SmudgeTypeClass::Find(typeId) : nullptr;

    hr = pStm->Read(&Frame, sizeof(Frame), &read);
    if (hr < 0 || read != sizeof(Frame)) return E_FAIL;

    hr = pStm->Read(&Health, sizeof(Health), &read);
    if (hr < 0 || read != sizeof(Health)) return E_FAIL;

    // Read bool flags as a bitmask
    uint32 flags = 0;
    hr = pStm->Read(&flags, sizeof(flags), &read);
    if (hr < 0 || read != sizeof(flags)) return E_FAIL;
    IsActive = (flags & 0x0001) != 0;

    return S_OK;
}

HRESULT SmudgeClass::Save(IStream* pStm, BOOL fClearDirty)
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

    hr = pStm->Write(&Frame, sizeof(Frame), &written);
    if (hr < 0 || written != sizeof(Frame)) return E_FAIL;

    hr = pStm->Write(&Health, sizeof(Health), &written);
    if (hr < 0 || written != sizeof(Health)) return E_FAIL;

    // Write bool flags as a bitmask
    uint32 flags = 0;
    if (IsActive) flags |= 0x0001;
    hr = pStm->Write(&flags, sizeof(flags), &written);
    if (hr < 0 || written != sizeof(flags)) return E_FAIL;

    return S_OK;
}

// ============================================================================
// RTTI / size
// ============================================================================

AbstractType SmudgeClass::WhatAmI() const
{
    return AbstractType::Smudge;
}

int32 SmudgeClass::Size() const
{
    return sizeof(SmudgeClass);
}

// ============================================================================
// CRC
// ============================================================================

void SmudgeClass::ComputeCRC(CRCEngine& crc) const
{
    ObjectClass::ComputeCRC(crc);

    crc.AddData(&Frame,    sizeof(Frame));
    crc.AddData(&Health,   sizeof(Health));
    crc.AddData(&IsActive, sizeof(IsActive));
}

int32 SmudgeClass::Get_CRC() const
{
    CRCEngine crc;
    ComputeCRC(crc);
    return static_cast<int32>(crc.GetCRC());
}

// ============================================================================
// Update
//
//  Called every frame by the main loop.  For animated smudges this advances
//  the animation frame.  For crater chains this propagates the chain forward.
//  Inactive or destroyed smudges are skipped.
// ============================================================================

void SmudgeClass::Update()
{
    if (!IsActive)
        return;

    if (Type == nullptr)
        return;

    // Destroyed smudges do not animate.
    if (Health <= 0)
        return;

    // ------------------------------------------------------------------
    // Animated smudges advance their frame counter.  The full binary uses a
    // per-type frame delay; the standalone build advances one frame per tick
    // and wraps at the type's frame count.
    // ------------------------------------------------------------------
    if (Type->IsAnimated && Type->Frames > 0)
    {
        ++Frame;
        if (Frame >= Type->Frames)
        {
            // Non-looping animated smudges stop on the last frame; looping
            // ones wrap to zero.  Bibs and scorches loop, craters do not.
            if (Type->IsCrater)
            {
                Frame = Type->Frames - 1;
            }
            else
            {
                Frame = 0;
            }
        }
    }

    // ------------------------------------------------------------------
    // Crater chain expansion.  When a crater smudge is part of a chain, each
    // update advances the chain step until ChainSteps is reached.  The full
    // binary spawns additional crater smudges on neighbouring cells.
    // ------------------------------------------------------------------
    if (Type->IsCrater && Type->ChainCount > 0 && Type->ChainSteps > 0)
    {
        // The standalone build records the chain progress in Health.  Each
        // tick decrements Health; when it reaches zero the chain is done.
        if (Health > 0)
        {
            --Health;
        }
    }
}

// ============================================================================
// Draw_It
//
//  Renders the smudge at the given screen origin.  The full binary blits the
//  smudge SHP frame onto the tactical surface; the standalone build is a
//  no-op because the rendering pipeline is not part of this reconstruction.
// ============================================================================

void SmudgeClass::Draw_It(int32 /*originX*/, int32 /*originY*/) const
{
    if (!IsActive)
        return;

    if (Type == nullptr)
        return;

    // The full binary resolves the SHP image for the smudge type and draws
    // the current frame at the cell's screen coordinate.  Flat smudges are
    // drawn beneath units; non-flat smudges are drawn on the surface layer.
    // Rendering is intentionally omitted from the standalone build.
}

// ============================================================================
// Is_Visible
//
//  Returns whether the smudge is visible to the local player.  Smudges that
//  are inactive, in limbo or hidden by fog/shroud are not visible.
// ============================================================================

bool SmudgeClass::Is_Visible() const
{
    if (!IsActive)
        return false;

    if (IsInLimbo)
        return false;

    if (Type != nullptr && !Type->IsFlat && Type->IsBib)
    {
        // Bibs are only visible while the owning building is on the map.
        // The standalone build treats bibs as always visible.
        return true;
    }

    // The full binary queries the map for fog/shroud state at the smudge's
    // cell.  The standalone build assumes the smudge is visible.
    return true;
}
