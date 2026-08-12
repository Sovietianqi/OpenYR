#include "ParticleClass.h"
#include "ParticleSystemClass.h"
#include "../Combat/WarheadTypeClass.h"
#include "../Map/MapClass.h"
#include "../Map/CellClass.h"
#include "../Houses/HouseClass.h"
#include "../Rendering/TacticalClass.h"
#include "../Rendering/DisplayClass.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

// ============================================================================
// Static members
// ============================================================================

DynamicVectorClass<ParticleClass*>* ParticleClass::Array = nullptr;

// ============================================================================
// Construction / Destruction
// ============================================================================

ParticleClass::ParticleClass(
    ParticleTypeClass* pParticleType,
    CoordStruct* pCrd1,
    CoordStruct* pCrd2,
    ParticleSystemClass* pParticleSystem) noexcept :
    ObjectClass(),
    Type(pParticleType),
    unknown_B0(0),
    unknown_B1(0),
    unknown_B2(0),
    unknown_B4(0),
    unknown_B8(0),
    unknown_BC(0),
    Velocity(0, 0, 0),
    unknown_CC(0),
    unknown_double_D0(0.0),
    unknown_D8(0),
    unknown_DC(0),
    unknown_E0(0),
    Speed(0.0f),
    DestCoords(0, 0, 0),
    SourceCoords(0, 0, 0),
    unknown_coords_100(0, 0, 0),
    unknown_vector3d_10C_x(0.0f),
    unknown_vector3d_10C_y(0.0f),
    unknown_vector3d_10C_z(0.0f),
    unknown_vector3d_118_x(0.0f),
    unknown_vector3d_118_y(0.0f),
    unknown_vector3d_118_z(0.0f),
    ParticleSystem(pParticleSystem),
    RemainingEC(0),
    RemainingDC(0),
    StateAIAdvance(0),
    unknown_12D(0),
    StartStateAI(0),
    Translucency(0),
    unknown_130(0),
    unknown_131(0),
    unused_134(0),
    Age(0),
    MaxAge(30),
    Acceleration(0, 0, 0),
    Color(255, 255, 255),
    Alpha(255),
    Size_(1.0f),
    Dead(false),
    FadingOut(false),
    ParticleType_(0)
{
    if (pCrd1) {
        SourceCoords = *pCrd1;
        Location = *pCrd1;
    }
    if (pCrd2) {
        DestCoords = *pCrd2;
    }

    if (Type) {
        MaxAge = Type->MaxLifetime;
        Speed = static_cast<int32>(std::sqrt(
            static_cast<float>(Type->InitialVelocity.X * Type->InitialVelocity.X +
                               Type->InitialVelocity.Y * Type->InitialVelocity.Y +
                               Type->InitialVelocity.Z * Type->InitialVelocity.Z)));
        Color = Type->InitialColor;
        Translucency = static_cast<BYTE>(128);
        Alpha = 127;

        // Set initial velocity based on type
        Velocity.X = Type->InitialVelocity.X;
        Velocity.Y = Type->InitialVelocity.Y;
        Velocity.Z = Type->InitialVelocity.Z + (Type->VelocityVariation.Z > 0 ? std::rand() % static_cast<int32>(Type->VelocityVariation.Z) : 0);
    }

    if (!Array) {
        Array = new DynamicVectorClass<ParticleClass*>();
    }
    Array->Add(this);
}

ParticleClass::~ParticleClass() {
    if (Array) {
        for (int32 i = 0; i < Array->Count; ++i) {
            if ((*Array)[i] == this) {
                Array->Remove(i);
                break;
            }
        }
    }
}

// ============================================================================
// IPersist
// ============================================================================

HRESULT ParticleClass::GetClassID(CLSID* pClassID) {
    if (!pClassID) return E_POINTER;
    pClassID->Data1 = static_cast<uint32>(AbstractType::Particle);
    return S_OK;
}

HRESULT ParticleClass::Load(IStream* pStm) {
    if (!pStm) return E_POINTER;

    ULONG read = 0;
    HRESULT hr = S_OK;

    // --- ObjectClass members (parent Load is a stub, serialize directly) ---
    hr = pStm->Read(&Location, sizeof(Location), &read);
    if (hr < 0 || read != sizeof(Location)) return E_FAIL;

    int32 ownerIdx = -1;
    hr = pStm->Read(&ownerIdx, sizeof(ownerIdx), &read);
    if (hr < 0 || read != sizeof(ownerIdx)) return E_FAIL;
    Owner = (ownerIdx >= 0 && ownerIdx < HouseClass::ArrayCount) ? HouseClass::Array[ownerIdx] : nullptr;

    uint32 objFlags = 0;
    hr = pStm->Read(&objFlags, sizeof(objFlags), &read);
    if (hr < 0 || read != sizeof(objFlags)) return E_FAIL;
    IsSelected = (objFlags & 0x01) != 0;
    IsInLimbo  = (objFlags & 0x02) != 0;

    // --- ParticleClass members ---
    // Read Type (string ID)
    char typeName[0x18];
    hr = pStm->Read(typeName, sizeof(typeName), &read);
    if (hr < 0 || read != sizeof(typeName)) return E_FAIL;
    typeName[sizeof(typeName) - 1] = '\0';
    Type = nullptr;
    if (typeName[0] && ParticleTypeClass::Array) {
        for (int32 i = 0; i < ParticleTypeClass::Array->Count; ++i) {
            ParticleTypeClass* pPT = ParticleTypeClass::Array->GetItem(i);
            if (pPT && pPT->GetName() && !_strcmpi(pPT->GetName(), typeName)) {
                Type = pPT;
                break;
            }
        }
    }

    // Read packed BYTEs (unknown_B0, unknown_B1, unknown_B2)
    uint32 packB = 0;
    hr = pStm->Read(&packB, sizeof(packB), &read);
    if (hr < 0 || read != sizeof(packB)) return E_FAIL;
    unknown_B0 = static_cast<BYTE>(packB & 0xFF);
    unknown_B1 = static_cast<BYTE>((packB >> 8) & 0xFF);
    unknown_B2 = static_cast<BYTE>((packB >> 16) & 0xFF);

    hr = pStm->Read(&unknown_B4, sizeof(unknown_B4), &read);
    if (hr < 0 || read != sizeof(unknown_B4)) return E_FAIL;

    hr = pStm->Read(&unknown_B8, sizeof(unknown_B8), &read);
    if (hr < 0 || read != sizeof(unknown_B8)) return E_FAIL;

    hr = pStm->Read(&unknown_BC, sizeof(unknown_BC), &read);
    if (hr < 0 || read != sizeof(unknown_BC)) return E_FAIL;

    hr = pStm->Read(&Velocity, sizeof(Velocity), &read);
    if (hr < 0 || read != sizeof(Velocity)) return E_FAIL;

    hr = pStm->Read(&unknown_CC, sizeof(unknown_CC), &read);
    if (hr < 0 || read != sizeof(unknown_CC)) return E_FAIL;

    hr = pStm->Read(&unknown_double_D0, sizeof(unknown_double_D0), &read);
    if (hr < 0 || read != sizeof(unknown_double_D0)) return E_FAIL;

    hr = pStm->Read(&unknown_D8, sizeof(unknown_D8), &read);
    if (hr < 0 || read != sizeof(unknown_D8)) return E_FAIL;

    hr = pStm->Read(&unknown_DC, sizeof(unknown_DC), &read);
    if (hr < 0 || read != sizeof(unknown_DC)) return E_FAIL;

    hr = pStm->Read(&unknown_E0, sizeof(unknown_E0), &read);
    if (hr < 0 || read != sizeof(unknown_E0)) return E_FAIL;

    hr = pStm->Read(&Speed, sizeof(Speed), &read);
    if (hr < 0 || read != sizeof(Speed)) return E_FAIL;

    hr = pStm->Read(&DestCoords, sizeof(DestCoords), &read);
    if (hr < 0 || read != sizeof(DestCoords)) return E_FAIL;

    hr = pStm->Read(&SourceCoords, sizeof(SourceCoords), &read);
    if (hr < 0 || read != sizeof(SourceCoords)) return E_FAIL;

    hr = pStm->Read(&unknown_coords_100, sizeof(unknown_coords_100), &read);
    if (hr < 0 || read != sizeof(unknown_coords_100)) return E_FAIL;

    hr = pStm->Read(&unknown_vector3d_10C_x, sizeof(unknown_vector3d_10C_x), &read);
    if (hr < 0 || read != sizeof(unknown_vector3d_10C_x)) return E_FAIL;

    hr = pStm->Read(&unknown_vector3d_10C_y, sizeof(unknown_vector3d_10C_y), &read);
    if (hr < 0 || read != sizeof(unknown_vector3d_10C_y)) return E_FAIL;

    hr = pStm->Read(&unknown_vector3d_10C_z, sizeof(unknown_vector3d_10C_z), &read);
    if (hr < 0 || read != sizeof(unknown_vector3d_10C_z)) return E_FAIL;

    hr = pStm->Read(&unknown_vector3d_118_x, sizeof(unknown_vector3d_118_x), &read);
    if (hr < 0 || read != sizeof(unknown_vector3d_118_x)) return E_FAIL;

    hr = pStm->Read(&unknown_vector3d_118_y, sizeof(unknown_vector3d_118_y), &read);
    if (hr < 0 || read != sizeof(unknown_vector3d_118_y)) return E_FAIL;

    hr = pStm->Read(&unknown_vector3d_118_z, sizeof(unknown_vector3d_118_z), &read);
    if (hr < 0 || read != sizeof(unknown_vector3d_118_z)) return E_FAIL;

    // Read ParticleSystem (int32 index)
    int32 psIdx = -1;
    hr = pStm->Read(&psIdx, sizeof(psIdx), &read);
    if (hr < 0 || read != sizeof(psIdx)) return E_FAIL;
    ParticleSystem = nullptr;
    if (psIdx >= 0 && ParticleSystemClass::Array && psIdx < ParticleSystemClass::Array->Count) {
        ParticleSystem = (*ParticleSystemClass::Array)[psIdx];
    }

    hr = pStm->Read(&RemainingEC, sizeof(RemainingEC), &read);
    if (hr < 0 || read != sizeof(RemainingEC)) return E_FAIL;

    hr = pStm->Read(&RemainingDC, sizeof(RemainingDC), &read);
    if (hr < 0 || read != sizeof(RemainingDC)) return E_FAIL;

    // Read packed BYTEs (StateAIAdvance, unknown_12D, StartStateAI, Translucency)
    uint32 packS = 0;
    hr = pStm->Read(&packS, sizeof(packS), &read);
    if (hr < 0 || read != sizeof(packS)) return E_FAIL;
    StateAIAdvance = static_cast<BYTE>(packS & 0xFF);
    unknown_12D    = static_cast<BYTE>((packS >> 8) & 0xFF);
    StartStateAI   = static_cast<BYTE>((packS >> 16) & 0xFF);
    Translucency   = static_cast<BYTE>((packS >> 24) & 0xFF);

    // Read packed BYTEs (unknown_130, unknown_131)
    uint16 packU = 0;
    hr = pStm->Read(&packU, sizeof(packU), &read);
    if (hr < 0 || read != sizeof(packU)) return E_FAIL;
    unknown_130 = static_cast<BYTE>(packU & 0xFF);
    unknown_131 = static_cast<BYTE>((packU >> 8) & 0xFF);

    hr = pStm->Read(&unused_134, sizeof(unused_134), &read);
    if (hr < 0 || read != sizeof(unused_134)) return E_FAIL;

    hr = pStm->Read(&Age, sizeof(Age), &read);
    if (hr < 0 || read != sizeof(Age)) return E_FAIL;

    hr = pStm->Read(&MaxAge, sizeof(MaxAge), &read);
    if (hr < 0 || read != sizeof(MaxAge)) return E_FAIL;

    hr = pStm->Read(&Acceleration, sizeof(Acceleration), &read);
    if (hr < 0 || read != sizeof(Acceleration)) return E_FAIL;

    hr = pStm->Read(&Color, sizeof(Color), &read);
    if (hr < 0 || read != sizeof(Color)) return E_FAIL;

    hr = pStm->Read(&Alpha, sizeof(Alpha), &read);
    if (hr < 0 || read != sizeof(Alpha)) return E_FAIL;

    hr = pStm->Read(&Size_, sizeof(Size_), &read);
    if (hr < 0 || read != sizeof(Size_)) return E_FAIL;

    // Read bool flags as a bitmask
    uint32 flags = 0;
    hr = pStm->Read(&flags, sizeof(flags), &read);
    if (hr < 0 || read != sizeof(flags)) return E_FAIL;
    Dead      = (flags & 0x01) != 0;
    FadingOut = (flags & 0x02) != 0;

    hr = pStm->Read(&ParticleType_, sizeof(ParticleType_), &read);
    if (hr < 0 || read != sizeof(ParticleType_)) return E_FAIL;

    return S_OK;
}

HRESULT ParticleClass::Save(IStream* pStm, BOOL fClearDirty) {
    if (!pStm) return E_POINTER;

    ULONG written = 0;
    HRESULT hr = S_OK;

    // --- ObjectClass members (parent Save is a stub, serialize directly) ---
    hr = pStm->Write(&Location, sizeof(Location), &written);
    if (hr < 0 || written != sizeof(Location)) return E_FAIL;

    int32 ownerIdx = -1;
    if (Owner) {
        for (int32 i = 0; i < HouseClass::ArrayCount; ++i) {
            if (HouseClass::Array[i] == Owner) { ownerIdx = i; break; }
        }
    }
    hr = pStm->Write(&ownerIdx, sizeof(ownerIdx), &written);
    if (hr < 0 || written != sizeof(ownerIdx)) return E_FAIL;

    uint32 objFlags = 0;
    if (IsSelected) objFlags |= 0x01;
    if (IsInLimbo)  objFlags |= 0x02;
    hr = pStm->Write(&objFlags, sizeof(objFlags), &written);
    if (hr < 0 || written != sizeof(objFlags)) return E_FAIL;

    // --- ParticleClass members ---
    // Write Type (string ID)
    char typeName[0x18];
    std::memset(typeName, 0, sizeof(typeName));
    if (Type && Type->GetName()) {
        const char* pName = Type->GetName();
        int32 j = 0;
        while (pName[j] && j < static_cast<int32>(sizeof(typeName)) - 1) {
            typeName[j] = pName[j]; ++j;
        }
    }
    hr = pStm->Write(typeName, sizeof(typeName), &written);
    if (hr < 0 || written != sizeof(typeName)) return E_FAIL;

    // Write packed BYTEs (unknown_B0, unknown_B1, unknown_B2)
    uint32 packB = static_cast<uint32>(unknown_B0)
                 | (static_cast<uint32>(unknown_B1) << 8)
                 | (static_cast<uint32>(unknown_B2) << 16);
    hr = pStm->Write(&packB, sizeof(packB), &written);
    if (hr < 0 || written != sizeof(packB)) return E_FAIL;

    hr = pStm->Write(&unknown_B4, sizeof(unknown_B4), &written);
    if (hr < 0 || written != sizeof(unknown_B4)) return E_FAIL;

    hr = pStm->Write(&unknown_B8, sizeof(unknown_B8), &written);
    if (hr < 0 || written != sizeof(unknown_B8)) return E_FAIL;

    hr = pStm->Write(&unknown_BC, sizeof(unknown_BC), &written);
    if (hr < 0 || written != sizeof(unknown_BC)) return E_FAIL;

    hr = pStm->Write(&Velocity, sizeof(Velocity), &written);
    if (hr < 0 || written != sizeof(Velocity)) return E_FAIL;

    hr = pStm->Write(&unknown_CC, sizeof(unknown_CC), &written);
    if (hr < 0 || written != sizeof(unknown_CC)) return E_FAIL;

    hr = pStm->Write(&unknown_double_D0, sizeof(unknown_double_D0), &written);
    if (hr < 0 || written != sizeof(unknown_double_D0)) return E_FAIL;

    hr = pStm->Write(&unknown_D8, sizeof(unknown_D8), &written);
    if (hr < 0 || written != sizeof(unknown_D8)) return E_FAIL;

    hr = pStm->Write(&unknown_DC, sizeof(unknown_DC), &written);
    if (hr < 0 || written != sizeof(unknown_DC)) return E_FAIL;

    hr = pStm->Write(&unknown_E0, sizeof(unknown_E0), &written);
    if (hr < 0 || written != sizeof(unknown_E0)) return E_FAIL;

    hr = pStm->Write(&Speed, sizeof(Speed), &written);
    if (hr < 0 || written != sizeof(Speed)) return E_FAIL;

    hr = pStm->Write(&DestCoords, sizeof(DestCoords), &written);
    if (hr < 0 || written != sizeof(DestCoords)) return E_FAIL;

    hr = pStm->Write(&SourceCoords, sizeof(SourceCoords), &written);
    if (hr < 0 || written != sizeof(SourceCoords)) return E_FAIL;

    hr = pStm->Write(&unknown_coords_100, sizeof(unknown_coords_100), &written);
    if (hr < 0 || written != sizeof(unknown_coords_100)) return E_FAIL;

    hr = pStm->Write(&unknown_vector3d_10C_x, sizeof(unknown_vector3d_10C_x), &written);
    if (hr < 0 || written != sizeof(unknown_vector3d_10C_x)) return E_FAIL;

    hr = pStm->Write(&unknown_vector3d_10C_y, sizeof(unknown_vector3d_10C_y), &written);
    if (hr < 0 || written != sizeof(unknown_vector3d_10C_y)) return E_FAIL;

    hr = pStm->Write(&unknown_vector3d_10C_z, sizeof(unknown_vector3d_10C_z), &written);
    if (hr < 0 || written != sizeof(unknown_vector3d_10C_z)) return E_FAIL;

    hr = pStm->Write(&unknown_vector3d_118_x, sizeof(unknown_vector3d_118_x), &written);
    if (hr < 0 || written != sizeof(unknown_vector3d_118_x)) return E_FAIL;

    hr = pStm->Write(&unknown_vector3d_118_y, sizeof(unknown_vector3d_118_y), &written);
    if (hr < 0 || written != sizeof(unknown_vector3d_118_y)) return E_FAIL;

    hr = pStm->Write(&unknown_vector3d_118_z, sizeof(unknown_vector3d_118_z), &written);
    if (hr < 0 || written != sizeof(unknown_vector3d_118_z)) return E_FAIL;

    // Write ParticleSystem (int32 index)
    int32 psIdx = -1;
    if (ParticleSystem && ParticleSystemClass::Array) {
        for (int32 i = 0; i < ParticleSystemClass::Array->Count; ++i) {
            if ((*ParticleSystemClass::Array)[i] == ParticleSystem) { psIdx = i; break; }
        }
    }
    hr = pStm->Write(&psIdx, sizeof(psIdx), &written);
    if (hr < 0 || written != sizeof(psIdx)) return E_FAIL;

    hr = pStm->Write(&RemainingEC, sizeof(RemainingEC), &written);
    if (hr < 0 || written != sizeof(RemainingEC)) return E_FAIL;

    hr = pStm->Write(&RemainingDC, sizeof(RemainingDC), &written);
    if (hr < 0 || written != sizeof(RemainingDC)) return E_FAIL;

    // Write packed BYTEs (StateAIAdvance, unknown_12D, StartStateAI, Translucency)
    uint32 packS = static_cast<uint32>(StateAIAdvance)
                 | (static_cast<uint32>(unknown_12D) << 8)
                 | (static_cast<uint32>(StartStateAI) << 16)
                 | (static_cast<uint32>(Translucency) << 24);
    hr = pStm->Write(&packS, sizeof(packS), &written);
    if (hr < 0 || written != sizeof(packS)) return E_FAIL;

    // Write packed BYTEs (unknown_130, unknown_131)
    uint16 packU = static_cast<uint16>(unknown_130)
                 | (static_cast<uint16>(unknown_131) << 8);
    hr = pStm->Write(&packU, sizeof(packU), &written);
    if (hr < 0 || written != sizeof(packU)) return E_FAIL;

    hr = pStm->Write(&unused_134, sizeof(unused_134), &written);
    if (hr < 0 || written != sizeof(unused_134)) return E_FAIL;

    hr = pStm->Write(&Age, sizeof(Age), &written);
    if (hr < 0 || written != sizeof(Age)) return E_FAIL;

    hr = pStm->Write(&MaxAge, sizeof(MaxAge), &written);
    if (hr < 0 || written != sizeof(MaxAge)) return E_FAIL;

    hr = pStm->Write(&Acceleration, sizeof(Acceleration), &written);
    if (hr < 0 || written != sizeof(Acceleration)) return E_FAIL;

    hr = pStm->Write(&Color, sizeof(Color), &written);
    if (hr < 0 || written != sizeof(Color)) return E_FAIL;

    hr = pStm->Write(&Alpha, sizeof(Alpha), &written);
    if (hr < 0 || written != sizeof(Alpha)) return E_FAIL;

    hr = pStm->Write(&Size_, sizeof(Size_), &written);
    if (hr < 0 || written != sizeof(Size_)) return E_FAIL;

    // Write bool flags as a bitmask
    uint32 flags = 0;
    if (Dead)      flags |= 0x01;
    if (FadingOut) flags |= 0x02;
    hr = pStm->Write(&flags, sizeof(flags), &written);
    if (hr < 0 || written != sizeof(flags)) return E_FAIL;

    hr = pStm->Write(&ParticleType_, sizeof(ParticleType_), &written);
    if (hr < 0 || written != sizeof(ParticleType_)) return E_FAIL;

    return S_OK;
}

// ============================================================================
// AbstractClass
// ============================================================================

AbstractType ParticleClass::WhatAmI() const {
    return AbstractType::Particle;
}

int32 ParticleClass::Size() const {
    return sizeof(ParticleClass);
}

void ParticleClass::Update() {
    if (Dead) return;

    UpdateMotion();
    UpdateLifetime();
    UpdateAlpha();
    UpdateSize();

    // Check if time to die
    if (Age >= MaxAge) {
        FadeOut();
    }

    if (FadingOut && Alpha <= 0) {
        Dead = true;
    }
}

CoordStruct* ParticleClass::GetCoords(CoordStruct* pCrd) const {
    if (pCrd) {
        *pCrd = Location;
    }
    return pCrd;
}

// ============================================================================
// Particle-specific
// ============================================================================

void ParticleClass::Fire() {
    Dead = false;
    FadingOut = false;
    Age = 0;
    Alpha = 255;
}

void ParticleClass::Draw() {
    if (Dead) return;
    if (Alpha == 0) return;

    // In a real implementation, this would render the particle
    // using the surface, color, alpha, and size at the current location
    // DSurface::DrawParticle(Location, Color, Alpha, Size_);
}

void ParticleClass::FadeOut() {
    FadingOut = true;
}

// ============================================================================
// Internal update methods
// ============================================================================

void ParticleClass::UpdateMotion() {
    // Apply velocity
    Location.X += Velocity.X;
    Location.Y += Velocity.Y;
    Location.Z += Velocity.Z;

    // Apply acceleration
    Velocity.X += Acceleration.X;
    Velocity.Y += Acceleration.Y;
    Velocity.Z += Acceleration.Z;

    // Apply gravity if needed
    if (Type) {
        Velocity.Z -= 1; // gravity

        // Apply wind effect
        if (Type->WindInfluence > 0.0f) {
            // In a real implementation, this would read wind data from the map
            Velocity.X += static_cast<int32>(Type->WindInfluence * 10.0f);
        }

        // Apply deceleration
        if (Type->Drag > 0.0f) {
            float decel = Type->Drag;
            float speed = std::sqrt(static_cast<float>(
                Velocity.X * Velocity.X + Velocity.Y * Velocity.Y + Velocity.Z * Velocity.Z));
            if (speed > 0.0f) {
                float factor = (speed - decel) / speed;
                if (factor < 0.0f) factor = 0.0f;
                Velocity.X = static_cast<int32>(Velocity.X * factor);
                Velocity.Y = static_cast<int32>(Velocity.Y * factor);
                Velocity.Z = static_cast<int32>(Velocity.Z * factor);
            }
        }
    }

    // Clamp to ground
    if (Location.Z < 0) {
        Location.Z = 0;
        Velocity.Z = -Velocity.Z / 2; // Bounce
    }
}

void ParticleClass::UpdateLifetime() {
    ++Age;

    if (Type && Type->MaxLifetime > 0) {
        if (Age % 10 == 0) {
            ++StateAIAdvance;
        }
    }

    if (Type && Type->MaxLifetime > 0) {
        RemainingDC = static_cast<WORD>(Type->MaxLifetime - static_cast<int32>(Age));
        if (RemainingDC > 0xFFFF) RemainingDC = 0;
    }

    if (Type && Type->MaxLifetime > 0) {
        RemainingEC = static_cast<WORD>(Type->MaxLifetime - static_cast<int32>(Age));
        if (RemainingEC > 0xFFFF) RemainingEC = 0;
    }
}

void ParticleClass::UpdateAlpha() {
    if (FadingOut) {
        // Fade out over 15 frames
        int32 fadeStep = 255 / 15;
        if (Alpha > static_cast<uint8>(fadeStep)) {
            Alpha -= static_cast<uint8>(fadeStep);
        } else {
            Alpha = 0;
        }
    } else if (Age > MaxAge - 15) {
        // Start fading near end of life
        // Smooth alpha transition
        int32 remaining = MaxAge - Age;
        if (remaining > 0) {
            Alpha = static_cast<uint8>((255 * remaining) / 15);
        } else {
            Alpha = 0;
        }
    }
}

void ParticleClass::UpdateSize() {
    if (Type) {
        // Size can grow or shrink over lifetime
        float lifePercent = (MaxAge > 0) ? static_cast<float>(Age) / static_cast<float>(MaxAge) : 0.0f;
        if (lifePercent < 0.5f) {
            Size_ = 0.5f + lifePercent; // Grow in first half
        } else {
            Size_ = 1.5f - lifePercent; // Shrink in second half
        }
    }
}

// ============================================================================
// Factory methods
// ============================================================================

ParticleClass* ParticleClass::CreateSparkParticle(const CoordStruct& pos, const CoordStruct& vel) {
    CoordStruct pos1 = pos;
    CoordStruct pos2 = pos;
    ParticleTypeClass* type = nullptr;
    ParticleClass* p = new ParticleClass(type, &pos1, &pos2, nullptr);
    p->SetVelocity(vel);
    p->ParticleType_ = 0;
    return p;
}

ParticleClass* ParticleClass::CreateFireParticle(const CoordStruct& pos, const CoordStruct& vel) {
    CoordStruct pos1 = pos;
    CoordStruct pos2 = pos;
    ParticleTypeClass* type = nullptr;
    ParticleClass* p = new ParticleClass(type, &pos1, &pos2, nullptr);
    p->SetVelocity(vel);
    p->ParticleType_ = 1;
    return p;
}

ParticleClass* ParticleClass::CreateSmokeParticle(const CoordStruct& pos, const CoordStruct& vel) {
    CoordStruct pos1 = pos;
    CoordStruct pos2 = pos;
    ParticleTypeClass* type = nullptr;
    ParticleClass* p = new ParticleClass(type, &pos1, &pos2, nullptr);
    p->SetVelocity(vel);
    p->ParticleType_ = 2;
    return p;
}

ParticleClass* ParticleClass::CreateGasParticle(const CoordStruct& pos, const CoordStruct& vel) {
    CoordStruct pos1 = pos;
    CoordStruct pos2 = pos;
    ParticleTypeClass* type = nullptr;
    ParticleClass* p = new ParticleClass(type, &pos1, &pos2, nullptr);
    p->SetVelocity(vel);
    p->ParticleType_ = 3;
    return p;
}

ParticleClass* ParticleClass::CreateRadiationParticle(const CoordStruct& pos, const CoordStruct& vel) {
    CoordStruct pos1 = pos;
    CoordStruct pos2 = pos;
    ParticleTypeClass* type = nullptr;
    ParticleClass* p = new ParticleClass(type, &pos1, &pos2, nullptr);
    p->SetVelocity(vel);
    p->ParticleType_ = 4;
    return p;
}

ParticleClass* ParticleClass::CreateElectricParticle(const CoordStruct& pos, const CoordStruct& vel) {
    CoordStruct pos1 = pos;
    CoordStruct pos2 = pos;
    ParticleTypeClass* type = nullptr;
    ParticleClass* p = new ParticleClass(type, &pos1, &pos2, nullptr);
    p->SetVelocity(vel);
    p->ParticleType_ = 5;
    return p;
}

ParticleClass* ParticleClass::CreateLaserParticle(const CoordStruct& pos, const CoordStruct& vel) {
    CoordStruct pos1 = pos;
    CoordStruct pos2 = pos;
    ParticleTypeClass* type = nullptr;
    ParticleClass* p = new ParticleClass(type, &pos1, &pos2, nullptr);
    p->SetVelocity(vel);
    p->ParticleType_ = 6;
    return p;
}

ParticleClass* ParticleClass::CreateBubbleParticle(const CoordStruct& pos, const CoordStruct& vel) {
    CoordStruct pos1 = pos;
    CoordStruct pos2 = pos;
    ParticleTypeClass* type = nullptr;
    ParticleClass* p = new ParticleClass(type, &pos1, &pos2, nullptr);
    p->SetVelocity(vel);
    p->ParticleType_ = 7;
    return p;
}

// ============================================================================
// File-local particle physics and rendering helpers
//
//  These utilities implement the detailed particle physics simulation
//  (gravity, wind, collision response), color gradient transitions, and
//  lifecycle state management that support the ParticleClass above.
//  Because the header cannot be modified, these are declared as free
//  functions in the anonymous namespace.
// ============================================================================

namespace
{

// --------------------------------------------------------------------------
// Physics constants
// --------------------------------------------------------------------------
constexpr float GRAVITY_ACCELERATION  = 1.0f;       // lepton/frame^2
constexpr float DEFAULT_BOUNCE_FACTOR = 0.5f;       // velocity retention on bounce
constexpr float DEFAULT_FRICTION      = 0.8f;       // velocity retention on ground
constexpr float MAX_VELOCITY          = 500.0f;     // velocity clamp (leptons/frame)
constexpr int32 COLLISION_MARGIN      = 2;          // tolerance for ground contact
constexpr int32 FADE_DURATION         = 15;         // frames for fade-out
constexpr float WIND_BASE_STRENGTH    = 10.0f;      // base wind force in leptons

// --------------------------------------------------------------------------
// Particle lifecycle states
// --------------------------------------------------------------------------
enum class ParticleState : int32
{
    Birth   = 0,   // Just spawned, growing
    Active  = 1,   // Normal lifetime
    Dying   = 2,   // Fading out
    Dead    = 3,   // Fully expired
};

// --------------------------------------------------------------------------
// GetParticleState - Determines the current lifecycle state of a particle
// based on its age, max age, and fade-out flag.
// --------------------------------------------------------------------------
ParticleState GetParticleState(int32 age, int32 maxAge, bool fadingOut)
{
    if (fadingOut) return ParticleState::Dying;
    if (age >= maxAge) return ParticleState::Dying;
    if (age < 5) return ParticleState::Birth;
    return ParticleState::Active;
}

// --------------------------------------------------------------------------
// LifeFraction - Returns the normalized age (0.0 to 1.0) of a particle.
// --------------------------------------------------------------------------
float LifeFraction(int32 age, int32 maxAge)
{
    if (maxAge <= 0) return 1.0f;
    float frac = static_cast<float>(age) / static_cast<float>(maxAge);
    if (frac < 0.0f) frac = 0.0f;
    if (frac > 1.0f) frac = 1.0f;
    return frac;
}

// --------------------------------------------------------------------------
// ApplyGravity - Adds a downward velocity component to simulate gravity.
// The strength can be modulated by a mass factor (heavier particles fall
// faster in game terms, though physically mass cancels out in vacuum).
// --------------------------------------------------------------------------
CoordStruct ApplyGravity(const CoordStruct& velocity, float gravityScale)
{
    CoordStruct result = velocity;
    int32 gravityDelta = static_cast<int32>(GRAVITY_ACCELERATION * gravityScale);
    result.Z -= gravityDelta;
    return result;
}

// --------------------------------------------------------------------------
// ApplyWind - Adds a horizontal velocity component based on the wind
// direction and the particle's wind influence factor.
// --------------------------------------------------------------------------
CoordStruct ApplyWind(const CoordStruct& velocity, float windX, float windY,
                      float influence)
{
    if (influence <= 0.0f) return velocity;

    CoordStruct result = velocity;
    int32 windForceX = static_cast<int32>(windX * WIND_BASE_STRENGTH * influence);
    int32 windForceY = static_cast<int32>(windY * WIND_BASE_STRENGTH * influence);

    result.X += windForceX;
    result.Y += windForceY;
    return result;
}

// --------------------------------------------------------------------------
// ApplyDrag - Reduces the velocity by a drag factor.  Drag is proportional
// to velocity, so faster particles decelerate more.
// --------------------------------------------------------------------------
CoordStruct ApplyDrag(const CoordStruct& velocity, float drag)
{
    if (drag <= 0.0f) return velocity;

    float speed = std::sqrt(static_cast<float>(
        velocity.X * velocity.X +
        velocity.Y * velocity.Y +
        velocity.Z * velocity.Z));

    if (speed <= 0.0f) return velocity;

    float decel = drag;
    float factor = (speed - decel) / speed;
    if (factor < 0.0f) factor = 0.0f;

    CoordStruct result;
    result.X = static_cast<int32>(static_cast<float>(velocity.X) * factor);
    result.Y = static_cast<int32>(static_cast<float>(velocity.Y) * factor);
    result.Z = static_cast<int32>(static_cast<float>(velocity.Z) * factor);
    return result;
}

// --------------------------------------------------------------------------
// ClampVelocity - Ensures the particle's velocity does not exceed the
// maximum speed limit.
// --------------------------------------------------------------------------
CoordStruct ClampVelocity(const CoordStruct& velocity, float maxSpeed)
{
    float speed = std::sqrt(static_cast<float>(
        velocity.X * velocity.X +
        velocity.Y * velocity.Y +
        velocity.Z * velocity.Z));

    if (speed <= maxSpeed) return velocity;
    if (speed <= 0.0f) return velocity;

    float scale = maxSpeed / speed;
    CoordStruct result;
    result.X = static_cast<int32>(static_cast<float>(velocity.X) * scale);
    result.Y = static_cast<int32>(static_cast<float>(velocity.Y) * scale);
    result.Z = static_cast<int32>(static_cast<float>(velocity.Z) * scale);
    return result;
}

// --------------------------------------------------------------------------
// GroundCollision - Handles the particle hitting the ground (Z <= 0).
// Returns the adjusted velocity after applying bounce and friction.
// --------------------------------------------------------------------------
CoordStruct GroundCollision(const CoordStruct& velocity, const CoordStruct& location,
                            float bounceFactor, float friction)
{
    CoordStruct result = velocity;

    if (location.Z <= COLLISION_MARGIN) {
        // Bounce: invert Z velocity with energy loss
        result.Z = -result.Z;
        result.Z = static_cast<int32>(static_cast<float>(result.Z) * bounceFactor);

        // If the bounce velocity is very small, stop bouncing
        if (std::abs(result.Z) < 2) {
            result.Z = 0;
        }

        // Apply ground friction to horizontal velocity
        result.X = static_cast<int32>(static_cast<float>(result.X) * friction);
        result.Y = static_cast<int32>(static_cast<float>(result.Y) * friction);
    }

    return result;
}

// --------------------------------------------------------------------------
// CellCollision - Checks if the particle has collided with a solid object
// on the map (building, terrain).  Returns true if a collision occurred.
// --------------------------------------------------------------------------
bool CellCollision(const CoordStruct& location, const CoordStruct& velocity,
                   CoordStruct& outAdjustedVel)
{
    outAdjustedVel = velocity;

    if (!MapClass::Instance) return false;

    CellStruct cell = CellClass::Coord2Cell(location);
    CellClass* pCell = MapClass::Instance->GetCellAt(cell);
    if (!pCell) return false;

    // Check for solid terrain or building occupier
    if (pCell->Occupier != nullptr) {
        // Simple collision: reverse horizontal velocity
        outAdjustedVel.X = -velocity.X / 2;
        outAdjustedVel.Y = -velocity.Y / 2;
        return true;
    }

    return false;
}

// --------------------------------------------------------------------------
// FullPhysicsUpdate - Applies the complete physics pipeline to a particle:
// gravity, wind, drag, collision, and velocity clamping.  Returns the
// updated velocity.
// --------------------------------------------------------------------------
CoordStruct FullPhysicsUpdate(const CoordStruct& currentVel,
                              const CoordStruct& location,
                              float gravityScale,
                              float windX, float windY,
                              float windInfluence,
                              float drag)
{
    CoordStruct vel = currentVel;

    // Step 1: Apply gravity
    vel = ApplyGravity(vel, gravityScale);

    // Step 2: Apply wind
    vel = ApplyWind(vel, windX, windY, windInfluence);

    // Step 3: Apply drag
    vel = ApplyDrag(vel, drag);

    // Step 4: Ground collision
    vel = GroundCollision(vel, location, DEFAULT_BOUNCE_FACTOR, DEFAULT_FRICTION);

    // Step 5: Cell collision
    CoordStruct adjustedVel;
    if (CellCollision(location, vel, adjustedVel)) {
        vel = adjustedVel;
    }

    // Step 6: Clamp to max velocity
    vel = ClampVelocity(vel, MAX_VELOCITY);

    return vel;
}

// --------------------------------------------------------------------------
// ColorKeyframe - A single point in a color gradient.
// --------------------------------------------------------------------------
struct ColorKeyframe
{
    float   Position;   // 0.0 to 1.0 (life fraction)
    uint8   R;
    uint8   G;
    uint8   B;
    uint8   Alpha;
};

// --------------------------------------------------------------------------
// InterpolateColor - Linearly interpolates between two color keyframes.
// --------------------------------------------------------------------------
ColorStruct InterpolateColorKey(const ColorKeyframe& a, const ColorKeyframe& b, float t)
{
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return ColorStruct(
        static_cast<uint8>(a.R + (b.R - a.R) * t),
        static_cast<uint8>(a.G + (b.G - a.G) * t),
        static_cast<uint8>(a.B + (b.B - a.B) * t));
}

// --------------------------------------------------------------------------
// InterpolateAlpha - Linearly interpolates between two alpha values.
// --------------------------------------------------------------------------
uint8 InterpolateAlpha(const ColorKeyframe& a, const ColorKeyframe& b, float t)
{
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    float alpha = a.Alpha + (b.Alpha - a.Alpha) * t;
    return static_cast<uint8>(alpha);
}

// --------------------------------------------------------------------------
// SparkGradient - Color keyframes for spark particles.  Sparks start
// bright white-yellow and fade to orange then dark red.
// --------------------------------------------------------------------------
const ColorKeyframe g_SparkGradient[] = {
    { 0.00f, 255, 255, 220, 255 },
    { 0.20f, 255, 220, 100, 240 },
    { 0.50f, 255, 140,  40, 200 },
    { 0.80f, 200,  60,  20, 120 },
    { 1.00f, 100,  20,  10,   0 },
};
constexpr int32 SPARK_GRADIENT_COUNT = 5;

// --------------------------------------------------------------------------
// FireGradient - Color keyframes for fire particles.  Fire goes from
// white-hot to yellow to orange to dark red as it cools.
// --------------------------------------------------------------------------
const ColorKeyframe g_FireGradient[] = {
    { 0.00f, 255, 255, 240, 200 },
    { 0.15f, 255, 240, 150, 220 },
    { 0.35f, 255, 180,  60, 200 },
    { 0.60f, 240, 100,  30, 160 },
    { 0.85f, 180,  50,  20, 100 },
    { 1.00f, 100,  20,  10,   0 },
};
constexpr int32 FIRE_GRADIENT_COUNT = 6;

// --------------------------------------------------------------------------
// SmokeGradient - Color keyframes for smoke particles.  Smoke starts dark
// and lightens as it rises and disperses.
// --------------------------------------------------------------------------
const ColorKeyframe g_SmokeGradient[] = {
    { 0.00f,  60,  60,  60, 200 },
    { 0.25f,  90,  90,  90, 180 },
    { 0.50f, 120, 120, 120, 150 },
    { 0.75f, 150, 150, 150, 100 },
    { 1.00f, 180, 180, 180,   0 },
};
constexpr int32 SMOKE_GRADIENT_COUNT = 5;

// --------------------------------------------------------------------------
// GasGradient - Color keyframes for gas particles (e.g., toxin).
// --------------------------------------------------------------------------
const ColorKeyframe g_GasGradient[] = {
    { 0.00f,  80, 200,  60, 200 },
    { 0.30f, 100, 220,  80, 180 },
    { 0.60f,  60, 160,  40, 140 },
    { 1.00f,  40, 100,  20,   0 },
};
constexpr int32 GAS_GRADIENT_COUNT = 4;

// --------------------------------------------------------------------------
// RadiationGradient - Color keyframes for radiation particles.
// --------------------------------------------------------------------------
const ColorKeyframe g_RadiationGradient[] = {
    { 0.00f, 100, 255,  50, 180 },
    { 0.30f,  80, 220,  40, 160 },
    { 0.60f,  60, 180,  30, 120 },
    { 1.00f,  40, 120,  20,   0 },
};
constexpr int32 RADIATION_GRADIENT_COUNT = 4;

// --------------------------------------------------------------------------
// ElectricGradient - Color keyframes for electric particles.
// --------------------------------------------------------------------------
const ColorKeyframe g_ElectricGradient[] = {
    { 0.00f, 200, 220, 255, 255 },
    { 0.25f, 150, 200, 255, 220 },
    { 0.50f, 100, 180, 255, 180 },
    { 0.75f,  80, 150, 240, 120 },
    { 1.00f,  60, 100, 200,   0 },
};
constexpr int32 ELECTRIC_GRADIENT_COUNT = 5;

// --------------------------------------------------------------------------
// LaserGradient - Color keyframes for laser particles.
// --------------------------------------------------------------------------
const ColorKeyframe g_LaserGradient[] = {
    { 0.00f, 255,  60,  60, 255 },
    { 0.30f, 255, 100, 100, 230 },
    { 0.60f, 220,  80,  80, 180 },
    { 1.00f, 180,  40,  40,   0 },
};
constexpr int32 LASER_GRADIENT_COUNT = 4;

// --------------------------------------------------------------------------
// BubbleGradient - Color keyframes for bubble particles (underwater).
// --------------------------------------------------------------------------
const ColorKeyframe g_BubbleGradient[] = {
    { 0.00f, 180, 220, 255, 150 },
    { 0.30f, 200, 240, 255, 160 },
    { 0.60f, 220, 250, 255, 140 },
    { 1.00f, 240, 255, 255,   0 },
};
constexpr int32 BUBBLE_GRADIENT_COUNT = 4;

// --------------------------------------------------------------------------
// GetGradientForType - Returns the color keyframe array and count for the
// given particle sub-type.
// --------------------------------------------------------------------------
const ColorKeyframe* GetGradientForType(int32 particleType, int32& outCount)
{
    switch (particleType) {
    case 0: outCount = SPARK_GRADIENT_COUNT;      return g_SparkGradient;
    case 1: outCount = FIRE_GRADIENT_COUNT;       return g_FireGradient;
    case 2: outCount = SMOKE_GRADIENT_COUNT;      return g_SmokeGradient;
    case 3: outCount = GAS_GRADIENT_COUNT;        return g_GasGradient;
    case 4: outCount = RADIATION_GRADIENT_COUNT;  return g_RadiationGradient;
    case 5: outCount = ELECTRIC_GRADIENT_COUNT;   return g_ElectricGradient;
    case 6: outCount = LASER_GRADIENT_COUNT;      return g_LaserGradient;
    case 7: outCount = BUBBLE_GRADIENT_COUNT;     return g_BubbleGradient;
    default: outCount = 0; return nullptr;
    }
}

// --------------------------------------------------------------------------
// EvaluateGradient - Returns the interpolated color at the given life
// fraction by walking the keyframe array.
// --------------------------------------------------------------------------
ColorStruct EvaluateGradient(const ColorKeyframe* keyframes, int32 count, float lifeFraction)
{
    if (!keyframes || count <= 0) return ColorStruct(255, 255, 255);

    if (lifeFraction <= keyframes[0].Position) {
        return ColorStruct(keyframes[0].R, keyframes[0].G, keyframes[0].B);
    }
    if (lifeFraction >= keyframes[count - 1].Position) {
        const ColorKeyframe& last = keyframes[count - 1];
        return ColorStruct(last.R, last.G, last.B);
    }

    for (int32 i = 0; i < count - 1; ++i) {
        if (lifeFraction >= keyframes[i].Position && lifeFraction <= keyframes[i + 1].Position) {
            float span = keyframes[i + 1].Position - keyframes[i].Position;
            if (span <= 0.0f) {
                return ColorStruct(keyframes[i].R, keyframes[i].G, keyframes[i].B);
            }
            float t = (lifeFraction - keyframes[i].Position) / span;
            return InterpolateColorKey(keyframes[i], keyframes[i + 1], t);
        }
    }

    return ColorStruct(keyframes[count - 1].R, keyframes[count - 1].G, keyframes[count - 1].B);
}

// --------------------------------------------------------------------------
// EvaluateAlphaGradient - Returns the interpolated alpha at the given life
// fraction.
// --------------------------------------------------------------------------
uint8 EvaluateAlphaGradient(const ColorKeyframe* keyframes, int32 count, float lifeFraction)
{
    if (!keyframes || count <= 0) return 255;

    if (lifeFraction <= keyframes[0].Position) return keyframes[0].Alpha;
    if (lifeFraction >= keyframes[count - 1].Position) return keyframes[count - 1].Alpha;

    for (int32 i = 0; i < count - 1; ++i) {
        if (lifeFraction >= keyframes[i].Position && lifeFraction <= keyframes[i + 1].Position) {
            float span = keyframes[i + 1].Position - keyframes[i].Position;
            if (span <= 0.0f) return keyframes[i].Alpha;
            float t = (lifeFraction - keyframes[i].Position) / span;
            return InterpolateAlpha(keyframes[i], keyframes[i + 1], t);
        }
    }

    return keyframes[count - 1].Alpha;
}

// --------------------------------------------------------------------------
// UpdateParticleColor - Sets the particle's color and alpha based on its
// current life fraction and particle sub-type gradient.
// --------------------------------------------------------------------------
void UpdateParticleColor(ParticleClass* particle)
{
    if (!particle) return;

    float frac = LifeFraction(particle->Age, particle->MaxAge);
    int32 count = 0;
    const ColorKeyframe* gradient = GetGradientForType(particle->ParticleType_, count);

    if (gradient && count > 0) {
        ColorStruct newColor = EvaluateGradient(gradient, count, frac);
        uint8 newAlpha = EvaluateAlphaGradient(gradient, count, frac);

        // Blend with the fade-out if active
        if (particle->FadingOut) {
            int32 fadeStep = 255 / FADE_DURATION;
            int32 currentAlpha = particle->Alpha;
            if (currentAlpha > fadeStep) {
                newAlpha = static_cast<uint8>(currentAlpha - fadeStep);
            } else {
                newAlpha = 0;
            }
        }

        particle->Color = newColor;
        particle->Alpha = newAlpha;
    }
}

// --------------------------------------------------------------------------
// SizeForLifeFraction - Computes the particle size based on its life
// fraction.  Particles grow during birth, hold during active life, and
// shrink during death.
// --------------------------------------------------------------------------
float SizeForLifeFraction(float lifeFraction, float baseSize)
{
    if (lifeFraction < 0.1f) {
        // Birth: grow from 0 to full size
        return baseSize * (lifeFraction / 0.1f);
    } else if (lifeFraction < 0.7f) {
        // Active: hold at full size with slight pulsation
        float pulse = 1.0f + 0.1f * std::sin(lifeFraction * 20.0f);
        return baseSize * pulse;
    } else {
        // Death: shrink from full to 0
        float deathProgress = (lifeFraction - 0.7f) / 0.3f;
        return baseSize * (1.0f - deathProgress);
    }
}

// --------------------------------------------------------------------------
// RandomizedMaxAge - Returns a randomized max age around a base value to
// give particles natural variation in their lifetimes.
// --------------------------------------------------------------------------
int32 RandomizedMaxAge(int32 baseAge, float variation)
{
    if (baseAge <= 0) return 30;
    if (variation <= 0.0f) return baseAge;

    int32 range = static_cast<int32>(static_cast<float>(baseAge) * variation);
    if (range <= 0) return baseAge;

    int32 offset = std::rand() % (range * 2 + 1) - range;
    int32 result = baseAge + offset;
    if (result < 1) result = 1;
    return result;
}

// --------------------------------------------------------------------------
// RandomVelocity - Generates a random velocity vector for particle spawn.
// Used to create spread patterns for explosions and emission effects.
// --------------------------------------------------------------------------
CoordStruct RandomVelocity(int32 speed, float spreadAngle, int32 upBias)
{
    if (speed <= 0) return CoordStruct(0, 0, 0);

    // Random angle in the horizontal plane
    float angle = static_cast<float>(std::rand() % 360) * (3.14159265358979323846f / 180.0f);

    // Apply spread to the vertical component
    float verticalSpread = (spreadAngle > 0.0f)
        ? static_cast<float>(std::rand() % 100) / 100.0f * spreadAngle
        : 0.0f;

    float horizontalSpeed = static_cast<float>(speed) * std::cos(verticalSpread);
    float verticalSpeed = static_cast<float>(speed) * std::sin(verticalSpread) + static_cast<float>(upBias);

    int32 vx = static_cast<int32>(std::cos(angle) * horizontalSpeed);
    int32 vy = static_cast<int32>(std::sin(angle) * horizontalSpeed);
    int32 vz = static_cast<int32>(verticalSpeed);

    return CoordStruct(vx, vy, vz);
}

// --------------------------------------------------------------------------
// RandomSpawnOffset - Returns a random offset from the center for spawning
// particles in a cluster.
// --------------------------------------------------------------------------
CoordStruct RandomSpawnOffset(int32 radius)
{
    if (radius <= 0) return CoordStruct(0, 0, 0);

    int32 dx = (std::rand() % (radius * 2 + 1)) - radius;
    int32 dy = (std::rand() % (radius * 2 + 1)) - radius;
    int32 dz = (std::rand() % (radius + 1));

    return CoordStruct(dx, dy, dz);
}

// --------------------------------------------------------------------------
// ParticleSubTypeName - Returns the name string for a particle sub-type.
// --------------------------------------------------------------------------
const char* ParticleSubTypeName(int32 type)
{
    switch (type) {
    case 0: return "Spark";
    case 1: return "Fire";
    case 2: return "Smoke";
    case 3: return "Gas";
    case 4: return "Radiation";
    case 5: return "Electric";
    case 6: return "Laser";
    case 7: return "Bubble";
    default: return "Unknown";
    }
}

// --------------------------------------------------------------------------
// ParticleStateName - Returns the name string for a particle lifecycle
// state.
// --------------------------------------------------------------------------
const char* ParticleStateName(ParticleState state)
{
    switch (state) {
    case ParticleState::Birth:  return "Birth";
    case ParticleState::Active: return "Active";
    case ParticleState::Dying:  return "Dying";
    case ParticleState::Dead:   return "Dead";
    default:                    return "Unknown";
    }
}

// --------------------------------------------------------------------------
// VelocitySpeed - Returns the magnitude of a velocity vector.
// --------------------------------------------------------------------------
float VelocitySpeed(const CoordStruct& velocity)
{
    return std::sqrt(static_cast<float>(
        velocity.X * velocity.X +
        velocity.Y * velocity.Y +
        velocity.Z * velocity.Z));
}

// --------------------------------------------------------------------------
// VelocityDirection - Returns the direction (in radians) of the horizontal
// component of a velocity vector.
// --------------------------------------------------------------------------
float VelocityDirection(const CoordStruct& velocity)
{
    if (velocity.X == 0 && velocity.Y == 0) return 0.0f;
    return std::atan2(static_cast<float>(velocity.Y), static_cast<float>(velocity.X));
}

// --------------------------------------------------------------------------
// IsParticleVisible - Returns true if the particle should be rendered
// (not dead, has non-zero alpha, and is within the visible map area).
// --------------------------------------------------------------------------
bool IsParticleVisible(const ParticleClass* particle)
{
    if (!particle) return false;
    if (particle->Dead) return false;
    if (particle->Alpha == 0) return false;
    return true;
}

// --------------------------------------------------------------------------
// DistanceBetweenParticles - Returns the 3D distance between two
// particles.
// --------------------------------------------------------------------------
int32 DistanceBetweenParticles(const ParticleClass* a, const ParticleClass* b)
{
    if (!a || !b) return 0;
    int32 dx = a->Location.X - b->Location.X;
    int32 dy = a->Location.Y - b->Location.Y;
    int32 dz = a->Location.Z - b->Location.Z;
    return static_cast<int32>(std::sqrt(static_cast<float>(dx * dx + dy * dy + dz * dz)));
}

// --------------------------------------------------------------------------
// ShouldParticleExpire - Returns true if the particle has exceeded its
// maximum age and should begin the fade-out process.
// --------------------------------------------------------------------------
bool ShouldParticleExpire(int32 age, int32 maxAge)
{
    return age >= maxAge;
}

// --------------------------------------------------------------------------
// ComputeFadeAlpha - Computes the alpha value during the fade-out phase.
// The alpha decreases linearly over FADE_DURATION frames.
// --------------------------------------------------------------------------
uint8 ComputeFadeAlpha(int32 fadeFramesElapsed)
{
    if (fadeFramesElapsed >= FADE_DURATION) return 0;
    int32 alpha = 255 - (255 * fadeFramesElapsed / FADE_DURATION);
    if (alpha < 0) alpha = 0;
    return static_cast<uint8>(alpha);
}

// --------------------------------------------------------------------------
// BirthAlpha - Computes the alpha value during the birth phase.  The alpha
// ramps up from 0 to full over a few frames for a smooth appearance.
// --------------------------------------------------------------------------
uint8 BirthAlpha(int32 age)
{
    constexpr int32 BIRTH_FRAMES = 5;
    if (age >= BIRTH_FRAMES) return 255;
    int32 alpha = (255 * age) / BIRTH_FRAMES;
    if (alpha < 0) alpha = 0;
    return static_cast<uint8>(alpha);
}

// --------------------------------------------------------------------------
// UpdateParticleState - Advances the particle's lifecycle state and
// returns the new state.  Transitions: Birth -> Active -> Dying -> Dead.
// --------------------------------------------------------------------------
ParticleState UpdateParticleState(ParticleClass* particle)
{
    if (!particle) return ParticleState::Dead;

    if (particle->Dead) return ParticleState::Dead;

    ParticleState state = GetParticleState(particle->Age, particle->MaxAge, particle->FadingOut);

    if (state == ParticleState::Dying && !particle->FadingOut) {
        particle->FadeOut();
    }

    if (particle->FadingOut && particle->Alpha == 0) {
        particle->Dead = true;
        return ParticleState::Dead;
    }

    return state;
}

// --------------------------------------------------------------------------
// SpawnChildParticle - Creates a child particle at the parent's location
// with a random velocity offset.  Used for particle emission systems.
// --------------------------------------------------------------------------
ParticleClass* SpawnChildParticle(ParticleClass* parent, int32 childType,
                                  int32 speed, float spreadAngle)
{
    if (!parent) return nullptr;

    CoordStruct offset = RandomSpawnOffset(8);
    CoordStruct spawnPos = parent->Location;
    spawnPos.X += offset.X;
    spawnPos.Y += offset.Y;
    spawnPos.Z += offset.Z;

    CoordStruct vel = RandomVelocity(speed, spreadAngle, 0);

    ParticleClass* child = nullptr;
    switch (childType) {
    case 0: child = ParticleClass::CreateSparkParticle(spawnPos, vel); break;
    case 1: child = ParticleClass::CreateFireParticle(spawnPos, vel); break;
    case 2: child = ParticleClass::CreateSmokeParticle(spawnPos, vel); break;
    case 3: child = ParticleClass::CreateGasParticle(spawnPos, vel); break;
    case 4: child = ParticleClass::CreateRadiationParticle(spawnPos, vel); break;
    case 5: child = ParticleClass::CreateElectricParticle(spawnPos, vel); break;
    case 6: child = ParticleClass::CreateLaserParticle(spawnPos, vel); break;
    case 7: child = ParticleClass::CreateBubbleParticle(spawnPos, vel); break;
    default: break;
    }

    return child;
}

// --------------------------------------------------------------------------
// CountParticlesByType - Returns the number of particles in the global
// array with the given sub-type.
// --------------------------------------------------------------------------
int32 CountParticlesByType(int32 particleType)
{
    if (!ParticleClass::Array) return 0;
    int32 count = 0;
    for (int32 i = 0; i < ParticleClass::Array->Count; ++i) {
        ParticleClass* p = (*ParticleClass::Array)[i];
        if (p && !p->Dead && p->ParticleType_ == particleType) {
            ++count;
        }
    }
    return count;
}

// --------------------------------------------------------------------------
// CountAliveParticles - Returns the total number of non-dead particles in
// the global array.
// --------------------------------------------------------------------------
int32 CountAliveParticles()
{
    if (!ParticleClass::Array) return 0;
    int32 count = 0;
    for (int32 i = 0; i < ParticleClass::Array->Count; ++i) {
        ParticleClass* p = (*ParticleClass::Array)[i];
        if (p && !p->Dead) {
            ++count;
        }
    }
    return count;
}

// --------------------------------------------------------------------------
// RemoveDeadParticles - Removes all dead particles from the global array
// and frees their memory.  Returns the number of particles removed.
// --------------------------------------------------------------------------
int32 RemoveDeadParticles()
{
    if (!ParticleClass::Array) return 0;
    int32 removed = 0;
    for (int32 i = ParticleClass::Array->Count - 1; i >= 0; --i) {
        ParticleClass* p = (*ParticleClass::Array)[i];
        if (p && p->Dead) {
            delete p;
            ++removed;
        }
    }
    return removed;
}

// --------------------------------------------------------------------------
// UpdateAllParticles - Updates every particle in the global array.
// --------------------------------------------------------------------------
void UpdateAllParticles()
{
    if (!ParticleClass::Array) return;
    for (int32 i = 0; i < ParticleClass::Array->Count; ++i) {
        ParticleClass* p = (*ParticleClass::Array)[i];
        if (p && !p->Dead) {
            p->Update();
        }
    }
}

// --------------------------------------------------------------------------
// DrawAllParticles - Draws every visible particle in the global array.
// --------------------------------------------------------------------------
void DrawAllParticles()
{
    if (!ParticleClass::Array) return;
    for (int32 i = 0; i < ParticleClass::Array->Count; ++i) {
        ParticleClass* p = (*ParticleClass::Array)[i];
        if (p && IsParticleVisible(p)) {
            p->Draw();
        }
    }
}

// --------------------------------------------------------------------------
// ClearAllParticles - Destroys all particles and clears the global array.
// --------------------------------------------------------------------------
void ClearAllParticles()
{
    if (!ParticleClass::Array) return;
    for (int32 i = 0; i < ParticleClass::Array->Count; ++i) {
        ParticleClass* p = (*ParticleClass::Array)[i];
        if (p) {
            delete p;
        }
    }
    ParticleClass::Array->Clear();
}

} // end anonymous namespace
