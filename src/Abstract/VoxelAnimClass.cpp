#include <Abstract/VoxelAnimClass.h>
#include <Abstract/VoxelAnimTypeClass.h>

#include <Core/Memory.h>
#include <Core/Macros.h>
#include <IO/CRC.h>
#include <Map/MapClass.h>
#include <Rules/RulesClass.h>
#include <Scenario/ScenarioClass.h>

#include <cstring>
#include <cstdlib>

// ============================================================================
// VoxelAnimClass.cpp
//
//  VoxelAnimClass is the runtime instance of a voxel-based animation - falling
//  debris, meteor chunks, and other tumbling voxel projectiles that bounce
//  across the map.  It inherits ObjectClass and is updated every frame by the
//  main loop, applying gravity, elasticity and lifetime decay.
//
//  This file implements:
//    * Static Array management (Init_Array / Delete_Array / Delete_All)
//    * Constructor / destructor
//    * Load / Save (stream serialization)
//    * WhatAmI / Size / ComputeCRC / Get_CRC
//    * Update (physics integration, bouncing, lifetime, damage application)
//    * Draw_It (voxel render hook)
//    * Get_Bounce_Physics (whether the anim bounces on impact)
// ============================================================================

// ============================================================================
// Static member definitions
// ============================================================================
DynamicVectorClass<VoxelAnimClass*>* VoxelAnimClass::Array = nullptr;

// ============================================================================
// Static array management
// ============================================================================

void VoxelAnimClass::Init_Array()
{
    if (Array != nullptr)
        return;

    Array = static_cast<DynamicVectorClass<VoxelAnimClass*>*>(
        YRMemory::Allocate(sizeof(DynamicVectorClass<VoxelAnimClass*>)));

    if (Array != nullptr)
    {
        new (Array) DynamicVectorClass<VoxelAnimClass*>();
    }
}

void VoxelAnimClass::Delete_Array()
{
    if (Array == nullptr)
        return;

    Array->~DynamicVectorClass<VoxelAnimClass*>();
    YRMemory::Deallocate(Array);
    Array = nullptr;
}

void VoxelAnimClass::Delete_All()
{
    if (Array == nullptr)
        return;

    for (int32 i = Array->Count - 1; i >= 0; --i)
    {
        VoxelAnimClass* item = Array->Items[i];
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

VoxelAnimClass::VoxelAnimClass(VoxelAnimTypeClass* pType, HouseClass* pOwner) noexcept
    : ObjectClass()
{
    Type       = pType;
    Velocity   = CoordStruct(0, 0, 0);
    Health     = 1;
    Lifetime   = 0;
    VoxelFlags = 0;
    IsActive   = true;
    Owner      = pOwner;
    IsSelected = false;
    IsInLimbo  = false;

    // Initialise the lifetime from the type when available.  The full binary
    // uses a per-type lifetime; the standalone build defaults to a fixed span.
    if (pType != nullptr)
    {
        Lifetime = 600;
    }
}

// ============================================================================
// Destructor
// ============================================================================

VoxelAnimClass::~VoxelAnimClass()
{
    // No heap resources to release at this level.
}

// ============================================================================
// IPersistStream
// ============================================================================

HRESULT VoxelAnimClass::GetClassID(CLSID* pClassID)
{
    if (pClassID == nullptr)
        return E_POINTER;
    pClassID->Data1 = static_cast<uint32>(AbstractType::VoxelAnim);
    return S_OK;
}

HRESULT VoxelAnimClass::Load(IStream* pStm)
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
    Type = typeId[0] ? VoxelAnimTypeClass::Find(typeId) : nullptr;

    hr = pStm->Read(&Velocity, sizeof(Velocity), &read);
    if (hr < 0 || read != sizeof(Velocity)) return E_FAIL;

    hr = pStm->Read(&Health, sizeof(Health), &read);
    if (hr < 0 || read != sizeof(Health)) return E_FAIL;

    hr = pStm->Read(&Lifetime, sizeof(Lifetime), &read);
    if (hr < 0 || read != sizeof(Lifetime)) return E_FAIL;

    hr = pStm->Read(&VoxelFlags, sizeof(VoxelFlags), &read);
    if (hr < 0 || read != sizeof(VoxelFlags)) return E_FAIL;

    // Read bool flags as a bitmask
    uint32 flags = 0;
    hr = pStm->Read(&flags, sizeof(flags), &read);
    if (hr < 0 || read != sizeof(flags)) return E_FAIL;
    IsActive = (flags & 0x0001) != 0;

    return S_OK;
}

HRESULT VoxelAnimClass::Save(IStream* pStm, BOOL fClearDirty)
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

    hr = pStm->Write(&Velocity, sizeof(Velocity), &written);
    if (hr < 0 || written != sizeof(Velocity)) return E_FAIL;

    hr = pStm->Write(&Health, sizeof(Health), &written);
    if (hr < 0 || written != sizeof(Health)) return E_FAIL;

    hr = pStm->Write(&Lifetime, sizeof(Lifetime), &written);
    if (hr < 0 || written != sizeof(Lifetime)) return E_FAIL;

    hr = pStm->Write(&VoxelFlags, sizeof(VoxelFlags), &written);
    if (hr < 0 || written != sizeof(VoxelFlags)) return E_FAIL;

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

AbstractType VoxelAnimClass::WhatAmI() const
{
    return AbstractType::VoxelAnim;
}

int32 VoxelAnimClass::Size() const
{
    return sizeof(VoxelAnimClass);
}

// ============================================================================
// CRC
// ============================================================================

void VoxelAnimClass::ComputeCRC(CRCEngine& crc) const
{
    ObjectClass::ComputeCRC(crc);

    crc.AddData(&Velocity,   sizeof(Velocity));
    crc.AddData(&Health,     sizeof(Health));
    crc.AddData(&Lifetime,   sizeof(Lifetime));
    crc.AddData(&VoxelFlags, sizeof(VoxelFlags));
    crc.AddData(&IsActive,   sizeof(IsActive));
}

int32 VoxelAnimClass::Get_CRC() const
{
    CRCEngine crc;
    ComputeCRC(crc);
    return static_cast<int32>(crc.GetCRC());
}

// ============================================================================
// Get_Bounce_Physics
//
//  Returns whether this voxel anim bounces when it hits the ground.  This is
//  determined by the type's WillBounce flag.
// ============================================================================

bool VoxelAnimClass::Get_Bounce_Physics() const
{
    if (Type == nullptr)
        return false;

    return Type->WillBounce;
}

// ============================================================================
// Update
//
//  Called every frame by the main loop.  Integrates the voxel anim's motion:
//    * Applies gravity to the vertical velocity component.
//    * Moves the anim by its velocity.
//    * Handles ground collision - bouncing (with elasticity) or stopping.
//    * Applies damage to anything within the damage radius on impact.
//    * Decrements the lifetime; the anim is deactivated when it expires.
// ============================================================================

void VoxelAnimClass::Update()
{
    if (!IsActive)
        return;

    if (Type == nullptr)
    {
        IsActive = false;
        return;
    }

    // ------------------------------------------------------------------
    // Lifetime decay.  When the lifetime counter reaches zero the anim is
    // deactivated and removed from the map.
    // ------------------------------------------------------------------
    if (Lifetime > 0)
    {
        --Lifetime;
        if (Lifetime == 0)
        {
            IsActive = false;
            return;
        }
    }

    // ------------------------------------------------------------------
    // Gravity integration.  The vertical (Z) velocity is reduced each tick
    // to simulate gravity.  The full binary uses a per-type gravity constant;
    // the standalone build uses a fixed acceleration.
    // ------------------------------------------------------------------
    const int32 Gravity = 32;
    Velocity.Z -= Gravity;

    // Clamp the vertical velocity to the type's maximum.
    int32 maxVel = 0;
    if (Type->MaxVelocity > 0.0)
    {
        maxVel = static_cast<int32>(Type->MaxVelocity);
    }
    if (maxVel > 0)
    {
        if (Velocity.Z < -maxVel) Velocity.Z = -maxVel;
        if (Velocity.Z >  maxVel) Velocity.Z =  maxVel;
        if (Velocity.X < -maxVel) Velocity.X = -maxVel;
        if (Velocity.X >  maxVel) Velocity.X =  maxVel;
        if (Velocity.Y < -maxVel) Velocity.Y = -maxVel;
        if (Velocity.Y >  maxVel) Velocity.Y =  maxVel;
    }

    // ------------------------------------------------------------------
    // Position integration.  Move the anim by its velocity vector.
    // ------------------------------------------------------------------
    Location.X += Velocity.X;
    Location.Y += Velocity.Y;
    Location.Z += Velocity.Z;

    // ------------------------------------------------------------------
    // Ground collision.  When the anim drops to or below ground level (Z<=0)
    // it either bounces (with elasticity) or comes to rest.
    // ------------------------------------------------------------------
    if (Location.Z <= 0)
    {
        Location.Z = 0;

        if (Type->WillBounce && Type->Elasticity > 0.0)
        {
            // Bounce: reverse and dampen the vertical velocity.
            int32 bounceVel = static_cast<int32>(
                static_cast<double>(-Velocity.Z) * Type->Elasticity);

            // Use the type's bounce velocity threshold to decide whether the
            // anim keeps bouncing or stops.
            int32 bounceThreshold = 0;
            if (Type->BounceVelocity > 0.0)
            {
                bounceThreshold = static_cast<int32>(Type->BounceVelocity);
            }

            if (bounceVel > bounceThreshold)
            {
                Velocity.Z = bounceVel;
                // Friction on the horizontal components.
                Velocity.X = static_cast<int32>(static_cast<double>(Velocity.X) * 0.5);
                Velocity.Y = static_cast<int32>(static_cast<double>(Velocity.Y) * 0.5);
            }
            else
            {
                // Too slow to bounce - come to rest and apply impact damage.
                Velocity = CoordStruct(0, 0, 0);

                // Impact: apply damage within the damage radius.  The full
                // binary calls WarheadTypeClass::Detonate; the standalone
                // build records the impact via the flags.
                if (Type->Damage > 0.0 && Type->DamageRadius > 0)
                {
                    VoxelFlags |= 0x1; // mark impact damage applied
                }

                // A bouncing anim that has stopped is deactivated.
                IsActive = false;
            }
        }
        else
        {
            // Non-bouncing anims stop on impact and apply damage.
            Velocity = CoordStruct(0, 0, 0);

            if (Type->Damage > 0.0 && Type->DamageRadius > 0)
            {
                VoxelFlags |= 0x1; // mark impact damage applied
            }

            IsActive = false;
        }
    }

    // ------------------------------------------------------------------
    // Health decay.  Some voxel anims (e.g. meteors) have hit points that
    // decay over time; when they reach zero the anim is destroyed.
    // ------------------------------------------------------------------
    if (Health > 0)
    {
        // The standalone build does not apply per-tick health decay; the
        // lifetime counter above governs expiration.
    }
    else
    {
        IsActive = false;
    }
}

// ============================================================================
// Draw_It
//
//  Renders the voxel anim at the given screen origin.  The full binary draws
//  the tumbling voxel model with rotation and lighting; the standalone build
//  omits the actual rendering.
// ============================================================================

void VoxelAnimClass::Draw_It(int32 /*originX*/, int32 /*originY*/) const
{
    if (!IsActive)
        return;

    if (Type == nullptr)
        return;

    // The full binary resolves the VXL/HVA model for the type and renders it
    // at the anim's screen coordinate, applying tumbling rotation, lighting
    // (when UseLight is set) and translucency.  Rendering is intentionally
    // omitted from the standalone build.
}
