#include "ParticleClass.h"
#include "ParticleSystemClass.h"
#include "../Abstract/TechnoClass.h"
#include "../Combat/WarheadTypeClass.h"
#include "../Map/MapClass.h"
#include "../Map/CellClass.h"
#include "../Houses/HouseClass.h"
#include "../Math/Timer.h"
#include "../Math/Rectangle.h"
#include "../Rendering/TacticalClass.h"
#include "../Rendering/DisplayClass.h"
#include "../Rendering/Surface.h"
#include "../Rendering/ConvertClass.h"
#include "../FileFormats/SHP.h"
#include "../Helpers/AsmUIHelpers.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

// ============================================================================
// Static members
// ============================================================================

DynamicVectorClass<ParticleClass*>* ParticleClass::Array = nullptr;

// asm: 数据段 ParticleClass_Default_CellCoords(+word_AC49B2) 与
//      ParticleClass_Default_X/Y/Z，由 Init_Default* 清零。
CellStruct ParticleClass::DefaultCellCoords = CellStruct(0, 0);
CoordStruct ParticleClass::DefaultRoomCoords = CoordStruct(0, 0, 0);

// ============================================================================
// Init_DefaultCellCoords / Init_DefaultRoomCoords
//
// asm: ParticleClass_Init_DefaultCellCoords (0x62D9E0)
//   xor eax,eax / mov ParticleClass_Default_CellCoords,ax
//               / mov word_AC49B2,ax / retn
//  把默认单元格坐标（X,Y 两个 16 位分量）整体清零。
// asm: ParticleClass_Init_DefaultRoomCoords (0x62D9F0)
//   xor eax,eax / mov Default_X,eax / Default_Y,eax / Default_Z,eax / retn
//  把默认世界坐标（X,Y,Z）整体清零。
// ============================================================================
void ParticleClass::Init_DefaultCellCoords() {
    DefaultCellCoords.X = 0;   // asm: mov ParticleClass_Default_CellCoords, ax
    DefaultCellCoords.Y = 0;   // asm: mov word_AC49B2, ax
}

void ParticleClass::Init_DefaultRoomCoords() {
    DefaultRoomCoords.X = 0;   // asm: mov ParticleClass_Default_X, eax
    DefaultRoomCoords.Y = 0;   // asm: mov ParticleClass_Default_Y, eax
    DefaultRoomCoords.Z = 0;   // asm: mov ParticleClass_Default_Z, eax
}

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

// ============================================================================
// GetAbstractDerivationID — 抽象派生编号
//
// asm: ParticleClass_GetAbstractDerivationID (0x62DA6E)
//   mov eax, 16h / retn
//   16h(22) == AbstractType::Particle，供存档 Swizzle 阶段判别派生类型。
// ============================================================================
AbstractType ParticleClass::GetAbstractDerivationID() const {
    return AbstractType::Particle;   // asm: mov eax,16h
}

// ============================================================================
// 图层三件套
// ============================================================================

// asm: ParticleClass_GetLayer (0x62DA8A)
//   mov eax, 2 / retn    -> Layer::Ground
Layer ParticleClass::GetLayer() const {
    return Layer::Ground;   // asm: mov eax,2
}

// asm: ParticleClass_SetLayer (0x62DB64)
//   mov eax,[esp+idxLayer] / push eax / call ObjectClass::Mark
//   test al,al / setnz al / retn 4
//   转发基类 Mark_Layer 并把返回真值规格化为 0/1。
bool ParticleClass::Mark_Layer(int32 idxLayer) {
    return ObjectClass::Mark_Layer(idxLayer);   // asm: call ObjectClass::Mark
}

// asm: ParticleClass_InWhichLayer (0x62DB8B)
//   mov eax, Air / retn   (Air == 3)
//   粒子一律绘制在 Air 层。
Layer ParticleClass::InWhichLayer() const {
    return Layer::Air;   // asm: mov eax,Air(3)
}

int32 ParticleClass::Size() const {
    return sizeof(ParticleClass);
}

// ============================================================================
// Update_All — 顶层 per-frame 推进
//
// asm: ParticleClass_Update_All (0x62CE40)
//   switch (Type->BehavesLike)  (jump table, 5 cases: 0..4)
//     0 -> Update_Smoke   1 -> Update_Gas   2 -> Update_Fire
//     3 -> Update_Spark   4 -> Update_Railgun
//   随后 dec RemainingEC；若归零则 TimeToDie = 1。
//   注：asm 的 TimeToDie 对应本实现的 Dead 标记。
// ============================================================================
void ParticleClass::Update_All() {
    // asm: mov eax,[esi+Type] / mov eax,[eax+BehavesLike] / cmp eax,4 / ja default
    if (Type) {
        switch (Type->BehavesLike) {
        case 0: Update_Smoke();   break;   // asm: bl0Smoke
        case 1: Update_Gas();     break;   // asm: bl1Gas
        case 2: Update_Fire();    break;   // asm: bl2Fire
        case 3: Update_Spark();   break;   // asm: bl3Spark
        case 4: Update_Railgun(); break;   // asm: bl4Railgun
        default: break;                    // asm: loc_62CE86
        }
    }

    // asm: dec [esi+ParticleClass.RemainEC] / cmp ..,0 / jnz skip
    if (RemainingEC > 0) {
        --RemainingEC;
    }
    if (RemainingEC == 0) {
        Dead = true;   // asm: mov [esi+ParticleClass.TimeToDie], 1
    }
}

// ============================================================================
// asm: ParticleClass_Update_All (0x62CE40) 的 per-frame 主体，经 AI() 表达。
//   Update() 只做门控（Dead 提前返回），随后转译 AI()。
// ============================================================================
void ParticleClass::Update() {
    if (Dead) return;
    AI();
}

// ============================================================================
// asm: ParticleClass_Update_All (0x62CE40) + 生命周期推进
//
//   1) 按 Type->BehavesLike 分派到 Update_Smoke/Gas/Fire/Spark/Railgun；
//   2) dec RemainingEC；归零 → Dead=1；
//   3) Age++；达 MaxAge → 淡出，淡出完毕 → Dead=1。
// ============================================================================
int32 ParticleClass::AI() {
    // 1) 行为分派（含 RemainingEC 递减与到期置死）。
    Update_All();

    // 2) 年龄推进与淡出。
    ++Age;
    if (MaxAge > 0 && Age >= MaxAge) {
        FadeOut();
    }
    UpdateAlpha();

    if (FadingOut && Alpha == 0) {
        Dead = true;
    }

    // 3) unknown_131（收束标志）→ Dead。
    if (unknown_131 != 0) {
        Dead = true;
    }

    return Dead ? 1 : 0;
}

// ============================================================================
// asm: ParticleClass_PointerGotInvalid(pInvalid, removed)
//   转发基类失效清理；若失效对象是本粒子的 ParticleSystem 引用则清空。
// ============================================================================
void ParticleClass::PointerGotInvalid(AbstractClass* pInvalid, bool removed) {
    ObjectClass::PointerGotInvalid(pInvalid, removed);

    if (pInvalid != nullptr
        && ParticleSystem == static_cast<ParticleSystemClass*>(pInvalid)) {
        ParticleSystem = nullptr;
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

// ============================================================================
// asm: ParticleClass_Draw (0x62CEF0, retn 8 = pos, OtherSurface)
//
//   把粒子当前帧画到目标表面。原版控制流：
//     * 帧率低于下限且非强制可见 → 跳过；
//     * DetailFrameRate 与 Type 可见性判定；点选/黑幕/地图外剔除；
//     * 依 Translucency 组合 BlitterFlags（25/50/75 档）；
//     * 由世界坐标换算屏幕落点并逐帧 blit。
//   OpenYR 的像素 blit 由战术层轮询 ParticleClass::Array 驱动，这里按 asm
//   计算落点并把脏矩形登记给战术层。
// ============================================================================
void ParticleClass::Draw() {
    if (Dead) return;
    if (Alpha == 0) return;
    if (Type == nullptr) return;
    if (TacticalClass::Instance == nullptr) return;

    Surface* pSurface = TacticalClass::Instance->Target_Surface();
    if (pSurface == nullptr) return;

    CoordStruct world;
    GetCoords(&world);

    Point2D screen = TacticalClass::Instance->CoordsToScreen(world);
    const Point2D origin = TacticalClass::Instance->GetTacticalOrigin();
    screen.X -= origin.X;
    screen.Y -= origin.Y;

    Rectangle clip(0, 0, pSurface->GetWidth(), pSurface->GetHeight());
    Draw(&screen, &clip);
}

// ============================================================================
// asm: ParticleClass_Draw (0x62CEF0) 的真正绘制分支
//
//   0x62CF65: 普通粒子（Type->ParticleType 不是 3/4）走
//     CC_Draw_Shape(TempSurface, Convert_ANIM, vt+6Ch(GetImage),
//                   vt+1E8h(GetImageFrame), pos,
//                   OtherSurface(boundingRect), flags, Remap=0,
//                   ZAdjust = 0xFFFFFFF1 - Draw_AdjustForBuildingZ(z),
//                   ZGradientDescIndex=2, Brightness=1000, TintColor=0,
//                   ZShape=0, ZShapeFrame=0, XOffset=0, YOffset=0)
//
//   flags 基值 0x2800，按 TransparencyLevel 追加 25/50/75 档
//   （asm: 0x19 -> |2, 0x32 -> |4, >=0x4B -> |6），最后统一 |0x0E00。
//
//   Type->ParticleType 为 3/4（Trail/Railgun）时 asm 走另一条
//   Coord_To_Pixel + 线段绘制分支；本树没有独立的 Convert/Trail 素材，
//   该分支不在此处伪造，交给 ParticleSystemClass 的源类型处理。
// ============================================================================
void ParticleClass::Draw(Point2D* pPos, const Rectangle* pClip) {
    if (Dead || pPos == nullptr) return;
    if (Alpha == 0) return;
    if (Type == nullptr) return;
    if (TacticalClass::Instance == nullptr) return;

    Surface* pSurface = TacticalClass::Instance->Target_Surface();
    if (pSurface == nullptr) return;

    // 没有素材就不画 —— 严禁用程序化图形顶替美术资源。
    SHPStruct* pShape = Type->GetImage();
    if (pShape == nullptr) return;

    // ---- BlitterFlags（asm 0x62CF8C..0x62CFEC）----------------------
    uint32 flags = 0x2800u;                       // ZGradientDeg45 | Alpha
    const int32 transparency = Type->Translucency;
    if (transparency == 0x19)                     // 25%
        flags |= 0x0002u;
    else if (transparency == 0x32)                // 50%
        flags |= 0x0004u;
    else if (transparency >= 0x4B)                // 75%
        flags |= 0x0006u;
    flags |= 0x0E00u;                             // Transparent | Alpha | Centered

    // ---- Z 调整（asm: ebx = 0xFFFFFFF1 - Draw_AdjustForBuildingZ(z)）----
    CoordStruct world;
    GetCoords(&world);
    const int32 zAdjust = -15 - Draw_AdjustForBuildingZ(world.Z);

    // ---- 帧号（asm vt+1E8h = GetImageFrame）--------------------------
    int32 frame = GetImageFrame();
    if (frame < 0) frame = 0;
    // 防止帧号越过 SHP 实际帧数（GetImageFrame 的烟雾线性推进可能超界）
    const int32 frameCount = static_cast<int32>(pShape->Frames);
    if (frameCount > 0 && frame >= frameCount)
        frame %= frameCount;

    CC_Draw_Shape(pSurface, StandardPalette, pShape, frame, pPos, pClip,
                  static_cast<BlitterFlags>(flags),
                  0, zAdjust, 2, 1000, 0, nullptr, 0, 0, 0);
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

// ------------------------------------------------------------------------
// asm: ParticleClass_Update_Gas (0x62C4F0)
//
//   毒气粒子的每帧推进。原版控制流：
//     * test Frame,1 / jz 跳过：奇数帧参与随机游走；
//       (rand&3)==0 时随机在 X 或 Y 上做 -1..+1 漂移，夹到 [-5,5]。
//     * 奇数帧一律把 Z 速度清零。
//     * unknown_130(状态计) < Type->EndStateAI 时，以 (Frame&1)+StateAIAdvance
//       为除数推进状态；到达 EndStateAI 且 DeleteOnStateLimit → unknown_131=1。
//     * 速度幅值 > 3.0 时按 Type->Deacc 衰减。
// ------------------------------------------------------------------------
void ParticleClass::Update_Gas()
{
    // 1) 随机游走。
    if ((FrameTimer::GetTime() & 1) != 0) {
        if ((std::rand() & 3) == 0) {
            const int32 step = (std::rand() % 3) - 1;
            if ((std::rand() & 1) == 0) {
                Velocity.X += step;
                if (Velocity.X < -5) Velocity.X = -5;
                if (Velocity.X >  5) Velocity.X =  5;
            } else {
                Velocity.Y += step;
                if (Velocity.Y < -5) Velocity.Y = -5;
                if (Velocity.Y >  5) Velocity.Y =  5;
            }
        }
        Velocity.Z = 0;   // asm: mov dword ptr [esi+0C8h], 0
    }

    if (Type == nullptr) {
        UpdateMotion();
        return;
    }

    // 2) 状态推进。
    if (unknown_130 < Type->EndStateAI) {
        const int32 divisor = (FrameTimer::GetTime() & 1) + StateAIAdvance;
        if (divisor > 0 && (FrameTimer::GetTime() % divisor) == 0) {
            ++unknown_130;
        }
        if (unknown_130 == Type->EndStateAI && Type->DeleteOnStateLimit) {
            unknown_131 = 1;
        }
    }

    // 3) 速度衰减（asm loc_62C6AB: fld Velocity / fcomp 3.0 / 超过则 -= Deacc）。
    const double speed = std::sqrt(static_cast<double>(
        Velocity.X * Velocity.X + Velocity.Y * Velocity.Y + Velocity.Z * Velocity.Z));
    if (speed > 3.0 && Type->Deacc > 0.0) {
        const int32 d = static_cast<int32>(Type->Deacc);
        if (d > 0) {
            Velocity.X += (Velocity.X > 0 ? -d : d);
            Velocity.Y += (Velocity.Y > 0 ? -d : d);
        }
    }

    UpdateMotion();
}

// ------------------------------------------------------------------------
// asm: ParticleClass_Update_Smoke (0x62BC00)
//
//   烟雾粒子每帧推进：偶数帧参与 1/8 概率的随机游走，漂移步长 -1..+1，
//   夹到 [-2,2]；其余状态推进/速度衰减与 Gas 同构；烟雾整体缓慢上升并
//   随寿命膨胀。
// ------------------------------------------------------------------------
void ParticleClass::Update_Smoke()
{
    if ((FrameTimer::GetTime() & 1) == 0) {
        if ((std::rand() & 7) == 0) {
            const int32 step = (std::rand() % 3) - 1;
            if ((std::rand() & 1) == 0) {
                Velocity.X += step;
                if (Velocity.X < -2) Velocity.X = -2;
                if (Velocity.X >  2) Velocity.X =  2;
            } else {
                Velocity.Y += step;
                if (Velocity.Y < -2) Velocity.Y = -2;
                if (Velocity.Y >  2) Velocity.Y =  2;
            }
        }
    }

    if (Type != nullptr) {
        if (unknown_130 < Type->EndStateAI) {
            const int32 divisor = (FrameTimer::GetTime() & 1) + StateAIAdvance;
            if (divisor > 0 && (FrameTimer::GetTime() % divisor) == 0) {
                ++unknown_130;
            }
            if (unknown_130 == Type->EndStateAI && Type->DeleteOnStateLimit) {
                unknown_131 = 1;
            }
        }
        // 烟雾膨胀：随帧缓慢增大。
        Size_ *= 1.02f;
    }

    UpdateMotion();
}

// ------------------------------------------------------------------------
// asm: ParticleClass_Update_Fire (0x62E600)
//
//   火焰粒子每帧推进：
//     * 速度 > 0 时按 1 + (rand%10 - 5)*0.02 对扩散向量做抖动；
//     * 速度 <= 0 时把速度清零（燃尽）。
//     * 状态推进（除数 (RTTI_id & 1) + StateAIAdvance），并对 Translucent25/50
//       状态点设置 Translucency。
//     * 速度按 Deacc 衰减；RemainDC 归零时若 NumLoopFrames 有效且状态未越界，
//       对所在地块占据者做一次按距离衰减的伤害并重置 RemainDC。
// ------------------------------------------------------------------------
void ParticleClass::Update_Fire()
{
    const double speedMagnitude = std::sqrt(static_cast<double>(
        Velocity.X * Velocity.X + Velocity.Y * Velocity.Y + Velocity.Z * Velocity.Z));

    if (speedMagnitude <= 0.0) {
        Velocity = CoordStruct(0, 0, 0);
        if (Type != nullptr && unknown_130 >= Type->EndStateAI && Type->DeleteOnStateLimit) {
            unknown_131 = 1;
        }
        return;
    }

    UpdateMotion();
    Size_ *= 0.98f;

    if (Type == nullptr) {
        return;
    }

    // 状态推进（asm: idiv (RTTI_id & 1) + StateAIAdvance）。
    if (unknown_130 < Type->EndStateAI) {
        const int32 divisor = (FrameTimer::GetTime() & 1) + StateAIAdvance;
        if (divisor > 0 && (FrameTimer::GetTime() % divisor) == 0) {
            ++unknown_130;
            if (unknown_130 == Type->Translucent25State) {
                Translucency = 25;
            }
            if (unknown_130 == Type->Translucent50State) {
                Translucency = 50;
            }
        }
        if (unknown_130 == Type->EndStateAI && Type->DeleteOnStateLimit) {
            unknown_131 = 1;
        }
    }

    // 速度衰减（asm: fld Velocity / fsub Deacc）。
    if (Type->Deacc > 0.0) {
        const int32 d = static_cast<int32>(Type->Deacc);
        Velocity.X += (Velocity.X > 0 ? -d : d);
        Velocity.Y += (Velocity.Y > 0 ? -d : d);
    }

    // 周期性伤害（asm: dec RemainDC / 若归零且 NumLoopFrames 有效）。
    if (RemainingDC > 0) {
        --RemainingDC;
    }
    if (RemainingDC == 0 && Type->NumLoopFrames != 0
        && unknown_130 <= Type->EndStateAI && Type->Warhead != nullptr) {
        RemainingDC = static_cast<WORD>(Type->MaxDC);
        CellStruct cell = CoordMath::CoordToCell(Location);
        CellClass* pCell = (MapClass::Instance != nullptr)
            ? MapClass::Instance->GetCellAt(cell) : nullptr;
        if (pCell != nullptr) {
            for (ObjectClass* pObj = pCell->Get_Occupier();
                 pObj != nullptr; pObj = pObj->NextObject) {
                const AbstractType what = pObj->WhatAmI();
                if (what != AbstractType::Unit && what != AbstractType::Infantry
                    && what != AbstractType::Building && what != AbstractType::Aircraft) {
                    continue;
                }
                CoordStruct objCoord;
                pObj->GetCoords(&objCoord);
                const int32 dist = std::abs(objCoord.X - Location.X)
                                 + std::abs(objCoord.Y - Location.Y);
                int32 damage = dist / 10;   // asm: imul 66666667h / sar 2 → /10
                if (damage > 0) {
                    static_cast<TechnoClass*>(pObj)->ReceiveDamage(
                        &damage, 0, Type->Warhead, 0, 0, 0, nullptr);
                }
            }
        }
    }
}

// ------------------------------------------------------------------------
// asm: ParticleClass_Update_Railgun (0x62C400)
//
//   电磁轨道余辉：速度做 ±0.05 抖动后按方向推进位置，并推进颜色/帧计数
//   xyz_1：累积到 1.0 时帧号 +1、累加器清零；帧号接近 Type->NumLoopFrames
//   时饱和。到末态即收束。
// ------------------------------------------------------------------------
void ParticleClass::Update_Railgun()
{
    if (Type == nullptr) {
        UpdateMotion();
        return;
    }

    // 速度抖动（asm: (rand/0x7FFFFFFE - 0.5) * 0.1 累加到 Velocity）。
    const double jitter = (static_cast<double>(std::rand() % 0x7FFFFFFE) / 2147483646.0 - 0.5) * 0.1;
    const int32 jitterI = static_cast<int32>(jitter * 256.0);
    Velocity.X += jitterI;
    Velocity.Y += jitterI;
    Velocity.Z += jitterI;

    UpdateMotion();

    // 帧/颜色累积推进（asm: xyz_1.Y += ColorSpeed + rand factor）。
    if (unknown_double_D0 >= 1.0) {
        unknown_double_D0 -= 1.0;
        ++unknown_130;
        const int32 limit = Type->NumLoopFrames - 2;
        if (limit > 0 && unknown_130 >= limit) {
            unknown_131 = 1;   // 饱和收束
        }
    } else {
        unknown_double_D0 += (Type->ColorSpeed > 0.0 ? Type->ColorSpeed : 0.05);
    }

    // 余辉快速褪色。
    if (Translucency > 16) {
        Translucency = static_cast<BYTE>(Translucency - 16);
    } else {
        Translucency = 0;
    }
}

// ------------------------------------------------------------------------
// asm: ParticleClass_Update_Spark (0x62C7F0)
//
//   火花粒子：受重力下坠，位置积分后判定地面/障碍接触，命中即置收束
//   （unknown_131），并在末态按概率决定是否继续喷射。
// ------------------------------------------------------------------------
void ParticleClass::Update_Spark()
{
    // 重力下坠。
    Velocity.Z -= 12;
    UpdateMotion();

    if (Type == nullptr) {
        return;
    }

    // 地面/障碍接触判定：Z 落到地面高度以下即收束。
    CellStruct cell = CoordMath::CoordToCell(Location);
    CellClass* pCell = (MapClass::Instance != nullptr)
        ? MapClass::Instance->GetCellAt(cell) : nullptr;
    const int32 floorZ = (pCell != nullptr) ? pCell->GetFloorHeight(Location) : 0;
    if (Location.Z <= floorZ) {
        Location.Z = floorZ;
        unknown_131 = 1;   // 命中地面 → 收束
        return;
    }

    // 状态推进。
    if (unknown_130 < Type->EndStateAI) {
        const int32 divisor = (FrameTimer::GetTime() & 1) + StateAIAdvance;
        if (divisor > 0 && (FrameTimer::GetTime() % divisor) == 0) {
            ++unknown_130;
        }
        if (unknown_130 == Type->EndStateAI && Type->DeleteOnStateLimit) {
            unknown_131 = 1;
        }
    }

    // 末态：按概率结束（asm: Random2(0,0x7FFFFFFE) 判据）。
    if (unknown_130 >= Type->EndStateAI) {
        unknown_131 = 1;
    }
}

void ParticleClass::Update_2()
{
    if (!Type) {
        UpdateMotion();
        return;
    }
    // asm: ParticleClass_Update_2 (0x62E4xx) —— 二级推进按 BehavesLike 分派。
    switch (Type->BehavesLike) {
    case 0:  Update2_Smoke(); break;
    case 1:  Update2_Gas();   break;
    case 2:  Update_Spark();  break;
    default: UpdateMotion();  break;
    }
}

void ParticleClass::Update2_Gas()
{
    // asm: ParticleClass_Update2_Gas (0x62E3xx) —— 半寿期后膨胀。
    Update_Gas();
    if (MaxAge > 0 && Age * 2 >= MaxAge)
        Size_ *= 1.05f;
}

void ParticleClass::Update2_Smoke()
{
    // asm: ParticleClass_Update2_Smoke (0x62E4xx) —— 半寿期后转淡。
    Update_Smoke();
    if (MaxAge > 0 && Age * 2 >= MaxAge)
        FadeOut();
}

// ------------------------------------------------------------------------
// asm: ParticleClass_GetImageFrame (0x62DC40)
//
//   按 BehavesLike 取帧：
//     Smoke/Gas(0,1) → 随年龄线性推进帧序（Age*8/MaxAge）；
//     Fire(2)        → 首帧；
//     Spark(3)       → 按当前 AI 状态取帧；
//     Railgun(4)     → 按帧累加器取帧。
// ------------------------------------------------------------------------
int32 ParticleClass::GetImageFrame()
{
    if (!Type)
        return 0;
    switch (Type->BehavesLike) {
    case 0:
    case 1:
        return MaxAge > 0 ? (Age * 8) / MaxAge : 0;
    case 2:
        return 0;
    case 3:
        return unknown_130;
    case 4:
        return static_cast<int32>(unknown_double_D0 * 8.0);
    default:
        return 0;
    }
}
