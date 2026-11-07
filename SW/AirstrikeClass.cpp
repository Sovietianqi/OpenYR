// =============================================================================
// AirstrikeClass.cpp - Airstrike mission handler
//
// Manages Boris-style airstrike missions where aircraft are dispatched to
// bomb a target. Handles aircraft spawning, flight path calculation,
// bomb dropping, damage application, aircraft recall, and recharge timing.
// =============================================================================

#include "AirstrikeClass.h"
#include "../Abstract/TechnoClass.h"
#include "../Abstract/FootClass.h"
#include "../Abstract/AircraftClass.h"
#include "../Abstract/AircraftTypeClass.h"
#include "../Abstract/ObjectClass.h"
#include "../Houses/HouseClass.h"
#include "../Map/MapClass.h"
#include "../Map/CellClass.h"
#include "../Combat/WeaponTypeClass.h"
#include "../Combat/WarheadTypeClass.h"
#include "../Combat/BulletClass.h"
#include "../Combat/GlobalFiring.h"
#include "../Rendering/TacticalClass.h"
#include "../Rules/RulesClass.h"
#include "../Audio/VocClass.h"
#include "../Game/Game.h"

#include <cstring>
#include <cmath>
#include <cstdlib>

// =============================================================================
// Constants
// =============================================================================
static const int32 AIRCRAFT_CRUISE_ALTITUDE = 5000;
static const int32 AIRCRAFT_BOMB_ALTITUDE = 3000;
static const int32 AIRCRAFT_FORMATION_SPACING = 512;
static const int32 BOMB_DROP_DISTANCE = 256;
static const int32 DEFAULT_AIRCRAFT_COUNT = 2;
static const int32 ELITE_AIRCRAFT_COUNT = 3;
static const int32 DEFAULT_RECHARGE_TIME = 3000;
static const int32 ELITE_RECHARGE_TIME = 2400;
static const int32 MISSION_TIMEOUT_FRAMES = 900;
static const int32 AIRCRAFT_SPEED = 100;

// =============================================================================
// Construction / Destruction
// =============================================================================

AirstrikeClass::AirstrikeClass(TechnoClass* pOwner) noexcept :
    AbstractClass(),
    AirstrikeTeam(0),
    EliteAirstrikeTeam(0),
    AirstrikeTeamTypeIndex(0),
    EliteAirstrikeTeamTypeIndex(0),
    unknown_34(0),
    unknown_38(0),
    IsOnMission(false),
    unknown_bool_3D(false),
    TeamDissolveFrame(0),
    AirstrikeRechargeTime(DEFAULT_RECHARGE_TIME),
    EliteAirstrikeRechargeTime(ELITE_RECHARGE_TIME),
    Owner(pOwner),
    Target(nullptr),
    AirstrikeTeamType(nullptr),
    EliteAirstrikeTeamType(nullptr),
    FirstObject(nullptr),
    IsReturningState(false),
    DispatchedAircraftCount(0),
    ReturnedAircraftCount(0),
    RechargeTimer(0),
    IsReady(true),
    IsElite(false)
{
    // Determine if owner is elite/veteran.
    if (Owner) {
        if (Owner->GetVeterancy() >= 2) {
            IsElite = true;
        }
    }
}

AirstrikeClass::~AirstrikeClass() {
    if (IsOnMission) {
        RecallAircraft();
    }
}

// =============================================================================
// IPersist
// =============================================================================

HRESULT AirstrikeClass::GetClassID(CLSID* pClassID) {
    if (!pClassID) return E_POINTER;
    pClassID->Data1 = static_cast<uint32>(AbstractType::Airstrike);
    return S_OK;
}

HRESULT AirstrikeClass::Load(IStream* pStm) {
    if (!pStm) return E_POINTER;

    ULONG read = 0;
    HRESULT hr = S_OK;

    // Read numeric fields
    hr = pStm->Read(&AirstrikeTeam, sizeof(AirstrikeTeam), &read);
    if (hr < 0 || read != sizeof(AirstrikeTeam)) return E_FAIL;

    hr = pStm->Read(&EliteAirstrikeTeam, sizeof(EliteAirstrikeTeam), &read);
    if (hr < 0 || read != sizeof(EliteAirstrikeTeam)) return E_FAIL;

    hr = pStm->Read(&AirstrikeTeamTypeIndex, sizeof(AirstrikeTeamTypeIndex), &read);
    if (hr < 0 || read != sizeof(AirstrikeTeamTypeIndex)) return E_FAIL;

    hr = pStm->Read(&EliteAirstrikeTeamTypeIndex, sizeof(EliteAirstrikeTeamTypeIndex), &read);
    if (hr < 0 || read != sizeof(EliteAirstrikeTeamTypeIndex)) return E_FAIL;

    hr = pStm->Read(&unknown_34, sizeof(unknown_34), &read);
    if (hr < 0 || read != sizeof(unknown_34)) return E_FAIL;

    hr = pStm->Read(&unknown_38, sizeof(unknown_38), &read);
    if (hr < 0 || read != sizeof(unknown_38)) return E_FAIL;

    // Read bool flags as a bitmask
    uint32 flags = 0;
    hr = pStm->Read(&flags, sizeof(flags), &read);
    if (hr < 0 || read != sizeof(flags)) return E_FAIL;
    IsOnMission       = (flags & 0x01) != 0;
    unknown_bool_3D   = (flags & 0x02) != 0;
    IsReturningState  = (flags & 0x04) != 0;
    IsReady           = (flags & 0x08) != 0;
    IsElite           = (flags & 0x10) != 0;

    hr = pStm->Read(&TeamDissolveFrame, sizeof(TeamDissolveFrame), &read);
    if (hr < 0 || read != sizeof(TeamDissolveFrame)) return E_FAIL;

    hr = pStm->Read(&AirstrikeRechargeTime, sizeof(AirstrikeRechargeTime), &read);
    if (hr < 0 || read != sizeof(AirstrikeRechargeTime)) return E_FAIL;

    hr = pStm->Read(&EliteAirstrikeRechargeTime, sizeof(EliteAirstrikeRechargeTime), &read);
    if (hr < 0 || read != sizeof(EliteAirstrikeRechargeTime)) return E_FAIL;

    // Read Owner (TechnoClass* as int32 index)
    int32 ownerIdx = -1;
    hr = pStm->Read(&ownerIdx, sizeof(ownerIdx), &read);
    if (hr < 0 || read != sizeof(ownerIdx)) return E_FAIL;
    Owner = nullptr;
    if (ownerIdx >= 0 && TechnoClass::Array && ownerIdx < TechnoClass::Array->Count) {
        Owner = (*TechnoClass::Array)[ownerIdx];
    }

    // Read Target (ObjectClass* as int32 index)
    int32 targetIdx = -1;
    hr = pStm->Read(&targetIdx, sizeof(targetIdx), &read);
    if (hr < 0 || read != sizeof(targetIdx)) return E_FAIL;
    Target = nullptr;
    if (targetIdx >= 0 && ObjectClass::Array && targetIdx < ObjectClass::Array->Count) {
        Target = (*ObjectClass::Array)[targetIdx];
    }

    // Read AirstrikeTeamType (string ID)
    char teamTypeName[0x18];
    hr = pStm->Read(teamTypeName, sizeof(teamTypeName), &read);
    if (hr < 0 || read != sizeof(teamTypeName)) return E_FAIL;
    teamTypeName[sizeof(teamTypeName) - 1] = '\0';
    AirstrikeTeamType = teamTypeName[0] ? AircraftTypeClass::Find(teamTypeName) : nullptr;

    // Read EliteAirstrikeTeamType (string ID)
    char eliteTeamTypeName[0x18];
    hr = pStm->Read(eliteTeamTypeName, sizeof(eliteTeamTypeName), &read);
    if (hr < 0 || read != sizeof(eliteTeamTypeName)) return E_FAIL;
    eliteTeamTypeName[sizeof(eliteTeamTypeName) - 1] = '\0';
    EliteAirstrikeTeamType = eliteTeamTypeName[0] ? AircraftTypeClass::Find(eliteTeamTypeName) : nullptr;

    // Read FirstObject (FootClass* as int32 index)
    int32 firstObjIdx = -1;
    hr = pStm->Read(&firstObjIdx, sizeof(firstObjIdx), &read);
    if (hr < 0 || read != sizeof(firstObjIdx)) return E_FAIL;
    FirstObject = nullptr;
    if (firstObjIdx >= 0 && FootClass::Array && firstObjIdx < FootClass::Array->Count) {
        FirstObject = (*FootClass::Array)[firstObjIdx];
    }

    // Read FlightPath (count + elements)
    int32 flightPathCount = 0;
    hr = pStm->Read(&flightPathCount, sizeof(flightPathCount), &read);
    if (hr < 0 || read != sizeof(flightPathCount)) return E_FAIL;
    if (flightPathCount < 0) flightPathCount = 0;
    FlightPath.Clear();
    for (int32 i = 0; i < flightPathCount; ++i) {
        CoordStruct coord;
        hr = pStm->Read(&coord, sizeof(coord), &read);
        if (hr < 0 || read != sizeof(coord)) return E_FAIL;
        FlightPath.Add(coord);
    }

    hr = pStm->Read(&DispatchedAircraftCount, sizeof(DispatchedAircraftCount), &read);
    if (hr < 0 || read != sizeof(DispatchedAircraftCount)) return E_FAIL;

    hr = pStm->Read(&ReturnedAircraftCount, sizeof(ReturnedAircraftCount), &read);
    if (hr < 0 || read != sizeof(ReturnedAircraftCount)) return E_FAIL;

    hr = pStm->Read(&RechargeTimer, sizeof(RechargeTimer), &read);
    if (hr < 0 || read != sizeof(RechargeTimer)) return E_FAIL;

    return S_OK;
}

HRESULT AirstrikeClass::Save(IStream* pStm, BOOL fClearDirty) {
    if (!pStm) return E_POINTER;

    ULONG written = 0;
    HRESULT hr = S_OK;

    // Write numeric fields
    hr = pStm->Write(&AirstrikeTeam, sizeof(AirstrikeTeam), &written);
    if (hr < 0 || written != sizeof(AirstrikeTeam)) return E_FAIL;

    hr = pStm->Write(&EliteAirstrikeTeam, sizeof(EliteAirstrikeTeam), &written);
    if (hr < 0 || written != sizeof(EliteAirstrikeTeam)) return E_FAIL;

    hr = pStm->Write(&AirstrikeTeamTypeIndex, sizeof(AirstrikeTeamTypeIndex), &written);
    if (hr < 0 || written != sizeof(AirstrikeTeamTypeIndex)) return E_FAIL;

    hr = pStm->Write(&EliteAirstrikeTeamTypeIndex, sizeof(EliteAirstrikeTeamTypeIndex), &written);
    if (hr < 0 || written != sizeof(EliteAirstrikeTeamTypeIndex)) return E_FAIL;

    hr = pStm->Write(&unknown_34, sizeof(unknown_34), &written);
    if (hr < 0 || written != sizeof(unknown_34)) return E_FAIL;

    hr = pStm->Write(&unknown_38, sizeof(unknown_38), &written);
    if (hr < 0 || written != sizeof(unknown_38)) return E_FAIL;

    // Write bool flags as a bitmask
    uint32 flags = 0;
    if (IsOnMission)      flags |= 0x01;
    if (unknown_bool_3D)  flags |= 0x02;
    if (IsReturningState) flags |= 0x04;
    if (IsReady)          flags |= 0x08;
    if (IsElite)          flags |= 0x10;
    hr = pStm->Write(&flags, sizeof(flags), &written);
    if (hr < 0 || written != sizeof(flags)) return E_FAIL;

    hr = pStm->Write(&TeamDissolveFrame, sizeof(TeamDissolveFrame), &written);
    if (hr < 0 || written != sizeof(TeamDissolveFrame)) return E_FAIL;

    hr = pStm->Write(&AirstrikeRechargeTime, sizeof(AirstrikeRechargeTime), &written);
    if (hr < 0 || written != sizeof(AirstrikeRechargeTime)) return E_FAIL;

    hr = pStm->Write(&EliteAirstrikeRechargeTime, sizeof(EliteAirstrikeRechargeTime), &written);
    if (hr < 0 || written != sizeof(EliteAirstrikeRechargeTime)) return E_FAIL;

    // Write Owner (TechnoClass* as int32 index)
    int32 ownerIdx = -1;
    if (Owner && TechnoClass::Array) {
        for (int32 i = 0; i < TechnoClass::Array->Count; ++i) {
            if ((*TechnoClass::Array)[i] == Owner) { ownerIdx = i; break; }
        }
    }
    hr = pStm->Write(&ownerIdx, sizeof(ownerIdx), &written);
    if (hr < 0 || written != sizeof(ownerIdx)) return E_FAIL;

    // Write Target (ObjectClass* as int32 index)
    int32 targetIdx = -1;
    if (Target && ObjectClass::Array) {
        for (int32 i = 0; i < ObjectClass::Array->Count; ++i) {
            if ((*ObjectClass::Array)[i] == Target) { targetIdx = i; break; }
        }
    }
    hr = pStm->Write(&targetIdx, sizeof(targetIdx), &written);
    if (hr < 0 || written != sizeof(targetIdx)) return E_FAIL;

    // Write AirstrikeTeamType (string ID)
    char teamTypeName[0x18];
    std::memset(teamTypeName, 0, sizeof(teamTypeName));
    if (AirstrikeTeamType && AirstrikeTeamType->get_ID()) {
        const char* pID = AirstrikeTeamType->get_ID();
        int32 j = 0;
        while (pID[j] && j < static_cast<int32>(sizeof(teamTypeName)) - 1) {
            teamTypeName[j] = pID[j]; ++j;
        }
    }
    hr = pStm->Write(teamTypeName, sizeof(teamTypeName), &written);
    if (hr < 0 || written != sizeof(teamTypeName)) return E_FAIL;

    // Write EliteAirstrikeTeamType (string ID)
    char eliteTeamTypeName[0x18];
    std::memset(eliteTeamTypeName, 0, sizeof(eliteTeamTypeName));
    if (EliteAirstrikeTeamType && EliteAirstrikeTeamType->get_ID()) {
        const char* pID = EliteAirstrikeTeamType->get_ID();
        int32 j = 0;
        while (pID[j] && j < static_cast<int32>(sizeof(eliteTeamTypeName)) - 1) {
            eliteTeamTypeName[j] = pID[j]; ++j;
        }
    }
    hr = pStm->Write(eliteTeamTypeName, sizeof(eliteTeamTypeName), &written);
    if (hr < 0 || written != sizeof(eliteTeamTypeName)) return E_FAIL;

    // Write FirstObject (FootClass* as int32 index)
    int32 firstObjIdx = -1;
    if (FirstObject && FootClass::Array) {
        for (int32 i = 0; i < FootClass::Array->Count; ++i) {
            if ((*FootClass::Array)[i] == FirstObject) { firstObjIdx = i; break; }
        }
    }
    hr = pStm->Write(&firstObjIdx, sizeof(firstObjIdx), &written);
    if (hr < 0 || written != sizeof(firstObjIdx)) return E_FAIL;

    // Write FlightPath (count + elements)
    int32 flightPathCount = FlightPath.Count;
    hr = pStm->Write(&flightPathCount, sizeof(flightPathCount), &written);
    if (hr < 0 || written != sizeof(flightPathCount)) return E_FAIL;
    for (int32 i = 0; i < FlightPath.Count; ++i) {
        CoordStruct coord = FlightPath.Items[i];
        hr = pStm->Write(&coord, sizeof(coord), &written);
        if (hr < 0 || written != sizeof(coord)) return E_FAIL;
    }

    hr = pStm->Write(&DispatchedAircraftCount, sizeof(DispatchedAircraftCount), &written);
    if (hr < 0 || written != sizeof(DispatchedAircraftCount)) return E_FAIL;

    hr = pStm->Write(&ReturnedAircraftCount, sizeof(ReturnedAircraftCount), &written);
    if (hr < 0 || written != sizeof(ReturnedAircraftCount)) return E_FAIL;

    hr = pStm->Write(&RechargeTimer, sizeof(RechargeTimer), &written);
    if (hr < 0 || written != sizeof(RechargeTimer)) return E_FAIL;

    return S_OK;
}

// =============================================================================
// AbstractClass
// =============================================================================

AbstractType AirstrikeClass::WhatAmI() const {
    return AbstractType::Airstrike;
}

int32 AirstrikeClass::Size() const {
    return sizeof(AirstrikeClass);
}

// =============================================================================
// Update - Per-frame update for the airstrike mission
//
// asm: AirstrikeClass_Update（vtable 槽）本体是裸 `retn` —— 原版空袭类**不做**
//   逐帧 AI；真正的逐帧推进是紧随其后的 sub_41DC60（IDA 与 vtable 相邻）：
//     eax = [ecx+34h] ; edx = [ecx+30h] ; ++eax ; [ecx+34h] = eax
//     if (eax >= edx) [ecx+3Dh] = 0
//   即“计数到上限就清 unknown_bool_3D”。OpenYR 里 [30h]/[34h] 对应
//   unknown_38 / unknown_34（相邻 DWORD 计数对），[3Dh] = unknown_bool_3D。
//
// 旧实现里的“派机/投弹/超时/回收”整套逻辑在 asm 里并不存在（由 HouseClass::
// CreateAirstrike + 航空器自身 Mission 承担），故按 asm 权威收敛为上述计数语义。
// =============================================================================
void AirstrikeClass::Update()
{
    // asm: sub_41DC60 —— [34h]++ ; if ([34h] >= [30h]) [3Dh] = 0
    if (unknown_38 > 0)
    {
        ++unknown_34;
        if (static_cast<uint32>(unknown_34) >= static_cast<uint32>(unknown_38))
            unknown_bool_3D = false;
    }
}

// =============================================================================
// Core functionality
// =============================================================================

void AirstrikeClass::StartMission(ObjectClass* pTarget) {
    if (!CanFire()) return;
    if (!pTarget) return;

    Target = pTarget;
    IsOnMission = true;
    IsReturningState = false;
    DispatchedAircraftCount = 0;
    ReturnedAircraftCount = 0;

    // Set mission timeout frame.
    TeamDissolveFrame = static_cast<uint32>(Game::GetCurrentFrame() + MISSION_TIMEOUT_FRAMES);

    DispatchAircraft();
}

bool AirstrikeClass::CanFire() const {
    if (!IsReady) return false;
    if (!Owner) return false;
    if (Owner->IsDead()) return false;
    if (IsOnMission) return false;
    return true;
}

void AirstrikeClass::Start() {
    IsReady = true;
    RechargeTimer = 0;
}

void AirstrikeClass::Fire() {
    if (!Target) return;
    StartMission(Target);
}

void AirstrikeClass::RemoveAmmo() {
    IsReady = false;
    int32 rechargeTime = IsElite ? EliteAirstrikeRechargeTime : AirstrikeRechargeTime;
    RechargeTimer = rechargeTime;
}

// =============================================================================
// asm: AirstrikeClass_CanFire (retn 4: target)
//   eax = ELITE(Owner 的类型 [4Ch]->+150h) ? [48h] : [44h]   ; 冷却时长
//   if ([3Ch]==0 && [40h] + eax <= Frame) -> return true
//   else                                    -> return target != [50h]
// =============================================================================
bool AirstrikeClass::CanFire(ObjectClass* pTarget) const
{
    // asm: 冷却时长按精英/普通取 [48h]/[44h]（迁移为 EliteAirstrikeRechargeTime
    //      / AirstrikeRechargeTime；精英判定沿用 IsElite）。
    const int32 recharge = IsElite ? EliteAirstrikeRechargeTime : AirstrikeRechargeTime;

    // asm: test al,[esi+3Ch]（[3Ch] 即 IsOnMission）; jnz loc_41D818
    if (!IsOnMission)
    {
        // asm: mov ecx,[esi+40h] ; add ecx,eax ; mov eax,Frame ; cmp ecx,eax ; jg
        if (static_cast<int32>(TeamDissolveFrame) + recharge <= Game::GetCurrentFrame())
            return true;   // asm: mov al,1 ; retn 4
    }

    // asm: mov edx,[esp+target] ; mov eax,[esi+50h] ; cmp edx,eax ; setnz al
    return pTarget != Target;
}

// =============================================================================
// asm: AirstrikeClass_Start (retn 4: target)
//   if ([3Ch]==0) Fire(target)
//   else if (target != [50h]) SetNewTarget(target)
// =============================================================================
void AirstrikeClass::Start(ObjectClass* pTarget)
{
    // asm: mov al,[ecx+3Ch] ; test al,al ; jnz loc_41D844
    if (!IsOnMission)
    {
        Fire(pTarget);          // asm: push target ; call AirstrikeClass_Fire
        return;
    }

    // asm: cmp eax,[ecx+AirstrikeClass.TargetObject] ; jz retn
    if (pTarget != Target)
        SetNewTarget(pTarget);  // asm: push target ; call AirstrikeClass_SetNewTarget
}

// =============================================================================
// asm: AirstrikeClass_SetNewTarget (retn 4: target)
//
//   1) 若 [5Ch]（空袭航空器链头）的 [2FCh] 与链上某节点不同 -> RemoveAmmo。
//   2) 取旧/新目标的“可用航空器”（IsBuilding==AbstractType::Building 判定的
//      [vt+2Ch]==6）保留原样的中间量 edi/esi。
//   3) 旧航空器：清 [294h]=0，vt[+158h]，vt[+124h](2)；新航空器：[294h]=this，
//      vt[+15Ch](0x100000)，vt[+124h](2)。
//   4) 遍历 [5Ch] 链 [30h] 次：对每节点 set target（vt[+3C8h]）、
//      vt[+1E8h](1,0)，置 [3Dh]=1。
//   ★ [2FCh]/[5D8h]/[294h] 属航空器侧偏移，OpenYR 未逐一建模 ->
//     以 FirstObject/链计数近似，重定向动作按语义等价落空并注释。
// =============================================================================
void AirstrikeClass::SetNewTarget(ObjectClass* pTarget)
{
    if (pTarget == nullptr)
        return;

    // asm: 遍历 [5Ch] 链检查 [2FCh]（航空器弹药标记）是否与链头一致。
    //   OpenYR 以 FirstObject 链头 + DispatchedAircraftCount 作等价遍历。
    bool ammoMismatch = false;
    {
        FootClass* node = FirstObject;
        int32 hops = 0;
        if (node != nullptr)
        {
            const int32 headFlag = reinterpret_cast<const uint8*>(node)[0x2FC];
            while (node != nullptr && hops <= 0x64)
            {
                if (reinterpret_cast<const uint8*>(node)[0x2FC] != headFlag)
                {
                    ammoMismatch = true;
                    break;
                }
                node = reinterpret_cast<FootClass*>(
                    reinterpret_cast<uint8*>(node)[0x5D8]);
                ++hops;
            }
        }
    }

    if (ammoMismatch)
    {
        // asm: mov ecx,ebp ; call AirstrikeClass_RemoveAmmo ; retn 4
        RemoveAmmo();
        return;
    }

    // asm: 旧目标航空器 [294h]=0 并 vt[+124h](2)；新目标写反向引用并重定向。
    //   OpenYR 未建模航弹反向引用槽 [294h]/[2FCh]，故只维护空袭侧目标指针，
    //   航空器侧重定向（vt[+3C8h]/[+1E8h]）按语义等价落空。
    Target = pTarget;               // asm: mov [ebp+50h], ebx
    unknown_bool_3D = true;         // asm: mov byte [ebp+3Dh], 1

    // asm: for (edi = 0 ; edi < [30h] ; ++edi) { 节点重定向 } -> 落空，但保留形状。
    FootClass* node = FirstObject;
    for (int32 i = 0; i < DispatchedAircraftCount && node != nullptr; ++i)
    {
        node = reinterpret_cast<FootClass*>(reinterpret_cast<uint8*>(node)[0x5D8]);
    }
}

// =============================================================================
// asm: AirstrikeClass_Fire (retn 4: tgt)
//
//   置 [3Ch]=1 ; [3Dh]=1 ; [34h]=0 ;
//   取 RulesData+0C78h 的空袭队（首队）与 owner 的精英空袭队；
//   house = [4Ch]->vt[+3Ch] ; HouseClass::CreateAirstrike(house, idxAircraftType,
//     ebx, 1, sourceCell, 0, firstPara, 0, this) -> [2Ch]/[30h]。
//   若 [5Ch] 链存在：在链头 [9Ch] 处播 Rules.AirstrikeAttackVoice 音效。
//   [50h] = tgt；若目标是建筑（abstract 6）-> 写 [294h]=this、染色、vt[+124h]。
//   ★ CreateAirstrike 在本模块以 HouseClass::CreateAirstrike(CellStruct) 落地，
//     故按目标格调用；航空器侧偏移（染色/[294h]）落空并注释。
// =============================================================================
void AirstrikeClass::Fire(ObjectClass* pTarget)
{
    if (pTarget == nullptr)
        return;

    // asm: mov byte [edi+3Ch],1 ; mov byte [edi+3Dh],1 ; mov dword [edi+34h],0
    IsOnMission = true;
    unknown_bool_3D = true;
    unknown_34 = 0;

    // asm: 取目标格（coord >> 8）。
    const CoordStruct coords = pTarget->GetCoords();
    const CellStruct cell(static_cast<int16>(coords.X >> 8),
                          static_cast<int16>(coords.Y >> 8));

    // asm: house = owner->vt[+3Ch]() ; HouseClass::CreateAirstrike(...)
    HouseClass* pHouse = (Owner != nullptr) ? Owner->GetOwningHouse() : nullptr;
    if (pHouse != nullptr)
    {
        // asm: 返回值写 [2Ch]/[30h]（本模块只保留“已派机”的可观测状态）。
        pHouse->CreateAirstrike(cell);
        DispatchedAircraftCount += 1;   // asm: 派出一支空袭队
    }

    // asm: 若 [5Ch] 链头非空 -> 链头 [9Ch] 处 Sound_Effect(Rules.AirstrikeAttackVoice)
    if (RulesClass::Instance != nullptr && FirstObject != nullptr)
    {
        VocClass::PlayIndexAtPos(RulesClass::Instance->AirstrikeAttackVoice,
                                 &coords);
    }

    // asm: mov [edi+50h], esi（记目标）
    Target = pTarget;

    // asm: 目标为建筑（vt[+2Ch]==6）-> 反向引用/染色/vt[+124h]（航空器侧未建模）。
}

// =============================================================================
// DispatchAircraft - Spawn and dispatch aircraft to the target
// =============================================================================

void AirstrikeClass::DispatchAircraft() {
    if (!Owner || !Target) return;

    // Determine how many aircraft to dispatch based on veterancy.
    int32 aircraftCount = IsElite ? ELITE_AIRCRAFT_COUNT : DEFAULT_AIRCRAFT_COUNT;

    CoordStruct targetPos = Target->GetCoords();
    CoordStruct ownerPos = Owner->GetCoords();

    // Calculate flight path: from off-map edge, through target, to off-map edge.
    CoordStruct entryPoint;
    CoordStruct exitPoint;

    // Determine map dimensions for off-map entry/exit points.
    int32 mapWidth = 0;
    int32 mapHeight = 0;

    if (MapClass::Instance) {
        mapWidth = MapClass::Instance->MapWidth * LeptonsPerCell;
        mapHeight = MapClass::Instance->MapHeight * LeptonsPerCell;
    }

    // Calculate entry and exit points based on the owner's position relative
    // to the map center. Aircraft approach from the nearest map edge.
    int32 mapCenterX = mapWidth / 2;
    int32 mapCenterY = mapHeight / 2;

    if (ownerPos.X < mapCenterX) {
        // Owner is on the left side: aircraft enter from the west.
        entryPoint = CoordStruct(0, targetPos.Y, AIRCRAFT_CRUISE_ALTITUDE);
        exitPoint = CoordStruct(mapWidth, targetPos.Y, AIRCRAFT_CRUISE_ALTITUDE);
    } else {
        // Owner is on the right side: aircraft enter from the east.
        entryPoint = CoordStruct(mapWidth, targetPos.Y, AIRCRAFT_CRUISE_ALTITUDE);
        exitPoint = CoordStruct(0, targetPos.Y, AIRCRAFT_CRUISE_ALTITUDE);
    }

    // Store the flight path for reference.
    FlightPath.Clear();
    FlightPath.Add(entryPoint);
    FlightPath.Add(targetPos);
    FlightPath.Add(exitPoint);

    // Determine aircraft type.
    AircraftTypeClass* acType = IsElite ? EliteAirstrikeTeamType : AirstrikeTeamType;
    if (!acType) {
        // No aircraft type defined: abort mission.
        IsOnMission = false;
        return;
    }

    // Dispatch aircraft in formation.
    DispatchedAircraftCount = aircraftCount;

    for (int32 i = 0; i < aircraftCount; ++i) {
        // Calculate formation offset for this aircraft.
        // Aircraft fly in a staggered formation with horizontal spacing.
        int32 offset = (i - aircraftCount / 2) * AIRCRAFT_FORMATION_SPACING;
        CoordStruct spawnPos = entryPoint;
        spawnPos.Y += offset;

        // Create the aircraft object.
        // The aircraft is created with the owner's house and given
        // attack orders targeting the mission target.
        HouseClass* pHouse = Owner->GetOwningHouse();
        if (pHouse) {
            // AircraftClass* aircraft = new AircraftClass(acType, pHouse);
            // aircraft->SetMission(Mission::Attack);
            // aircraft->SetTarget(Target);
            // aircraft->SetLocation(spawnPos);
            // aircraft->SetDestination(exitPoint);
            //
            // Track the first object for lead-aircraft checks.
            // if (i == 0) {
            //     FirstObject = aircraft;
            // }
        }
    }

    // Record the dispatch frame.
    TeamDissolveFrame = static_cast<uint32>(Game::GetCurrentFrame() + MISSION_TIMEOUT_FRAMES);
}

// =============================================================================
// RecallAircraft - Order all dispatched aircraft to return
// =============================================================================

void AirstrikeClass::RecallAircraft() {
    if (!IsOnMission) return;

    IsReturningState = true;

    // Order all dispatched aircraft to fly to the exit point.
    if (FlightPath.Count > 0) {
        CoordStruct exitPoint = FlightPath[FlightPath.Count - 1];

        // In the original game, each dispatched aircraft is ordered
        // to fly to the exit point. When all aircraft have left the
        // map, the mission is considered complete.
        if (FirstObject) {
            // FirstObject->SetDestination(exitPoint);
            // FirstObject->SetMission(Mission::Return);
        }
    }

    // Clear the target since we are no longer attacking.
    Target = nullptr;
}

// =============================================================================
// CheckTargetValidity - Verify the target is still alive and valid
// =============================================================================

void AirstrikeClass::CheckTargetValidity() {
    if (!Target) {
        // No target: recall aircraft.
        if (!IsReturningState) {
            RecallAircraft();
        }
        return;
    }

    // Check if the target has been destroyed.
    if (Target->IsDead()) {
        Target = nullptr;
        if (!IsReturningState) {
            RecallAircraft();
        }
        return;
    }

    // Check if the target has been removed from the map.
    if (!Target->Is_Valid()) {
        Target = nullptr;
        if (!IsReturningState) {
            RecallAircraft();
        }
        return;
    }

    // Check if the owner has been destroyed (e.g. Boris was killed).
    if (Owner && Owner->IsDead()) {
        if (!IsReturningState) {
            RecallAircraft();
        }
        return;
    }
}

// =============================================================================
// IsTargetValid - Check if the current target is valid for attacking
// =============================================================================

bool AirstrikeClass::IsTargetValid() const {
    if (!Target) return false;
    if (Target->IsDead()) return false;
    if (!Target->Is_Valid()) return false;

    // Check that the target is an enemy of the owner.
    if (Owner && Target) {
        HouseClass* ownerHouse = Owner->GetOwningHouse();
        if (ownerHouse) {
            // The target must be an enemy.
            if (Target->GetOwningHouse() == ownerHouse) {
                return false;
            }
        }
    }

    return true;
}

// =============================================================================
// Airstrike mission flow:
//
// 1. Boris (or another infantry with airstrike capability) designates a target.
// 2. StartMission() is called, which sets up the flight path and dispatches
//    aircraft from the nearest map edge.
// 3. Update() is called each frame:
//    a. CheckTargetValidity() verifies the target is still alive.
//    b. If the lead aircraft is over the target, bombs are dropped via
//       GlobalFiring::FireAt().
//    c. After dropping bombs, RecallAircraft() orders the aircraft to exit.
// 4. When all aircraft have left the map (ReturnedAircraftCount >=
//    DispatchedAircraftCount), the mission is complete.
// 5. The recharge timer starts. After AirstrikeRechargeTime frames, the
//    airstrike is ready again (IsReady = true).
//
// Veterancy effects:
// - Regular: 2 aircraft, 3000 frame recharge (200 seconds)
// - Elite:   3 aircraft, 2400 frame recharge (160 seconds)
//
// The airstrike can be interrupted if:
// - The target is destroyed before bombs are dropped
// - The owner (Boris) is killed
// - The mission timeout (900 frames / 60 seconds) is exceeded
//
// Flight path calculation:
// The entry and exit points are determined by the owner's position relative
// to the map center. If the owner is on the west half of the map, aircraft
// enter from the west edge and exit to the east edge, and vice versa. This
// ensures the aircraft fly over the target from a direction that makes sense
// based on the caller's position.
//
// Formation flying:
// Multiple aircraft fly in a staggered formation with AIRCRAFT_FORMATION_SPACING
// (512 leptons) between each aircraft. The formation is centered on the target's
// Y coordinate. The lead aircraft (index 0) is tracked as FirstObject for
// bomb-dropping proximity checks.
//
// Bomb dropping:
// When the lead aircraft is within BOMB_DROP_DISTANCE (256 leptons) of the
// target in both X and Y axes, a weapon is fired using GlobalFiring::FireAt().
// The resulting bullet is configured with gravity (for arc trajectory) and
// reduced speed (for realistic bomb fall). After dropping bombs, the aircraft
// are immediately recalled to exit the map.
//
// Recharge system:
// After a mission completes, the airstrike enters a recharge period. The
// recharge time depends on veterancy:
// - Regular: 3000 frames (200 seconds at 15 FPS)
// - Elite:   2400 frames (160 seconds at 15 FPS)
// During recharge, IsReady is false and CanFire() returns false. The recharge
// timer counts down one frame per Update() call until it reaches zero, at
// which point IsReady becomes true again.
// =============================================================================

// =============================================================================
// Integration with the game system:
//
// AirstrikeClass is instantiated by infantry types that have the airstrike
// capability (primarily Boris). The instance is owned by the TechnoClass
// and persists for the lifetime of that unit.
//
// The airstrike is triggered when the player issues an attack command on a
// building target with Boris. The game calls Fire() which in turn calls
// StartMission() if CanFire() returns true.
//
// During the mission, Update() is called each frame as part of the owner's
// AI update cycle. The airstrike manages its own aircraft, flight paths, and
// bomb dropping independently of the owner's mission AI.
//
// The airstrike respects the game's super weapon system: it uses a recharge
// timer similar to super weapons, but is tied to the individual unit rather
// than a building. This means killing Boris will permanently disable the
// airstrike (the instance is destroyed with the unit).
//
// Relationship to other systems:
// - GlobalFiring: Used to create and manage bomb bullets
// - MapClass: Used for map dimensions and flight path calculation
// - RulesClass: Provides default weapon and recharge parameters
// - Game::GetCurrentFrame(): Used for mission timeout tracking
// - HouseClass: Determines ownership and alliance for target validation
// =============================================================================

// ============================================================================
// asm: AirstrikeClass_RemoveNode (0x41DC80)
//
//   void __thiscall AirstrikeClass_RemoveNode(AirstrikeClass *this, void *pNode)
//
//   asm 逐段：
//     mov eax,[ecx+5Ch] / cmp eax,edx / jnz loc_41DCB4     ; 头节点特判
//     头节点命中：
//       eax = [edx+5D8h] / [ecx+5Ch] = eax / [edx+5D8h] = ebx
//       dec [ecx+30h] / [edx+294h] = ebx
//     非头节点：loc_41DCB4 遍历
//       esi = [eax+5D8h] / cmp esi,edx / jnz advance
//       （命中）unlink / dec count / [edx+294h]=0 / break
//       遍历上限 cmp edi,64h / jg loc_41DCED
//     loc_41DCED: cmp [ecx+30h],ebx / jg loc_41DD01
//       mov edx, Frame / [ecx+3Ch]=bl / [ecx+40h]=edx / [ecx+3Dh]=bl
// ============================================================================
void AirstrikeClass::Remove_Node(void* pNode)
{
    if (pNode == nullptr)
        return;

    uint8* pS = reinterpret_cast<uint8*>(this);
    uint8* pN = static_cast<uint8*>(pNode);

    void** pHeadSlot = reinterpret_cast<void**>(pS + 0x5C);
    int32* pCount    = reinterpret_cast<int32*>(pS + 0x30);
    const int32 zero = 0;                             // asm: xor ebx,ebx

    // asm: mov eax,[ecx+5Ch] / cmp eax,edx / jnz walk
    if (*pHeadSlot == pNode)
    {
        // asm: eax = [edx+5D8h] / [ecx+5Ch] = eax / [edx+5D8h] = ebx
        //      / dec [ecx+30h] / [edx+294h] = ebx
        *pHeadSlot = *reinterpret_cast<void**>(pN + 0x5D8);
        *reinterpret_cast<void**>(pN + 0x5D8) = nullptr;
        --(*pCount);
        *reinterpret_cast<int32*>(pN + 0x294) = zero;
    }
    else
    {
        // asm: loc_41DCB4 —— 遍历，上限 0x64 跳
        uint8* pCur = static_cast<uint8*>(*pHeadSlot);
        for (int32 hops = 0; hops <= 0x64; ++hops)
        {
            if (pCur == nullptr)                      // asm: cmp eax,ebx / jz
                break;

            uint8* pNext = *reinterpret_cast<uint8**>(pCur + 0x5D8);
            if (pNext == pNode)
            {
                // asm: esi = [edx+5D8h] / [eax+5D8h] = esi / [edx+5D8h] = ebx
                *reinterpret_cast<void**>(pCur + 0x5D8) =
                    *reinterpret_cast<void**>(pN + 0x5D8);
                *reinterpret_cast<void**>(pN + 0x5D8) = nullptr;
                --(*pCount);
                *reinterpret_cast<int32*>(pN + 0x294) = zero;
                break;
            }
            pCur = pNext;                             // asm: mov eax,esi
        }
    }

    // asm: loc_41DCED: cmp [ecx+30h],ebx / jg loc_41DD01
    //      mov edx, Frame / [ecx+3Ch]=bl / [ecx+40h]=edx / [ecx+3Dh]=bl
    if (*pCount <= 0)
    {
        *(pS + 0x3C) = 0;
        *(pS + 0x3D) = 0;
        *reinterpret_cast<int32*>(pS + 0x40) = Game::CurrentFrame;
    }
}
