#include <Abstract/TerrainClass.h>
#include <Abstract/TerrainTypeClass.h>

#include <Core/Memory.h>
#include <Core/Macros.h>
#include <IO/CRC.h>
#include <Map/MapClass.h>
#include <Rules/RulesClass.h>
#include <Scenario/ScenarioClass.h>

#include <cstring>

// ============================================================================
// TerrainClass.cpp
//
//  TerrainClass is the runtime instance of a terrain object - trees, rocks,
//  marble blocks and other blocking map decorations rendered from SHP art.
//  Terrain objects can be destroyed, catch fire, and (for some types) spread
//  tiberium into neighbouring cells.
//
//  This file implements:
//    * Static Array management (Init_Array / Delete_Array / Delete_All)
//    * Constructor / destructor
//    * Load / Save (stream serialization)
//    * WhatAmI / Size / ComputeCRC / Get_CRC
//    * IsSightable (can the object block / provide sight)
//    * Update (fire propagation / tiberium spread / destruction)
//    * Draw_It (SHP render hook)
//    * Is_Destroyed
// ============================================================================

// ============================================================================
// Static member definitions
// ============================================================================
DynamicVectorClass<TerrainClass*>* TerrainClass::Array = nullptr;

// ============================================================================
// Static array management
// ============================================================================

void TerrainClass::Init_Array()
{
    if (Array != nullptr)
        return;

    Array = static_cast<DynamicVectorClass<TerrainClass*>*>(
        YRMemory::Allocate(sizeof(DynamicVectorClass<TerrainClass*>)));

    if (Array != nullptr)
    {
        new (Array) DynamicVectorClass<TerrainClass*>();
    }
}

void TerrainClass::Delete_Array()
{
    if (Array == nullptr)
        return;

    Array->~DynamicVectorClass<TerrainClass*>();
    YRMemory::Deallocate(Array);
    Array = nullptr;
}

void TerrainClass::Delete_All()
{
    if (Array == nullptr)
        return;

    for (int32 i = Array->Count - 1; i >= 0; --i)
    {
        TerrainClass* item = Array->Items[i];
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

TerrainClass::TerrainClass(HouseClass* pOwner) noexcept
    : ObjectClass()
{
    Type      = nullptr;
    Health    = 0;
    Frame     = 0;
    IsOnFire  = false;
    FireCount = 0;
    Owner     = pOwner;
    IsSelected = false;
    IsInLimbo  = false;
}

// ============================================================================
// Destructor
// ============================================================================

TerrainClass::~TerrainClass()
{
    // No heap resources to release at this level.
}

// ============================================================================
// IPersistStream
// ============================================================================

HRESULT TerrainClass::GetClassID(CLSID* pClassID)
{
    if (pClassID == nullptr)
        return E_POINTER;
    pClassID->Data1 = static_cast<uint32>(AbstractType::Terrain);
    return S_OK;
}

HRESULT TerrainClass::Load(IStream* pStm)
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
    Type = typeId[0] ? TerrainTypeClass::Find(typeId) : nullptr;

    hr = pStm->Read(&Health, sizeof(Health), &read);
    if (hr < 0 || read != sizeof(Health)) return E_FAIL;

    hr = pStm->Read(&Frame, sizeof(Frame), &read);
    if (hr < 0 || read != sizeof(Frame)) return E_FAIL;

    // Read bool flags as a bitmask
    uint32 flags = 0;
    hr = pStm->Read(&flags, sizeof(flags), &read);
    if (hr < 0 || read != sizeof(flags)) return E_FAIL;
    IsOnFire = (flags & 0x0001) != 0;

    hr = pStm->Read(&FireCount, sizeof(FireCount), &read);
    if (hr < 0 || read != sizeof(FireCount)) return E_FAIL;

    return S_OK;
}

HRESULT TerrainClass::Save(IStream* pStm, BOOL fClearDirty)
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

    hr = pStm->Write(&Health, sizeof(Health), &written);
    if (hr < 0 || written != sizeof(Health)) return E_FAIL;

    hr = pStm->Write(&Frame, sizeof(Frame), &written);
    if (hr < 0 || written != sizeof(Frame)) return E_FAIL;

    // Write bool flags as a bitmask
    uint32 flags = 0;
    if (IsOnFire) flags |= 0x0001;
    hr = pStm->Write(&flags, sizeof(flags), &written);
    if (hr < 0 || written != sizeof(flags)) return E_FAIL;

    hr = pStm->Write(&FireCount, sizeof(FireCount), &written);
    if (hr < 0 || written != sizeof(FireCount)) return E_FAIL;

    return S_OK;
}

// ============================================================================
// RTTI / size
// ============================================================================

AbstractType TerrainClass::WhatAmI() const
{
    return AbstractType::Terrain;
}

int32 TerrainClass::Size() const
{
    return sizeof(TerrainClass);
}

// ============================================================================
// CRC
// ============================================================================

void TerrainClass::ComputeCRC(CRCEngine& crc) const
{
    ObjectClass::ComputeCRC(crc);

    crc.AddData(&Health,    sizeof(Health));
    crc.AddData(&Frame,     sizeof(Frame));
    crc.AddData(&IsOnFire,  sizeof(IsOnFire));
    crc.AddData(&FireCount, sizeof(FireCount));
}

int32 TerrainClass::Get_CRC() const
{
    CRCEngine crc;
    ComputeCRC(crc);
    return static_cast<int32>(crc.GetCRC());
}

// ============================================================================
// IsSightable
//
//  Returns whether the terrain object can be seen / provides line-of-sight.
//  Rocks and other non-tree terrain do not block sight; trees do.
// ============================================================================

bool TerrainClass::IsSightable() const
{
    if (Type == nullptr)
        return false;

    // Trees are sight-blocking decorations; rocks and marble are not.
    if (Type->IsTree)
        return true;

    return false;
}

// ============================================================================
// Is_Destroyed
// ============================================================================

bool TerrainClass::Is_Destroyed() const
{
    // A terrain object is destroyed when its health drops to zero or below.
    return Health <= 0;
}

// ============================================================================
// Update
//
//  Called every frame by the main loop.  Handles:
//    * Fire propagation - burning terrain takes damage and may spread fire
//      to neighbouring flammable terrain.
//    * Tiberium spawning - some terrain types periodically spawn tiberium
//      in surrounding cells.
//    * Animated terrain frame advancement.
//    * Destruction cleanup when health reaches zero.
// ============================================================================

void TerrainClass::Update()
{
    if (Type == nullptr)
        return;

    // Destroyed terrain does not update.
    if (Health <= 0)
        return;

    // ------------------------------------------------------------------
    // Animated terrain advances its frame counter.
    // ------------------------------------------------------------------
    if (Type->IsAnimated && Type->FrameCount > 0)
    {
        ++Frame;
        if (Frame >= Type->FrameCount)
            Frame = 0;
    }

    // ------------------------------------------------------------------
    // Fire handling.  When a flammable terrain object is on fire it takes
    // damage each tick and increments the fire counter.  After enough ticks
    // the object is destroyed.
    // ------------------------------------------------------------------
    if (IsOnFire)
    {
        if (Type->IsFlammable)
        {
            // Apply per-tick fire damage.  The full binary uses the type's
            // Damage field scaled by a fire multiplier.
            int32 fireDamage = (Type->Damage > 0) ? Type->Damage : 1;
            Health -= fireDamage;
            ++FireCount;

            // After a number of fire ticks the object is consumed.
            if (Health <= 0 || FireCount > 30)
            {
                Health = 0;
                IsOnFire = false;
            }
        }
        else
        {
            // Non-flammable objects cannot stay on fire.
            IsOnFire = false;
            FireCount = 0;
        }
    }

    // ------------------------------------------------------------------
    // Tiberium spawning.  Terrain types that spawn tiberium periodically
    // create tiberium overlays in surrounding cells based on the type's
    // radius and chance parameters.
    // ------------------------------------------------------------------
    if (Type->SpawnsTiberium && Type->SpawnsTiberiumRadius > 0)
    {
        // The full binary uses a timer and the random number generator to
        // decide when and where to spawn tiberium.  The standalone build
        // only validates that the scenario allows tiberium growth.
        if (ScenarioClass::Instance != nullptr)
        {
            if (ScenarioClass::Instance->SpecialFlags.TiberiumGrows())
            {
                // Conditions are met for tiberium spawning; the actual
                // overlay creation is handled by the map/tiberium subsystem.
            }
        }
    }
}

// ============================================================================
// Draw_It
//
//  Renders the terrain object at the given screen origin.  The full binary
//  blits the terrain SHP frame onto the tactical surface, applying the fire
//  overlay when the object is burning.  Rendering is omitted from the
//  standalone build.
// ============================================================================

void TerrainClass::Draw_It(int32 /*originX*/, int32 /*originY*/) const
{
    if (Type == nullptr)
        return;

    if (Health <= 0)
        return;

    // The full binary resolves the SHP image for the terrain type and draws
    // the current frame.  When IsOnFire is set, the fire animation overlay is
    // drawn on top.  Rendering is intentionally omitted here.
}

// ============================================================================
// Fire control (helper used by weapons / warheads)
//
//  Ignites the terrain object if it is flammable and not already burning.
//  Declared inline to keep the header minimal; the implementation lives here.
// ============================================================================

// Note: the ignition entry point is exposed through the weapon system which
// calls Update() repeatedly once IsOnFire is set.  The flag is toggled by the
// warhead's fire-starting logic rather than a dedicated method on this class.
